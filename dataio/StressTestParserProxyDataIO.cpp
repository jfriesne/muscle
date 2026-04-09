#include "dataio/StressTestParserProxyDataIO.h"

namespace muscle {

StressTestParserProxyDataIO :: StressTestParserProxyDataIO(const DataIORef & childIO, uint32 minChildWriteSize, uint32 maxChildWriteSize, uint64 optMinimumDelayBetweenWritesMicros)
   : ProxyDataIO(childIO)
   , _outputBufferBytesSent(0)
   , _minChildWriteSize(minChildWriteSize)
   , _maxChildWriteSize(muscleMax(maxChildWriteSize, (uint32)1))
   , _optMinimumDelayBetweenWritesMicros(optMinimumDelayBetweenWritesMicros)
   , _mostRecentChildWriteTime(0)
{
   // empty
}

io_status_t StressTestParserProxyDataIO :: Write(const void * buffer, uint32 size)
{
   MRETURN_ON_ERROR(_outputBuffer.AppendBytes((const uint8 *) buffer, size));
   (void) DrainOutputBuffer(false);  // ignore any error here since we already buffered up the bytes
   return size;  // we absorbed all of the user's bytes, even if we didn't actually send them all out yet
}

io_status_t StressTestParserProxyDataIO :: DrainOutputBuffer(bool forceSendAllPendingBytes)
{
   const uint32 minBytesToPassToWrite = forceSendAllPendingBytes ? 0 : _minChildWriteSize;

   io_status_t ret;
   while((_outputBuffer.GetNumBytes() > 0)&&(_outputBufferBytesSent < _outputBuffer.GetNumBytes()))
   {
      const uint8 * toSend               = _outputBuffer.GetBuffer() + _outputBufferBytesSent;
      const uint32 numBytesAvailable     = _outputBuffer.GetNumBytes()-_outputBufferBytesSent;
      const uint32 numBytesToPassToWrite = (numBytesAvailable >= minBytesToPassToWrite) ? muscleMin(numBytesAvailable, _maxChildWriteSize) : 0;
      if (numBytesToPassToWrite > 0)
      {
         {
            // Snoozing to discourage the underlying TCP layer from recombining our buffers into the same TCP packet again
            const uint64 now             = GetRunTime64();
            const uint64 microsSincePrev = now-_mostRecentChildWriteTime;
            if (microsSincePrev < _optMinimumDelayBetweenWritesMicros)
            {
               ProxyDataIO::FlushOutput();
               (void) Snooze64(_optMinimumDelayBetweenWritesMicros-microsSincePrev);
            }
         }

         _mostRecentChildWriteTime = GetRunTime64();
         const io_status_t numBytesSent = ProxyDataIO::Write(toSend, numBytesToPassToWrite);
         if (numBytesSent.GetByteCount() > 0)
         {
            _outputBufferBytesSent += numBytesSent.GetByteCount();
            if (_outputBufferBytesSent == _outputBuffer.GetNumBytes())
            {
               _outputBuffer.Clear();
               _outputBufferBytesSent = 0;
            }
         }

         MTALLY_BYTES_OR_RETURN_ON_ERROR_OR_BREAK(ret, numBytesSent);
      }
      else break;
   }

   if (_outputBufferBytesSent > (_outputBuffer.GetNumBytes()/2))
   {
      // Slide the remaining bytes to the top of the buffer to avoid a potential ever-growing buffer
      const uint32 numBytesLeftToSend = _outputBuffer.GetNumBytes()-_outputBufferBytesSent;
      uint8 * buf = _outputBuffer.GetBuffer();
      memcpy(buf, &buf[_outputBufferBytesSent], numBytesLeftToSend);
      _outputBuffer.TruncateToLength(numBytesLeftToSend);
      _outputBufferBytesSent = 0;
   }

   return ret;
}

} // end namespace muscle
