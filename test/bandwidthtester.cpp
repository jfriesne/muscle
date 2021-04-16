/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

/** Sends a stream of messages to the server, or receives them.  Prints out the average send/receive speed */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   IPAddressAndPort iap((argc>1)?argv[1]:"localhost", 2960, true);

   ConstSocketRef s = Connect(iap, argv[1], "bandwidthtester", false);
   if (s() == NULL) return 10;

   MessageIOGateway gw;
   gw.SetDataIO(DataIORef(new TCPSocketDataIO(s, false)));

   const bool send = (argc > 2)&&(strcmp(argv[2], "send") == 0);
   if (send) printf("Sending bandwidthtester messages...\n");
   else
   {
      printf("Listening for bandwidthtester messages....\n");
      // Tell the server that we are interested in receiving bandwidthtester messages
      MessageRef uploadMsg(GetMessageFromPool(PR_COMMAND_SETDATA));
      uploadMsg()->AddMessage("bandwidthtester", GetMessageFromPool());
      gw.AddOutgoingMessage(uploadMsg);
   }

   // Here is a (fairly large) message that we will send repeatedly in order to bandwidthtester the server
   MessageRef sendMsgRef(GetMessageFromPool(0x666));
   sendMsgRef()->AddString(PR_NAME_KEYS, "bandwidthtester");
   sendMsgRef()->AddData("bandwidthtester test data", B_RAW_TYPE, NULL, 8000);

   SocketMultiplexer multiplexer;
   uint64 startTime = GetRunTime64();
   struct timeval lastPrintTime = {0, 0};
   uint32 tallyBytesSent = 0, tallyBytesReceived = 0;
   QueueGatewayMessageReceiver inQueue;
   while(true)
   {
      const int fd = s.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if ((send)||(gw.HasBytesToOutput())) multiplexer.RegisterSocketForWriteReady(fd);

      const struct timeval printInterval = {5, 0};
      if (OnceEvery(printInterval, lastPrintTime))
      {
         const uint64 now = GetRunTime64();
         if (tallyBytesSent > 0) 
         {
            if (send) LogTime(MUSCLE_LOG_INFO, "Sending at " UINT32_FORMAT_SPEC " bytes/second\n", tallyBytesSent/((uint32)(((now-startTime))/MICROS_PER_SECOND)));
                 else LogTime(MUSCLE_LOG_INFO, "Sent " UINT32_FORMAT_SPEC " bytes\n", tallyBytesSent);
            tallyBytesSent = 0;
         }
         if (tallyBytesReceived > 0)
         {
            if (send) LogTime(MUSCLE_LOG_INFO, "Received " UINT32_FORMAT_SPEC " bytes\n", tallyBytesReceived);
                 else LogTime(MUSCLE_LOG_INFO, "Receiving at " UINT32_FORMAT_SPEC " bytes/second\n", tallyBytesReceived/((uint32)((now-startTime)/MICROS_PER_SECOND)));
            tallyBytesReceived = 0;
         }
         startTime = now;
      }

      if (multiplexer.WaitForEvents() < 0) LogTime(MUSCLE_LOG_CRITICALERROR, "bandwidthtester: WaitForEvents() failed! [%s]\n", B_ERRNO());
      if ((send)&&(gw.HasBytesToOutput() == false)) for (int i=0; i<10; i++) (void) gw.AddOutgoingMessage(sendMsgRef);
      const bool reading = multiplexer.IsSocketReadyForRead(fd);
      const bool writing = multiplexer.IsSocketReadyForWrite(fd);
      const int32 wroteBytes = (writing) ? gw.DoOutput() : 0;
      const int32 readBytes  = (reading) ? gw.DoInput(inQueue) : 0;
      if ((readBytes < 0)||(wroteBytes < 0))
      {
         LogTime(MUSCLE_LOG_ERROR, "Connection closed, exiting.\n");
         break;
      }
      tallyBytesSent     += wroteBytes;
      tallyBytesReceived += readBytes;

      MessageRef incoming;
      while(inQueue.RemoveHead(incoming).IsOK()) {/* ignore it, we just want to measure bandwidth */}
   }
   LogTime(MUSCLE_LOG_INFO, "\n\nBye!\n");
   return 0;
}
