#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/AtomicCounter.h"
#include "system/Thread.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program stress-tests an AtomicCounter by having multiple threads\n");
   printf("incrementing it and decrementing it simultaneously.\n");
   printf("\n");
   printf("After that, this program does the same thing with a plain-old-int\n");
   printf("counter to demonstrate the difference in behavior.\n");
   printf("\n");
}

// This will be modified by all threads without any synchronization (okay to do!)
static AtomicCounter _theAtomicCounter;

// This will also be used by all the threads without any synchronization (RACE CONDITION!)
// (the volatile keyword is necessary here, otherwise the C++ optimizer hides the problem)
static volatile int _nonAtomicCounter = 0;

class ThreadThatUsesAtomicCounter : public Thread
{
public:
   ThreadThatUsesAtomicCounter() {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         // Play with the atomic counter
         const int max = 100000;
         for (int i=0; i<max; i++) _theAtomicCounter.AtomicIncrement();
         for (int i=0; i<max; i++) _theAtomicCounter.AtomicDecrement();

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0) >= 0)  // 0 == don't wait, just poll
         {
            if (msg() == NULL) break;
         }
      }
   }
};

class ThreadWithoutAtomicCounter : public Thread
{
public:
   ThreadWithoutAtomicCounter() {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         // Play with the nonatomic counter
         const int max = 100000;
         for (int i=0; i<max; i++) _nonAtomicCounter++;
         for (int i=0; i<max; i++) _nonAtomicCounter--;

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0) >= 0)  // 0 == don't wait, just poll
         {
            if (msg() == NULL) break;
         }
      }
   }
};

/* This little program demonstrates basic usage of the muscle::AtomicCounter class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int NUM_THREADS = 10;

   printf("Demonstration of an AtomicCounter.  First we'll spawn %i threads, and have them all increment the AtomicCounter and then decrement it, in a loop, for 5 seconds....\n", NUM_THREADS);
   {
      ThreadThatUsesAtomicCounter threads[NUM_THREADS];
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
      Snooze64(SecondsToMicros(5));
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();
   }

   printf("After shutting down the threads, the final value of the AtomicCounter is %i (should be 0)\n", _theAtomicCounter.GetCount());
   printf("\n");

   printf("Now we'll spawn %i more threads, except this time they'll use a plain int instead of an AtomicCounter.  This introduces a race condition!\n", NUM_THREADS);
   {
      ThreadWithoutAtomicCounter threads[NUM_THREADS];
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
      Snooze64(SecondsToMicros(5));
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();
   }

   printf("After shutting down the threads, the final value of the int is %i (ideally should be 0, but likely won't be, due to the race condition!)\n", _nonAtomicCounter);
   printf("\n");

   return 0;
}
