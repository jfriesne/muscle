#include "system/ReaderWriterMutex.h"

namespace muscle {

status_t ReaderWriterMutex :: LockReadOnlyAux(uint64 optTimeoutTimestamp) const
{
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   CheckForLockingViolation("LockReadOnly");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   (void) optTimeoutTimestamp;
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   // coverity[missing_unlock : FALSE] - on error-return, lock was never locked so it doesn't need an Unlock()
   MRETURN_ON_ERROR(_stateMutex.Lock());  // can't use RAII here because we may need to Wait() below

   ThreadState * ts = _executingThreads.Get(tid);
   if (ts)
   {
      // Easy case:  we already have at least read-only access, so just increase our read-only-recursion-count and we are done
      ts->_readOnlyRecurseCount++;
      (void) _stateMutex.Unlock();
      return B_NO_ERROR;
   }
   else if (IsOkayForReaderThreadsToExecuteNow() == false)
   {
      if (optTimeoutTimestamp == 0)
      {
         // No point Wait()-ing if we know it's going to fail anyway
         (void) _stateMutex.Unlock();
         return B_TIMED_OUT;
      }

      // Oops, some other thread has the write-lock currently, so we'll have to Wait() until that thread has released it
      ts = GetOrAllocateThreadState(_waitingReaderThreads, tid, true);
      if (ts == NULL)
      {
         (void) _stateMutex.Unlock();
         return B_OUT_OF_MEMORY;
      }
      
      RefCountableWaitConditionRef tempWCRef = ts->_waitConditionRef;  // avoid race condition after we unlock _stateMutex below
      (void) _stateMutex.Unlock();   // necessary because we don't want to be holding this mutex while we Wait() for possibly a long time

      while(true)
      {
         status_t ret = tempWCRef()->_waitCondition.Wait(optTimeoutTimestamp);  // may block for a long time

         DECLARE_MUTEXGUARD(_stateMutex);  // gotta re-lock now, so we can safely update our state-tables
         if (ret.IsError())
         {
            (void) _waitingReaderThreads.Remove(tid);  // clean up!
            return ret;
         }
         else if (IsOkayForReaderThreadsToExecuteNow())  // check if we got scooped by another thread
         {
            ts = _waitingReaderThreads.Get(tid);  // necessary since (ts) could have been invalidated while _stateMutex was unlocked
            MASSERT(ts != NULL, "ReaderWriterMutex::LockReadOnlyAux():  My ThreadState has disappeared while I was waiting!");

            // Register our thread into the executing-threads table
            ThreadState * exTS = GetOrAllocateThreadState(_executingThreads, tid, false);
            if (exTS)
            {
               exTS->_readOnlyRecurseCount  = ts->_readOnlyRecurseCount+1;
               exTS->_readWriteRecurseCount = ts->_readWriteRecurseCount;
            }
            else ret = B_OUT_OF_MEMORY;

            (void) _waitingReaderThreads.Remove(tid);
            return ret;
         }
      }
   }
   else
   {
      // Nobody is currently holding the writer-lock, so we can just register and start executing immediately
      ts = GetOrAllocateThreadState(_executingThreads, tid, false);
      if (ts) ts->_readOnlyRecurseCount++;
      (void) _stateMutex.Unlock();
      return ts ? B_NO_ERROR : B_OUT_OF_MEMORY;
   }
#endif
}

status_t ReaderWriterMutex :: LockReadWriteAux(uint64 optTimeoutTimestamp) const
{
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   CheckForLockingViolation("LockReadWrite");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   (void) optTimeoutTimestamp;
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   // coverity[missing_unlock : FALSE] - on error-return, lock was never locked so doesn't need to be unlocked
   MRETURN_ON_ERROR(_stateMutex.Lock());  // can't use RAII here because we may need to Wait() below

   ThreadState * ts = _executingThreads.Get(tid);
   if (ts)
   {
      if ((ts->_readWriteRecurseCount > 0)||(_executingThreads.GetNumItems() == 1))
      {
         // Easy cases:  just increment the read-write-recurse-counts and we're done
         ts->_readWriteRecurseCount++;
         _totalReadWriteRecurseCount++;
         (void) _stateMutex.Unlock();
         return B_NO_ERROR;
      }
      else
      {
         // tricky case:  we already have read-only access and we want to upgrade to read/write access
         // but there are other read-only threads executing so we need to Wait() until they are done
         // To avoid potential deadlocks, I'm going to just release all of our read-only locks and then re-lock everything
         const uint32 readOnlyRecurseCount = ts->_readOnlyRecurseCount;

         (void) _stateMutex.Unlock();

         for (uint32 i=0; i<readOnlyRecurseCount; i++) (void) UnlockReadOnly();
         MRETURN_ON_ERROR(LockReadWriteAux(optTimeoutTimestamp));
         for (uint32 i=0; i<readOnlyRecurseCount; i++) (void) LockReadOnly();  // safe because at this point we know we're the sole writer
         return B_NO_ERROR; 
      }
   }
   else if (IsOkayForWriterThreadToExecuteNow(tid))
   {
      // Nobody is currently holding any locks, so we can just register and start executing immediately
      ts = GetOrAllocateThreadState(_executingThreads, tid, false);
      if (ts)
      {
         ts->_readWriteRecurseCount++;
         _totalReadWriteRecurseCount++;
      }

      (void) _stateMutex.Unlock();
      return ts ? B_NO_ERROR : B_OUT_OF_MEMORY;
   }
   else
   {
      if (optTimeoutTimestamp == 0)
      {
         // No point Wait()-ing if we know it's going to fail anyway
         (void) _stateMutex.Unlock();
         return B_TIMED_OUT;
      }

      // Oops, some other threads are executing, so we'll have to Wait() until they have all gone away
      ts = GetOrAllocateThreadState(_waitingWriterThreads, tid, true);
      if (ts == NULL)
      {
         (void) _stateMutex.Unlock();
         return B_OUT_OF_MEMORY;
      }

      RefCountableWaitConditionRef tempWCRef = ts->_waitConditionRef;  // avoid race condition on (ts) after we unlock _stateMutex below
      (void) _stateMutex.Unlock();   // necessary because we don't want to be holding this mutex while we Wait() for possibly a long time

      while(true)
      {
         status_t ret = tempWCRef()->_waitCondition.Wait(optTimeoutTimestamp);  // may block for a long time

         DECLARE_MUTEXGUARD(_stateMutex);  // gotta re-lock now, so we can safely update our state-tables (RAII OK because we never Wait() below)
         if (ret.IsError())
         {
            (void) _waitingWriterThreads.Remove(tid);  // clean up!
            return ret;
         }
         else if (IsOkayForWriterThreadToExecuteNow(tid))
         {
            ts = _waitingWriterThreads.GetFirstValue();  // necessary since our old (ts) might have been invalidated while (_stateMutex) was unlocked
            MASSERT(ts != NULL, "ReaderWriterMutex::LockReadWrite():  My ThreadState has disappeared while I was waiting!");

            // Register our thread into the executing-threads table
            ThreadState * exTS = GetOrAllocateThreadState(_executingThreads, tid, false);
            if (exTS)
            {
               exTS->_readOnlyRecurseCount  = ts->_readOnlyRecurseCount;
               exTS->_readWriteRecurseCount = ts->_readWriteRecurseCount+1;
               _totalReadWriteRecurseCount++;
            }
            else ret = B_OUT_OF_MEMORY;

            (void) _waitingWriterThreads.RemoveFirst();
            return ret;
         }
      }
   }
#endif
}

status_t ReaderWriterMutex :: UnlockReadOnlyAux() const
{
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   CheckForLockingViolation("UnlockReadOnly");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   DECLARE_MUTEXGUARD(_stateMutex);

   ThreadState * ts = _executingThreads.Get(tid); // threads that currently have either read-only or read/write access
   if ((ts == NULL)||(ts->_readOnlyRecurseCount == 0)) return B_LOCK_FAILED;  // can't release a read-only lock if our thread doesn't have one!

   if ((--ts->_readOnlyRecurseCount == 0)&&(ts->_readWriteRecurseCount == 0))
   {
      (void) _executingThreads.Remove(tid);  // invalidates (ts)
      if ((_totalReadWriteRecurseCount == 0)&&(_executingThreads.IsEmpty())) (void) NotifySomeWaitingThreads();
   }
   return B_NO_ERROR;
#endif
}

status_t ReaderWriterMutex :: UnlockReadWriteAux() const
{
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   CheckForLockingViolation("UnlockReadWrite");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   DECLARE_MUTEXGUARD(_stateMutex);

   ThreadState * ts = _executingThreads.Get(tid); // threads that currently have either read-only or read/write access
   if ((ts == NULL)||(ts->_readWriteRecurseCount == 0)) return B_LOCK_FAILED;  // can't release a read-write lock if our thread doesn't have one!

   MASSERT(_totalReadWriteRecurseCount > 0, "ReaderWriterMutex::UnlockReadWriteAux():  _totalReadWriteRecurseCount was already zero!?");

   if ((--ts->_readWriteRecurseCount == 0)&&(ts->_readOnlyRecurseCount == 0)) (void) _executingThreads.Remove(tid);  // invalidates (ts)
   if ((--_totalReadWriteRecurseCount == 0)&&(_executingThreads.IsEmpty())) (void) NotifySomeWaitingThreads();
   return B_NO_ERROR;
#endif
}

#ifndef MUSCLE_SINGLE_THREAD_ONLY
// Assumes _stateMutex is already locked
status_t ReaderWriterMutex :: NotifySomeWaitingThreads() const
{
   MASSERT(_totalReadWriteRecurseCount == 0, "ReaderWriterMutex::NotifySomeWaitingThreads:  _totalReadWriteRecurseCount is non-zero!");
   MASSERT(_executingThreads.IsEmpty(),      "ReaderWriterMutex::NotifySomeWaitingThreads:  some threads are still executing!");

   const bool writersWaiting = _waitingWriterThreads.HasItems();
   const bool readersWaiting = _waitingReaderThreads.HasItems();

        if ((readersWaiting)&&(writersWaiting)) return _preferWriters ? NotifyNextWriterThread() : NotifyAllReaderThreads();
   else if (readersWaiting)                     return NotifyAllReaderThreads();
   else if (writersWaiting)                     return NotifyNextWriterThread();
   else                                         return B_NO_ERROR;
}

// Assumes _stateMutex is already locked
status_t ReaderWriterMutex :: NotifyNextWriterThread() const
{
   ThreadState * ts = _waitingWriterThreads.GetFirstValue();
   RefCountableWaitCondition * wc = ts ? ts->_waitConditionRef() : NULL;
   return wc ? wc->_waitCondition.Notify() : B_BAD_OBJECT;
}

// Assumes _stateMutex is already locked
status_t ReaderWriterMutex :: NotifyAllReaderThreads() const
{
   status_t ret = B_NO_ERROR;
   for (HashtableIterator<muscle_thread_id, ThreadState> iter(_waitingReaderThreads); iter.HasData(); iter++)
   {
      RefCountableWaitCondition * wc = iter.GetValue()._waitConditionRef();
      if (wc) ret |= wc->_waitCondition.Notify();
   }
   return ret;
}

// Assumes _stateMutex is already locked
ReaderWriterMutex::ThreadState * ReaderWriterMutex :: GetOrAllocateThreadState(Hashtable<muscle_thread_id, ThreadState> & table, muscle_thread_id tid, bool okayToAllocateWC) const
{
   ThreadState * ts = table.Get(tid);
   if (ts) return ts;

   ts = table.PutAndGet(tid);
   if (ts == NULL) return NULL;

   if (okayToAllocateWC)
   {
      ts->_waitConditionRef = RefCountableWaitConditionRef(_waitConditionPool.ObtainObject());
      if (ts->_waitConditionRef() == NULL)
      {
         (void) table.Remove(tid);  // roll back!
         return NULL;
      }
   }
   return ts;
}

#endif // MUSCLE_SINGLE_THREAD_ONLY
         
} // end namespace muscle
