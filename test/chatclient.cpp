/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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
      chatMessage()->AddString(PR_NAME_KEYS, toString.Cstr());
      chatMessage()->AddString("session", "blah");  // will be set by server
      chatMessage()->AddString("text", messageText);
      if (strcmp(targetSessionID, "*")) chatMessage()->AddBool("private", true);
   }
   return chatMessage;
}

static MessageRef GenerateServerSubscription(const char * subscriptionString, bool quietly)
{
   MessageRef queryMsg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   if (queryMsg())
   {
      queryMsg()->AddBool(subscriptionString, true);  // the true doesn't signify anything
      if (quietly) queryMsg()->AddBool(PR_NAME_SUBSCRIBE_QUIETLY, true);  // suppress initial-state response
   }
   return queryMsg;
}        

static MessageRef GenerateSetLocalUserName(const char * name)
{
   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   MessageRef nameMessage = GetMessageFromPool();
   if ((uploadMsg())&&(nameMessage()))
   {
      nameMessage()->AddString("name", name);
      nameMessage()->AddInt32("port", 0);  // BeShare requires this, even though we don't use it
      nameMessage()->AddString("version_name", "MUSCLE demo chat client");
      nameMessage()->AddString("version_num", VERSION_STRING);
      uploadMsg()->AddMessage("beshare/name", nameMessage);
   }
   return uploadMsg;
}              

static MessageRef GenerateSetLocalUserStatus(const char * name)
{
   MessageRef uploadMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   MessageRef nameMessage = GetMessageFromPool();
   if ((uploadMsg())&&(nameMessage()))
   {
      nameMessage()->AddString("userstatus", name);
      uploadMsg()->AddMessage("beshare/userstatus", nameMessage);
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
   gw.AddOutgoingMessage(GenerateSetLocalUserName(userName));
   gw.AddOutgoingMessage(GenerateSetLocalUserStatus(userStatus));
   gw.AddOutgoingMessage(GenerateServerSubscription("SUBSCRIBE:beshare/*", false));
 
   StdinDataIO stdinIO(false);
   QueueGatewayMessageReceiver stdinInQueue;
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DataIORef(&stdinIO, false));
   const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

   // Our event loop
   Hashtable<String, String> _users;
   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   while(s())
   {
      const int fd = s.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      multiplexer.RegisterSocketForReadReady(stdinFD);

      while(s()) 
      {
         if (multiplexer.WaitForEvents() < 0)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed! [%s]\n", B_ERRNO());
            s.Reset();
            break;
         }

         if (multiplexer.IsSocketReadyForRead(stdinFD))
         {
            while(1)
            {
               const int32 bytesRead = stdinGateway.DoInput(stdinInQueue);
               if (bytesRead < 0)
               {
                  printf("Stdin closed, exiting!\n");
                  s.Reset();  // break us out of the outer loop
                  break;
               }
               else if (bytesRead == 0) break;  // no more to read
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
                     String sendText = String(tok.GetRemainderOfString()).Trim();
                     if (sendText.HasChars()) gw.AddOutgoingMessage(GenerateChatMessage(targetSessionID, sendText()));
                  }
                  else if (text.StartsWith("/nick "))
                  {
                     (void) tok();
                     String name = String(tok.GetRemainderOfString()).Trim();
                     if (name.HasChars())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Setting local user name to [%s]\n", name());
                        gw.AddOutgoingMessage(GenerateSetLocalUserName(name()));
                     }
                  }
                  else if (text.StartsWith("/status "))
                  {
                     (void) tok();
                     String status = String(tok.GetRemainderOfString()).Trim();
                     if (status.HasChars())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Setting local user status to [%s]\n", status());
                        gw.AddOutgoingMessage(GenerateSetLocalUserStatus(status()));
                     }
                  }
                  else if (text.StartsWith("/help"))
                  {
                     LogTime(MUSCLE_LOG_INFO, "Available commands are:  /nick, /msg, /status, /help, and /quit\n");
                  }
                  else if (text.StartsWith("/quit")) s.Reset();
                  else if (strlen(text()) > 0) gw.AddOutgoingMessage(GenerateChatMessage("*", text()));
               }
            }
         }

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);
         const bool writeError = ((writing)&&(gw.DoOutput() < 0));
         const bool readError  = ((reading)&&(gw.DoInput(inQueue) < 0));
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

                     msg()->RemoveName(PR_NAME_KEYS);
                     msg()->AddString(PR_NAME_KEYS, toString);

                     msg()->RemoveName("session");
                     msg()->AddString("session", "blah");  // server will set this correctly for us

                     msg()->RemoveName("version");
                     msg()->AddString("version", "MUSCLE demo chat client v" VERSION_STRING);

                     gw.AddOutgoingMessage(msg);
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

                        switch(GetPathDepth(nodepath.Cstr()))
                        {
                           case USER_NAME_DEPTH:
                           if (strncmp(GetPathClause(USER_NAME_DEPTH, nodepath.Cstr()), "name", 4) == 0) 
                           {
                              String userNameString = GetUserName(_users, sessionID);
                              if (_users.Remove(sessionID).IsOK()) LogTime(MUSCLE_LOG_INFO, "User [%s] has disconnected.\n", userNameString());
                           }
                           break;
                        }
                     }
                  }

                  // Look for sub-messages that indicate that nodes were added to the tree
                  for (MessageFieldNameIterator iter = msg()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
                  {
                     const String & np = iter.GetFieldName();
                     const int pathDepth = GetPathDepth(np());
                     if (pathDepth == USER_NAME_DEPTH)
                     {
                        MessageRef tempRef;
                        if (msg()->FindMessage(np, tempRef).IsOK())
                        {
                           const Message * pmsg = tempRef();
                           String sessionID = GetPathClause(SESSION_ID_DEPTH, np());
                           sessionID = sessionID.Substring(0, sessionID.IndexOf('/'));
                           switch(pathDepth)
                           {
                              case USER_NAME_DEPTH:
                              {
                                 String hostNameString = GetPathClause(HOST_NAME_DEPTH, np());
                                 hostNameString = hostNameString.Substring(0, hostNameString.IndexOf('/'));

                                 const char * nodeName = GetPathClause(USER_NAME_DEPTH, np());
                                 if (strncmp(nodeName, "name", 4) == 0)
                                 {
                                    const char * name;
                                    if (pmsg->FindString("name", &name).IsOK())
                                    {
                                       if (_users.ContainsKey(sessionID) == false) LogTime(MUSCLE_LOG_INFO, "User #%s has connected\n", sessionID());
                                       _users.Put(sessionID, name);
                                       LogTime(MUSCLE_LOG_INFO, "User #%s is now known as %s\n", sessionID(), name);
                                    }
                                 }
                                 else if (strncmp(nodeName, "userstatus", 9) == 0)
                                 {
                                    const char * status;
                                    if (pmsg->FindString("userstatus", &status).IsOK()) LogTime(MUSCLE_LOG_INFO, "%s is now [%s]\n", GetUserName(_users, sessionID)(), status);
                                 }
                              }
                              break;
                           }
                        }
                     }
                  }
               }
            }
         }

         if ((reading == false)&&(writing == false)) break;

         multiplexer.RegisterSocketForReadReady(stdinFD);
         multiplexer.RegisterSocketForReadReady(fd);
         if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      }
   } 

   if (gw.HasBytesToOutput())
   {
      LogTime(MUSCLE_LOG_INFO, "Waiting for all pending messages to be sent...\n");
      while((gw.HasBytesToOutput())&&(gw.DoOutput() >= 0)) {Log(MUSCLE_LOG_INFO, "."); fflush(stdout);}
   }
   LogTime(MUSCLE_LOG_INFO, "Bye!\n");

   return 0;
}
