/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"

namespace muscle {

FileDataIO :: FileDataIO(FILE * file)
   : _file(file) 
{
   SetSocketsFromFile(_file);
}

FileDataIO ::~FileDataIO() 
{
   if (_file) fclose(_file);
}

int32 FileDataIO :: Read(void * buffer, uint32 size)  
{
   if (_file)
   {
      const int32 ret = (int32) fread(buffer, 1, size, _file);
      return (ret > 0) ? ret : -1;  // EOF is an error, and it's returned as zero
   }
   else return -1;
}

int32 FileDataIO :: Write(const void * buffer, uint32 size)
{
   if (_file)
   {
      const int32 ret = (int32) fwrite(buffer, 1, size, _file);
      return (ret > 0) ? ret : -1;   // zero is an error
   }
   else return -1;
}

status_t FileDataIO :: Seek(int64 offset, int whence)
{
   if (_file == NULL) return B_BAD_OBJECT;

   switch(whence)
   {
      case IO_SEEK_SET:  whence = SEEK_SET;  break;
      case IO_SEEK_CUR:  whence = SEEK_CUR;  break;
      case IO_SEEK_END:  whence = SEEK_END;  break;
      default:           return B_BAD_ARGUMENT;
   }
   return (fseek(_file, (long) offset, whence) == 0) ? B_NO_ERROR : B_ERRNO;
}

int64 FileDataIO :: GetPosition() const
{
   return _file ? (int64) ftell(_file) : -1;
}

void FileDataIO :: FlushOutput() 
{
   if (_file) fflush(_file);
}

void FileDataIO :: Shutdown()
{
   if (_file)
   {
      fclose(_file);
      ReleaseFile();
   }
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

} // end namespace muscle
