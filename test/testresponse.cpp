#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

// This program measures the response time of a MUSCLE server.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   if (argc > 1)
   {
      ConstSocketRef s = Connect(argv[1], 2960, "testresponse");
      if (s())
      {
         Message pingMessage(PR_COMMAND_PING);  // we'll keep on sending this and seeing how long it takes to get back

         TCPSocketDataIO sockIO(s, false);

         MessageIOGateway ioGateway;
         ioGateway.SetDataIO(DataIORef(&sockIO, false));

         QueueGatewayMessageReceiver inQueue;
         uint64 lastThrowTime = 0;
         bool pingSent = false;
         uint64 min = (uint64)-1;
         uint64 max = 0;
         uint64 total = 0;
         uint64 count = 0;
         uint64 lastPrintTime = 0;

         SocketMultiplexer multiplexer;
         while(1)
         {
            if ((pingSent == false)&&(ioGateway.AddOutgoingMessage(MessageRef(&pingMessage, false)).IsOK())) 
            {
               pingSent = true;
               lastThrowTime = GetRunTime64();
            }

            const int fd = s.GetFileDescriptor();
            multiplexer.RegisterSocketForReadReady(fd);
            if (ioGateway.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(fd);
            if (multiplexer.WaitForEvents() < 0)
            {
               LogTime(MUSCLE_LOG_ERROR, "WaitForEvents() failed, aborting! [%s]\n", B_ERRNO());
               break;
            }
            if (multiplexer.IsSocketReadyForRead(fd))
            {
               if (ioGateway.DoInput(inQueue) < 0)
               {
                  LogTime(MUSCLE_LOG_ERROR, "Error reading from gateway, aborting!\n");
                  break;
               }

               MessageRef next;
               while(inQueue.RemoveHead(next).IsOK())
               {
                  if ((pingSent)&&(next()->what == PR_RESULT_PONG))
                  {
                     const uint64 result = GetRunTime64()-lastThrowTime;
                     min = muscleMin(min, result);
                     max = muscleMax(max, result);
                     total += result;
                     count++;
                     if (OnceEvery(MICROS_PER_SECOND, lastPrintTime)) LogTime(MUSCLE_LOG_INFO, "Results: min=" UINT64_FORMAT_SPEC "us max=" UINT64_FORMAT_SPEC "us avg=" UINT64_FORMAT_SPEC "us trials=" UINT64_FORMAT_SPEC "\n", min, max, total/count, count);

                     pingSent = false;  // we need to send another one now
                  }
               }
            }

            if ((multiplexer.IsSocketReadyForWrite(fd))&&(ioGateway.DoOutput() < 0))
            {
               LogTime(MUSCLE_LOG_ERROR, "Error writing to gateway, aborting!\n");
               break;
            }
         }
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "Usage: testresponse 192.168.0.150\n");
   return 0;
}
