/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/Thread.h"
#include "system/ReaderWriterMutex.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for ParseArgs()

using namespace muscle;

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

class TestThread : public Thread, public RefCountable
{
public:
   TestThread(ReaderWriterMutex & rwMutex, bool isWriter, uint32 numIterations) : _isWriter(isWriter), _numIterations(numIterations), _rwMutex(rwMutex) {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      char desc[128];
      muscleSprintf(desc, "(%s %p)", _isWriter ? "WRITER" : "Reader", this);

      LogTime(MUSCLE_LOG_TRACE, "%s launched, starting " UINT32_FORMAT_SPEC " iterations!\n", desc, _numIterations);

      {
         DECLARE_MUTEXGUARD(_statsMutex);
         _activeThreadsCount++;
      }

      for (uint32 i=0; i<_numIterations; i++)
      {
         if (_isWriter)
         {
            LogTime(MUSCLE_LOG_TRACE, "   %s step=" UINT32_FORMAT_SPEC ":  About to lock mutex for writing!\n", desc, i);

            DECLARE_READWRITE_MUTEXGUARD(_rwMutex);

            {
               DECLARE_MUTEXGUARD(_statsMutex);
               AdjustStat(_readWriteOwnerToRecurseCount, 1);
               LogTime(MUSCLE_LOG_TRACE, "     %s: step=#" UINT32_FORMAT_SPEC "  Mutex is locked for exclusive access!\n", desc, i);
               VerifyExpectedConditions(desc, i);
            }

            (void) Snooze64(MillisToMicros(GetInsecurePseudoRandomNumber(20)));

            DECLARE_MUTEXGUARD(_statsMutex);
            AdjustStat(_readWriteOwnerToRecurseCount, -1);
         }
         else
         {
            LogTime(MUSCLE_LOG_TRACE, "   %s: step=#" UINT32_FORMAT_SPEC "  About to lock mutex for read-only!\n", desc, i);

            DECLARE_READONLY_MUTEXGUARD(_rwMutex);

            {
               DECLARE_MUTEXGUARD(_statsMutex);
               AdjustStat(_readOnlyOwnerToRecurseCount, 1);
               LogTime(MUSCLE_LOG_TRACE, "     %s: step=#" UINT32_FORMAT_SPEC "  Mutex is locked read-only!\n", desc, i);
               VerifyExpectedConditions(desc, i);
            }

            (void) Snooze64(MillisToMicros(GetInsecurePseudoRandomNumber(20)));

            DECLARE_MUTEXGUARD(_statsMutex);
            AdjustStat(_readOnlyOwnerToRecurseCount, -1);
         }

         LogTime(MUSCLE_LOG_TRACE, "   %s: step=#" UINT32_FORMAT_SPEC "  At this point, the mutex is unlocked again.\n", desc, i);
      }

      LogTime(MUSCLE_LOG_TRACE, "%s completed, exiting\n", desc);

      DECLARE_MUTEXGUARD(_statsMutex);
      _activeThreadsCount--;
   }

private:
   const bool _isWriter;
   const uint32 _numIterations;

   ReaderWriterMutex & _rwMutex;

   void VerifyExpectedConditions(const char * desc, uint32 step) const
   {
      DECLARE_MUTEXGUARD(_statsMutex);

      uint32 roCount = _readOnlyOwnerToRecurseCount.GetNumItems();
      uint32 rwCount = _readWriteOwnerToRecurseCount.GetNumItems();
      for (ConstHashtableIterator<muscle_thread_id, uint32> iter(_readWriteOwnerToRecurseCount); iter.HasData(); iter++)
      {
         const muscle_thread_id tid = iter.GetKey();
         if (_readOnlyOwnerToRecurseCount.ContainsKey(tid)) roCount--;  // if it's a read/write owner that also has a read-only lock, that's okay
      }
      LogTime(MUSCLE_LOG_DEBUG, " %s step=" UINT32_FORMAT_SPEC " roOwnersTableSize=" INT32_FORMAT_SPEC " rwOwnersTableSize=" INT32_FORMAT_SPEC "\n", desc, step, roCount, rwCount);

      switch(rwCount)
      {
         case 1:
         {
            // If someone has a read/write lock, then nobody else should have a read-only lock
            if (roCount > 0)
            {
               LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR, SOMEONE HAS A READ-ONLY LOCK SIMULTANEOUSLY WITH A READ/WRITE LOCK!?!?! (%s, roCount=" INT32_FORMAT_SPEC ")\n", desc, roCount);

               char tempBuf[20];

               printf("ReadOnlyTable:\n");
               for (ConstHashtableIterator<muscle_thread_id, uint32> iter(_readOnlyOwnerToRecurseCount); iter.HasData(); iter++) printf("  %s -> " UINT32_FORMAT_SPEC "\n", iter.GetKey().ToString(tempBuf), iter.GetValue());
               printf("ReadWriteTable:\n");
               for (ConstHashtableIterator<muscle_thread_id, uint32> iter(_readWriteOwnerToRecurseCount); iter.HasData(); iter++) printf("  %s -> " UINT32_FORMAT_SPEC "\n", iter.GetKey().ToString(tempBuf), iter.GetValue());

               MCRASH("Doh! A");
            }
         }
         break;

         case 0:
            // with no read/write locks held, any number of read-only counts is fine
         break;

         default:
            LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR, MULTIPLE READ/WRITE LOCK HOLDERS!?!?  (%s, rwCount=" INT32_FORMAT_SPEC ")\n", desc, rwCount);
            MCRASH("Doh! B");
         break;
      }
   }
};
DECLARE_REFTYPES(TestThread);

// This program tests the ReaderWriterMutex class to verify that it
// does what it is supposed to do and doesn't deadlock when used correctly.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   status_t ret;

   const uint32 numIters         = muscleMax((uint32) atoi(args.GetCstr("iterations", "1000")), (uint32)1);
   const uint32 numReaderThreads = atoi(args.GetCstr("readerthreads", "20"));
   const uint32 numWriterThreads = atoi(args.GetCstr("writerthreads", "2"));
   const uint32 numThreads       = numReaderThreads+numWriterThreads;
   const bool preferWriters      = !args.HasName("preferreaders");
   LogTime(MUSCLE_LOG_INFO, "Spawning " UINT32_FORMAT_SPEC " reader threads, " UINT32_FORMAT_SPEC " writer threads at " UINT32_FORMAT_SPEC " iterations/thread... (prefer %s)\n", numReaderThreads, numWriterThreads, numIters, preferWriters?"writers":"readers");

   ReaderWriterMutex _rwMutex("test", preferWriters);

   Queue<TestThreadRef> testThreads;
   for (uint32 i=0; i<numThreads; i++)
   {
      TestThreadRef ttRef(new TestThread(_rwMutex, i<numWriterThreads, numIters));
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
