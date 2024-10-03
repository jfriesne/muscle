/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleReaderWriterMutex_h
#define MuscleReaderWriterMutex_h

#ifndef MUSCLE_SINGLE_THREAD_ONLY
# include "util/Hashtable.h"
# include "util/ObjectPool.h"
# include "system/Mutex.h"   // for the deadlock-finder stuff
# include "system/WaitCondition.h"
#endif

#include "support/NotCopyable.h"
#include "util/TimeUtilityFunctions.h"  // for MUSCLE_TIME_NEVER

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# include "util/overloaded_preprocessor_macro.h"
#endif

#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
// Should be defined elsewhere to return true iff it's considered okay to call the given method on the given
// ReaderWriterMutex from the current thread-context.
namespace muscle {class ReaderWriterMutex;}
extern bool IsOkayToAccessMuscleReaderWriterMutex(const muscle::ReaderWriterMutex * m, const char * methodName);
#endif

namespace muscle {

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define LockReadOnly(...)       OVERLOADED_PREPROCESSOR_MACRO(LockReadOnly, __VA_ARGS__)
# define LockReadOnly0()         DeadlockFinderLockReadOnlyWrapper(__FILE__, __LINE__, MUSCLE_TIME_NEVER)
# define LockReadOnly1(ts)       DeadlockFinderLockReadOnlyWrapper(__FILE__, __LINE__, ts)
# define LockReadWrite(...)      OVERLOADED_PREPROCESSOR_MACRO(LockReadWrite, __VA_ARGS__)
# define LockReadWrite0()        DeadlockFinderLockReadWriteWrapper(__FILE__, __LINE__, MUSCLE_TIME_NEVER)
# define LockReadWrite1(ts)      DeadlockFinderLockReadWriteWrapper(__FILE__, __LINE__, ts)
# define UnlockReadOnly()        DeadlockFinderUnlockReadOnlyWrapper  (__FILE__, __LINE__)
# define UnlockReadWrite()       DeadlockFinderUnlockReadWriteWrapper (__FILE__, __LINE__)
#endif

class ReadOnlyMutexGuard;  // forward declaration
class ReadWriteMutexGuard;  // forward declaration

/** This class is a platform-independent API for a recursive reader/writer lock (a.k.a mutex).
  * Typically used to serialize the execution of critical sections in a multithreaded API
  * This class allows multiple threads to hold the read-only lock simultaneously, but guarantees that
  * only one thread can hold the read/write lock * at any given time (and that no read-only threads
  * will hold any lock while a thread holds the read/write lock).  When compiling with the
  * MUSCLE_SINGLE_THREAD_ONLY preprocessor flag defined, this class becomes a no-op.
  */
class ReaderWriterMutex MUSCLE_FINAL_CLASS : public NotCopyable
{
public:
   /** Constructor
     * @param preferWriters if true, and we have a choice between waking up a blocked writer-thread or
     *                      waking up one or more blocked reader-threads, we'll wake up the writer-thread.
     *                      If false, we'll wake up the reader-threads instead.  Defaults to true.
     */
   ReaderWriterMutex(bool preferWriters = true)
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      : _preferWriters(preferWriters)
      , _totalReadWriteRecurseCount(0)
#endif
   {
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      (void) preferWriters;
#endif
   }

   /** Destructor.  If a ReaderWriterMutex is destroyed while another thread is blocking in its LockReadOnly()
     * or LockReadWrite() methods, the results are undefined.
     */
   ~ReaderWriterMutex() {/* empty */}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderLockReadOnlyWrapper(const char * fileName, int fileLine, uint64 optTimeoutAt) const
#else
   /** Attempts to lock the mutex for shared/read-only access.
     * Any thread that tries to LockReadOnly() this object while it is already locked-for-read+write access
     * by another thread will block until the writer-thread unlocks the lock.  The lock is recursive, however;
     * if a given thread calls LockReadOnly() twice in a row it won't deadlock itself (although it will
     * need to call UnlockReadOnly() twice in a row in order to truly unlock the lock)
     * @param optTimeoutAt timestamp at which we should give up and return B_TIMED_OUT if we still haven't
     *                     been able to acquire the lock.  Pass 0 for a guaranteed non-blocking/instantaneous call,
     *                     or a timestamp value (e.g. GetRunTime64()+SecondsToMicros(1)) for a finite timeout,
     *                     or pass MUSCLE_TIME_NEVER (the default value) to have no timeout.
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked for some reason.
     */
   status_t LockReadOnly(uint64 optTimeoutAt = MUSCLE_TIME_NEVER) const
#endif
   {
      const status_t ret = LockReadOnlyAux(optTimeoutAt);
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent((optTimeoutAt==MUSCLE_TIME_NEVER)?LOCK_ACTION_LOCK:LOCK_ACTION_TRYLOCK, fileName, fileLine);
#endif
      return ret;
   }

   /** Attempts to lock the mutex for shared/read-only access without blocking.
     * If access cannot be obtained immediately, then this call will return B_TIMED_OUT right away.
     * @note calling this method is equivalent to calling LockReadOnly(0)
     */
   status_t TryLockReadOnly() const {return LockReadOnly(0);}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderLockReadWriteWrapper(const char * fileName, int fileLine, uint64 optTimeoutAt) const
#else
   /** Attempts to lock the lock for exclusive/read-write access.
     * Any thread that tries to LockReadWrite() this object while it is already locked by another thread
     * until after all threads have unlocked the lock.  The lock is recursive, however;
     * if a given thread calls LockReadWrite() twice in a row it won't deadlock itself (although it will
     * need to call UnlockReadWrite() twice in a row in order to truly unlock the lock)
     * @param optTimeoutAt timestamp at which we should give up and return B_TIMED_OUT if we still haven't
     *                     been able to acquire the lock.  Pass 0 for a guaranteed non-blocking/instantaneous call,
     *                     or a timestamp value (e.g. GetRunTime64()+SecondsToMicros(1)) for a finite timeout,
     *                     or pass MUSCLE_TIME_NEVER (the default value) to have no timeout.
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked for some reason.
     */
   status_t LockReadWrite(uint64 optTimeoutAt = MUSCLE_TIME_NEVER) const
#endif
   {
      const status_t ret = LockReadWriteAux(optTimeoutAt);
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent((optTimeoutAt==MUSCLE_TIME_NEVER)?LOCK_ACTION_LOCK:LOCK_ACTION_TRYLOCK, fileName, fileLine);
#endif
      return ret;
   }

   /** Attempts to lock the mutex for exclusive/read-write access without blocking.
     * If access cannot be obtained immediately, then this call will return B_TIMED_OUT right away.
     * @note calling this method is equivalent to calling LockReadWrite(0)
     */
   status_t TryLockReadWrite() const {return LockReadWrite(0);}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderUnlockReadOnlyWrapper(const char * fileName, int fileLine) const
#else
   /** Unlocks the a read-only-locked lock.  Once this is done, any other thread that is blocked in the Lock()
     * methods will gain ownership of the lock and return.
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED on failure (perhaps you tried to unlock a lock
     *          that you didn't currently have locked-read-only?  This method should never fail in typical usage)
     */
   status_t UnlockReadOnly() const
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging while we are still are locked, otherwise our counter can suffer from race conditions
      LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK, fileName, fileLine);
#endif

      return UnlockReadOnlyAux();
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderUnlockReadWriteWrapper(const char * fileName, int fileLine) const
#else
   /** Unlocks the a read-write-locked lock.  Once this is done, any other thread that is blocked in the Lock()
     * methods will gain ownership of the lock and return.
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED on failure (perhaps you tried to unlock a lock
     *          that you didn't currently have locked-read-write?  This method should never fail in typical usage)
     */
   status_t UnlockReadWrite() const
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging while we are still are locked, otherwise our counter can suffer from race conditions
      LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK, fileName, fileLine);
#endif

      return UnlockReadWriteAux();
   }

   /** If MUSCLE_ENABLE_DEADLOCK_FINDER is defined, this method disables mutex-callback-logging on this ReaderWriterMutex,
     * and returns true on the outermost nested call (ie if we've just entered the disabled-logging state).
     * Otherwise this method is a no-op and returns false.
     */
   bool BeginAvoidFindDeadlockCallbacks()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      return _inDeadlockFinderCallback.Increment();
#else
      return false;
#endif
   }

   /** If MUSCLE_ENABLE_DEADLOCK_FINDER is defined, this method re-enables mutex-callback-logging on this ReaderWriterMutex,
     * and returns true on the outermost nested call (ie if we've just exited the disabled-logging state).
     * Otherwise this method is a no-op and returns false.
     */
   bool EndAvoidFindDeadlockCallbacks()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      return _inDeadlockFinderCallback.Decrement();
#else
      return false;
#endif
   }

private:
   friend class ReadOnlyMutexGuard;
   friend class ReadWriteMutexGuard;

   void Cleanup();
   
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   void CheckForLockingViolation(const char * methodName) const
   {
      if (::IsOkayToAccessMuscleReaderWriterMutex(this, methodName) == false) printf("ReaderWriterMutex(%p)::%s:  Locking violation!\n", this, methodName);
   }
#endif

   status_t LockReadOnlyAux(uint64 optTimeoutTimestamp) const;
   status_t LockReadWriteAux(uint64 optTimeoutTimestamp) const;
   status_t UnlockReadOnlyAux() const;
   status_t UnlockReadWriteAux() const;

   status_t NotifySomeWaitingThreads() const;
   status_t NotifyNextWriterThread() const;
   status_t NotifyAllReaderThreads() const;

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   // Assumes _stateMutex is already locked
   bool IsOkayForReaderThreadsToExecuteNow() const {return ((_totalReadWriteRecurseCount == 0)&&((_preferWriters == false)||(_waitingWriterThreads.IsEmpty())));}
   bool IsOkayForWriterThreadToExecuteNow(muscle_thread_id tid) const {return ((_executingThreads.IsEmpty())&&((_waitingWriterThreads.IsEmpty())||((*_waitingWriterThreads.GetFirstKey() == tid))));}
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   void LogDeadlockFinderEvent(uint32 lockActionType, const char * fileName, int fileLine) const
   {
      if ((_enableDeadlockFinderPrints)&&(!_inDeadlockFinderCallback.IsInBatch()))
      {
         NestCountGuard ncg(_inDeadlockFinderCallback);
         DeadlockFinder_LogEvent(lockActionType, this, fileName, fileLine);
      }
   }

   mutable NestCount _inDeadlockFinderCallback;
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   class RefCountableWaitCondition : public RefCountable
   {
   public:
      RefCountableWaitCondition() {/* empty */}

      RefCountableWaitCondition & operator = (const RefCountableWaitCondition &) {/* deliberate no-op! */ return *this;}

      WaitCondition _waitCondition;
   };
   DECLARE_REFTYPES(RefCountableWaitCondition);

   class ThreadState
   {
   public:
      ThreadState() : _readOnlyRecurseCount(0), _readWriteRecurseCount(0) {/* empty */}

      uint32 _readOnlyRecurseCount;   // how many times this thread has already locked the read-only lock
      uint32 _readWriteRecurseCount;  // how many times this thread has already locked the read-only lock
      RefCountableWaitConditionRef _waitConditionRef;  // used to wake up the sleeping thread when it's time for him to run again
   };

   // Assumes _stateMutex is already locked
   ThreadState * GetOrAllocateThreadState(Hashtable<muscle_thread_id, ThreadState> & table, muscle_thread_id tid, bool okayToAllocateWC) const;

   // experimental
   Mutex _stateMutex;   // serialize access to our member-variables below

   mutable ObjectPool<RefCountableWaitCondition> _waitConditionPool;

   mutable Hashtable<muscle_thread_id, ThreadState> _waitingReaderThreads;  // threads waiting for read-only access
   mutable Hashtable<muscle_thread_id, ThreadState> _waitingWriterThreads;  // threads waiting for read/write access

   const bool _preferWriters;
   mutable uint32 _totalReadWriteRecurseCount;   // current sum of all _readWriteRecurseCount values

   mutable Hashtable<muscle_thread_id, ThreadState> _executingThreads; // threads that currently have either read-only or read/write access
#endif
};

/** This convenience class can be used to automatically lock/unlock a ReaderWriterMutex based on the ReadOnlyMutexGuard's ctor/dtor.
  * @note it's safer to use the DECLARE_READONLY_MUTEXGUARD(theReaderWriterMutex) macro rather than manually placing a ReadOnlyMutexGuard object
  *       onto the stack, since that avoids any possibility of forgetting to give the ReadOnlyMutexGuard stack-object a name
  *       (eg typing "ReadOnlyMutexGuard(myReaderWriterMutex);" rather than "ReadOnlyMutexGuard mg(myReaderWriterMutex);", would introduce a perniciously
  *       non-obvious run-time error where your ReaderWriterMutex only gets locked momentarily rather than until the end-of-scope)
  *       Using the DECLARE_READONLY_MUTEXGUARD(theReaderWriterMutex) macro will also allow MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or via -DMUSCLE_ENABLE_DEADLOCK_FINDER as compiler argument)
  *       to give you more useful debugging information about deadlocks that could happen (even if they didn't happen this time)
  */
class MUSCLE_NODISCARD ReadOnlyMutexGuard MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  Locks the specified ReaderWriterMutex for read-only/shared access.
     * @param m The ReaderWriterMutex to on which we will call LockReadOnly() and UnlockReadOnly().
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   ReadOnlyMutexGuard(const ReaderWriterMutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(m), _optFileName(optFileName), _fileLine(fileLine)
#else
   ReadOnlyMutexGuard(const ReaderWriterMutex & m) : _mutex(m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      if (_mutex.LockReadOnlyAux(MUSCLE_TIME_NEVER).IsError()) MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex LockReadOnly() failed!\n");
      _mutex.LogDeadlockFinderEvent(LOCK_ACTION_LOCK, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);  // must be called while the ReaderWriterMutex is locked
#else
      if (_mutex.LockReadOnly().IsError())    MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex LockReadOnly() failed!\n");
#endif
   }

   /** Destructor.  Unlocks the ReaderWriterMutex previously specified in the constructor. */
   ~ReadOnlyMutexGuard()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _mutex.LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__); // must be called while the ReaderWriterMutex is locked
      if (_mutex.UnlockReadOnlyAux().IsError()) MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex UnlockReadOnly() failed!\n");
#else
      if (_mutex.UnlockReadOnly().IsError())    MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex UnlockReadOnly() failed!\n");
#endif
   }

private:
   ReadOnlyMutexGuard(const ReadOnlyMutexGuard &);  // copy ctor, deliberately inaccessible

   const ReaderWriterMutex & _mutex;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   const char * _optFileName;
   const int _fileLine;
#endif
};

/** This convenience class can be used to automatically lock/unlock a ReaderWriterMutex based on the ReadWriteMutexGuard's ctor/dtor.
  * @note it's safer to use the DECLARE_READONLY_MUTEXGUARD(theReaderWriterMutex) macro rather than manually placing a ReadWriteMutexGuard object
  *       onto the stack, since that avoids any possibility of forgetting to give the ReadWriteMutexGuard stack-object a name
  *       (eg typing "ReadWriteMutexGuard(myReaderWriterMutex);" rather than "ReadWriteMutexGuard mg(myReaderWriterMutex);", would introduce a perniciously
  *       non-obvious run-time error where your ReaderWriterMutex only gets locked momentarily rather than until the end-of-scope)
  *       Using the DECLARE_READONLY_MUTEXGUARD(theReaderWriterMutex) macro will also allow MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or via -DMUSCLE_ENABLE_DEADLOCK_FINDER as compiler argument)
  *       to give you more useful debugging information about deadlocks that could happen (even if they didn't happen this time)
  */
class MUSCLE_NODISCARD ReadWriteMutexGuard MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  Locks the specified ReaderWriterMutex for read-write/exclusive access.
     * @param m The ReaderWriterMutex to on which we will call LockReadWrite() and UnlockReadWrite().
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   ReadWriteMutexGuard(const ReaderWriterMutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(m), _optFileName(optFileName), _fileLine(fileLine)
#else
   ReadWriteMutexGuard(const ReaderWriterMutex & m) : _mutex(m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      if (_mutex.LockReadWriteAux(MUSCLE_TIME_NEVER).IsError()) MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex LockReadWrite() failed!\n");
      _mutex.LogDeadlockFinderEvent(LOCK_ACTION_LOCK, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);  // must be called while the ReaderWriterMutex is locked
#else
      if (_mutex.LockReadWrite().IsError())    MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex LockReadWrite() failed!\n");
#endif
   }

   /** Destructor.  Unlocks the ReaderWriterMutex previously specified in the constructor. */
   ~ReadWriteMutexGuard()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _mutex.LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__); // must be called while the ReaderWriterMutex is locked
      if (_mutex.UnlockReadWriteAux().IsError()) MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex UnlockReadWrite() failed!\n");
#else
      if (_mutex.UnlockReadWrite().IsError())    MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex UnlockReadWrite() failed!\n");
#endif
   }

private:
   ReadWriteMutexGuard(const ReadWriteMutexGuard &);  // copy ctor, deliberately inaccessible

   const ReaderWriterMutex & _mutex;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   const char * _optFileName;
   const int _fileLine;
#endif
};

/** A macro to quickly and safely put a ReadOnlyMutexGuard on the stack for the given ReaderWriterMutex.
  * @note Using this macro is better than just declaring a ReadOnlyMutexGuard object directly in
  *       two ways:  (1) it makes it impossible to forget to name the ReadOnlyMutexGuard (which
  *       would result in an anonymous temporary ReadOnlyMutexGuard object that wouldn't keep
  *       the ReaderWriterMutex locked until the end of the scope), and (2) it allows the deadlock
  *       finding code to specify the ReadOnlyMutexGuard's location in its output rather than
  *       the location of the Lock() call within the ReadOnlyMutexGuard constructor, which
  *       makes for easier debugging via MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or by passing -DMUSCLE_ENABLE_DEADLOCK_FINDER
  *       as a compiler-argument)
  */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define DECLARE_READONLY_MUTEXGUARD(mutex) muscle::ReadOnlyMutexGuard MUSCLE_UNIQUE_NAME(mutex, __FILE__, __LINE__)
# else
# define DECLARE_READONLY_MUTEXGUARD(mutex) muscle::ReadOnlyMutexGuard MUSCLE_UNIQUE_NAME(mutex)
#endif

/** A macro to quickly and safely put a ReadWriteMutexGuard on the stack for the given ReaderWriterMutex.
  * @note Using this macro is better than just declaring a ReadWriteMutexGuard object directly in
  *       two ways:  (1) it makes it impossible to forget to name the ReadWriteMutexGuard (which
  *       would result in an anonymous temporary ReadWriteMutexGuard object that wouldn't keep
  *       the ReaderWriterMutex locked until the end of the scope), and (2) it allows the deadlock
  *       finding code to specify the ReadWriteMutexGuard's location in its output rather than
  *       the location of the Lock() call within the ReadWriteMutexGuard constructor, which
  *       makes for easier debugging via MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or by passing -DMUSCLE_ENABLE_DEADLOCK_FINDER
  *       as a compiler-argument)
  */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define DECLARE_READWRITE_MUTEXGUARD(mutex) muscle::ReadWriteMutexGuard MUSCLE_UNIQUE_NAME(mutex, __FILE__, __LINE__)
# else
# define DECLARE_READWRITE_MUTEXGUARD(mutex) muscle::ReadWriteMutexGuard MUSCLE_UNIQUE_NAME(mutex)
#endif

} // end namespace muscle

#endif
