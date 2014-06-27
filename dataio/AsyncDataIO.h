/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAsyncDataIO_h
#define MuscleAsyncDataIO_h

#include "dataio/DataIO.h"
#include "system/Thread.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 * This class can be used to "wrap" a streaming I/O object (e.g. a FileDataIO) in order to make 
 * it appear completely non-blocking (even if the wrapped DataIO requires blocking I/O operations).
 * It does this by handing the file I/O operations off to a separate internal thread, so that the main
 * thread will never block.
 */
class AsyncDataIO : public DataIO, private Thread, private CountedObject<AsyncDataIO>
{
public:
   /**
    *  Constructor.
    *  @param slaveIO The underlying streaming DataIO object to pass calls on through to.
    *                 Note that this I/O object will be operated on from a separate thread,
    *                 and therefore should generally not be accessed further from the main thread,
    *                 to avoid race conditions.
    */
   AsyncDataIO(const DataIORef & slaveIO) : _slaveIO(slaveIO), _mainThreadBytesWritten(0) {/* empty */}
   virtual ~AsyncDataIO();

   /** This must be called before using the AsyncDataIO object! */
   virtual status_t StartInternalThread();

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual status_t Seek(int64 offset, int whence);

   /** AsyncDataIO::GetPosition() always returns -1, since the current position of the I/O is not well-defined outside of the internal I/O thread. */
   virtual int64 GetPosition() const {return -1;}

   /** Will tell the I/O thread to flush, asynchronously. */
   virtual void FlushOutput();

   /** Will tell the I/O thread to shut down its I/O, asynchronously. */
   virtual void Shutdown();

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return const_cast<AsyncDataIO &>(*this).GetOwnerWakeupSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return const_cast<AsyncDataIO &>(*this).GetOwnerWakeupSocket();}

   /** AsyncDataIO::GetLength() always returns -1, since the current length of the I/O is not well-defined outside of the internal I/O thread. */
   virtual int64 GetLength() {return -1;}

   /** Returns a reference to our held slave I/O object.  Use with caution, since this object may currently be in use by the internal thread! */
   const DataIORef & GetSlaveIO() const {return _slaveIO;}

protected:
   virtual void InternalThreadEntry();

private:
   int32 SlaveRead(void * buffer, uint32 size)        {return _slaveIO() ? _slaveIO()->Read(buffer, size)  : -1;}
   int32 SlaveWrite(const void * buffer, uint32 size) {return _slaveIO() ? _slaveIO()->Write(buffer, size) : -1;}
   void NotifyInternalThread();

   enum {
      ASYNC_COMMAND_SEEK = 0,
      ASYNC_COMMAND_FLUSH,
      ASYNC_COMMAND_SHUTDOWN,
      NUM_ASYNC_COMMANDS
   };
 
   class AsyncCommand 
   {
   public:
      AsyncCommand() : _streamLocation(0), _offset(0), _whence(0), _cmd(NUM_ASYNC_COMMANDS) {/* empty */}
      AsyncCommand(uint64 streamLocation, uint8 cmd) : _streamLocation(streamLocation), _offset(0), _whence(0), _cmd(cmd) {/* empty */}
      AsyncCommand(uint64 streamLocation, uint8 cmd, int64 offset, int whence) : _streamLocation(streamLocation), _offset(offset), _whence(whence), _cmd(cmd) {/* empty */}

      uint64 GetStreamLocation() const {return _streamLocation;}
      uint8 GetCommand() const {return _cmd;}
      int64 GetOffset() const {return _offset;}
      int GetWhence() const {return _whence;}

   private:
      uint64 _streamLocation;
      int64 _offset;
      int _whence;
      uint8 _cmd;
   };
  
   ConstSocketRef _mainThreadNotifySocket, _ioThreadNotifySocket;
   DataIORef _slaveIO;
   uint64 _mainThreadBytesWritten;
   Mutex _asyncCommandsMutex;
   Queue<AsyncCommand> _asyncCommands;
};

}; // end namespace muscle

#endif
