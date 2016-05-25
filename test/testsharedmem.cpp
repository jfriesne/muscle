/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SharedMemory.h"
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

static const char * TEST_KEY       = "testsharedmem_key";
static const uint32 TEST_AREA_SIZE = 4096;

// This program exercises the Message class.
int main(int argc, char ** argv) 
{
   bool deleteArea = ((argc > 1)&&(strncmp(argv[1], "del", 3) == 0));

   uint8 base = 0;
   LogTime(MUSCLE_LOG_INFO, deleteArea ? "Deleting shared memory area!\n" : "Beginning shared memory test!\n");

   SharedMemory m;
   if (m.SetArea(TEST_KEY, TEST_AREA_SIZE, true) == B_NO_ERROR)
   {
      if (deleteArea) LogTime(MUSCLE_LOG_INFO, "Deletion of area %s %s\n", m.GetAreaName()(), (m.DeleteArea() == B_NO_ERROR) ? "succeeded" : "failed");
      else
      {
         uint8 * a = m.GetAreaPointer();
         uint32 s = m.GetAreaSize();

         if (m.IsCreatedLocally()) 
         {
            LogTime(MUSCLE_LOG_INFO, "Created new shared memory area %s\n", m.GetAreaName()());
            for (uint32 i=0; i<s; i++) a[i] = base;
         }
         else LogTime(MUSCLE_LOG_INFO, "Found existing shared memory area %s\n", m.GetAreaName()());

         LogTime(MUSCLE_LOG_INFO, "Area is " UINT32_FORMAT_SPEC " bytes long, starting at address %p\n", s, a);

         m.UnlockArea();

         uint64 lastTime = 0;
         while(1)
         {
            if (OnceEvery(MICROS_PER_SECOND, lastTime)) LogTime(MUSCLE_LOG_INFO, "Still going... base=%u\n", base);

            // Test out the read/write exclusive lock
            if (m.LockAreaReadWrite() == B_NO_ERROR)
            {
               uint32 s = m.GetAreaSize();
               if (s > 0)
               {
                  uint8 * a = m.GetAreaPointer();
                  {for (uint32 i=1; i<s; i++) if (a[i-1] != a[i]) LogTime(MUSCLE_LOG_ERROR, "A. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
                  {for (uint32 i=0; i<s; i++) a[i] = base;}
                  {for (uint32 i=0; i<s; i++) if (a[i] != base) LogTime(MUSCLE_LOG_ERROR, "B. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
               }
               else LogTime(MUSCLE_LOG_ERROR, "Area size is zero!?\n");

               m.UnlockArea();
            }
            else 
            {
               LogTime(MUSCLE_LOG_ERROR, "Exclusive Lock failed!  Maybe the area was deleted!\n");
               break;
            }

            // Also test out the read-only lock
            if (m.LockAreaReadOnly() == B_NO_ERROR)
            {
               uint32 s = m.GetAreaSize();
               if (s > 0)
               {
                  const uint8 * a = m.GetAreaPointer();
                  {for (uint32 i=1; i<s; i++) if (a[i-1] != a[i]) LogTime(MUSCLE_LOG_ERROR, "C. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
               }
               else LogTime(MUSCLE_LOG_ERROR, "Area size is zero!?\n");

               m.UnlockArea();
            }
            else 
            {
               LogTime(MUSCLE_LOG_ERROR, "Read-Only Lock failed!  Maybe the area was deleted!\n");
               break;
            }

            base++;
         }
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SetArea() failed, exiting!\n");

   return 0;
}
