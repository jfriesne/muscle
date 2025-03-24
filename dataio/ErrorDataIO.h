/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleErrorDataIO_h
#define MuscleErrorDataIO_h

#include "dataio/DataIO.h"

namespace muscle {

/**
 *  An ErrorDataIO's methods always just return B_BAD_OBJECT, no matter what.
 *  The main purpose of this class is just to act as a dummy-object so that upper-level classes
 *  who are lacking a genuine DataIO object don't have to check for that condition separately.
 */
class ErrorDataIO : public DataIO
{
public:
   /** Constructor.
     * @param readSelectSocket  Optional ConstSocketRef to return in GetReadSelectSocket().   Defaults to a NULL ref.
     * @param writeSelectSocket Optional ConstSocketRef to return in GetWriteSelectSocket().  Defaults to a NULL ref.
     */
   ErrorDataIO(const ConstSocketRef & readSelectSocket = ConstSocketRef(), const ConstSocketRef & writeSelectSocket = ConstSocketRef()) : _readSelectSocket(readSelectSocket), _writeSelectSocket(writeSelectSocket) {/* empty */}

   /** Virtual Destructor, to keep C++ honest */
   virtual ~ErrorDataIO() {/* empty */}

   /**
    *  No-op method, always returns B_BAD_OBJECT.
    *  @param buffer Points to a buffer to read bytes into (ignored).
    *  @param size Number of bytes in the buffer (ignored).
    *  @returns B_BAD_OBJECT
    */
   virtual io_status_t Read(void * buffer, uint32 size)  {(void) buffer; (void) size; return io_status_t(B_BAD_OBJECT);}

   /**
    *  No-op method, always returns B_BAD_OBJECT.
    *  @param buffer Points to a buffer to write bytes from (ignored).
    *  @param size Number of bytes in the buffer (ignored).
    *  @returns B_BAD_OBJECT
    */
   virtual io_status_t Write(const void * buffer, uint32 size) {(void) buffer; (void) size; return io_status_t(B_BAD_OBJECT);}

   /**
    *  No-op method.
    *  This method doesn't do anything at all.
    */
   virtual void FlushOutput() {/* empty */}

   /** No-op method
    *  This method doesn't do anything at all.
    */
   virtual void Shutdown() {/* empty */}

   /** Returns the read socket specified in our constructor (if any) */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const {return _readSelectSocket;}

   /** Returns the write socket specified in our constructor (if any) */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _writeSelectSocket;}

private:
   ConstSocketRef _readSelectSocket;
   ConstSocketRef _writeSelectSocket;

   DECLARE_COUNTED_OBJECT(ErrorDataIO);
};
DECLARE_REFTYPES(ErrorDataIO);

} // end namespace muscle

#endif
