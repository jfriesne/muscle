/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#ifdef __APPLE__
# include <sys/resource.h>
#endif

#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

// This program tests the SocketMultiplexer class by seeing how many chained socket-pairs
// we can chain a message through sequentially
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   uint32 numPairs = 5;
   if (argc > 1) numPairs = atoi(argv[1]);

   bool quiet = false;
   if ((argc > 2)&&(strcmp(argv[2], "quiet") == 0)) quiet = true;

#ifdef __APPLE__
   // Tell MacOS/X that yes, we really do want to create this many file descriptors
   struct rlimit rl;
   rl.rlim_cur = rl.rlim_max = (numPairs*2)+5;
   if (setrlimit(RLIMIT_NOFILE, &rl) != 0) perror("setrlimit");
#endif

   printf("Testing %i socket-pairs chained together...\n", numPairs);

   Queue<ConstSocketRef> senders;   (void) senders.EnsureSize(numPairs, true);
   Queue<ConstSocketRef> receivers; (void) receivers.EnsureSize(numPairs, true);
   
   for (uint32 i=0; i<numPairs; i++) 
   {
      if (CreateConnectedSocketPair(senders[i], receivers[i]).IsError())
      {
         printf("Error, failed to create socket pair #" UINT32_FORMAT_SPEC "!\n", i);
         return 10;
      }
   }

   // Start the game off
   const char c = 'C';
   if (SendData(senders[0], &c, 1, false) != 1)
   {
      printf("Error, couldn't send initial byte!\n");
      return 10;
   }

   uint64 count = 0;
   uint64 tally = 0;
   uint64 minRunTime = (uint64)-1;
   uint64 maxRunTime = 0;
   SocketMultiplexer multiplexer;
   const uint64 endTime = GetRunTime64() + SecondsToMicros(10);
   bool error = false;
   while(error==false)
   {
      for (uint32 i=0; i<numPairs; i++)
      {
         if (multiplexer.RegisterSocketForReadReady(receivers[i].GetFileDescriptor()).IsError())
         {
            printf("Error, RegisterSocketForRead() failed for receiver #" UINT32_FORMAT_SPEC "!\n", i);
            error = true;
            break;
         }
      }
      if (error) break;

      const uint64 then = GetRunTime64();
      if (then >= endTime) break;

      const int ret = multiplexer.WaitForEvents();
      if (ret < 0)
      {
         printf("WaitForEvents errored out, aborting test!\n"); 
         break;
      }

      const uint64 elapsed = GetRunTime64()-then; 
      if (quiet == false) printf("WaitForEvents returned %i after " UINT64_FORMAT_SPEC " microseconds.\n", ret, elapsed);

      count++;
      tally += elapsed;
      minRunTime = muscleMin(minRunTime, elapsed);
      maxRunTime = muscleMax(maxRunTime, elapsed);
      
      for (uint32 i=0; i<numPairs; i++)
      {
         if (multiplexer.IsSocketReadyForRead(receivers[i].GetFileDescriptor()))
         {
            char buf[64];
            const int32 numBytesReceived = ReceiveData(receivers[i], buf, sizeof(buf), false);
            if (quiet == false) printf("Receiver #" UINT32_FORMAT_SPEC " signalled ready-for-read, read " INT32_FORMAT_SPEC " bytes.\n", i, numBytesReceived);
            if (numBytesReceived > 0)
            {
               const uint32 nextIdx = (i+1)%numPairs;
               const int32 sentBytes = SendData(senders[nextIdx], buf, numBytesReceived, false);
               if (quiet == false) printf("Sent " INT32_FORMAT_SPEC " bytes on sender #" UINT32_FORMAT_SPEC "\n", sentBytes, nextIdx);
            }
         }
      }
   }
   printf("Test complete:  WaitEvents() called " UINT64_FORMAT_SPEC " times, averageTime=" UINT64_FORMAT_SPEC "uS, minimumTime=" UINT64_FORMAT_SPEC "uS, maximumTime=" UINT64_FORMAT_SPEC "uS.\n", count, tally/(count?count:1), minRunTime, maxRunTime);
   return 0;
}
