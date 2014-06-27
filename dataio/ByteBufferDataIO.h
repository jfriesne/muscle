/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteBufferDataIO_h
#define MuscleByteBufferDataIO_h

#include "dataio/DataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 *  Data I/O class to allow reading from/writing to a ByteBuffer object (as if it was an I/O device)
 *  The ByteBuffer will behave much like a file would (automatically being resized larger when you
 *  Write() past the end of it, etc), except of course it's all in memory, not on disk.
 */
class ByteBufferDataIO : public DataIO, private CountedObject<ByteBufferDataIO>
{
public:
   /** Constructor.
    *  @param buf Reference to the byte buffer to read from.  If not specified (or specified
    *             as a NULL reference), you will need to call SetBuffer() before using this ByteBufferDataIO.
    */
   ByteBufferDataIO(const ByteBufferRef & buf = ByteBufferRef()) : _buf(buf), _seekPos(0) {/* empty */}

   /** Virtual Destructor, to keep C++ honest */
   virtual ~ByteBufferDataIO() {/* empty */}

   /** Sets our held buffer to a different value.  Note that this call will not change the seek
     * position of this ByteBufferDataIO, so you may want to call Seek() also.
     */
   void SetBuffer(const ByteBufferRef & buf) {_buf = buf;}

   /** Returns the current byte buffer reference we are using. */
   const ByteBufferRef & GetBuffer() const {return _buf;}

   /** 
    *  Copies bytes from our ByteBuffer into (buffer).  If we have no buffer, returns -1.
    *  @param buffer Points to a buffer to read bytes into.
    *  @param size Number of bytes in the buffer.
    *  @return zero.
    */
   virtual int32 Read(void * buffer, uint32 size)  
   {
      if (_buf())
      {
         int32 copyBytes = muscleMin((int32)size, muscleMax((int32)0, (int32)(_buf()->GetNumBytes()-_seekPos)));
         memcpy(buffer, _buf()->GetBuffer()+_seekPos, copyBytes);
         _seekPos += copyBytes;
         return copyBytes;
      }
      return -1;
   }

   /** 
    *  Writes bytes into our write buffer.  If we have no write buffer, or we cannot allocate more memory for the write buffer, returns -1.
    *  @param buffer Points to a buffer to write bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return (size).
    */
   virtual int32 Write(const void * buffer, uint32 size) 
   {
      if (_buf() == NULL) return -1;

      uint32 oldBufSize = _buf()->GetNumBytes();
      uint32 pastOffset = muscleMax(oldBufSize, _seekPos+size);
      if (pastOffset > oldBufSize)
      {
         uint32 preallocBytes = (pastOffset*2);  // exponential resize to avoid too many reallocs
         if (_buf()->SetNumBytes(preallocBytes, true) != B_NO_ERROR) return -1;   // allocate the memory
         memset(_buf()->GetBuffer()+oldBufSize, 0, preallocBytes-oldBufSize);  // make sure newly alloc'd memory is zeroed out!
         (void) _buf()->SetNumBytes(pastOffset, true);  // guaranteed not to fail
      }
      
      memcpy(_buf()->GetBuffer()+_seekPos, buffer, size);
      _seekPos += size;
      return size;
   }

   /** Seeks to the specified point in our ByteBuffer.
    *  Note that only 32-bit seeks are supported in this implementation.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END. 
    *  @return B_NO_ERROR on success, B_ERROR on failure
    */ 
   virtual status_t Seek(int64 offset, int whence)
   {
      uint32 fileLen = _buf() ? _buf()->GetNumBytes() : 0;
      int32 o = (int32) offset;
      int32 newSeekPos = -1;
      switch(whence)
      {
         case IO_SEEK_SET:  newSeekPos = o;          break;
         case IO_SEEK_CUR:  newSeekPos = _seekPos+o; break;
         case IO_SEEK_END:  newSeekPos = fileLen-o;  break;
         default:           return B_ERROR;
      }
      if (newSeekPos < 0) return B_ERROR;
      _seekPos = newSeekPos;
      return B_NO_ERROR;
   }
   
   virtual int64 GetPosition() const {return _seekPos;}

   /** 
    *  No-op method.
    *  This method doesn't do anything at all.
    */
   virtual void FlushOutput() {/* empty */}

   /** Disable us! */ 
   virtual void Shutdown() {_buf.Reset();}

   /** Can't select on this one, sorry */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Can't select on this one, sorry */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

private:
   ByteBufferRef _buf;
   int32 _seekPos;
};

}; // end namespace muscle

#endif
