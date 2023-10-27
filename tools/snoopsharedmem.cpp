/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/SetupSystem.h"
#include "system/SharedMemory.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

// This program just repeatedly prints out the contents of the SharedMemory region with the specified name
// Useful if you want to watch what some other program is doing with a region of shared memory!
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;
   uint32 maxBytesToPrint = MUSCLE_NO_LIMIT;

   if (argc < 2)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Usage:  ./snoopsharedmem shared_memory_region_name [maxBytesToPrint]\n");
      return 0;
   }

   const char * shmemName = argv[1];

   if (argc > 2)
   {
      const uint32 numBytes = atol(argv[2]);
      if (numBytes > 0)
      {
         LogTime(MUSCLE_LOG_INFO, "Limiting printouts to the first " UINT32_FORMAT_SPEC " bytes of the shared memory area.\n", numBytes);
         maxBytesToPrint = numBytes;
      }
   }

   status_t ret;
   SharedMemory m;
   if (m.SetArea(shmemName, 0, false).IsOK(ret))
   {
      uint8 * a = m.GetAreaPointer();
      const uint32 memSize = m.GetAreaSize();
      LogTime(MUSCLE_LOG_INFO, "Successfully attached to Shared Memory region [%s], which is located at %p and is " UINT32_FORMAT_SPEC " bytes long.\n", shmemName, a, memSize);

      while(1)
      {
         (void) Snooze64(MillisToMicros(100));
         PrintHexBytes(a, muscleMin(memSize, maxBytesToPrint));
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SetArea(%s) failed, exiting! [%s]\n", shmemName, ret());

   return 0;
}
