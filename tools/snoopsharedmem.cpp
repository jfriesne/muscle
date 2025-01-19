/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/SetupSystem.h"
#include "system/SharedMemory.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

// This program just repeatedly prints out the contents of the SharedMemory region with the specified name
// Useful if you want to watch what some other program is doing with a region of shared memory!
int snoopsharedmemmain(const Message & args)
{
   CompleteSetupSystem css;
   uint32 maxBytesToPrint = MUSCLE_NO_LIMIT;

   const char * shmemName = args.GetCstr("region");
   if (shmemName == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Usage:  ./snoopsharedmem region=shared_memory_region_name [head=N] [clear] [delay=100mS]\n");
      return 0;
   }


   const char * maxBytesStr = args.GetCstr("head");
   const uint32 maxBytesArg = maxBytesStr ? atol(maxBytesStr) : MUSCLE_NO_LIMIT;
   if (maxBytesArg != MUSCLE_NO_LIMIT)
   {
      maxBytesToPrint = muscleMin(maxBytesToPrint, maxBytesArg);
      LogTime(MUSCLE_LOG_INFO, "Limiting printouts to the first " UINT32_FORMAT_SPEC " bytes of the shared memory area.\n", maxBytesToPrint);
   }

   const bool isClear = args.HasName("clear");
   if (isClear) LogTime(MUSCLE_LOG_INFO, "Will zero out the shared memory region after printing it\n");

   const char * delayStr = args.GetCstr("delay");
   const uint64 delayMicros = delayStr ? ParseHumanReadableTimeIntervalString(delayStr) : MillisToMicros(100);
   if (delayStr) LogTime(MUSCLE_LOG_INFO, "Using loop-delay of:  %s\n", GetHumanReadableTimeIntervalString(delayMicros)());

   status_t ret;
   SharedMemory m;
   if (m.SetArea(shmemName, 0, false).IsOK(ret))
   {
      uint8 * a = m.GetAreaPointer();
      const uint32 memSize = m.GetAreaSize();
      LogTime(MUSCLE_LOG_INFO, "Successfully attached to Shared Memory region [%s], which is located at %p and is " UINT32_FORMAT_SPEC " bytes long.\n", shmemName, a, memSize);

      while(1)
      {
         (void) Snooze64(delayMicros);
         PrintHexBytes(MUSCLE_LOG_INFO, a, muscleMin(memSize, maxBytesToPrint));
         if (isClear) memset(a, 0, memSize);
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SetArea(%s) failed, exiting! [%s]\n", shmemName, ret());

   return 0;
}

#ifndef UNIFIED_DAEMON
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   return snoopsharedmemmain(args);
}
#endif
