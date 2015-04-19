/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x) != B_NO_ERROR) printf("Test failed, line %i\n",__LINE__)

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

   SocketMultiplexer multiplexer;
   PlainTextMessageIOGateway gw;
   gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));
   char text[1000] = "";
   while(s())
   {
      int fd = s.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      multiplexer.RegisterSocketForReadReady(STDIN_FILENO);

      QueueGatewayMessageReceiver inQueue;
      while(s())
      {
         if (multiplexer.WaitForEvents() < 0) printf("portablereflectclient: WaitForEvents() failed!\n");
         if (multiplexer.IsSocketReadyForRead(STDIN_FILENO))
         {
            if (fgets(text, sizeof(text), stdin) == NULL) text[0] = '\0';
            char * ret = strchr(text, '\n'); if (ret) *ret = '\0';
         }

         if (text[0])
         {
            printf("Sending: [%s]\n",text);
            MessageRef msg = GetMessageFromPool();
            msg()->AddString(PR_NAME_TEXT_LINE, text);
            gw.AddOutgoingMessage(msg);
            text[0] = '\0';
         }

         bool reading = multiplexer.IsSocketReadyForRead(fd);
         bool writing = multiplexer.IsSocketReadyForWrite(fd);
         bool writeError = ((writing)&&(gw.DoOutput() < 0));
         bool readError  = ((reading)&&(gw.DoInput(inQueue) < 0));
         if ((readError)||(writeError))
         {
            printf("Connection closed, exiting.\n");
            s.Reset();
         }

         MessageRef incoming;
         while(inQueue.RemoveHead(incoming) == B_NO_ERROR)
         {
            printf("Heard message from server:-----------------------------------\n");
            const char * inStr;
            for (int i=0; (incoming()->FindString(PR_NAME_TEXT_LINE, i, &inStr) == B_NO_ERROR); i++) printf("Line %i: [%s]\n", i, inStr);

            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         multiplexer.RegisterSocketForReadReady(fd);
         if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      }
   }

   if (gw.HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((gw.HasBytesToOutput())&&(gw.DoOutput() >= 0)) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");
   return 0;
}
