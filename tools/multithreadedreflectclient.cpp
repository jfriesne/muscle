/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/QueryFilter.h"
#include "system/CallbackMessageTransceiverThread.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketCallbackMechanism.h"

#ifdef WIN32
# define TEST_WIN32CALLBACKMECHANISM
# include "platform/win32/Win32CallbackMechanism.h"
#endif

using namespace muscle;

// Our subclass of CallbackMessageTransceiverThread.  Here's where we can get callbacks in the main thread whenever
// anything interesting has happened in the I/O thread, so that we can respond
class TestCallbackMessageTransceiverThread : public CallbackMessageTransceiverThread
{
public:
   TestCallbackMessageTransceiverThread(ICallbackMechanism * optCallbackMechanism) : CallbackMessageTransceiverThread(optCallbackMechanism), _keepGoing(true) {/* empty */}

   void SetStdinSessionRootPath(const String & sessionRootPath) {_stdinSessionRootPath = sessionRootPath;}
   void SetTCPSessionRootPath(  const String & sessionRootPath) {_tcpSessionRootPath   = sessionRootPath;}

   bool IsKeepGoing() const {return _keepGoing;}

protected:
   virtual void BeginMessageBatch()
   {
      printf("Callback called in main thread:  BeginMessageBatch()\n");
   }

   virtual void MessageReceived(const MessageRef & msg, const String & sessionRootPath)
   {
      printf("Callback called in main thread:  MessageReceived(%p,%s)\n", msg(), sessionRootPath());

      if (sessionRootPath == _stdinSessionRootPath)
      {
         const String * nextLine;
         for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &nextLine).IsOK(); i++) HandleTextLineFromStdin(*nextLine);
      }
      else if (msg()) msg()->PrintToStream();  // Any other Messages presumably came from the server, we'll just print them out
   }

   virtual void EndMessageBatch()
   {
      printf("Callback called in main thread:  EndMessageBatch()\n");
   }

   virtual void SessionAccepted(const String & sessionRootPath, uint32 factoryID, const IPAddressAndPort & iap)
   {
      printf("Callback called in main thread:  SessionAccepted(%s, " UINT32_FORMAT_SPEC ", %s)\n", sessionRootPath(), factoryID, iap.ToString()());
   }

   virtual void SessionAttached(const String & sessionRootPath)
   {
      printf("Callback called in main thread:  SessionAttached(%s)\n", sessionRootPath());
   }

   virtual void SessionConnected(const String & sessionRootPath, const IPAddressAndPort & connectedTo)
   {
      printf("Callback called in main thread:  SessionConnected(%s,%s)\n", sessionRootPath(), connectedTo.ToString()());
   }

   virtual void SessionDisconnected(const String & sessionRootPath)
   {
      printf("Callback called in main thread:  SessionDisconnected(%s)\n", sessionRootPath());
   }

   virtual void SessionDetached(const String & sessionRootPath)
   {
      printf("Callback called in main thread:  SessionDetached(%s)\n", sessionRootPath());
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

private:
   bool _keepGoing;
   String _stdinSessionRootPath;  // this will be set to the session-path-string of our Stdin-reading session in the local I/O thread
   String _tcpSessionRootPath;    // this will be set to the session-path-string of our TCP-reading/writing session in the local I/O thread

   status_t SendMessageToMuscleServer(const MessageRef & msg) {return SendMessageToSessions(msg, _tcpSessionRootPath);}
   void HandleTextLineFromStdin(const String & stdinText);
};

// This is a multithreaded text based test client for the muscled server.
// It is useful for testing the CallbakMessageTransceiverThread class,
//  and could possibly be useful for other things, I don't know.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

#ifdef WIN32
   Win32AllocateStdioConsole();
#endif

#ifdef TEST_WIN32CALLBACKMECHANISM
   // Just to test the Win32-specific callback mechanism (SocketCallbackMechanism would also work under Windows)
   Win32CallbackMechanism callbackMechanism(CreateEvent(0, false, false, 0), true);
#else
   SocketCallbackMechanism callbackMechanism;  // the mechanism our I/O thread will use to notify the main thread about new events
#endif

   TestCallbackMessageTransceiverThread networkThread(&callbackMechanism);  // the I/O thread object

   status_t ret;

   // Set up a session to receive plain-text from stdin in the I/O thread's event-loop
   // the received text will then be sent to the main thread to be handled by our HandleTextLineFromStdin() method
   {
      PlainTextMessageIOGatewayRef stdinGateway(new PlainTextMessageIOGateway);
      stdinGateway()->SetDataIO(DataIORef(new StdinDataIO(false)));

      ThreadWorkerSessionRef stdinSession(new ThreadWorkerSession);
      stdinSession()->SetGateway(stdinGateway);

      if (networkThread.AddNewSession(stdinSession).IsError(ret))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add connect stdin-reading session [%s]\n", ret());
         return 10;
      }

      networkThread.SetStdinSessionRootPath(stdinSession()->GetSessionRootPath());  // so he'll know which Messages are coming from stdinSession
   }

   // Set up a session to connect to the muscled server via TCP and send/receive Messages to/from the server
   {
      ThreadWorkerSessionRef tcpSession(new ThreadWorkerSession);

      String hostName = "localhost";
      uint16 port = 2960;
      if (argc > 1) (void) ParseConnectArg(argv[1], hostName, port, false);
      if (networkThread.AddNewConnectSession(hostName, port, tcpSession).IsError(ret))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add connect session for [%s:%u] [%s]\n", hostName(), port, ret());
         return 10;
      }

      networkThread.SetTCPSessionRootPath(tcpSession()->GetSessionRootPath());  // so he'll know which session to send outgoing-to-the-server Messages to
   }

   // Start the I/O thread running
   if (networkThread.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start networking thread!  [%s]\n", ret());
      return 10;
   }

#ifdef TEST_WIN32CALLBACKMECHANISM
   // This implementation will work on Windows only; I'm including it here solely as a
   // way to test/verify that the Win32CallbackMechanism class works as intended
   while((networkThread.IsKeepGoing())&&(WaitForSingleObject(callbackMechanism.GetSignalHandle(), INFINITE) == WAIT_OBJECT_0)) callbackMechanism.DispatchCallbacks();
#else
   // This implementation will work on all OS's (including Windows)

   // Setting the notifier-socket to blocking mode allows us to do the event-loop below without having
   // to use a SocketMultiplexer to wait until an event-signal is received.  Without this the while-loop
   // below would busy-wait and chew up 100% of a core
   if (SetSocketBlockingEnabled(callbackMechanism.GetDispatchThreadNotifierSocket(), true).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set notifier socket to blocking mode! [%s]\n", ret());
      return 10;
   }

   // Our main thread's event-loop:  Wait for the next signal from the I/O thread, then DispatchCallbacks()
   // will call the appropriate virtual-methods in networkThread for us
   while(networkThread.IsKeepGoing()) callbackMechanism.DispatchCallbacks();
#endif

   // Graceful cleanup and exit
   printf("\n\nBye!\n");
   (void) networkThread.ShutdownInternalThread();  // makes sure we get a well-ordered shutdown
   return 0;
}

// Code to parse and react to handle the various text commands the user might type in to stdin
void TestCallbackMessageTransceiverThread :: HandleTextLineFromStdin(const String & stdinText)
{
   status_t ret;

   const char * text = stdinText();

   printf("You typed: [%s]\n", text);
   bool send = true;
   MessageRef ref = GetMessageFromPool();

   const char * arg1 = (stdinText.Length()>2) ? &text[2] : NULL;
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

            if (SendMessageToMuscleServer(ref).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Fast SendMessageToMuscleServer() failed!  [%s]\n", ret());
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
         _keepGoing = send = false;
      break;

      case 'p':
         ref()->what = PR_COMMAND_SETPARAMETERS;
         if (arg1) (void) ref()->AddString(arg1, "");
      break;

      case 'P':
         ref()->what = PR_COMMAND_GETPARAMETERS;
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
      printf("Sending outgoing Message to I/O session [%s]...\n", _tcpSessionRootPath());
      ref()->PrintToStream();
      if (SendMessageToMuscleServer(ref).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "SendMessageToMuscleServer() failed!  [%s]\n", ret());
   }
}
