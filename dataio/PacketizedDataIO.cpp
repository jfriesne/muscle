#include "dataio/PacketizedDataIO.h"

namespace muscle {

PacketizedDataIO :: PacketizedDataIO(const DataIORef & slaveIO, uint32 maxTransferUnit) : _slaveIO(slaveIO), _maxTransferUnit(maxTransferUnit), _inputBufferSize(0), _inputBufferSizeBytesRead(0), _inputBufferBytesRead(0), _outputBufferBytesSent(0)
{
   // empty
}

int32 PacketizedDataIO :: Read(void * buffer, uint32 size)
{
   int32 ret = 0;

   if (_inputBufferSizeBytesRead < sizeof(uint32))
   {
      uint8 * ip = (uint8 *) &_inputBufferSize;
      int32 numSizeBytesRead = SlaveRead(&ip[_inputBufferSizeBytesRead], sizeof(uint32)-_inputBufferSizeBytesRead);
      if (numSizeBytesRead < 0) return -1;
      _inputBufferSizeBytesRead += numSizeBytesRead;
      if (_inputBufferSizeBytesRead == sizeof(uint32))
      {
         _inputBufferSize = B_LENDIAN_TO_HOST_INT32(_inputBufferSize);
         if (_inputBufferSize > _maxTransferUnit)
         {
            LogTime(MUSCLE_LOG_ERROR, "PacketizedDataIO:  Error, incoming packet with size " UINT32_FORMAT_SPEC ", max transfer unit is set to " UINT32_FORMAT_SPEC "\n", _inputBufferSize, _maxTransferUnit);
            return -1;
         }
         if (_inputBuffer.SetNumBytes(_inputBufferSize, false) != B_NO_ERROR) return -1;
         _inputBufferBytesRead = 0;

         // Special case for empty packets
         if (_inputBufferSize == 0) _inputBufferSizeBytesRead = 0;
      }
   }

   uint32 inBufSize = _inputBuffer.GetNumBytes();
   if ((_inputBufferSizeBytesRead == sizeof(uint32))&&(_inputBufferBytesRead < inBufSize))
   {
      int32 numBytesRead = SlaveRead(_inputBuffer.GetBuffer()+_inputBufferBytesRead, inBufSize-_inputBufferBytesRead);
      if (numBytesRead < 0) return -1;

      _inputBufferBytesRead += numBytesRead;
      if (_inputBufferBytesRead == inBufSize)
      {
         uint32 copyBytes = muscleMin(size, inBufSize);
         if (size < inBufSize) LogTime(MUSCLE_LOG_WARNING, "PacketizedDataIO:  Truncating incoming packet (" UINT32_FORMAT_SPEC " bytes available, only " UINT32_FORMAT_SPEC " bytes in user buffer)\n", inBufSize, size);
         memcpy(buffer, _inputBuffer.GetBuffer(), copyBytes);
         ret = copyBytes;

         _inputBufferSizeBytesRead = _inputBufferBytesRead = 0;
         _inputBuffer.Clear(inBufSize>(64*1024));  // free up memory after a large packet recv
      }
   }
   return ret;
}

int32 PacketizedDataIO :: Write(const void * buffer, uint32 size)
{
   if (size > _maxTransferUnit) 
   {
      LogTime(MUSCLE_LOG_ERROR, "PacketizedDataIO:  Error, tried to send packet with size " UINT32_FORMAT_SPEC ", max transfer unit is set to " UINT32_FORMAT_SPEC "\n", size, _maxTransferUnit);
      return -1;
   }

   // Only accept more data if we are done sending the data we already have buffered up
   bool tryAgainAfter = false;
   int32 ret = 0;
   if (HasBufferedOutput()) tryAgainAfter = true;
   else
   {
      // No data buffered?
      _outputBufferBytesSent = 0;

      if (_outputBuffer.SetNumBytes(sizeof(uint32)+size, false) != B_NO_ERROR) return 0;
      *((uint32 *)_outputBuffer.GetBuffer()) = B_HOST_TO_LENDIAN_INT32(size);
      memcpy(_outputBuffer.GetBuffer()+sizeof(uint32), buffer, size);
      ret = size;
   }

   if (WriteBufferedOutputAux() != B_NO_ERROR) 
   {
      return -1;
   }
   return ((tryAgainAfter)&&(HasBufferedOutput() == false)) ? Write(buffer, size) : ret;
}

status_t PacketizedDataIO :: WriteBufferedOutputAux()
{
   // Now try to send as much of our buffered output data as we can
   uint32 bufSize = _outputBuffer.GetNumBytes();
   if (_outputBufferBytesSent < bufSize)
   {
      int32 bytesSent = SlaveWrite(_outputBuffer.GetBuffer()+_outputBufferBytesSent, bufSize-_outputBufferBytesSent);
      if (bytesSent >= 0) 
      {
         _outputBufferBytesSent += bytesSent;
         if (_outputBufferBytesSent == bufSize)
         {
            _outputBuffer.Clear(bufSize>(64*1024));  // free up memory after a large packet send
            _outputBufferBytesSent = 0;
         }
      }
      else return B_ERROR;
   }
   return B_NO_ERROR;
}

}; // end namespace muscle
