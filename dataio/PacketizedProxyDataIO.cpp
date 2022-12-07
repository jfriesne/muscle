#include "dataio/PacketizedProxyDataIO.h"

namespace muscle {

PacketizedProxyDataIO :: PacketizedProxyDataIO(const DataIORef & childIO, uint32 maxTransferUnit)
   : ProxyDataIO(childIO)
   , _maxTransferUnit(maxTransferUnit)
   , _inputBufferSize(0)
   , _inputBufferSizeBytesRead(0)
   , _inputBufferBytesRead(0)
   , _outputBufferBytesSent(0)
{
   // empty
}

io_status_t PacketizedProxyDataIO :: Read(void * buffer, uint32 size)
{
   uint32 ret = 0;

   if (_inputBufferSizeBytesRead < sizeof(uint32))
   {
      uint8 * ip = (uint8 *) &_inputBufferSize;
      const io_status_t numSizeBytesRead = ProxyDataIO::Read(&ip[_inputBufferSizeBytesRead], sizeof(uint32)-_inputBufferSizeBytesRead);
      MRETURN_ON_ERROR(numSizeBytesRead);

      _inputBufferSizeBytesRead += numSizeBytesRead.GetByteCount();
      if (_inputBufferSizeBytesRead == sizeof(uint32))
      {
         _inputBufferSize = DefaultEndianConverter::Import<uint32>(&_inputBufferSize);
         if (_inputBufferSize > _maxTransferUnit)
         {
            LogTime(MUSCLE_LOG_ERROR, "PacketizedProxyDataIO:  Error, incoming packet with size " UINT32_FORMAT_SPEC ", max transfer unit is set to " UINT32_FORMAT_SPEC "\n", _inputBufferSize, _maxTransferUnit);
            return B_BAD_DATA;
         }
         MRETURN_ON_ERROR(_inputBuffer.SetNumBytes(_inputBufferSize, false));
         _inputBufferBytesRead = 0;

         // Special case for empty packets
         if (_inputBufferSize == 0) _inputBufferSizeBytesRead = 0;
      }
   }

   const uint32 inBufSize = _inputBuffer.GetNumBytes();
   if ((_inputBufferSizeBytesRead == sizeof(uint32))&&(_inputBufferBytesRead < inBufSize))
   {
      const io_status_t numBytesRead = ProxyDataIO::Read(_inputBuffer.GetBuffer()+_inputBufferBytesRead, inBufSize-_inputBufferBytesRead);
      MRETURN_ON_ERROR(numBytesRead);

      _inputBufferBytesRead += numBytesRead.GetByteCount();
      if (_inputBufferBytesRead == inBufSize)
      {
         const uint32 copyBytes = muscleMin(size, inBufSize);
         if (size < inBufSize) LogTime(MUSCLE_LOG_WARNING, "PacketizedProxyDataIO:  Truncating incoming packet (" UINT32_FORMAT_SPEC " bytes available, only " UINT32_FORMAT_SPEC " bytes in user buffer)\n", inBufSize, size);
         memcpy(buffer, _inputBuffer.GetBuffer(), copyBytes);
         ret = copyBytes;

         _inputBufferSizeBytesRead = _inputBufferBytesRead = 0;
         _inputBuffer.Clear(inBufSize>(64*1024));  // free up memory after a large packet recv
      }
   }

   return ret;
}

io_status_t PacketizedProxyDataIO :: Write(const void * buffer, uint32 size)
{
   if (size > _maxTransferUnit)
   {
      LogTime(MUSCLE_LOG_ERROR, "PacketizedProxyDataIO:  Error, tried to send packet with size " UINT32_FORMAT_SPEC ", max transfer unit is set to " UINT32_FORMAT_SPEC "\n", size, _maxTransferUnit);
      return B_BAD_ARGUMENT;
   }

   // Only accept more data if we are done sending the data we already have buffered up
   bool tryAgainAfter = false;
   uint32 ret = 0;
   if (HasBufferedOutput()) tryAgainAfter = true;
   else
   {
      // No data buffered?
      _outputBufferBytesSent = 0;

      MRETURN_ON_ERROR(_outputBuffer.SetNumBytes(sizeof(uint32)+size, false));
      DefaultEndianConverter::Export(size, _outputBuffer.GetBuffer());
      memcpy(_outputBuffer.GetBuffer()+sizeof(uint32), buffer, size);
      ret = size;
   }

   MRETURN_ON_ERROR(WriteBufferedOutputAux());

   return ((tryAgainAfter)&&(HasBufferedOutput() == false)) ? Write(buffer, size) : io_status_t(ret);
}

status_t PacketizedProxyDataIO :: WriteBufferedOutputAux()
{
   // Now try to send as much of our buffered output data as we can
   const uint32 bufSize = _outputBuffer.GetNumBytes();
   if (_outputBufferBytesSent < bufSize)
   {
      const io_status_t bytesSent = ProxyDataIO::Write(_outputBuffer.GetBuffer()+_outputBufferBytesSent, bufSize-_outputBufferBytesSent);
      MRETURN_ON_ERROR(bytesSent.GetStatus());

      _outputBufferBytesSent += bytesSent.GetByteCount();
      if (_outputBufferBytesSent == bufSize)
      {
         _outputBuffer.Clear(bufSize>(64*1024));  // free up memory after a large packet send
         _outputBufferBytesSent = 0;
      }
   }
   return B_NO_ERROR;
}

} // end namespace muscle
