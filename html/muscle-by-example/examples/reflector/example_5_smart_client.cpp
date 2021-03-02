#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"
#include "util/StringTokenizer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"smart\" Message client.  It will connect to\n");
   printf("the same TCP port that the example_4_smart_server listens on, and then\n");
   printf("send a Message objects to the server whenever you type a line of text on\n");
   printf("stdin.  It will also receive Messages from the server and print them\n");
   printf("to stdout.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

static void PrintHelp()
{
   LogTime(MUSCLE_LOG_INFO, "Commands that the smart-client supports are of this type:\n");
   LogTime(MUSCLE_LOG_INFO, "   set some/node/path = some text\n");
   LogTime(MUSCLE_LOG_INFO, "   get /some/node/path                 (wildcarded paths ok)\n");
   LogTime(MUSCLE_LOG_INFO, "   delete some/node/path               (wildcarded paths ok)\n");
   LogTime(MUSCLE_LOG_INFO, "   subscribe /some/node/path           (wildcarded paths ok)\n");
   LogTime(MUSCLE_LOG_INFO, "   unsubscribe /some/node/path         (wildcarded paths ok)h\n");
   LogTime(MUSCLE_LOG_INFO, "   msg /some/node/path some text       (wildcarded paths ok)h\n");
}

/** This session will read the user's input from stdin and create Messages
  * to pass to the client's DumbReflectSession so that the Messages get sent to the server.
  */
class MySmartStdinSession : public AbstractReflectSession
{
public:
   MySmartStdinSession() {/* empty */}

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
      const String * nextStr;
      for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &nextStr).IsOK(); i++) HandleStdinCommandFromUser(*nextStr);
   }

   // Called when we've received a MessageRef from another session object on 
   // our ReflectServer.  (In this case it would have to be from the
   // StorageReflectSession object since that is the only other session object present)
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "Received Message from the smart server:\n");
      msg()->PrintToStream();
   }

   // If stdin is closed (e.g. via the user pressing CTRL-D) 
   // that should cause the client to quit, so let's request that here
   virtual bool ClientConnectionClosed()
   {
      LogTime(MUSCLE_LOG_INFO, "MySmartStdinSession::ClientConnectClosed() called, EOF detected on stdin, ending the client's event loop!\n");
      EndServer();
      return AbstractReflectSession::ClientConnectionClosed();  // returns true
   }

private:
   // My function to parse commands from stdin (see help text in main() for the syntax)
   void HandleStdinCommandFromUser(const String & stdinCommand)
   {
      if (stdinCommand.IsEmpty()) return;

      StringTokenizer tok(stdinCommand(), NULL, " ");
      const String cmd  = tok();
      if (cmd == "die")
      {
         LogTime(MUSCLE_LOG_INFO, "Client process death requested, bye!\n");
         EndServer();
         return;
      }
      else if ((cmd == "set") || (cmd == "s"))
      {
         const String theRest = String(tok.GetRemainderOfString());
         if (theRest.HasChars())
         {
            const int eqIdx = theRest.IndexOf('=');
            const String pathArg = (eqIdx >= 0) ? theRest.Substring(0, eqIdx).Trim() : theRest;
            const String dataArg = (eqIdx >= 0) ? theRest.Substring(eqIdx+1).Trim()  : "default";

            if (pathArg.StartsWith("/"))
            {
               LogTime(MUSCLE_LOG_ERROR, "PR_COMMAND_SETDATA paths cannot start with a slash (because you're only allowed to set nodes within your own session-folder!)\n");
            }
            else
            {
               LogTime(MUSCLE_LOG_INFO, "Sending PR_COMMAND_SETDATA to set node at subpath [%s] to contain a Message containing data string [%s]\n", pathArg(), dataArg());
               if (HasRegexTokens(pathArg)) LogTime(MUSCLE_LOG_WARNING, "Note: PR_COMMAND_SETDATA won't do pattern-matching on wildcard chars; rather they will become literal chars in the node-path!\n");
   
                MessageRef dataPayloadMsg = GetMessageFromPool();
                dataPayloadMsg()->AddString("User String", dataArg);

                MessageRef setDataMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
                setDataMsg()->AddMessage(pathArg, dataPayloadMsg);

                SendMessageToServer(setDataMsg);
            } 
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  set my_node_dir/my_node_file = some text to put in the node\n");
      }
      else if ((cmd == "get") || (cmd == "g"))
      {
         const String pathArg = tok();
         if (pathArg.HasChars())
         {
            LogTime(MUSCLE_LOG_INFO, "Sending PR_COMMAND_GETDATA to do a one-time download of nodes matching the following path: [%s]\n", pathArg());

            MessageRef getDataMsg = GetMessageFromPool(PR_COMMAND_GETDATA);
            getDataMsg()->AddString(PR_NAME_KEYS, pathArg);
            SendMessageToServer(getDataMsg);
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  get /*/*\n");
      }
      else if ((cmd == "delete") || (cmd == "d"))
      {
         const String pathArg = tok();
         if (pathArg.HasChars())
         {
            LogTime(MUSCLE_LOG_INFO, "Sending PR_COMMAND_REMOVEDATA to delete any nodes matching the following path: [%s]\n", pathArg());

            MessageRef deleteNodesMsg = GetMessageFromPool(PR_COMMAND_REMOVEDATA);
            deleteNodesMsg()->AddString(PR_NAME_KEYS, pathArg);
            SendMessageToServer(deleteNodesMsg);
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  delete *\n");
      }
      else if ((cmd == "subscribe") || (cmd == "S"))
      {
         const String pathArg = tok();
         if (pathArg.HasChars())
         {
            LogTime(MUSCLE_LOG_INFO, "Sending PR_COMMAND_SETPARAMETERS to set up a \"live\" subscription to any nodes matching the following path: [%s]\n", pathArg());

            MessageRef subscribeToNodesMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
            subscribeToNodesMsg()->AddBool(String("SUBSCRIBE:%1").Arg(pathArg), true);
            SendMessageToServer(subscribeToNodesMsg);
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  subscribe /*/*\n");
      }
      else if ((cmd == "unsubscribe") || (cmd == "u"))
      {
         const String pathArg = tok();
         if (pathArg.HasChars())
         {
            LogTime(MUSCLE_LOG_INFO, "Sending PR_COMMAND_REMOVEPARAMETERS to get rid of any \"live\" subscriptions that match the following string: [SUBSCRIBE:%s]\n", pathArg());

            MessageRef unsubscribeFromNodesMsg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
            unsubscribeFromNodesMsg()->AddString(PR_NAME_KEYS, String("SUBSCRIBE:%1").Arg(pathArg));
            SendMessageToServer(unsubscribeFromNodesMsg);
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  unsubscribe /*/*\n");
      }
      else if ((cmd == "msg") || (cmd == "m"))
      {
         const String pathArg  = tok();
         if (pathArg.HasChars())
         {
            const String userText = tok.GetRemainderOfString();

            MessageRef chatMsg = GetMessageFromPool(1234);  // any non-PR_COMMAND_* message code will work here
            chatMsg()->AddString(PR_NAME_KEYS, pathArg);
            chatMsg()->AddString("chat_text", userText);
            SendMessageToServer(chatMsg);
         }
         else LogTime(MUSCLE_LOG_INFO, "Usage Example:  msg /*/* Hey guys!\n");
      }
      else if ((cmd == "help") || (cmd == "h"))
      {
         PrintHelp();
      }
      else LogTime(MUSCLE_LOG_ERROR, "Couldn't parse stdin command [%s].  Enter help to review the command-help-text.\n", stdinCommand());
   }

   void SendMessageToServer(const MessageRef & msg)
   {
      BroadcastToAllSessionsOfType<DumbReflectSession>(msg); // send this Message to the tcpSession (and from there to the server)

      LogTime(MUSCLE_LOG_INFO, "Sent the following Message to the smart server:\n");
      msg()->PrintToStream();
      printf("\n");
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the client is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   ReflectServer reflectServer;

   status_t ret;

   MySmartStdinSession myStdinSession;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&myStdinSession, false), GetInvalidSocket()).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add MySmartStdinSession to the client, aborting! [%s]\n", ret());
      return 10;
   }

   // Still using a DumbReflectSession here since all we need is Message-forwarding.
   // (All of the client's "smarts" will be implemented in the MySmartStdinSession class)
   DumbReflectSession tcpSession;
   if (reflectServer.AddNewConnectSession(AbstractReflectSessionRef(&tcpSession, false), localhostIP, SMART_SERVER_TCP_PORT, SecondsToMicros(1)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add StorageReflectSession to the client, aborting! [%s]\n", ret());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "This program is designed to be run in conjunction with example_4_smart_server\n");
   LogTime(MUSCLE_LOG_INFO, "You'll probably want to run multiple instances of this client at the same time, also.\n");
   printf("\n");
   PrintHelp();

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Some example commands that you can enter:\n");
   LogTime(MUSCLE_LOG_INFO, "   subscribe /*/*       -> will set up a subscription that always lets you know who is connected\n");
   LogTime(MUSCLE_LOG_INFO, "   subscribe /*/*/*     -> will set up a subscription that always lets you know who set/deleted/updated a node\n");
   LogTime(MUSCLE_LOG_INFO, "   subscribe *          -> is the same as the previous command (the initial wildcards can be implicit)\n");
   LogTime(MUSCLE_LOG_INFO, "   set frood = groovy   -> create a node named 'frood' in your session-folder, with the word 'groovy' in its Message\n");
   LogTime(MUSCLE_LOG_INFO, "   delete frood         -> delete the node named 'frood' in your session-folder\n");
   LogTime(MUSCLE_LOG_INFO, "   delete f*            -> delete all nodes in your session-folder whose names start with f\n");
   LogTime(MUSCLE_LOG_INFO, "   delete *             -> delete all nodes in your session-folder\n");
   LogTime(MUSCLE_LOG_INFO, "   msg /*/* hello       -> say hello to everyone who is connected\n");
   LogTime(MUSCLE_LOG_INFO, "   msg /*/*/frood hello -> say hello to everyone who is connected and created a node named \'frood\' in their session-folder\n");
   LogTime(MUSCLE_LOG_INFO, "   die                  -> cause the client process to exit\n");
   printf("\n");

   // Now there's nothing left to do but run the event loop
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "example_5_smart_client is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_5_smart_client is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions
   // before they are destroyed (necessary only because we have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
