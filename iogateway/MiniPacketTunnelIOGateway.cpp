/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/MiniPacketTunnelIOGateway.h"
#include "util/DataFlattener.h"
#include "util/DataUnflattener.h"
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
# include "zlib/ZLibCodec.h"
#endif

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

         DataUnflattener unflat(_inputPacketBuffer.GetBuffer(), bytesRead);
         if ((_allowMiscData)&&((bytesRead < (int32)PACKET_HEADER_SIZE)||(((uint32)B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(_inputPacketBuffer.GetBuffer()))) != _magic)))
         {
            // If we're allowed to handle miscellaneous data, we'll just pass it on through verbatim
            HandleIncomingByteBuffer(receiver, _inputPacketBuffer.GetBuffer(), bytesRead, fromIAP);
         }
         else if (bytesRead >= (int32)PACKET_HEADER_SIZE)
         {
            // Read the packet header
            const uint32 magic    = unflat.ReadInt32();
            const uint32 sexID    = unflat.ReadInt32();
            const uint32 cLAndID  = unflat.ReadInt32();  // (compressionLevel<<24) | (packetID)
            const uint8  cLevel   = (cLAndID >> 24) & 0xFF;
#ifdef PACKET_ID_ISNT_CURRENTLY_USED_SO_AVOID_COMPILER_WARNING
            const uint32 packetID = (cLAndID >> 00) & 0xFFFFFF;
#endif
//printf("   PARSE magic=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " compressionLevel=%u sex=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " packetID=" UINT32_FORMAT_SPEC " status=[%s]\n", magic, _magic, cLevel, sexID, _sexID, (cLAndID>>00)&0xFFFFFF, unflat.GetStatus()());

            if ((magic == _magic)&&((_sexID == 0)||(_sexID != sexID)))
            {
               if (cLevel > 0)
               {
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
                  // Payload-chunks are compressed!  Gotta zlib-inflate them first
                  if (_codec() == NULL)
                  {
                     _codec.SetRef(newnothrow ZLibCodec(3));  // compression-level doesn't really matter for inflation step
                     if (_codec() == NULL) MWARN_OUT_OF_MEMORY;
                  }
                  infBuf = _codec() ? static_cast<ZLibCodec *>(_codec())->Inflate(unflat.GetCurrentReadPointer(), unflat.GetNumBytesAvailable()) : ByteBufferRef();
                  if (infBuf()) unflat.SetBuffer(*infBuf());  // code below will read from the inflated-data buffer instead
                  else
                  {
                     LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoInputImplementation():  zlib-inflate failed!\n");
                     (void) unflat.SeekTo(unflat.GetMaxNumBytes());  // packet looks corrupt, let's skip it
                  }
#else
                  LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoInputImplementation():  Can't zlib-inflate incoming MiniPacketTunnelIOGateway, ZLib support wasn't compiled in!\n");
                  (void) unflat.SeekTo(unflat.GetMaxNumBytes());   // packet looks unusable, let's skip it
#endif
               }

               // Parse out each message-chunk from the packet
               while(unflat.GetNumBytesAvailable() >= (uint32)CHUNK_HEADER_SIZE)
               {
                  const uint32 chunkSizeBytes = unflat.ReadInt32();  // this is the only field in a MiniPacketTunnelIOGateway chunk-header (for now, anyway)

                  const uint32 bytesAvailable = unflat.GetNumBytesAvailable();
                  if (chunkSizeBytes <= bytesAvailable)
                  {
                     HandleIncomingByteBuffer(receiver, unflat.GetCurrentReadPointer(), chunkSizeBytes, fromIAP);
                     (void) unflat.SeekRelative(chunkSizeBytes);
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
   if (_outputPacketBuffer.SetNumBytes(_maxTransferUnit, false).IsError()) return -1;  // _outputPacketBuffer.GetNumBytes() should be _maxTransferUnit at all times

   UncheckedDataFlattener flat(_outputPacketBuffer.GetBuffer(), _outputPacketBuffer.GetNumBytes());
   (void) flat.SeekRelative(_outputPacketSize);  // skip past any bytes that are already present in _outputPacketBuffer from previously

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
         else if ((flat.GetNumBytesWritten()+((flat.GetNumBytesWritten()==0)?PACKET_HEADER_SIZE:0)+CHUNK_HEADER_SIZE+sbSize) <= _maxTransferUnit)
         {
            if (flat.GetNumBytesWritten() == 0)
            {
               // Add a packet-header to the packet
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
               const uint32 cLAndID = _sendPacketIDCounter|(_sendCompressionLevel<<24);
#else
               const uint32 cLAndID = _sendPacketIDCounter;
#endif
               (void) flat.WriteInt32(_magic);
               (void) flat.WriteInt32(_sexID);
               (void) flat.WriteInt32(cLAndID);
            }

            // Add the chunk-header and chunk-data to the packet
            (void) flat.WriteInt32(sbSize);
            (void) flat.WriteBytes(_currentOutputBuffers.Head()()->GetBuffer(), sbSize);
            (void) _currentOutputBuffers.RemoveHead();
         }
         else break;  // can't fit the current output buffer into this packet; it'll have to wait for the next one
      }

      // Step 2:  If we have a non-empty packet to send, send it!
      ByteBufferRef defBuf;
      uint8 * writeBuf = _outputPacketBuffer.GetBuffer();  // may be changed below if we deflate
      uint32 writeSize = flat.GetNumBytesWritten();
      if (writeSize > 0)
      {
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
         if (_sendCompressionLevel > 0)
         {
            ZLibCodec * codec = static_cast<ZLibCodec *>(_codec());
            if ((codec == NULL)||(codec->GetCompressionLevel() != _sendCompressionLevel)) _codec.SetRef(codec = newnothrow ZLibCodec(_sendCompressionLevel));
            if (codec)
            {
               defBuf = codec->Deflate(_outputPacketBuffer.GetBuffer()+PACKET_HEADER_SIZE, flat.GetNumBytesWritten()-PACKET_HEADER_SIZE, true, PACKET_HEADER_SIZE);
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
            else MWARN_OUT_OF_MEMORY;

            if (defBuf() == NULL)
            {
               // Oops, no compression occurred!  Better patch the packet-header to reflect that or the receiver will be confused
               muscleCopyOut(&writeBuf[2*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(_sendPacketIDCounter));
            }
         }
#endif

         // If bytesWritten is set to zero, we just hold this buffer until our next call.
         const int32 bytesWritten = GetDataIO()() ? GetDataIO()()->Write(writeBuf, writeSize) : -1;
         if (bytesWritten > 0)
         {
            if (bytesWritten != (int32)writeSize) LogTime(MUSCLE_LOG_ERROR, "MiniPacketTunnelIOGateway::DoOutput():  Short write!  (" INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes)\n", bytesWritten, writeSize);
            totalBytesWritten += bytesWritten;
            flat.SeekTo(0);
            _sendPacketIDCounter = (_sendPacketIDCounter+1)%16777216;  // 24-bit counter
         }
         else if (bytesWritten == 0) break;  // no more space to write, for now
         else return -1;
      }
      else break;  // nothing more to do!
   }
   _outputPacketSize = flat.GetNumBytesWritten();  // remember for next time how many still-pending packets are left in our _outputPacketBuffer
   return totalBytesWritten;
}

} // end namespace muscle
