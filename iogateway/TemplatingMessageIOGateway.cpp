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

void TemplatingMessageIOGateway :: TrimLRUCache(Hashtable<uint64, MessageRef> & lruCache, uint32 & tallyBytes) const
{
   while((lruCache.GetNumItems()>1)&(tallyBytes > _maxLRUCacheSizeBytes))
   {
      const uint32 lastSize = lruCache.GetLastValue()->GetItemPointer()->FlattenedSize();
      (void) lruCache.RemoveLast();

      if (tallyBytes >= lastSize) tallyBytes -= lastSize;
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "TrimLRUCache():  tallyBytes is too small!  " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", tallyBytes, lastSize, lruCache.GetNumItems());
         tallyBytes = 0;  // I guess?
      }
   }
}

static const uint32 _templateIncludedMask = (1<<31);  // high-bit-set in the encoding word means we've included a template definition

int32 TemplatingMessageIOGateway :: GetBodySize(const uint8 * headerBuf) const
{
   return (muscleInRange(((uint32)B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[1*sizeof(uint32)])))&~_templateIncludedMask, (uint32)MUSCLE_MESSAGE_ENCODING_DEFAULT, (uint32)MUSCLE_MESSAGE_ENCODING_END_MARKER-1)) ? (int32)(B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[0*sizeof(uint32)]))) : -1;
}

ByteBufferRef TemplatingMessageIOGateway :: FlattenHeaderAndMessage(const MessageRef & msgRef) const
{
   if (msgRef() == NULL) return ByteBufferRef();

   uint32 sendTemplateSize = 0;  // if non-zero, the flattened-size of the template-Message that we also need to send
   const MessageRef * templateMsgRef = NULL;
   uint64 templateID = GetTemplateHashCode64ForMessage(*msgRef());
   if (templateID > 0)
   {
      templateMsgRef = _outgoingTemplates.GetAndMoveToFront(templateID);
      if (templateMsgRef == NULL)
      {
         // demand-allocate a template for this Message-fields-layout
         MessageRef newTemplateMsgRef = msgRef()->CreateMessageTemplate();
         if ((newTemplateMsgRef())&&(_outgoingTemplates.PutAtFront(templateID, newTemplateMsgRef).IsOK()))
         {
            templateMsgRef = _outgoingTemplates.GetFirstValue();  // guaranteed not to fail
            sendTemplateSize = newTemplateMsgRef()->FlattenedSize();
            _outgoingTemplatesTotalSizeBytes += sendTemplateSize;
            TrimLRUCache(_outgoingTemplates, _outgoingTemplatesTotalSizeBytes);
         }
         else LogTime(MUSCLE_LOG_ERROR, "TemplatingMessageIOGateway::FlattenHeaderAndMessage():  Couldn't create a template for Message hash=" UINT64_FORMAT_SPEC "\n", templateID);
      }
   }

   uint32 msgFlatSize = 0;
   if (templateMsgRef) msgFlatSize = msgRef()->TemplatedFlattenedSize(*templateMsgRef->GetItemPointer());
   else
   {
      templateID = 0;                           // in case template-generation failed above
      msgFlatSize = msgRef()->FlattenedSize();  // we'll send this one the old-fashioned way
   }

   const uint32 hs = GetHeaderSize();
   ByteBufferRef retBuf = GetByteBufferFromPool(hs+sendTemplateSize+msgFlatSize);
   if (retBuf())
   {
      if (templateMsgRef)
      {
         const Message & templateMsg = *templateMsgRef->GetItemPointer();
         if (sendTemplateSize > 0) templateMsg.Flatten(retBuf()->GetBuffer()+hs);            // template-send (first-time only, for a given template ID)
         msgRef()->TemplatedFlatten(templateMsg, retBuf()->GetBuffer()+hs+sendTemplateSize);  // new-style metadata-only send
      }
      else msgRef()->Flatten(retBuf()->GetBuffer()+hs);  // old-style send with inline metadata

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
      if (sendTemplateSize > 0) encoding |= _templateIncludedMask;  // we'll set the high bit of the encoding-word to indicate that we've included both the template and its first payload

      if (retBuf())
      {
         uint8 * lhb = retBuf()->GetBuffer();
         muscleCopyOut(&lhb[0*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(retBuf()->GetNumBytes()-hs));
         muscleCopyOut(&lhb[1*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(encoding));
         muscleCopyOut(&lhb[2*sizeof(uint32)], B_HOST_TO_LENDIAN_INT64(templateID));
      }
   }
#ifdef DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY
   printf("SENDING: outgoingTableSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " templateID=" UINT64_FORMAT_SPEC " sendTemplateSize=" UINT32_FORMAT_SPEC " bufSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", _outgoingTemplates.GetNumItems(), _outgoingTemplatesTotalSizeBytes, templateID, sendTemplateSize, retBuf()->GetNumBytes(), msgRef()->FlattenedSize()); retBuf()->PrintToStream();
#endif
   return retBuf;
}

MessageRef TemplatingMessageIOGateway :: UnflattenHeaderAndMessage(const ConstByteBufferRef & bufRef) const
{
   TCHECKPOINT;

   MessageRef retMsg = bufRef() ? GetMessageFromPool() : MessageRef();
   if (retMsg() == NULL) return MessageRef();

   uint32 offset = GetHeaderSize();

   const uint8 * lhb    = bufRef()->GetBuffer();
   const uint32 lhbSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&lhb[0*sizeof(uint32)]));
   if ((offset+lhbSize) != bufRef()->GetNumBytes())
   {
      LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway %p:  Unexpected lhb size " UINT32_FORMAT_SPEC ", expected " INT32_FORMAT_SPEC "\n", this, lhbSize, bufRef()->GetNumBytes()-offset);
      return MessageRef();
   }

   bool templateIncluded = false;
   uint32 encoding = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&lhb[1*sizeof(uint32)]));
   if (encoding & _templateIncludedMask)
   {
      encoding &= ~_templateIncludedMask;
      templateIncluded = true;
   }

   const uint64 templateID = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(&lhb[2*sizeof(uint32)]));

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

   if (bb)
   {
      const uint8 * inPtr            = bb->GetBuffer()+offset;
      const uint8 * firstInvalidByte = bb->GetBuffer()+bb->GetNumBytes();

      if (templateID > 0)
      {
         MessageRef * templateMsgRef = NULL;
         if (templateIncluded)
         {
            MessageRef tMsg = GetMessageFromPool();
            status_t ret;
            if ((tMsg() == NULL)||(tMsg()->Unflatten(inPtr, firstInvalidByte-inPtr).IsError(ret))||(_incomingTemplates.PutAtFront(templateID, tMsg).IsError(ret)))
            {
               LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Unable to unflatten template Message from " UINT32_FORMAT_SPEC " bytes of data (%s)\n", (uint32)(firstInvalidByte-inPtr), ret());
               return MessageRef();
            }

            templateMsgRef = _incomingTemplates.GetFirstValue();  // guaranteed not to fail
            const uint32 templateSize = tMsg()->FlattenedSize();
            _incomingTemplatesTotalSizeBytes += templateSize;
            inPtr                            += templateSize;
            TrimLRUCache(_incomingTemplates, _incomingTemplatesTotalSizeBytes);
         }
         else templateMsgRef = _incomingTemplates.GetAndMoveToFront(templateID);

         if (templateMsgRef)
         {
            status_t ret;
            if (retMsg()->TemplatedUnflatten(*templateMsgRef->GetItemPointer(), inPtr, firstInvalidByte-inPtr).IsError(ret))
            {
               LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Error unflattening " UINT32_FORMAT_SPEC " bytes using template ID " UINT64_FORMAT_SPEC "\n", (uint32)(firstInvalidByte-inPtr), templateID);
               return MessageRef();
            }
         }
         else
         {
            LogTime(MUSCLE_LOG_DEBUG, "TemplatingMessageIOGateway::UnflattenHeaderAndMessage():  Template " UINT64_FORMAT_SPEC " not found in cache!\n", templateID);
            return MessageRef();
         }
      }
      else if (retMsg()->Unflatten(inPtr, firstInvalidByte-inPtr).IsError()) return MessageRef();
   }
   else return MessageRef();

#ifdef DEBUG_TEMPLATING_MESSAGE_IO_GATEWAY
   printf("RECEIVED: incomingTable=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " templateIncluded=%i templateID=" UINT64_FORMAT_SPEC " bufSize=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", _incomingTemplates.GetNumItems(), _incomingTemplatesTotalSizeBytes, templateIncluded, templateID, bufRef()->GetNumBytes(), retMsg()?retMsg()->FlattenedSize():0); bufRef()->PrintToStream();
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

} // end namespace muscle
