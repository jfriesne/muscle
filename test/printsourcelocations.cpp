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

static void CheckFile(const String & path, Queue<String> & codes)
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
               const int32 commentIdx = line->IndexOf("//");   // don't include LogTime() calls that are commented out
               if ((commentIdx < 0)||(commentIdx > ltIdx)) 
               {
                  const String loc = SourceCodeLocationKeyToString(GenerateSourceCodeLocationKey(fileName(), lineNumber));
                  codes.AddTail(line->Prepend(String("[%1] %2:%3: ").Arg(loc).Arg(path).Arg(lineNumber)));
               }
            }
            lineNumber++;
         }
      }
   }
}

static void DoSearch(const String & path, Queue<String> & codes)
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
                 if (fpi.IsDirectory()) DoSearch(subPath, codes);
            else if (fpi.IsRegularFile())
            {
               String iName = nextName; iName = iName.ToLowerCase();
               if ((iName.EndsWith(".c"))||(iName.EndsWith(".cpp"))||(iName.EndsWith(".h"))||(iName.EndsWith(".hpp"))||(iName.EndsWith(".cc"))) CheckFile(subPath, codes);
            }
         }
         d++;
      }
   }
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
   DoSearch(argv[1], codes);
   codes.Sort();
   for (uint32 i=0; i<codes.GetNumItems(); i++) printf("%s\n", codes[i]());

   return 0;
}
