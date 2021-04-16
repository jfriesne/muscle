/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "syslog/SysLog.h"
#include "system/SetupSystem.h"
#include "system/SystemInfo.h"

using namespace muscle;

static void CheckFile(const String & path, uint32 code)
{
   FileDataIO dio(muscleFopen(path(), "r"));
   if (dio.GetFile())
   {
      String fileName = path.Substring(GetFilePathSeparator());

      PlainTextMessageIOGateway gw;
      gw.SetDataIO(DataIORef(&dio, false));

      // Read in the file
      QueueGatewayMessageReceiver q;
      while(gw.DoInput(q) > 0) {/* empty */}

      // Now parse the lines, and see if any match
      uint32 lineNumber = 1;
      MessageRef msg;
      while(q.RemoveHead(msg).IsOK())
      {
         const String * line;
         for (uint32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &line).IsOK(); i++)
         {
            if (GenerateSourceCodeLocationKey(fileName(), lineNumber) == code) printf("%s:" UINT32_FORMAT_SPEC ": %s\n", path(), lineNumber, line->Cstr());
            lineNumber++;
         }
      }
   }
}

static void DoSearch(const String & path, uint32 code)
{
   Directory d(path());
   if (d.IsValid())
   {
      const char * nextName;
      while((nextName = d.GetCurrentFileName()) != NULL)
      {
         if (nextName[0] != '.')
         {
            const String subPath = path + GetFilePathSeparator() + nextName;
            FilePathInfo fpi(subPath());
                 if (fpi.IsDirectory()) DoSearch(subPath, code);
            else if (fpi.IsRegularFile())
            {
               String iName = nextName; iName = iName.ToLowerCase();
               if ((iName.EndsWith(".c"))||(iName.EndsWith(".cpp"))||(iName.EndsWith(".h"))||(iName.EndsWith(".hpp"))||(iName.EndsWith(".cc"))) CheckFile(subPath, code);
            }
         }
         d++;
      }
   }
}

// This program accepts a source-code-location key (e.g. "FB72", as shown in MUSCLE Log messages
// when -DMUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME is defined) and iterates over all .c, .cpp, and .h
// files in or under the specified directory and prints out any lines that match the code.
int main(int argc, char ** argv) 
{
   CompleteSetupSystem css;

   if (argc <  3)
   {
      printf("Usage:  findsourcelocations code dirname\n");
      return 10;
   }

   uint32 code = SourceCodeLocationKeyFromString(argv[1]);
   if (code == 0)
   {
      printf("Invalid source location code [%s]\n", argv[1]);
      return 10;
   }

   Directory d(argv[2]);
   if (d.IsValid() == false)
   {
      printf("[%s] is not a valid directory path.\n", argv[2]);
      return 10;
   }

   DoSearch(argv[2], code);
   return 0;
}
