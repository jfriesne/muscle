/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "system/Thread.h"
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()

using namespace muscle;

static Mutex g_mutexA;
static Mutex g_mutexB;

class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   virtual void InternalThreadEntry()
   {
      const uint32 numIterations = 5;  // enough iterations that we deadlock sometimes, but not always
      for (uint32 i=0; i<numIterations; i++)
      {
         (void) Snooze64(MillisToMicros(GetInsecurePseudoRandomNumber(200))); // space things out in time, otherwise we get deadlocks so often that it's hard to run the program to completion

         const bool reverseOrder = (GetInsecurePseudoRandomNumber(2) == 0);
         Mutex * m1 = reverseOrder ? &g_mutexB : &g_mutexA;
         Mutex * m2 = reverseOrder ? &g_mutexA : &g_mutexB;
         DECLARE_MUTEXGUARD(*m1);  // using the macro allows the deadlock-finder to find this line-location, rather than the line in Mutex.h
         DECLARE_MUTEXGUARD(*m1);  // doing it a second time just to make sure that recursive-locking is handled as expected

         const status_t r1 = m2->Lock();
         if (r1.IsError()) printf("Error, couldn't lock second Mutex!  (this should never happen!) [%s]\n", r1());

         const status_t r2 = m2->Unlock();
         if (r2.IsError()) printf("Error, couldn't unlock second Mutex!  (this should never happen!) [%s]\n", r2());
      }
   }
};

// This program is designed to sometimes deadlock!  Compile it with -DMUSCLE_ENABLE_DEADLOCK_FINDER
// and watch its output -- if it doesn't deadlock and hang, it should exit quickly and report the potential deadlock instead.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   _enableDeadlockFinderPrints = true;
#endif

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   printf("Deadlocking program begins!  This program might run to completion, or it might deadlock and hang, you never know!\n");
   TestThread threads[10];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].StartInternalThread().IsError()) printf("Error, couldn't start thread #" UINT32_FORMAT_SPEC "\n", i);
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) if (threads[i].WaitForInternalThreadToExit().IsError()) printf("Error, couldn't wait for thread #" UINT32_FORMAT_SPEC "\n", i);
   printf("Deadlocking program completed!  Lucky!\n");

   return 0;
}
