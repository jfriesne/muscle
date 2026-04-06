/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/MiscUtilityFunctions.h"  // for GetBytesSizeString()
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

/** Sends a stream of messages to the server, or receives them.  Prints out the average send/receive speed */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   const char * hostName = (argc>1) ? argv[1] : "localhost";
   IPAddressAndPort iap(hostName, 2960, true);

   ConstSocketRef s = Connect(iap, hostName, "bandwidthtester", false);
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
      (void) uploadMsg()->AddMessage("bandwidthtester", GetMessageFromPool());
      (void) gw.AddOutgoingMessage(uploadMsg);
   }

   // Here is a (fairly large) message that we will send repeatedly in order to bandwidth-test the server
   MessageRef sendMsgRef(GetMessageFromPool(0x666));
   (void) sendMsgRef()->AddString(PR_NAME_KEYS, "bandwidthtester");
   (void) sendMsgRef()->AddData("bandwidthtester test data", B_RAW_TYPE, NULL, 8000);

   SocketMultiplexer multiplexer;
   uint64 startTime      = GetRunTime64();
   uint64 lastPrintTime  = 0;
   uint32 tallyBytesSent = 0, tallyBytesReceived = 0;
   QueueGatewayMessageReceiver inQueue;
   while(true)
   {
      const int fd = s.GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if ((send)||(gw.HasBytesToOutput())) (void) multiplexer.RegisterSocketForWriteReady(fd);

      const uint64 printInterval = SecondsToMicros(5);
      if (OnceEvery(printInterval, lastPrintTime))
      {
         const uint64 now = GetRunTime64();
         if (tallyBytesSent > 0)
         {
            if (send) LogTime(MUSCLE_LOG_INFO, "Sending at %s/second\n", GetBytesSizeString(tallyBytesSent/((uint32)(((now-startTime))/MICROS_PER_SECOND)))());
                 else LogTime(MUSCLE_LOG_INFO, "Sent " UINT32_FORMAT_SPEC " bytes\n", tallyBytesSent);
            tallyBytesSent = 0;
         }
         if (tallyBytesReceived > 0)
         {
            if (send) LogTime(MUSCLE_LOG_INFO, "Received " UINT32_FORMAT_SPEC " bytes\n", tallyBytesReceived);
                 else LogTime(MUSCLE_LOG_INFO, "Receiving at %s/second\n", GetBytesSizeString(tallyBytesReceived/((uint32)((now-startTime)/MICROS_PER_SECOND)))());
            tallyBytesReceived = 0;
         }
         startTime = now;
      }

      status_t ret;
      if (multiplexer.WaitForEvents().IsError(ret))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "bandwidthtester: WaitForEvents() failed! [%s]\n", ret());
         break;
      }

      if ((send)&&(gw.HasBytesToOutput() == false)) for (int i=0; i<10; i++) (void) gw.AddOutgoingMessage(sendMsgRef);
      const bool  reading    = multiplexer.IsSocketReadyForRead(fd);
      const bool  writing    = multiplexer.IsSocketReadyForWrite(fd);
      const io_status_t wroteBytes = writing ? gw.DoOutput()       : io_status_t();
      const io_status_t readBytes  = reading ? gw.DoInput(inQueue) : io_status_t();
      if ((readBytes.IsError())||(wroteBytes.IsError()))
      {
         LogTime(MUSCLE_LOG_ERROR, "Connection closed, exiting (%s).\n", (readBytes.GetStatus()|wroteBytes.GetStatus())());
         break;
      }
      tallyBytesSent     += wroteBytes.GetByteCount();
      tallyBytesReceived += readBytes.GetByteCount();

      MessageRef incoming;
      while(inQueue.RemoveHead(incoming).IsOK()) {/* ignore it, we just want to measure bandwidth */}
   }
   LogTime(MUSCLE_LOG_INFO, "\n\nBye!\n");
   return 0;
}
