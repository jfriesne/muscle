/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/PacketDataIO.h"  // for retrieving the source IP address and port, where possible
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

PacketTunnelIOGateway :: PacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway, uint32 maxTransferUnit, uint32 magic)
   : ProxyIOGateway(slaveGateway)
   , _magic(magic)
   , _maxTransferUnit(muscleMax(maxTransferUnit, FRAGMENT_HEADER_SIZE+1))
   , _allowMiscData(false)
   , _sexID(0)
   , _outputPacketSize(0)
   , _sendMessageIDCounter(0)
   , _maxIncomingMessageSize(MUSCLE_NO_LIMIT)
{
   // empty
}

int32 PacketTunnelIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   if (_inputPacketBuffer.SetNumBytes(_maxTransferUnit, false).IsError()) return -1;

   bool firstTime = true;
   uint32 totalBytesRead = 0;
   while((totalBytesRead < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      const int32 bytesRead = GetDataIO()() ? GetDataIO()()->Read(_inputPacketBuffer.GetBuffer(), _inputPacketBuffer.GetNumBytes()) : -1;
//printf("   READ " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes\n", bytesRead, _inputPacketBuffer.GetNumBytes());
      if (bytesRead > 0)
      {
         totalBytesRead += bytesRead;

         IPAddressAndPort fromIAP;
         const PacketDataIO * packetIO = dynamic_cast<PacketDataIO *>(GetDataIO()());
         if (packetIO) fromIAP = packetIO->GetSourceOfLastReadPacket();

         const uint8 * p = (const uint8 *) _inputPacketBuffer.GetBuffer();
         if ((_allowMiscData)&&((bytesRead < (int32)FRAGMENT_HEADER_SIZE)||(((uint32)B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(p))) != _magic)))
         {
            // If we're allowed to handle miscellaneous data, we'll just pass it on through verbatim
            HandleIncomingByteBuffer(receiver, p, bytesRead, fromIAP);
         }
         else
         {
            const uint8 * invalidByte = p+bytesRead;
            while(invalidByte-p >= (int32)FRAGMENT_HEADER_SIZE)
            {
               const uint32 magic     = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[0*sizeof(uint32)]));
               const uint32 sexID     = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[1*sizeof(uint32)]));
               const uint32 messageID = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[2*sizeof(uint32)]));
               const uint32 offset    = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[3*sizeof(uint32)]));
               const uint32 chunkSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[4*sizeof(uint32)]));
               const uint32 totalSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[5*sizeof(uint32)]));
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

                     const uint32 rsSize = rs->_buf()->GetNumBytes();
//printf("  CHECK:  offset=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " %s\n", offset, rs->_offset, (offset==rs->_offset)?"":"DISCONTINUITY!!!");
                     if ((messageID == rs->_messageID)&&(totalSize == rsSize)&&(offset == rs->_offset)&&(offset+chunkSize <= rsSize))
                     {
                        memcpy(rs->_buf()->GetBuffer()+offset, p, chunkSize);
                        rs->_offset += chunkSize;
                        if (rs->_offset == rsSize) 
                        {
                           HandleIncomingByteBuffer(receiver, rs->_buf, fromIAP);
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

int32 PacketTunnelIOGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if (_outputPacketBuffer.SetNumBytes(_maxTransferUnit, false).IsError()) return -1;

   uint32 totalBytesWritten = 0;
   bool firstTime = true;
   while((totalBytesWritten < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      // Step 1:  Add as much data to our output packet buffer as we can fit into it
      while((_outputPacketSize+FRAGMENT_HEADER_SIZE < _maxTransferUnit)&&(HasBytesToOutput()))
      {
         // Demand-create the next set of send-buffers
         if (_currentOutputBuffers.IsEmpty())
         {
            GenerateOutgoingByteBuffers(_currentOutputBuffers);
            if (_currentOutputBuffers.HasItems()) _currentOutputBufferOffset = 0;
         }
         if (_currentOutputBuffers.IsEmpty()) break;   // nothing more to send?

         const uint32 sbSize          = _currentOutputBuffers.Head()()->GetNumBytes();
         const uint32 dataBytesToSend = muscleMin(_maxTransferUnit-(_outputPacketSize+FRAGMENT_HEADER_SIZE), sbSize-_currentOutputBufferOffset);

         uint8 * p = _outputPacketBuffer.GetBuffer()+_outputPacketSize;
         muscleCopyOut(&p[0*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_magic));                      // a well-known magic number, for sanity checking
         muscleCopyOut(&p[1*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_sexID));                      // source exclusion ID
         muscleCopyOut(&p[2*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_sendMessageIDCounter));       // message ID tag so the receiver can track what belongs where
         muscleCopyOut(&p[3*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_currentOutputBufferOffset));  // start offset (within its message) for this sub-chunk
         muscleCopyOut(&p[4*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(dataBytesToSend));             // size of this sub-chunk
         muscleCopyOut(&p[5*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(sbSize));                      // total size of this message
//printf("CREATING PACKET magic=" UINT32_FORMAT_SPEC " msgID=" UINT32_FORMAT_SPEC " offset=" UINT32_FORMAT_SPEC " chunkSize=" UINT32_FORMAT_SPEC " totalSize=" UINT32_FORMAT_SPEC "\n", _magic, _sendMessageIDCounter, _currentOutputBufferOffset, dataBytesToSend, sbSize);
         memcpy(p+FRAGMENT_HEADER_SIZE, _currentOutputBuffers.Head()()->GetBuffer()+_currentOutputBufferOffset, dataBytesToSend);

         _outputPacketSize += (FRAGMENT_HEADER_SIZE+dataBytesToSend);
         _currentOutputBufferOffset += dataBytesToSend;
         if (_currentOutputBufferOffset == sbSize)
         {
            (void) _currentOutputBuffers.RemoveHead();
            _sendMessageIDCounter++;
            _currentOutputBufferOffset = 0;
            if (_currentOutputBuffers.IsEmpty()) ClearFakeSendBuffer(MAX_CACHE_SIZE);  // don't keep too much memory around!
         }
      }

      // Step 2:  If we have a non-empty packet to send, send it!
      if (_outputPacketSize > 0)
      {
         // If bytesWritten is set to zero, we just hold this buffer until our next call.
         const int32 bytesWritten = GetDataIO()() ? GetDataIO()()->Write(_outputPacketBuffer.GetBuffer(), _outputPacketSize) : -1;
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

} // end namespace muscle
