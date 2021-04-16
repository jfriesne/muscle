/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "syslog/SysLog.h"
#include "util/String.h"
#include "util/FilePathInfo.h"

using namespace muscle;

static const char * GetBoolString(bool b) {return b ? "YES" : "NO";}

// This program exercises the FilePathInfo class.
int main(int argc, char ** argv) 
{
   if (argc < 2)
   {
      printf("Usage:  testfilepathinfo <filepath>\n"); 
      return 10;
   }

   FilePathInfo fpi(argv[1]);

   printf("FilePathInfo for [%s]:\n", argv[1]);
   printf("Exists:\t\t\t%s\n", GetBoolString(fpi.Exists()));
   printf("IsRegularFile:\t\t%s\n", GetBoolString(fpi.IsRegularFile()));
   printf("IsDirectory:\t\t%s\n", GetBoolString(fpi.IsDirectory()));
   printf("IsSymLink:\t\t%s\n", GetBoolString(fpi.IsSymLink()));
   printf("FileSize (bytes):\t" UINT64_FORMAT_SPEC "\n", fpi.GetFileSize());
   printf("AccessTime:\t\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetAccessTime(), GetHumanReadableTimeString(fpi.GetAccessTime())());
   printf("ModificationTime:\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetModificationTime(), GetHumanReadableTimeString(fpi.GetModificationTime())());
   printf("CreationTime:\t\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetCreationTime(), GetHumanReadableTimeString(fpi.GetCreationTime())());
   return 0;
}
