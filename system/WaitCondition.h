/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleWaitCondition_h
#define MuscleWaitCondition_h

#include "support/NotCopyable.h"
#include "util/TimeUtilityFunctions.h"  // for MUSCLE_TIME_NEVER

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# error "You're not allowed use the WaitCondition class if you have the MUSCLE_SINGLE_THREAD_ONLY compiler constant defined!"
#endif

#if defined(QT_CORE_LIB)  // is Qt4 available?
# include <Qt>  // to bring in the proper value of QT_VERSION
#endif
#if defined(QT_THREAD_SUPPORT) || (QT_VERSION >= 0x040000)
# define MUSCLE_QT_HAS_THREADS 1
#endif
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
# include <condition_variable>
# include <mutex>
#else
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
#  include <time.h>
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
#  // empty
# elif defined(MUSCLE_QT_HAS_THREADS)
#  if (QT_VERSION >= 0x040000)
#   include <QWaitCondition>
#  else
#   include <qwaitcondition.h>
#  endif
# else
#  error "WaitCondition:  threading support not implemented for this platform.  You'll need to add support code for your platform to the MUSCLE WaitCondition class before you can use this class."
# endif
#endif

namespace muscle {

/** This class is a platform-independent API for a wait/notify mechanism via which one thread
  * can block inside Wait(), until another thread signals it to wake up by calling Notify().
  */
class WaitCondition MUSCLE_FINAL_CLASS : public NotCopyable
{
public:
   /** Constructor */
   WaitCondition() : _conditionSignalled(false) {Setup();}

   /** Destructor.  If a WaitCondition is destroyed while another thread is blocking in its Lock() method,
     * the results are undefined.
     */
   ~WaitCondition() {Cleanup();}

   /** Blocks until the next time someone calls Notify() on this object, or until (wakeupTime)
     * is reached, whichever comes first.
     * @param wakeupTime the time (e.g. as returned by GetRunTime64()) at which to return B_TIMED_OUT
     *                   if Notify() hasn't been called by then.  Defaults to MUSCLE_TIME_NEVER.
     * @returns B_NO_ERROR if Notify() was called, or B_TIMED_OUT if the timeout time was reached,
     *                     or some other value on an error.
     * @note that if Notify() was called before Wait() was called, then Wait() will return immediately.
     *       That way the Wait()-ing thread doesn't have to worry about "missing" a notification if
     *       it was busy doing something else at the moment Notify() was called.  Only a single Notify()
     *       call is "cached" in this manner, however; for example, even if Notify() was called multiple times
     *       since the thread's last call to Wait(), only the first subsequent call to Wait() will return
     *       immediately.
     */
   status_t Wait(uint64 wakeupTime = MUSCLE_TIME_NEVER) const {return (wakeupTime == MUSCLE_TIME_NEVER) ? WaitAux() : WaitUntilAux(wakeupTime);}

   /** Wakes up the thread that is blocking inside Wait() on this object.  If no thread is currently
     * blocking inside Wait(), then it sets a flag so that the next time Wait() is called, Wait() will
     * return immediately.
     * @returns B_NO_ERROR on success, or another value on failure.
     */
   status_t Notify() const {return NotifyAux();}

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   std::condition_variable & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   pthread_cond_t & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   CONDITION_VARIABLE & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   QWaitCondition & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#endif

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   std::mutex & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   pthread_mutex_t & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   CRITICAL_SECTION & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   QMutex & GetNativeMutexImplementation() const {return _conditionMutex;}
#endif

private:
   void Setup()
   {
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      // empty
#elif defined(MUSCLE_USE_PTHREADS)
      pthread_mutex_init(&_conditionMutex,    NULL);
      pthread_cond_init( &_conditionVariable, NULL);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      InitializeCriticalSection(  &_conditionMutex);
      InitializeConditionVariable(&_conditionVariable);
#endif
   }

   void Cleanup()
   {
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      // do nothing
#elif defined(MUSCLE_USE_PTHREADS)
      (void) pthread_cond_destroy(&_conditionVariable);
      (void) pthread_mutex_destroy(&_conditionMutex);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      DeleteCriticalSection(&_conditionMutex);
      // there's no DeleteConditionVariable() call in Win32 API
#elif defined(MUSCLE_QT_HAS_THREADS)
      // do nothing
#endif
   }

   mutable bool _conditionSignalled;

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   mutable std::condition_variable _conditionVariable;
   mutable std::mutex              _conditionMutex;
#elif defined(MUSCLE_USE_PTHREADS)
   mutable pthread_cond_t          _conditionVariable;
   mutable pthread_mutex_t         _conditionMutex;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   mutable CONDITION_VARIABLE      _conditionVariable;
   mutable CRITICAL_SECTION        _conditionMutex;
#elif defined(MUSCLE_QT_HAS_THREADS)
   mutable QWaitCondition          _conditionVariable;
   mutable QMutex                  _conditionMutex;
#endif

   status_t WaitAux() const
   {
      status_t ret;
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      std::unique_lock<std::mutex> lockGuard(_conditionMutex);
      _conditionVariable.wait(lockGuard, [this]{return _conditionSignalled;});
      if (ret.IsOK()) _conditionSignalled = false;
#elif defined(MUSCLE_USE_PTHREADS)
      if (pthread_mutex_lock(&_conditionMutex) == 0)
      {
         while(_conditionSignalled == false)
         {
            const int pret = pthread_cond_wait(&_conditionVariable, &_conditionMutex);
            if (pret != 0)
            {
               ret = B_ERRNUM(pret);
               break;
            }
         }
         if (ret.IsOK()) _conditionSignalled = false;
         (void) pthread_mutex_unlock(&_conditionMutex);
      }
      else ret = B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      while(_conditionSignalled == false)
      {
         if (SleepConditionVariableCS(&_conditionVariable, &_conditionMutex, INFINITE) == false)
         {
            ret = (GetLastError() == ERROR_TIMEOUT) ? B_TIMED_OUT : B_ERRNO;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) _conditionSignalled = false;
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      while(_conditionSignalled == false)
      {
         if (_conditionVariable.wait(&_conditionMutex) == false)
         {
            ret = B_TIMED_OUT;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) _conditionSignalled = false;
      _conditionMutex.unlock();
#endif
      return ret;
   }

   status_t WaitUntilAux(uint64 wakeupTime) const
   {
      int64 timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
      if (timeDeltaMicros <= 0) return B_TIMED_OUT;

      status_t ret;
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      {
         std::unique_lock<std::mutex> lockGuard(_conditionMutex);
         auto timeoutTime = std::chrono::system_clock::now() + std::chrono::microseconds(timeDeltaMicros);
         if (_conditionVariable.wait_until(lockGuard, timeoutTime, [this](){return _conditionSignalled;}) == false) ret = B_TIMED_OUT;
         if (ret.IsOK()) _conditionSignalled = false;
      }
#elif defined(MUSCLE_USE_PTHREADS)
      if (pthread_mutex_lock(&_conditionMutex) == 0)
      {
         struct timespec realTimeClockNow;
         if (clock_gettime(CLOCK_REALTIME, &realTimeClockNow) != 0) ret = B_ERRNO;
         else
         {
            const int64 realTimeClockWakeupTimeNanos = SecondsToNanos(realTimeClockNow.tv_sec) + (int64)realTimeClockNow.tv_nsec + MicrosToNanos(timeDeltaMicros);
            struct timespec realTimeClockThen;
            realTimeClockThen.tv_sec  = realTimeClockWakeupTimeNanos / SecondsToNanos(1);
            realTimeClockThen.tv_nsec = realTimeClockWakeupTimeNanos % SecondsToNanos(1);
            while(_conditionSignalled == false)
            {
               const int pret = pthread_cond_timedwait(&_conditionVariable, &_conditionMutex, &realTimeClockThen);
               if (pret != 0) {ret = (pret == ETIMEDOUT) ? B_TIMED_OUT : B_ERRNUM(pret); break;}
            }
         }
         if (ret.IsOK()) _conditionSignalled = false;
         (void) pthread_mutex_unlock(&_conditionMutex);
      }
      else ret = B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      while(_conditionSignalled == false)
      {
         timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
         if ((timeDeltaMicros <= 0)||(SleepConditionVariableCS(&_conditionVariable, &_conditionMutex, (DWORD) MicrosToMillis(timeDeltaMicros)) == false)
         {
            ret = (GetLastError() == ERROR_TIMEOUT) ? B_TIMED_OUT : B_ERRNO;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) _conditionSignalled = false;
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      while(_conditionSignalled == false)
      {
         timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
         if ((timeDeltaMicros <= 0)||(_conditionVariable.wait(&_conditionMutex, (unsigned long) MicrosToMillis(timeDeltaMicros)) == false))
         {
            ret = B_TIMED_OUT;
            break;
         }
      }
      if (ret.IsOK()) _conditionSignalled = false;
      _conditionMutex.unlock();
#endif
      return ret;
   }

   status_t NotifyAux() const
   {
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      std::unique_lock<std::mutex> lockGuard(_conditionMutex);
      if (_conditionSignalled == false)
      {
         _conditionVariable.notify_one();
         _conditionSignalled = true;
      }
#elif defined(MUSCLE_USE_PTHREADS)
      if (pthread_mutex_lock(&_conditionMutex) == 0)
      {
         status_t ret;
         if (_conditionSignalled == false)
         {
            if (pthread_cond_signal(&_conditionVariable) == 0) _conditionSignalled = true;
                                                          else ret = B_BAD_OBJECT;
         }
         (void) pthread_mutex_unlock(&_conditionMutex);
         return ret;
      }
      else return B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      if (_conditionSignalled == false)
      {
         WakeConditionVariable(&_conditionVariable);
         _conditionSignalled = true;
      }
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      if (_conditionSignalled == false)
      {
         _conditionVariable.wakeOne();
         _conditionSignalled = true;
      }
      _conditionMutex.unlock();
#endif

      return B_NO_ERROR;
   }
};

} // end namespace muscle

#endif
