/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMutex_h
#define MuscleMutex_h

#include "support/MuscleSupport.h"  // needed for WIN32 defines, etc

#ifndef MUSCLE_SINGLE_THREAD_ONLY

#if defined(QT_CORE_LIB)  // is Qt4 available?
# include <Qt>  // to bring in the proper value of QT_VERSION
#endif

#if defined(QT_THREAD_SUPPORT) || (QT_VERSION >= 0x040000)
# define MUSCLE_QT_HAS_THREADS 1
#endif

# if defined(WIN32)
#  if defined(MUSCLE_QT_HAS_THREADS) && defined(MUSCLE_PREFER_QT_OVER_WIN32)
    /* empty - we don't have to do anything for this case. */
#  else
#   ifndef MUSCLE_PREFER_WIN32_OVER_QT
#    define MUSCLE_PREFER_WIN32_OVER_QT
#   endif
#  endif
# endif
# if defined(MUSCLE_USE_PTHREADS)
#  include <pthread.h>
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
#  // empty
# elif defined(MUSCLE_QT_HAS_THREADS)
#  if (QT_VERSION >= 0x040000)
#   include <QMutex>
#  else
#   include <qthread.h>
#  endif
# elif defined(__BEOS__) || defined(__HAIKU__)
#  include <support/Locker.h>
# elif defined(__ATHEOS__)
#  include <util/locker.h>
# else
#  error "Mutex:  threading support not implemented for this platform.  You'll need to add code to the MUSCLE Mutex class for your platform, or add -DMUSCLE_SINGLE_THREAD_ONLY to your build line if your program is single-threaded or for some other reason does not need to worry about locking"
# endif
#endif

namespace muscle {

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# define Lock()   DeadlockFinderLockWrapper  (__FILE__, __LINE__)
# define Unlock() DeadlockFinderUnlockWrapper(__FILE__, __LINE__)
extern void DeadlockFinder_LogEvent(bool isLock, const void * mutexPtr, const char * fileName, int fileLine);
extern bool _enableDeadlockFinderPrints;
#define LOG_DEADLOCK_FINDER_EVENT(val)                                   \
      if ((_enableDeadlockFinderPrints)&&(_inDeadlockCallbackCount == 0)) \
      {                                                                  \
         _inDeadlockCallbackCount++;                                     \
         DeadlockFinder_LogEvent(val, this, fileName, fileLine);         \
         _inDeadlockCallbackCount--;                                    \
      }
#endif

// If false, then we must not assume that we are running in single-threaded mode.
// This variable should be set by the ThreadSetupSystem constructor ONLY!
extern bool _muscleSingleThreadOnly;

/** This class is a platform-independent API for a recursive mutual exclusion semaphore (a.k.a mutex).
  * Typically used to serialize the execution of critical sections in a multithreaded API
  * (e.g. the MUSCLE ObjectPool or Thread classes)
  * When compiling with the MUSCLE_SINGLE_THREAD_ONLY preprocessor flag defined, this class becomes a no-op.
  */
class Mutex MUSCLE_FINAL_CLASS
{
public:
   /** Constructor */
   Mutex()
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      : _isEnabled(_muscleSingleThreadOnly == false)
# if defined(MUSCLE_USE_PTHREADS)
      // empty
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      // empty
# elif defined(MUSCLE_QT_HAS_THREADS)
#  if (QT_VERSION >= 0x040000)
      , _locker(QMutex::Recursive)
#  else
      , _locker(true)
#  endif
# elif defined(__ATHEOS__)
      , _locker(NULL)
# endif
#endif
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      _inDeadlockCallbackCount = 0;
#endif

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled)
      {
# if defined(MUSCLE_USE_PTHREADS)
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
     * @returns B_NO_ERROR on success, or B_ERROR if the lock could not be locked for some reason.
     */
   status_t Lock() const
#endif
   {
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      return B_NO_ERROR;
#else
      if (_isEnabled == false) return B_NO_ERROR;

# if defined(MUSCLE_USE_PTHREADS)
      status_t ret = (pthread_mutex_lock(&_locker) == 0) ? B_NO_ERROR : B_ERROR;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_locker);
      status_t ret = B_NO_ERROR;
# elif defined(MUSCLE_QT_HAS_THREADS)
      _locker.lock();
      status_t ret = B_NO_ERROR;
# elif defined(__BEOS__) || defined(__HAIKU__)
      status_t ret = _locker.Lock() ? B_NO_ERROR : B_ERROR;
# elif defined(__ATHEOS__)
      status_t ret = _locker.Lock() ? B_ERROR : B_NO_ERROR;  // Is this correct?  Kurt's documentation sucks
# endif

# ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // We gotta do the logging after we are locked, otherwise our counter can suffer from race conditions
      if (ret == B_NO_ERROR) LOG_DEADLOCK_FINDER_EVENT(true);
# endif

      return ret;
#endif
   }

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   status_t DeadlockFinderUnlockWrapper(const char * fileName, int fileLine) const
#else
   /** Unlocks the lock.  Once this is done, any other thread that is blocked in the Lock()
     * method will gain ownership of the lock and return.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (perhaps you tried to unlock a lock
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
      LOG_DEADLOCK_FINDER_EVENT(false);
# endif

# if defined(MUSCLE_USE_PTHREADS)
      return (pthread_mutex_unlock(&_locker) == 0) ? B_NO_ERROR : B_ERROR;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      LeaveCriticalSection(&_locker);
      return B_NO_ERROR;
# elif defined(MUSCLE_QT_HAS_THREADS)
      _locker.unlock();
      return B_NO_ERROR;
# elif defined(__BEOS__) || defined(__HAIKU__)
      _locker.Unlock();
      return B_NO_ERROR;
# elif defined(__ATHEOS__)
      return _locker.Unlock() ? B_ERROR : B_NO_ERROR;  // Is this correct?  Kurt's documentation sucks
# endif
#endif
   }

   /** Turns this Mutex into a no-op object.  Irreversible! */
   void Neuter() {Cleanup();}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   void AvoidFindDeadlockCallbacks() {_inDeadlockCallbackCount++;}
#endif

private:
   void Cleanup()
   {
#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (_isEnabled)
      {
# if defined(MUSCLE_USE_PTHREADS)
         pthread_mutex_destroy(&_locker);
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
         DeleteCriticalSection(&_locker);
# elif defined(MUSCLE_QT_HAS_THREADS)
         // do nothing
# endif
         _isEnabled = false;
      }
#endif
   }

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   bool _isEnabled;  // if false, this Mutex is a no-op
# if defined(MUSCLE_USE_PTHREADS)
   mutable pthread_mutex_t _locker;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   mutable CRITICAL_SECTION _locker;
# elif defined(MUSCLE_QT_HAS_THREADS)
   mutable QMutex _locker;
# elif defined(__BEOS__) || defined(__HAIKU__)
   mutable BLocker _locker;
# elif defined(__ATHEOS__)
   mutable os::Locker _locker;
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
   MutexGuard(const Mutex & m) : _mutex(m)
   {
      if (_mutex.Lock() == B_NO_ERROR) _isMutexLocked = true;
      else
      {
         _isMutexLocked = false;
         printf("MutexGuard %p:  could not lock mutex %p!\n", this, &_mutex);
      }
   }

   /** Destructor.  Unlocks the Mutex previously specified in the constructor. */
   ~MutexGuard()
   {
      if ((_isMutexLocked)&&(_mutex.Unlock() != B_NO_ERROR)) printf("MutexGuard %p:  could not unlock mutex %p!\n", this, &_mutex);
   }

   /** Returns true iff we successfully locked our Mutex. */
   bool IsMutexLocked() const {return _isMutexLocked;}

private:
   MutexGuard(const MutexGuard &);  // copy ctor, deliberately inaccessible

   const Mutex & _mutex;
   bool _isMutexLocked;
};

/** A macro to quickly and safely put a MutexGuard on the stack for the given Mutex. */
#define DECLARE_MUTEXGUARD(mutex) MutexGuard MUSCLE_UNIQUE_NAME(mutex)

}; // end namespace muscle

#endif
