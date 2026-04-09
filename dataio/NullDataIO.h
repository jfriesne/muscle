/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleNullDataIO_h
#define MuscleNullDataIO_h

#include "dataio/DataIO.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateDataSinkSocket()

namespace muscle {

/**
 *  Data I/O equivalent to /dev/null.  A NullDataIO's methods always just return success, but never actually do anything.
 *  The only time they will return an error is if Shutdown() has been called; in that case they will return an error code.
 */
class NullDataIO : public DataIO
{
public:
   /** Default Constructor.  Sets up a DataIO that never has any input data to read, and will always accept output data and discard it.
     * @note in this constructor, the write-select socket will be set by calling CreateDataSinkSocket(false)
     */
   NullDataIO() : _writeSelectSocket(CreateDataSinkSocket(false)), _shutdown(false) {LogWriteSocketSetupErrorsIfAny();}

   /** Explicit constructor.
     * @param readSelectSocket Optional ConstSocketRef to return in GetReadSelectSocket().
     * @note in this constructor, the write-select socket will be set by calling CreateDataSinkSocket(false)
     */
   NullDataIO(const ConstSocketRef & readSelectSocket) : _readSelectSocket(readSelectSocket), _writeSelectSocket(CreateDataSinkSocket(false)), _shutdown(false) {LogWriteSocketSetupErrorsIfAny();}

   /** Explicit constructor.  You can provide one or both sockets to select on, but our Read() and Write() methods will still just do nothing.
     * @param readSelectSocket  Optional ConstSocketRef to return in GetReadSelectSocket().
     * @param writeSelectSocket Optional ConstSocketRef to return in GetWriteSelectSocket().  Note that you'll probably want to pass in the result of CreateDataSinkSocket() here.
     */
   NullDataIO(const ConstSocketRef & readSelectSocket, const ConstSocketRef & writeSelectSocket) : _readSelectSocket(readSelectSocket), _writeSelectSocket(writeSelectSocket), _shutdown(false) {/* empty */}

   /** Virtual Destructor, to keep C++ honest */
   virtual ~NullDataIO() {/* empty */}

   /**
    *  No-op method, always returns zero (except if Shutdown() was called).
    *  @param buffer Points to a buffer to read bytes into (ignored).
    *  @param size Number of bytes in the buffer (ignored).
    *  @return An io_status_t with zero-bytes-read, or B_END_OF_STREAM if Shutdown() has been called.
    */
   virtual io_status_t Read(void * buffer, uint32 size)  {(void) buffer; (void) size; return _shutdown ? io_status_t(B_END_OF_STREAM) : io_status_t();}

   /**
    *  No-op method, always returns (size) (except if Shutdown() was called).
    *  @param buffer Points to a buffer to write bytes from (ignored).
    *  @param size Number of bytes in the buffer (ignored).
    *  @return An io_status_t with (size)-bytes-written, or B_BAD_OBJECT if Shutdown() has been called.
    */
   virtual io_status_t Write(const void * buffer, uint32 size) {(void) buffer; return _shutdown ? io_status_t(B_BAD_OBJECT) : io_status_t((int32) muscleMin((uint32)INT32_MAX, size));}

   /**
    *  No-op method.
    *  This method doesn't do anything at all.
    */
   virtual void FlushOutput() {/* empty */}

   /** Permanently disables this object */
   virtual void Shutdown()
   {
      _shutdown = true;
      _readSelectSocket.Reset();
      _writeSelectSocket.Reset();
   }

   /** Returns the read socket specified in our constructor (if any) */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const {return _readSelectSocket;}

   /** Returns the write socket specified in our constructor (may be a bit-bucket socket) */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _writeSelectSocket;}

private:
   ConstSocketRef _readSelectSocket;
   ConstSocketRef _writeSelectSocket;
   bool _shutdown;

   DECLARE_COUNTED_OBJECT(NullDataIO);

   void LogWriteSocketSetupErrorsIfAny()
   {
      if (_writeSelectSocket() == NULL) LogTime(MUSCLE_LOG_ERROR, "NullDataIO:  CreateDataSinkSocket() failed!  [%s]\n", _writeSelectSocket.GetStatus()());
   }
};
DECLARE_REFTYPES(NullDataIO);

} // end namespace muscle

#endif
