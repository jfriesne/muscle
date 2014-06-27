/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "iogateway/RawDataMessageIOGateway.h"

namespace muscle {

RawDataMessageIOGateway ::
RawDataMessageIOGateway(uint32 minChunkSize, uint32 maxChunkSize) : _recvScratchSpace(NULL), _minChunkSize(minChunkSize), _maxChunkSize(maxChunkSize)
{
   // empty
}

RawDataMessageIOGateway ::
~RawDataMessageIOGateway()
{
   delete [] _recvScratchSpace;
}

int32 
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
         if (msg->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, ++_sendBufIndex, &_sendBuf, (uint32*)(&_sendBufLength)) == B_NO_ERROR) _sendBufByteOffset = 0;
         else 
         {
            _sendMsgRef.Reset();  // no more data available?  Go to the next message then.
            return DoOutputImplementation(maxBytes);
         }
      }

      if ((_sendBufByteOffset >= 0)&&(_sendBufByteOffset < _sendBufLength))
      {
         uint32 mtuSize = GetDataIO()()->GetPacketMaximumSize();
         if (mtuSize > 0)
         {
            // UDP mode -- send each data chunk as its own UDP packet
            int32 bytesWritten = GetDataIO()()->Write(_sendBuf, muscleMin((uint32)_sendBufLength, mtuSize));
            if (bytesWritten > 0)
            {
               _sendBufByteOffset = _sendBufLength;  // We don't support partial sends for UDP style, so pretend the whole thing was sent
               int32 subRet = DoOutputImplementation((maxBytes>(uint32)bytesWritten)?(maxBytes-bytesWritten):0);
               return (subRet >= 0) ? subRet+bytesWritten : -1;
            }
            else if (bytesWritten < 0) return -1;
         }
         else
         {
            // TCP mode -- send as much as we can of the current data block
            int32 bytesWritten = GetDataIO()()->Write(&((char *)_sendBuf)[_sendBufByteOffset], muscleMin(maxBytes, (uint32) (_sendBufLength-_sendBufByteOffset)));
                 if (bytesWritten < 0) return -1;
            else if (bytesWritten > 0)
            {
               _sendBufByteOffset += bytesWritten;
               int32 subRet = DoOutputImplementation(maxBytes-bytesWritten);
               return (subRet >= 0) ? subRet+bytesWritten : -1;
            }
         }
      }
   }
   return 0;
}


int32 
RawDataMessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   uint32 mtuSize = GetDataIO()()->GetPacketMaximumSize();
   int32 ret = 0;
   if (mtuSize > 0)
   {
      // UDP mode:  Each UDP packet is represented as a Message containing one data chunk
      while(maxBytes > 0)
      {
         ByteBufferRef bufRef = GetByteBufferFromPool(mtuSize);
         if (bufRef() == NULL) return -1;  // out of memory?

         int32 bytesRead = GetDataIO()()->Read(bufRef()->GetBuffer(), mtuSize);
         if (bytesRead > 0)
         {
            (void) bufRef()->SetNumBytes(bytesRead, true);
            MessageRef msg = GetMessageFromPool(PR_COMMAND_RAW_DATA);
            if ((msg())&&(msg()->AddFlat(PR_NAME_DATA_CHUNKS, bufRef) == B_NO_ERROR))
            {
               ret += bytesRead;
               maxBytes = (maxBytes>(uint32)bytesRead) ? (maxBytes-bytesRead) : 0;         
               receiver.CallMessageReceivedFromGateway(msg);  // Call receive immediately; that way he can found out the source via GetDataIO()()->GetSourceOfLastReadPacket() if necessary
            }
         }
         else if (bytesRead < 0) return -1;
         else break;
      }
   }
   else
   {
      // TCP mode:  read in as a stream
      Message * inMsg = _recvMsgRef();
      if (_minChunkSize > 0)
      {
         // Minimum-chunk-size mode:  we read bytes directly into the Message's data field until it is full, then 
         // forward that message on to the user code and start the next.  Advantage of this is:  no data-copying necessary!
         if (inMsg == NULL)
         {
            _recvMsgRef = GetMessageFromPool(PR_COMMAND_RAW_DATA);
            inMsg = _recvMsgRef();
            if (inMsg)
            {
               if ((inMsg->AddData(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, NULL, _minChunkSize)                         == B_NO_ERROR)&&
                   (inMsg->FindDataPointer(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, &_recvBuf, (uint32*)&_recvBufLength) == B_NO_ERROR)) _recvBufByteOffset = 0;
               else
               {
                  _recvMsgRef.Reset();
                  return -1;  // oops, no mem?
               }
            }
         }
         if (inMsg)
         {
            int32 bytesRead = GetDataIO()()->Read(&((char*)_recvBuf)[_recvBufByteOffset], muscleMin(maxBytes, (uint32)(_recvBufLength-_recvBufByteOffset)));
                 if (bytesRead < 0) return -1;
            else if (bytesRead > 0)
            {
               ret += bytesRead;
               _recvBufByteOffset += bytesRead;
               if (_recvBufByteOffset == _recvBufLength)
               {
                  // This buffer is full... forward it on to the user, and start receiving the next one.
                  receiver.CallMessageReceivedFromGateway(_recvMsgRef);
                  _recvMsgRef.Reset();
                  int32 subRet = IsSuggestedTimeSliceExpired() ? 0 : DoInputImplementation(receiver, maxBytes-bytesRead);
                  return (subRet >= 0) ? (ret+subRet) : -1;
               }
            }
         }
      }
      else
      {
         // Immediate-forward mode... Read data into a temporary buffer, and immediately forward it to the user.
         if (_recvScratchSpace == NULL) 
         {
            // demand-allocate a scratch buffer
            const uint32 maxScratchSpaceSize = 8192;  // we probably won't ever get more than this much at once anyway
            _recvScratchSpaceSize = (_maxChunkSize < maxScratchSpaceSize) ? _maxChunkSize : maxScratchSpaceSize;
            _recvScratchSpace = newnothrow_array(uint8, _recvScratchSpaceSize);
            if (_recvScratchSpace == NULL)
            {
               WARN_OUT_OF_MEMORY;
               return -1;
            }
         }

         int32 bytesRead = GetDataIO()()->Read(_recvScratchSpace, muscleMin(_recvScratchSpaceSize, maxBytes));
              if (bytesRead < 0) return -1;
         else if (bytesRead > 0)
         {
            ret += bytesRead;
            MessageRef ref = GetMessageFromPool(PR_COMMAND_RAW_DATA);
            if ((ref())&&(ref()->AddData(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, _recvScratchSpace, bytesRead) == B_NO_ERROR)) receiver.CallMessageReceivedFromGateway(ref);
            // note:  don't recurse here!  It would be bad (tm) on a fast feed since we might never return
         }
      }
   }
   return ret;
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

CountedRawDataMessageIOGateway :: CountedRawDataMessageIOGateway(uint32 minChunkSize, uint32 maxChunkSize) : RawDataMessageIOGateway(minChunkSize, maxChunkSize), _outgoingByteCount(0)
{
   // empty
}

status_t CountedRawDataMessageIOGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   if (RawDataMessageIOGateway::AddOutgoingMessage(messageRef) != B_NO_ERROR) return B_ERROR;

   uint32 msgSize = GetNumRawBytesInMessage(messageRef);
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
      uint32 retSize = GetNumRawBytesInMessage(ret);
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
      for (int32 i=0; messageRef()->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, i, &junk, &temp) == B_NO_ERROR; i++) count += temp;
      return count;
   }
   else return 0;
}

}; // end namespace muscle
