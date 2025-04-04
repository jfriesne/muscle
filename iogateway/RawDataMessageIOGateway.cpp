/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/RawDataMessageIOGateway.h"

namespace muscle {

RawDataMessageIOGateway ::
RawDataMessageIOGateway(uint32 minChunkSize, uint32 maxChunkSize)
   : _sendBuf(NULL)        // initialized only to make Coverity happy
   , _sendBufLength(0)     // same
   , _sendBufIndex(0)      // same
   , _sendBufByteOffset(0) // same
   , _recvBuf(NULL)        // same
   , _recvBufLength(0)     // same
   , _recvBufByteOffset(0) // same
   , _recvScratchSpace(NULL)
   , _recvScratchSpaceSize(0)
   , _minChunkSize(minChunkSize)
   , _maxChunkSize(maxChunkSize)
   , _receiveTimestampingEnabled(false)
{
   // empty
}

RawDataMessageIOGateway ::
~RawDataMessageIOGateway()
{
   delete [] _recvScratchSpace;
}

io_status_t
RawDataMessageIOGateway ::
DoOutputImplementation(uint32 maxBytes)
{
   TCHECKPOINT;

   const Message * msg = _sendMsgRef();
   if (msg == NULL)
   {
      // try to get the next message from our queue
      _sendMsgRef = PopNextOutgoingMessage();
      msg = _sendMsgRef();
      _sendBufLength = _sendBufIndex = _sendBufByteOffset = -1;
   }

   if (msg)
   {
      if ((_sendBufByteOffset < 0)||(_sendBufByteOffset >= _sendBufLength))
      {
         // Try to get the next field from our message message
         if (msg->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, ++_sendBufIndex, &_sendBuf, (uint32*)(&_sendBufLength)).IsOK()) _sendBufByteOffset = 0;
         else
         {
            _sendMsgRef.Reset();  // no more data available?  Go to the next message then.
            return DoOutputImplementation(maxBytes);
         }
      }

      // At this point we are guaranteed that (_sendBufByteOffset >= 0 or else we would have returned, above
      if (_sendBufByteOffset < _sendBufLength)
      {
         const uint32 mtuSize = GetMaximumPacketSize();
         if (mtuSize > 0)
         {
            // UDP mode -- send each data chunk as its own UDP packet
            PacketDataIO * pdio = GetPacketDataIO();  // guaranteed non-NULL because (mtuSize > 0)
            const uint32 sendSize = muscleMin((uint32)_sendBufLength, mtuSize);
            IPAddressAndPort packetDestIAP;
            const io_status_t bytesWritten = msg->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, packetDestIAP).IsOK()
                                           ? pdio->WriteTo(_sendBuf, sendSize, packetDestIAP)
                                           : pdio->Write(  _sendBuf, sendSize);
            MRETURN_ON_ERROR(bytesWritten);
            if (bytesWritten.GetByteCount() > 0)
            {
               _sendBufByteOffset = _sendBufLength;  // We don't support partial sends for UDP style, so pretend the whole thing was sent
               return bytesWritten + DoOutputImplementation((maxBytes>(uint32)bytesWritten.GetByteCount())?(maxBytes-bytesWritten.GetByteCount()):0);
            }
         }
         else
         {
            // TCP mode -- send as much as we can of the current data block
            const io_status_t bytesWritten = GetDataIO()() ? GetDataIO()()->Write(&((char *)_sendBuf)[_sendBufByteOffset], muscleMin(maxBytes, (uint32) (_sendBufLength-_sendBufByteOffset))) : io_status_t(B_BAD_OBJECT);
            MRETURN_ON_ERROR(bytesWritten);
            if (bytesWritten.GetByteCount() > 0)
            {
               _sendBufByteOffset += bytesWritten.GetByteCount();
               return bytesWritten + DoOutputImplementation(maxBytes-bytesWritten.GetByteCount());
            }
         }
      }
   }

   return io_status_t();
}


io_status_t
RawDataMessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   const uint32 mtuSize = GetMaximumPacketSize();
   if (mtuSize > 0)
   {
      io_status_t totalBytesRead;

      // UDP mode:  Each UDP packet is represented as a Message containing one data chunk
      while(maxBytes > 0)
      {
         ByteBufferRef bufRef = GetByteBufferFromPool(mtuSize);
         MRETURN_ON_ERROR(bufRef);

         IPAddressAndPort packetSource;
         const io_status_t bytesRead = GetPacketDataIO()->ReadFrom(bufRef()->GetBuffer(), mtuSize, packetSource);
         MTALLY_BYTES_OR_RETURN_ON_ERROR_OR_BREAK(totalBytesRead, bytesRead);

         bufRef()->TruncateToLength(bytesRead.GetByteCount());
         MessageRef msg = GetMessageFromPool(PR_COMMAND_RAW_DATA);
         if ((msg())&&(msg()->AddFlat(PR_NAME_DATA_CHUNKS, bufRef).IsOK())
           &&((GetPacketRemoteLocationTaggingEnabled() == false)||(msg()->AddFlat(PR_NAME_PACKET_REMOTE_LOCATION, packetSource).IsOK()))
           &&((GetReceiveTimestampingEnabled()         == false)||(msg()->AddInt64(PR_NAME_DATA_TIMESTAMP,      GetRunTime64()).IsOK())))
         {
            maxBytes = (maxBytes>(uint32)bytesRead.GetByteCount()) ? (maxBytes-bytesRead.GetByteCount()) : 0;
            receiver.CallMessageReceivedFromGateway(msg);
         }
      }
      return totalBytesRead;
   }
   else
   {
      // TCP mode:  read in as a stream
      if (_minChunkSize > 0)
      {
         // Minimum-chunk-size mode:  we read bytes directly into the Message's data field until it is full, then
         // forward that message on to the user code and start the next.  Advantage of this is:  no data-copying necessary!
         if (_recvMsgRef() == NULL)
         {
            MessageRef newMsg = GetMessageFromPool(PR_COMMAND_RAW_DATA);
            MRETURN_ON_ERROR(newMsg);
            MRETURN_ON_ERROR(newMsg()->AddData(        PR_NAME_DATA_CHUNKS, B_RAW_TYPE, NULL,      _minChunkSize));
            MRETURN_ON_ERROR(newMsg()->FindDataPointer(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, &_recvBuf, (uint32*)&_recvBufLength));

            _recvBufByteOffset = 0;
            _recvMsgRef        = std_move_if_available(newMsg);
         }

         const io_status_t bytesRead = GetDataIO()() ? GetDataIO()()->Read(&((char*)_recvBuf)[_recvBufByteOffset], muscleMin(maxBytes, (uint32)(_recvBufLength-_recvBufByteOffset))) : io_status_t(B_BAD_OBJECT);
         MRETURN_ON_ERROR(bytesRead);

         if (bytesRead.GetByteCount() > 0)
         {
            if ((GetReceiveTimestampingEnabled())&&(_recvBufByteOffset == 0)) MRETURN_ON_ERROR(_recvMsgRef()->AddInt64(PR_NAME_DATA_TIMESTAMP, GetRunTime64()));

            _recvBufByteOffset += bytesRead.GetByteCount();
            if (_recvBufByteOffset == _recvBufLength)
            {
               // This buffer is full... forward it on to the user, and start receiving the next one.
               receiver.CallMessageReceivedFromGateway(_recvMsgRef);
               _recvMsgRef.Reset();

               return bytesRead+(IsSuggestedTimeSliceExpired() ? io_status_t() : DoInputImplementation(receiver, maxBytes-bytesRead.GetByteCount()));
            }
         }
         return bytesRead;
      }
      else
      {
         // Immediate-forward mode... Read data into a temporary buffer, and immediately forward it to the user.
         if (_recvScratchSpace == NULL)
         {
            // demand-allocate a scratch buffer
            const uint32 maxScratchSpaceSize = 8192;  // we probably won't ever get more than this much at once anyway
            _recvScratchSpaceSize = (_maxChunkSize < maxScratchSpaceSize) ? _maxChunkSize : maxScratchSpaceSize;
            _recvScratchSpace     = newnothrow_array(uint8, _recvScratchSpaceSize);
            MRETURN_OOM_ON_NULL(_recvScratchSpace);
         }

         const io_status_t bytesRead = GetDataIO()() ? GetDataIO()()->Read(_recvScratchSpace, muscleMin(_recvScratchSpaceSize, maxBytes)) : io_status_t(B_BAD_OBJECT);
         MRETURN_ON_ERROR(bytesRead);

         if (bytesRead.GetByteCount() > 0)
         {
            MessageRef ref = GetMessageFromPool(PR_COMMAND_RAW_DATA);
            MRETURN_ON_ERROR(ref);

            if (GetReceiveTimestampingEnabled()) MRETURN_ON_ERROR(_recvMsgRef()->AddInt64(PR_NAME_DATA_TIMESTAMP, GetRunTime64()));
            MRETURN_ON_ERROR(ref()->AddData(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, _recvScratchSpace, bytesRead.GetByteCount()));

            receiver.CallMessageReceivedFromGateway(ref);

            // note:  don't recurse here!  It would be bad (tm) on a fast feed since we might never return
         }
         return bytesRead;
      }
   }
}

bool
RawDataMessageIOGateway ::
HasBytesToOutput() const
{
   return ((_sendMsgRef())||(GetOutgoingMessageQueue().HasItems()));
}

void
RawDataMessageIOGateway ::
Reset()
{
   TCHECKPOINT;

   AbstractMessageIOGateway::Reset();
   _sendMsgRef.Reset();
   _recvMsgRef.Reset();
}

MessageRef
RawDataMessageIOGateway ::
PopNextOutgoingMessage()
{
   return GetOutgoingMessageQueue().RemoveHeadWithDefault();
}

CountedRawDataMessageIOGateway :: CountedRawDataMessageIOGateway(uint32 minChunkSize, uint32 maxChunkSize)
   : RawDataMessageIOGateway(minChunkSize, maxChunkSize)
   , _outgoingByteCount(0)
{
   // empty
}

status_t CountedRawDataMessageIOGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   MRETURN_ON_ERROR(RawDataMessageIOGateway::AddOutgoingMessage(messageRef));

   const uint32 msgSize = GetNumRawBytesInMessage(messageRef);
   if (GetOutgoingMessageQueue().GetNumItems() > 1) _outgoingByteCount += msgSize;
                                               else _outgoingByteCount  = msgSize;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access
   return B_NO_ERROR;
}

void CountedRawDataMessageIOGateway :: Reset()
{
   RawDataMessageIOGateway::Reset();
   _outgoingByteCount = 0;
}

MessageRef CountedRawDataMessageIOGateway :: PopNextOutgoingMessage()
{
   MessageRef ret = RawDataMessageIOGateway::PopNextOutgoingMessage();
   if (GetOutgoingMessageQueue().HasItems())
   {
      const uint32 retSize = GetNumRawBytesInMessage(ret);
      _outgoingByteCount = (retSize<_outgoingByteCount) ? (_outgoingByteCount-retSize) : 0;  // paranoia to avoid underflow
   }
   else _outgoingByteCount = 0;  // semi-paranoia about meddling via GetOutgoingMessageQueue() access

   return ret;
}

uint32 CountedRawDataMessageIOGateway :: GetNumRawBytesInMessage(const MessageRef & messageRef) const
{
   if (messageRef())
   {
      uint32 count = 0;
      const void * junk;
      uint32 temp;
      for (int32 i=0; messageRef()->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, i, &junk, &temp).IsOK(); i++) count += temp;
      return count;
   }
   else return 0;
}

} // end namespace muscle
