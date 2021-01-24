#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a limited \"smart\" Message client with a QueryFilter.\n");
   printf("\n");
   printf("It will connect to the same TCP port that the example_4_smart_server listens on,\n"); 
   printf("and subscribe to all client-supplied nodes that match the subscription-path\n");
   printf("AND whose current Message contains a field named \"User String\" whose\n");
   printf("contents contain the word \"magic\".\n");
   printf("\n");
   printf("Any nodes that don't meet those criteria will not be subscribed to or printed out.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

/** Bare-mimimum session needed to connect to the server and print out the Messages we get back */
class MyTCPSession : public AbstractReflectSession
{
public:
   MyTCPSession() {/* empty */}

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "Received the following Message from the server:\n");
      msg()->PrintToStream();
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the client is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   ReflectServer reflectServer;

   // Still using a DumbReflectSession here since all we need is Message-forwarding.
   // (All of the client's "smarts" will be implemented in the MySmartStdinSession class)
   MyTCPSession tcpSession;
   status_t ret;
   if (reflectServer.AddNewConnectSession(AbstractReflectSessionRef(&tcpSession, false), localhostIP, SMART_SERVER_TCP_PORT, SecondsToMicros(1)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add tcpSession to the client, aborting! [%s]\n", ret());
      return 10;
   }

   // Set up this client's subscription
   {
      MessageRef subscribeToNodesMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);

      // Our filter to only match Messages whose "User String" field contains \"magic\"
      StringQueryFilter sqf("User String", StringQueryFilter::OP_CONTAINS_IGNORECASE, "magic");
      if (subscribeToNodesMsg()->AddArchiveMessage("SUBSCRIBE:/*/*/*", sqf).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't add StringQueryFilter to subscribe Message, aborting! [%s]\n", ret());
         return 10;
      }

      // Send off our subscription request
      LogTime(MUSCLE_LOG_INFO, "Sending StringQueryFiltered-subscription request message to server:\n");
      subscribeToNodesMsg()->PrintToStream();

      // Send off our subscription request
      if (tcpSession.AddOutgoingMessage(subscribeToNodesMsg).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't send filtered-subscription Message, aborting! [%s]\n", ret());
         return 10;
      }
   }

   LogTime(MUSCLE_LOG_INFO, "This program is designed to be run in conjunction with example_4_smart_server\n");
   LogTime(MUSCLE_LOG_INFO, "Run this program, then run another smart client (e.g. example_5_smart_client)\n");
   LogTime(MUSCLE_LOG_INFO, "in another Terminal window, and start creating nodes with the other smart client\n");
   LogTime(MUSCLE_LOG_INFO, "by typing commands like these into the example_5_smart_client's Terminal window:\n");
   LogTime(MUSCLE_LOG_INFO, "   set node1 = foo\n");
   LogTime(MUSCLE_LOG_INFO, "   set node2 = magic foo\n");
   LogTime(MUSCLE_LOG_INFO, "   set node3 = bar\n");
   LogTime(MUSCLE_LOG_INFO, "   set node4 = magic bar\n");
   LogTime(MUSCLE_LOG_INFO, "   delete node*\n");
   LogTime(MUSCLE_LOG_INFO, "... and note that this client ONLY gets updates about nodes whose contents contain \"magic\"!\n");
   LogTime(MUSCLE_LOG_INFO, "(Other nodes don't match the QueryFilter we supplied and thus don't exist as far as our\n");
   LogTime(MUSCLE_LOG_INFO, "subscription is concerned)\n");
   printf("\n");

   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "eexample_2_smart_client_with_queryfilter is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_2_smart_client_with_queryfilter is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions
   // before they are destroyed (necessary only because we have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
