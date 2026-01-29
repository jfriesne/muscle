#include "dataio/StressTestParserProxyDataIO.h"

namespace muscle {

StressTestParserProxyDataIO :: StressTestParserProxyDataIO(const DataIORef & childIO, uint32 minChildWriteSize, uint32 maxChildWriteSize, uint64 optMinimumDelayBetweenWritesMicros)
   : ProxyDataIO(childIO)
   , _outputBufferBytesSent(0)
   , _minChildWriteSize(minChildWriteSize)
   , _maxChildWriteSize(maxChildWriteSize)
   , _optMinimumDelayBetweenWritesMicros(optMinimumDelayBetweenWritesMicros)
   , _mostRecentChildWriteTime(0)
{
   // empty
}

io_status_t StressTestParserProxyDataIO :: Write(const void * buffer, uint32 size)
{
   MRETURN_ON_ERROR(_outputBuffer.AppendBytes((const uint8 *) buffer, size));

   const io_status_t ret = DrainOutputBuffer(false);
   if (ret.GetByteCount() > 0) return size;  // we absorbed all of the user's bytes, even if we didn't actually send them all out yet
                          else return ret;
}

io_status_t StressTestParserProxyDataIO :: DrainOutputBuffer(bool forceSendAll)
{
   if (_maxChildWriteSize == 0) return 0;  // if we can't write, we can't write

   const uint32 minBytesToPassToWrite = forceSendAll ? 0 : _minChildWriteSize;

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

   return ret;
}

} // end namespace muscle
