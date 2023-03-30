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
   const uint32 bodySize = DefaultEndianConverter::Import<uint32>(&headerBuf[0*sizeof(uint32)]) & ~CREATE_TEMPLATE_BIT;
   const uint32 encoding = DefaultEndianConverter::Import<uint32>(&headerBuf[1*sizeof(uint32)]) & ~PAYLOAD_ENCODING_BIT;
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

   const uint32 hs      = GetHeaderSize();
   uint32 msgFlatSize   = 0;  // demand-calculated
   const uint32 tmSize  = templateMsgRef ? msgRef()->TemplatedFlattenedSize(*templateMsgRef->GetItemPointer()) : 0;
   const uint32 bufSize = hs + (templateMsgRef ? (sizeof(templateID)+tmSize) : (isMessageTrivial ? sizeof(uint32) : (msgFlatSize=msgRef()->FlattenedSize())));
   ByteBufferRef retBuf = GetByteBufferFromPool(bufSize);
   if (retBuf())
   {
      DataFlattener flat(*retBuf());
      (void) flat.SeekRelative(hs);  // skip past the header for now

      if (templateMsgRef)
      {
         flat.WriteInt64(templateID);
         msgRef()->TemplatedFlatten(*templateMsgRef->GetItemPointer(), DataFlattener(flat, tmSize));  // the new payload-only format
      }
      else if (isMessageTrivial) flat.WriteInt32(msgRef()->what);  // special-case for what-code-only Messages
      else msgRef()->Flatten(DataFlattener(flat, msgFlatSize));  // the old full-freight MessageIOGateway-style format (msgFlatSize will be set non-negative if we got here)

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
         DataFlattener subFlat(retBuf()->GetBuffer(), GetHeaderSize());
         subFlat.WriteInt32(((uint32)(retBuf()->GetNumBytes()-hs)) | (createTemplate ? CREATE_TEMPLATE_BIT  : 0));
         subFlat.WriteInt32(((uint32)encoding)                     | (templateMsgRef ? PAYLOAD_ENCODING_BIT : 0));
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
   MRETURN_ON_ERROR(retMsg);

   uint32 offset = GetHeaderSize();

   const uint8 * lhb       = bufRef()->GetBuffer();
   const uint32 lengthWord = DefaultEndianConverter::Import<uint32>(&lhb[0*sizeof(uint32)]);
   const uint32 lhbSize    = lengthWord & ~CREATE_TEMPLATE_BIT;
   if ((offset+lhbSize) != bufRef()->GetNumBytes())
   {
      LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Unexpected lhb size " UINT32_FORMAT_SPEC ", expected " INT32_FORMAT_SPEC "\n", this, lhbSize, bufRef()->GetNumBytes()-offset);
      return B_BAD_DATA;
   }

   const uint32 encodingWord = DefaultEndianConverter::Import<uint32>(&lhb[1*sizeof(uint32)]);
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
         return expRef.GetStatus();
      }
   }
#else
   if (encoding != MUSCLE_MESSAGE_ENCODING_DEFAULT) return B_UNIMPLEMENTED;
#endif

   const bool createTemplate      = ((lengthWord & CREATE_TEMPLATE_BIT) != 0);
   const uint8 * inPtr            = bb->GetBuffer()+offset;
   const uint8 * firstInvalidByte = bb->GetBuffer()+bb->GetNumBytes();
   const uint32 numBodyBytes      = (uint32) (firstInvalidByte-inPtr);
   uint64 templateID              = 0;

   status_t ret;
   if ((encodingWord & PAYLOAD_ENCODING_BIT) != 0)
   {
      if (createTemplate)
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Incoming buffer had both CREATE_TEMPLATE_BIT and PAYLOAD_ENCODING_BIT bits set!\n", this);
         return B_BAD_DATA;
      }
      else if (numBodyBytes >= sizeof(uint64))
      {
         templateID = DefaultEndianConverter::Import<uint64>(inPtr);
         const MessageRef * templateMsgRef = _incomingTemplates.GetAndMoveToFront(templateID);
         if (templateMsgRef)
         {
            const uint8 * payloadBytes = inPtr + sizeof(uint64);
            DataUnflattener unflat(payloadBytes, (uint32)(firstInvalidByte-payloadBytes));
            if (retMsg()->TemplatedUnflatten(*templateMsgRef->GetItemPointer(), unflat).IsError(ret))
            {
               LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Error unflattening " UINT32_FORMAT_SPEC " payload-bytes using template ID " UINT64_FORMAT_SPEC "\n", (uint32)(firstInvalidByte-payloadBytes), templateID);
               return ret;
            }
         }
         else
         {
            LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Template " UINT64_FORMAT_SPEC " not found in incoming-templates cache!\n", templateID);
            return B_DATA_NOT_FOUND;
         }
      }
      else
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Payload-only buffer is too short for template ID!  (" UINT32_FORMAT_SPEC " bytes)\n", numBodyBytes);
         return B_BAD_DATA;
      }
   }
   else
   {
           if (numBodyBytes == sizeof(uint32)) retMsg()->what = DefaultEndianConverter::Import<uint32>(inPtr);  // special-case for what-code-only Messages
      else if (retMsg()->UnflattenFromBytes(inPtr, numBodyBytes).IsError(ret))
      {
         LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Unflatten() failed on " UINT32_FORMAT_SPEC "-byte buffer (%s)\n", numBodyBytes, ret());
         return ret;
      }

      if (createTemplate)
      {
         MessageRef tMsg = retMsg()->CreateMessageTemplate();
         if (tMsg() == NULL)
         {
            LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  CreateTemplateMessage() failed!\n");
            return tMsg;
         }

         templateID = tMsg()->TemplateHashCode64();
         MRETURN_ON_ERROR(_incomingTemplates.PutAtFront(templateID, tMsg));

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
