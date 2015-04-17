#include "util/FilePathInfo.h"
#include "system/SystemInfo.h"  // for GetFilePathSeparator()

#ifndef WIN32
# if defined(__APPLE__) && defined(MUSCLE_64_BIT_PLATFORM) && !defined(_DARWIN_FEATURE_64_BIT_INODE)
#  define _DARWIN_FEATURE_64_BIT_INODE   // enable 64-bit stat(), if it isn't already enabled
# endif
# include <sys/stat.h>
#endif

namespace muscle {

FilePathInfo :: FilePathInfo(bool exists, bool isRegularFile, bool isDir, bool isSymlink, uint64 fileSizeBytes, uint64 aTime, uint64 cTime, uint64 mTime)
   : _flags((exists?(1<<FPI_FLAG_EXISTS):0)|(isRegularFile?(1<<FPI_FLAG_ISREGULARFILE):0)|(isDir?(1<<FPI_FLAG_ISDIRECTORY):0)|(isSymlink?(1<<FPI_FLAG_ISSYMLINK):0)),
     _size(fileSizeBytes), _atime(aTime), _ctime(cTime), _mtime(mTime)
{
   // empty
}

void FilePathInfo :: SetFilePath(const char * optFilePath)
{
   size_t sLen = optFilePath ? strlen(optFilePath) : 0;
   if ((sLen > 0)&&(optFilePath[sLen-1] == *GetFilePathSeparator()))
   {
      // Special case for paths that end in a slash:  we want to ignore the slash (otherwise we can't see files at this location, only folders)
      char * temp = newnothrow_array(char, sLen);  // not plus 1, because we're going to put the NUL where the trailing slash was
      if (temp)
      {
         memcpy(temp, optFilePath, sLen-1);
         temp[sLen-1] = '\0';
         SetFilePath(temp);
         delete [] temp;
         return;
      }
      else WARN_OUT_OF_MEMORY;
   }

   Reset();

   if (optFilePath)
   {
#ifdef WIN32
      // FILE_FLAG_BACKUP_SEMANTICS is necessary or CreateFile() will
      // fail when trying to open a directory.
      HANDLE hFile = CreateFileA(optFilePath, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
      if (hFile != INVALID_HANDLE_VALUE)
      {
         _flags |= (1L<<FPI_FLAG_EXISTS);

         BY_HANDLE_FILE_INFORMATION info;
         if (GetFileInformationByHandle(hFile, &info))
         {
            _flags |= (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? (1L<<FPI_FLAG_ISDIRECTORY) : (1L<<FPI_FLAG_ISREGULARFILE);
            _atime = InternalizeFileTime(info.ftLastAccessTime);
            _ctime = InternalizeFileTime(info.ftCreationTime);
            _mtime = InternalizeFileTime(info.ftLastWriteTime);
            _size  = (((uint64)info.nFileSizeHigh)<<32)|((uint64)info.nFileSizeLow);
         }
         CloseHandle(hFile);
      }
#else
# if defined(MUSCLE_64_BIT_PLATFORM) && !defined(__APPLE__)
      struct stat64 statInfo;
      if (stat64(optFilePath, &statInfo) == 0)
# else
      struct stat statInfo;
      if (stat(optFilePath, &statInfo) == 0)
# endif
      {
         _flags |= (1L<<FPI_FLAG_EXISTS);
         if (S_ISDIR(statInfo.st_mode)) _flags |= (1L<<FPI_FLAG_ISDIRECTORY);
         if (S_ISREG(statInfo.st_mode)) _flags |= (1L<<FPI_FLAG_ISREGULARFILE);

         _size = statInfo.st_size;
# if defined(MUSCLE_64_BIT_PLATFORM) && !defined(_POSIX_SOURCE)
         _atime = InternalizeTimeSpec(statInfo.st_atimespec);
#  if !defined(__APPLE__) || (__DARWIN_64_BIT_INO_T == 1)
         _ctime = InternalizeTimeSpec(statInfo.st_birthtimespec);
#else
         _ctime = InternalizeTimeT(statInfo.st_ctime);  // fallback for older Apple APIs that didn't have this
#endif
         _mtime = InternalizeTimeSpec(statInfo.st_mtimespec);
# else
         _atime = InternalizeTimeT(statInfo.st_atime);
         _ctime = InternalizeTimeT(statInfo.st_ctime);
         _mtime = InternalizeTimeT(statInfo.st_mtime);
# endif

         struct stat lstatInfo;
         if ((lstat(optFilePath, &lstatInfo)==0)&&(S_ISLNK(lstatInfo.st_mode))) _flags |= (1L<<FPI_FLAG_ISSYMLINK);
      }
#endif
   }
}

void FilePathInfo :: Reset()
{
   _flags = 0;
   _size  = 0;
   _atime = 0;
   _ctime = 0;
   _mtime = 0;
}

#ifdef WIN32
uint64 FilePathInfo :: InternalizeFileTime(const FILETIME & ft) const
{
   // subtract (1970-1601) to convert from Windows time base, in 100ns units
   const uint64 diffTime = ((uint64)116444736)*NANOS_PER_SECOND;
   uint64 wft = (((uint64)ft.dwHighDateTime)<<32)|((uint64)ft.dwLowDateTime);
   if (wft <= diffTime) return 0;
   return ((wft-diffTime)/10);  // convert to MUSCLE-style microseconds
}
#endif

}; // end namespace muscle
