/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(WIN32)
#include "winsupport/Win32FileHandleDataIO.h"

namespace muscle {

Win32FileHandleDataIO ::
Win32FileHandleDataIO(::HANDLE handle) : _handle(handle)
{
   // empty
}

Win32FileHandleDataIO ::
~Win32FileHandleDataIO() 
{
   if (_handle != INVALID_HANDLE_VALUE) CloseHandle(_handle);
}

int32 Win32FileHandleDataIO :: Read(void * buffer, uint32 size)  
{
   if (_handle != INVALID_HANDLE_VALUE)
   {
      DWORD readCount=0;
      return ReadFile(_handle, buffer, size, &readCount, 0) ? readCount : -1;
   }
   else return -1;
}

int32 Win32FileHandleDataIO :: Write(const void * buffer, uint32 size)
{
   if (_handle != INVALID_HANDLE_VALUE) 
   {
      DWORD writeCount;
      return WriteFile(_handle, buffer, size, &writeCount, 0) ? writeCount : -1;
   }
   else return -1;
}

void Win32FileHandleDataIO :: FlushOutput()
{
   // empty
}

status_t Win32FileHandleDataIO :: SetBlockingIOEnabled(bool blocking)
{
   if (_handle == INVALID_HANDLE_VALUE) return B_BAD_OBJECT;
   return blocking ? B_NO_ERROR : B_UNIMPLEMENTED;  // only blocking-mode supported for now
}

void Win32FileHandleDataIO :: Shutdown()
{
   if (_handle != INVALID_HANDLE_VALUE) 
   {
      CloseHandle(_handle);
      _handle = INVALID_HANDLE_VALUE;
   }
}

status_t Win32FileHandleDataIO :: Seek(int64 offset, int whence)
{
   if (_handle == INVALID_HANDLE_VALUE) return B_BAD_OBJECT;

   switch(whence)
   {
      case IO_SEEK_SET: whence = FILE_BEGIN;   break;
      case IO_SEEK_CUR: whence = FILE_CURRENT; break;
      case IO_SEEK_END: whence = FILE_END;     break;
      default:          return B_BAD_ARGUMENT;
   }

   LARGE_INTEGER newPosition;    newPosition.QuadPart    = 0;
   LARGE_INTEGER distanceToMove; distanceToMove.QuadPart = offset;
   return SetFilePointerEx(_handle, distanceToMove, &newPosition, whence) ? B_NO_ERROR : B_ERRNO;
}

int64 Win32FileHandleDataIO :: GetPosition() const
{
   if (_handle != INVALID_HANDLE_VALUE)
   {
      LARGE_INTEGER newPosition;    newPosition.QuadPart    = 0;
      LARGE_INTEGER distanceToMove; distanceToMove.QuadPart = 0;
      return SetFilePointerEx(_handle, distanceToMove, &newPosition, FILE_CURRENT) ? newPosition.QuadPart : -1;
   }
   return -1;
}

} // end namespace muscle

#endif
