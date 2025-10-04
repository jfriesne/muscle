/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteBufferDataIO_h
#define MuscleByteBufferDataIO_h

#include "dataio/SeekableDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 *  DataIO class to allow reading from/writing to a ByteBuffer object (as if it was an I/O device)
 *  The ByteBuffer will behave much like a file would (automatically being resized larger when you
 *  Write() past the end of it, etc), except of course it's all in memory, not on disk.
 */
class ByteBufferDataIO : public SeekableDataIO
{
public:
   /** Constructor.
    *  @param buf Reference to the byte buffer to read from.  If not specified (or specified
    *             as a NULL reference), you will need to call SetBuffer() before using this ByteBufferDataIO.
    */
   ByteBufferDataIO(const ByteBufferRef & buf = ByteBufferRef());

   /** Virtual Destructor, to keep C++ honest */
   virtual ~ByteBufferDataIO();

   /** Sets our held buffer to a different value.  Note that this call will not change the seek
     * position of this ByteBufferDataIO, so you may want to call Seek() also.
     * @param buf the new ByteBufferRef to use as our data source and/or destination
     */
   void SetBuffer(const ByteBufferRef & buf) {_buf = buf;}

   /** Returns the current byte buffer reference we are using. */
   MUSCLE_NODISCARD const ByteBufferRef & GetBuffer() const {return _buf;}

   /**
    *  Copies bytes from our ByteBuffer into (buffer).  If we have no buffer, returns B_BAD_OBJECT.
    *  @param buffer Points to a buffer to read bytes into.
    *  @param size Number of bytes in the buffer.
    *  @returns the number of bytes that were written into (buffer), or an error code.
    */
   virtual io_status_t Read(void * buffer, uint32 size);

   /**
    *  Writes bytes into our write buffer.  If we have no write buffer, or we cannot allocate more memory for the write buffer, returns B_BAD_OBJECT.
    *  @param buffer Points to a buffer to write bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return the number of bytes that were read out of (buffer), or an error code.
    */
   virtual io_status_t Write(const void * buffer, uint32 size);

   /** Seeks to the specified point in our ByteBuffer.
    *  Note that only 32-bit seeks are supported in this implementation.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure.
    */
   virtual status_t Seek(int64 offset, int whence);

   /** Returns our current seek-position within the ByteBuffer */
   MUSCLE_NODISCARD virtual int64 GetPosition() const {return _seekPos;}

   /** Returns the number of valid bytes that are currently in our ByteBuffer */
   MUSCLE_NODISCARD virtual int64 GetLength() const {return _buf() ? _buf()->GetNumBytes() : 0;}

   /** Truncates the ByteBuffer's length to our current seek-position */
   virtual status_t Truncate();

   /** Implemented as a no-op, since a ByteBuffer has no concept of flushing output */
   virtual void FlushOutput() {/* empty */}

   /** Implemented to set our held ByteBufferRef to NULL */
   virtual void Shutdown() {_buf.Reset();}

   /** Returns a null ConstSocketRef, since there's no way to select() on a ByteBuffer */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Returns a null ConstSocketRef, since there's no way to select() on a ByteBuffer */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

private:
   ByteBufferRef _buf;
   int32 _seekPos;

   DECLARE_COUNTED_OBJECT(ByteBufferDataIO);
};
DECLARE_REFTYPES(ByteBufferDataIO);

} // end namespace muscle

#endif
