/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "system/ReaderWriterMutex.h"
#include "system/Thread.h"

using namespace muscle;

static ReaderWriterMutex g_mutexA;
static ReaderWriterMutex g_mutexB;

class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   virtual void InternalThreadEntry()
   {
      const uint32 numIterations = 5;  // enough iterations that we deadlock sometimes, but not always
      for (uint32 i=0; i<numIterations; i++)
      {
         (void) Snooze64(MillisToMicros(rand()%10));  // space things out in time, otherwise we get deadlocks so often that it's hard to run the program to completion

         const bool reverseOrder = ((rand()%2) == 0);
         ReaderWriterMutex * m1 = reverseOrder ? &g_mutexB : &g_mutexA;
         ReaderWriterMutex * m2 = reverseOrder ? &g_mutexA : &g_mutexB;
         DECLARE_READWRITE_MUTEXGUARD(*m1);  // using the macro allows the deadlock-finder to find this line-location, rather than the line in ReadWriteMutex.h
         DECLARE_READWRITE_MUTEXGUARD(*m1);  // doing it a second time just to make sure that recursive-locking is handled as expected
         if (m2->LockReadWrite().IsError()) printf("Error, couldn't lock second ReaderWriterMutex!  (this should never happen!)\n");
         if (m2->UnlockReadWrite().IsError()) printf("Error, couldn't unlock second ReaderWriterMutex!  (this should never happen!)\n");
      }
   }
};

// This program is designed to sometimes deadlock!  Compile it with -DMUSCLE_ENABLE_DEADLOCK_FINDER
// and watch its output -- if it doesn't deadlock and hang, it should exit quickly and report the potential deadlock instead.
int main(int /*argc*/, char ** /*argv*/)
{
   CompleteSetupSystem css;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   _enableDeadlockFinderPrints = true;
#endif

   printf("Deadlocking program begins!  This program might run to completion, or it might deadlock and hang, you never know!\n");
   TestThread threads[3];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].StartInternalThread().IsError()) printf("Error, couldn't start thread #" UINT32_FORMAT_SPEC "\n", i);
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].WaitForInternalThreadToExit().IsError()) printf("Error, couldn't wait for thread #" UINT32_FORMAT_SPEC "\n", i);
   printf("Deadlocking program completed!  Lucky!\n");
   return 0;
}
