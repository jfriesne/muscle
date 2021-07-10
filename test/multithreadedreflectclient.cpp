/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/QueryFilter.h"
#include "util/MiscUtilityFunctions.h"
#include "util/SocketCallbackMechanism.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"
#include "system/CallbackMessageTransceiverThread.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

class TestCallbackMessageTransceiverThread : public CallbackMessageTransceiverThread
{
public:
   TestCallbackMessageTransceiverThread(ICallbackMechanism * optCallbackMechanism) : CallbackMessageTransceiverThread(optCallbackMechanism) {/* empty */}

protected:
   virtual void BeginMessageBatch()
   {
      printf("Callback called in main thread:  BeginMessageBatch()\n");
   }

   virtual void MessageReceived(const MessageRef & msg, const String & sessionID)
   {
      printf("Callback called in main thread:  MessageReceived(%p,%s)\n", msg(), sessionID());
      if (msg()) msg()->PrintToStream();
   }

   virtual void EndMessageBatch()
   {
      printf("Callback called in main thread:  EndMessageBatch()\n");
   }

   virtual void SessionAccepted(const String & sessionID, uint32 factoryID, const IPAddressAndPort & iap)
   {
      printf("Callback called in main thread:  SessionAccepted(%s, " UINT32_FORMAT_SPEC ", %s)\n", sessionID(), factoryID, iap.ToString()());
   }

   virtual void SessionAttached(const String & sessionID)
   {
      printf("Callback called in main thread:  SessionAttached(%s)\n", sessionID());
   }

   virtual void SessionConnected(const String & sessionID, const IPAddressAndPort & connectedTo)
   {
      printf("Callback called in main thread:  SessionConnected(%s,%s)\n", sessionID(), connectedTo.ToString()());
   }

   virtual void SessionDisconnected(const String & sessionID)
   {
      printf("Callback called in main thread:  SessionDisconnected(%s)\n", sessionID());
   }

   virtual void SessionDetached(const String & sessionID)
   {
      printf("Callback called in main thread:  SessionDetached(%s)\n", sessionID());
   }

   virtual void FactoryAttached(uint32 factoryID)
   {
      printf("Callback called in main thread:  FactoryAttached(" UINT32_FORMAT_SPEC ")\n", factoryID);
   }

   virtual void FactoryDetached(uint32 factoryID)
   {
      printf("Callback called in main thread:  FactoryDetached(" UINT32_FORMAT_SPEC ")\n", factoryID);
   }

   virtual void ServerExited()
   {
      printf("Callback called in main thread:  ServerExited()\n");
   }

   virtual void OutputQueuesDrained(const MessageRef & ref)
   {
      printf("Callback called in main thread:  OutputQueuesDrained(%p)\n", ref());
      if (ref()) ref()->PrintToStream();
   }
};

// This is a multithreaded text based test client for the muscled server.
// It is useful for testing the CallbakMessageTransceiverThread class,
//  and could possibly be useful for other things, I don't know.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   SocketCallbackMechanism callbackMechanism;

   TestCallbackMessageTransceiverThread networkThread(&callbackMechanism);

   status_t ret;
   if (networkThread.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start networking thread!  [%s]\n", ret());
      return 10;
   }

   String hostName;
   uint16 port = 2960;
   if (argc > 1) ParseConnectArg(argv[1], hostName, port, false);

   if (networkThread.AddNewConnectSession(hostName, port).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add connect session for [%s:%u] [%s]\n", hostName(), port, ret());
      return 10;
   }

   // We'll receive plain text over stdin in the main thread's event-loop
   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));

   SocketMultiplexer multiplexer;
   QueueGatewayMessageReceiver stdinInQueue;
   bool keepGoing = true;
   while(keepGoing)
   {
      const int stdinFD        = stdinIO.GetReadSelectSocket().GetFileDescriptor();
      const int notifierReadFD = callbackMechanism.GetDispatchThreadNotifierSocket().GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(stdinFD);
      multiplexer.RegisterSocketForReadReady(notifierReadFD);
      if (multiplexer.WaitForEvents() < 0) printf("multithreadedreflectclient: WaitForEvents() failed in the main thread!\n");

      const uint64 now = GetRunTime64();

      // Receive data from stdin
      if (multiplexer.IsSocketReadyForRead(stdinFD))
      {
         while(1)
         {
            const int32 bytesRead = stdinGateway.DoInput(stdinInQueue);
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
                  ref()->what = MAKETYPE("umsg");
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
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
                  if (arg1) ref()->AddMessage(arg1, uploadMsg);
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
                     if (enableSupercede)  ref()->AddFlat(PR_NAME_FLAGS, SetDataNodeFlags(SETDATANODE_FLAG_ENABLESUPERCEDE));

                     MessageRef subMsg = GetMessageFromPool();
                     subMsg()->AddInt32(String("%1 counter").Arg(enableSupercede?"Supercede":"Normal"), j);
                     ref()->AddMessage("test_node", subMsg);

                     if (networkThread.SendMessageToSessions(ref).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Fast SendMessageToSessions() failed!  [%s]\n", ret());
                  }

                  ref = GetMessageFromPool(PR_COMMAND_PING);  // just so we can see when it's done
               }
               break;

               case 'k':
                  ref()->what = PR_COMMAND_KICK;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'b':
                  ref()->what = PR_COMMAND_ADDBANS;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'B':
                  ref()->what = PR_COMMAND_REMOVEBANS;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'g':
                  ref()->what = PR_COMMAND_GETDATA;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'G':
                  ref()->what = PR_COMMAND_GETDATATREES;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
                  ref()->AddString(PR_NAME_TREE_REQUEST_ID, "Tree ID!");
               break;

               case 'q':
                  keepGoing = send = false;
               break;

               case 'p':
                  ref()->what = PR_COMMAND_SETPARAMETERS;
                  if (arg1) ref()->AddString(arg1, "");
               break;

               case 'P':
                  ref()->what = PR_COMMAND_GETPARAMETERS;
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
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
               break;

               case 'D':
                  ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                  if (arg1) ref()->AddString(PR_NAME_KEYS, arg1);
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
               if (networkThread.SendMessageToSessions(ref).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "SendMessageToSessions() failed!  [%s]\n", ret());
            }
         }
      }

      // If notifier-socket is ready-for-read, it's time to dispatch events handed to us by the network-thread
      if (multiplexer.IsSocketReadyForRead(notifierReadFD)) callbackMechanism.DispatchCallbacks();
   }

   (void) networkThread.ShutdownInternalThread();  // makes sure we get a well-ordered shutdown
   printf("\n\nBye!\n");

   return 0;
}
