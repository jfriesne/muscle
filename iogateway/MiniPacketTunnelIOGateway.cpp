/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/PacketDataIO.h"  // for retrieving the source IP address and port, where possible
#include "iogateway/MiniPacketTunnelIOGateway.h"

namespace muscle {

// Each packet-header has the following fields in it:
//    uint32 magic_number
//    uint32 source_exclusion_id
//    uint32 packet_id
static const uint32 PACKET_HEADER_SIZE = 3*(sizeof(uint32));

// Each chunk header has the following fields in it:
//    uint32 chunk_size_bytes
static const uint32 CHUNK_HEADER_SIZE = 1*(sizeof(uint32));

MiniPacketTunnelIOGateway :: MiniPacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway, uint32 maxTransferUnit, uint32 magic)
   : _magic(magic)
   , _maxTransferUnit(muscleMax(maxTransferUnit, PACKET_HEADER_SIZE+CHUNK_HEADER_SIZE+1))
   , _allowMiscData(false)
   , _sexID(0)
   , _slaveGateway(slaveGateway)
   , _outputPacketSize(0)
   , _sendPacketIDCounter(0)
{
   _fakeSendIO.SetBuffer(ByteBufferRef(&_fakeSendBuffer, false));
   // _fakeReceiveIO's buffer will be set just before it is used
}

int32 MiniPacketTunnelIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
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
         if ((_allowMiscData)&&((bytesRead < (int32)PACKET_HEADER_SIZE)||(((uint32)B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(p))) != _magic)))
         {
            // If we're allowed to handle miscellaneous data, we'll just pass it on through verbatim
            HandleIncomingMessage(receiver, p, bytesRead, fromIAP);
         }
         else if (bytesRead >= PACKET_HEADER_SIZE)
         {
            // Read the packet header
            const uint8 * invalidByte = p+bytesRead;

            const uint32 magic    = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[0*sizeof(uint32)]));
            const uint32 sexID    = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[1*sizeof(uint32)]));
            const uint32 packetID = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[2*sizeof(uint32)]));
            p += PACKET_HEADER_SIZE;
//printf("   PARSE magic=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " sex=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " packetID=" UINT32_FORMAT_SPEC "\n", magic, _magic, sexID, _sexID, packetID);

            if ((magic == _magic)&&((_sexID == 0)||(_sexID != sexID)))
            {
               // Parse out each message-chunk from the packet
               while((invalidByte-p) >= (int32)CHUNK_HEADER_SIZE)
               {
                  const uint32 chunkSizeBytes = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(p));
                  p += CHUNK_HEADER_SIZE;

                  const uint32 bytesAvailable = invalidByte-p;
                  if (chunkSizeBytes <= bytesAvailable)
                  {
                     HandleIncomingMessage(receiver, p, chunkSizeBytes, fromIAP);
                     p += chunkSizeBytes;
                  }
                  else
                  {
                     LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoInputImplementation:  Chunk size " UINT32_FORMAT_SPEC " is too large, only " UINT32_FORMAT_SPEC " bytes remain in the packet!\n", chunkSizeBytes, bytesAvailable);
                     break;
                  }
               }
            }
         }
      }
      else if (bytesRead < 0) return -1;
      else break;
   }
   return totalBytesRead;
}

void MiniPacketTunnelIOGateway :: HandleIncomingMessage(AbstractGatewayMessageReceiver & receiver, const uint8 * p, uint32 bytesRead, const IPAddressAndPort & fromIAP)
{
   ByteBuffer temp;
   temp.AdoptBuffer(bytesRead, const_cast<uint8 *>(p));
   HandleIncomingMessage(receiver, ByteBufferRef(&temp, false), fromIAP);
   (void) temp.ReleaseBuffer();
}

void MiniPacketTunnelIOGateway :: HandleIncomingMessage(AbstractGatewayMessageReceiver & receiver, const ByteBufferRef & buf, const IPAddressAndPort & fromIAP)
{
   if (_slaveGateway())
   {
      DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

      _fakeReceiveIO.SetBuffer(buf); (void) _fakeReceiveIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
      _slaveGateway()->SetDataIO(DataIORef(&_fakeReceiveIO, false));

      uint32 slaveBytesRead = 0;
      while(slaveBytesRead < buf()->GetNumBytes())
      {
         _scratchReceiver    = &receiver;
         _scratchReceiverArg = (void *) &fromIAP;
         const int32 nextBytesRead = _slaveGateway()->DoInput(*this, buf()->GetNumBytes()-slaveBytesRead);
         if (nextBytesRead > 0) slaveBytesRead += nextBytesRead;
                           else break;
      }

      _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
      _fakeReceiveIO.SetBuffer(ByteBufferRef());
   }
   else
   {
      MessageRef inMsg = GetMessageFromPool();
      if ((inMsg())&&(inMsg()->UnflattenFromByteBuffer(*buf()).IsOK())) receiver.CallMessageReceivedFromGateway(inMsg, (void *) &fromIAP);
   }
}

int32 MiniPacketTunnelIOGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if (_outputPacketBuffer.SetNumBytes(_maxTransferUnit, false).IsError()) return -1;

   uint32 totalBytesWritten = 0;
   bool firstTime = true;
   while((totalBytesWritten < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      // Step 1:  Add as many messages to our output-packet-buffer as we can fit into it
      while(HasBytesToOutput())
      {
         // Demand-create the next send-buffer
         if (_currentOutputBuffer() == NULL)
         {
            MessageRef msg;
            if (GetOutgoingMessageQueue().RemoveHead(msg).IsOK())
            {
               _currentOutputBufferOffset = 0;

               if (_slaveGateway())
               {
                  DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

                  // Get the slave gateway to generate its output into our ByteBuffer
                  _fakeSendBuffer.SetNumBytes(0, false);
                  _fakeSendIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
                  _slaveGateway()->SetDataIO(DataIORef(&_fakeSendIO, false));
                  _slaveGateway()->AddOutgoingMessage(msg);
                  while(_slaveGateway()->DoOutput() > 0) {/* empty */}

                  _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
                  _currentOutputBuffer.SetRef(&_fakeSendBuffer, false);
               }
               else if (_fakeSendBuffer.SetNumBytes(msg()->FlattenedSize(), false).IsOK())
               {
                  // Default algorithm:  Just flatten the Message into the buffer
                  msg()->Flatten(_fakeSendBuffer.GetBuffer());
                  _currentOutputBuffer.SetRef(&_fakeSendBuffer, false);
               }
            }
         }
         if (_currentOutputBuffer() == NULL) break;

         const uint32 sbSize = _currentOutputBuffer()->GetNumBytes();
         if ((PACKET_HEADER_SIZE+CHUNK_HEADER_SIZE+sbSize) > _maxTransferUnit)
         {
            LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutputImplementation():  Outgoing Message payload is " UINT32_FORMAT_SPEC " bytes long, it can't fit into i packet!  Dropping it\n", _currentOutputBuffer()->GetNumBytes());
            _currentOutputBuffer.Reset();
         }
         else if ((((_outputPacketSize==0)?PACKET_HEADER_SIZE:0)+CHUNK_HEADER_SIZE+sbSize) < _maxTransferUnit)
         {
            uint8 * b = _outputPacketBuffer.GetBuffer();

            if (_outputPacketSize == 0)
            {
               // Add a packet-header to the packet
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(_magic));               _outputPacketSize += sizeof(_magic);
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(_sexID));               _outputPacketSize += sizeof(_sexID);
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(_sendPacketIDCounter)); _outputPacketSize += sizeof(_sendPacketIDCounter);
               _sendPacketIDCounter++;
            }

            // Add the chunk-header and chunk-data to the packet
            muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(sbSize));      _outputPacketSize += sizeof(sbSize);
            memcpy(&b[_outputPacketSize], _currentOutputBuffer()->GetBuffer(), sbSize); _outputPacketSize += sbSize;
         }
         else break;  // can't fit _currentOutputBuffer into this packet; it'll have to wait for the next one
      }

      // Step 2:  If we have a non-empty packet to send, send it!
      if (_outputPacketSize > 0)
      {
         // If bytesWritten is set to zero, we just hold this buffer until our next call.
         const int32 bytesWritten = GetDataIO()() ? GetDataIO()()->Write(_outputPacketBuffer.GetBuffer(), _outputPacketSize) : -1;
//printf("WROTE " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes %s\n", bytesWritten, _outputPacketSize, (bytesWritten==(int32)_outputPacketSize)?"":"******** SHORT ***********");
         if (bytesWritten > 0)
         {
            if (bytesWritten != (int32)_outputPacketSize) LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutput():  Short write!  (" INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes)\n", bytesWritten, _outputPacketSize);
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
