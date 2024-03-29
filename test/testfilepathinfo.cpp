/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "syslog/SysLog.h"
#include "util/String.h"
#include "util/FilePathInfo.h"

using namespace muscle;

static const char * GetBoolString(bool b) {return b ? "YES" : "NO";}

static void PrintFilePathInfo(const char * inFileName)
{
   FilePathInfo fpi(inFileName);
   printf("\n");
   printf("FilePathInfo for [%s]:\n", inFileName);
   printf("Exists:\t\t\t%s\n", GetBoolString(fpi.Exists()));
   printf("IsRegularFile:\t\t%s\n", GetBoolString(fpi.IsRegularFile()));
   printf("IsDirectory:\t\t%s\n", GetBoolString(fpi.IsDirectory()));
   printf("IsSymLink:\t\t%s\n", GetBoolString(fpi.IsSymLink()));
   printf("FileSize (bytes):\t" UINT64_FORMAT_SPEC "\n", fpi.GetFileSize());
   printf("AccessTime:\t\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetAccessTime(), GetHumanReadableTimeString(fpi.GetAccessTime())());
   printf("ModificationTime:\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetModificationTime(), GetHumanReadableTimeString(fpi.GetModificationTime())());
   printf("CreationTime:\t\t" UINT64_FORMAT_SPEC " (%s)\n", fpi.GetCreationTime(), GetHumanReadableTimeString(fpi.GetCreationTime())());
   printf("HardLinkCount:\t\t" UINT32_FORMAT_SPEC "\n", fpi.GetHardLinkCount());
}

// This program exercises the FilePathInfo class.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const char * inFileName = ((argc<2)||(strcmp(argv[1], "fromscript")==0)) ? argv[0] : argv[1];

   PrintFilePathInfo(inFileName);
   for (int i=2; i<argc; i++) PrintFilePathInfo(argv[i]);

   return 0;
}
