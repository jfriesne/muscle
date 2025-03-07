#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/Thread.h"
#include "system/ThreadLocalStorage.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates basic usage of the muscle::ThreadLocalStorage class\n");
   p.printf("Note that although each thread is accessing the same global g_perThreadVariable,\n");
   p.printf("each thread is \"seeing\" a different value from the others.\n");
   p.printf("\n");
}

static ThreadLocalStorage<int> g_perThreadVariable;

/** Thread class to demonstrate the per-thread behavior of a ThreadLocalStorage<T> object */
class MyThread : public Thread
{
public:
   MyThread() {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      int * myInt = g_perThreadVariable.GetOrCreateThreadLocalObject();
      if (myInt == NULL)
      {
         printf("Thread %p:  Couldn't get a pointer to my thread-local value!  Aborting!\n", this);
         return;
      }

      const int myVal = muscleAbs(((int)((uintptr)this))%10000);  // pick a value that is per-thread unique
      printf("Thread %p setting my g_perThreadVariable value to %i\n", this, myVal);
      *myInt = myVal;

      while(1)
      {
         printf("Thread %p:  *myInt is %i (should be %i)\n", this, *myInt, myVal);
         (void) Snooze64(SecondsToMicros(1));

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0).IsOK())  // 0 == don't block, just poll and return immediately
         {
            if (msg() == NULL) break;
         }
      }
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   const int NUM_THREADS = 5;

   MyThread threads[NUM_THREADS];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();

   (void) Snooze64(SecondsToMicros(5));

   for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();

   return 0;
}
