/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDescriptorDataIO.h"  // must be first!

#ifndef WIN32  // Windows can't handle file descriptors!

#include <sys/stat.h>  // for fstat()

#if defined(__linux__)
# ifndef _FILE_OFFSET_BITS
#  define _FILE_OFFSET_BITS  64 // NOLINT - make sure it's using large file access
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

#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"  // for read_ignore_eintr() and write_ignore_eintr()

namespace muscle {

FileDescriptorDataIO ::
FileDescriptorDataIO(const ConstSocketRef & fd, bool blocking)
   : _fd(fd)
   , _blocking(true)  // just to keep the static analyzers happy
   , _dofSyncOnClose(false)
{
   (void) SetBlockingIOEnabled(blocking);
}

FileDescriptorDataIO ::
~FileDescriptorDataIO()
{
   if (_dofSyncOnClose)
   {
      const int fd = _fd.GetFileDescriptor();
      if (fd >= 0) (void) fsync(fd);
   }
}

io_status_t FileDescriptorDataIO :: Read(void * buffer, uint32 size)
{
   const int fd = _fd.GetFileDescriptor();
   if (fd < 0) return B_BAD_OBJECT;

   const int32 r = read_ignore_eintr(fd, buffer, size);
   if (r == 0) return B_END_OF_STREAM;

   const int32 er = _blocking ? r : ConvertReturnValueToMuscleSemantics(r, size, _blocking);
   return (er >= 0) ? io_status_t(er) : io_status_t(B_ERRNO);
}

io_status_t FileDescriptorDataIO :: Write(const void * buffer, uint32 size)
{
   const int fd = _fd.GetFileDescriptor();
   if (fd < 0) return B_BAD_OBJECT;

   const long   w = write_ignore_eintr(fd, buffer, size);
   const int32 ew = _blocking ? (int32)w : ConvertReturnValueToMuscleSemantics(w, size, _blocking);
   return (ew >= 0) ? io_status_t(ew) : io_status_t(B_ERRNO);
}

void FileDescriptorDataIO :: FlushOutput()
{
   // empty
}

status_t FileDescriptorDataIO :: SetBlockingIOEnabled(bool blocking)
{
   const int fd = _fd.GetFileDescriptor();
   if (fd >= 0)
   {
      if (fcntl(fd, F_SETFL, blocking ? 0 : O_NONBLOCK) == 0)
      {
         _blocking = blocking;
         return B_NO_ERROR;
      }
      else return B_ERRNO;
   }
   else return B_BAD_OBJECT;
}

void FileDescriptorDataIO :: Shutdown()
{
   _fd.Reset();
}

status_t FileDescriptorDataIO :: Seek(int64 offset, int whence)
{
   const int fd = _fd.GetFileDescriptor();
   if (fd < 0) return B_BAD_OBJECT;

   switch(whence)
   {
      case IO_SEEK_SET:  whence = SEEK_SET;  break;
      case IO_SEEK_CUR:  whence = SEEK_CUR;  break;
      case IO_SEEK_END:  whence = SEEK_END;  break;
      default:           return B_BAD_ARGUMENT;
   }

#ifdef MUSCLE_USE_LLSEEK
   loff_t spot;
   return (_llseek(fd, (uint32)((offset >> 32) & 0xFFFFFFFF), (uint32)(offset & 0xFFFFFFFF), &spot, whence) >= 0) ? B_NO_ERROR : B_ERRNO;
#else
   return (lseek(fd, (off_t) offset, whence) >= 0) ? B_NO_ERROR : B_ERRNO;
#endif
}

int64 FileDescriptorDataIO :: GetPosition() const
{
   const int fd = _fd.GetFileDescriptor();
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

int64 FileDescriptorDataIO :: GetLength() const
{
   const int fd = _fd.GetFileDescriptor();
   if (fd < 0) return -1;

   struct stat st;
   return (fstat(fd, &st) == 0) ? st.st_size : -1;
}

status_t FileDescriptorDataIO :: Truncate()
{
   const int64 curPos = GetPosition();
   if (curPos < 0) return B_BAD_OBJECT;

   const int r = ftruncate(_fd.GetFileDescriptor(), curPos);
   return (r == 0) ? B_NO_ERROR : B_ERRNO;
}

} // end namespace muscle

#endif
