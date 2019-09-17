#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/DumbReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"dumb\" Message client.  It will connect to\n");
   printf("the same TCP port that the example_1_dumb_server listens on, and then\n");
   printf("send a Message objects to the server whenever you type a line of text on\n");
   printf("stdin.  It will also receive Messages from the server and print them\n");
   printf("to stdout.\n");
   printf("\n");
   printf("Note that we are using the same ReflectServer event loop as the\n");
   printf("example_1_dumb_server did, but we aren't calling PutAcceptFactory()\n");
   printf("on it so this process won't be accepting any incoming TCP connections.\n");
   printf("\n");
}

static const uint16 DUMB_SERVER_TCP_PORT = 8765;  // arbitrary port number for the "dumb" server

/** This session will read the user's input from stdin and create Messages
  * to pass to the DumbReflectSession so that the Messages get sent to the server.
  */
class MyDumbStdinSession : public DumbReflectSession
{
public:
   MyDumbStdinSession() {/* empty */}

   // We need this session to read from stdin
   virtual DataIORef CreateDataIO(const ConstSocketRef & socket)
   {
      return DataIORef(new StdinDataIO(false));  // false == non-blocking mode (ReflectServers prefer non-blocking mode)
   }

   // The expected data to read from stdin will be text from the user's keyboard
   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      return AbstractMessageIOGatewayRef(new PlainTextMessageIOGateway);
   }

   // Called when some data has come in from our PlainTextMessageIOGateway
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userPtr)
   {
      if (msg()->GetString(PR_NAME_TEXT_LINE).IsEmpty()) return;  // no sense sending a Message with no text

      // Add some other data to the Message, just because we can
      msg()->AddString("This Message inspected for quality by", "Jeremy");
      msg()->AddInt32("The answer is", 42);

      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "Sending the following Message to the dumb server:\n");
      msg()->PrintToStream();

      // DumbReflectSession::MessageReceivedFromGateway() will forward this Message
      // on to all the other sessions that live on our ReflectServer.  (In our case
      // the only other session is the one connecting us via TCP to the server process)
      DumbReflectSession::MessageReceivedFromGateway(msg, userPtr);
   }

   // Called when we've received a MessageRef from another session object on 
   // our ReflectServer.  (In this case it would have to be from the
   // DumbReflectSession object since that is the only other session object present)
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "Received Message from the dumb server:\n");
      msg()->PrintToStream();
   }

   // If stdin is closed (e.g. via the user pressing CTRL-D) 
   // that should cause the client to quit, so let's request that here
   virtual bool ClientConnectionClosed()
   {
      LogTime(MUSCLE_LOG_INFO, "MyDumbStdinSession::ClientConnectClosed() called, EOF detected on stdin, ending the client's event loop!\n");
      EndServer();
      return DumbReflectSession::ClientConnectionClosed();
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the client is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   // This object contains our client program's event loop.
   // (Yes, even though it says 'Server'.  It's a general-purpose event loop)
   ReflectServer reflectServer;

   // This session's job will be to read text from stdin and create Messages
   // to pass to the DumbReflectSession that is connected to the server.
   MyDumbStdinSession myStdinSession;
   status_t ret;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&myStdinSession, false), GetInvalidSocket()).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add MyDumbStdinSession to the client, aborting! [%s]\n", ret());
      return 10;
   }

   // This session will connect out to the server (on localhost) and handle TCP-transmission
   // of Messages to the server and TCP-reception of Messages from the server.
   //
   // The SecondsToMicros(1) argument tells the ReflectServer to handle
   // a TCP disconnect by automatically reconnecting the session after a 1-second delay.
   DumbReflectSession tcpSession;
   if (reflectServer.AddNewConnectSession(AbstractReflectSessionRef(&tcpSession, false), localhostIP, DUMB_SERVER_TCP_PORT, SecondsToMicros(1)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add DumbReflectSession to the client, aborting! [%s]\n", ret());
      return 10;
   }

   // Now there's nothing left to do but run the event loop
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "example_2_dumb_client is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_2_dumb_client is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions
   // before they are destroyed (necessary only because we have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
