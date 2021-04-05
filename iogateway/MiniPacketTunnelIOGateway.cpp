/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/MiniPacketTunnelIOGateway.h"

namespace muscle {

// Each packet-header has the following fields in it:
//    uint32 : magic_number
//    uint32 : source_exclusion_id
//    uint32 : (compression_level<<24) | packet_id
static const uint32 PACKET_HEADER_SIZE = 3*(sizeof(uint32));

// Each chunk header has the following fields in it:
//    uint32 chunk_size_bytes
static const uint32 CHUNK_HEADER_SIZE = 1*(sizeof(uint32));

MiniPacketTunnelIOGateway :: MiniPacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway, uint32 maxTransferUnit, uint32 magic)
   : ProxyIOGateway(slaveGateway)
   , _magic(magic)
   , _maxTransferUnit(muscleMax(maxTransferUnit, PACKET_HEADER_SIZE+CHUNK_HEADER_SIZE+1))
   , _sendCompressionLevel(0)
   , _allowMiscData(false)
   , _sexID(0)
   , _outputPacketSize(0)
   , _sendPacketIDCounter(0)
{
   // empty
}

int32 MiniPacketTunnelIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   if (_inputPacketBuffer.SetNumBytes(_maxTransferUnit, false).IsError()) return -1;

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   ByteBufferRef infBuf;
#endif

   bool firstTime = true;
   uint32 totalBytesRead = 0;
   while((totalBytesRead < maxBytes)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      const int32 bytesRead = GetDataIO()() ? GetDataIO()()->Read(_inputPacketBuffer.GetBuffer(), _inputPacketBuffer.GetNumBytes()) : -1;
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
            HandleIncomingByteBuffer(receiver, p, bytesRead, fromIAP);
         }
         else if (bytesRead >= (int32)PACKET_HEADER_SIZE)
         {
            // Read the packet header
            const uint8 * invalidByte = p+bytesRead;

            const uint32 magic    = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[0*sizeof(uint32)]));
            const uint32 sexID    = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[1*sizeof(uint32)]));
            const uint32 cLAndID  = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&p[2*sizeof(uint32)]));  // (compressionLevel<<24) | (packetID)
            const uint8  cLevel   = (cLAndID >> 24) & 0xFF;
#ifdef PACKET_ID_ISNT_CURRENTLY_USED_SO_AVOID_COMPILER_WARNING
            const uint32 packetID = (cLAndID >> 00) & 0xFFFFFF;
#endif
            p += PACKET_HEADER_SIZE;
//printf("   PARSE magic=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " compressionLevel=%u sex=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " packetID=" UINT32_FORMAT_SPEC "\n", magic, _magic, cLevel, sexID, _sexID, packetID);

            if ((magic == _magic)&&((_sexID == 0)||(_sexID != sexID)))
            {
               if (cLevel > 0)
               {
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
                  // Payloads are compressed!  Gotta zlib-inflate them first
                  if (_codec() == NULL) _codec.SetRef(newnothrow ZLibCodec(3));  // compression-level doesn't really matter for inflation step
                  infBuf = _codec() ? _codec()->Inflate(p, invalidByte-p) : ByteBufferRef();
                  if (infBuf())
                  {
                     p = infBuf()->GetBuffer();
                     invalidByte = p+infBuf()->GetNumBytes();
                  }
                  else
                  {
                     LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoInputImplementation():  zlib-inflate failed!\n");
                     p = invalidByte;
                  }
#else
                  LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoInputImplementation():  Can't zlib-inflate incoming MiniPacketTunnelIOGateway, ZLib support wasn't compiled in!\n");
                  p = invalidByte;
#endif
               }

               // Parse out each message-chunk from the packet
               while((invalidByte-p) >= (int32)CHUNK_HEADER_SIZE)
               {
                  const uint32 chunkSizeBytes = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(p));
                  p += CHUNK_HEADER_SIZE;

                  const uint32 bytesAvailable = invalidByte-p;
                  if (chunkSizeBytes <= bytesAvailable)
                  {
                     HandleIncomingByteBuffer(receiver, p, chunkSizeBytes, fromIAP);
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
         // Demand-create the next Message-buffer
         if (_currentOutputBuffers.IsEmpty()) GenerateOutgoingByteBuffers(_currentOutputBuffers);
         if (_currentOutputBuffers.IsEmpty()) break;

         const uint32 sbSize = _currentOutputBuffers.Head()()->GetNumBytes();
         if ((PACKET_HEADER_SIZE+CHUNK_HEADER_SIZE+sbSize) > _maxTransferUnit)
         {
            LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutputImplementation():  Outgoing payload is " UINT32_FORMAT_SPEC " bytes, it can't fit into a packet with MTU=" UINT32_FORMAT_SPEC "!  Dropping it\n", sbSize, _maxTransferUnit);
            (void) _currentOutputBuffers.RemoveHead();
         }
         else if ((((_outputPacketSize==0)?PACKET_HEADER_SIZE:0)+CHUNK_HEADER_SIZE+sbSize) <= _maxTransferUnit)
         {
            uint8 * b = _outputPacketBuffer.GetBuffer();

            if (_outputPacketSize == 0)
            {
               // Add a packet-header to the packet
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
               const uint32 cLAndID = _sendPacketIDCounter|(_sendCompressionLevel<<24);
#else
               const uint32 cLAndID = _sendPacketIDCounter;
#endif
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(_magic));  _outputPacketSize += sizeof(_magic);
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(_sexID));  _outputPacketSize += sizeof(_sexID);
               muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(cLAndID)); _outputPacketSize += sizeof(cLAndID);
            }

            // Add the chunk-header and chunk-data to the packet
            muscleCopyOut(&b[_outputPacketSize], B_HOST_TO_LENDIAN_INT32(sbSize));              _outputPacketSize += sizeof(sbSize);
            memcpy(&b[_outputPacketSize], _currentOutputBuffers.Head()()->GetBuffer(), sbSize); _outputPacketSize += sbSize;
            (void) _currentOutputBuffers.RemoveHead();
         }
         else break;  // can't fit the current output buffer into this packet; it'll have to wait for the next one
      }

      // Step 2:  If we have a non-empty packet to send, send it!
      ByteBufferRef defBuf;
      uint8 * writeBuf = _outputPacketBuffer.GetBuffer();  // may be changed below if we deflate
      uint32 writeSize = _outputPacketSize;
      if (writeSize > 0)
      {
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
         if (_sendCompressionLevel > 0)
         {
            if ((_codec() == NULL)||(_codec()->GetCompressionLevel() != _sendCompressionLevel)) _codec.SetRef(newnothrow ZLibCodec(_sendCompressionLevel));
            if (_codec())
            {
               defBuf = _codec()->Deflate(_outputPacketBuffer.GetBuffer()+PACKET_HEADER_SIZE, _outputPacketSize-PACKET_HEADER_SIZE, true, PACKET_HEADER_SIZE);
               if (defBuf())
               {
                  if (defBuf()->GetNumBytes() < writeSize)  // no sense sending deflated data if it didn't actually change anything!
                  {
                     memcpy(defBuf()->GetBuffer(), writeBuf, PACKET_HEADER_SIZE);
                     writeBuf  = defBuf()->GetBuffer();
                     writeSize = defBuf()->GetNumBytes();
                  }
                  else defBuf.Reset();
               }
               else LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutputImplementation():  Deflate() failed!\n");
            }

            if (defBuf() == NULL)
            {
               // Oops, no compression occurred!  Better patch the packet-header to reflect that or the receiver will be confused
               muscleCopyOut(&writeBuf[2*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_sendPacketIDCounter));
            }
         }
#endif

         // If bytesWritten is set to zero, we just hold this buffer until our next call.
         const int32 bytesWritten = GetDataIO()() ? GetDataIO()()->Write(writeBuf, writeSize) : -1;
//printf("WROTE " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes %s\n", bytesWritten, writeSize, (bytesWritten==(int32)writeSize)?"":"******** SHORT ***********");
         if (bytesWritten > 0)
         {
            if (bytesWritten != (int32)writeSize) LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutput():  Short write!  (" INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes)\n", bytesWritten, _outputPacketSize);
            _outputPacketBuffer.Clear();
            _outputPacketSize = 0;
            totalBytesWritten += bytesWritten;
            _sendPacketIDCounter = (_sendPacketIDCounter+1)%16777216;  // 24-bit counter
         }
         else if (bytesWritten == 0) break;  // no more space to write, for now
         else return -1;
      }
      else break;  // nothing more to do!
   }
   return totalBytesWritten;
}

} // end namespace muscle
