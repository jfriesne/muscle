#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/ReaderWriterMutex.h"
#include "system/Thread.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::ReaderWriterMutex class to implement shared and exclusive access to data.\n");
   printf("\n");
}

static ReaderWriterMutex g_theRWMutex;  // will be used by all threads

class ThreadThatUsesAReaderWriterMutex : public Thread
{
public:
   ThreadThatUsesAReaderWriterMutex() : _writerModeEnabled(false) {/* empty */}

   void SetWriterModeEnabled(bool writerModeEnabled) {_writerModeEnabled = true;}

protected:
   virtual void InternalThreadEntry()
   {
      while(1)
      {
         const status_t lockRet = _writerModeEnabled ? g_theRWMutex.LockReadWrite() : g_theRWMutex.LockReadOnly();
         if (lockRet.IsOK())
         {
            // Do some thready little task
            const int max = 10;
            for (int i=0; i<max; i++) printf("%s Thread %p:  i=%i/%i\n", _writerModeEnabled?"WRITER":"Reader", this, i+1, max);
            printf("\n");

            if (_writerModeEnabled) (void) g_theRWMutex.UnlockReadWrite();
                               else (void) g_theRWMutex.UnlockReadOnly();
         }

         // See if it is time for us to go away yet
         MessageRef msg;
         if (WaitForNextMessageFromOwner(msg, 0).IsOK())  // 0 == don't block, just poll and return immediately
         {
            if (msg() == NULL) break;
         }
      }

      // coverity[missing_unlock : FALSE] - We called UnlockReadWrite() or UnlockReadOnly() if necessary, above
   }

private:
   bool _writerModeEnabled;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int NUM_THREADS = 10;

   printf("Demonstration of a ReaderWriterMutex.  First we'll spawn %i threads, and have them each count to 10 repeatedly inside a ReaderWriterMutex....\n", NUM_THREADS);
   (void) Snooze64(SecondsToMicros(5));

   {
      ThreadThatUsesAReaderWriterMutex threads[NUM_THREADS];
      threads[0].SetWriterModeEnabled(true);  // let's have only the first thread use writer/exclusive locking
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
      (void) Snooze64(SecondsToMicros(5));
      for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].ShutdownInternalThread();
   }

   printf("\n");
   return 0;
}
