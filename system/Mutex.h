/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMutex_h
#define MuscleMutex_h

#include "support/MuscleSupport.h"  // needed for WIN32 defines, etc
#include "support/NotCopyable.h"
#include "util/NestCount.h"
#include "util/OutputPrinter.h"

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
#    include <QMutex>
#   else
#    include <qthread.h>
#   endif
#  else
#   error "Mutex:  threading support not implemented for this platform.  You'll need to add code to the MUSCLE Mutex class for your platform, or add -DMUSCLE_SINGLE_THREAD_ONLY to your build line if your program is single-threaded or for some other reason does not need to worry about locking"
#  endif
# endif
#endif

#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
// Should be defined elsewhere to return true iff it's considered okay to call the given method on the given
// Mutex from the current thread-context.
namespace muscle {class Mutex;}
extern bool IsOkayToAccessMuscleMutex(const muscle::Mutex * m, const char * methodName);
#endif

namespace muscle {

class String;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
enum {
   LOCK_ACTION_UNLOCK_EXCLUSIVE = 0,
   LOCK_ACTION_UNLOCK_SHARED,
   LOCK_ACTION_LOCK_EXCLUSIVE,
   LOCK_ACTION_LOCK_SHARED,
   LOCK_ACTION_TRYLOCK_EXCLUSIVE,
   LOCK_ACTION_TRYLOCK_SHARED,
   NUM_LOCK_ACTIONS
};
# define Lock()    DeadlockFinderLockWrapper   (__FILE__, __LINE__)
# define TryLock() DeadlockFinderTryLockWrapper(__FILE__, __LINE__)
# define Unlock()  DeadlockFinderUnlockWrapper (__FILE__, __LINE__)
extern void DeadlockFinder_LogEvent(uint32 lockActionType, const void * mutexPtr, const char * fileName, int fileLine);
extern bool _enableDeadlockFinderPrints;
class ReaderWriterMutex;
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
// If false, then we must not assume that we are running in single-threaded mode.
// This variable should be set by the ThreadSetupSystem constructor ONLY!
extern bool _muscleSingleThreadOnly;
#endif

class MutexGuard;  // forward declaration

/** This class is a platform-independent API for a recursive mutual exclusion semaphore (aka mutex).
  * Typically used to serialize the execution of critical sections in a multithreaded API
  * (eg the MUSCLE ObjectPool or Thread classes)
  * When compiling with the MUSCLE_SINGLE_THREAD_ONLY preprocessor flag defined, this class becomes a no-op.
  */
class Mutex MUSCLE_FINAL_CLASS : public NotCopyable
{
public:
   /** Constructor */
   Mutex()
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
      , _locker(QMutex::Recursive)
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

   /** Destructor.  If a Mutex is destroyed while another thread is blocking in its Lock() method,
     * the results are undefined.
     */
   ~Mutex() {Cleanup();}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderLockWrapper(const char * fileName, int fileLine) const
#else
   /** Attempts to lock the lock.
     * Any thread that tries to Lock() this object while it is already locked by another thread
     * will block until the other thread unlocks the lock.  The lock is recursive, however;
     * if a given thread calls Lock() twice in a row it won't deadlock itself (although it will
     * need to call Unlock() twice in a row in order to truly unlock the lock)
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked for some reason.
     */
   status_t Lock() const
#endif
   {
      const status_t ret = LockAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(LOCK_ACTION_LOCK_EXCLUSIVE, fileName, fileLine);
#endif
      return ret;
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderTryLockWrapper(const char * fileName, int fileLine) const
#else
   /** Similar to Lock(), except this method is guaranteed to always return immediately (ie never blocks).
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked (eg because it is
     *          already locked by another thread)
     */
   status_t TryLock() const
#endif
   {
      const status_t ret = TryLockAux();
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(LOCK_ACTION_TRYLOCK_EXCLUSIVE, fileName, fileLine);
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
      LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK_EXCLUSIVE, fileName, fileLine);
#endif

      return UnlockAux();
   }

   /** Turns this Mutex into a no-op object.  Irreversible! */
   void Neuter() {Cleanup();}

#ifndef MUSCLE_SINGLE_THREAD_ONLY
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   std::recursive_mutex & GetNativeMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   pthread_mutex_t & GetNativeMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   CRITICAL_SECTION & GetNativeMutexImplementation() const {return _locker;}
# elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   QMutex & GetNativeMutexImplementation() const {return _locker;}
# endif
#endif

   /** If MUSCLE_ENABLE_DEADLOCK_FINDER is defined, this method disables mutex-callback-logging on this Mutex,
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

   /** If MUSCLE_ENABLE_DEADLOCK_FINDER is defined, this method re-enables mutex-callback-logging on this Mutex,
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
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   friend class ReaderWriterMutex;   // just so ReaderWriterMutex::LogDeadlockFinderEvent() can call LockAux() and UnlockAux() directly
#endif
   friend class MutexGuard;

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
      if (::IsOkayToAccessMuscleMutex(this, methodName) == false) printf("Mutex(%p)::%s:  Locking violation!\n", this, methodName);
   }
#endif

   status_t LockAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("Lock");
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

   status_t TryLockAux() const
   {
#ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      CheckForLockingViolation("TryLock");
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
   void LogDeadlockFinderEvent(uint32 lockAction, const char * fileName, int fileLine) const
   {
      if ((_enableDeadlockFinderPrints)&&(!_inDeadlockFinderCallback.IsInBatch()))
      {
         NestCountGuard ncg(_inDeadlockFinderCallback);
         DeadlockFinder_LogEvent(lockAction, this, fileName, fileLine);
      }
   }
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   bool _isEnabled;  // if false, this Mutex is a no-op
# if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   mutable std::recursive_mutex _locker;
# elif defined(MUSCLE_USE_PTHREADS)
   mutable pthread_mutex_t _locker;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   mutable CRITICAL_SECTION _locker;
# elif defined(MUSCLE_QT_HAS_THREADS)
   mutable QMutex _locker;
# endif
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   mutable NestCount _inDeadlockFinderCallback;
#endif
};

/** This convenience class can be used to automatically lock/unlock a Mutex based on the MutexGuard's ctor/dtor.
  * @note it's safer to use the DECLARE_MUTEXGUARD(theMutex) macro rather than manually placing a MutexGuard object
  *       onto the stack, since that avoids any possibility of forgetting to give the MutexGuard stack-object a name
  *       (eg typing "MutexGuard(myMutex);" rather than "MutexGuard mg(myMutex);", would introduce a perniciously
  *       non-obvious run-time error where your Mutex only gets locked momentarily rather than until the end-of-scope)
  *       Using the DECLARE_MUTEXGUARD(theMutex) macro will also allow MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or via -DMUSCLE_ENABLE_DEADLOCK_FINDER as compiler argument)
  *       to give you more useful debugging information about deadlocks that could happen (even if they didn't happen this time)
  */
class MUSCLE_NODISCARD MutexGuard MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  Locks the specified Mutex.
     * @param m The Mutex to lock.
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   MutexGuard(const Mutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(&m), _optFileName(optFileName), _fileLine(fileLine)
#else
   MutexGuard(const Mutex & m) : _mutex(&m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      if (_mutex->LockAux().IsError()) MCRASH("MutexGuard:  Mutex Lock() failed!\n");
      _mutex->LogDeadlockFinderEvent(LOCK_ACTION_LOCK_EXCLUSIVE, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);  // must be called while the Mutex is locked
#else
      if (_mutex->Lock().IsError())    MCRASH("MutexGuard:  Mutex Lock() failed!\n");
#endif
   }

   /** Destructor.  Unlocks the Mutex previously specified in the constructor. */
   ~MutexGuard() {UnlockAux();}

   /** Call this to unlock our guarded Mutex "early" (right now, instead of when our destructor executes)
     * If called more than once, the second and further calls will have no effect.
     */
   void UnlockEarly() {UnlockAux();}

private:
   MutexGuard(const MutexGuard &);  // copy ctor, deliberately inaccessible

   void UnlockAux()
   {
      if (_mutex)
      {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
         _mutex->LogDeadlockFinderEvent(LOCK_ACTION_UNLOCK_EXCLUSIVE, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__); // must be called while the Mutex is locked
         if (_mutex->UnlockAux().IsError()) MCRASH("MutexGuard:  Mutex Unlock() failed!\n");
#else
         if (_mutex->Unlock().IsError())    MCRASH("MutexGuard:  Mutex Unlock() failed!\n");
#endif
         _mutex = NULL;
      }
   }

   const Mutex * _mutex;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   const char * _optFileName;
   const int _fileLine;
#endif
};

/** If MUSCLE_ENABLE_DEADLOCK_FINDER was defined during compilation, this function will print
  * out a human-readable report about how Mutexes have been locked so far, and whether
  * any inconsistent locking ordering has been detected.  Otherwise an error message will be printed.
  * @param p the OutputPrinter to use for printing the output.
  * @returns B_NO_ERROR on success, or another value on failure.
  */
status_t PrintMutexLockingReport(const OutputPrinter & p);

/** A macro to quickly and safely put a MutexGuard on the stack for the given Mutex.
  * @note Using this macro is better than just declaring a MutexGuard object directly in
  *       two ways:  (1) it makes it impossible to forget to name the MutexGuard (which
  *       would result in an anonymous temporary MutexGuard object that wouldn't keep
  *       the Mutex locked until the end of the scope), and (2) it allows the deadlock
  *       finding code to specify the MutexGuard's location in its output rather than
  *       the location of the Lock() call within the MutexGuard constructor, which
  *       makes for easier debugging via MUSCLE's deadlock-finder functionality (enabled
  *       via -DWITH_DEADLOCK_FINDER=ON in CMake, or by passing -DMUSCLE_ENABLE_DEADLOCK_FINDER
  *       as a compiler-argument)
  */
#define DECLARE_MUTEXGUARD(mutex) DECLARE_NAMED_MUTEXGUARD(MUSCLE_UNIQUE_NAME, mutex)

/** This macro is the same as DECLARE_MUTEXGUARD() (above) except that it allows the caller
  * to specify the name of the MutexGuard stack-object.
  * This is useful in cases where you need to make method calls on the MutexGuard object later.
  */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define DECLARE_NAMED_MUTEXGUARD(guardName, mutex) muscle::MutexGuard guardName(mutex, __FILE__, __LINE__)
# else
# define DECLARE_NAMED_MUTEXGUARD(guardName, mutex) muscle::MutexGuard guardName(mutex)
#endif

} // end namespace muscle

#endif
