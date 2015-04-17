/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(_WIN32) || defined(WIN32)
# include <errno.h>
# include <io.h>
#else
# include <dirent.h>
# include <sys/stat.h>  // needed for chmod codes under MacOS/X
#endif

#include "system/SystemInfo.h"  // for GetFilePathSeparator()
#include "util/Directory.h"

namespace muscle {

#ifdef WIN32

/* Windows implementation of dirent.h Copyright Kevlin Henney, 1997, 2003. All rights reserved.

    Permission to use, copy, modify, and distribute this software and its
    documentation for any purpose is hereby granted without fee, provided
    that this copyright and permissions notice appear in all copies and
    derivatives.

    This software is supplied "as is" without express or implied warranty.

    But that said, if there are any problems please get in touch.
*/

struct dirent
{
   char * d_name;
};

typedef struct _DIR
{
   long                handle; /* -1 for failed rewind */
   struct _finddata_t  info;
   struct dirent       result; /* d_name null iff first time */
   char                *name;  /* null-terminated char string */
} DIR;

static DIR * opendir(const char *name)
{
   DIR * dir = NULL;
   if((name)&&(name[0]))
   {
      size_t base_length = strlen(name);
      const char *all = strchr("/\\", name[base_length - 1]) ? "*" : "/*";
      size_t nameLen = base_length + strlen(all) + 1;
      if (((dir = (DIR *)malloc(sizeof *dir)) != NULL) && ((dir->name = (char *) malloc(nameLen)) != NULL))
      {
         muscleStrncpy(dir->name, name, nameLen);
         muscleStrncpy(dir->name+base_length, all, nameLen-base_length);
         if((dir->handle = (long) _findfirst(dir->name, &dir->info)) != -1) dir->result.d_name = NULL;
         else
         {
            free(dir->name);
            free(dir);
            dir = 0;
         }
      }
      else
      {
         free(dir);
         dir   = 0;
         errno = ENOMEM;
      }
   }
   else errno = EINVAL;
   return dir;
}

static int closedir(DIR *dir)
{
   int result = -1;
   if (dir)
   {
      if (dir->handle != -1) result = _findclose(dir->handle);
      free(dir->name);
      free(dir);
   }
   if (result == -1) errno = EBADF;
   return result;
}

static struct dirent * readdir(DIR *dir)
{
   struct dirent * result = NULL;
   if(dir && dir->handle != -1)
   {
      if((!dir->result.d_name)||(_findnext(dir->handle, &dir->info) != -1))
      {
         result         = &dir->result;
         result->d_name = dir->info.name;
      }
   }
   else errno = EBADF;
   return result;
}

static void rewinddir(DIR *dir)
{
   if((dir)&&(dir->handle != -1))
   {
      _findclose(dir->handle);
      dir->handle = (long) _findfirst(dir->name, &dir->info);
      dir->result.d_name = 0;
   }
   else errno = EBADF;
}
#endif

void Directory :: operator++(int)
{
   DIR * dir = (DIR *) _dirPtr;
   struct dirent * entry = dir ? readdir(dir) : NULL;
   _currentFileName = entry ? entry->d_name : NULL;
}

void Directory :: Rewind()
{
   if (_dirPtr) rewinddir((DIR *)_dirPtr);
   (*this)++;
}

void Directory :: Reset()
{
   if (_path)
   {
      delete [] _path;
      _path = NULL;
   }
   if (_dirPtr)
   {
      closedir((DIR *)_dirPtr);
      _dirPtr = NULL;
   }
}

bool Directory :: Exists(const char * dirPath)
{
   Directory d;
   return ((dirPath)&&(d.SetDir(dirPath) == B_NO_ERROR));
}

status_t Directory :: SetDir(const char * dirPath)
{
   Reset();
   if (dirPath)
   {
      int pathLen = (int) strlen(dirPath);
      const char * sep = GetFilePathSeparator();
      int sepLen = (int) strlen(sep);
      int extraBytes = ((pathLen<sepLen)||(strcmp(dirPath+pathLen-sepLen, sep) != 0)) ? sepLen : 0;
      int allocLen = pathLen+extraBytes+1;
      _path = newnothrow_array(char, allocLen);
      if (_path == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
      muscleStrncpy(_path, dirPath, allocLen);
      if (extraBytes != 0) muscleStrncpy(_path+pathLen, sep, allocLen-pathLen);

      _dirPtr = opendir(dirPath);
      if (_dirPtr == NULL)
      {
         Reset();  // to free and null-out _path
         return B_ERROR;
      }

      (*this)++;   // make the first entry in the directory the current entry.
      return B_NO_ERROR;
   }
   else return B_NO_ERROR;
}

status_t Directory :: MakeDirectory(const char * dirPath, bool forceCreateParentDirsIfNecessary, bool errorIfAlreadyExists)
{
   if (forceCreateParentDirsIfNecessary)
   {
      const char sep = *GetFilePathSeparator();  // technically cheating but I don't want to have to write strrstr()
      const char * lastSlash = strrchr(dirPath+((dirPath[0]==sep)?1:0), sep);
      if (lastSlash)
      {
         uint32 subLen = (uint32)(lastSlash-dirPath);
         char * temp = newnothrow_array(char, subLen+1);
         if (temp == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}

         memcpy(temp, dirPath, subLen);
         temp[subLen] = '\0';

         Directory pd(temp);
         if ((pd.IsValid() == false)&&(Directory::MakeDirectory(temp, true, false) != B_NO_ERROR))
         {
            delete [] temp;
            return B_ERROR;
         }
         else delete [] temp;
      }
   }

   // base case!
#ifdef WIN32
   return ((CreateDirectoryA(dirPath, NULL))||((errorIfAlreadyExists==false)&&(GetLastError()==ERROR_ALREADY_EXISTS))) ? B_NO_ERROR : B_ERROR;
#else
   return ((mkdir(dirPath, S_IRWXU|S_IRWXG|S_IRWXO) == 0)||((errorIfAlreadyExists==false)&&(errno==EEXIST))) ? B_NO_ERROR : B_ERROR;
#endif
}

status_t Directory :: MakeDirectoryForFile(const char * filePath)
{
   int pathLen = (int) strlen(filePath);
   char * p = newnothrow_array(char, pathLen+1);
   if (p == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}

   muscleStrncpy(p, filePath, pathLen+1);
   char * lastSep = strrchr(p, GetFilePathSeparator()[0]);
   if (lastSep) *lastSep = '\0';  // truncate the file name
   status_t ret = lastSep ? MakeDirectory(p, true) : B_NO_ERROR;  // No directory clauses?  then there's nothing for us to do.
   delete [] p;
   return ret;
}

status_t Directory :: DeleteDirectory(const char * dirPath, bool forceDeleteSubItemsIfNecessary)
{
   if (forceDeleteSubItemsIfNecessary)
   {
      Directory d;
      if ((dirPath==NULL)||(d.SetDir(dirPath) != B_NO_ERROR)) return B_ERROR;

      const char * sep = GetFilePathSeparator();
      int sepLen       = (int) strlen(sep);
      int dirPathLen   = (int) strlen(dirPath);

      // No point in including a separator if (dirPath) already ends in one
      if ((dirPathLen >= sepLen)&&(strcmp(&dirPath[dirPathLen-sepLen], sep) == 0)) {sep = ""; sepLen=0;}

      for (const char * fn; (fn = d.GetCurrentFileName()) != NULL; d++)
      {
         if ((strcmp(fn, ".") != 0)&&(strcmp(fn, "..") != 0))
         {
            int fnLen = (int) strlen(fn);
            int catLen = dirPathLen+sepLen+fnLen+1;
            char * catStr = newnothrow_array(char, catLen);
            if (catStr == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}

            // Compose the sub-item's full path
            muscleStrncpy(catStr,                   dirPath, catLen);
            muscleStrncpy(catStr+dirPathLen,        sep,     catLen-dirPathLen);
            muscleStrncpy(catStr+dirPathLen+sepLen, fn,      catLen-(dirPathLen+sepLen));

            // First, try to delete the sub-item as a file; if not, as a directory
#ifdef _MSC_VER
            int unlinkRet = _unlink(catStr);  // stupid MSVC!
#else
            int unlinkRet = unlink(catStr);
#endif
            status_t ret = (unlinkRet == 0) ? B_NO_ERROR : Directory::DeleteDirectory(catStr, true);
            delete [] catStr;
            if (ret != B_NO_ERROR) return ret;
         }
      }
   }
#ifdef WIN32
   return RemoveDirectoryA(dirPath) ? B_NO_ERROR : B_ERROR;
#else
   return (rmdir(dirPath) == 0) ? B_NO_ERROR : B_ERROR;
#endif
}

}; // end namespace muscle
