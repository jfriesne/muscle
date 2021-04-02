/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_PING, PR_RESULT_PONG
#include "dataio/TCPSocketDataIO.h"

namespace muscle {

static const String PR_NAME_MESSAGE_REUSE_TAG = "_mrutag";

static Mutex _messageReuseTagMutex;  // we could do one Mutex per tag, but that seems excessive, so let's just use a single Mutex for all

class MessageReuseTag : public RefCountable
{  
public:
   MessageReuseTag() {/* empty */}
   virtual ~MessageReuseTag() {/* empty */}
   
   ByteBufferRef _cachedData;   // the first MessageIOGateway's flattened data will be cached here for potential reuse by other gateways
};
DECLARE_REFTYPES(MessageReuseTag);

bool IsMessageOptimizedForTransmissionToMultipleGateways(const MessageRef & msg)
{
   return ((msg())&&(msg()->HasName(PR_NAME_MESSAGE_REUSE_TAG, B_TAG_TYPE)));
}

status_t OptimizeMessageForTransmissionToMultipleGateways(const MessageRef & msg)
{
   if (msg() == NULL) return B_BAD_ARGUMENT;
   if (IsMessageOptimizedForTransmissionToMultipleGateways(msg)) return B_NO_ERROR;  // it's already tagged!

   MessageReuseTagRef tagRef(newnothrow MessageReuseTag);
   MRETURN_OOM_ON_NULL(tagRef());
   return msg()->AddTag(PR_NAME_MESSAGE_REUSE_TAG, tagRef);
}

MessageIOGateway :: MessageIOGateway(int32 encoding) :
   _maxIncomingMessageSize(MUSCLE_NO_LIMIT),
   _outgoingEncoding(encoding), 
   _aboutToFlattenCallback(NULL), _aboutToFlattenCallbackData(NULL),
   _flattenedCallback(NULL), _flattenedCallbackData(NULL),
   _unflattenedCallback(NULL), _unflattenedCallbackData(NULL)
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   , _sendCodec(NULL), _recvCodec(NULL)
#endif
   , _syncPingCounter(0), _pendingSyncPingCounter(-1)
{
   // empty
}

MessageIOGateway :: ~MessageIOGateway() 
{
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   delete _sendCodec;
   delete _recvCodec;
#endif
}

status_t
MessageIOGateway ::
PopNextOutgoingMessage(MessageRef & retMsg)
{
   return GetOutgoingMessageQueue().RemoveHead(retMsg);
}

// For this method, B_NO_ERROR means "keep sending", and B_ERROR means "stop sending for now", and isn't fatal to the stream
// If there is a fatal error in the stream it will call SetHosed() to indicate that.
status_t 
MessageIOGateway :: SendMoreData(int32 & sentBytes, uint32 & maxBytes)
{
   TCHECKPOINT;

   const ByteBuffer * bb    = _sendBuffer._buffer();
   const int32 attemptSize  = muscleMin(maxBytes, bb->GetNumBytes()-_sendBuffer._offset);
   const int32 numBytesSent = GetDataIO()() ? GetDataIO()()->Write(bb->GetBuffer()+_sendBuffer._offset, attemptSize) : -1;
   if (numBytesSent >= 0)
   {
      maxBytes            -= numBytesSent;
      sentBytes           += numBytesSent;
      _sendBuffer._offset += numBytesSent;
   }
   else SetHosed();

   return (numBytesSent < attemptSize) ? B_ERROR : B_NO_ERROR;
}

int32 
MessageIOGateway ::
DoOutputImplementation(uint32 maxBytes)
{
   TCHECKPOINT;

   const uint32 mtuSize = GetMaximumPacketSize();

   int32 sentBytes = 0;
   while((maxBytes > 0)&&(IsHosed() == false))
   {
      // First, make sure our outgoing byte-buffer has data.  If it doesn't, fill it with the next outgoing message.
      if (_sendBuffer._buffer() == NULL)
      {
         while(true)
         {
            MessageRef nextRef;
            if (PopNextOutgoingMessage(nextRef).IsError()) return sentBytes;  // nothing more to send, so we're done!

            if (nextRef())
            {
               if ((_aboutToFlattenCallback)&&(_aboutToFlattenCallback(nextRef, _aboutToFlattenCallbackData).IsError())) continue;

               bool movedPRL = false;
               if (mtuSize > 0)
               {
                  if (nextRef()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, _nextPacketDest).IsOK()) 
                  {
                     // Temporarily move this field out before flattening the Message, 
                     // since we don't want to send the destination IAP as part of the packet
                     movedPRL = nextRef()->MoveName(PR_NAME_PACKET_REMOTE_LOCATION, _scratchPacketMessage).IsOK();
                  }
                  else _nextPacketDest.Reset();
               }

               _sendBuffer._offset = 0;
               _sendBuffer._buffer = FlattenHeaderAndMessageAux(nextRef);

               // Restore the PR_NAME_PACKET_REMOTE_LOCATION field, since we're not supposed to be modifying any Messages
               if (movedPRL) (void) _scratchPacketMessage.MoveName(PR_NAME_PACKET_REMOTE_LOCATION, *nextRef());

               if (_sendBuffer._buffer() == NULL) {SetHosed(); return -1;}

               if (_flattenedCallback) (void) _flattenedCallback(nextRef, _flattenedCallbackData);

#ifdef DELIBERATELY_INJECT_ERRORS_INTO_OUTGOING_MESSAGE_FOR_TESTING_ONLY_DONT_ENABLE_THIS_UNLESS_YOU_LIKE_CHAOS
 const uint32 hs    = GetHeaderSize();
 const uint32 bs    = _sendBuffer._buffer()->GetNumBytes() - hs;
 const uint32 start = rand()%bs;
 const uint32 end   = (start+5)%bs;
 if (start > end) muscleSwap(start, end);
 printf("Bork! %u->%u\n", start, end);
 for (uint32 i=start; i<=end; i++) _sendBuffer._buffer()->GetBuffer()[i+hs] = (uint8) (rand()%256);
#endif

               break;  // now go on to the sending phase
            }
         }
         if (IsHosed()) break;  // in case our callbacks called SetHosed()
      }

      // At this point, _sendBuffer._buffer() is guaranteed not to be NULL!
      if (mtuSize > 0)
      {
         PacketDataIO * pdio = GetPacketDataIO();  // guaranteed non-NULL because (mtuSize > 0)
         IPAddressAndPort packetDest;
         const ByteBuffer * bb = _sendBuffer._buffer();
         const int32 numBytesSent = _nextPacketDest.IsValid()
                                  ? pdio->WriteTo(bb->GetBuffer(), bb->GetNumBytes(), _nextPacketDest)
                                  : pdio->Write(  bb->GetBuffer(), bb->GetNumBytes());
         if (numBytesSent > 0)
         {
            maxBytes   = (maxBytes>(uint32)numBytesSent)?(maxBytes-numBytesSent):0;
            sentBytes += numBytesSent;
            _sendBuffer.Reset();
         }
         else if (numBytesSent < 0) SetHosed();
         else break;
      }
      else
      {
         if (SendMoreData(sentBytes, maxBytes).IsError()) break;  // output buffer is temporarily full
         if (_sendBuffer._offset == _sendBuffer._buffer()->GetNumBytes()) _sendBuffer.Reset();
      }
   }
   return IsHosed() ? -1 : sentBytes;
}

ByteBufferRef
MessageIOGateway ::
GetScratchReceiveBuffer()
{
   static const uint32 _scratchRecvBufferSizeBytes = 2048;  // seems like a reasonable upper limit for "small" Messages, no?

   if ((_scratchRecvBuffer())&&(_scratchRecvBuffer()->SetNumBytes(_scratchRecvBufferSizeBytes, false).IsOK())) return _scratchRecvBuffer;
   else
   {
      _scratchRecvBuffer = GetByteBufferFromPool(_scratchRecvBufferSizeBytes);  // demand-allocation
      return _scratchRecvBuffer;
   }
}

void
MessageIOGateway ::
ForgetScratchReceiveBufferIfSubclassIsStillUsingIt()
{
   // If a subclass implementation of UnflattenHeaderAndMessage() retained a reference to _scratchRecvBuffer()
   // Then we need to not modify it anymore, or we'll mess up the data they are going to use later.  So we'll
   // forget about it here, and re-allocate a new one later when we next need it.
   if ((_scratchRecvBuffer())&&(_scratchRecvBuffer.IsRefPrivate() == false)) _scratchRecvBuffer.Reset();
}

int32 
MessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   const uint32 mtuSize = GetMaximumPacketSize();
   const uint32 hs = GetHeaderSize();
   bool firstTime = true;  // always go at least once, to avoid live-lock
   int32 readBytes = 0;
   while((maxBytes > 0)&&(IsHosed() == false)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      if (mtuSize > 0)
      {
         // For UDP-style I/O, we'll read all header data and body data at once from a single packet
         if (_recvBuffer._buffer() == NULL)
         {
            ByteBufferRef scratchBuf = GetScratchReceiveBuffer();
            if (scratchBuf() == NULL) {SetHosed(); break;}  // out of memory?

            _recvBuffer._offset = 0;
            _recvBuffer._buffer = (mtuSize<=scratchBuf()->GetNumBytes()) ? scratchBuf : GetByteBufferFromPool(mtuSize);
            if (_recvBuffer._buffer() == NULL) {SetHosed(); break;}  // out of memory?
         }

         IPAddressAndPort sourceIAP;
         const int32 numBytesRead = GetPacketDataIO()->ReadFrom(_recvBuffer._buffer()->GetBuffer(), mtuSize, sourceIAP);
              if (numBytesRead < 0) {SetHosed(); break;}
         else if (numBytesRead > 0)
         {
            readBytes += numBytesRead;
            maxBytes   = (maxBytes>(uint32)numBytesRead)?(maxBytes-numBytesRead):0;
            (void) _recvBuffer._buffer()->SetNumBytes(numBytesRead, true);  // trim off any unused bytes

            MessageRef msg = UnflattenHeaderAndMessage(_recvBuffer._buffer);
            _recvBuffer.Reset();  // reset our state for the next one!
            ForgetScratchReceiveBufferIfSubclassIsStillUsingIt();

            if (msg())  // for UDP, unexpected data shouldn't be fatal
            {
               if (GetPacketRemoteLocationTaggingEnabled())
               {
                  if (sourceIAP.IsValid()) (void) msg()->ReplaceFlat(true, PR_NAME_PACKET_REMOTE_LOCATION, sourceIAP);
                                      else (void) msg()->RemoveName(PR_NAME_PACKET_REMOTE_LOCATION);  // paranoia
               }
               if ((_unflattenedCallback == NULL)||(_unflattenedCallback(msg, _unflattenedCallbackData).IsOK())) receiver.CallMessageReceivedFromGateway(msg);
            }
         }
         else break;
      }
      else
      {
         // For TCP-style I/O, we need to read the header first, and then the body, in as many steps as it takes
         if (_recvBuffer._buffer() == NULL)
         {
            ByteBufferRef scratchBuf = GetScratchReceiveBuffer();
            if (scratchBuf() == NULL) {SetHosed(); break;}  // out of memory?

            _recvBuffer._offset = 0;
            _recvBuffer._buffer = (hs<=scratchBuf()->GetNumBytes()) ? scratchBuf : GetByteBufferFromPool(hs);
            if (_recvBuffer._buffer() == NULL) {SetHosed(); break;}  // out of memory?
         }

         ByteBuffer * bb = _recvBuffer._buffer();  // guaranteed not to be NULL, if we got here!
         if (_recvBuffer._offset < hs)
         { 
            // We don't have the entire header yet, so try and read some more of it
            if (ReceiveMoreData(readBytes, maxBytes, hs).IsError()) break;
            if (_recvBuffer._offset >= hs)  // how about now?
            {
               // Now that we have the full header, parse it and allocate space for the message-body-bytes per its instructions
               const int32 bodySize = GetBodySize(bb->GetBuffer());
               if ((bodySize >= 0)&&(((uint32)bodySize) <= _maxIncomingMessageSize))
               {
                  const int32 availableBodyBytes = bb->GetNumBytes()-hs;
                  if (bodySize <= availableBodyBytes) (void) bb->SetNumBytes(hs+bodySize, true);  // trim off any extra space we don't need
                  else
                  {
                     // Oops, the Message body is greater than our buffer has bytes to store!  We're going to need a bigger buffer!
                     ByteBufferRef bigBuf = GetByteBufferFromPool(hs+bodySize);
                     if (bigBuf() == NULL) {SetHosed(); break;}
                     memcpy(bigBuf()->GetBuffer(), bb->GetBuffer(), hs);  // copy over the received header bytes to the big buffer now
                     _recvBuffer._buffer = bigBuf;
                     bb = bigBuf();
                  }
               }
               else 
               {
                  LogTime(MUSCLE_LOG_DEBUG, "MessageIOGateway %p:  bodySize " INT32_FORMAT_SPEC " is out of range, limit is " UINT32_FORMAT_SPEC "\n", this, bodySize, _maxIncomingMessageSize);
                  SetHosed(); 
                  break;
               }
            }
         }

         if (_recvBuffer._offset >= hs)  // if we got here, we're ready to read the body of the incoming Message
         {
            if ((_recvBuffer._offset < bb->GetNumBytes())&&(ReceiveMoreData(readBytes, maxBytes, bb->GetNumBytes()).IsError())) break;
            if (_recvBuffer._offset == bb->GetNumBytes())
            {
               // Finished receiving message bytes... now reconstruct that bad boy!
               MessageRef msg = UnflattenHeaderAndMessage(_recvBuffer._buffer);
               _recvBuffer.Reset();  // reset our state for the next one!
               ForgetScratchReceiveBufferIfSubclassIsStillUsingIt();

               if (msg() == NULL) {SetHosed(); break;}
               if (_unflattenedCallback) _unflattenedCallback(msg, _unflattenedCallbackData);
               receiver.CallMessageReceivedFromGateway(msg);
            }
         }
      }
   }
   return IsHosed() ? -1 : readBytes;
}

// For this method, B_NO_ERROR means "We got all the data we had room for", and B_ERROR
// means "short read".  A real network error will also cause SetHosed() to be called.
status_t 
MessageIOGateway :: ReceiveMoreData(int32 & readBytes, uint32 & maxBytes, uint32 maxArraySize)
{
   TCHECKPOINT;

   const int32 attemptSize  = muscleMin(maxBytes, (uint32)((maxArraySize>_recvBuffer._offset)?(maxArraySize-_recvBuffer._offset):0));
   const int32 numBytesRead = GetDataIO()() ? GetDataIO()()->Read(_recvBuffer._buffer()->GetBuffer()+_recvBuffer._offset, attemptSize) : -1;
   if (numBytesRead >= 0)
   {
      maxBytes            -= numBytesRead;
      readBytes           += numBytesRead;
      _recvBuffer._offset += numBytesRead;
   }
   else SetHosed();

   return (numBytesRead < attemptSize) ? B_ERROR : B_NO_ERROR;
}

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
ZLibCodec * 
MessageIOGateway ::
GetCodec(int32 newEncoding, ZLibCodec * & setCodec) const
{
   TCHECKPOINT;

   if (muscleInRange(newEncoding, (int32)MUSCLE_MESSAGE_ENCODING_ZLIB_1, (int32)MUSCLE_MESSAGE_ENCODING_ZLIB_9))
   {
      const int newLevel = (newEncoding-MUSCLE_MESSAGE_ENCODING_ZLIB_1)+1;
      if ((setCodec == NULL)||(newLevel != setCodec->GetCompressionLevel()))
      {
         delete setCodec;  // oops, encoding change!  Throw out the old codec, if any
         setCodec = newnothrow ZLibCodec(newLevel);
         if (setCodec == NULL) MWARN_OUT_OF_MEMORY;
      }
      return setCodec;
   }
   return NULL;
}
#endif

ByteBufferRef 
MessageIOGateway ::
FlattenHeaderAndMessageAux(const MessageRef & msgRef) const
{
   TCHECKPOINT;

   ByteBufferRef ret;
   if (msgRef())
   {
      RefCountableRef rcRef;
      if (msgRef()->FindTag(PR_NAME_MESSAGE_REUSE_TAG, rcRef).IsOK())
      {
         MessageReuseTagRef mrtRef(rcRef, false);
         if (mrtRef())
         {
            MutexGuard mg(_messageReuseTagMutex);  // in case (msgRef) has been shared across threads!
            if (mrtRef()->_cachedData()) ret = mrtRef()->_cachedData;  // re-use data from a neighboring gateway!
            else
            {
               ret = FlattenHeaderAndMessage(msgRef);
               if (ret()) mrtRef()->_cachedData = ret;  // also save the buffer for the next gateway to reuse
            }
         }
      }
   }
   return ret() ? ret : FlattenHeaderAndMessage(msgRef);  // the standard approach (every gateway for himself)
}

ByteBufferRef 
MessageIOGateway ::
FlattenHeaderAndMessage(const MessageRef & msgRef) const
{
   ByteBufferRef ret;
   if (msgRef())
   {
      const uint32 hs = GetHeaderSize();
      ret = GetByteBufferFromPool(hs+msgRef()->FlattenedSize());
      if (ret())
      {
         msgRef()->Flatten(ret()->GetBuffer()+hs);

         int32 encoding = MUSCLE_MESSAGE_ENCODING_DEFAULT;

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
         if (ret()->GetNumBytes() >= 32)  // below 32 bytes, the compression headers usually offset the benefits
         {
            ZLibCodec * enc = GetCodec(_outgoingEncoding, _sendCodec);
            if (enc)
            {
               ByteBufferRef compressedRef = enc->Deflate(ret()->GetBuffer()+hs, ret()->GetNumBytes()-hs, AreOutgoingMessagesIndependent(), hs);
               if (compressedRef())
               {
                  encoding = MUSCLE_MESSAGE_ENCODING_ZLIB_1+enc->GetCompressionLevel()-1;
                  ret = compressedRef;
               }
               else ret.Reset();  // uh oh, the compressor failed
            }
         }
#endif

         if (ret())
         {
            uint8 * lhb = ret()->GetBuffer();
            muscleCopyOut(&lhb[0*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(ret()->GetNumBytes()-hs));
            muscleCopyOut(&lhb[1*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(encoding));
         }
      }
   }
   return ret;
}

MessageRef 
MessageIOGateway ::
UnflattenHeaderAndMessage(const ConstByteBufferRef & bufRef) const
{
   TCHECKPOINT;

   MessageRef ret;
   if (bufRef())
   {
      ret = GetMessageFromPool();
      if (ret())
      {
         uint32 offset = GetHeaderSize();

         const uint8 * lhb = bufRef()->GetBuffer();
         const uint32 lhbSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&lhb[0*sizeof(uint32)]));
         if ((offset+lhbSize) != bufRef()->GetNumBytes())
         {
            LogTime(MUSCLE_LOG_DEBUG, "MessageIOGateway %p:  Unexpected lhb size " UINT32_FORMAT_SPEC ", expected " INT32_FORMAT_SPEC "\n", this, lhbSize, bufRef()->GetNumBytes()-offset);
            return MessageRef();
         }

         const int32 encoding = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<int32>(&lhb[1*sizeof(uint32)]));

         const ByteBuffer * bb = bufRef();  // default; may be changed below

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
         ByteBufferRef expRef;  // must be declared outside the brackets below!
         ZLibCodec * enc = GetCodec(encoding, _recvCodec);
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
               LogTime(MUSCLE_LOG_DEBUG, "MessageIOGateway %p:  Error inflating compressed byte buffer!\n", this);
               bb = NULL;
            }
         }
#else
         if (encoding != MUSCLE_MESSAGE_ENCODING_DEFAULT) bb = NULL;
#endif

         if ((bb == NULL)||(ret()->Unflatten(bb->GetBuffer()+offset, bb->GetNumBytes()-offset).IsError())) ret.Reset();
      }
   }
   return ret;
}

// Returns the size of the pre-flattened-message header section, in bytes.
// The default format has an 8-byte header (4 bytes for encoding ID, 4 bytes for message length)
uint32 
MessageIOGateway ::
GetHeaderSize() const
{
   return 2 * sizeof(uint32);  // one long for the encoding ID, and one long for the body length 
}

int32 
MessageIOGateway ::
GetBodySize(const uint8 * headerBuf) const 
{
   return (muscleInRange((uint32)B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[1*sizeof(uint32)])), (uint32)MUSCLE_MESSAGE_ENCODING_DEFAULT, (uint32)MUSCLE_MESSAGE_ENCODING_END_MARKER-1)) ? (int32)(B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&headerBuf[0*sizeof(uint32)]))) : -1;
}

bool 
MessageIOGateway ::
HasBytesToOutput() const
{
   return ((IsHosed() == false)&&((_sendBuffer._buffer())||(GetOutgoingMessageQueue().HasItems())));
}

void
MessageIOGateway ::
Reset()
{
   TCHECKPOINT;

   AbstractMessageIOGateway::Reset();

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   delete _sendCodec; _sendCodec = NULL;
   delete _recvCodec; _recvCodec = NULL;
#endif

   _sendBuffer.Reset();
   _recvBuffer.Reset();
}

MessageRef MessageIOGateway :: CreateSynchronousPingMessage(uint32 syncPingCounter) const
{
   MessageRef pingMsg = GetMessageFromPool(PR_COMMAND_PING);
   return ((pingMsg())&&(pingMsg()->AddInt32("_miosp", syncPingCounter).IsOK())) ? pingMsg : MessageRef();
}

status_t MessageIOGateway :: ExecuteSynchronousMessaging(AbstractGatewayMessageReceiver * optReceiver, uint64 timeoutPeriod)
{
   const DataIO * dio = GetDataIO()();
   if ((dio == NULL)||(dio->GetReadSelectSocket().GetFileDescriptor() < 0)||(dio->GetWriteSelectSocket().GetFileDescriptor() < 0)) return B_BAD_OBJECT;

   MessageRef pingMsg = CreateSynchronousPingMessage(_syncPingCounter);
   if (pingMsg() == NULL) return B_ERROR("CreateSynchronousPingMessage() failed");

   status_t ret;
   if (AddOutgoingMessage(pingMsg).IsOK(ret))
   {
      _pendingSyncPingCounter = _syncPingCounter;
      _syncPingCounter++;
      return AbstractMessageIOGateway::ExecuteSynchronousMessaging(optReceiver, timeoutPeriod);
   }
   else return ret;
}

void MessageIOGateway :: SynchronousMessageReceivedFromGateway(const MessageRef & msg, void * userData, AbstractGatewayMessageReceiver & r) 
{
   if ((_pendingSyncPingCounter >= 0)&&(IsSynchronousPongMessage(msg, _pendingSyncPingCounter)))
   {
      // Yay, we found our pong Message, so we are no longer waiting for one.
      _pendingSyncPingCounter = -1;
   }
   else AbstractMessageIOGateway::SynchronousMessageReceivedFromGateway(msg, userData, r);
}

bool MessageIOGateway :: IsSynchronousPongMessage(const MessageRef & msg, uint32 pendingSyncPingCounter) const
{
   return ((msg()->what == PR_RESULT_PONG)&&((uint32)msg()->GetInt32("_miosp", -1) == pendingSyncPingCounter));
}

MessageRef MessageIOGateway :: ExecuteSynchronousMessageRPCCall(const Message & requestMessage, const IPAddressAndPort & targetIAP, uint64 timeoutPeriod)
{
   MessageRef ret;

   const uint64 timeBeforeConnect = GetRunTime64();
   ConstSocketRef s = Connect(targetIAP, NULL, NULL, true, timeoutPeriod);
   if (s())
   {
      if (timeoutPeriod != MUSCLE_TIME_NEVER)
      {
         uint64 connectDuration = GetRunTime64()-timeBeforeConnect;
         timeoutPeriod = (timeoutPeriod > connectDuration) ? (timeoutPeriod-connectDuration) : 0;
      }
 
      DataIORef oldIO = GetDataIO();
      TCPSocketDataIO tsdio(s, false);
      SetDataIO(DataIORef(&tsdio, false));
      QueueGatewayMessageReceiver receiver;
      if ((AddOutgoingMessage(MessageRef(const_cast<Message *>(&requestMessage), false)).IsOK())&&(ExecuteSynchronousMessaging(&receiver, timeoutPeriod).IsOK())) ret = receiver.GetMessages().HasItems() ? receiver.GetMessages().Head() : GetMessageFromPool();
      SetDataIO(oldIO);  // restore any previous I/O
   }
   return ret;
}

status_t MessageIOGateway :: ExecuteSynchronousMessageSend(const Message & requestMessage, const IPAddressAndPort & targetIAP, uint64 timeoutPeriod)
{
   status_t ret = B_ERROR;
   const uint64 timeBeforeConnect = GetRunTime64();
   ConstSocketRef s = Connect(targetIAP, NULL, NULL, true, timeoutPeriod);
   if (s())
   {
      if (timeoutPeriod != MUSCLE_TIME_NEVER)
      {
         const uint64 connectDuration = GetRunTime64()-timeBeforeConnect;
         timeoutPeriod = (timeoutPeriod > connectDuration) ? (timeoutPeriod-connectDuration) : 0;
      }
 
      DataIORef oldIO = GetDataIO();
      TCPSocketDataIO tsdio(s, false);
      SetDataIO(DataIORef(&tsdio, false));
      QueueGatewayMessageReceiver receiver;
      if (AddOutgoingMessage(MessageRef(const_cast<Message *>(&requestMessage), false)).IsOK()) 
      {
         NestCountGuard ncg(_noRPCReply);  // so that we'll return as soon as we've sent the request Message, and not wait for a reply Message.
         ret = ExecuteSynchronousMessaging(&receiver, timeoutPeriod);
      }
      SetDataIO(oldIO);
   }
   return ret;
}

CountedMessageIOGateway :: CountedMessageIOGateway(int32 outgoingEncoding)
   : MessageIOGateway(outgoingEncoding)
   , _outgoingByteCount(0)
{
   // empty
}

status_t CountedMessageIOGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   MRETURN_ON_ERROR(MessageIOGateway::AddOutgoingMessage(messageRef));

   const uint32 msgSize = messageRef()?messageRef()->FlattenedSize():0;
   if (GetOutgoingMessageQueue().GetNumItems() > 1) _outgoingByteCount += msgSize;
                                               else _outgoingByteCount  = msgSize;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access
   return B_NO_ERROR;
}

void CountedMessageIOGateway :: Reset()
{
   MessageIOGateway::Reset();
   _outgoingByteCount = 0;
}

status_t CountedMessageIOGateway :: PopNextOutgoingMessage(MessageRef & outMsg)
{
   MRETURN_ON_ERROR(MessageIOGateway::PopNextOutgoingMessage(outMsg));

   if (GetOutgoingMessageQueue().HasItems())
   {
      const uint32 retSize = outMsg()?outMsg()->FlattenedSize():0;
      _outgoingByteCount = (retSize<_outgoingByteCount) ? (_outgoingByteCount-retSize) : 0;  // paranoia to avoid underflow
   }
   else _outgoingByteCount = 0;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access

   return B_NO_ERROR;
}

} // end namespace muscle
