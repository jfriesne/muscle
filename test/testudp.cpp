/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "dataio/UDPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/RawDataMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

#define TEST(x) if ((x) != B_NO_ERROR) printf("Test failed, line %i\n",__LINE__)

// This is a text based UDP test client.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   printf("Note: MUSCLE_EXPECTED_MTU_SIZE_BYTES=%i\n", MUSCLE_EXPECTED_MTU_SIZE_BYTES);
   printf("Note: MUSCLE_IP_HEADER_SIZE_BYTES=%i\n", MUSCLE_IP_HEADER_SIZE_BYTES);
   printf("Note: MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES=%i\n", MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES);
   printf("Note: MUSCLE_UDP_HEADER_SIZE_BYTES=%i\n", (int) MUSCLE_UDP_HEADER_SIZE_BYTES);
   printf("Note: MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET=%i\n", (int) MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);

   Message args; (void) ParseArgs(argc, argv, args);
   const char * target = args.GetCstr("sendto", "localhost");
   const char * bindto = args.GetCstr("listen", "3960");
   bool useRawGateway  = args.HasName("raw");
   if (useRawGateway) printf("Using RawDataMessageIOGateway...\n");

   ConstSocketRef s = CreateUDPSocket();
   if (s() == NULL)
   {
      printf("Error creating UDP Socket!\n");
      return 10;
   }

   uint16 bindPort = atoi(bindto);
   uint16 actualPort;
   if (BindUDPSocket(s, bindPort, &actualPort) == B_NO_ERROR) printf("Bound socket to port %u\n", actualPort);
                                                         else printf("Error, couldn't bind to port %u\n", bindPort);

   MessageIOGateway gw;
   RawDataMessageIOGateway rgw;
   UDPSocketDataIO * udpIO = new UDPSocketDataIO(s, false);
   udpIO->SetSendDestination(IPAddressAndPort(target, 3960, true));
   printf("Set UDP send destination to [%s]\n", udpIO->GetSendDestination().ToString()());

   // Only one of these will actually be used
   gw.SetDataIO(DataIORef(udpIO));
   rgw.SetDataIO(DataIORef(udpIO));
   AbstractMessageIOGateway * agw = useRawGateway ? (AbstractMessageIOGateway *)&rgw : (AbstractMessageIOGateway *)&gw;

   char text[1000] = "";
   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   printf("UDP Event loop starting...\n");
   while(s())
   {
      int fd = s.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);

      if (agw->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      multiplexer.RegisterSocketForReadReady(STDIN_FILENO);

      while(s())
      {
         if (multiplexer.WaitForEvents() < 0) printf("testudp: WaitForEvents() failed!\n");
         if (multiplexer.IsSocketReadyForRead(STDIN_FILENO))
         {
            if (fgets(text, sizeof(text), stdin) == NULL) text[0] = '\0';
            char * ret = strchr(text, '\n'); if (ret) *ret = '\0';
         }

         if (text[0])
         {
            printf("You typed: [%s]\n",text);
            bool send = true;
            MessageRef ref = GetMessageFromPool(useRawGateway?PR_COMMAND_RAW_DATA:0);

            if (useRawGateway) ref()->AddFlat(PR_NAME_DATA_CHUNKS, ParseHexBytes(text));
            else
            {
               switch(text[0])
               {
                  case 'm':
                     ref()->what = MAKETYPE("umsg");
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                     ref()->AddString("info", "This is a user message");
                  break;

                  case 's':
                     ref()->what = PR_COMMAND_SETDATA;
                     ref()->AddMessage(&text[2], Message(MAKETYPE("HELO")));
                  break;

                  case 'k':
                     ref()->what = PR_COMMAND_KICK;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'b':
                     ref()->what = PR_COMMAND_ADDBANS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'B':
                     ref()->what = PR_COMMAND_REMOVEBANS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'g':
                     ref()->what = PR_COMMAND_GETDATA;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'G':
                     ref()->what = PR_COMMAND_GETDATATREES;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                     ref()->AddString(PR_NAME_TREE_REQUEST_ID, "Tree ID!");
                  break;

                  case 'q':
                     send = false;
                     s.Reset();
                  break;

                  case 'p':
                     ref()->what = PR_COMMAND_SETPARAMETERS;
                     ref()->AddString(&text[2], "");
                  break;

                  case 'P':
                     ref()->what = PR_COMMAND_GETPARAMETERS;
                  break;

                  case 'd':
                     ref()->what = PR_COMMAND_REMOVEDATA;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 'D':
                     ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                     ref()->AddString(PR_NAME_KEYS, &text[2]);
                  break;

                  case 't':
                  {
                     // test all data types
                     ref()->what = 1234;
                     ref()->AddString("String", "this is a string");
                     ref()->AddInt8("Int8", 123);
                     ref()->AddInt8("-Int8", -123);
                     ref()->AddInt16("Int16", 1234);
                     ref()->AddInt16("-Int16", -1234);
                     ref()->AddInt32("Int32", 12345);
                     ref()->AddInt32("-Int32", -12345);
                     ref()->AddInt64("Int64", 123456789);
                     ref()->AddInt64("-Int64", -123456789);
                     ref()->AddBool("Bool", true);
                     ref()->AddBool("-Bool", false);
                     ref()->AddFloat("Float", 1234.56789f);
                     ref()->AddFloat("-Float", -1234.56789f);
                     ref()->AddDouble("Double", 1234.56789);
                     ref()->AddDouble("-Double", -1234.56789);
                     ref()->AddPointer("Pointer", ref());
                     ref()->AddFlat("Flat", *ref());
                     char data[] = "This is some data";
                     ref()->AddData("Flat", B_RAW_TYPE, data, sizeof(data));
                  }
                  break;

                  default:
                     printf("Sorry, wot?\n");
                     send = false;
                  break;
               }
            }

            if (send)
            {
               printf("Sending message...\n");
//             ref()->PrintToStream();
               agw->AddOutgoingMessage(ref);
            }
            text[0] = '\0';
         }

         bool reading = multiplexer.IsSocketReadyForRead(fd);
         bool writing = multiplexer.IsSocketReadyForWrite(fd);
         bool writeError = ((writing)&&(agw->DoOutput() < 0));
         bool readError  = ((reading)&&(agw->DoInput(inQueue) < 0));
         if ((readError)||(writeError))
         {
            printf("%s:  Connection closed, exiting.\n", readError?"Read Error":"Write Error");
            s.Reset();
         }

         MessageRef incoming;
         while(inQueue.RemoveHead(incoming) == B_NO_ERROR)
         {
            printf("Heard message from server:-----------------------------------\n");
            incoming()->PrintToStream();
            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         multiplexer.RegisterSocketForReadReady(fd);
         if (agw->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
         multiplexer.RegisterSocketForReadReady(STDIN_FILENO);
      }
   }

   if (agw->HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((agw->HasBytesToOutput())&&(agw->DoOutput() >= 0)) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");

   return 0;
}
