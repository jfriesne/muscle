/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"
#include "system/GlobalMemoryAllocator.h"  // for muscleStrdup()

#ifdef WIN32
# include <io.h>  // for _fileno, _chsize_s
#endif

namespace muscle {

FileDataIO :: FileDataIO(FILE * file)
   : _pendingFilePath(NULL)
   , _pendingFileMode(NULL)
   , _file(file)
{
   SetSocketsFromFile(_file);
}

FileDataIO :: FileDataIO(const char * path, const char * mode)
   : _pendingFilePath(muscleStrdup(path))
   , _pendingFileMode(muscleStrdup(mode))
   , _file(NULL)
{
   // empty
}

FileDataIO :: ~FileDataIO()
{
   FreePendingFileInfo();
   if (_file) fclose(_file);
}

io_status_t FileDataIO :: Read(void * buffer, uint32 size)
{
   if (_file)
   {
      const size_t ret = fread(buffer, 1, size, _file);
      return (ret > 0) ? io_status_t((int32)ret) : (ferror(_file) ? B_IO_ERROR : B_END_OF_STREAM);
   }
   else return EnsureDeferredModeFopenCalled() ? Read(buffer, size) : io_status_t(B_BAD_OBJECT);
}

io_status_t FileDataIO :: Write(const void * buffer, uint32 size)
{
   if (_file)
   {
      const size_t ret = fwrite(buffer, 1, size, _file);
      return (ret > 0) ? io_status_t((int32)ret) : io_status_t(B_IO_ERROR);
   }
   else return EnsureDeferredModeFopenCalled() ? Write(buffer, size) : io_status_t(B_BAD_OBJECT);
}

status_t FileDataIO :: Seek(int64 offset, int whence)
{
   if (_file == NULL) return EnsureDeferredModeFopenCalled() ? Seek(offset, whence) : B_BAD_OBJECT;

   switch(whence)
   {
      case IO_SEEK_SET:  whence = SEEK_SET;  break;
      case IO_SEEK_CUR:  whence = SEEK_CUR;  break;
      case IO_SEEK_END:  whence = SEEK_END;  break;
      default:           return B_BAD_ARGUMENT;
   }
#ifdef WIN32
   return (_fseeki64(_file, offset, whence) == 0) ? B_NO_ERROR : B_IO_ERROR;
#else
   return (   fseeko(_file, offset, whence) == 0) ? B_NO_ERROR : B_IO_ERROR;
#endif
}

int64 FileDataIO :: GetPosition() const
{
#ifdef WIN32
   return _file ? (int64) _ftelli64(_file) : (_pendingFilePath?0:-1);
#else
   return _file ? (int64)    ftello(_file) : (_pendingFilePath?0:-1);
#endif
}

status_t FileDataIO :: Truncate()
{
   if (_file == NULL) return EnsureDeferredModeFopenCalled() ? Truncate() : B_BAD_OBJECT;

   const int64 curPos = GetPosition();
   if (curPos < 0) return B_BAD_OBJECT;

   if (fflush(_file) != 0) return B_ERRNO;  // make sure all recently-buffered data gets pushed to disk before we start chopping

#ifdef WIN32
   const int fd = _fileno(_file);
   if (fd < 0) return B_BAD_OBJECT;

   const int r = _chsize_s(fd, curPos);
   return (r == 0) ? B_NO_ERROR : B_ERRNUM(r);
#else
   const int fd = fileno(_file);
   if (fd < 0) return B_BAD_OBJECT;

   const int r = ftruncate(fd, curPos);
   return (r == 0) ? B_NO_ERROR : B_ERRNO;
#endif
}

void FileDataIO :: FlushOutput()
{
   if (_file) (void) fflush(_file);
}

void FileDataIO :: Shutdown()
{
   if (_file)
   {
      fclose(_file);
      ReleaseFile();
   }
   FreePendingFileInfo();
}

void FileDataIO :: ReleaseFile()
{
   _file = NULL;
   SetSocketsFromFile(NULL);
}

void FileDataIO :: SetFile(FILE * fp)
{
   Shutdown();
   _file = fp;
   SetSocketsFromFile(_file);
}

void FileDataIO :: SetSocketsFromFile(FILE * optFile)
{
   _selectSocketRef.Reset();
#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE   // windows can't do the select-on-file-descriptor trick, sorry!
   _selectSocket.Clear();

   const int fd = optFile ? fileno(optFile) : -1;
   if (fd >= 0)
   {
      _selectSocket.SetFileDescriptor(fd, false);  // false because the fclose() will call close(fd), so we should not
      _selectSocketRef.SetRef(&_selectSocket, false);
   }
#else
   (void) optFile; // avoid compiler warning
#endif
}

void FileDataIO :: FreePendingFileInfo()
{
   if (_pendingFilePath) {muscleFree(_pendingFilePath); _pendingFilePath = NULL;}
   if (_pendingFileMode) {muscleFree(_pendingFileMode); _pendingFileMode = NULL;}
}

bool FileDataIO :: EnsureDeferredModeFopenCalled()
{
   if ((_pendingFilePath)&&(_file == NULL))
   {
      SetFile(muscleFopen(_pendingFilePath, _pendingFileMode?_pendingFileMode:"rb"));  // SetFile() will call Shutdown()
      return (_file != NULL);                                                          // which will call FreePendingFileInfo()
   }
   return false;
}

} // end namespace muscle
