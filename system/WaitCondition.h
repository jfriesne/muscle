/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
   WaitCondition() : _pendingNotificationsCount(0) {Setup();}

   /** Destructor.  If a WaitCondition is destroyed while another thread is blocking in its Lock() method,
     * the results are undefined.
     */
   ~WaitCondition() {Cleanup();}

   /** Blocks until the next time someone calls Notify() on this object, or until (wakeupTime)
     * is reached, whichever comes first.
     * @param wakeupTime the timestamp (eg as returned by GetRunTime64()) at which to give up and return
     *                   B_TIMED_OUT if Notify() hasn't been called by then.  Defaults to MUSCLE_TIME_NEVER.
     * @param optRetNotificationsCount if set non-NULL, then when this method returns successfully, the uint32
     *                   pointed to by this pointer will contain the number of notifications that
     *                   occurred (via calls to Notify()) since the previous time Wait() was called.
     * @returns B_NO_ERROR if this method returned because Notify() was called, or B_TIMED_OUT if the
     *                     timeout time was reached, or some other value if an error occurred.
     * @note if Notify() had already been called before Wait() was called, then Wait() will return immediately.
     *       That way the Wait()-ing thread doesn't have to worry about missing notifications if
     *       it was busy doing something else at the instant Notify() was called.
     */
   status_t Wait(uint64 wakeupTime = MUSCLE_TIME_NEVER, uint32 * optRetNotificationsCount = NULL) const
   {
      uint32 junk;
      uint32 & retCounter = optRetNotificationsCount ? *optRetNotificationsCount : junk;
      retCounter = 0;
      return (wakeupTime == MUSCLE_TIME_NEVER) ? WaitAux(retCounter) : WaitUntilAux(wakeupTime, retCounter);
   }

   /** Wakes up the thread that is blocking inside Wait() on this object.  If no thread is currently
     * blocking inside Wait(), then it just increases this object's internal notifications-counter,
     * so that the next time Wait() is called, that Wait() call will return immediately.
     * @param increaseBy the number to increase our internal notification-calls-counter by.  Defaults to 1.
     * @note the exact value of the internal notifications-counter isn't used directly by the WaitCondition
     *       class itself (as long as the counter is greater than zero, Wait() will return ASAP), but it can
     *       be passed to next caller of Wait() for that caller to examine, if it cares to.
     * @returns B_NO_ERROR on success, or another value on failure.
     */
   status_t Notify(uint32 increaseBy=1) const {return NotifyAux(increaseBy);}

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD std::condition_variable & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD pthread_cond_t & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD CONDITION_VARIABLE & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end condition-variable implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD QWaitCondition & GetNativeConditionVariableImplementation() const {return _conditionVariable;}
#endif

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD std::mutex & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_USE_PTHREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD pthread_mutex_t & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD CRITICAL_SECTION & GetNativeMutexImplementation() const {return _conditionMutex;}
#elif defined(MUSCLE_QT_HAS_THREADS)
   /** Returns a reference to our back-end mutex implementation object.  Don't call this method from code that is meant to remain portable! */
   MUSCLE_NODISCARD QMutex & GetNativeMutexImplementation() const {return _conditionMutex;}
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

   mutable uint32 _pendingNotificationsCount;

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

   // Should be called with our internal mutex locked!
   void FlushNotificationsCount(uint32 & retNotificationsCount) const
   {
      retNotificationsCount = _pendingNotificationsCount;
      _pendingNotificationsCount = 0;
   }

   status_t WaitAux(uint32 & retNotificationsCount) const
   {
      status_t ret;
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      std::unique_lock<std::mutex> lockGuard(_conditionMutex);
      _conditionVariable.wait(lockGuard, [this]{return (_pendingNotificationsCount>0);});
      FlushNotificationsCount(retNotificationsCount);
#elif defined(MUSCLE_USE_PTHREADS)
      if (pthread_mutex_lock(&_conditionMutex) == 0)
      {
         while(_pendingNotificationsCount == 0)
         {
            const int pret = pthread_cond_wait(&_conditionVariable, &_conditionMutex);
            if (pret != 0)
            {
               ret = B_ERRNUM(pret);
               break;
            }
         }
         if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
         (void) pthread_mutex_unlock(&_conditionMutex);
      }
      else ret = B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      while(_pendingNotificationsCount == 0)
      {
         if (SleepConditionVariableCS(&_conditionVariable, &_conditionMutex, INFINITE) == false)
         {
            ret = (GetLastError() == ERROR_TIMEOUT) ? B_TIMED_OUT : B_ERRNO;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      while(_pendingNotificationsCount == 0)
      {
         if (_conditionVariable.wait(&_conditionMutex) == false)
         {
            ret = B_TIMED_OUT;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
      _conditionMutex.unlock();
#endif
      return ret;
   }

   status_t WaitUntilAux(uint64 wakeupTime, uint32 & retNotificationsCount) const
   {
      int64 timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
      if (timeDeltaMicros <= 0) return B_TIMED_OUT;

      status_t ret;
#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      {
         std::unique_lock<std::mutex> lockGuard(_conditionMutex);
         auto timeoutTime = std::chrono::system_clock::now() + std::chrono::microseconds(timeDeltaMicros);
         if (_conditionVariable.wait_until(lockGuard, timeoutTime, [this](){return (_pendingNotificationsCount>0);}) == false) ret = B_TIMED_OUT;
         if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
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
            while(_pendingNotificationsCount == 0)
            {
               const int pret = pthread_cond_timedwait(&_conditionVariable, &_conditionMutex, &realTimeClockThen);
               if (pret != 0) {ret = (pret == ETIMEDOUT) ? B_TIMED_OUT : B_ERRNUM(pret); break;}
            }
         }
         if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
         (void) pthread_mutex_unlock(&_conditionMutex);
      }
      else ret = B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      while(_pendingNotificationsCount == 0)
      {
         timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
         if ((timeDeltaMicros <= 0)||(SleepConditionVariableCS(&_conditionVariable, &_conditionMutex, (DWORD) MicrosToMillis(timeDeltaMicros)) == false))
         {
            ret = (GetLastError() == ERROR_TIMEOUT) ? B_TIMED_OUT : B_ERRNO;  // timeout shouldn't ever happen, but just for form's sake
            break;
         }
      }
      if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      while(_pendingNotificationsCount == 0)
      {
         timeDeltaMicros = wakeupTime-GetRunTime64();  // how far in the future the wakeup-time is, in microseconds
         if ((timeDeltaMicros <= 0)||(_conditionVariable.wait(&_conditionMutex, (unsigned long) MicrosToMillis(timeDeltaMicros)) == false))
         {
            ret = B_TIMED_OUT;
            break;
         }
      }
      if (ret.IsOK()) FlushNotificationsCount(retNotificationsCount);
      _conditionMutex.unlock();
#endif
      return ret;
   }

   // Should be called with our internal mutex locked!
   void IncreaseNotificationsCount(uint32 increaseBy) const
   {
      const uint32 newCount = _pendingNotificationsCount + increaseBy;
      _pendingNotificationsCount = (newCount > _pendingNotificationsCount) ? newCount : MUSCLE_NO_LIMIT;  // saturating addition
   }

   status_t NotifyAux(uint32 increaseBy) const
   {
      status_t ret;

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
      std::unique_lock<std::mutex> lockGuard(_conditionMutex);
      if (_pendingNotificationsCount == 0) _conditionVariable.notify_one();
      IncreaseNotificationsCount(increaseBy);
#elif defined(MUSCLE_USE_PTHREADS)
      if (pthread_mutex_lock(&_conditionMutex) == 0)
      {
         int pret;

              if (_pendingNotificationsCount > 0)                         IncreaseNotificationsCount(increaseBy);
         else if ((pret = pthread_cond_signal(&_conditionVariable)) == 0) IncreaseNotificationsCount(increaseBy);
         else                                                             ret = B_ERRNUM(pret);

         (void) pthread_mutex_unlock(&_conditionMutex);
      }
      else ret = B_LOCK_FAILED;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      EnterCriticalSection(&_conditionMutex);
      if (_pendingNotificationsCount == 0) WakeConditionVariable(&_conditionVariable);
      IncreaseNotificationsCount(increaseBy);
      LeaveCriticalSection(&_conditionMutex);
#elif defined(MUSCLE_QT_HAS_THREADS)
      _conditionMutex.lock();
      if (_pendingNotificationsCount == 0) _conditionVariable.wakeOne();
      IncreaseNotificationsCount(increaseBy);
      _conditionMutex.unlock();
#endif

      return ret;
   }
};

} // end namespace muscle

#endif
