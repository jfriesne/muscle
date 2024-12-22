/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
   if (args.HasName("fromscript"))
   {
      printf("Called from script, skipping test!\n");
      return 0;
   }

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
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));
   const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

   QueueGatewayMessageReceiver inQueue;
   SocketMultiplexer multiplexer;
   printf("UDP Event loop starting...\n");
   while(s())
   {
      const int fd = s.GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if (agw()->HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
      (void) multiplexer.RegisterSocketForReadReady(stdinFD);

      while(s())
      {
         status_t ret;
         if (multiplexer.WaitForEvents().IsError(ret)) printf("testudp: WaitForEvents() failed! [%s]\n", ret());

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
                  const char * text = st->Cstr();
                  printf("You typed: [%s]\n", text);
                  bool send = true;
                  MessageRef ref = GetMessageFromPool(useTextGateway?PR_COMMAND_TEXT_STRINGS:(useRawGateway?PR_COMMAND_RAW_DATA:0));

                       if (useTextGateway) (void) ref()->AddString(PR_NAME_TEXT_LINE, st->Trimmed());
                  else if (useRawGateway)  (void) ref()->AddFlat(PR_NAME_DATA_CHUNKS, HexBytesFromString(text));
                  else
                  {
                     switch(text[0])
                     {
                        case 'm':
                           ref()->what = MakeWhatCode("umsg");
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                           (void) ref()->AddString("info", "This is a user message");
                        break;

                        case 's':
                           ref()->what = PR_COMMAND_SETDATA;
                           (void) ref()->AddMessage(&text[2], Message(MakeWhatCode("HELO")));
                        break;

                        case 'k':
                           ref()->what = PR_COMMAND_KICK;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 'b':
                           ref()->what = PR_COMMAND_ADDBANS;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 'B':
                           ref()->what = PR_COMMAND_REMOVEBANS;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 'g':
                           ref()->what = PR_COMMAND_GETDATA;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 'G':
                           ref()->what = PR_COMMAND_GETDATATREES;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                           (void) ref()->AddString(PR_NAME_TREE_REQUEST_ID, "Tree ID!");
                        break;

                        case 'q':
                           send = false;
                           s.Reset();
                        break;

                        case 'p':
                           ref()->what = PR_COMMAND_SETPARAMETERS;
                           (void) ref()->AddString(&text[2], "");
                        break;

                        case 'P':
                           ref()->what = PR_COMMAND_GETPARAMETERS;
                        break;

                        case 'd':
                           ref()->what = PR_COMMAND_REMOVEDATA;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 'D':
                           ref()->what = PR_COMMAND_REMOVEPARAMETERS;
                           (void) ref()->AddString(PR_NAME_KEYS, &text[2]);
                        break;

                        case 't':
                        {
                           // test all data types
                           ref()->what = 1234;
                           (void) ref()->AddString("String", "this is a string");
                           (void) ref()->AddInt8("Int8", 123);
                           (void) ref()->AddInt8("-Int8", -123);
                           (void) ref()->AddInt16("Int16", 1234);
                           (void) ref()->AddInt16("-Int16", -1234);
                           (void) ref()->AddInt32("Int32", 12345);
                           (void) ref()->AddInt32("-Int32", -12345);
                           (void) ref()->AddInt64("Int64", 123456789);
                           (void) ref()->AddInt64("-Int64", -123456789);
                           (void) ref()->AddBool("Bool", true);
                           (void) ref()->AddBool("-Bool", false);
                           (void) ref()->AddFloat("Float", 1234.56789f);
                           (void) ref()->AddFloat("-Float", -1234.56789f);
                           (void) ref()->AddDouble("Double", 1234.56789);
                           (void) ref()->AddDouble("-Double", -1234.56789);
                           (void) ref()->AddPointer("Pointer", ref());
                           (void) ref()->AddFlat("Flat", *ref());
                           char data[] = "This is some data";
                           (void) ref()->AddData("Flat", B_RAW_TYPE, data, sizeof(data));
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
                     (void) agw()->AddOutgoingMessage(ref);
                  }
               }
            }
         }

         const bool reading = multiplexer.IsSocketReadyForRead(fd);
         const bool writing = multiplexer.IsSocketReadyForWrite(fd);
         const bool writeError = ((writing)&&(agw()->DoOutput().IsError()));
         const bool readError  = ((reading)&&(agw()->DoInput(inQueue).IsError()));
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

         (void) multiplexer.RegisterSocketForReadReady(fd);
         if (agw()->HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(fd);
         (void) multiplexer.RegisterSocketForReadReady(stdinFD);
      }
   }

   if (agw()->HasBytesToOutput())
   {
      printf("Waiting for all pending messages to be sent...\n");
      while((agw()->HasBytesToOutput())&&(agw()->DoOutput().IsOK())) {printf ("."); fflush(stdout);}
   }
   printf("\n\nBye!\n");

   return 0;
}
