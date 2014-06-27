/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/UDPSocketDataIO.h"  // for retrieving the source IP address and port, where possible
#include "iogateway/PacketTunnelIOGateway.h"

namespace muscle {

// Each chunk header has the following fields in it:
//    uint32 magic_number
//    uint32 source_exclusion_id
//    uint32 message_id
//    uint32 subchunk_offset
//    uint32 subchunk_size
//    uint32 message_total_size
static const uint32 FRAGMENT_HEADER_SIZE = 6*(sizeof(uint32));

// The maximum number of bytes of memory to keep in a ByteBuffer to avoid reallocations
static const uint32 MAX_CACHE_SIZE = 20*1024;

PacketTunnelIOGateway :: PacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway, uint32 maxTransferUnit, uint32 magic) : _magic(magic), _maxTransferUnit(muscleMax(maxTransferUnit, FRAGMENT_HEADER_SIZE+1)), _allowMiscData(false), _sexID(0), _slaveGateway(slaveGateway), _outputPacketSize(0), _sendMessageIDCounter(0), _maxIncomingMessageSize(MUSCLE_NO_LIMIT)
{
   _fakeSendIO.SetBuffer(ByteBufferRef(&_fakeSendBuffer, false));
   // _fakeReceiveIO's buffer will be set just before it is used
}

int32 PacketTunnelIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   if (_inputPacketBuffer.SetNumBytes(_maxTransferUnit, false) != B_NO_ERROR) return -1;

   bool firstTime = true;
   uint32 totalBytesRead = 0;
   while((totalBytesRead < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      int32 bytesRead = GetDataIO()()->Read(_inputPacketBuffer.GetBuffer(), _inputPacketBuffer.GetNumBytes());
//printf("   READ " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes\n", bytesRead, _inputPacketBuffer.GetNumBytes());
      if (bytesRead > 0)
      {
         totalBytesRead += bytesRead;

         IPAddressAndPort fromIAP;
         UDPSocketDataIO * udpIO = dynamic_cast<UDPSocketDataIO *>(GetDataIO()());
         if (udpIO) fromIAP = udpIO->GetSourceOfLastReadPacket();

         const uint8 * p = (const uint8 *) _inputPacketBuffer.GetBuffer();
         if ((_allowMiscData)&&((bytesRead < (int32)FRAGMENT_HEADER_SIZE)||((uint32)B_LENDIAN_TO_HOST_INT32(*((const uint32 *)p)) != _magic)))
         {
            // If we're allowed to handle miscellaneous data, we'll just pass it on through verbatim
            ByteBuffer temp;
            temp.AdoptBuffer(bytesRead, const_cast<uint8 *>(p));
            HandleIncomingMessage(receiver, ByteBufferRef(&temp, false), fromIAP);
            (void) temp.ReleaseBuffer();
         }
         else
         {
            const uint8 * invalidByte = p+bytesRead;
            while(invalidByte-p >= (int32)FRAGMENT_HEADER_SIZE)
            {
               const uint32 * h32 = (const uint32 *) p;
               uint32 magic       = B_LENDIAN_TO_HOST_INT32(h32[0]);
               uint32 sexID       = B_LENDIAN_TO_HOST_INT32(h32[1]);
               uint32 messageID   = B_LENDIAN_TO_HOST_INT32(h32[2]);
               uint32 offset      = B_LENDIAN_TO_HOST_INT32(h32[3]);
               uint32 chunkSize   = B_LENDIAN_TO_HOST_INT32(h32[4]);
               uint32 totalSize   = B_LENDIAN_TO_HOST_INT32(h32[5]);
//printf("   PARSE magic=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " sex=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " messageID=" UINT32_FORMAT_SPEC " offset=" UINT32_FORMAT_SPEC " chunkSize=" UINT32_FORMAT_SPEC " totalSize=" UINT32_FORMAT_SPEC "\n", magic, _magic, sexID, _sexID, messageID, offset, chunkSize, totalSize);

               p += FRAGMENT_HEADER_SIZE;
               if ((magic == _magic)&&((_sexID == 0)||(_sexID != sexID))&&((invalidByte-p >= (int32)chunkSize)&&(totalSize <= _maxIncomingMessageSize)))
               {
                  ReceiveState * rs = _receiveStates.Get(fromIAP);
                  if (rs == NULL)
                  {
                     if (offset == 0) rs = _receiveStates.PutAndGet(fromIAP, ReceiveState(messageID));
                     if (rs)
                     {
                        rs->_buf = GetByteBufferFromPool(totalSize);
                        if (rs->_buf() == NULL)
                        {
                           _receiveStates.Remove(fromIAP);
                           rs = NULL;
                        }
                     }
                  }
                  if (rs)
                  {
                     if ((offset == 0)||(messageID != rs->_messageID))
                     {
                        // A new message... start receiving it (but only if we are starting at the beginning)
                        rs->_messageID = messageID;
                        rs->_offset    = 0;
                        rs->_buf()->SetNumBytes(totalSize, false);
                     }

                     uint32 rsSize = rs->_buf()->GetNumBytes();
//printf("  CHECK:  offset=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " %s\n", offset, rs->_offset, (offset==rs->_offset)?"":"DISCONTINUITY!!!");
                     if ((messageID == rs->_messageID)&&(totalSize == rsSize)&&(offset == rs->_offset)&&(offset+chunkSize <= rsSize))
                     {
                        memcpy(rs->_buf()->GetBuffer()+offset, p, chunkSize);
                        rs->_offset += chunkSize;
                        if (rs->_offset == rsSize) 
                        {
                           HandleIncomingMessage(receiver, rs->_buf, fromIAP);
                           rs->_offset = 0;
                           rs->_buf()->Clear(rsSize > MAX_CACHE_SIZE);
                        }
                     }
                     else 
                     {
                        LogTime(MUSCLE_LOG_DEBUG, "Unknown fragment (" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") received from %s, ignoring it.\n", messageID, offset, chunkSize, totalSize, fromIAP.ToString()());
                        rs->_offset = 0;
                        rs->_buf()->Clear(rsSize > MAX_CACHE_SIZE);
                     }
                  }
                  p += chunkSize;
               }
               else break;
            }
         }
      }
      else if (bytesRead < 0) return -1;
      else break;
   }
   return totalBytesRead;
}

void PacketTunnelIOGateway :: HandleIncomingMessage(AbstractGatewayMessageReceiver & receiver, const ByteBufferRef & buf, const IPAddressAndPort & fromIAP)
{
   if (_slaveGateway())
   {
      DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

      _fakeReceiveIO.SetBuffer(buf); (void) _fakeReceiveIO.Seek(0, DataIO::IO_SEEK_SET);
      _slaveGateway()->SetDataIO(DataIORef(&_fakeReceiveIO, false));

      uint32 slaveBytesRead = 0;
      while(slaveBytesRead < buf()->GetNumBytes())
      {
         _scratchReceiver    = &receiver;
         _scratchReceiverArg = (void *) &fromIAP;
         int32 nextBytesRead = _slaveGateway()->DoInput(*this, buf()->GetNumBytes()-slaveBytesRead);
         if (nextBytesRead > 0) slaveBytesRead += nextBytesRead;
                           else break;
      }

      _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
      _fakeReceiveIO.SetBuffer(ByteBufferRef());
   }
   else
   {
      MessageRef inMsg = GetMessageFromPool();
      if ((inMsg())&&(inMsg()->UnflattenFromByteBuffer(*buf()) == B_NO_ERROR)) receiver.CallMessageReceivedFromGateway(inMsg, (void *) &fromIAP);
   }
}

int32 PacketTunnelIOGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if (_outputPacketBuffer.SetNumBytes(_maxTransferUnit, false) != B_NO_ERROR) return -1;

   uint32 totalBytesWritten = 0;
   bool firstTime = true;
   while((totalBytesWritten < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      // Step 1:  Add as much data to our output packet buffer as we can fit into it
      while((_outputPacketSize+FRAGMENT_HEADER_SIZE < _maxTransferUnit)&&(HasBytesToOutput()))
      {
         // Demand-create the next send-buffer
         if (_currentOutputBuffer() == NULL)
         {
            MessageRef msg;
            if (GetOutgoingMessageQueue().RemoveHead(msg) == B_NO_ERROR)
            {
               _currentOutputBufferOffset = 0; 
               _currentOutputBuffer.Reset();

               if (_slaveGateway())
               {
                  DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

                  // Get the slave gateway to generate its output into our ByteBuffer
                  _fakeSendBuffer.SetNumBytes(0, false);
                  _fakeSendIO.Seek(0, DataIO::IO_SEEK_SET);
                  _slaveGateway()->SetDataIO(DataIORef(&_fakeSendIO, false));
                  _slaveGateway()->AddOutgoingMessage(msg);
                  while(_slaveGateway()->DoOutput() > 0) {/* empty */}

                  _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
                  _currentOutputBuffer.SetRef(&_fakeSendBuffer, false);
               }
               else if (_fakeSendBuffer.SetNumBytes(msg()->FlattenedSize(), false) == B_NO_ERROR)
               {
                  // Default algorithm:  Just flatten the Message into the buffer
                  msg()->Flatten(_fakeSendBuffer.GetBuffer());
                  _currentOutputBuffer.SetRef(&_fakeSendBuffer, false);
               }
            }
         }
         if (_currentOutputBuffer() == NULL) break;   // oops, out of mem?

         uint32 sbSize          = _currentOutputBuffer()->GetNumBytes();
         uint32 dataBytesToSend = muscleMin(_maxTransferUnit-(_outputPacketSize+FRAGMENT_HEADER_SIZE), sbSize-_currentOutputBufferOffset);

         uint8  * p   = ((uint8 *)_outputPacketBuffer.GetBuffer()) + _outputPacketSize;
         uint32 * h32 = (uint32 *) p;
         h32[0] = B_HOST_TO_LENDIAN_INT32(_magic);                      // a well-known magic number, for sanity checking
         h32[1] = B_HOST_TO_LENDIAN_INT32(_sexID);                      // source exclusion ID
         h32[2] = B_HOST_TO_LENDIAN_INT32(_sendMessageIDCounter);       // message ID tag so the receiver can track what belongs where
         h32[3] = B_HOST_TO_LENDIAN_INT32(_currentOutputBufferOffset);  // start offset (within its message) for this sub-chunk
         h32[4] = B_HOST_TO_LENDIAN_INT32(dataBytesToSend); // size of this sub-chunk
         h32[5] = B_HOST_TO_LENDIAN_INT32(sbSize);          // total size of this message
//printf("CREATING PACKET magic=" UINT32_FORMAT_SPEC " msgID=" UINT32_FORMAT_SPEC " offset=" UINT32_FORMAT_SPEC " chunkSize=" UINT32_FORMAT_SPEC " totalSize=" UINT32_FORMAT_SPEC "\n", _magic, _sendMessageIDCounter, _currentOutputBufferOffset, dataBytesToSend, sbSize);
         memcpy(p+FRAGMENT_HEADER_SIZE, _currentOutputBuffer()->GetBuffer()+_currentOutputBufferOffset, dataBytesToSend);

         _outputPacketSize += (FRAGMENT_HEADER_SIZE+dataBytesToSend);
         _currentOutputBufferOffset += dataBytesToSend;
         if (_currentOutputBufferOffset == sbSize)
         {
            _currentOutputBuffer.Reset();
            _fakeSendBuffer.Clear(_fakeSendBuffer.GetNumBytes() > MAX_CACHE_SIZE);  // don't keep too much memory around!
            _sendMessageIDCounter++;
         }
      }

      // Step 2:  If we have a non-empty packet to send, send it!
      if (_outputPacketSize > 0)
      {
         // If bytesWritten is set to zero, we just hold this buffer until our next call.
         int32 bytesWritten = GetDataIO()()->Write(_outputPacketBuffer.GetBuffer(), _outputPacketSize);
//printf("WROTE " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes %s\n", bytesWritten, _outputPacketSize, (bytesWritten==(int32)_outputPacketSize)?"":"******** SHORT ***********");
         if (bytesWritten > 0)
         {
            if (bytesWritten != (int32)_outputPacketSize) LogTime(MUSCLE_LOG_ERROR, "PacketTunnelIOGateway::DoOutput():  Short write!  (" INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes)\n", bytesWritten, _outputPacketSize);
            _outputPacketBuffer.Clear();
            _outputPacketSize = 0;
            totalBytesWritten += bytesWritten;
         }
         else if (bytesWritten == 0) break;  // no more space to write, for now
         else return -1;
      }
      else break;  // nothing more to do! 
   }
   return totalBytesWritten;
}

}; // end namespace muscle
