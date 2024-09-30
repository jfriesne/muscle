/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleReaderWriterMutex_h
#define MuscleReaderWriterMutex_h

#include "system/Mutex.h"   // for the deadlock-finder stuff

#ifndef MUSCLE_SINGLE_THREAD_ONLY
# if defined(QT_CORE_LIB)  // is Qt4 available?
#  include <Qt>  // to bring in the proper value of QT_VERSION
# endif
# if defined(QT_THREAD_SUPPORT) || (QT_VERSION >= 0x040000)
#  define MUSCLE_QT_HAS_THREADS 1
# endif
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
#  include <mutex>
# else
#  if defined(WIN32)
#   if defined(MUSCLE_QT_HAS_THREADS) && defined(MUSCLE_PREFER_QT_OVER_WIN32)
    /* empty - we don't have to do anything for this case. */
#   else
#    ifndef MUSCLE_PREFER_WIN32_OVER_QT
#     define MUSCLE_PREFER_WIN32_OVER_QT
#    endif
#   endif
#  endif
#  if defined(MUSCLE_USE_PTHREADS)
#   include <pthread.h>
#  elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
#   // empty
#  elif defined(MUSCLE_QT_HAS_THREADS)
#   if (QT_VERSION >= 0x040000)
#    include <QReaderWriterMutex>
#   else
#    include <qthread.h>
#   endif
#  else
#   error "ReaderWriterMutex:  threading support not implemented for this platform.  You'll need to add code to the MUSCLE ReaderWriterMutex class for your platform, or add -DMUSCLE_SINGLE_THREAD_ONLY to your build line if your program is single-threaded or for some other reason does not need to worry about locking"
#  endif
# endif
#endif

#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
// Should be defined elsewhere to return true iff it's considered okay to call the given method on the given
// ReaderWriterMutex from the current thread-context.
namespace muscle {class ReaderWriterMutex;}
extern bool IsOkayToAccessMuscleReaderWriterMutex(const muscle::ReaderWriterMutex * m, const char * methodName);
#endif

namespace muscle {

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define LockReadOnly()     DeadlockFinderLockReadOnlyWrapper    (__FILE__, __LINE__)
# define TryLockReadOnly()  DeadlockFinderTryLockReadOnlyWrapper (__FILE__, __LINE__)
# define LockReadWrite()    DeadlockFinderLockReadWriteWrapper   (__FILE__, __LINE__)
# define TryLockReadWrite() DeadlockFinderTryLockReadWriteWrapper(__FILE__, __LINE__)
# define Unlock()           DeadlockFinderUnlockWrapper          (__FILE__, __LINE__)
#endif

class ReadOnlyMutexGuard;  // forward declaration
class ReadWriteMutexGuard;  // forward declaration

/** This class is a platform-independent API for a recursive mutual exclusion semaphore (a.k.a mutex).
  * Typically used to serialize the execution of critical sections in a multithreaded API
  * (eg the MUSCLE ObjectPool or Thread classes)
  * When compiling with the MUSCLE_SINGLE_THREAD_ONLY preprocessor flag defined, this class becomes a no-op.
  */
class ReaderWriterMutex MUSCLE_FINAL_CLASS : public NotCopyable
{
public:
   /** Constructor */
   ReaderWriterMutex()
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      : _isEnabled(_muscleSingleThreadOnly == false)
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      // empty
# elif defined(MUSCLE_USE_PTHREADS)
      // empty
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      // empty
# elif defined(MUSCLE_QT_HAS_THREADS)
#  if (QT_VERSION >= 0x040000)
      , _locker(QReaderWriterMutex::Recursive)
#  else
      , _locker(true)
#  endif
# endif
#endif
   {
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled)
      {
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
	 // empty
# elif defined(MUSCLE_USE_PTHREADS)
         pthread_mutexattr_t mutexattr;
         pthread_mutexattr_init(&mutexattr);                              // Note:  If this code doesn't compile, then
         pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);  // you may need to add -D_GNU_SOURCE to your
         pthread_mutex_init(&_locker, &mutexattr);                        // Linux Makefile to enable it properly.
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
         InitializeCriticalSection(&_locker);
# endif
      }
#endif
   }

   /** Destructor.  If a ReaderWriterMutex is destroyed while another thread is blocking in its LockReadOnly()
     * or LockReadWrite() method, the results are undefined.
     */
   ~ReaderWriterMutex() {Cleanup();}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderLockReadOnlyWrapper(const char * fileName, int fileLine) const
#else
   /** Attempts to lock the mutex for shared/read-only access.
     * Any thread that tries to LockReadOnly() this object while it is already locked-for-read+write access
     * by another thread will block until the writer-thread unlocks the lock.  The lock is recursive, however;
     * if a given thread calls LockReadOnly() twice in a row it won't deadlock itself (although it will
     * need to call Unlock() twice in a row in order to truly unlock the lock)
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked for some reason.
     */
   status_t LockReadOnly() const
#endif
   {
      const status_t ret = LockReadOnlyAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
#endif
      return ret;
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderTryLockReadOnlyWrapper(const char * fileName, int fileLine) const
#else
   /** Similar to LockReadOnly(), except this method is guaranteed to always return immediately (ie never blocks).
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked (eg because it is
     *          already locked by a writer thread)
     */
   status_t TryLockReadOnly() const
#endif
   {
      const status_t ret = TryLockReadOnlyAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
#endif
      return ret;
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderLockReadWriteWrapper(const char * fileName, int fileLine) const
#else
   /** Attempts to lock the lock for exclusive/read-write access.
     * Any thread that tries to LockReadWrite() this object while it is already locked by another thread
     * until after all threads have unlocked the lock.  The lock is recursive, however;
     * if a given thread calls LockReadWrite() twice in a row it won't deadlock itself (although it will
     * need to call Unlock() twice in a row in order to truly unlock the lock)
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked for some reason.
     */
   status_t LockReadWrite() const
#endif
   {
      const status_t ret = LockReadWriteAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
#endif
      return ret;
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderTryLockReadWriteWrapper(const char * fileName, int fileLine) const
#else
   /** Similar to LockReadWrite(), except this method is guaranteed to always return immediately (ie never blocks).
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked (eg because it is
     *          already locked by another thread)
     */
   status_t TryLockReadWrite() const
#endif
   {
      const status_t ret = TryLockReadWriteAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
#endif
      return ret;
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderUnlockWrapper(const char * fileName, int fileLine) const
#else
   /** Unlocks the lock.  Once this is done, any other thread that is blocked in the Lock()
     * method will gain ownership of the lock and return.
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED on failure (perhaps you tried to unlock a lock
     *          that wasn't locked?  This method should never fail in typical usage)
     */
   status_t Unlock() const
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging while we are still are locked, otherwise our counter can suffer from race conditions
      LogDeadlockFinderEvent(false, fileName, fileLine);
#endif

      return UnlockAux();
   }

   /** Turns this ReaderWriterMutex into a no-op object.  Irreversible! */
   void Neuter() {Cleanup();}

#ifndef MUSCLE_SINGLE_THREAD_ONLY
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   std::recursive_mutex & GetNativeReaderWriterMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   pthread_mutex_t & GetNativeReaderWriterMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   CRITICAL_SECTION & GetNativeReaderWriterMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   QReaderWriterMutex & GetNativeReaderWriterMutexImplementation() const {return _locker;}
# endif
#endif

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

   void Cleanup()
   {
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled)
      {
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
         // dunno of a way to do this with a std::mutex
# elif defined(MUSCLE_USE_PTHREADS)
         (void) pthread_mutex_destroy(&_locker);
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
         DeleteCriticalSection(&_locker);
# elif defined(MUSCLE_QT_HAS_THREADS)
         // do nothing
# endif
         _isEnabled = false;
      }
#endif
   }

#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
   void CheckForLockingViolation(const char * methodName) const
   {
      if (::IsOkayToAccessMuscleReaderWriterMutex(this, methodName) == false) printf("ReaderWriterMutex(%p)::%s:  Locking violation!\n", this, methodName);
   }
#endif

   status_t LockReadOnlyAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("LockReadOnly");
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled == false) return B_NO_ERROR;
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
# if !defined(MUSCLE_NO_EXCEPTIONS)
      try {
# endif
         _locker.lock();
# if !defined(MUSCLE_NO_EXCEPTIONS)
      } catch(...) {return B_LOCK_FAILED;}
# endif
      return B_NO_ERROR;
#elif defined(MUSCLE_USE_PTHREADS)
      return B_ERRNUM(pthread_mutex_lock(&_locker));
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_locker);
      return B_NO_ERROR;
#elif defined(MUSCLE_QT_HAS_THREADS)
      _locker.lock();
      return B_NO_ERROR;
#else
      return B_UNIMPLEMENTED;
#endif
   }

   status_t TryLockReadOnlyAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("TryLockReadOnly");
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled == false) return B_NO_ERROR;
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return _locker.try_lock() ? B_NO_ERROR : B_LOCK_FAILED;
#elif defined(MUSCLE_USE_PTHREADS)
      const int pret = pthread_mutex_trylock(&_locker);
      return (pret == EBUSY) ? B_LOCK_FAILED : B_ERRNUM(pret);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      return TryEnterCriticalSection(&_locker) ? B_NO_ERROR : B_LOCK_FAILED;
#elif defined(MUSCLE_QT_HAS_THREADS)
      return _locker.tryLock() ? B_NO_ERROR : B_LOCK_FAILED;
#else
      return B_UNIMPLEMENTED;
#endif
   }

   status_t LockReadWriteAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("LockReadWrite");
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled == false) return B_NO_ERROR;
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
# if !defined(MUSCLE_NO_EXCEPTIONS)
      try {
# endif
         _locker.lock();
# if !defined(MUSCLE_NO_EXCEPTIONS)
      } catch(...) {return B_LOCK_FAILED;}
# endif
      return B_NO_ERROR;
#elif defined(MUSCLE_USE_PTHREADS)
      return B_ERRNUM(pthread_mutex_lock(&_locker));
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_locker);
      return B_NO_ERROR;
#elif defined(MUSCLE_QT_HAS_THREADS)
      _locker.lock();
      return B_NO_ERROR;
#else
      return B_UNIMPLEMENTED;
#endif
   }

   status_t TryLockReadWriteAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("TryLockReadWrite");
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled == false) return B_NO_ERROR;
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return _locker.try_lock() ? B_NO_ERROR : B_LOCK_FAILED;
#elif defined(MUSCLE_USE_PTHREADS)
      const int pret = pthread_mutex_trylock(&_locker);
      return (pret == EBUSY) ? B_LOCK_FAILED : B_ERRNUM(pret);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      return TryEnterCriticalSection(&_locker) ? B_NO_ERROR : B_LOCK_FAILED;
#elif defined(MUSCLE_QT_HAS_THREADS)
      return _locker.tryLock() ? B_NO_ERROR : B_LOCK_FAILED;
#else
      return B_UNIMPLEMENTED;
#endif
   }

   status_t UnlockAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("Unlock");
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled == false) return B_NO_ERROR;
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
      _locker.unlock();
      return B_NO_ERROR;
#elif defined(MUSCLE_USE_PTHREADS)
      return B_ERRNUM(pthread_mutex_unlock(&_locker));
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      LeaveCriticalSection(&_locker);
      return B_NO_ERROR;
#elif defined(MUSCLE_QT_HAS_THREADS)
      _locker.unlock();
      return B_NO_ERROR;
#else
      return B_UNIMPLEMENTED;
#endif
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   void LogDeadlockFinderEvent(bool isLock, const char * fileName, int fileLine) const
   {
      if ((_enableDeadlockFinderPrints)&&(!_inDeadlockFinderCallback.IsInBatch()))
      {
         NestCountGuard ncg(_inDeadlockFinderCallback);
         DeadlockFinder_LogEvent(isLock, this, fileName, fileLine);
      }
   }
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   bool _isEnabled;  // if false, this ReaderWriterMutex is a no-op
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   mutable std::recursive_mutex _locker;
# elif defined(MUSCLE_USE_PTHREADS)
   mutable pthread_mutex_t _locker;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   mutable CRITICAL_SECTION _locker;
# elif defined(MUSCLE_QT_HAS_THREADS)
   mutable QReaderWriterMutex _locker;
# endif
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   mutable NestCount _inDeadlockFinderCallback;
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
     * @param m The ReaderWriterMutex to on which we will call LockReadOnly() and Unlock().
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   ReadOnlyMutexGuard(const ReaderWriterMutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(m), _optFileName(optFileName), _fileLine(fileLine)
#else
   ReadOnlyMutexGuard(const ReaderWriterMutex & m) : _mutex(m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      if (_mutex.LockReadOnlyAux().IsError()) MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex LockReadOnly() failed!\n");
      _mutex.LogDeadlockFinderEvent(true, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);  // must be called while the ReaderWriterMutex is locked
#else
      if (_mutex.LockReadOnly().IsError())    MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex LockReadOnly() failed!\n");
#endif
   }

   /** Destructor.  Unlocks the ReaderWriterMutex previously specified in the constructor. */
   ~ReadOnlyMutexGuard()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _mutex.LogDeadlockFinderEvent(false, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__); // must be called while the ReaderWriterMutex is locked
      if (_mutex.UnlockAux().IsError()) MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex Unlock() failed!\n");
#else
      if (_mutex.Unlock().IsError())    MCRASH("ReadOnlyMutexGuard:  ReaderWriterMutex Unlock() failed!\n");
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
     * @param m The ReaderWriterMutex to on which we will call LockReadWrite() and Unlock().
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   ReadWriteMutexGuard(const ReaderWriterMutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(m), _optFileName(optFileName), _fileLine(fileLine)
#else
   ReadWriteMutexGuard(const ReaderWriterMutex & m) : _mutex(m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      if (_mutex.LockReadWriteAux().IsError()) MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex LockReadWrite() failed!\n");
      _mutex.LogDeadlockFinderEvent(true, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);  // must be called while the ReaderWriterMutex is locked
#else
      if (_mutex.LockReadWrite().IsError())    MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex LockReadWrite() failed!\n");
#endif
   }

   /** Destructor.  Unlocks the ReaderWriterMutex previously specified in the constructor. */
   ~ReadWriteMutexGuard()
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _mutex.LogDeadlockFinderEvent(false, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__); // must be called while the ReaderWriterMutex is locked
      if (_mutex.UnlockAux().IsError()) MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex Unlock() failed!\n");
#else
      if (_mutex.Unlock().IsError())    MCRASH("ReadWriteMutexGuard:  ReaderWriterMutex Unlock() failed!\n");
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
