/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

// This program exercises the ParseFile() function.
int main(int argc, char ** argv) 
{
   if (argc < 2) 
   {
      printf("Usage:  parsefile <filename> [filename] [...]\n");
      return 5;
   }
   else
   {
      status_t ret;

      for (int i=1; i<argc; i++)
      {
         printf("TESTING ParseFile() with a FILE pointer for file [%s]\n", argv[i]);
         {
            FILE * fpIn = muscleFopen(argv[i], "r");
            if (fpIn)
            {
               Message msg;
               if (ParseFile(fpIn, msg).IsOK(ret))
               {
                  LogTime(MUSCLE_LOG_INFO, "Parsed contents of file [%s]:\n", argv[i]);
                  msg.PrintToStream();
                  printf("\n");

                  const String s = UnparseFile(msg);
                  printf(" UnparseFile(msg) output is below: -------------\n[%s]", s());
               }
               else LogTime(MUSCLE_LOG_ERROR, "Error parsing file [%s] [%s]\n", argv[i], ret());
               fclose(fpIn);
            }
            else LogTime(MUSCLE_LOG_ERROR, "Unable to open file [%s] [%s]\n", argv[i], B_ERRNO());
         }

         printf("\n\nTESTING ParseFile() with a String for file [%s]\n", argv[i]);
         {
            FILE * fpIn = muscleFopen(argv[i], "r");
            if (fpIn)
            {
               FileDataIO fdio(fpIn);
               const int64 fileLen = fdio.GetLength();
               ByteBuffer bb;
               if ((fileLen >= 0)&&(bb.SetNumBytes((uint32)fileLen, false).IsOK())&&(fdio.ReadFully(bb.GetBuffer(), fileLen) == fileLen))
               {
                  const String s((const char *) bb.GetBuffer(), bb.GetNumBytes());
                  Message msg;
                  if (ParseFile(s, msg).IsOK(ret))
                  {
                     LogTime(MUSCLE_LOG_INFO, "Parsed contents of file [%s]:\n", argv[i]);
                     msg.PrintToStream();
                     printf("\n");

                     const String ss = UnparseFile(msg);
                     printf(" UnparseFile(msg) output is below: -------------\n[%s]", ss());
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "Error parsing file [%s] [%s]\n", argv[i], ret());
               }
               else LogTime(MUSCLE_LOG_ERROR, "Unable to read file [%s]\n", argv[i]);
            }
            else LogTime(MUSCLE_LOG_ERROR, "Unable to open file [%s] [%s]\n", argv[i], B_ERRNO());
         }
      }
      return 0;
   }
}
