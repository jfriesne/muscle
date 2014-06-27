/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_PING, PR_RESULT_PONG
#include "dataio/TCPSocketDataIO.h"

namespace muscle {

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
   _scratchRecvBuffer.AdoptBuffer(sizeof(_scratchRecvBufferBytes), _scratchRecvBufferBytes);
}

MessageIOGateway :: ~MessageIOGateway() 
{
   (void) _scratchRecvBuffer.ReleaseBuffer();  // otherwise it will try to delete[] _scratchRecvBufferBytes and that would be bad

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

   const ByteBuffer * bb = _sendBuffer._buffer();
   int32 attemptSize     = muscleMin(maxBytes, bb->GetNumBytes()-_sendBuffer._offset);
   int32 numSent         = GetDataIO()()->Write(bb->GetBuffer()+_sendBuffer._offset, attemptSize);
   if (numSent >= 0)
   {
      maxBytes            -= numSent;
      sentBytes           += numSent;
      _sendBuffer._offset += numSent;
   }
   else SetHosed();

   return (numSent < attemptSize) ? B_ERROR : B_NO_ERROR;
}

int32 
MessageIOGateway ::
DoOutputImplementation(uint32 maxBytes)
{
   TCHECKPOINT;

   int32 sentBytes = 0;
   while((maxBytes > 0)&&(IsHosed() == false))
   {
      // First, make sure our outgoing byte-buffer has data.  If it doesn't, fill it with the next outgoing message.
      if (_sendBuffer._buffer() == NULL)
      {
         while(true)
         {
            MessageRef nextRef;
            if (PopNextOutgoingMessage(nextRef) != B_NO_ERROR) 
            {
               if ((GetFlushOnEmpty())&&(sentBytes > 0)) GetDataIO()()->FlushOutput();
               return sentBytes;  // nothing more to send, so we're done!
            }

            const Message * nextSendMsg = nextRef();
            if (nextSendMsg)
            {
               if (_aboutToFlattenCallback) _aboutToFlattenCallback(nextRef, _aboutToFlattenCallbackData);

               _sendBuffer._offset = 0;
               _sendBuffer._buffer = FlattenHeaderAndMessage(nextRef);
               if (_sendBuffer._buffer() == NULL) {SetHosed(); return -1;}

               if (_flattenedCallback) _flattenedCallback(nextRef, _flattenedCallbackData);

#ifdef DELIBERATELY_INJECT_ERRORS_INTO_OUTGOING_MESSAGE_FOR_TESTING_ONLY_DONT_ENABLE_THIS_UNLESS_YOU_LIKE_CHAOS
 uint32 hs    = GetHeaderSize();
 uint32 bs    = _sendBuffer._buffer()->GetNumBytes() - hs;
 uint32 start = rand()%bs;
 uint32 end   = (start+5)%bs;
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
      const uint32 mtuSize = GetDataIO()()->GetPacketMaximumSize();
      if (mtuSize > 0)
      {
         const ByteBuffer * bb = _sendBuffer._buffer();
         int32 numSent         = GetDataIO()()->Write(bb->GetBuffer(), bb->GetNumBytes());
         if (numSent > 0)
         {
            maxBytes   = (maxBytes>(uint32)numSent)?(maxBytes-numSent):0;
            sentBytes += numSent;
            _sendBuffer.Reset();
         }
         else if (numSent < 0) SetHosed();
         else break;
      }
      else
      {
         if (SendMoreData(sentBytes, maxBytes) != B_NO_ERROR) break;  // output buffer is temporarily full
         if (_sendBuffer._offset == _sendBuffer._buffer()->GetNumBytes()) _sendBuffer.Reset();
      }
   }
   return IsHosed() ? -1 : sentBytes;
}

int32 
MessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   const uint32 hs = GetHeaderSize();
   bool firstTime = true;  // always go at least once, to avoid live-lock
   int32 readBytes = 0;
   while((maxBytes > 0)&&(IsHosed() == false)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      const uint32 mtuSize = GetDataIO()()->GetPacketMaximumSize();
      if (mtuSize > 0)
      {
         // For UDP-style I/O, we'll read all header data and body data at once from a single packet
         if (_recvBuffer._buffer() == NULL)
         {
            (void) _scratchRecvBuffer.SetNumBytes(sizeof(_scratchRecvBufferBytes), false);   // return our scratch buffer to its "full size" in case it was sized smaller earlier
            _recvBuffer._offset = 0;
            _recvBuffer._buffer = (mtuSize<=_scratchRecvBuffer.GetNumBytes()) ? ByteBufferRef(&_scratchRecvBuffer,false) : GetByteBufferFromPool(mtuSize);
            if (_recvBuffer._buffer() == NULL) {SetHosed(); break;}  // out of memory?
         }

         int32 numRead = GetDataIO()()->Read(_recvBuffer._buffer()->GetBuffer(), mtuSize);
              if (numRead < 0) {SetHosed(); break;}
         else if (numRead > 0)
         {
            readBytes += numRead;
            maxBytes   = (maxBytes>(uint32)numRead)?(maxBytes-numRead):0;
            (void) _recvBuffer._buffer()->SetNumBytes(numRead, true);  // trim off any unused bytes

            // Finished receiving message bytes... now reconstruct that bad boy!
            MessageRef msg = UnflattenHeaderAndMessage(_recvBuffer._buffer);
            _recvBuffer.Reset();  // reset our state for the next one!
            if (msg())  // for UDP, unexpected data shouldn't be fatal
            {
               if (_unflattenedCallback) _unflattenedCallback(msg, _unflattenedCallbackData);
               receiver.CallMessageReceivedFromGateway(msg);
            }
         }
         else break;
      }
      else
      {
         // For TCP-style I/O, we need to read the header first, and then the body, in as many steps as it takes
         if (_recvBuffer._buffer() == NULL)
         {
            (void) _scratchRecvBuffer.SetNumBytes(sizeof(_scratchRecvBufferBytes), false);   // return our scratch buffer to its "full size" in case it was sized smaller earlier
            _recvBuffer._offset = 0;
            _recvBuffer._buffer = (hs<=_scratchRecvBuffer.GetNumBytes()) ? ByteBufferRef(&_scratchRecvBuffer,false) : GetByteBufferFromPool(hs);
            if (_recvBuffer._buffer() == NULL) {SetHosed(); break;}  // out of memory?
         }

         ByteBuffer * bb = _recvBuffer._buffer();  // guaranteed not to be NULL, if we got here!
         if (_recvBuffer._offset < hs)
         { 
            // We don't have the entire header yet, so try and read some more of it
            if (ReceiveMoreData(readBytes, maxBytes, hs) != B_NO_ERROR) break;
            if (_recvBuffer._offset >= hs)  // how about now?
            {
               // Now that we have the full header, parse it and allocate space for the message-body-bytes per its instructions
               int32 bodySize = GetBodySize(bb->GetBuffer());
               if ((bodySize >= 0)&&(((uint32)bodySize) <= _maxIncomingMessageSize))
               {
                  int32 availableBodyBytes = bb->GetNumBytes()-hs;
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
            if ((_recvBuffer._offset < bb->GetNumBytes())&&(ReceiveMoreData(readBytes, maxBytes, bb->GetNumBytes()) != B_NO_ERROR)) break;
            if (_recvBuffer._offset == bb->GetNumBytes())
            {
               // Finished receiving message bytes... now reconstruct that bad boy!
               MessageRef msg = UnflattenHeaderAndMessage(_recvBuffer._buffer);
               _recvBuffer.Reset();  // reset our state for the next one!
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

   int32 attemptSize = muscleMin(maxBytes, (uint32)((maxArraySize>_recvBuffer._offset)?(maxArraySize-_recvBuffer._offset):0));
   int32 numRead     = GetDataIO()()->Read(_recvBuffer._buffer()->GetBuffer()+_recvBuffer._offset, attemptSize);
   if (numRead >= 0)
   {
      maxBytes            -= numRead;
      readBytes           += numRead;
      _recvBuffer._offset += numRead;
   }
   else SetHosed();

   return (numRead < attemptSize) ? B_ERROR : B_NO_ERROR;
}

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
ZLibCodec * 
MessageIOGateway ::
GetCodec(int32 newEncoding, ZLibCodec * & setCodec) const
{
   TCHECKPOINT;

   if (muscleInRange(newEncoding, (int32)MUSCLE_MESSAGE_ENCODING_ZLIB_1, (int32)MUSCLE_MESSAGE_ENCODING_ZLIB_9))
   {
      int newLevel = (newEncoding-MUSCLE_MESSAGE_ENCODING_ZLIB_1)+1;
      if ((setCodec == NULL)||(newLevel != setCodec->GetCompressionLevel()))
      {
         delete setCodec;  // oops, encoding change!  Throw out the old codec, if any
         setCodec = newnothrow ZLibCodec(newLevel);
         if (setCodec == NULL) WARN_OUT_OF_MEMORY;
      }
      return setCodec;
   }
   return NULL;
}
#endif

ByteBufferRef 
MessageIOGateway ::
FlattenHeaderAndMessage(const MessageRef & msgRef) const
{
   TCHECKPOINT;

   ByteBufferRef ret;
   if (msgRef())
   {
      uint32 hs = GetHeaderSize();
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
            uint32 * lhb = (uint32 *) ret()->GetBuffer();
            lhb[0] = B_HOST_TO_LENDIAN_INT32(ret()->GetNumBytes()-hs);
            lhb[1] = B_HOST_TO_LENDIAN_INT32(encoding);
         }
      }
   }
   return ret;
}

MessageRef 
MessageIOGateway ::
UnflattenHeaderAndMessage(const ByteBufferRef & bufRef) const
{
   TCHECKPOINT;

   MessageRef ret;
   if (bufRef())
   {
      ret = GetMessageFromPool();
      if (ret())
      {
         uint32 offset = GetHeaderSize();

         const uint32 * lhb = (const uint32 *) bufRef()->GetBuffer();
         if ((offset+((uint32) B_LENDIAN_TO_HOST_INT32(lhb[0]))) != bufRef()->GetNumBytes())
         {
            LogTime(MUSCLE_LOG_DEBUG, "MessageIOGateway %p:  Unexpected lhb size " UINT32_FORMAT_SPEC ", expected " INT32_FORMAT_SPEC "\n", this, (uint32) B_LENDIAN_TO_HOST_INT32(lhb[0]), bufRef()->GetNumBytes()-offset);
            return MessageRef();
         }

         int32 encoding = B_LENDIAN_TO_HOST_INT32(lhb[1]);

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

         if ((bb == NULL)||(ret()->Unflatten(bb->GetBuffer()+offset, bb->GetNumBytes()-offset) != B_NO_ERROR)) ret.Reset();
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
   const uint32 * h = (const uint32 *) headerBuf;
   return (muscleInRange((uint32)B_LENDIAN_TO_HOST_INT32(h[1]), (uint32)MUSCLE_MESSAGE_ENCODING_DEFAULT, (uint32)MUSCLE_MESSAGE_ENCODING_END_MARKER-1)) ? (int32)(B_LENDIAN_TO_HOST_INT32(h[0])) : -1;
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
   return ((pingMsg())&&(pingMsg()->AddInt32("_miosp", syncPingCounter) == B_NO_ERROR)) ? pingMsg : MessageRef();
}

status_t MessageIOGateway :: ExecuteSynchronousMessaging(AbstractGatewayMessageReceiver * optReceiver, uint64 timeoutPeriod)
{
   const DataIO * dio = GetDataIO()();
   if ((dio == NULL)||(dio->GetReadSelectSocket().GetFileDescriptor() < 0)||(dio->GetWriteSelectSocket().GetFileDescriptor() < 0)) return B_ERROR;

   MessageRef pingMsg = CreateSynchronousPingMessage(_syncPingCounter);
   if ((pingMsg())&&(AddOutgoingMessage(pingMsg) == B_NO_ERROR))
   {
      _pendingSyncPingCounter = _syncPingCounter;
      _syncPingCounter++;
      return AbstractMessageIOGateway::ExecuteSynchronousMessaging(optReceiver, timeoutPeriod);
   }
   else return B_ERROR;
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

   uint64 timeBeforeConnect = GetRunTime64();
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
      if ((AddOutgoingMessage(MessageRef(const_cast<Message *>(&requestMessage), false)) == B_NO_ERROR)&&(ExecuteSynchronousMessaging(&receiver, timeoutPeriod) == B_NO_ERROR)) ret = receiver.HasItems() ? receiver.Head() : GetMessageFromPool();
      SetDataIO(oldIO);  // restore any previous I/O
   }
   return ret;
}

status_t MessageIOGateway :: ExecuteSynchronousMessageSend(const Message & requestMessage, const IPAddressAndPort & targetIAP, uint64 timeoutPeriod)
{
   status_t ret = B_ERROR;
   uint64 timeBeforeConnect = GetRunTime64();
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
      if (AddOutgoingMessage(MessageRef(const_cast<Message *>(&requestMessage), false)) == B_NO_ERROR) 
      {
         NestCountGuard ncg(_noRPCReply);  // so that we'll return as soon as we've sent the request Message, and not wait for a reply Message.
         ret = ExecuteSynchronousMessaging(&receiver, timeoutPeriod);
      }
      SetDataIO(oldIO);
   }
   return ret;
}

CountedMessageIOGateway :: CountedMessageIOGateway(int32 outgoingEncoding) : MessageIOGateway(outgoingEncoding), _outgoingByteCount(0)
{
   // empty
}

status_t CountedMessageIOGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   if (MessageIOGateway::AddOutgoingMessage(messageRef) != B_NO_ERROR) return B_ERROR;

   uint32 msgSize = messageRef()?messageRef()->FlattenedSize():0;
   if (GetOutgoingMessageQueue().GetNumItems() > 1) _outgoingByteCount += msgSize;
                                               else _outgoingByteCount  = msgSize;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access
   return B_NO_ERROR;
}

void CountedMessageIOGateway :: Reset()
{
   MessageIOGateway::Reset();
   _outgoingByteCount = 0;
}

status_t CountedMessageIOGateway :: PopNextOutgoingMessage(MessageRef & ret)
{
   if (MessageIOGateway::PopNextOutgoingMessage(ret) != B_NO_ERROR) return B_ERROR;

   if (GetOutgoingMessageQueue().HasItems())
   {
      uint32 retSize = ret()?ret()->FlattenedSize():0;
      _outgoingByteCount = (retSize<_outgoingByteCount) ? (_outgoingByteCount-retSize) : 0;  // paranoia to avoid underflow
   }
   else _outgoingByteCount = 0;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access

   return B_NO_ERROR;
}

}; // end namespace muscle
