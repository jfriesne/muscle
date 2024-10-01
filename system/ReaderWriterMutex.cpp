#include "system/ReaderWriterMutex.h"

namespace muscle {

status_t ReaderWriterMutex :: LockReadOnlyAux(uint64 optTimeoutTimestamp) const
{
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   CheckForLockingViolation("LockReadOnly");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   MRETURN_ON_ERROR(_stateMutex.Lock());  // can't use RAII here because we may need to Wait() below

   ThreadState * ts = _executingThreads.Get(tid);
   if (ts)
   {
      // Easy case:  we already have at least read-only access, so just increase our read-only-recursion-count and we are done
      ts->_readOnlyRecurseCount++;
      (void) _stateMutex.Unlock();
      return B_NO_ERROR;
   }
   else if (_totalReadWriteRecurseCount > 0)
   {
      if (optTimeoutTimestamp == 0)
      {
         // No point Wait()-ing if we know it's going to fail anyway
         (void) _stateMutex.Unlock();
         return B_TIMED_OUT;
      }

      // Oops, some other thread has the write-lock currently, so we'll have to Wait() until that thread has released it
      ts = GetOrAllocateThreadState(_waitingReaderThreads, tid, RefCountableWaitConditionRef());
      if (ts)
      {
         RefCountableWaitConditionRef tempWCRef = ts->_waitConditionRef;  // avoid race condition after we unlock _stateMutex below
         (void) _stateMutex.Unlock();   // because we don't want to hold this mutex while we Wait() for possibly a long time

         status_t ret = tempWCRef()->_waitCondition.Wait(optTimeoutTimestamp);  // may block for a long time

         DECLARE_MUTEXGUARD(_stateMutex);  // gotta re-lock now, so we can safely update our state tables

         // Register our thread into the executing-threads table
         ThreadState * exTS = ret.IsOK() ? GetOrAllocateThreadState(_executingThreads, tid, tempWCRef) : NULL;
         if (exTS)
         {
            exTS->_readOnlyRecurseCount  = ts->_readOnlyRecurseCount+1;
            exTS->_readWriteRecurseCount = ts->_readWriteRecurseCount;
         }
         else ret |= B_OUT_OF_MEMORY;  // only sets ret to B_OUT_OF_MEMORY if ret didn't already indicate an error

         (void) _waitingReaderThreads.Remove(tid);
         (void) _stateMutex.Unlock();

         return ret;
      }
      else
      {
         (void) _stateMutex.Unlock();
         return B_OUT_OF_MEMORY;
      }
   }
   else
   {
      // Nobody is currently holding the writer-lock, so we can just register and start executing immediately
      ts = GetOrAllocateThreadState(_executingThreads, tid, RefCountableWaitConditionRef());
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
   return B_NO_ERROR;
#else
   const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

   MRETURN_ON_ERROR(_stateMutex.Lock());  // can't use RAII here because we may need to Wait() below

   ThreadState * ts = _executingThreads.Get(tid); // threads that currently have either read-only or read/write access
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
   else if (_executingThreads.HasItems())
   {
      if (optTimeoutTimestamp == 0)
      {
         // No point Wait()-ing if we know it's going to fail anyway
         (void) _stateMutex.Unlock();
         return B_TIMED_OUT;
      }

      // Oops, some other threads are executing, so we'll have to Wait() until they have all gone away
      ts = GetOrAllocateThreadState(_waitingWriterThreads, tid, RefCountableWaitConditionRef());
      if (ts)
      {
         RefCountableWaitConditionRef tempWCRef = ts->_waitConditionRef;  // avoid race condition after we unlock _stateMutex below
         (void) _stateMutex.Unlock();   // because we don't want to hold this mutex while we Wait() for possibly a long time

         status_t ret = tempWCRef()->_waitCondition.Wait(optTimeoutTimestamp);  // may block for a long time

         DECLARE_MUTEXGUARD(_stateMutex);  // gotta re-lock now, so we can safely update our state tables

         // Register our thread into the executing-threads table
         ThreadState * exTS = ret.IsOK() ? GetOrAllocateThreadState(_executingThreads, tid, tempWCRef) : NULL;
         if (exTS)
         {
            exTS->_readOnlyRecurseCount  = ts->_readOnlyRecurseCount;
            exTS->_readWriteRecurseCount = ts->_readWriteRecurseCount+1;
            _totalReadWriteRecurseCount++;
         }
         else ret |= B_OUT_OF_MEMORY;  // only sets ret to B_OUT_OF_MEMORY if ret didn't already indicate an error

         (void) _waitingWriterThreads.Remove(tid);
         (void) _stateMutex.Unlock();

         return ret;
      }
      else
      {
         (void) _stateMutex.Unlock();
         return B_OUT_OF_MEMORY;
      }
   }
   else
   {
      // Nobody is currently holding any locks, so we can just register and start executing immediately
      ts = GetOrAllocateThreadState(_executingThreads, tid, RefCountableWaitConditionRef());
      if (ts)
      {
         ts->_readWriteRecurseCount++;
         _totalReadWriteRecurseCount++;
      }

      (void) _stateMutex.Unlock();
      return ts ? B_NO_ERROR : B_OUT_OF_MEMORY;
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
   return B_UNIMPLEMENTED;  // TODO
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
   return B_UNIMPLEMENTED;  // TODO
#endif
}

// Assumes _stateMutex is already locked
ReaderWriterMutex::ThreadState * ReaderWriterMutex :: GetOrAllocateThreadState(Hashtable<muscle_thread_id, ThreadState> & table, muscle_thread_id tid, const RefCountableWaitConditionRef & optWCRef) const
{
   ThreadState * ts = table.Get(tid);
   if (ts) return ts;

   ts = table.PutAndGet(tid);
   if (ts == NULL) return NULL;

   ts->_waitConditionRef = optWCRef() ? optWCRef : RefCountableWaitConditionRef(_waitConditionPool.ObtainObject());
   if (ts->_waitConditionRef() == NULL)
   {
      (void) table.Remove(tid);  // roll back!
      return NULL;
   }
   return ts;
}
         
} // end namespace muscle
