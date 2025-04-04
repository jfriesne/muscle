/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "iogateway/MessageIOGateway.h"
#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"
#include "zlib/ZLibDataIO.h"

using namespace muscle;

#define TEST(x) {if ((x).IsError()) {printf("Test failed, line %i\n",__LINE__); return 10;}}

DataIORef GetFileRef(FILE * file)
{
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   return DataIORef(new ZLibDataIO(DataIORef(new FileDataIO(file))));   // enable transparent file compression!
#else
   return DataIORef(new FileDataIO(file));
#endif
}

// This program tests the functionality of the MessageIOGateway by writing a Message
// out to a file, then reading it back in.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   QueueGatewayMessageReceiver inQueue;
   if ((argc<2)||(strcmp(argv[1], "fromscript")==0))
   {
      FILE * f = muscleFopen("test.dat", "wb");
      if (f)
      {
         printf("Outputting test messages to test.dat...\n");
         MessageIOGateway g;
         g.SetDataIO(GetFileRef(f));
         for (int i=0; i<100; i++)
         {
            MessageRef m = GetMessageFromPool(MakeWhatCode("TeSt"));
            TEST(m()->AddString("Jo", "Mama"));
            TEST(m()->AddInt32("Age", 90+i));
            TEST(m()->AddBool("Ugly", (i%2)!=0));
            TEST(g.AddOutgoingMessage(m));
         }
         while(g.HasBytesToOutput()) TEST(g.DoOutput());
         //g.GetDataIO()()->FlushOutput();
         printf("Done Writing!\n");
      }
      else {printf("Error, could not open test file!\n"); return 10;}

      // Now try to read it back in
      FILE * in = muscleFopen("test.dat", "rb");
      if (in)
      {
         printf("Reading test messages from test.dat...\n");
         MessageIOGateway g;
         g.SetDataIO(GetFileRef(in));
         while(g.DoInput(inQueue).IsOK())
         {
            MessageRef msgRef;
            while(inQueue.RemoveHead(msgRef).IsOK()) msgRef()->Print(stdout);
         }

         printf("Done Reading!\n");
      }
      else {printf("Error, could not re-open test file!\n"); return 10;}
   }
   else if (argc > 1)
   {
      for (int i=1; i<argc; i++)
      {
         FILE * in = muscleFopen(argv[i], "rb");
         if (in)
         {
            printf("Reading message file %s...\n", argv[i]);
            MessageIOGateway g;
            g.SetDataIO(GetFileRef(in));
            int32 readBytes;
            while((readBytes = g.DoInput(inQueue).GetByteCount()) >= 0)
            {
               printf("Read " UINT32_FORMAT_SPEC " bytes...\n", readBytes);
               MessageRef msgRef;
               while(inQueue.RemoveHead(msgRef).IsOK()) msgRef()->Print(stdout);
            }
            printf("Done Reading file [%s]!\n", argv[i]);
         }
         else {printf("Error, could not open file [%s]!\n", argv[i]); return 10;}
      }
   }
   return 0;
}
