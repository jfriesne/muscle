/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "zlib/ZipFileUtilityFunctions.h"

using namespace muscle;

#define COMMAND_HELLO   0x1234
#define COMMAND_GOODBYE 0x4321

// This program tests the ZipFileUtilityFunctions functions
int main(int argc, char ** argv)
{
   if (argc <= 1)
   {
      printf("testzip somezipfiletoread.zip [newzipfiletowrite.zip] [namesonly]\n");
      return 0;
   }

   bool loadData = true;
   for (int i=1; i<argc; i++)
   {
      if (strcmp(argv[i], "namesonly") == 0)
      {
         loadData = false;
         for (int j=i; j<argc; j++) argv[j] = argv[j+1];
         argc--;
         break;
      }
   }
   
   MessageRef msg = ReadZipFile(argv[1], loadData);
   if (msg())
   {
      printf("Contents of [%s] as a Message are:\n", argv[1]);
      msg()->PrintToStream();

      if (argc > 2)
      {
         if (loadData)
         {
            printf("\n\n... writing new .zip file [%s]\n", argv[2]);
            if (WriteZipFile(argv[2], *msg()).IsOK()) printf("Creation of [%s] succeeded!\n", argv[2]);
                                                 else printf("Creation of [%s] FAILED!\n", argv[2]);
         }
         else printf("There's no point in writing output file [%s], since I never loaded the .zip data anyway.\n", argv[2]);
      }
   }
   else printf("Error reading .zip file [%s]\n", argv[1]);
   
   return 0;
}
