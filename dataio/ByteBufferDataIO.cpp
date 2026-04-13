/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/ByteBufferDataIO.h"

namespace muscle {

ByteBufferDataIO :: ByteBufferDataIO(const ByteBufferRef & buf, bool okayToReturnEndOfStream)
   : _buf(buf)
   , _seekPos(0)
   , _okayToReturnEndOfStream(okayToReturnEndOfStream)
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
   if (size == 0) return io_status_t(0);

   const uint32 numBytesToCopy = muscleMin(size, GetNumBytesAvailable());
   if (numBytesToCopy == 0) return _okayToReturnEndOfStream ? io_status_t(B_END_OF_STREAM) : io_status_t(0);

   memcpy(buffer, _buf()->GetBuffer()+_seekPos, numBytesToCopy);
   _seekPos += numBytesToCopy;
   return numBytesToCopy;
}

io_status_t ByteBufferDataIO :: Write(const void * buffer, uint32 size)
{
   if (_buf() == NULL) return B_BAD_OBJECT;
   if (size == 0) return io_status_t(0);

   if (WillUnsignedAddOverflow(_seekPos, size)) return B_RESOURCE_LIMIT;

   const uint32 oldBufSize = _buf()->GetNumBytes();
   const uint32 newBufSize = muscleMax(oldBufSize, _seekPos+size);
   if (newBufSize > oldBufSize) MRETURN_ON_ERROR(_buf()->AppendBytes(NULL, newBufSize-oldBufSize, true)); // enlarge the buffer

   memcpy(_buf()->GetBuffer()+_seekPos, buffer, size);
   _seekPos += size;

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
      case IO_SEEK_END: newSeekPos = GetLength()+offset; break;  // yes, the + is intentional; in this case, offset is expected to be a negative value, or zero
      default:          return B_BAD_ARGUMENT;
   }
   if ((newSeekPos < 0)||(newSeekPos > GetLength())) return B_BAD_ARGUMENT;  // yes, the strictly-greater-than test is intentional, so we can successfully seek to EOF

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
