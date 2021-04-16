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
   uint32 ret = inet_addr(name);  // first see if we can parse it as a numeric address
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

// This is a text based test client for the muscled server.  It is useful for testing
// the server, and could possibly be useful for other things, I don't know.
// This implementation of the client uses only the C MMessage interface, for mininal executable size.
int main(int argc, char ** argv)
{
   char * hostName = "localhost";
   int port = 0;
   int s;

   MMessageGateway * gw = MGAllocMessageGateway();
   if (gw == NULL)
   {
      printf("Error allocating MMessageGateway, aborting!\n");
      exit(10);
   }

   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 2960;

   s = Connect(hostName, (uint16)port);
   if (s >= 0)
   {
      char text[1000] = "";
      MBool keepGoing = MTrue;
      fd_set readSet, writeSet;

      printf("Connection to [%s:%i] succeeded.\n", hostName, port);
      (void) fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0)|O_NONBLOCK);  /* Set s to non-blocking I/O mode */
      while(keepGoing)
      {
         int maxfd = s;

         FD_ZERO(&readSet);
         FD_ZERO(&writeSet);
         FD_SET(s, &readSet);

         if (MGHasBytesToOutput(gw)) FD_SET(s, &writeSet);

#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE  // Windows doesn't support reading from STDIN_FILENO :(
         if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;
         FD_SET(STDIN_FILENO, &readSet);
#endif

         while(keepGoing) 
         {
            if (select(maxfd+1, &readSet, &writeSet, NULL, NULL) < 0) printf("minireflectclient: select() failed!\n");

#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
            if (FD_ISSET(STDIN_FILENO, &readSet))
            {
               char * ret;
               if (fgets(text, sizeof(text), stdin) == NULL) text[0] = '\0';
               ret = strchr(text, '\n'); 
               if (ret) *ret = '\0';
            }
#endif

            if (text[0])
            {
               MBool send = MTrue;
               MMessage * msg = MMAllocMessage(0);
               if (msg == NULL)
               {
                  printf("Error allocating MMessage!\n");
                  break;
               }

               printf("You typed: [%s]\n",text);

               switch(text[0])
               {
                  case 'm':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     MByteBuffer ** ib = MMPutStringField(msg, false, "info", 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     if (ib) ib[0] = MBStrdupByteBuffer("This is a user message");
                     MMSetWhat(msg, MAKETYPE("umsg"));
                  }
                  break;

                  case 's':
                  {
                     MMessage ** mb = MMPutMessageField(msg, false, &text[2], 1);
                     if (mb) mb[0] = MMAllocMessage(MAKETYPE("HELO"));
                     MMSetWhat(msg, PR_COMMAND_SETDATA);
                  }
                  break;
      
                  case 'k':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_KICK);
                  }
                  break;

                  case 'b':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_ADDBANS);
                  }
                  break;

                  case 'B':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_REMOVEBANS);
                  }
                  break;

                  case 'g':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_GETDATA);
                  }
                  break;
      
                  case 'G':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     MByteBuffer ** ib = MMPutStringField(msg, false, PR_NAME_TREE_REQUEST_ID, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     if (ib) ib[0] = MBStrdupByteBuffer("Tree ID!");
                     MMSetWhat(msg, PR_COMMAND_GETDATATREES);
                  }
                  break;

                  case 'q':
                     keepGoing = send = MFalse;
                  break;
      
                  case 'p':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, &text[2], 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer("");
                     MMSetWhat(msg, PR_COMMAND_SETPARAMETERS);
                  }
                  break;
      
                  case 'P':
                     MMSetWhat(msg, PR_COMMAND_GETPARAMETERS);
                  break;
      
                  case 'd':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_REMOVEDATA);
                  }
                  break;
      
                  case 'D':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, PR_NAME_KEYS, 1); 
                     if (sb) sb[0] = MBStrdupByteBuffer(&text[2]);
                     MMSetWhat(msg, PR_COMMAND_REMOVEPARAMETERS);
                  }
                  break;
      
                  case 't':
                  {
                     MByteBuffer ** sb = MMPutStringField(msg, false, "String", 1);
                     int8 * i8   = MMPutInt8Field(msg, false, "Int8", 2);
                     int16 * i16 = MMPutInt16Field(msg, false, "Int16", 2);
                     int32 * i32 = MMPutInt32Field(msg, false, "Int32", 2);
                     int64 * i64 = MMPutInt64Field(msg, false, "Int64", 2);
                     MBool * ib  = MMPutBoolField(msg, false, "Bool", 2);
                     float * fb  = MMPutFloatField(msg, false, "Float", 2);
                     double * db = MMPutDoubleField(msg, false, "Double", 2);
                     void ** pb  = MMPutPointerField(msg, false, "Pointer", 1);
                     MByteBuffer ** rb = MMPutDataField(msg, false, B_RAW_TYPE, "Flat", 1);
                     char data[]       = "This is some data";

                     if (sb) sb[0] = MBStrdupByteBuffer("this is a string");
                     if (i8)  {i8[0]  = 123;   i8[1]  = -123;}
                     if (i16) {i16[0] = 1234;  i16[1] = -1234;}
                     if (i32) {i32[0] = 12345; i32[1] = -12345;}
                     if (i64) {i64[0] = 123456789; i64[1] = -123456789;}
                     if (ib)  {ib[0]  = MFalse; ib[1]  = MTrue;}
                     if (fb)  {fb[0]  = 1234.56789f; fb[1] = -1234.56789f;}
                     if (db)  {db[0]  = 1234.56789;  db[1] = -1234.56789;}
                     if (pb)  {pb[0]  = NULL;}
                     if (rb)  {rb[0]  = MBStrdupByteBuffer(data);}
                     MMSetWhat(msg, 1234);
                  }
                  break;

                  default:
                     printf("Sorry, wot?\n");
                     send = MFalse;
                  break;
               }
   
               if (send) 
               {
                  printf("Sending message...\n");
                  MMPrintToStream(msg, stdout);
                  MGAddOutgoingMessage(gw, msg);
               }
               MMFreeMessage(msg);

               text[0] = '\0';
            }
   
            {
               MMessage * incomingMsg = NULL;
               const MBool reading    = FD_ISSET(s, &readSet);
               const MBool writing    = FD_ISSET(s, &writeSet);
               const MBool writeError = ((writing)&&(MGDoOutput(gw, ~0, SocketSendFunc, &s) < 0));
               const MBool readError  = ((reading)&&(MGDoInput( gw, ~0, SocketRecvFunc, &s, &incomingMsg) < 0));

               if (incomingMsg)
               {
                  printf("Heard message from server:-----------------------------------\n");
                  MMPrintToStream(incomingMsg, stdout);
                  printf("-------------------------------------------------------------\n");
                  MMFreeMessage(incomingMsg);
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
#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
               FD_SET(STDIN_FILENO, &readSet);
#endif
            }
         }
      } 
      close(s);
   }
   else printf("Connection to [%s:%i] failed!\n", hostName, port);

   MGFreeMessageGateway(gw);

   printf("\n\nBye!\n");

   return 0;
}
