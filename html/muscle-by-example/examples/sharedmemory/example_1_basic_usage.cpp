#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/SharedMemory.h"
#include "util/MiscUtilityFunctions.h"   // for PrintHexBytes()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a version of \"Core War\" using the SharedMemory class.\n");
   printf("\n");
   printf("It will open a SharedMemory region, and every 100mS it will lock the region for\n");  
   printf("read/write access, and write its chosen letter to a random location inside that region.\n");
   printf("\n");
   printf("Then it will unlock the region, lock it for read-only access, and print out the\n");
   printf("current contents of the region via a call to PrintHexBytes().\n");
   printf("\n");
   printf("Run multiple copies of the program simultaneously to see them fight for control of\n");
   printf("the shared memory region!\n");
   printf("\n");
}

/* This little program demonstrates basic usage of the muscle::SharedMemory class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   Snooze64(SecondsToMicros(5));  // give the user a bit of time to read the example description!

   srand(time(NULL));

   const uint32 areaSize = 64;  // bytes

   status_t ret;

   SharedMemory sm;
   if (sm.SetArea("example_1_basic_usage_shared_memory_area", areaSize, true).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't open shared memory area, aborting! [%s]\n", ret());
      return 10;
   }

   // At this point, the SharedMemory area is locked (read/write).
   if (sm.IsCreatedLocally())
   {
      LogTime(MUSCLE_LOG_INFO, "I created the Shared Memory region, so I'll initialize it to all zeros.\n");
      memset(sm(), 0, sm.GetAreaSize());
   }

   // Release our initial read/write lock here
   sm.UnlockArea();

   // Let's choose a letter to represent us
   const char myVal = 'A' + (rand()%26);

   while(true)
   {
      // Let's write to the shared memory area!
      if (sm.LockAreaReadWrite().IsOK(ret))
      {
         const uint32 offset = rand() % sm.GetAreaSize();
         printf("\nWRITING value %c to offset " UINT32_FORMAT_SPEC "\n", myVal, offset);
         sm()[offset] = myVal;
         sm.UnlockArea();
      }
      else printf("LockAreaReadWrite() failed?! [%s]\n", ret());

      // Now we'll read the area's current state and print it out
      // For this we only need a read-only lock (which won't block other readers)
      if (sm.LockAreaReadOnly().IsOK(ret))
      {
         printf("\nREADING shared memory area, its contents are as follows:\n");
         PrintHexBytes(sm(), sm.GetAreaSize());
         sm.UnlockArea();
      }
      else printf("LockAreaReadOnly() failed?! [%s]\n", ret());

      Snooze64(MillisToMicros(100));
   }

   return 0;
}
