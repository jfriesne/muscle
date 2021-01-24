#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/MessageTransceiverThread.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"
#include "util/StringTokenizer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"smart\" Message client using a MessageTransceiverThread.\n");
   printf("\n");
   printf("It will connect to the same TCP port that the example_4_smart_server listens on,\n");
   printf("and then send a Message objects to the server whenever you type a line of text on\n");
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

// Parses a string that the user typed in to stdin and creates a Message to send to the server.
static MessageRef ParseStdinCommand(const String & stdinCommand)
{
   if (stdinCommand.IsEmpty()) return MessageRef();

   StringTokenizer tok(stdinCommand(), NULL, " ");
   const String cmd  = tok();
   if ((cmd == "set") || (cmd == "s"))
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

             return setDataMsg;
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
         return getDataMsg;
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
         return deleteNodesMsg;
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
         return subscribeToNodesMsg;
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
         return unsubscribeFromNodesMsg;
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
         return chatMsg;
      }
      else LogTime(MUSCLE_LOG_INFO, "Usage Example:  msg /*/* Hey guys!\n");
   }
   else if ((cmd == "help") || (cmd == "h"))
   {
      PrintHelp();
   }
   else LogTime(MUSCLE_LOG_ERROR, "Couldn't parse stdin command [%s].  Enter help to review the command-help-text.\n", stdinCommand());

   return MessageRef();  // nothing to send for now
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the client is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   status_t ret;

   MessageTransceiverThread mtt;
   if (mtt.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't start the MessageTransceiverThread, aborting! [%s]\n", ret());
      return 10;
   }

   if (mtt.AddNewConnectSession(localhostIP, SMART_SERVER_TCP_PORT, SecondsToMicros(1)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "mtt.AddNewConnectSession() failed, aborting! [%s]\n", ret());
      mtt.ShutdownInternalThread();
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

   // Run our own event loop to read from stdin and retrieve
   // feedback events from the MessageTransceiverThread.
   // (In other contexts this might be a QEventLoop or
   // a Win32 event loop or an SDL event loop or etc; anything
   // where the main thread needs to be doing some non-MUSCLE
   // event loop is a good use case for QMessageTransceiverThread)

   StdinDataIO stdinIO(false);
   SocketMultiplexer sm;
   while(true)
   {
      sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      sm.RegisterSocketForReadReady(mtt.GetOwnerWakeupSocket().GetFileDescriptor());

      sm.WaitForEvents();

      if (sm.IsSocketReadyForRead(stdinIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         // Handle stdin input, and send a Message to the MessageTransceiverThread
         // for it to send on to the server, if appropriate
         uint8 inputBuf[1024];
         const int numBytesRead = stdinIO.Read(inputBuf, sizeof(inputBuf)-1);
         if (numBytesRead > 0)
         {
            String inputCmd((const char *) inputBuf, numBytesRead);
            inputCmd = inputCmd.Trim();
            if (inputCmd == "die") break;

            MessageRef msgToSend = ParseStdinCommand(inputCmd);
            if (msgToSend())
            {
               printf("Calling mtt.SendMessageToSessions() with the following Message:\n");
               msgToSend()->PrintToStream();
              (void) mtt.SendMessageToSessions(msgToSend);
            }
         }
         else if (numBytesRead < 0) break;
      }

      if (sm.IsSocketReadyForRead(mtt.GetOwnerWakeupSocket().GetFileDescriptor()))
      {
         // Handle any feedback events sent back to us from the MessageTransceiverThread
         uint32 code;
         MessageRef ref;
         String session;
         uint32 factoryID;
         IPAddressAndPort location;
         while(mtt.GetNextEventFromInternalThread(code, &ref, &session, &factoryID, &location) >= 0)
         {
            String codeStr;
            switch(code)
            {
               case MTT_EVENT_INCOMING_MESSAGE:      codeStr = "IncomingMessage";     break;
               case MTT_EVENT_SESSION_ACCEPTED:      codeStr = "SessionAccepted";     break;
               case MTT_EVENT_SESSION_ATTACHED:      codeStr = "SessionAttached";     break;
               case MTT_EVENT_SESSION_CONNECTED:     codeStr = "SessionConnected";    break;
               case MTT_EVENT_SESSION_DISCONNECTED:  codeStr = "SessionDisconnected"; break;
               case MTT_EVENT_SESSION_DETACHED:      codeStr = "SessionDetached";     break;
               case MTT_EVENT_FACTORY_ATTACHED:      codeStr = "FactoryAttached";     break;
               case MTT_EVENT_FACTORY_DETACHED:      codeStr = "FactoryDetached";     break;
               case MTT_EVENT_OUTPUT_QUEUES_DRAINED: codeStr = "OutputQueuesDrained"; break;
               case MTT_EVENT_SERVER_EXITED:         codeStr = "ServerExited";        break;
               default:                              codeStr = String("\'%1\'").Arg(GetTypeCodeString(code)); break;
            }
            printf("Event from MTT:  type=[%s], session=[%s] factoryID=[" UINT32_FORMAT_SPEC "] location=[%s]\n", codeStr(), session(), factoryID, location.ToString()());
            if (ref()) ref()->PrintToStream();
         }
      }
   }
  
   mtt.ShutdownInternalThread();

   return 0;
}

