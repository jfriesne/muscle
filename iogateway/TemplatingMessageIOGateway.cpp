#include "iogateway/TemplatingMessageIOGateway.h"

//Uncomment this to see statistics about how well we're doing
//#define DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY 1

namespace muscle {

TemplatingMessageIOGateway :: TemplatingMessageIOGateway(uint32 maxLRUCacheSizeBytes, int32 outgoingEncoding)
   : CountedMessageIOGateway(outgoingEncoding)
   , _maxLRUCacheSizeBytes(maxLRUCacheSizeBytes)
   , _incomingTemplatesTotalSizeBytes(0)
   , _outgoingTemplatesTotalSizeBytes(0)
{
   // empty
}

static const uint32 CREATE_TEMPLATE_BIT  = (((uint32)1)<<31);  // if the high-bit is set in the length-word, that means the receiver should use the buffer's Message to create a template
static const uint32 PAYLOAD_ENCODING_BIT = (((uint32)1)<<31);  // if the high-bit is set in the encoding-word, that means the receiver should interpret the buffer as payload-only and not as a flattened Message

int32 TemplatingMessageIOGateway :: GetBodySize(const uint8 * headerBuf) const
{
   const uint32 bodySize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[0*sizeof(uint32)])) & ~CREATE_TEMPLATE_BIT;
   const uint32 encoding = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[1*sizeof(uint32)])) & ~PAYLOAD_ENCODING_BIT;
   return muscleInRange(encoding, (uint32)MUSCLE_MESSAGE_ENCODING_DEFAULT, (uint32)(MUSCLE_MESSAGE_ENCODING_END_MARKER-1)) ? (int32)bodySize : (int32)-1;
}

ByteBufferRef TemplatingMessageIOGateway :: FlattenHeaderAndMessage(const MessageRef & msgRef) const
{
   if (msgRef() == NULL) return ByteBufferRef();

   bool createTemplate = false;
   const MessageRef * templateMsgRef = NULL;  // will be set non-NULL iff we want to send our Message as payload-only
   uint64 templateID = 0;
   const bool isMessageTrivial = (msgRef()->GetNumNames() == 0);  // what-code only Messages can be sent in just 4 bytes
   if ((isMessageTrivial == false)&&(IsOkayToTemplatizeMessage(*msgRef())))
   {
      templateID     = msgRef()->TemplateHashCode64();
      templateMsgRef = _outgoingTemplates.GetAndMoveToFront(templateID);
      if (templateMsgRef == NULL)
      {
         // demand-allocate a template-Message for us to cache and use in the future
         // Note that I'm deliberately leaving (templateMsgRef) set to NULL here, though
         // since I want this Message to be sent via Flatten() so that the receiver can make a template out of it
         MessageRef newTemplateMsgRef = msgRef()->CreateMessageTemplate();
         if ((newTemplateMsgRef())&&(_outgoingTemplates.PutAtFront(templateID, newTemplateMsgRef).IsOK()))
         {
            createTemplate = true;
            _outgoingTemplatesTotalSizeBytes += newTemplateMsgRef()->FlattenedSize();
            TrimLRUCache(_outgoingTemplates, _outgoingTemplatesTotalSizeBytes, "SEND");
         }
         else LogTime(MUSCLE_LOG_ERROR, "TemplatingMessageIOGateway::FlattenHeaderAndMessage():  Couldn't create a template for Message hash=" UINT64_FORMAT_SPEC "\n", templateID);
      }
   }

   const uint32 hs = GetHeaderSize();
   ByteBufferRef retBuf = GetByteBufferFromPool(hs+(templateMsgRef ? (sizeof(uint64)+msgRef()->TemplatedFlattenedSize(*templateMsgRef->GetItemPointer())) : (isMessageTrivial ? sizeof(uint32) : msgRef()->FlattenedSize())));
   if (retBuf())
   {
      uint8 * bodyPtr = retBuf()->GetBuffer()+hs;
      if (templateMsgRef)
      {
         muscleCopyOut(bodyPtr, B_HOST_TO_LENDIAN_INT64(templateID));
         msgRef()->TemplatedFlatten(*templateMsgRef->GetItemPointer(), bodyPtr+sizeof(uint64));  // the new payload-only format
      }
      else if (isMessageTrivial) muscleCopyOut(bodyPtr, B_HOST_TO_LENDIAN_INT32(msgRef()->what));  // special-case for what-code-only Messages
      else                       msgRef()->Flatten(bodyPtr);  // the old full-freight MessageIOGateway-style format

      int32 encoding = MUSCLE_MESSAGE_ENCODING_DEFAULT;
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
      if (retBuf()->GetNumBytes() >= 32)  // below 32 bytes, the compression headers usually offset the benefits
      {
         ZLibCodec * enc = GetSendCodec();
         if (enc)
         {
            ByteBufferRef compressedRef = enc->Deflate(retBuf()->GetBuffer()+hs, retBuf()->GetNumBytes()-hs, AreOutgoingMessagesIndependent(), hs);
            if (compressedRef())
            {
               encoding = MUSCLE_MESSAGE_ENCODING_ZLIB_1+enc->GetCompressionLevel()-1;
               retBuf = compressedRef;
            }
            else retBuf.Reset();  // uh oh, the compressor failed
         }
      }
#endif

      if (retBuf())
      {
         uint8 * lhb = retBuf()->GetBuffer();
         muscleCopyOut(&lhb[0*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(((uint32)(retBuf()->GetNumBytes()-hs)) | (createTemplate ? CREATE_TEMPLATE_BIT  : 0)));
         muscleCopyOut(&lhb[1*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(((uint32)encoding)                     | (templateMsgRef ? PAYLOAD_ENCODING_BIT : 0)));
      }
   }

#ifdef DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY
   const uint32 mySize  = retBuf()->GetNumBytes();
   const uint32 oldSize = msgRef()->FlattenedSize()+(3*sizeof(uint32));
   printf("SENT (down %.0f%%): outgoingTableSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " templateID=" UINT64_FORMAT_SPEC " createTemplate=%i templateMsgRef=%p bufSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", 100.0f*(1.0f-(((float)mySize)/oldSize)), _outgoingTemplates.GetNumItems(), _outgoingTemplatesTotalSizeBytes, templateID, createTemplate, templateMsgRef, mySize, oldSize);
   //retBuf()->PrintToStream();
#endif

   return retBuf;
}

MessageRef TemplatingMessageIOGateway :: UnflattenHeaderAndMessage(const ConstByteBufferRef & bufRef) const
{
   TCHECKPOINT;

   MessageRef retMsg = bufRef() ? GetMessageFromPool() : MessageRef();
   if (retMsg() == NULL) return MessageRef();

   uint32 offset = GetHeaderSize();

   const uint8 * lhb       = bufRef()->GetBuffer();
   const uint32 lengthWord = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&lhb[0*sizeof(uint32)]));
   const uint32 lhbSize    = lengthWord & ~CREATE_TEMPLATE_BIT;
   if ((offset+lhbSize) != bufRef()->GetNumBytes())
   {
      LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Unexpected lhb size " UINT32_FORMAT_SPEC ", expected " INT32_FORMAT_SPEC "\n", this, lhbSize, bufRef()->GetNumBytes()-offset);
      return MessageRef();
   }

   const uint32 encodingWord = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&lhb[1*sizeof(uint32)]));
   uint32 encoding = encodingWord & ~PAYLOAD_ENCODING_BIT;

   const ByteBuffer * bb = bufRef();  // default; may be changed below

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   ByteBufferRef expRef;  // must be declared outside the brackets below!
   ZLibCodec * enc = GetReceiveCodec(encoding);
   if (enc)
   {
      expRef = enc->Inflate(bb->GetBuffer()+offset, bb->GetNumBytes()-offset);
      if (expRef())
      {
         bb = expRef();
         offset = 0;
      }
      else
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Error inflating compressed byte buffer!\n", this);
         bb = NULL;
      }
   }
#else
   if (encoding != MUSCLE_MESSAGE_ENCODING_DEFAULT) bb = NULL;
#endif

   if (bb == NULL) return MessageRef();

   const bool createTemplate      = ((lengthWord & CREATE_TEMPLATE_BIT) != 0);
   const uint8 * inPtr            = bb->GetBuffer()+offset;
   const uint8 * firstInvalidByte = bb->GetBuffer()+bb->GetNumBytes();
   const uint32 numBodyBytes      = firstInvalidByte-inPtr;
   uint64 templateID              = 0;

   status_t ret;
   if ((encodingWord & PAYLOAD_ENCODING_BIT) != 0)
   {
      if (createTemplate)
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Incoming buffer had both CREATE_TEMPLATE_BIT and PAYLOAD_ENCODING_BIT bits set!\n", this);
         return MessageRef();
      }
      else if (numBodyBytes >= sizeof(uint64))
      {
         templateID = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(inPtr));
         const MessageRef * templateMsgRef = _incomingTemplates.GetAndMoveToFront(templateID);
         if (templateMsgRef)
         {
            const uint8 * payloadBytes = inPtr + sizeof(uint64);
            if (retMsg()->TemplatedUnflatten(*templateMsgRef->GetItemPointer(), payloadBytes, firstInvalidByte-payloadBytes).IsError(ret))
            {
               LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Error unflattening " UINT32_FORMAT_SPEC " payload-bytes using template ID " UINT64_FORMAT_SPEC "\n", (uint32)(firstInvalidByte-payloadBytes), templateID);
               return MessageRef();
            }
         }
         else
         {
            LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Template " UINT64_FORMAT_SPEC " not found in incoming-templates cache!\n", templateID);
            return MessageRef();
         }
      }
      else
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Payload-only buffer is too short for template ID!  (" UINT32_FORMAT_SPEC " bytes)\n", numBodyBytes);
         return MessageRef();
      }
   }
   else
   {
           if (numBodyBytes == sizeof(uint32)) retMsg()->what = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(inPtr));  // special-case for what-code-only Messages
      else if (retMsg()->Unflatten(inPtr, numBodyBytes).IsError(ret))
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Unflatten() failed on " UINT32_FORMAT_SPEC "-byte buffer (%s)\n", numBodyBytes, ret());
         return MessageRef();
      }

      if (createTemplate)
      {
         MessageRef tMsg = retMsg()->CreateMessageTemplate();
         if (tMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  CreateTemplateMessage() failed!\n");
            return MessageRef();
         }

         templateID = tMsg()->TemplateHashCode64();
         if (_incomingTemplates.PutAtFront(templateID, tMsg).IsError()) return MessageRef();

         _incomingTemplatesTotalSizeBytes += tMsg()->FlattenedSize();
         TrimLRUCache(_incomingTemplates, _incomingTemplatesTotalSizeBytes, "RECV");
      }
   }

#ifdef DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY
   const uint32 mySize  = bufRef()->GetNumBytes();
   const uint32 oldSize = (retMsg()?retMsg()->FlattenedSize():0)+(3*sizeof(uint32));
   printf("RECEIVED (down %.0f%%): incomingTable=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " createTemplate=%i templateID=" UINT64_FORMAT_SPEC " bufSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", 100.0f*(1.0f-(((float)mySize)/oldSize)), _incomingTemplates.GetNumItems(), _incomingTemplatesTotalSizeBytes, createTemplate, templateID, mySize, oldSize);
   //bufRef()->PrintToStream();
#endif
   return retMsg;
}

void TemplatingMessageIOGateway :: Reset()
{
   CountedMessageIOGateway::Reset();
   _incomingTemplates.Clear();
   _outgoingTemplates.Clear();
   _incomingTemplatesTotalSizeBytes = _outgoingTemplatesTotalSizeBytes = 0;
}

void TemplatingMessageIOGateway :: TrimLRUCache(Hashtable<uint64, MessageRef> & lruCache, uint32 & tallyBytes, const char * desc) const
{
   while((lruCache.GetNumItems()>1)&&(tallyBytes > _maxLRUCacheSizeBytes))
   {
      const uint32 lastSize = lruCache.GetLastValue()->GetItemPointer()->FlattenedSize();
      (void) lruCache.RemoveLast();

      if (tallyBytes >= lastSize) tallyBytes -= lastSize;
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "TrimLRUCache():  tallyBytes is too small!  " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", tallyBytes, lastSize, lruCache.GetNumItems());
         tallyBytes = 0;  // I guess?
      }

#ifdef DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY
      printf("TRIM lastSize=" UINT32_FORMAT_SPEC ", %s LRU is now at size=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", lastSize, desc, lruCache.GetNumItems(), tallyBytes);
#else
      (void) desc;
#endif
   }
}

} // end namespace muscle
