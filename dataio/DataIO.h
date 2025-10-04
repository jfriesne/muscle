/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataIO_h
#define MuscleDataIO_h

#include "support/NotCopyable.h"
#include "support/PseudoFlattenable.h"
#include "util/Socket.h"
#include "util/TimeUtilityFunctions.h"  // for MUSCLE_TIME_NEVER
#include "util/CountedObject.h"

namespace muscle {

/** Abstract base class interface for any object that can perform basic data I/O operations such as reading or writing bytes.  */
class DataIO : public RefCountable, private NotCopyable
{
public:
   /** Default Constructor */
   DataIO() {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~DataIO() {/* empty */}

   /** Tries to read (size) bytes of new data and place them into (buffer).
    *  Returns the actual number of bytes transferred (which may be smaller than
    *  (size)), or an error code if there was an error.
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes available to write to in (buffer).
    *  @return The number of bytes placed into (buffer), or an error code on error.
    */
   virtual io_status_t Read(void * buffer, uint32 size) = 0;

   /** Reads up to (size) bytes from (buffer) and pushes them into the outgoing
    *  I/O stream.  Returns the actual number of bytes that were transmitted
    *  (which may be smaller than (size)), or an error code if there was an error.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of valid bytes in (buffer).
    *  @return Number of bytes that were transmitted, or an error code on error.
    */
   virtual io_status_t Write(const void * buffer, uint32 size) = 0;

   /**
    * Returns the max number of microseconds to allow
    * for an output stall, before presuming that the I/O is hosed.
    * Default implementation returns MUSCLE_TIME_NEVER, aka no time limit.
    */
   MUSCLE_NODISCARD virtual uint64 GetOutputStallLimit() const {return MUSCLE_TIME_NEVER;}

   /**
    * Flushes the output buffer, if possible.  For some implementations,
    * this is a no-op.  For others (eg TCPSocketDataIO) this can be
    * called to reduced latency of outgoing data blocks.
    */
   virtual void FlushOutput() = 0;

   /**
    * Closes the connection.  After calling this method, the
    * DataIO object should not be used any more.
    */
   virtual void Shutdown() = 0;

   /**
    * This method should return a ConstSocketRef object containing a file descriptor
    * that can be passed to the readSet argument of select(), so that select() can
    * return when there is data available to be read from this DataIO (via Read()).
    *
    * If this DataIO cannot provide a socket that will notify select() about
    * data-ready-to-be-read, then this method should return GetNullSocket().
    *
    * Note that the only thing you are allowed to do with the returned ConstSocketRef
    * is pass it to a SocketMultiplexer to block on (or pass the underlying file descriptor
    * to select()/etc's readSet).  For all other operations, use the appropriate
    * methods in the DataIO interface instead.  If you attempt to do any other I/O operations
    * on Socket or its file descriptor directly, the results are undefined.
    */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const = 0;

   /**
    * This method should return a ConstSocketRef object containing a file descriptor
    * that can be passed to the writeSet argument of select(), so that select() can
    * return when there is buffer space available to Write() to this DataIO.
    *
    * If this DataIO cannot provide a socket that will notify select() about
    * space-ready-to-be-written-to, then this method should return GetNullSocket().
    *
    * Note that the only thing you are allowed to do with the returned ConstSocketRef
    * is pass it to a SocketMultiplexer to block on (or pass the underlying file descriptor
    * to select()/etc's writeSet).  For all other operations, use the appropriate
    * methods in the DataIO interface instead.  If you attempt to do any other I/O operations
    * on Socket or its file descriptor directly, the results are undefined.
    */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const = 0;

   /**
    * Optional:  If your DataIO subclass is holding buffered data that it wants
    *            to output as soon as possible but hasn't been able to yet,
    *            then override this method to return true, and that will cause
    *            WriteBufferedOutput() to be called ASAP.  Default implementation
    *            always returns false.
    */
   MUSCLE_NODISCARD virtual bool HasBufferedOutput() const {return false;}

   /**
    * Optional:  If this DataIO is holding any buffered output data, this method should
    *            be implemented to Write() as much of that data as possible.  Default
    *            implementation is a no-op.
    */
   virtual void WriteBufferedOutput() {/* empty */}

   /** Convenience method:  Calls Write() in a loop until the entire buffer is written, or
     * until an error occurs.  This method should only be used in conjunction with
     * blocking I/O; it will not work reliably with non-blocking I/O.
     * @param buffer Pointer to the first byte of the buffer to write data from.
     * @param size Number of bytes to write
     * @return B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteFully(const void * buffer, uint32 size);

   /** Convenience method:  Calls Read() in a loop until (size) bytes have been
     * read into (buffer).
     * This method should only be used in conjunction with
     * blocking I/O; it will not work reliably with non-blocking I/O.
     * @param buffer Pointer to the first byte of the buffer to place the read data into.
     * @param size Number of bytes to read
     * @return B_NO_ERROR on success, or an error code on failure.  In particular, if end-of-file is
     *         reached before (size) bytes have been read, B_DATA_NOT_FOUND is returned.
     */
   status_t ReadFully(void * buffer, uint32 size);

   /** Convenience method:  Calls Read() in a loop until the entire buffer is read, or
     * until an error occurs, or until end-of-file is reached.
     * This method should only be used in conjunction with
     * blocking I/O; it will not work reliably with non-blocking I/O.
     * @param buffer Pointer to the first byte of the buffer to place the read data into.
     * @param size Number of bytes to read
     * @return B_NO_ERROR (and the number of bytes read) on success, or an error code on failure.
     * @note that the difference between this method and ReadFully() is that this method does not
     *       consider it to be an error-condition if end-of-file is reached before (size) bytes
     *       have been read.
     */
   io_status_t ReadFullyUpTo(void * buffer, uint32 size);

private:
   DECLARE_COUNTED_OBJECT(DataIO);
};
DECLARE_REFTYPES(DataIO);

// The methods below have been implemented here (instead of inside DataFlattener.h or DataUnflattener.h)
// to avoid chicken-and-egg programs with include-ordering.  At this location we are guaranteed that the compiler
// knows everything it needs to know about both the DataFlattener/DataUnflattener classes and the DataIO class.

template<class SubclassType>
status_t PseudoFlattenable<SubclassType>::FlattenToDataIO(DataIO & outputStream, bool addSizeHeader) const
{
   uint8 smallBuf[256];
   uint8 * bigBuf = NULL;

   const SubclassType * sc = static_cast<const SubclassType *>(this);
   const uint32 fs      = sc->FlattenedSize();
   const uint32 bufSize = fs+(addSizeHeader?sizeof(uint32):0);

   uint8 * b;
   if (bufSize<=ARRAYITEMS(smallBuf)) b = smallBuf;
   else
   {
      b = bigBuf = newnothrow_array(uint8, bufSize);
      MRETURN_OOM_ON_NULL(bigBuf);
   }

   // Populate the buffer
   if (addSizeHeader)
   {
      DefaultEndianConverter::Export(fs, b);
      sc->FlattenToBytes(b+sizeof(uint32), fs);
   }
   else FlattenToBytes(b, fs);

   // And finally, write out the buffer
   const status_t ret = outputStream.WriteFully(b, bufSize);
   delete [] bigBuf;
   return ret;
}

template<class SubclassType>
status_t PseudoFlattenable<SubclassType>::UnflattenFromDataIO(DataIO & inputStream, int32 optReadSize, uint32 optMaxReadSize)
{
   uint32 readSize = (uint32) optReadSize;
   if (optReadSize < 0)
   {
      uint32 leSize;
      MRETURN_ON_ERROR(inputStream.ReadFully(&leSize, sizeof(leSize)));

      readSize = DefaultEndianConverter::Import<uint32>(&leSize);
      if (readSize > optMaxReadSize) return B_BAD_DATA;
   }

   uint8 smallBuf[256];
   uint8 * bigBuf = NULL;
   uint8 * b;
   if (readSize<=ARRAYITEMS(smallBuf)) b = smallBuf;
   else
   {
      b = bigBuf = newnothrow_array(uint8, readSize);
      MRETURN_OOM_ON_NULL(bigBuf);
   }

   status_t ret = inputStream.ReadFully(b, readSize).GetStatus();
   if (ret.IsOK()) ret = static_cast<SubclassType *>(this)->UnflattenFromBytes(b, readSize);

   delete [] bigBuf;
   return ret;
}

} // end namespace muscle

#endif
