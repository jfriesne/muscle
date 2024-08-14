/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/Hashtable.h"
#include "util/SocketMultiplexer.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/MiscUtilityFunctions.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define VERSION_STRING "1.05"

// stolen from ShareNetClient.h
enum
{
   NET_CLIENT_CONNECTED_TO_SERVER = 0,
   NET_CLIENT_DISCONNECTED_FROM_SERVER,
   NET_CLIENT_NEW_CHAT_TEXT,
   NET_CLIENT_CONNECT_BACK_REQUEST,
   NET_CLIENT_CHECK_FILE_COUNT,
   NET_CLIENT_PING,
   NET_CLIENT_PONG,
   NET_CLIENT_SCAN_THREAD_REPORT
};

// ditto
enum
{
   ROOT_DEPTH = 0,         // root node
   HOST_NAME_DEPTH,
   SESSION_ID_DEPTH,
   BESHARE_HOME_DEPTH,     // used to separate our stuff from other (non-BeShare) data on the same server
   USER_NAME_DEPTH,        // user's handle node would be found here
   FILE_INFO_DEPTH         // user's shared file list is here
};

static MessageRef GenerateChatMessage(const char * targetSessionID, const char * messageText)
{
   MessageRef chatMessage = GetMessageFromPool(NET_CLIENT_NEW_CHAT_TEXT);
   if (chatMessage())
   {
      String toString("/*/");  // send message to all hosts...
      toString += targetSessionID;
      toString += "/beshare";
      MRETURN_ON_ERROR(chatMessage()->AddString(PR_NAME_KEYS, toString.Cstr()));
      MRETURN_ON_ERROR(chatMessage()->AddString("session", "blah"));  // will be set by server
      MRETURN_ON_ERROR(chatMessage()->AddString("text", messageText));
      if (strcmp(targetSessionID, "*") != 0) MRETURN_ON_ERROR(chatMessage()->AddBool("private", true));
   }
   return chatMessage;
}

static MessageRef GenerateServerSubscription(const char * subscriptionString, bool quietly)
{
   MessageRef queryMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   if (queryMsg())
   {
      MRETURN_ON_ERROR(queryMsg()->AddBool(subscriptionString, true));  // the true doesn't signify anything
      MRETURN_ON_ERROR(queryMsg()->CAddBool(PR_NAME_SUBSCRIBE_QUIETLY, quietly));  // suppress initial-state response
   }
   return queryMsg;
}

static MessageRef GenerateSetLocalUserName(const char * name)
{
   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   MessageRef nameMessage = GetMessageFromPool();
   if ((uploadMsg())&&(nameMessage()))
   {
      MRETURN_ON_ERROR(nameMessage()->AddString("name", name));
      MRETURN_ON_ERROR(nameMessage()->AddInt32("port", 0));  // BeShare requires this, even though we don't use it
      MRETURN_ON_ERROR(nameMessage()->AddString("version_name", "MUSCLE demo chat client"));
      MRETURN_ON_ERROR(nameMessage()->AddString("version_num", VERSION_STRING));
      MRETURN_ON_ERROR(uploadMsg()->AddMessage("beshare/name", nameMessage));
   }
   return uploadMsg;
}

static MessageRef GenerateSetLocalUserStatus(const char * name)
{
   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   MessageRef nameMessage = GetMessageFromPool();
   if ((uploadMsg())&&(nameMessage()))
   {
      MRETURN_ON_ERROR(nameMessage()->AddString("userstatus", name));
      MRETURN_ON_ERROR(uploadMsg()->AddMessage("beshare/userstatus", nameMessage));
   }
   return uploadMsg;
}

static String GetUserName(Hashtable<String, String> & users, const String & sessionID)
{
   String ret;
   const String * userName = users.Get(sessionID);
   return sessionID + "/" + (userName ? (*userName) : String("<unknown>"));
}

// Simple little text-based BeShare-compatible chat client.  Should work with any muscled server.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args;
   (void) ParseArgs(argc, argv, args);

   const char * hostName   = "beshare.tycomsystems.com";  (void) args.FindString("server", &hostName);
   const char * userName   = "clyde";                     (void) args.FindString("nick", &userName);
   const char * userStatus = "here";                      (void) args.FindString("status", &userStatus);
   const char * tempStr;
   uint16 port = 0; if (args.FindString("port", &tempStr).IsOK()) port = (uint16) atoi(tempStr);
   if (port == 0) port = 2960;

   // Connect to the server
   ConstSocketRef s = Connect(hostName, port, "clyde", false);
   if (s() == NULL) return 10;

   // Do initial setup
   MessageIOGateway gw; gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));
   (void) gw.AddOutgoingMessage(GenerateSetLocalUserName(userName));
   (void) gw.AddOutgoingMessage(GenerateSetLocalUserStatus(userStatus));
   (void) gw.AddOutgoingMessage(GenerateServerSubscription("SUBSCRIBE:beshare/*", false));

   StdinDataIO stdinIO(false);
   QueueGatewayMessageReceiver stdinInQueue;
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));
   const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

   // Our event loop
   Hashtable<String, String> _users;
   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   while(s())
   {
      const int fd = s.GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
      (void) multiplexer.RegisterSocketForReadReady(stdinFD);

      while(s())
      {
         status_t ret;
         if (multiplexer.WaitForEvents().IsError(ret))
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed! [%s]\n", ret());
            s.Reset();
            break;
         }

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

            MessageRef msgFromStdin;
            while(stdinInQueue.RemoveHead(msgFromStdin).IsOK())
            {
               const String * st;
               for (int32 i=0; msgFromStdin()->FindString(PR_NAME_TEXT_LINE, i, &st).IsOK(); i++)
               {
                  const String & text = *st;
                  printf("Sending: [%s]\n", text());

                  StringTokenizer tok(text());
                  if (text.StartsWith("/msg "))
                  {
                     (void) tok();
                     const char * targetSessionID = tok();
                     const String sendText = String(tok.GetRemainderOfString()).Trimmed();
                     if (sendText.HasChars()) (void)gw.AddOutgoingMessage(GenerateChatMessage(targetSessionID, sendText()));
                  }
                  else if (text.StartsWith("/nick "))
                  {
                     (void) tok();
                     const String name = String(tok.GetRemainderOfString()).Trimmed();
                     if (name.HasChars())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Setting local user name to [%s]\n", name());
                        (void) gw.AddOutgoingMessage(GenerateSetLocalUserName(name()));
                     }
                  }
                  else if (text.StartsWith("/status "))
                  {
                     (void) tok();
                     const String status = String(tok.GetRemainderOfString()).Trimmed();
                     if (status.HasChars())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Setting local user status to [%s]\n", status());
                        (void) gw.AddOutgoingMessage(GenerateSetLocalUserStatus(status()));
                     }
                  }
                  else if (text.StartsWith("/help"))
                  {
                     LogTime(MUSCLE_LOG_INFO, "Available commands are:  /nick, /msg, /status, /help, and /quit\n");
                  }
                  else if (text.StartsWith("/quit")) s.Reset();
                  else if (strlen(text()) > 0) (void) gw.AddOutgoingMessage(GenerateChatMessage("*", text()));
               }
            }
         }

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);
         const bool writeError = ((writing)&&(gw.DoOutput().IsError()));
         const bool readError  = ((reading)&&(gw.DoInput(inQueue).IsError()));
         if ((readError)||(writeError))
         {
            LogTime(MUSCLE_LOG_ERROR, "Connection closed, exiting.\n");
            s.Reset();
         }

         MessageRef msg;
         while(inQueue.RemoveHead(msg).IsOK())
         {
            switch(msg()->what)
            {
               case NET_CLIENT_PING:  // respond to other clients' pings
               {
                  String replyTo;
                  if (msg()->FindString("session", replyTo).IsOK())
                  {
                     msg()->what = NET_CLIENT_PONG;

                     String toString("/*/");
                     toString += replyTo;
                     toString += "/beshare";

                     (void) msg()->RemoveName(PR_NAME_KEYS);
                     (void) msg()->AddString(PR_NAME_KEYS, toString);

                     (void) msg()->RemoveName("session");
                     (void) msg()->AddString("session", "blah");  // server will set this correctly for us

                     (void) msg()->RemoveName("version");
                     (void) msg()->AddString("version", "MUSCLE demo chat client v" VERSION_STRING);

                     (void) gw.AddOutgoingMessage(msg);
                  }
               }
               break;

               case NET_CLIENT_NEW_CHAT_TEXT:
               {
                  // Someone has sent a line of chat text to display
                  const char * text;
                  const char * session;
                  if ((msg()->FindString("text", &text).IsOK())&&(msg()->FindString("session", &session).IsOK()))
                  {
                     if (strncmp(text, "/me ", 4) == 0) LogTime(MUSCLE_LOG_INFO, "<ACTION>: %s %s\n", GetUserName(_users, session)(), &text[4]);
                                                   else LogTime(MUSCLE_LOG_INFO, "%s(%s): %s\n", msg()->HasName("private") ? "<PRIVATE>: ":"", GetUserName(_users, session)(), text);
                  }
               }
               break;

               case PR_RESULT_DATAITEMS:
               {
                  // Look for sub-messages that indicate that nodes were removed from the tree
                  String nodepath;
                  for (int i=0; (msg()->FindString(PR_NAME_REMOVED_DATAITEMS, i, nodepath).IsOK()); i++)
                  {
                     const int pathDepth = GetPathDepth(nodepath.Cstr());
                     if (pathDepth >= USER_NAME_DEPTH)
                     {
                        String sessionID = GetPathClause(SESSION_ID_DEPTH, nodepath.Cstr());
                        sessionID = sessionID.Substring(0, sessionID.IndexOf('/'));

                        if ((GetPathDepth(nodepath.Cstr()) == USER_NAME_DEPTH)&&(strncmp(GetPathClause(USER_NAME_DEPTH, nodepath.Cstr()), "name", 4) == 0))
                        {
                           String userNameString = GetUserName(_users, sessionID);
                           if (_users.Remove(sessionID).IsOK()) LogTime(MUSCLE_LOG_INFO, "User [%s] has disconnected.\n", userNameString());
                        }
                     }
                  }

                  // Look for sub-messages that indicate that nodes were added to the tree
                  for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
                  {
                     const String & np = iter.GetFieldName();
                     if (GetPathDepth(np()) == USER_NAME_DEPTH)
                     {
                        ConstMessageRef tempRef;
                        if (msg()->FindMessage(np, tempRef).IsOK())
                        {
                           const Message * pmsg = tempRef();
                           String sessionID = GetPathClause(SESSION_ID_DEPTH, np());
                           sessionID = sessionID.Substring(0, sessionID.IndexOf('/'));

                           String hostNameString = GetPathClause(HOST_NAME_DEPTH, np());
                           hostNameString = hostNameString.Substring(0, hostNameString.IndexOf('/'));

                           const char * nodeName = GetPathClause(USER_NAME_DEPTH, np());
                           if (strncmp(nodeName, "name", 4) == 0)
                           {
                              const char * name;
                              if (pmsg->FindString("name", &name).IsOK())
                              {
                                 if (_users.ContainsKey(sessionID) == false) LogTime(MUSCLE_LOG_INFO, "User #%s has connected\n", sessionID());
                                 (void) _users.Put(sessionID, name);
                                 LogTime(MUSCLE_LOG_INFO, "User #%s is now known as %s\n", sessionID(), name);
                              }
                           }
                           else if (strncmp(nodeName, "userstatus", 10) == 0)
                           {
                              const char * status;
                              if (pmsg->FindString("userstatus", &status).IsOK()) LogTime(MUSCLE_LOG_INFO, "%s is now [%s]\n", GetUserName(_users, sessionID)(), status);
                           }
                        }
                     }
                  }
               }
               break;

               default:
                  LogTime(MUSCLE_LOG_WARNING, "Received unknown incoming Message what-code " UINT32_FORMAT_SPEC "\n", msg()->what);
               break;
            }
         }

         if ((reading == false)&&(writing == false)) break;

         (void) multiplexer.RegisterSocketForReadReady(stdinFD);
         (void) multiplexer.RegisterSocketForReadReady(fd);
         if (gw.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
      }
   }

   if (gw.HasBytesToOutput())
   {
      LogTime(MUSCLE_LOG_INFO, "Waiting for all pending messages to be sent...\n");
      while((gw.HasBytesToOutput())&&(gw.DoOutput().IsOK())) {LogPlain(MUSCLE_LOG_INFO, "."); fflush(stdout);}
   }
   LogTime(MUSCLE_LOG_INFO, "Bye!\n");

   return 0;
}
