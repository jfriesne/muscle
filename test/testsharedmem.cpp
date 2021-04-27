/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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

   status_t ret;

   SharedMemory m;
   if (m.SetArea(TEST_KEY, TEST_AREA_SIZE, true).IsOK(ret))
   {
      if (deleteArea) LogTime(MUSCLE_LOG_INFO, "Deletion of area:  %s %s\n", m.GetAreaName()(), m.DeleteArea()());
      else
      {
         uint8 * a = m.GetAreaPointer();
         const uint32 s = m.GetAreaSize();

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
            if (m.LockAreaReadWrite().IsOK(ret))
            {
               const uint32 sas = m.GetAreaSize();
               if (sas > 0)
               {
                  uint8 * ap = m.GetAreaPointer();
                  {for (uint32 i=1; i<sas; i++) if (ap[i-1] != ap[i]) LogTime(MUSCLE_LOG_ERROR, "A. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
                  {for (uint32 i=0; i<sas; i++) ap[i] = base;}
                  {for (uint32 i=0; i<sas; i++) if (ap[i] != base) LogTime(MUSCLE_LOG_ERROR, "B. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
               }
               else LogTime(MUSCLE_LOG_ERROR, "Area size is zero!?\n");

               m.UnlockArea();
            }
            else 
            {
               LogTime(MUSCLE_LOG_ERROR, "Exclusive Lock failed!  (Maybe the area was deleted?)  [%s]\n", ret());
               break;
            }

            // Also test out the read-only lock
            if (m.LockAreaReadOnly().IsOK(ret))
            {
               const uint32 sas = m.GetAreaSize();
               if (s > 0)
               {
                  const uint8 * ap = m.GetAreaPointer();
                  {for (uint32 i=1; i<sas; i++) if (ap[i-1] != ap[i]) LogTime(MUSCLE_LOG_ERROR, "C. ERROR@" UINT32_FORMAT_SPEC "\n",i);}
               }
               else LogTime(MUSCLE_LOG_ERROR, "Area size is zero!?\n");

               m.UnlockArea();
            }
            else 
            {
               LogTime(MUSCLE_LOG_ERROR, "Read-Only Lock failed!  (Maybe the area was deleted?)  [%s]\n", ret());
               break;
            }

            base++;
         }
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "SetArea() failed, exiting! [%s]\n", ret());

   return 0;
}
