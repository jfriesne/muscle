/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef WIN32  // Windows can't handle file descriptors!

#if defined(__linux__)
# ifndef _FILE_OFFSET_BITS
#  define _FILE_OFFSET_BITS  64 //make sure it's using large file access
# endif
# include <sys/types.h>
# include <unistd.h>
# if !defined(ANDROID)
#  if !__GLIBC_PREREQ(2,2)
#   define MUSCLE_USE_LLSEEK
    _syscall5(int, _llseek, uint, fd, ulong, hi, ulong, lo, loff_t *, res, uint, wh);  // scary --jaf
#  endif
# endif
#endif

#include "dataio/FileDescriptorDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"  // for read_ignore_eintr() and write_ignore_eintr()

namespace muscle {

FileDescriptorDataIO ::
FileDescriptorDataIO(const ConstSocketRef & fd, bool blocking) : _fd(fd), _dofSyncOnClose(false)
{
   SetBlockingIOEnabled(blocking);
}

FileDescriptorDataIO ::
~FileDescriptorDataIO()
{
   if (_dofSyncOnClose)
   {
      int fd = _fd.GetFileDescriptor();
      if (fd >= 0) (void) fsync(fd);
   }
}

int32 FileDescriptorDataIO :: Read(void * buffer, uint32 size)
{
   int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
      int r = read_ignore_eintr(fd, buffer, size);
      return _blocking ? r : ConvertReturnValueToMuscleSemantics(r, size, _blocking);
   }
   else return -1;
}

int32 FileDescriptorDataIO :: Write(const void * buffer, uint32 size)
{
   int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
      int w = write_ignore_eintr(fd, buffer, size);
      return _blocking ? w : ConvertReturnValueToMuscleSemantics(w, size, _blocking);
   }
   else return -1;
}

void FileDescriptorDataIO :: FlushOutput()
{
   // empty
}

status_t FileDescriptorDataIO :: SetBlockingIOEnabled(bool blocking)
{
   int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
      if (fcntl(fd, F_SETFL, blocking ? 0 : O_NONBLOCK) == 0)
      {
         _blocking = blocking;
         return B_NO_ERROR;
      }
      else
      {
         perror("FileDescriptorDataIO:SetBlockingIO failed");
         return B_ERROR;
      }
   }
   else return B_ERROR;
}

void FileDescriptorDataIO :: Shutdown()
{
   _fd.Reset();
}

status_t FileDescriptorDataIO :: Seek(int64 offset, int whence)
{
   int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
      switch(whence)
      {
         case IO_SEEK_SET:  whence = SEEK_SET;  break;
         case IO_SEEK_CUR:  whence = SEEK_CUR;  break;
         case IO_SEEK_END:  whence = SEEK_END;  break;
         default:           return B_ERROR;
      }
#ifdef MUSCLE_USE_LLSEEK
      loff_t spot;
      return (_llseek(fd, (uint32)((offset >> 32) & 0xFFFFFFFF), (uint32)(offset & 0xFFFFFFFF), &spot, whence) >= 0) ? B_NO_ERROR : B_ERROR;
#else
      return (lseek(fd, (off_t) offset, whence) >= 0) ? B_NO_ERROR : B_ERROR;
#endif
   }
   return B_ERROR;
}

int64 FileDescriptorDataIO :: GetPosition() const
{
   int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
#ifdef MUSCLE_USE_LLSEEK
      loff_t spot;
      return (_llseek(fd, 0, 0, &spot, SEEK_CUR) == 0) ? spot : -1;
#else
      return lseek(fd, 0, SEEK_CUR);
#endif
   }
   return -1;
}

}; // end namespace muscle

#endif
