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

   if (argc < 2)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Usage:  ./snoopsharedmem shared_memory_region_name\n");
      return 0;
   }

   const char * shmemName = argv[1];

   status_t ret;
   SharedMemory m;
   if (m.SetArea(shmemName, 0, false).IsOK(ret))
   {
      uint8 * a = m.GetAreaPointer();
      const uint32 memSize = m.GetAreaSize();
      LogTime(MUSCLE_LOG_INFO, "Successfully attached to Shared Memory region [%s], which is located at %p and is " UINT32_FORMAT_SPEC " bytes long.\n", shmemName, a, memSize);

      uint64 lastTime = 0;
      while(1)
      {
         Snooze64(MillisToMicros(100));
         PrintHexBytes(a, memSize);
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SetArea(%s) failed, exiting! [%s]\n", shmemName, ret());

   return 0;
}
