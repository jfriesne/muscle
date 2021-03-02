/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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
      const uint32 numIterations = 25;  // enough iterations that we deadlock sometimes, but not always
      for (uint32 i=0; i<numIterations; i++)
      {
         const bool reverseOrder = ((rand()%2) == 0);
         Mutex * m1 = reverseOrder ? &_mutexB : &_mutexA;
         Mutex * m2 = reverseOrder ? &_mutexA : &_mutexB;
         if (m1->Lock().IsError()) printf("Error, couldn't lock first Mutex!  (this should never happen!)\n");
         if (m2->Lock().IsError()) printf("Error, couldn't lock second Mutex!  (this should never happen!)\n");
         if (m2->Unlock().IsError()) printf("Error, couldn't unlock second Mutex!  (this should never happen!)\n");
         if (m1->Unlock().IsError()) printf("Error, couldn't unlock first Mutex!  (this should never happen!)\n");
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
