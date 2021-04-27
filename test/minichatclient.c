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

#include "minimessage/MiniMessageGateway.h"
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

/* Creates an MMessage to send to the server when the user types in a text chat string */
static MMessage * GenerateChatMessage(const char * targetSessionID, const char * messageText)
{
   MMessage * chatMessage = MMAllocMessage(NET_CLIENT_NEW_CHAT_TEXT);
   if (chatMessage)
   {
      /* Specify which client(s) the Message should be forwarded to by muscled */
      {
         MByteBuffer ** sb = MMPutStringField(chatMessage, MFalse, PR_NAME_KEYS, 1); 
         if (sb)
         {
            char buf[1024];
            muscleSprintf(buf, "/*/%s/beshare", targetSessionID);
            sb[0] = MBStrdupByteBuffer(buf);
         }
      }

      /* Add a "session" field so that the target clients will know who the             */
      /* Message came from.  Note that muscled will automatically set this field        */
      /* to its proper value, so we don't have to; we just have to make sure it exists. */
      {
         MByteBuffer ** sb = MMPutStringField(chatMessage, MFalse, "session", 1); 
         if (sb) sb[0] = MBStrdupByteBuffer("blah");  /* will be set by server */
      }

      /* Include the chat text that the user typed in to stdin */
      {
         MByteBuffer ** sb = MMPutStringField(chatMessage, MFalse, "text", 1); 
         if (sb) sb[0] = MBStrdupByteBuffer(messageText);  /* will be set by server */
      }

      /* And lastly, if the Message wasn't meant to be a public chat message,  */
      /* include a "private" keyword to let the receiver know it's a private   */
      /* communication, so they can print it in a different color or whatever. */
      if (strcmp(targetSessionID, "*")) 
      {
         MBool * b = MMPutBoolField(chatMessage, MFalse, "private", 1);
         if (b) *b = MTrue;
      }
   }
   return chatMessage;
}

/* Creates an MMessage to send to the server to set up our subscriptions              */
/* so that we get the proper notifications when other clients connect/disconnect, etc */
static MMessage * GenerateServerSubscription(const char * subscriptionString, MBool quietly)
{
   MMessage * queryMsg = MMAllocMessage(PR_COMMAND_SETPARAMETERS);
   if (queryMsg)
   {
      /* Tell muscled what we want to subscribe to */
      {
         MBool * b = MMPutBoolField(queryMsg, MFalse, subscriptionString, 1);
         if (b) *b = MTrue;  /* the value doesn't signify anything */
      }

      if (quietly) 
      {
         MBool * b = MMPutBoolField(queryMsg, MFalse, PR_NAME_SUBSCRIBE_QUIETLY, 1);
         if (b) *b = MTrue;  /* suppress the initial-state response from muscled */
      }
   }
   return queryMsg;
}        

/* Generates a MMessage that tells the server to post some interesting information */
/* about our client, for the other clients to see.                                 */
static MMessage * GenerateSetLocalUserName(const char * name)
{
   MMessage * uploadMsg = MMAllocMessage(PR_COMMAND_SETDATA);
   if (uploadMsg)
   { 
      MMessage * nameMsg = MMAllocMessage(0);
      if (nameMsg)
      {
         {
            MByteBuffer ** sb = MMPutStringField(nameMsg, MFalse, "name", 1); 
            if (sb) sb[0] = MBStrdupByteBuffer(name);
         }

         {
            /* BeShare requires this field, even though we don't use it since we don't share files */
            int32 * pf = MMPutInt32Field(nameMsg, MFalse, "port", 1);
            if (pf) *pf = 0;
         }

         {
            MByteBuffer ** sb = MMPutStringField(nameMsg, MFalse, "version_name", 1); 
            if (sb) sb[0] = MBStrdupByteBuffer("MUSCLE C mini chat client");
         }

         {
            MByteBuffer ** sb = MMPutStringField(nameMsg, MFalse, "version_num", 1); 
            if (sb) sb[0] = MBStrdupByteBuffer(VERSION_STRING);
         }

         {
            MByteBuffer ** sb = MMPutStringField(nameMsg, MFalse, "host_os", 1); 
            if (sb) sb[0] = MBStrdupByteBuffer(GetOSName());
         }

         {
            MMessage ** mf = MMPutMessageField(uploadMsg, MFalse, "beshare/name", 1); 
            if (mf) mf[0] = nameMsg;
         }
      }
      else
      {
         MMFreeMessage(uploadMsg);
         uploadMsg = NULL;
      }
   }
   return uploadMsg;
}              

/* Generates a message to set this client's user-status on the server (e.g. "Here" or "Away") */
static MMessage * GenerateSetLocalUserStatus(const char * status)
{
   MMessage * uploadMsg = MMAllocMessage(PR_COMMAND_SETDATA);
   if (uploadMsg)
   {
      MMessage * statusMsg = MMAllocMessage(0);
      if (statusMsg)
      {
         {
            MByteBuffer ** sb = MMPutStringField(statusMsg, MFalse, "userstatus", 1); 
            if (sb) sb[0] = MBStrdupByteBuffer(status);
         }
         {
            MMessage ** mf = MMPutMessageField(uploadMsg, MFalse, "beshare/userstatus", 1); 
            if (mf) mf[0] = statusMsg;
         }
      }
      else
      {
         MMFreeMessage(uploadMsg);
         uploadMsg = NULL;
      }
   }
   return uploadMsg;
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

static MBool RemoveUserName(struct User ** users, int sid)
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
         return MTrue;
      }
      prev = cur;
      cur  = cur->_nextUser;
   }
   return MFalse;
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
/* This implementation of the client uses only the C MMessage interface, for mininal executable size. */
int main(int argc, char ** argv)
{
   const char * hostName   = "beshare.tycomsystems.com";
   const char * userName   = "miniclyde";
   const char * userStatus = "here";
   int s;
   int port = 0;
   struct User * users = NULL;  /* doubly-linked-list! */

   MMessageGateway * gw = MGAllocMessageGateway();
   if (gw == NULL)
   {
      printf("Error allocating MMessageGateway, aborting!\n");
      exit(10);
   }

#ifdef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
   printf("Warning:  This program doesn't run very well on this OS, because the OS can't select() on stdin.  You'll need to press return a lot.\n");
#endif

   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 2960;  /* default muscled port */

   s = Connect(hostName, (uint16)port);
   if (s >= 0)
   {
      MBool keepGoing = MTrue;
      fd_set readSet, writeSet;

      printf("Connection to [%s:%i] succeeded.\n", hostName, port);
      (void) fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0)|O_NONBLOCK);  /* Set s to non-blocking I/O mode */

      {
         /* Tell the server our user's name (etc) */
         MMessage * msg = GenerateSetLocalUserName(userName);
         if (msg)
         {
            MGAddOutgoingMessage(gw, msg);
            MMFreeMessage(msg);
         }
      }

      {
         /* Tell the server our status */
         MMessage * msg = GenerateSetLocalUserStatus(userStatus);
         if (msg)
         {
            MGAddOutgoingMessage(gw, msg);
            MMFreeMessage(msg);
         }
      }

      {
         /* Tell the server we want to be updated about the beshare-specific attributes of other clients */
         MMessage * msg = GenerateServerSubscription("SUBSCRIBE:beshare/*", MFalse);
         if (msg)
         {
            MGAddOutgoingMessage(gw, msg);
            MMFreeMessage(msg);
         }
      }
 
      /* the main event loop */
      while(keepGoing)
      {
         int maxfd = s;
         struct timeval * timeout = NULL;
         char buf[2048] = "";

         FD_ZERO(&readSet);
         FD_ZERO(&writeSet);
         FD_SET(s, &readSet);

         if (MGHasBytesToOutput(gw)) FD_SET(s, &writeSet);

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
            if (select(maxfd+1, &readSet, &writeSet, NULL, timeout) < 0) printf("minireflectclient: select() failed!\n");

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
               MMessage * sendMsg = NULL;
               if (strncmp(buf, "/msg ", 5) == 0)
               {
                  char * nextSpace = strchr(buf+5, ' '); /* after the target ID */
                  if (nextSpace)
                  {
                      *nextSpace = '\0';   
                      sendMsg = GenerateChatMessage(buf+5, nextSpace+1);
                  }
                  else printf("Can't send private /msg, no message text was specified!\n");
               }
               else if (strncmp(buf, "/nick ", 6) == 0)
               {
                  printf("Setting local user name to [%s]\n", buf+6);
                  sendMsg = GenerateSetLocalUserName(buf+6);
               }
               else if (strncmp(buf, "/status ", 8) == 0)
               {
                  printf("Setting local user status to [%s]\n", buf+8);
                  sendMsg = GenerateSetLocalUserStatus(buf+8);
               }
               else if (strncmp(buf, "/help", 5) == 0) printf("Available commands are:  /nick, /msg, /status, /help, and /quit\n");
               else if (strncmp(buf, "/quit", 5) == 0) keepGoing = MFalse;
               else                                    sendMsg = GenerateChatMessage("*", buf);

               if (sendMsg) 
               {
/* printf("Sending message...\n"); */
/* MMPrintToStream(sendMsg); */
                  MGAddOutgoingMessage(gw, sendMsg);
                  MMFreeMessage(sendMsg);  /* yes, we are still responsible for freeing sendMsg! */
               }
               buf[0] = '\0';
            }
   
            {
               MMessage * msg = NULL;
               const MBool reading    = FD_ISSET(s, &readSet);
               const MBool writing    = FD_ISSET(s, &writeSet);
               const MBool writeError = ((writing)&&(MGDoOutput(gw, ~0, SocketSendFunc, &s) < 0));
               const MBool readError  = ((reading)&&(MGDoInput( gw, ~0, SocketRecvFunc, &s, &msg) < 0));  /* sets (msg) if one is available */
               if (msg)
               {
/* printf("Heard message from server:-----------------------------------\n"); */
/* MMPrintToStream(msg); */
/* printf("-------------------------------------------------------------\n"); */
                  switch(MMGetWhat(msg))
                  {
                     case NET_CLIENT_PING:  /* respond to other clients' pings */
                     {
                        MByteBuffer ** sf = MMGetStringField(msg, "session", NULL);
                        if (sf)
                        {
                           const char * replyTo = (const char *) &sf[0]->bytes;
                           const int replyToLen = 3+((int)strlen(replyTo))+8+1;
                           MByteBuffer * temp = MBAllocByteBuffer(replyToLen, MTrue);

                           MMSetWhat(msg, NET_CLIENT_PONG);
                           MMRemoveField(msg, PR_NAME_KEYS);  /* out with the old address */

                           if (temp)
                           {
                              char * xbuf = (char *) &temp->bytes;
                              MByteBuffer ** sb = MMPutStringField(msg, MFalse, PR_NAME_KEYS, 1);
                              if (sb)
                              {
                                 strncat(xbuf, "/*/",      replyToLen);
                                 strncat(xbuf, replyTo,    replyToLen);
                                 strncat(xbuf, "/beshare", replyToLen);
                                 sb[0] = temp;
                              }
                              else MBFreeByteBuffer(temp);
                           }
                              
                           MMRemoveField(msg, "version");
                           {
                              MByteBuffer ** sb = MMPutStringField(msg, MFalse, "version", 1);
                              if (sb) sb[0] = MBStrdupByteBuffer("MUSCLE C mini chat client v" VERSION_STRING);
                           }
                           MGAddOutgoingMessage(gw, msg);  /* send the pong back to the server */
                        }
                     }
                     break;

                     case NET_CLIENT_NEW_CHAT_TEXT:
                     {
                        /* Someone has sent a line of chat text to display */
                        MByteBuffer ** textField    = MMGetStringField(msg, "text",    NULL);
                        MByteBuffer ** sessionField = MMGetStringField(msg, "session", NULL);
                        if ((textField)&&(sessionField))
                        {
                           const char * text = (const char *) &textField[0]->bytes; 
                           int const session = atoi((const char *)&sessionField[0]->bytes);
                           if (strncmp(text, "/me ", 4) == 0) printf("<ACTION>: %s %s\n", GetUserName(users, session), &text[4]); 
                                                         else printf("%s(%s): %s\n", MMGetBoolField(msg, "private", NULL) ? "<PRIVATE>: ":"", GetUserName(users, session), text);
                        }
                     }
                     break;     

                     case PR_RESULT_DATAITEMS:
                     {
                        /* Look for sub-messages that indicate that nodes were removed from the tree */
                        uint32 removeCount = 0;
                        uint32 i;
                        MByteBuffer ** removedField = MMGetStringField(msg, PR_NAME_REMOVED_DATAITEMS, &removeCount);
                        for (i=0; i<removeCount; i++)
                        {
                           const char * nodepath = (const char *) &removedField[i]->bytes;
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
                           MMessageIterator iter = MMGetFieldNameIterator(msg, B_MESSAGE_TYPE);
                           const char * nodepath;
                           while((nodepath = MMGetNextFieldName(&iter, NULL)) != NULL)
                           {
                              const int pathDepth = GetPathDepth(nodepath);
                              if (pathDepth == USER_NAME_DEPTH)
                              {
                                 uint32 msgCount = 0;
                                 uint32 i;
                                 MMessage ** mf = MMGetMessageField(msg, nodepath, &msgCount);
                                 for (i=0; i<msgCount; i++)
                                 {
                                    const MMessage * pmsg = mf[i];
                                    const char * sessionIDStr = GetPathClause(SESSION_ID_DEPTH, nodepath);
                                    if (sessionIDStr)
                                    {
                                       const int sid = atol(sessionIDStr);
                                       switch(pathDepth)
                                       {
                                          case USER_NAME_DEPTH:
                                          {
                                             const char * nodeName = GetPathClause(USER_NAME_DEPTH, nodepath);
                                             if (strncmp(nodeName, "name", 4) == 0)
                                             {
                                                MByteBuffer ** nf = MMGetStringField(pmsg, "name", NULL);
                                                if (nf)
                                                {
                                                   if (GetUserName(users, sid) == NULL) printf("User #%i has connected\n", sid);
                                                   PutUserName(&users, sid, (const char *) &nf[0]->bytes);
                                                   printf("User #%i is now known as %s\n", sid, (const char *) (&nf[0]->bytes));
                                                }
                                             }
                                             else if (strncmp(nodeName, "userstatus", 9) == 0)
                                             {
                                                MByteBuffer ** sf = MMGetStringField(pmsg, "userstatus", NULL);
                                                if (sf) printf("%s is now [%s]\n", GetUserName(users, sid), (const char *) &sf[0]->bytes);
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
                  }
                  MMFreeMessage(msg);
               }

               if ((readError)||(writeError))
               {
                  printf("Connection closed (%s), exiting.\n", writeError?"Write Error":"Read Error");
                  close(s);
                  keepGoing = MFalse;
               }

               if ((reading == MFalse)&&(writing == MFalse)) break;

               FD_ZERO(&readSet);
               FD_ZERO(&writeSet);
               FD_SET(s, &readSet);
               if (MGHasBytesToOutput(gw)) FD_SET(s, &writeSet);
               FD_SET(STDIN_FILENO, &readSet);
            }
         }
      } 
      close(s);
   }
   else printf("Connection to [%s:%i] failed!\n", hostName, port);

   MGFreeMessageGateway(gw);

   while(users)
   {
      struct User * next = users->_nextUser;
      free(users);
      users = next;
   }

   printf("\n\nBye!\n");

   return 0;
}
