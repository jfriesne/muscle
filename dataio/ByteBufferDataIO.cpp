/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/ByteBufferDataIO.h"

namespace muscle {

ByteBufferDataIO :: ByteBufferDataIO(const ByteBufferRef & buf) 
   : _buf(buf), _seekPos(0)
{
   // empty
}

ByteBufferDataIO :: ~ByteBufferDataIO()
{
   // empty
}

int32 ByteBufferDataIO :: Read(void * buffer, uint32 size)  
{
   if (_buf())
   {
      const int32 copyBytes = muscleMin((int32)size, muscleMax((int32)0, (int32)(_buf()->GetNumBytes()-_seekPos)));
      memcpy(buffer, _buf()->GetBuffer()+_seekPos, copyBytes);
      _seekPos += copyBytes;
      return copyBytes;
   }
   return -1;
}

int32 ByteBufferDataIO :: Write(const void * buffer, uint32 size) 
{
   if (_buf() == NULL) return -1;

   const uint32 oldBufSize = _buf()->GetNumBytes();
   const uint32 pastOffset = muscleMax(oldBufSize, _seekPos+size);
   if (pastOffset > oldBufSize)
   {
      const uint32 preallocBytes = (pastOffset*2);  // exponential resize to avoid too many reallocs
      if (_buf()->SetNumBytes(preallocBytes, true).IsError()) return -1;   // allocate the memory
      memset(_buf()->GetBuffer()+oldBufSize, 0, preallocBytes-oldBufSize);  // make sure newly alloc'd memory is zeroed out!
      (void) _buf()->SetNumBytes(pastOffset, true);  // guaranteed not to fail
   }
   
   memcpy(_buf()->GetBuffer()+_seekPos, buffer, size);
   _seekPos += size;
   return size;
}

status_t ByteBufferDataIO :: Seek(int64 offset, int whence)
{
   const uint32 fileLen = _buf() ? _buf()->GetNumBytes() : 0;
   const int32 o = (int32) offset;
   int32 newSeekPos = -1;
   switch(whence)
   {
      case IO_SEEK_SET:  newSeekPos = o;          break;
      case IO_SEEK_CUR:  newSeekPos = _seekPos+o; break;
      case IO_SEEK_END:  newSeekPos = fileLen-o;  break;
      default:           return B_BAD_ARGUMENT;
   }
   if (newSeekPos < 0) return B_BAD_ARGUMENT;
   _seekPos = newSeekPos;
   return B_NO_ERROR;
}
   
} // end namespace muscle
