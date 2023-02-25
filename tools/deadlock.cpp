/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "system/Thread.h"

using namespace muscle;

static Mutex _mutexA;
static Mutex _mutexB;

class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   virtual void InternalThreadEntry()
   {
      const uint32 numIterations = 5;  // enough iterations that we deadlock sometimes, but not always
      for (uint32 i=0; i<numIterations; i++)
      {
         const bool reverseOrder = ((rand()%2) == 0);
         Mutex * m1 = reverseOrder ? &_mutexB : &_mutexA;
         Mutex * m2 = reverseOrder ? &_mutexA : &_mutexB;
         DECLARE_MUTEXGUARD(*m1);  // using the macro allows the deadlock-finder to find this line-location, rather than the line in Mutex.h
         DECLARE_MUTEXGUARD(*m1);  // doing it a second time just to make sure that recursive-locking is handled as expected
         if (m2->Lock().IsError()) printf("Error, couldn't lock second Mutex!  (this should never happen!)\n");
         if (m2->Unlock().IsError()) printf("Error, couldn't unlock second Mutex!  (this should never happen!)\n");
      }
   }
};

// This program is designed to sometimes deadlock!  Compile it with -DMUSCLE_ENABLE_DEADLOCK_FINDER
// and feed its stdout output into the deadlockfinder program to see if deadlockfinder can detect
// the potential deadlock!
int main(int /*argc*/, char ** /*argv*/)
{
   CompleteSetupSystem css;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   _enableDeadlockFinderPrints = true;
#endif

   printf("Deadlocking program begins!\n");
   TestThread threads[10];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].StartInternalThread().IsError()) printf("Error, couldn't start thread #" UINT32_FORMAT_SPEC "\n", i);
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].WaitForInternalThreadToExit().IsError()) printf("Error, couldn't wait for thread #" UINT32_FORMAT_SPEC "\n", i);
   printf("Deadlocking program completed!  Lucky!\n");
   return 0;
}
