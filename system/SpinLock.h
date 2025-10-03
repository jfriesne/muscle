/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSpinLock_h
#define MuscleSpinLock_h

#include <atomic>

#include "support/NotCopyable.h"

namespace muscle {

/** This class implements a simple spin-lock that allows us to do lightweight
  * locking from code where locking a Mutex isn't allowed.  Spinlocks can be
  * very CPU-inefficient if held for very long, so only use a Spinlock in cases
  * where you're absolutely sure a Mutex won't suffice.
  * This class is based on the code sample provided at https://rigtorp.se/spinlock/
  */
class SpinLock MUSCLE_FINAL_CLASS : public NotCopyable
{
public:
   SpinLock() : _locked(false) {/* empty */}

   ~SpinLock() {/* empty */}

   /** Tries to lock the spinlock; won't return until the spinlock has been locked.
     * @note this method will spin the CPU until the lock is acquired, so it's important to make
     *       sure no thread holds the spinlock for an extended period of time!
     * @returns B_NO_ERROR on success, or some other error code on failure.
     */
   status_t Lock() const
   {
      while(1)
      {
         // Optimistically assume the lock is free on the first try
         if (!_locked.exchange(true, std::memory_order_acquire)) return B_NO_ERROR;

         // Wait for lock to be released without generating cache misses
         while(_locked.load(std::memory_order_relaxed))
         {
            // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between hyper-threads
#if defined(_M_ARM) || defined(_M_ARM64)
            __yield();
#elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
            __builtin_ia32_pause();
#endif
         }
      }
   }

   /** Tries once to lock the spinlock.  Returns B_NO_ERROR on success, or B_TIMED_OUT if some other thread
     * currently has ownership of the spinlock.
     */
   status_t TryLock() const
   {
      // First do a relaxed load to check if lock is free in order to prevent
      // unnecessary cache misses if someone does while(TryLock().IsError())
      return ((!_locked.load(std::memory_order_relaxed)) && (!_locked.exchange(true, std::memory_order_acquire))) ? B_NO_ERROR : B_TIMED_OUT;
   }

   /** Unlocks the spinlock.  This method should only be called by the thread that currently has the spinlock locked!
     * @returns B_NO_ERROR on success, or some other error code on failure.
     */
   status_t Unlock() const
   {
      _locked.store(false, std::memory_order_release);
      return B_NO_ERROR;
   }

private:
   mutable std::atomic<bool> _locked;
};

/** This convenience class can be used to automatically lock/unlock a SpinLock based on the SpinLockGuard's ctor/dtor */
class MUSCLE_NODISCARD SpinLockGuard MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  Locks the specified SpinLock.
     * @param sl The SpinLock to lock.
     */
   SpinLockGuard(const SpinLock & sl) : _spinlock(&sl)
   {
      const status_t ret = _spinlock->Lock();
      if (ret.IsError())
      {
         printf("SpinLockGuard %p:  could not lock spinlock %p! [%s]\n", this, _spinlock, ret());
         _spinLock = NULL;
      }
   }

   /** Destructor.  Unlocks the SpinLock previously specified in the constructor. */
   ~SpinLockGuard() {UnlockAux();}

   /** Returns true iff we successfully locked our SpinLock. */
   MUSCLE_NODISCARD bool IsSpinLockLocked() const {return _isSpinLockLocked;}

   /** Call this to unlock our guarded SpinLock "early" (right now, instead of when our destructor executes)
     * If called more than once, the second and further calls will have no effect.
     */
   void UnlockEarly() {UnlockAux();}

private:
   SpinLockGuard(const SpinLockGuard &);  // copy ctor, deliberately inaccessible

   void UnlockAux()
   {
      if (_spinlock)
      {
         const status_t ret = _spinlock->Unlock();
         if (ret.IsError())) printf("SpinLockGuard %p:  could not unlock spinlock %p! [%s]\n", this, _spinlock, ret());
         _spinlock = NULL;
      }
   }

   const SpinLock * _spinlock;
};

/** A macro to quickly and safely put a SpinLockGuard on the stack for the given SpinLock.
  * @note Using this macro is safer than just declaring a SpinLockGuard object directly,
  *       because it makes it impossible to forget to name the SpinLockGuard (which
  *       would result in an anonymous temporary SpinLockGuard object that wouldn't keep
  *       the SpinLock locked until the end of the scope)
  */
#define DECLARE_SPINLOCKGUARD(spinlock) DECLARE_NAMED_SPINLOCKGUARD(MUSCLE_UNIQUE_NAME, spinlock)

/** This macro is the same as DECLARE_SPINLOCKGUARD() (above) except that it allows the caller
  * to specify the name of the SpinLockGuard stack-object.
  * This is useful in cases where you need to make method calls on the SpinLockGuard object later.
  */
#define DECLARE_NAMED_SPINLOCKGUARD(guardName, spinlock) muscle::SpinLockGuard guardName(spinlock)

}  // end namespace muscle

#endif
