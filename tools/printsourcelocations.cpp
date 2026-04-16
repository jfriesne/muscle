/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "syslog/SysLog.h"
#include "system/SetupSystem.h"
#include "system/SystemInfo.h"

using namespace muscle;

static status_t CheckFile(const String & path, Queue<String> & codes)
{
   FileDataIO dio(muscleFopen(path(), "r"));
   if (dio.GetFile())
   {
      const String fileName = path.Substring(GetFilePathSeparator());

      PlainTextMessageIOGateway gw;
      gw.SetDataIO(DummyDataIORef(dio));

      // Read in the file
      QueueGatewayMessageReceiver q;
      while(gw.DoInput(q).GetByteCount() > 0) {/* empty */}

      // Now parse the lines, and for any that have LogTime() in them, print out the line and the corresponding code key
      uint32 lineNumber = 1;
      MessageRef msg;
      while(q.RemoveHead(msg).IsOK())
      {
         const String * line;
         for (uint32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &line).IsOK(); i++)
         {
            const int32 ltIdx = line->IndexOf("LogTime(");
            if (ltIdx >= 0)
            {
               const String loc = SourceCodeLocationKeyToString(GenerateSourceCodeLocationKey(fileName(), lineNumber));
               MRETURN_ON_ERROR(codes.AddTail(line->WithPrepend(String("[%1] %2:%3: ").Arg(loc).Arg(path).Arg(lineNumber))));
            }
            lineNumber++;
         }
      }

      return B_NO_ERROR;
   }
   else return B_FILE_NOT_FOUND;
}

static status_t DoSearch(const String & path, Queue<String> & codes)
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
            const FilePathInfo fpi(subPath());

                 if (fpi.IsDirectory()) MRETURN_ON_ERROR(DoSearch(subPath, codes));
            else if (fpi.IsRegularFile())
            {
               const String iName = String(nextName).ToLowerCase();
               if ((iName.EndsWith(".c"))||(iName.EndsWith(".cpp"))||(iName.EndsWith(".h"))||(iName.EndsWith(".hpp"))||(iName.EndsWith(".cc"))) MRETURN_ON_ERROR(CheckFile(subPath, codes));
            }
         }
         d++;
      }

      return B_NO_ERROR;
   }
   else return B_FILE_NOT_FOUND;
}

// This program accepts a directory name as the argument, and will find and print all LogTime()
// calls in and underneath that directory, along with their source-code-location keys. (e.g. "FB72")
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc < 2)
   {
      printf("Usage:  printsourcelocations dirname\n");
      return 10;
   }

   Directory d(argv[1]);
   if (d.IsValid() == false)
   {
      printf("[%s] is not a valid directory path.\n", argv[1]);
      return 10;
   }

   Queue<String> codes;
   const status_t r = DoSearch(argv[1], codes);
   if (r.IsOK())
   {
      codes.Sort();
      for (uint32 i=0; i<codes.GetNumItems(); i++) printf("%s\n", codes[i]());
   }
   else LogTime(MUSCLE_LOG_ERROR, "DoSearch(%s) returned [%s]\n", argv[1], r());

   return 0;
}
