/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAsyncDataIO_h
#define MuscleAsyncDataIO_h

#include "dataio/ProxyDataIO.h"
#include "system/Thread.h"

namespace muscle {

/**
 * This class can be used to "wrap" a streaming I/O object (e.g. a FileDataIO) in order to make 
 * it appear completely non-blocking (even if the wrapped DataIO requires blocking I/O operations).
 * It does this by handing the file I/O operations off to a separate internal thread, so that the main
 * thread will never block.
 */
class AsyncDataIO : public ProxyDataIO, private Thread
{
public:
   /**
    *  Constructor.
    *  @param childIO The underlying streaming DataIO object to pass calls on through to.
    *                 Note that this I/O object will be operated on from a separate thread,
    *                 and therefore should generally not be accessed further from the main thread,
    *                 to avoid race conditions.
    */
   AsyncDataIO(const DataIORef & childIO) : ProxyDataIO(childIO), _mainThreadBytesWritten(0) {/* empty */}

   virtual ~AsyncDataIO();

   /** This must be called before using the AsyncDataIO object! */
   virtual status_t StartInternalThread();

   /** This may be called before deleting the AsyncDataIO object.  It will be called by the AsyncDataIO destructor
     * in any case, but if you subclassed AsyncDataIO you will want to call it from your subclass's destructor as well,
     * to avoid race conditions.
     * @param waitForThread If true, this method call won't return until the thread has gone away.  defaults to true.
     */
   virtual void ShutdownInternalThread(bool waitForThread = true);

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

protected:
   virtual void InternalThreadEntry();

   /** Called in the internal thread; should return the next time the internal thread should be forced to wake up and
     * call InternalThreadPulse().   Default implementation returns MUSCLE_TIME_NEVER, meaning that InternalThreadPulse()
     * will never be called by default.
     * @param prevPulseTime the time that was previously returned by this method, or MUSCLE_TIME_NEVER if this method
     *                      was never previously called.
     */
   virtual uint64 InternalThreadGetPulseTime(uint64 prevPulseTime) {(void) prevPulseTime; return MUSCLE_TIME_NEVER;}

   /** Called in the internal thread at roughly the time specified by GetInternalThreadPulseTime().
     * Default implementation is a no-op. 
     * @param scheduledPulseTime the time that this method was supposed to be called at (actually call timing may vary somewhat)
     */
   virtual void InternalThreadPulse(uint64 scheduledPulseTime) {(void) scheduledPulseTime;}

   /** May be called by the internal thread to write bytes to the main thread.  These bytes will appear to the
     * main thread as if they had come from the DataIO's normal source.  This is useful for test situations where
     * you want to create mock input data without requiring the presence of a real input device.
     * @param bytes The bytes to send to the main thread
     * @param numBytes How many bytes (bytes) points to
     * @param allowPartialWrite Iff false, then this call will either write all of the bytes or none of them.
     * @returns The number of bytes that were actually written.
     */
   uint32 WriteToMainThread(const uint8 * bytes, uint32 numBytes, bool allowPartialWrite);

private:
   void NotifyInternalThread();

   enum {
      ASYNC_COMMAND_SEEK = 0,
      ASYNC_COMMAND_FLUSH,
      ASYNC_COMMAND_SHUTDOWN,
      NUM_ASYNC_COMMANDS
   };
 
#ifndef DOXYGEN_SHOULD_IGNORE_THIS  // this is here so doxygen-coverage won't complain that I haven't documented this class -- it's a private class so I don't need to
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
#endif
  
   ConstSocketRef _mainThreadNotifySocket, _ioThreadNotifySocket;
   uint64 _mainThreadBytesWritten;
   Mutex _asyncCommandsMutex;
   Queue<AsyncCommand> _asyncCommands;

   // values below should be accessed from within the internal thread only!
   char * _fromChildIOBuf;
   uint32 _fromChildIOBufSize;
   uint32 * _fromChildIOBufReadIdx;
   uint32 * _fromChildIOBufNumValid;

   DECLARE_COUNTED_OBJECT(AsyncDataIO);
};
DECLARE_REFTYPES(AsyncDataIO);

} // end namespace muscle

#endif
