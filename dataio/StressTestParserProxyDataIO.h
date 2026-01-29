/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStressTestParserProxyDataIO_h
#define MuscleStressTestParserProxyDataIO_h

#include "dataio/ProxyDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 * This class is for debugging purposes:  It is intended to be used to help surface bad assumptions in another program's
 * input-stream parsing.  StressTestParserProxyDataIO is a ProxyDataIO, so it will pass method-calls through to its child
 * DataIO, but instead of passing the Write() calls through verbatim, it will group the bytes together into different-sized
 * groups, with an optional delay between successive calls to the child DataIO's Write() method.  That way, if the remote
 * program consuming and parsing our output stream is implicitly relying on bytes being delivered in particular groups,
 * wrapping our output stream in this DataIO may cause the remote to malfunction more obviously than if you just waited
 * for the TCP layer to supply the same effects due to network conditions.
 *
 * @note this class is probably not useful for UDP-style packet-based I/O.
 */
class StressTestParserProxyDataIO : public ProxyDataIO
{
public:
   /**
    *  Constructor.
    *  @param childIO The underlying streaming DataIO object to pass calls on through to.
    *  @param minChildWriteSize The minimum number of bytes we should pass to our child DataIO's
    *                           Write() method in a single call.  (Note that we will still pass
    *                           fewer bytes than this, if we have outgoing bytes buffered when
    *                           when our FlushOutput() method is called)
    *  @param maxChildWriteSize the maximum number of bytes we should pass to our child DataIO's
    *                           Write() method in a single call.  If we have more bytes to send
    *                           than this, we will break them up into multiple successive calls.
    *  @param optMinimumDelayBetweenWritesMicros the minimum amount of time that should pass between
    *                           successive calls to our child DataIO's Write() method.  This class
    *                           will call Snooze64() if needed to enforce the minimum delay (so
    *                           beware of this class's potential impact on the performance of your
    *                           thread's event loop!)
    */
   StressTestParserProxyDataIO(const DataIORef & childIO, uint32 minChildWriteSize, uint32 maxChildWriteSize, uint64 optMinimumDelayBetweenWritesMicros);

   /** Returns the minimum child write size, as specified in our constructor */
   MUSCLE_NODISCARD uint32 GetMinChildWriteSize() const {return _minChildWriteSize;}

   /** Returns the maximum child write size, as specified in our constructor */
   MUSCLE_NODISCARD uint32 GetMaxChildWriteSize() const {return _maxChildWriteSize;}

   /** Returns the minimum delay between Write() calls to our child's DataIO, as specified in our constructor */
   MUSCLE_NODISCARD uint32 GetMinimumDelayBetweenWritesMicros() const {return _optMinimumDelayBetweenWritesMicros;}

   virtual io_status_t Write(const void * buffer, uint32 size);
   virtual void Shutdown() {ProxyDataIO::Shutdown(); _outputBuffer.Clear(true);}

   MUSCLE_NODISCARD virtual bool HasBufferedOutput() const {return (_outputBufferBytesSent < _outputBuffer.GetNumBytes());}
   virtual void WriteBufferedOutput() {(void) DrainOutputBuffer(true);}
   virtual void FlushOutput()         {(void) DrainOutputBuffer(true);}

private:
   io_status_t DrainOutputBuffer(bool forceSendAllPendingBytes);

   ByteBuffer _outputBuffer;
   uint32 _outputBufferBytesSent;

   const uint32 _minChildWriteSize;
   const uint32 _maxChildWriteSize;
   const uint64 _optMinimumDelayBetweenWritesMicros;

   uint64 _mostRecentChildWriteTime;

   DECLARE_COUNTED_OBJECT(StressTestParserProxyDataIO);
};
DECLARE_REFTYPES(StressTestParserProxyDataIO);

} // end namespace muscle

#endif
