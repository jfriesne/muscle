/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/ByteBufferDataIO.h"

namespace muscle {

ByteBufferDataIO :: ByteBufferDataIO(const ByteBufferRef & buf)
   : _buf(buf)
   , _seekPos(0)
{
   // empty
}

ByteBufferDataIO :: ~ByteBufferDataIO()
{
   // empty
}

io_status_t ByteBufferDataIO :: Read(void * buffer, uint32 size)
{
   if (_buf() == NULL) return B_BAD_OBJECT;

   const uint32 numBytesToCopy = muscleMin(size, GetNumBytesAvailable());
   if (numBytesToCopy == 0) return B_END_OF_STREAM;  // as opposed to "nothing more to read right now"

   memcpy(buffer, _buf()->GetBuffer()+_seekPos, numBytesToCopy);
   _seekPos += numBytesToCopy;
   return numBytesToCopy;
}

io_status_t ByteBufferDataIO :: Write(const void * buffer, uint32 size)
{
   if (_buf() == NULL) return B_BAD_OBJECT;

   const uint32 oldBufSize = _buf()->GetNumBytes();
   const uint32 newBufSize = muscleMax(oldBufSize, _seekPos+size);
   if (newBufSize > oldBufSize) MRETURN_ON_ERROR(_buf()->AppendBytes(NULL, newBufSize-oldBufSize, true)); // enlarge the buffer

   memcpy(_buf()->GetBuffer()+_seekPos, buffer, size);
   _seekPos += size;
   MRETURN_ON_ERROR(_buf()->SetNumBytes(_seekPos, true));  // never touches the heap since we know the ByteBuffer's buffer is already big enough to hold (_seekPos) bytes

   return size;
}

status_t ByteBufferDataIO :: Seek(int64 offset, int whence)
{
   if (_buf() == NULL) return B_BAD_OBJECT;

   int64 newSeekPos;
   switch(whence)
   {
      case IO_SEEK_SET: newSeekPos = offset;             break;
      case IO_SEEK_CUR: newSeekPos = offset+_seekPos;    break;
      case IO_SEEK_END: newSeekPos = GetLength()+offset; break;  // yes, the + is intentional
      default:          return B_BAD_ARGUMENT;
   }
   if ((offset < 0)||(offset > GetLength())) return B_BAD_ARGUMENT;  // yes, the strictly-greater-than test is intentional, so we can successfully seek to EOF

   _seekPos = (uint32) newSeekPos;
   return B_NO_ERROR;
}

status_t ByteBufferDataIO :: Truncate()
{
   return _buf()
        ? _buf()->SetNumBytes(_seekPos, true)
        : B_BAD_OBJECT;
}

} // end namespace muscle
