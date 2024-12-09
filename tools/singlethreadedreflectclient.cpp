/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#ifdef MUSCLE_ENABLE_SSL
# include "dataio/SSLSocketDataIO.h"
# include "iogateway/SSLSocketAdapterGateway.h"
#endif

#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
# include "iogateway/TemplatingMessageIOGateway.h"
#else
# include "iogateway/MessageIOGateway.h"
#endif
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/QueryFilter.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

// This is a text based test client for the muscled server.  It is useful for testing
// the server, and could possibly be useful for other things, I don't know.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   String hostName = "localhost";
   uint16 port = 2960;
   if (argc > 1) (void) ParseConnectArg(argv[1], hostName, port, false);
   ConstSocketRef sock = Connect(hostName(), port, "singlethreadedreflectclient", false);
   if (sock() == NULL) return 10;

   // We'll receive plain text over stdin
   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));

   // And send and receive flattened Message objects over our TCP socket
   TCPSocketDataIO tcpIO(sock, false);
#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
   TemplatingMessageIOGateway tcpGateway;
#else
   MessageIOGateway tcpGateway;
#endif
   tcpGateway.SetDataIO(DummyDataIORef(tcpIO));

   DummyDataIORef networkIORef(tcpIO);
   DummyAbstractMessageIOGatewayRef gatewayRef(tcpGateway);

#ifdef MUSCLE_ENABLE_SSL
   const char * publicKeyPath  = NULL;
   const char * privateKeyPath = NULL;

   for (int i=1; i<argc; i++)
   {
      const char * a = argv[i];
           if (strncmp(a, "publickey=",  10) == 0) publicKeyPath  = a+10;
      else if (strncmp(a, "privatekey=", 11) == 0) privateKeyPath = a+11;
   }

   if ((privateKeyPath)&&(publicKeyPath == NULL)) publicKeyPath = privateKeyPath;  // grab public key from private-key-file
   if ((publicKeyPath)||(privateKeyPath))
   {
      SSLSocketDataIORef sslIORef(new SSLSocketDataIO(sock, false, false));
      if (publicKeyPath)
      {
         status_t ret;
         if (sslIORef()->SetPublicKeyCertificate(publicKeyPath).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Using public key certificate file [%s] to connect to server\n", publicKeyPath);
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't load public key certificate file [%s] [%s]\n", publicKeyPath, ret());
            return 10;
         }
      }
      if (privateKeyPath)
      {
         status_t ret;
         if (sslIORef()->SetPrivateKey(privateKeyPath).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Using private key file [%s] to authenticate client with server\n", privateKeyPath);
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't load private key file [%s] [%s]\n", privateKeyPath, ret());
            return 10;
         }
      }

      networkIORef = sslIORef;
      gatewayRef.SetRef(new SSLSocketAdapterGateway(gatewayRef));
      gatewayRef()->SetDataIO(networkIORef);
   }
#endif

   SocketMultiplexer multiplexer;
   QueueGatewayMessageReceiver stdinInQueue, tcpInQueue;
   bool keepGoing = true;
   uint64 nextTimeoutTime = MUSCLE_TIME_NEVER;
   while(keepGoing)
   {
      const int stdinFD       = stdinIO.GetReadSelectSocket().GetFileDescriptor();
      const int socketReadFD  = networkIORef()->GetReadSelectSocket().GetFileDescriptor();
      const int socketWriteFD = networkIORef()->GetWriteSelectSocket().GetFileDescriptor();

      (void) multiplexer.RegisterSocketForReadReady(stdinFD);
      (void) multiplexer.RegisterSocketForReadReady(socketReadFD);
      if (gatewayRef()->HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(socketWriteFD);

      status_t ret;
      if (multiplexer.WaitForEvents(nextTimeoutTime).IsError(ret)) printf("singlethreadedreflectclient: WaitForEvents() failed! [%s]\n", ret());

      const uint64 now = GetRunTime64();
      if (now >= nextTimeoutTime)
      {
         // For OpenSSL testing:  Generate some traffic to the server every 50mS
         printf("Uploading timed OpenSSL-tester update at time " UINT64_FORMAT_SPEC "\n", now);
         MessageRef stateMsg = GetMessageFromPool();
         (void) stateMsg()->AddString("username", "singlethreadedreflectclient");
         (void) stateMsg()->AddPoint("position", Point((rand()%100)/100.0f, (rand()%100)/100.0f));  // coverity[dont_call] -- don't care, not security-related
         (void) stateMsg()->AddInt32("color", -1);

         MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
         (void) uploadMsg()->AddMessage("qt_example/state", stateMsg);
         (void) gatewayRef()->AddOutgoingMessage(uploadMsg);

         nextTimeoutTime = now + MillisToMicros(50);
      }

      // Receive data from stdin
      if (multiplexer.IsSocketReadyForRead(stdinFD))
      {
         while(1)
         {
            const io_status_t bytesRead = stdinGateway.DoInput(stdinInQueue);
            if (bytesRead.IsError())
            {
               printf("Stdin closed, exiting!\n");
               keepGoing = false;
               break;
            }
            else if (bytesRead.GetByteCount() == 0) break;  // no more to read
         }
      }

      // Handle any input lines that were received from stdin
      MessageRef msgFromStdin;
      while(stdinInQueue.RemoveHead(msgFromStdin).IsOK())
      {
         const String * st;
         for (int32 i=0; msgFromStdin()->FindString(PR_NAME_TEXT_LINE, i, &st).IsOK(); i++)
         {
            const char * text = st->Cstr();

            printf("You typed: [%s]\n", text);
            bool send = true;
            MessageRef ref = GetMessageFromPool();

            const char * arg1 = (st->Length()>2) ? &text[2] : NULL;
            switch(text[0])
            {
               case 'm':
                  ref()->what = MakeWhatCode("umsg");
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
                  (void) ref()->AddString("info", "This is a user message");
               break;

               case 'i':
                  ref()->what = PR_COMMAND_PING;
                  (void) ref()->AddString("Test ping", "yeah");
               break;

               case 's':
               {
                  ref()->what = PR_COMMAND_SETDATA;
                  MessageRef uploadMsg = GetMessageFromPool(MakeWhatCode("HELO"));
                  (void) uploadMsg()->AddString("This node was posted at: ", GetHumanReadableTimeString(GetRunTime64()));
                  if (arg1) (void) ref()->AddMessage(arg1, uploadMsg);
               }
               break;

               case 'e':
               {
                  // test the uploading of a node with an "evil" (i.e. abusively long) node-path.
                  // Expected behavior is that the server will stop at a path-depth of 100, and print
                  // an error message to its stdout.
                  String evilPath = "EVIL";
                  for (int i=0; i<500; i++) evilPath += String("/DEEPER_%1").Arg(i);

                  ref()->what = PR_COMMAND_SETDATA;
                  (void) ref()->AddMessage(evilPath(), GetMessageFromPool(MakeWhatCode("EVIL")));
               }
               break;

               case 'c': case 'C':
               {
                  // Set the same node multiple times in rapid succession,
                  // to test the results of the SETDATANODE_FLAG_ENABLESUPERCEDE flag
                  const bool enableSupercede = (text[0] == 'C');

                  for (int j=0; j<10; j++)
                  {
                     ref = GetMessageFromPool(PR_COMMAND_SETDATA);
                     if (enableSupercede) (void) ref()->AddFlat(PR_NAME_FLAGS, SetDataNodeFlags(SETDATANODE_FLAG_ENABLESUPERCEDE));

                     MessageRef subMsg = GetMessageFromPool();
                     (void) subMsg()->AddInt32(String("%1 counter").Arg(enableSupercede?"Supercede":"Normal"), j);
                     (void) ref()->AddMessage("test_node", subMsg);

                     (void) gatewayRef()->AddOutgoingMessage(ref);
                  }

                  ref = GetMessageFromPool(PR_COMMAND_PING);  // just so we can see when it's done
               }
               break;

               case 'K':
               {
                  const uint32 keepAliveSeconds = arg1 ? (uint32) atol(arg1) : 0;

                  ref = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
                  if ((ref())&&(ref()->AddInt32(PR_NAME_KEEPALIVE_INTERVAL_SECONDS, keepAliveSeconds).IsOK())) LogTime(MUSCLE_LOG_INFO, "Sending PR_NAME_KEEPALIVE_INTERVAL_SECONDS=" UINT32_FORMAT_SPEC "\n", keepAliveSeconds);
               }
               break;

               case 'k':
                  ref()->what = PR_COMMAND_KICK;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'b':
                  ref()->what = PR_COMMAND_ADDBANS;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'B':
                  ref()->what = PR_COMMAND_REMOVEBANS;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'g':
                  ref()->what = PR_COMMAND_GETDATA;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'G':
                  ref()->what = PR_COMMAND_GETDATATREES;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
                  (void) ref()->AddString(PR_NAME_TREE_REQUEST_ID, "Tree ID!");
               break;

               case 'q':
                  keepGoing = send = false;
               break;

               case 'p':
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  if (arg1) (void) ref()->AddString(arg1, "");
               break;

               case 'P':
                  ref()->what = PR_COMMAND_GETPARAMETERS;
               break;

               case 'L':
               {
                  // simulate the behavior of qt_example, for testing OpenSSL problem
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  (void) ref()->AddBool("SUBSCRIBE:qt_example/state", true);
                  printf("Starting OpenSSL problem test...\n");
                  nextTimeoutTime = 0;
               }
               break;

               case 'x':
               {
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  StringQueryFilter sqf("sc_tstr", StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH, "*Output*");
                  (void) ref()->AddArchiveMessage("SUBSCRIBE:/*/*/csproj/default/subcues/*", sqf);
               }
               break;

               case 'd':
                  ref()->what = PR_COMMAND_REMOVEDATA;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'D':
                  ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                  if (arg1) (void) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 't':
               {
                  // test all data types
                  ref()->what = 1234;
                  (void) ref()->AddString("String", "this is a string");
                  (void) ref()->AddInt8("Int8", 123);
                  (void) ref()->AddInt8("-Int8", -123);
                  (void) ref()->AddInt16("Int16", 1234);
                  (void) ref()->AddInt16("-Int16", -1234);
                  (void) ref()->AddInt32("Int32", 12345);
                  (void) ref()->AddInt32("-Int32", -12345);
                  (void) ref()->AddInt64("Int64", 123456789);
                  (void) ref()->AddInt64("-Int64", -123456789);
                  (void) ref()->AddBool("Bool", true);
                  (void) ref()->AddBool("-Bool", false);
                  (void) ref()->AddFloat("Float", 1234.56789f);
                  (void) ref()->AddFloat("-Float", -1234.56789f);
                  (void) ref()->AddDouble("Double", 1234.56789);
                  (void) ref()->AddDouble("-Double", -1234.56789);
                  (void) ref()->AddPointer("Pointer", ref());
                  (void) ref()->AddFlat("Flat", *ref());
                  char data[] = "This is some data";
                  (void) ref()->AddData("Flat", B_RAW_TYPE, data, sizeof(data));
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
               (void) gatewayRef()->AddOutgoingMessage(ref);
            }
         }
      }

      // Handle input and output on the TCP socket
      const bool reading = multiplexer.IsSocketReadyForRead(socketReadFD);
      const bool writing = multiplexer.IsSocketReadyForWrite(socketWriteFD);
      const bool writeError = ((writing)&&(gatewayRef()->DoOutput().IsError()));
      const bool readError  = ((reading)&&(gatewayRef()->DoInput(tcpInQueue).IsError()));
      if ((readError)||(writeError))
      {
         printf("Connection closed (%s), exiting.\n", writeError?"Write Error":"Read Error");
         keepGoing = false;
      }

      MessageRef msgFromTCP;
      while(tcpInQueue.RemoveHead(msgFromTCP).IsOK())
      {
         printf("Heard message from server:-----------------------------------\n");
         msgFromTCP()->PrintToStream();
         printf("-------------------------------------------------------------\n");
      }
   }

   if (gatewayRef()->HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((gatewayRef()->HasBytesToOutput())&&(gatewayRef()->DoOutput().IsOK())) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");

   return 0;
}
