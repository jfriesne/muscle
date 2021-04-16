/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/StdinDataIO.h"
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "iogateway/RawDataMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

static AbstractMessageIOGatewayRef CreateUDPGateway(bool useTextGateway, bool useRawGateway, const DataIORef & dataIO)
{
   AbstractMessageIOGatewayRef ret;

        if (useTextGateway) ret.SetRef(new PlainTextMessageIOGateway);
   else if (useRawGateway)  ret.SetRef(new RawDataMessageIOGateway);
   else                     ret.SetRef(new MessageIOGateway);

   ret()->SetDataIO(dataIO);
   return ret;
}

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
   bool useTextGateway = args.HasName("text");
   bool useRawGateway  = args.HasName("raw");
   if (useTextGateway)
   {
      printf("Using PlainTextMessageIOGateway...\n");
      useRawGateway = false;
   }
   if (useRawGateway) printf("Using RawDataMessageIOGateway...\n");

   ConstSocketRef s = CreateUDPSocket();
   if (s() == NULL)
   {
      printf("Error creating UDP Socket!\n");
      return 10;
   }

   uint16 bindPort = atoi(bindto);
   uint16 actualPort;
   if (BindUDPSocket(s, bindPort, &actualPort).IsOK()) printf("Bound socket to port %u\n", actualPort);
                                                         else printf("Error, couldn't bind to port %u\n", bindPort);

   UDPSocketDataIORef udpIORef(new UDPSocketDataIO(s, false));
   (void) udpIORef()->SetPacketSendDestination(IPAddressAndPort(target, 3960, true));
   printf("Set UDP send destination to [%s]\n", udpIORef()->GetPacketSendDestination().ToString()());

   AbstractMessageIOGatewayRef agw = CreateUDPGateway(useTextGateway, useRawGateway, udpIORef);

   StdinDataIO stdinIO(false);
   QueueGatewayMessageReceiver stdinInQueue;
   PlainTextMessageIOGateway stdinGateway;
   stdinGateway.SetDataIO(DataIORef(&stdinIO, false));
   const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   printf("UDP Event loop starting...\n");
   while(s())
   {
      const int fd = s.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if (agw()->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
      multiplexer.RegisterSocketForReadReady(stdinFD);

      while(s())
      {
         if (multiplexer.WaitForEvents() < 0) printf("testudp: WaitForEvents() failed!\n");
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
                  const char * text = st->Cstr();
                  printf("You typed: [%s]\n", text);
                  bool send = true;
                  MessageRef ref = GetMessageFromPool(useTextGateway?PR_COMMAND_TEXT_STRINGS:(useRawGateway?PR_COMMAND_RAW_DATA:0));

                       if (useTextGateway) ref()->AddString(PR_NAME_TEXT_LINE, st->Trim());
                  else if (useRawGateway)  ref()->AddFlat(PR_NAME_DATA_CHUNKS, ParseHexBytes(text));
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
//                   ref()->PrintToStream();
                     agw()->AddOutgoingMessage(ref);
                  }
               }
            }
         }

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);
         const bool writeError = ((writing)&&(agw()->DoOutput() < 0));
         const bool readError  = ((reading)&&(agw()->DoInput(inQueue) < 0));
         if ((readError)||(writeError))
         {
            printf("%s:  Connection closed, exiting.\n", readError?"Read Error":"Write Error");
            s.Reset();
         }

         MessageRef incoming;
         while(inQueue.RemoveHead(incoming).IsOK())
         {
            IPAddressAndPort iap;
            (void) incoming()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, iap);

            printf("Incoming message from %s:-----------------------------------\n", iap.ToString()());
            incoming()->PrintToStream();
            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         multiplexer.RegisterSocketForReadReady(fd);
         if (agw()->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
         multiplexer.RegisterSocketForReadReady(stdinFD);
      }
   }

   if (agw()->HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((agw()->HasBytesToOutput())&&(agw()->DoOutput() >= 0)) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");

   return 0;
}
