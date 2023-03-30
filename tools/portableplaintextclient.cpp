/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

// This is a text based test client to test the PlainTextMessageIOGateway class.
// It should be able to communicate with any server that sends and receives
// lines of ASCII text (e.g. Web servers, XML, eCommerce, etc).
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const char * hostName = "localhost";
   int port = 0;
   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 80;

   ConstSocketRef s = Connect(hostName, (uint16)port, "portableplaintextclient", false);
   if (s() == NULL) return 10;

   StdinDataIO stdinIO(false);
   QueueGatewayMessageReceiver stdinInQueue;
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));
   const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

   SocketMultiplexer multiplexer;
   PlainTextMessageIOGateway gw;
   gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));
   while(s())
   {
      const int fd = s.GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
      (void) multiplexer.RegisterSocketForReadReady(stdinFD);

      QueueGatewayMessageReceiver inQueue;
      while(s())
      {
         status_t ret;
         if (multiplexer.WaitForEvents().IsError(ret)) printf("portableplaintextclient: WaitForEvents() failed! [%s]\n", ret());
         if (multiplexer.IsSocketReadyForRead(stdinFD))
         {
            while(1)
            {
               const io_status_t bytesRead = stdinGateway.DoInput(stdinInQueue);
               if (bytesRead.IsError())
               {
                  printf("Stdin closed, exiting!\n");
                  s.Reset();  // break us out of the outer loop
                  break;
               }
               else if (bytesRead.GetByteCount() == 0) break;  // no more to read
            }
         }

         MessageRef msgFromStdin;
         while(stdinInQueue.RemoveHead(msgFromStdin).IsOK())
         {
            const String * st;
            for (int32 i=0; msgFromStdin()->FindString(PR_NAME_TEXT_LINE, i, &st).IsOK(); i++)
            {
               printf("Sending: [%s]\n", st->Cstr());

               MessageRef msg = GetMessageFromPool();
               (void) msg()->AddString(PR_NAME_TEXT_LINE, *st);
               (void) gw.AddOutgoingMessage(msg);
            }
         }

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);
         const bool writeError = ((writing)&&(gw.DoOutput().IsError()));
         const bool readError  = ((reading)&&(gw.DoInput(inQueue).IsError()));
         if ((readError)||(writeError))
         {
            printf("Connection closed, exiting.\n");
            s.Reset();
         }

         MessageRef incoming;
         while(inQueue.RemoveHead(incoming).IsOK())
         {
            printf("Heard message from server:-----------------------------------\n");
            const char * inStr;
            for (int i=0; (incoming()->FindString(PR_NAME_TEXT_LINE, i, &inStr).IsOK()); i++) printf("Line %i: [%s]\n", i, inStr);

            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         (void) multiplexer.RegisterSocketForReadReady(fd);
         if (gw.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
         (void) multiplexer.RegisterSocketForReadReady(stdinFD);
      }
   }

   if (gw.HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((gw.HasBytesToOutput())&&(gw.DoOutput().IsOK())) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");
   return 0;
}
