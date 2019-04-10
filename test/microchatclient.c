/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/stat.h>

#ifdef __HAIKU__
# include <sys/select.h>
#endif

#include "micromessage/MicroMessageGateway.h"
#include "reflector/StorageReflectConstants.h"

#define VERSION_STRING "1.05"

/* stolen from SystemInfo.cpp */
static const char * GetOSName()
{
   const char * ret = "Unknown";
   (void) ret;  // just to shut the Borland compiler up

#ifdef WIN32
   ret = "Windows";
#endif

#ifdef __CYGWIN__
   ret = "Windows (CygWin)";
#endif

#ifdef __APPLE__
   ret = "MacOS/X";
#endif

#ifdef __linux__
   ret = "Linux";
#endif

#ifdef __BEOS__
   ret = "BeOS";
#endif

#ifdef __HAIKU__
   ret = "Haiku";
#endif

#ifdef __ATHEOS__
   ret = "AtheOS";
#endif

#if defined(__QNX__) || defined(__QNXTO__)
   ret = "QNX";
#endif

#ifdef __FreeBSD__
   ret = "FreeBSD";
#endif

#ifdef __OpenBSD__
   ret = "OpenBSD";
#endif

#ifdef __NetBSD__
   ret = "NetBSD";
#endif

#ifdef __osf__
   ret = "Tru64";
#endif

#if defined(IRIX) || defined(__sgi)
   ret = "Irix";
#endif

#ifdef __OS400__
   ret = "AS400";
#endif

#ifdef __OS2__
   ret = "OS/2";
#endif

#ifdef _AIX
   ret = "AIX";
#endif

#ifdef _SEQUENT_
   ret = "Sequent";
#endif

#ifdef _SCO_DS
   ret = "OpenServer";
#endif

#if defined(_HP_UX) || defined(__hpux) || defined(_HPUX_SOURCE)
   ret = "HPUX";
#endif

#if defined(SOLARIS) || defined(__SVR4)
   ret = "Solaris";
#endif

#if defined(__UNIXWARE__) || defined(__USLC__)
   ret = "UnixWare";
#endif

   return ret;
}

/* stolen from ShareNetClient.h */
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

/* ditto */
enum
{
   ROOT_DEPTH = 0,         /* root node */
   HOST_NAME_DEPTH,
   SESSION_ID_DEPTH,
   BESHARE_HOME_DEPTH,     /* used to separate our stuff from other (non-BeShare) data on the same server */
   USER_NAME_DEPTH,        /* user's handle node would be found here */
   FILE_INFO_DEPTH         /* user's shared file list is here */
};         

static void Inet_NtoA(uint32 addr, char * ipbuf)
{
   muscleSprintf(ipbuf, INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "", (addr>>24)&0xFF, (addr>>16)&0xFF, (addr>>8)&0xFF, (addr>>0)&0xFF);
}

static int ConnectToIP(uint32 hostIP, uint16 port)
{
   char ipbuf[16];
   struct sockaddr_in saAddr; 
   int s;

   Inet_NtoA(hostIP, ipbuf);
   memset(&saAddr, 0, sizeof(saAddr));
   saAddr.sin_family      = AF_INET;
   saAddr.sin_port        = htons(port);
   saAddr.sin_addr.s_addr = htonl(hostIP);

   s = socket(AF_INET, SOCK_STREAM, 0);
   if (s >= 0)
   {
      if (connect(s, (struct sockaddr *) &saAddr, sizeof(saAddr)) >= 0) return s;
      close(s);
   }
   return -1;
}

static uint32 GetHostByName(const char * name)
{
   uint32 ret = inet_addr(name);  /* first see if we can parse it as a numeric address */
   if ((ret == 0)||(ret == (uint32)-1))
   {
      struct hostent * he = gethostbyname(name);
      ret = ntohl(he ? *((uint32*)he->h_addr) : 0);
   }
   else ret = ntohl(ret);

   return ret;
}

static int Connect(const char * hostName, uint16 port)
{
   const uint32 hostIP = GetHostByName(hostName);
   return (hostIP > 0) ? ConnectToIP(hostIP, port) : -1;
}

static int32 SocketSendFunc(const uint8 * buf, uint32 numBytes, void * arg)
{
   const int ret = send(*((int *)arg), (const char *)buf, numBytes, 0L);
   return (ret == -1) ? ((errno == EWOULDBLOCK)?0:-1) : ret;
}

static int32 SocketRecvFunc(uint8 * buf, uint32 numBytes, void * arg)
{
   const int ret = recv(*((int *)arg), (char *)buf, numBytes, 0L);
   if (ret == 0) return -1;  // 0 means TCP connection has closed, we'll treat that as an error
   return (ret == -1) ? ((errno == EWOULDBLOCK)?0:-1) : ret;
}

/* Creates an UMessage to send to the server when the user types in a text chat string */
static void SendChatMessage(UMessageGateway * gw, const char * targetSessionID, const char * messageText)
{
   UMessage chatMessage = UGGetOutgoingMessage(gw, NET_CLIENT_NEW_CHAT_TEXT);
   if (UMIsMessageValid(&chatMessage))
   {
      /* Specify which client(s) the Message should be forwarded to by muscled */
      char buf[1024]; muscleSprintf(buf, "/*/%s/beshare", targetSessionID);
      UMAddString(&chatMessage, PR_NAME_KEYS, buf);

      /* Add a "session" field so that the target clients will know who the             */
      /* Message came from.  Note that muscled will automatically set this field        */
      /* to its proper value, so we don't have to; we just have to make sure it exists. */
      UMAddString(&chatMessage, "session", "blah");

      /* Include the chat text that the user typed in to stdin */
      UMAddString(&chatMessage, "text", messageText);

      /* And lastly, if the Message wasn't meant to be a public chat message,  */
      /* include a "private" keyword to let the receiver know it's a private   */
      /* communication, so they can print it in a different color or whatever. */
      if (strcmp(targetSessionID, "*")) UMAddBool(&chatMessage, "private", UTrue);

      UGOutgoingMessagePrepared(gw, &chatMessage);
   }
}

/* Creates an UMessage to send to the server to set up our subscriptions              */
/* so that we get the proper notifications when other clients connect/disconnect, etc */
static void SendServerSubscription(UMessageGateway * gw, const char * subscriptionString, UBool quietly)
{
   UMessage uploadMsg = UGGetOutgoingMessage(gw, PR_COMMAND_SETPARAMETERS);
   if (UMIsMessageValid(&uploadMsg))
   {
      UMAddBool(&uploadMsg, subscriptionString, UFalse);
      if (quietly) UMAddBool(&uploadMsg, PR_NAME_SUBSCRIBE_QUIETLY, UTrue);
      UGOutgoingMessagePrepared(gw, &uploadMsg);
   }
}

/* Generates a UMessage that tells the server to post some interesting information */
/* about our client, for the other clients to see.                                 */
static void UploadLocalUserName(UMessageGateway * gw, const char * name)
{
   UMessage uploadMsg = UGGetOutgoingMessage(gw, PR_COMMAND_SETDATA);
   if (UMIsMessageValid(&uploadMsg))
   {
      UMessage nameMsg = UMInlineAddMessage(&uploadMsg, "beshare/name", 0);
      if (UMIsMessageValid(&nameMsg))
      {
         UMAddString(&nameMsg, "name", name);
         UMAddInt32(&nameMsg, "port", 0); /* BeShare requires this field, even though we don't use it since we don't share files */
         UMAddString(&nameMsg, "version_name", "MUSCLE C micro chat client");
         UMAddString(&nameMsg, "version_num", VERSION_STRING);
         UMAddString(&nameMsg, "version_num", GetOSName());
         UGOutgoingMessagePrepared(gw, &uploadMsg);
      }
      else UGOutgoingMessageCancelled(gw, &uploadMsg);
   }
}

/* Generates a message to set this client's user-status on the server (e.g. "Here" or "Away") */
static void UploadLocalUserStatus(UMessageGateway * gw, const char * status)
{
   UMessage uploadMsg = UGGetOutgoingMessage(gw, PR_COMMAND_SETDATA);
   if (UMIsMessageValid(&uploadMsg))
   {
      UMessage statusMsg = UMInlineAddMessage(&uploadMsg, "beshare/userstatus", 0);
      if (UMIsMessageValid(&statusMsg))
      {
         UMAddString(&statusMsg, "userstatus", status);
         UGOutgoingMessagePrepared(gw, &uploadMsg);
      }
      else UGOutgoingMessageCancelled(gw, &uploadMsg);
   }
}              

/* Returns a pointer into (path) after the (depth)'th '/' char */
static const char * GetPathClause(int depth, const char * path)
{
   int i;
   for (i=0; i<depth; i++)
   {
      const char * nextSlash = strchr(path, '/');
      if (nextSlash == NULL) 
      {
         path = NULL;
         break;
      }
      path = nextSlash + 1;
   }
   return path;          
}

/* Returns the depth of the given path string (e.g. "/"==0, "/hi"==1, "/hi/there"==2, etc) */
static int GetPathDepth(const char * path)
{
   int depth = 0;
   if (path[0] == '/') path++;  /* ignore any leading slash */

   while(1)
   {
      if (path[0]) depth++;

      path = strchr(path, '/');
      if (path) path++;
           else break;
   }
   return depth;
}

/* This struct represents the info we've collected about a given user (chat client) */
struct User
{
   int _sessionID;
   const char * _name;  /* strdup'd string */
   struct User * _nextUser;
};

static const char * GetUserName(struct User * cur, int sid)
{
   while(cur)
   {
      if (cur->_sessionID == sid) return cur->_name;
      cur = cur->_nextUser;
   }
   return "<unknown user>";
}

static UBool RemoveUserName(struct User ** users, int sid)
{
   struct User * cur  = *users;
   struct User * prev = NULL;
   while(cur)
   {
      if (cur->_sessionID == sid)
      {
         if (prev) prev->_nextUser = cur->_nextUser;
              else *users          = cur->_nextUser;
         free((char *) cur->_name);
         free(cur);
         return UTrue;
      }
      prev = cur;
      cur  = cur->_nextUser;
   }
   return UFalse;
}

static void PutUserName(struct User ** users, int sid, const char * name)
{
   struct User * newUser = malloc(sizeof(struct User));
   if (newUser)
   {
      (void) RemoveUserName(users, sid);  /* paranoia (ensure no duplicate sid's) */

      newUser->_sessionID = sid;
      newUser->_nextUser  = *users;
      newUser->_name      = strdup(name);
      if (newUser->_name) *users = newUser;
                     else free(newUser);
   }
}

/* This is a text based BeShare-compatible chat client for the muscled server.                        */
/* It is useful as an example of a chat client written in C, and perhaps for other things also.       */
/* This implementation of the client uses only the C UMessage interface, for micronal executable size. */
int main(int argc, char ** argv)
{
   const char * hostName   = "beshare.tycomsystems.com";
   const char * userName   = "microclyde";
   const char * userStatus = "here";
   int s;
   int port = 0;
   struct User * users = NULL;  /* doubly-linked-list! */
   uint8 inputBuffer[16*1024];
   uint8 outputBuffer[16*1024];

   UMessageGateway gw;
   UGGatewayInitialize(&gw, inputBuffer, sizeof(inputBuffer), outputBuffer, sizeof(outputBuffer));

#ifdef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
   printf("Warning:  This program doesn't run very well on this OS, because the OS can't select() on stdin.  You'll need to press return a lot.\n");
#endif

   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 2960;  /* default muscled port */

   s = Connect(hostName, (uint16)port);
   if (s >= 0)
   {
      UBool keepGoing = UTrue;
      fd_set readSet, writeSet;

      printf("Connection to [%s:%i] succeeded.\n", hostName, port);
      (void) fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0)|O_NONBLOCK);  /* Set s to non-blocking I/O mode */

      (void) UploadLocalUserName(&gw, userName);
      (void) UploadLocalUserStatus(&gw, userStatus);

      /* Tell the server we want to be updated about the beshare-specific attributes of other clients */
      SendServerSubscription(&gw, "SUBSCRIBE:beshare/*", UFalse);
 
      /* the main event loop */
      while(keepGoing)
      {
         int maxfd = s;
         struct timeval * timeout = NULL;
         char buf[2048] = "";

         FD_ZERO(&readSet);
         FD_ZERO(&writeSet);
         FD_SET(s, &readSet);
         if (UGHasBytesToOutput(&gw)) FD_SET(s, &writeSet);

#ifdef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
         /* This OS can't select() on stdin, so just make the user press enter or whatever */
         fflush(stdout);
         if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = '\0';
         {char * ret = strchr(buf, '\n'); if (ret) *ret = '\0';}
         struct timeval poll = {0, 0};
         timeout = &poll;  /* prohibit blocking in select() */
#else
         if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;
         FD_SET(STDIN_FILENO, &readSet);
#endif

         while(keepGoing) 
         {
            if (select(maxfd+1, &readSet, &writeSet, NULL, timeout) < 0) printf("microreflectclient: select() failed!\n");

#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
            if (FD_ISSET(STDIN_FILENO, &readSet))
            {
               if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = '\0';
               {char * ret; ret = strchr(buf, '\n'); if (ret) *ret = '\0'; }
            }
#endif

            /* Handle what the user typed in to stdin */
            if (buf[0])
            {
               if (strncmp(buf, "/msg ", 5) == 0)
               {
                  char * nextSpace = strchr(buf+5, ' '); /* after the target ID */
                  if (nextSpace)
                  {
                      *nextSpace = '\0';   
                      SendChatMessage(&gw, buf+5, nextSpace+1);
                  }
                  else printf("Can't send private /msg, no message text was specified!\n");
               }
               else if (strncmp(buf, "/nick ", 6) == 0)
               {
                  printf("Setting local user name to [%s]\n", buf+6);
                  UploadLocalUserName(&gw, buf+6);
               }
               else if (strncmp(buf, "/status ", 8) == 0)
               {
                  printf("Setting local user status to [%s]\n", buf+8);
                  UploadLocalUserStatus(&gw, buf+8);
               }
               else if (strncmp(buf, "/help", 5) == 0) printf("Available commands are:  /nick, /msg, /status, /help, and /quit\n");
               else if (strncmp(buf, "/quit", 5) == 0) keepGoing = UFalse;
               else                                    SendChatMessage(&gw, "*", buf);

               buf[0] = '\0';
            }
   
            {
               UMessage msg = {0};
               const UBool reading    = FD_ISSET(s, &readSet);
               const UBool writing    = FD_ISSET(s, &writeSet);
               const UBool writeError = ((writing)&&(UGDoOutput(&gw, ~0, SocketSendFunc, &s) < 0));
               const UBool readError  = ((reading)&&(UGDoInput( &gw, ~0, SocketRecvFunc, &s, &msg) < 0));  /* sets (msg) if one is available */
               if (UMIsMessageValid(&msg))
               {
/* printf("Heard message from server:-----------------------------------\n"); */
/* UMPrintToStream(&msg, stdout);                                             */
/* printf("-------------------------------------------------------------\n"); */
                  switch(UMGetWhatCode(&msg))
                  {
                     case NET_CLIENT_PING:  /* respond to other clients' pings */
                     {
                        const char * replyTo = UMGetString(&msg, "session", 0);
                        if (replyTo)
                        {
                           UMessage pongMsg = UGGetOutgoingMessage(&gw, NET_CLIENT_PONG);
                           if (UMIsMessageValid(&pongMsg))
                           {
                              UMAddString(&pongMsg, "session", replyTo);

                              const int replyToLen = strlen(replyTo);
                              char * keyBuf = malloc(3+replyToLen+8+1);
                              if (keyBuf)
                              {
                                 memcpy(keyBuf, "/*/", 3);
                                 memcpy(keyBuf+3, replyTo, replyToLen);
                                 memcpy(keyBuf+3+replyToLen, "/beshare\0", 8+1);
                                 UMAddString(&pongMsg, PR_NAME_KEYS, keyBuf);
                                 free(keyBuf);
                              }
                              UMAddString(&pongMsg, "version", "MUSCLE C micro chat client v" VERSION_STRING);
                              UGOutgoingMessagePrepared(&gw, &pongMsg);
                           }
                        }
                     }
                     break;

                     case NET_CLIENT_NEW_CHAT_TEXT:
                     {
                        /* Someone has sent a line of chat text to display */
                        const char * text    = UMGetString(&msg, "text", 0);
                        const char * session = UMGetString(&msg, "session", 0);
                        if ((text)&&(session))
                        {
                           const int sid = atoi(session);
                           if (strncmp(text, "/me ", 4) == 0) printf("<ACTION>: %s %s\n", GetUserName(users, sid), &text[4]); 
                                                         else printf("%s(%s): %s\n", UMGetBool(&msg, "private", 0) ? "<PRIVATE>: ":"", GetUserName(users, sid), text);
                        }
                     }
                     break;     

                     case PR_RESULT_DATAITEMS:
                     {
                        /* Look for sub-messages that indicate that nodes were removed from the tree */
                        uint32 i;
                        const char * nodepath;
                        for (i=0; ((nodepath = UMGetString(&msg, PR_NAME_REMOVED_DATAITEMS, i)) != NULL); i++)
                        {
                           const int pathDepth = GetPathDepth(nodepath);
                           if (pathDepth >= USER_NAME_DEPTH)
                           {
                              const char * sessionID = GetPathClause(SESSION_ID_DEPTH, nodepath);
                              if (sessionID)
                              {
                                 const int sid = atol(sessionID);
                                 switch(GetPathDepth(nodepath))
                                 {
                                    case USER_NAME_DEPTH:
                                       if (strncmp(GetPathClause(USER_NAME_DEPTH, nodepath), "name", 4) == 0) 
                                       {
                                          const char * userName = GetUserName(users, sid);
                                          if (userName) 
                                          {
                                             printf("User [%s] has disconnected.\n", userName);
                                             RemoveUserName(&users, sid); 
                                          }
                                       }
                                       break;
                                 }
                              }
                           }
                        }

                        /* Look for sub-messages that indicate that nodes were added to the tree */
                        {
                           UMessageFieldNameIterator iter; UMIteratorInitialize(&iter, &msg, B_MESSAGE_TYPE);
                           const char * fieldName;
                           while((fieldName = UMIteratorGetCurrentFieldName(&iter, NULL, NULL)) != NULL)
                           {
                              int pathDepth = GetPathDepth(fieldName);
                              if (pathDepth == USER_NAME_DEPTH)
                              {
                                 uint32 i = 0;
                                 while(1)
                                 {
                                    UMessage subMsg = UMGetMessage(&msg, fieldName, i);
                                    if (UMIsMessageValid(&subMsg) == false) break;

                                    const char * sessionIDStr = GetPathClause(SESSION_ID_DEPTH, fieldName);
                                    if (sessionIDStr)
                                    {
                                       const int sid = atol(sessionIDStr);
                                       switch(pathDepth)
                                       {
                                          case USER_NAME_DEPTH:
                                          {
                                             const char * nodeName = GetPathClause(USER_NAME_DEPTH, fieldName);
                                             if (strncmp(nodeName, "name", 4) == 0)
                                             {
                                                const char * userName = UMGetString(&subMsg, "name", 0);
                                                if (userName)
                                                {
                                                   if (GetUserName(users, sid) == NULL) printf("User #%i has connected\n", sid);
                                                   PutUserName(&users, sid, userName);
                                                   printf("User #%i is now known as %s\n", sid, userName);
                                                }
                                             }
                                             else if (strncmp(nodeName, "userstatus", 9) == 0)
                                             {
                                                const char * userStatus = UMGetString(&subMsg, "userstatus", 0);
                                                if (userStatus) printf("%s is now [%s]\n", GetUserName(users, sid), userStatus);
                                             }
                                          }
                                          break;
                                       }
                                    }
                                    i++;
                                 }
                              }
                              UMIteratorAdvance(&iter);
                           }
                        }
                     }
                  }
               }

               if ((readError)||(writeError))
               {
                  printf("Connection closed (%s), exiting.\n", writeError?"Write Error":"Read Error");
                  close(s);
                  keepGoing = UFalse;
               }

               if ((reading == UFalse)&&(writing == UFalse)) break;

               FD_ZERO(&readSet);
               FD_ZERO(&writeSet);
               FD_SET(s, &readSet);
               if (UGHasBytesToOutput(&gw)) FD_SET(s, &writeSet);
               FD_SET(STDIN_FILENO, &readSet);
            }
         }
      } 
      close(s);
   }
   else printf("Connection to [%s:%i] failed!\n", hostName, port);

   while(users)
   {
      struct User * next = users->_nextUser;
      free(users);
      users = next;
   }

   printf("\n\nBye!\n");

   return 0;
}
