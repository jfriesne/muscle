/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/SetupSystem.h"
#include "system/SharedMemory.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

// This program just deletes any SharedMemory areas it finds with the given
// names.  Good for cleanup if you've changed their sizes and don't want
// to have to reboot to deal with backwards-compatibility problems.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc < 2)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Usage:  ./erasesharedmemory [shared_memory_region_name] [..]\n");
      return 0;
   }

   for (int i=1; i<argc; i++)
   {
      const char * shmemName = argv[1];

      status_t ret;
      SharedMemory m;
      if (m.SetArea(shmemName, 0, false).IsOK(ret))
      {
         uint8 * a = m.GetAreaPointer();
         const uint32 memSize = m.GetAreaSize();
         LogTime(MUSCLE_LOG_INFO, "Successfully attached to Shared Memory region [%s], which is located at %p and is " UINT32_FORMAT_SPEC " bytes long.\n", shmemName, a, memSize);

         if (m.DeleteArea().IsOK(ret)) LogTime(MUSCLE_LOG_INFO, "Deleted Shared Memory region [%s]\n", shmemName);
                                  else LogTime(MUSCLE_LOG_INFO, "Error, couldn't delete Shared Memory region [%s] [%s]\n", shmemName, ret());
      }
      else LogTime(MUSCLE_LOG_ERROR, "SetArea(%s) failed! [%s]\n", shmemName, ret());
   }

   return 0;
}
