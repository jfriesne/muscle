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
// This implementation of the client uses only the C UMessage interface, for micronal executable size.
int main(int argc, char ** argv)
{
   char * hostName = "localhost";
   int port = 0;
   int s;
   uint8 inputBuffer[16*1024];
   uint8 outputBuffer[32*1024];

   UMessageGateway gw;
   UGGatewayInitialize(&gw, inputBuffer, sizeof(inputBuffer), outputBuffer, sizeof(outputBuffer));
   
   if (argc > 1) hostName = argv[1];
   if (argc > 2) port = atoi(argv[2]);
   if (port <= 0) port = 2960;

   s = Connect(hostName, (uint16)port);
   if (s >= 0)
   {
      char text[1000] = "";
      UBool keepGoing = UTrue;
      fd_set readSet, writeSet;

      printf("Connection to [%s:%i] succeeded.\n", hostName, port);
      (void) fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0)|O_NONBLOCK);  /* Set s to non-blocking I/O mode */
      while(keepGoing)
      {
         int maxfd = s;

         FD_ZERO(&readSet);
         FD_ZERO(&writeSet);
         FD_SET(s, &readSet);
         if (UGHasBytesToOutput(&gw)) FD_SET(s, &writeSet);

#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE  // Windows can't select on STDIN_FILENO :(
         if (STDIN_FILENO > maxfd) maxfd = STDIN_FILENO;
         FD_SET(STDIN_FILENO, &readSet);
#endif

         while(keepGoing) 
         {
            if (select(maxfd+1, &readSet, &writeSet, NULL, NULL) < 0) printf("microreflectclient: select() failed!\n");

#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE  // Windows can't select on STDIN_FILENO :(
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
               UBool send = UTrue;
               UMessage msg = UGGetOutgoingMessage(&gw, 0);
               if (!UMIsMessageValid(&msg))
               {
                  printf("Error allocating UMessage to send!\n");
                  break;
               }

               printf("You typed: [%s]\n",text);
               switch(text[0])
               {
                  case 'm':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMAddString(&msg, "info", "This is a user message");
                     UMSetWhatCode(&msg, MAKETYPE("umsg"));
                  break;

                  case 's':
                  {
                     UMessage subMsg = UMInlineAddMessage(&msg, &text[2], MAKETYPE("HELO"));                     
                     UMAddString(&subMsg, "test", "this is a sub message");
                     UMSetWhatCode(&msg, PR_COMMAND_SETDATA);
                  }
                  break;
      
                  case 'k':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_KICK);
                  break;

                  case 'b':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_ADDBANS);
                  break;

                  case 'B':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_REMOVEBANS);
                  break;

                  case 'g':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_GETDATA);
                  break;
      
                  case 'G':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMAddString(&msg, PR_NAME_TREE_REQUEST_ID, "Tree ID!");
                     UMSetWhatCode(&msg, PR_COMMAND_GETDATATREES);
                  break;

                  case 'q':
                     keepGoing = send = UFalse;
                  break;
      
                  case 'p':
                     UMAddString(&msg, &text[2], "");
                     UMSetWhatCode(&msg, PR_COMMAND_SETPARAMETERS);
                  break;
      
                  case 'P':
                     UMSetWhatCode(&msg, PR_COMMAND_GETPARAMETERS);
                  break;
      
                  case 'd':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_REMOVEDATA);
                  break;
      
                  case 'D':
                     UMAddString(&msg, PR_NAME_KEYS, &text[2]);
                     UMSetWhatCode(&msg, PR_COMMAND_REMOVEPARAMETERS);
                  break;
      
                  case 't':
                  {
                     const uint8 data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

                     uint8 subBuf[1024];
                     UMessage uSubMsg;
                     {
                        UMInitializeToEmptyMessage(&uSubMsg, subBuf, sizeof(subBuf), 2345);
                        UMAddString(&uSubMsg, "SubStringField", "string in the sub-UMessage");
                        UMAddInt16( &uSubMsg, "SubInt16Field", 45);
                        UMAddInt32( &uSubMsg, "SubInt32Field", 46);
                        UMAddInt64( &uSubMsg, "SubInt64Field", -12345678);
                     }

                     UMAddString( &msg, "String", "this is a string");
                     UMAddInt8(   &msg, "Int8",   8);
                     UMAddInt8(   &msg, "Int8",   9);
                     UMAddInt16(  &msg, "Int16",  16);
                     UMAddInt16(  &msg, "Int16",  17);
                     UMAddInt32(  &msg, "Int32",  32);
                     UMAddInt32(  &msg, "Int32",  33);
                     UMAddInt64(  &msg, "Int64",  64);
                     UMAddInt64(  &msg, "Int64",  65);
                     UMAddBool(   &msg, "Bool",   UTrue);
                     UMAddFloat(  &msg, "Float",  3.14159f);
                     UMAddDouble( &msg, "Double", 6.28318f);
                     UMAddMessage(&msg, "Message", uSubMsg);
                     UMAddData(   &msg, "Flat", B_RAW_TYPE, data, sizeof(data));

                     UMSetWhatCode(&msg, 1234);
                  }
                  break;

                  default:
                     printf("Sorry, wot?\n");
                     send = UFalse;
                  break;
               }
   
               if (send) 
               {
                  printf("Sending message...\n");
                  UMPrintToStream(&msg, stdout);
                  UGOutgoingMessagePrepared(&gw, &msg);
               }
               else UGOutgoingMessageCancelled(&gw, &msg);

               text[0] = '\0';
            }
   
            {
               UMessage incomingMsg; UMInitializeToInvalid(&incomingMsg);
               const UBool reading    = FD_ISSET(s, &readSet);
               const UBool writing    = FD_ISSET(s, &writeSet);
               const UBool writeError = ((writing)&&(UGDoOutput(&gw, ~0, SocketSendFunc, &s) < 0));
               const UBool readError  = ((reading)&&(UGDoInput( &gw, ~0, SocketRecvFunc, &s, &incomingMsg) < 0));

               if (UMIsMessageValid(&incomingMsg))
               {
                  printf("Heard message from server:-----------------------------------\n");
                  UMPrintToStream(&incomingMsg, stdout);
                  printf("-------------------------------------------------------------\n");
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
#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE  // Windows can't select on STDIN_FILENO :(
               FD_SET(STDIN_FILENO, &readSet);
#endif
            }
         }
      } 
      close(s);
   }
   else printf("Connection to [%s:%i] failed!\n", hostName, port);

   printf("\n\nBye!\n");

   return 0;
}
