/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleReadOnlyByteBufferDataIO_h
#define MuscleReadOnlyByteBufferDataIO_h

#include "dataio/SeekableDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 *  DataIO class to allow reading from a ConstByteBuffer object (as if it was an I/O device)
 *  The ConstByteBuffer will behave much like a read-only file would.
 */
class ReadOnlyByteBufferDataIO : public SeekableDataIO
{
public:
   /** Constructor.
    *  @param buf Reference to the byte buffer to read from.  If not specified (or specified
    *             as a NULL reference), you will need to call SetBuffer() before using this ReadOnlyByteBufferDataIO.
    *  @param okayToReturnEndOfStream if true, then when Read() has no more data to supply because the
    *             seek-point has reached the end of the held ConstByteBuffer, Read() will return B_END_OF_STREAM
    *             to indicate that fact.  If false, it will just return 0.  Default value is false.
    */
   ReadOnlyByteBufferDataIO(const ConstByteBufferRef & buf = ConstByteBufferRef(), bool okayToReturnEndOfStream = false);

   /** Virtual Destructor, to keep C++ honest */
   virtual ~ReadOnlyByteBufferDataIO();

   /** Sets our held buffer to a different value.  Note that this call will not change the seek
     * position of this ReadOnlyByteBufferDataIO, so you may want to call Seek() also.
     * @param buf the new ConstByteBufferRef to use as our data source and/or destination
     */
   void SetBuffer(const ConstByteBufferRef & buf) {_buf = buf;}

   /** Returns the current byte buffer reference we are using. */
   MUSCLE_NODISCARD const ConstByteBufferRef & GetBuffer() const {return _buf;}

   /**
    *  Copies bytes from our ConstByteBuffer into (buffer).  If we have no buffer, returns B_BAD_OBJECT.
    *  @param buffer Points to a buffer to read bytes into.
    *  @param size Number of bytes in the buffer.
    *  @returns the number of bytes that were written into (buffer), or an error code.
    */
   virtual io_status_t Read(void * buffer, uint32 size);

   /**
    *  Implemented to return B_ACCESS_DENIED because this class's byte buffer is read only.
    *  @param buffer Points to a buffer to write bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return B_ACCESS_DENIED, unless or buffer is NULL, in which case it will return B_BAD_OBJECT instead.
    */
   virtual io_status_t Write(const void * buffer, uint32 size);

   /** Seeks to the specified point in our ConstByteBuffer.
    *  Note that only 32-bit seeks are supported in this implementation.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure.
    */
   virtual status_t Seek(int64 offset, int whence);

   /** Returns our current seek-position within the ConstByteBuffer */
   MUSCLE_NODISCARD virtual int64 GetPosition() const {return (int64) _seekPos;}

   /** Returns the number of valid bytes that are currently in our ConstByteBuffer */
   MUSCLE_NODISCARD virtual int64 GetLength() const {return _buf() ? _buf()->GetNumBytes() : 0;}

   /** Returns B_ACCESS_DENIED or B_BAD_OBJECT, as modifying the ConstByteBuffer is not supported. */
   virtual status_t Truncate();

   /** Implemented as a no-op, since a ConstByteBuffer has no concept of flushing output */
   virtual void FlushOutput() {/* empty */}

   /** Implemented to set our held ConstByteBufferRef to NULL */
   virtual void Shutdown() {_buf.Reset();}

   /** Returns a null ConstSocketRef, since there's no way to select() on a ConstByteBuffer */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Returns a null ConstSocketRef, since there's no way to select() on a ConstByteBuffer */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

private:
   // Returns the number of bytes that can still be read starting at the current seek-position
   uint32 GetNumBytesAvailable() const
   {
      const uint32 numBytesInBuffer = _buf() ? _buf()->GetNumBytes() : 0;
      return (_seekPos < numBytesInBuffer) ? (numBytesInBuffer-_seekPos) : 0;
   }

   ConstByteBufferRef _buf;
   uint32 _seekPos;
   const bool _okayToReturnEndOfStream;

   DECLARE_COUNTED_OBJECT(ReadOnlyByteBufferDataIO);
};
DECLARE_REFTYPES(ReadOnlyByteBufferDataIO);

} // end namespace muscle

#endif
