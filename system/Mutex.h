/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMutex_h
#define MuscleMutex_h

#include "support/MuscleSupport.h"  // needed for WIN32 defines, etc
#include "support/NotCopyable.h"

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

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define Lock()    DeadlockFinderLockWrapper   (__FILE__, __LINE__)
# define TryLock() DeadlockFinderTryLockWrapper(__FILE__, __LINE__)
# define Unlock()  DeadlockFinderUnlockWrapper (__FILE__, __LINE__)
extern void DeadlockFinder_LogEvent(bool isLock, const void * mutexPtr, const char * fileName, int fileLine);
extern bool _enableDeadlockFinderPrints;
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
// If false, then we must not assume that we are running in single-threaded mode.
// This variable should be set by the ThreadSetupSystem constructor ONLY!
extern bool _muscleSingleThreadOnly;
#endif

class MutexGuard;  // forward declaration

/** This class is a platform-independent API for a recursive mutual exclusion semaphore (a.k.a mutex). 
  * Typically used to serialize the execution of critical sections in a multithreaded API 
  * (e.g. the MUSCLE ObjectPool or Thread classes)
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
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _inDeadlockCallbackCount = 0;
#endif

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
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#else
      if (_isEnabled == false) return B_NO_ERROR;

      const status_t ret = LockAux();
# ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
# endif
      return ret;
#endif
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderTryLockWrapper(const char * fileName, int fileLine) const
#else
   /** Similar to Lock(), except this method is guaranteed to always return immediately (i.e. never blocks).
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the lock could not be locked (e.g. because it is 
     *          already locked by another thread)
     */
   status_t TryLock() const
#endif
   {
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#else
      if (_isEnabled == false) return B_NO_ERROR;

      const status_t ret = TryLockAux();
# ifdef MUSCLE_ENABLE_LOCKING_VIOLATIONS_CHECKER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret.IsOK()) LogDeadlockFinderEvent(true, fileName, fileLine);
# endif
      return ret;
#endif
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
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#else
      if (_isEnabled == false) return B_NO_ERROR;

# ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging while we are still are locked, otherwise our counter can suffer from race conditions
      LogDeadlockFinderEvent(false, fileName, fileLine);
# endif

      return UnlockAux();
#endif
   }

   /** Turns this Mutex into a no-op object.  Irreversible! */
   void Neuter() {Cleanup();}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   void AvoidFindDeadlockCallbacks() {_inDeadlockCallbackCount++;}
#endif

private:
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

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
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

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
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

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
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
      if ((_enableDeadlockFinderPrints)&&(_inDeadlockCallbackCount == 0))
      {
         _inDeadlockCallbackCount++;
         DeadlockFinder_LogEvent(isLock, this, fileName, fileLine);
         _inDeadlockCallbackCount--;
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
   mutable uint32 _inDeadlockCallbackCount;
#endif
};

/** This convenience class can be used to automatically lock/unlock a Mutex based on the MutexGuard's ctor/dtor */
class MutexGuard MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  Locks the specified Mutex.
     * @param m The Mutex to lock. 
     */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   MutexGuard(const Mutex & m, const char * optFileName = NULL, int fileLine = 0) : _mutex(m), _optFileName(optFileName), _fileLine(fileLine)
#else
   MutexGuard(const Mutex & m) : _mutex(m)
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# ifdef MUSCLE_SINGLE_THREAD_ONLY
      _isMutexLocked = true;
# else
      if ((_mutex._isEnabled==false)||(_mutex.LockAux().IsOK()))
      {
         _isMutexLocked = true;
         _mutex.LogDeadlockFinderEvent(true, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);
      }
# endif
#else
      if (_mutex.Lock().IsOK()) _isMutexLocked = true;
#endif
      else
      {
         _isMutexLocked = false;
         printf("MutexGuard %p:  could not lock mutex %p!\n", this, &_mutex);
      }
   }

   /** Destructor.  Unlocks the Mutex previously specified in the constructor. */
   ~MutexGuard()
   {
      if (_isMutexLocked)
      {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# ifdef MUSCLE_SINGLE_THREAD_ONLY
         const status_t ret;
# else
         // We gotta do the logging while the mutex is still are locked, otherwise our counter can suffer from race conditions
         _mutex.LogDeadlockFinderEvent(false, _optFileName?_optFileName:__FILE__, _optFileName?_fileLine:__LINE__);
         const status_t ret = _mutex._isEnabled ? _mutex.UnlockAux() : B_NO_ERROR;
# endif
#else
         const status_t ret = _mutex.Unlock();
#endif
         if (ret.IsError()) printf("MutexGuard %p:  could not unlock mutex %p! [%s]\n", this, &_mutex, ret());
      }
   }

   /** Returns true iff we successfully locked our Mutex. */
   bool IsMutexLocked() const {return _isMutexLocked;}

private:
   MutexGuard(const MutexGuard &);  // copy ctor, deliberately inaccessible

   const Mutex & _mutex;
   bool _isMutexLocked;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   const char * _optFileName;
   const int _fileLine;
#endif
};

/** A macro to quickly and safely put a MutexGuard on the stack for the given Mutex.
  * @note Using this macro is better than just declaring a MutexGuard object directly in
  *       two ways:  (1) it makes it impossible to forget to name the MutexGuard (which
  *       would result in an anonymous temporary MutexGuard object that wouldn't keep
  *       the Mutex locked until the end of the scope), and (2) it allows the deadlock
  *       finding code to specify the MutexGuard's location in its output rather than
  *       the location of the Lock() call within the MutexGuard constructor, which
  *       makes for easier debugging of deadlockfinder.cpp's output.
  */
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define DECLARE_MUTEXGUARD(mutex) MutexGuard MUSCLE_UNIQUE_NAME(mutex, __FILE__, __LINE__)
# else
# define DECLARE_MUTEXGUARD(mutex) MutexGuard MUSCLE_UNIQUE_NAME(mutex)
#endif
 
} // end namespace muscle

#endif
