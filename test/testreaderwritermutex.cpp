/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/Thread.h"
#include "system/ReaderWriterMutex.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for ParseArgs()

using namespace muscle;

static ReaderWriterMutex _rwMutex;

static Mutex _statsMutex;
static uint32 _activeThreadsCount = 0;
static Hashtable<muscle_thread_id, uint32> _readOnlyOwnerToRecurseCount;
static Hashtable<muscle_thread_id, uint32> _readWriteOwnerToRecurseCount;

// Assumes _statsMutex is already locked
static void AdjustStat(Hashtable<muscle_thread_id, uint32> & table, int32 delta)
{
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   if (delta > 0)
   {
      uint32 * v = table.GetOrPut(tid, 0);
      if (v) (*v)++;
   }
   else if (delta < 0)
   {
      uint32 * v = table.Get(tid);
      if ((v==NULL)||(*v == 0)) MCRASH("Expected stat not in table!\n");
      if ((--(*v)) == 0) (void) table.Remove(tid);
   }
}

class TestThread;

// Should only be called when the calling thread has acquired a lock
// squawks if it sees any problems
static void VerifyExpectedConditions(const TestThread * caller)
{
   DECLARE_MUTEXGUARD(_statsMutex);

   uint32 roCount = _readOnlyOwnerToRecurseCount.GetNumItems();
   uint32 rwCount = _readWriteOwnerToRecurseCount.GetNumItems();
   for (HashtableIterator<muscle_thread_id, uint32> iter(_readWriteOwnerToRecurseCount); iter.HasData(); iter++)
   {
      const muscle_thread_id tid = iter.GetKey();
      if (_readOnlyOwnerToRecurseCount.ContainsKey(tid)) roCount--;  // if it's a read/write owner that also has a read-only lock, that's okay
   }
   LogTime(MUSCLE_LOG_DEBUG, " caller=%p roOwnersTableSize=" INT32_FORMAT_SPEC " rwOwnersTableSize=" INT32_FORMAT_SPEC "\n", caller, roCount, rwCount);

   switch(rwCount)
   {
      case 1:
      {
         // If someone has a read/write lock, then nobody else should have a read-only lock
         if (roCount > 0)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR, SOMEONE HAS A READ-ONLY LOCK SIMULTANEOUSLY WITH A READ/WRITE LOCK!?!?! (caller=%p, roCount=" INT32_FORMAT_SPEC ")\n", caller, roCount);

            char tempBuf[20];

            printf("ReadOnlyTable:\n");
            for (HashtableIterator<muscle_thread_id, uint32> iter(_readOnlyOwnerToRecurseCount); iter.HasData(); iter++) printf("  %s -> " UINT32_FORMAT_SPEC "\n", iter.GetKey().ToString(tempBuf), iter.GetValue());
            printf("ReadWriteTable:\n");
            for (HashtableIterator<muscle_thread_id, uint32> iter(_readWriteOwnerToRecurseCount); iter.HasData(); iter++) printf("  %s -> " UINT32_FORMAT_SPEC "\n", iter.GetKey().ToString(tempBuf), iter.GetValue());

            MCRASH("Doh! A");
         }
      }
      break;

      case 0:
         // with no read/write locks held, any number of read-only counts is fine
      break;

      default:
         LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR, MULTIPLE READ/WRITE LOCK HOLDERS!?!?  (caller=%p, rwCount=" INT32_FORMAT_SPEC ")\n", caller, rwCount);
         MCRASH("Doh! B");
      break;
   }

   (void) Snooze64(MillisToMicros(rand()%5));
}

class TestThread : public Thread, public RefCountable
{
public:
   TestThread(bool isWriter) : _isWriter(isWriter) {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      LogTime(MUSCLE_LOG_TRACE, "%s Child thread %p begins!\n", _isWriter?"WRITER":"Reader", this);

      {
         DECLARE_MUTEXGUARD(_statsMutex);
         _activeThreadsCount++;
      }

      for (int i=0; i<1000; i++)
      {
         if (_isWriter)
         {
            LogTime(MUSCLE_LOG_TRACE, "   %p:  About to lock mutex for writing!\n", this);

            DECLARE_READWRITE_MUTEXGUARD(_rwMutex);

            DECLARE_MUTEXGUARD(_statsMutex);
            AdjustStat(_readWriteOwnerToRecurseCount, 1);
            LogTime(MUSCLE_LOG_TRACE, "     %p:  Mutex is locked for exclusive access!\n", this);
            VerifyExpectedConditions(this);
            AdjustStat(_readWriteOwnerToRecurseCount, -1);
         }
         else
         {
            LogTime(MUSCLE_LOG_TRACE, "   %p:  About to lock mutex for read-only!\n", this);

            DECLARE_READONLY_MUTEXGUARD(_rwMutex);

            DECLARE_MUTEXGUARD(_statsMutex);
            AdjustStat(_readOnlyOwnerToRecurseCount, 1);
            LogTime(MUSCLE_LOG_TRACE, "     %p:  Mutex is locked read-only!\n", this);
            VerifyExpectedConditions(this);
            AdjustStat(_readOnlyOwnerToRecurseCount, -1);
         }

         LogTime(MUSCLE_LOG_TRACE, "   %p:  At this point, the mutex is unlocked again.\n", this);
      }

      LogTime(MUSCLE_LOG_TRACE, "%s Child thread %p ends!\n", _isWriter?"WRITER":"Reader", this);

      DECLARE_MUTEXGUARD(_statsMutex);
      _activeThreadsCount--;
   }

private:
   const bool _isWriter;
};
DECLARE_REFTYPES(TestThread);

// This program demonstrates running a ReflectServer event-loop in a child thread, and communicating with it from the main thread
int main(int argc, char ** argv)
{
   if ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0))
   {
      printf("Called from script, skipping test\n");
      return 0;
   }

   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   status_t ret;

   const uint32 numThreads = 5;
   LogTime(MUSCLE_LOG_INFO, "Spawning " UINT32_FORMAT_SPEC " threads...\n", numThreads);

   Queue<TestThreadRef> testThreads;
   for (uint32 i=0; i<numThreads; i++)
   {
      TestThreadRef ttRef(new TestThread((i%10)==0));
      if ((ttRef()->StartInternalThread().IsError(ret))||(testThreads.AddTail(ttRef).IsError(ret)))
      {
         LogTime(MUSCLE_LOG_ERROR, "Couldn't spawn child thread!  [%s]\n", ret());
         return 10;
      }
   }

   // hacky wait to wait until everyone has exited naturally
   while(true)
   {
      (void) Snooze64(MillisToMicros(200));

      DECLARE_MUTEXGUARD(_statsMutex);
      LogTime(MUSCLE_LOG_INFO, UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " threads are still active.\n", _activeThreadsCount, numThreads);
      if (_activeThreadsCount == 0) break;
   }

   // Make sure everyone has actually gone away
   for (uint32 i=0; i<testThreads.GetNumItems(); i++) testThreads[i]()->ShutdownInternalThread();

   printf("All child thread have exited -- main thread is exiting now -- bye!\n");
   return ret.IsOK() ? 0 : 10;
}
