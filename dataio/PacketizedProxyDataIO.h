/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePacketizedProxyDataIO_h
#define MusclePacketizedProxyDataIO_h

#include "dataio/ProxyDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 * This class can be used to "wrap" a streaming I/O object (for example a TCPSocketDataIO) in order to make 
 * it appear like a packet-based I/O object (e.g. a UDPSocketDataIO) to the calling code.
 * 
 * It does this by inserting message-length fields into the outgoing byte stream, and parsing
 * message-length fields from the incoming byte stream, so that data will be returned in Read()
 * in the same chunk sizes that it was originally passed in to Write() on the other end.  Note
 * that this does change the underlying byte-stream protocol, so the receiver must be aware
 * of the change (typically by having the receiver wrap his DataIO object in a PacketizedProxyDataIO
 * object also)
 *
 * You might use this class to simulate a lossless UDP connection by "tunneling" UDP over TCP.
 */
class PacketizedProxyDataIO : public ProxyDataIO
{
public:
   /**
    *  Constructor.
    *  @param childIO The underlying streaming DataIO object to pass calls on through to.
    *  @param maxTransferUnit the maximum "packet size" we should support.  Calls to
    *         Read() or Write() with more than this many bytes specified will be truncated, 
    *         analogous to the way too-large packets are handled by UDP.
    */
   PacketizedProxyDataIO(const DataIORef & childIO, uint32 maxTransferUnit = MUSCLE_NO_LIMIT);

   /** Returns the maximum "packet size" we will be willing to send or receive.  Defaults to MUSCLE_NO_LIMIT. */
   uint32 GetMaxTransferUnit() const {return _maxTransferUnit;}

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual void Shutdown() {ProxyDataIO::Shutdown(); _outputBuffer.Clear(true); _inputBuffer.Clear(true); _inputBufferSizeBytesRead = 0;}

   virtual bool HasBufferedOutput() const {return (_outputBufferBytesSent < _outputBuffer.GetNumBytes());}
   virtual void WriteBufferedOutput() {(void) WriteBufferedOutputAux();}

private:
   status_t WriteBufferedOutputAux();

   uint32 _maxTransferUnit;

   ByteBuffer _inputBuffer;
   uint32 _inputBufferSize;
   uint32 _inputBufferSizeBytesRead;
   uint32 _inputBufferBytesRead;
  
   ByteBuffer _outputBuffer;
   uint32 _outputBufferBytesSent;

   DECLARE_COUNTED_OBJECT(PacketizedProxyDataIO);
};
DECLARE_REFTYPES(PacketizedProxyDataIO);

} // end namespace muscle

#endif
