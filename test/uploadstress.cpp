/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

// This client just uploads a bunch of stuff to the server, trying to batter it down.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const char * hostName = "localhost";
   int port = 0;
   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 2960;

   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   while(true)
   {
      uint32 bufCount=0;
      ConstSocketRef s = Connect(hostName, (uint16)port, "uploadstress");
      if (s() == NULL) return 10;

      MessageIOGateway gw;
      gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));
      while(true)
      {
         const int fd = s.GetFileDescriptor();
         multiplexer.RegisterSocketForReadReady(fd);
         multiplexer.RegisterSocketForWriteReady(fd);

         if (multiplexer.WaitForEvents() < 0) printf("uploadstress: WaitForEvents() failed!\n");

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);

         if (gw.HasBytesToOutput() == false)
         {
            char buf[128];
            muscleSprintf(buf, UINT32_FORMAT_SPEC, bufCount++);
            printf("Adding message [%s]\n", buf);

            MessageRef smsg = GetMessageFromPool(PR_COMMAND_SETDATA);
            Message data(1234);
            data.AddString("nerf", "boy!");
            smsg()->AddMessage(buf, data);
            gw.AddOutgoingMessage(smsg);
         }

         const bool writeError = ((writing)&&(gw.DoOutput() < 0));
         const bool readError  = ((reading)&&(gw.DoInput(inQueue) < 0));
         if ((readError)||(writeError))
         {
            printf("Connection closed, exiting.\n");
            break;
         }

         MessageRef incoming;
         while(inQueue.RemoveHead(incoming).IsOK())
         {
            printf("Heard message from server:-----------------------------------\n");
            incoming()->PrintToStream();
            printf("-------------------------------------------------------------\n");
         }
      }
   }

   printf("\n\nBye!\n");
   return 0;
}
