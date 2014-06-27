/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePacketizedDataIO_h
#define MusclePacketizedDataIO_h

#include "dataio/DataIO.h"
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
 * of the change (typically by having the receiver wrap his DataIO object in a PacketizedDataIO
 * object also)
 *
 * You might use this class to simulate a lossless UDP connection by "tunneling" UDP over TCP.
 */
class PacketizedDataIO : public DataIO, private CountedObject<PacketizedDataIO>
{
public:
   /**
    *  Constructor.
    *  @param slaveIO The underlying streaming DataIO object to pass calls on through to.
    *  @param maxTransferUnit the maximum "packet size" we should support.  Calls to
    *         Read() or Write() with more than this many bytes specified will be truncated, 
    *         analogous to the way too-large packets are handled by UDP.
    */
   PacketizedDataIO(const DataIORef & slaveIO, uint32 maxTransferUnit = MUSCLE_NO_LIMIT);

   /** Returns a reference to our underlying DataIO object. */
   const DataIORef & GetSlaveIO() const {return _slaveIO;}

   /** Sets our underlying Data I/O object.  Use with caution! */
   void SetSlaveIO(const DataIORef & sio) {_slaveIO = sio;}

   /** Returns the maximum "packet size" we will be willing to send or receive.  Defaults to MUSCLE_NO_LIMIT. */
   uint32 GetMaxTransferUnit() const {return _maxTransferUnit;}

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual status_t Seek(int64 offset, int whence) {return _slaveIO() ? _slaveIO()->Seek(offset, whence) : B_ERROR;}
   virtual int64 GetPosition() const {return _slaveIO() ? _slaveIO()->GetPosition() : -1;}
   virtual uint64 GetOutputStallLimit() const {return _slaveIO() ? _slaveIO()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;}
   virtual void FlushOutput() {if (_slaveIO()) _slaveIO()->FlushOutput();}
   virtual void Shutdown() {if (_slaveIO()) _slaveIO()->Shutdown(); _slaveIO.Reset(); _outputBuffer.Clear(true); _inputBuffer.Clear(true); _inputBufferSizeBytesRead = 0;}
   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _slaveIO() ? _slaveIO()->GetReadSelectSocket()  : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _slaveIO() ? _slaveIO()->GetWriteSelectSocket() : GetNullSocket();}
   virtual int64 GetLength() {return _slaveIO() ? _slaveIO()->GetLength() : -1;}

   virtual bool HasBufferedOutput() const {return (_outputBufferBytesSent < _outputBuffer.GetNumBytes());}
   virtual void WriteBufferedOutput() {(void) WriteBufferedOutputAux();}

private:
   int32 SlaveRead(void * buffer, uint32 size)        {return _slaveIO() ? _slaveIO()->Read(buffer, size)  : -1;}
   int32 SlaveWrite(const void * buffer, uint32 size) {return _slaveIO() ? _slaveIO()->Write(buffer, size) : -1;}
   status_t WriteBufferedOutputAux();

   DataIORef _slaveIO;
   uint32 _maxTransferUnit;

   ByteBuffer _inputBuffer;
   uint32 _inputBufferSize;
   uint32 _inputBufferSizeBytesRead;
   uint32 _inputBufferBytesRead;
  
   ByteBuffer _outputBuffer;
   uint32 _outputBufferBytesSent;
};

}; // end namespace muscle

#endif
