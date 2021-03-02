#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/Mutex.h"
#include "system/Thread.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::Mutex class to implement a critical section.\n");
   printf("\n");
}

static Mutex _theMutex;  // will be used by all threads

class ThreadThatUsesAMutex : public Thread
{
public:
   ThreadThatUsesAMutex() {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         if (_theMutex.Lock().IsOK())
         {
            // Do some thready little task
            const int max = 10;
            for (int i=0; i<max; i++) printf("SYNCHRONIZED Thread %p:  i=%i/%i\n", this, i+1, max);
            printf("\n");

            (void) _theMutex.Unlock();
         }

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0) >= 0)  // 0 == don't wait, just poll
         {
            if (msg() == NULL) break;
         }
      }
   }
};

class ThreadWithoutMutex : public Thread
{
public:
   ThreadWithoutMutex() {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         // Do some task
         const int max = 10;
         for (int i=0; i<max; i++) printf("UNSYNCHRONIZED Thread %p:  i=%i/%i\n", this, i+1, max);
         printf("\n");

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0) >= 0)  // 0 == don't wait, just poll
         {
            if (msg() == NULL) break;
         }
      }
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int NUM_THREADS = 10;

   printf("Demonstration of a Mutex.  First we'll spawn %i threads, and have them each count to 10 repeatedly inside a Mutex....\n", NUM_THREADS);
   Snooze64(SecondsToMicros(5));

   {
      ThreadThatUsesAMutex threads[NUM_THREADS];
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
      Snooze64(SecondsToMicros(5));
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();
   }

   printf("\n");
   printf("In the above output, you should see that the output of each 1-10 count is separate from the others due to the serialization.\n");
   printf("\n");
   
   printf("Now we'll spawn %i more threads, except this time they'll execute with no Mutex.  See how the output is different!\n", NUM_THREADS);
   Snooze64(SecondsToMicros(5));
   
   {
      ThreadWithoutMutex threads[NUM_THREADS];
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
      Snooze64(SecondsToMicros(5));
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();
   }

   printf("\n");
   return 0;
}
