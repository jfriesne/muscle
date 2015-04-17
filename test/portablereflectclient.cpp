/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_SSL
# include "dataio/SSLSocketDataIO.h"
# include "iogateway/SSLSocketAdapterGateway.h"
#endif

#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/QueryFilter.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x) != B_NO_ERROR) printf("Test failed, line %i\n",__LINE__)

// This is a text based test client for the muscled server.  It is useful for testing
// the server, and could possibly be useful for other things, I don't know.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   String hostName;
   uint16 port = 2960;
   if (argc > 1) ParseConnectArg(argv[1], hostName, port, false);
   ConstSocketRef sock = Connect(hostName(), port, "portablereflectclient", false);
   if (sock() == NULL) return 10;

   // We'll receive plain text over stdin
   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DataIORef(&stdinIO, false));

   // And send and receive flattened Message objects over our TCP socket
   TCPSocketDataIO tcpIO(sock, false);
   MessageIOGateway tcpGateway;
   tcpGateway.SetDataIO(DataIORef(&tcpIO, false));

   DataIORef networkIORef(&tcpIO, false);
   AbstractMessageIOGatewayRef gatewayRef(&tcpGateway, false);

#ifdef MUSCLE_ENABLE_SSL
   for (int i=1; i<argc; i++)
   {
      const char * a = argv[i];
      if (strncmp(a, "publickey=", 10) == 0)
      {
         a += 10;  // skip past the 'publickey=' part
         SSLSocketDataIO * sslIO = new SSLSocketDataIO(sock, false, false);
         DataIORef sslIORef(sslIO);
         if (sslIO->SetPublicKeyCertificate(a) == B_NO_ERROR)
         {
            LogTime(MUSCLE_LOG_INFO, "Using public key certificate file [%s] to connect to server\n", a);
            networkIORef = sslIORef;
            gatewayRef.SetRef(new SSLSocketAdapterGateway(gatewayRef));
            gatewayRef()->SetDataIO(networkIORef);
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't load public key certificate file [%s] (file not found?)\n", a);
            return 10;
         }
      }
   }
#endif

   SocketMultiplexer multiplexer;
   QueueGatewayMessageReceiver stdinInQueue, tcpInQueue;
   bool keepGoing = true;
   uint64 nextTimeoutTime = MUSCLE_TIME_NEVER;
   while(keepGoing)
   {
      int stdinFD       = stdinIO.GetReadSelectSocket().GetFileDescriptor();
      int socketReadFD  = networkIORef()->GetReadSelectSocket().GetFileDescriptor();
      int socketWriteFD = networkIORef()->GetWriteSelectSocket().GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(stdinFD);
      multiplexer.RegisterSocketForReadReady(socketReadFD);
      if (gatewayRef()->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(socketWriteFD);
      if (multiplexer.WaitForEvents(nextTimeoutTime) < 0) printf("portablereflectclient: WaitForEvents() failed!\n");

      uint64 now = GetRunTime64();
      if (now >= nextTimeoutTime)
      {
         // For OpenSSL testing:  Generate some traffic to the server every 50mS
         printf("Uploading timed OpenSSL-tester update at time " UINT64_FORMAT_SPEC "\n", now);
         MessageRef stateMsg = GetMessageFromPool();
         stateMsg()->AddString("username", "portablereflectclient");
         stateMsg()->AddPoint("position", Point((rand()%100)/100.0f, (rand()%100)/100.0f));
         stateMsg()->AddInt32("color", -1);

         MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
         uploadMsg()->AddMessage("qt_example/state", stateMsg);
         gatewayRef()->AddOutgoingMessage(uploadMsg);

         nextTimeoutTime = now + MillisToMicros(50);
      }

      // Receive data from stdin
      if (multiplexer.IsSocketReadyForRead(stdinFD))
      {
         while(1)
         {
            int32 bytesRead = stdinGateway.DoInput(stdinInQueue);
            if (bytesRead < 0)
            {
               printf("Stdin closed, exiting!\n");
               keepGoing = false;
               break;
            }
            else if (bytesRead == 0) break;  // no more to read
         }
      }

      // Handle any input lines that were received from stdin
      MessageRef msgFromStdin;
      while(stdinInQueue.RemoveHead(msgFromStdin) == B_NO_ERROR)
      {
         const String * st;
         for (int32 i=0; msgFromStdin()->FindString(PR_NAME_TEXT_LINE, i, &st) == B_NO_ERROR; i++)
         {
            const char * text = st->Cstr();

            printf("You typed: [%s]\n", text);
            bool send = true;
            MessageRef ref = GetMessageFromPool();

            switch(text[0])
            {
               case 'm':
                  ref()->what = MAKETYPE("umsg");
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
                  ref()->AddString("info", "This is a user message");
               break;

               case 'i':
                  ref()->what = PR_COMMAND_PING;
                  ref()->AddString("Test ping", "yeah");
               break;

               case 's':
               {
                  ref()->what = PR_COMMAND_SETDATA;
                  MessageRef uploadMsg = GetMessageFromPool(MAKETYPE("HELO"));
                  uploadMsg()->AddString("This node was posted at: ", GetHumanReadableTimeString(GetRunTime64()));
                  ref()->AddMessage(&text[2], uploadMsg);
               }
               break;

               case 'k':
                  ref()->what = PR_COMMAND_KICK;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 'b':
                  ref()->what = PR_COMMAND_ADDBANS;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 'B':
                  ref()->what = PR_COMMAND_REMOVEBANS;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 'g':
                  ref()->what = PR_COMMAND_GETDATA;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 'G':
                  ref()->what = PR_COMMAND_GETDATATREES;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
                  ref()->AddString(PR_NAME_TREE_REQUEST_ID, "Tree ID!");
               break;

               case 'q':
                  keepGoing = send = false;
               break;

               case 'p':
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  ref()->AddString(&text[2], "");
               break;

               case 'P':
                  ref()->what = PR_COMMAND_GETPARAMETERS;
               break;

               case 'L':
               {
                  // simulate the behavior of qt_example, for testing OpenSSL problem
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  ref()->AddBool("SUBSCRIBE:qt_example/state", true);
                  printf("Starting OpenSSL problem test...\n");
                  nextTimeoutTime = 0;
               }
               break;

               case 'x':
               {
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  StringQueryFilter sqf("sc_tstr", StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH, "*Output*");
                  ref()->AddArchiveMessage("SUBSCRIBE:/*/*/csproj/default/subcues/*", sqf);
               }
               break;

               case 'd':
                  ref()->what = PR_COMMAND_REMOVEDATA;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 'D':
                  ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                  ref()->AddString(PR_NAME_KEYS, &text[2]);
               break;

               case 't':
               {
                  // test all data types
                  ref()->what = 1234;
                  ref()->AddString("String", "this is a string");
                  ref()->AddInt8("Int8", 123);
                  ref()->AddInt8("-Int8", -123);
                  ref()->AddInt16("Int16", 1234);
                  ref()->AddInt16("-Int16", -1234);
                  ref()->AddInt32("Int32", 12345);
                  ref()->AddInt32("-Int32", -12345);
                  ref()->AddInt64("Int64", 123456789);
                  ref()->AddInt64("-Int64", -123456789);
                  ref()->AddBool("Bool", true);
                  ref()->AddBool("-Bool", false);
                  ref()->AddFloat("Float", 1234.56789f);
                  ref()->AddFloat("-Float", -1234.56789f);
                  ref()->AddDouble("Double", 1234.56789);
                  ref()->AddDouble("-Double", -1234.56789);
                  ref()->AddPointer("Pointer", ref());
                  ref()->AddFlat("Flat", *ref());
                  char data[] = "This is some data";
                  ref()->AddData("Flat", B_RAW_TYPE, data, sizeof(data));
               }
               break;

               default:
                  printf("Sorry, wot?\n");
                  send = false;
               break;
            }

            if (send)
            {
               printf("Sending message...\n");
               ref()->PrintToStream();
               gatewayRef()->AddOutgoingMessage(ref);
            }
         }
      }

      // Handle input and output on the TCP socket
      bool reading = multiplexer.IsSocketReadyForRead(socketReadFD);
      bool writing = multiplexer.IsSocketReadyForWrite(socketWriteFD);
      bool writeError = ((writing)&&(gatewayRef()->DoOutput() < 0));
      bool readError  = ((reading)&&(gatewayRef()->DoInput(tcpInQueue) < 0));
      if ((readError)||(writeError))
      {
         printf("Connection closed (%s), exiting.\n", writeError?"Write Error":"Read Error");
         keepGoing = false;
      }

      MessageRef msgFromTCP;
      while(tcpInQueue.RemoveHead(msgFromTCP) == B_NO_ERROR)
      {
         printf("Heard message from server:-----------------------------------\n");
         msgFromTCP()->PrintToStream();
         printf("-------------------------------------------------------------\n");
      }
   }

   if (gatewayRef()->HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((gatewayRef()->HasBytesToOutput())&&(gatewayRef()->DoOutput() >= 0)) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");

   return 0;
}
