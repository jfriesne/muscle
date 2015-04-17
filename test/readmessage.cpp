/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "message/Message.h"
#include "zlib/ZLibUtilityFunctions.h"

using namespace muscle;

// A simple utility to read in a flattened Message file from disk, and print it out.
int main(int argc, char ** argv)
{
   int retVal = 0;

   CompleteSetupSystem css;

   const char * fileName = (argc > 1) ? argv[1] : "test.msg";
   FILE * fpIn = muscleFopen(fileName, "rb");
   if (fpIn)
   {
      uint32 bufSize = 100*1024; // if your message is >100KB flattened, then tough luck
      uint8 * buf = new uint8[bufSize];
      int numBytesRead = fread(buf, 1, bufSize, fpIn);
      printf("Read %i bytes from [%s]\n", numBytesRead, fileName);

      Message msg;
      if (msg.Unflatten(buf, numBytesRead) == B_NO_ERROR)
      {
         printf("Message is:\n");
         msg.PrintToStream();

         MessageRef infMsg = InflateMessage(MessageRef(&msg, false));
         if (infMsg())
         {
            printf("Inflated Message is:\n");
            infMsg()->PrintToStream();
         }
      }
      else
      {
         printf("Error unflattening message! (%i bytes read)\n", numBytesRead);
         retVal = 10;
      }
      fclose(fpIn);
      delete [] buf;
   }
   else
   {
      printf("Could not read input flattened-message file [%s]\n", fileName);
      retVal = 10;
   }

   return retVal;
}
