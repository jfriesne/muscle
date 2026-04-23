/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAtomicCounter_h
#define MuscleAtomicCounter_h

#include "support/MuscleSupport.h"

#ifdef MUSCLE_SINGLE_THREAD_ONLY
  // empty
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
# include <atomic>
#elif defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
  // empty
#elif defined(WIN32)
  // empty
#elif defined(__APPLE__)
# include <libkern/OSAtomic.h>
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY) || defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
  // empty
#elif defined(MUSCLE_USE_PTHREADS) || defined(ANDROID)
# define MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS
#endif

#if defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
# include "system/Mutex.h"
# ifndef MUSCLE_MUTEX_POOL_SIZE
#  define MUSCLE_MUTEX_POOL_SIZE 256
# endif
#endif

namespace muscle {

#if defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
extern Mutex * _muscleAtomicMutexes;
MUSCLE_NODISCARD static inline int32 DoMutexAtomicIncrement(volatile int32 * count, int32 delta)
{
   int32 ret;
   if (_muscleAtomicMutexes)
   {
      DECLARE_MUTEXGUARD(_muscleAtomicMutexes[(((uint32)((uintptr)count))/sizeof(int32))%MUSCLE_MUTEX_POOL_SIZE]);  // double-cast for AMD64
      ret = *count = (*count + delta);
   }
   else
   {
      // if _muscleAtomicMutexes isn't allocated, then we're in process-setup or process-shutdown, so there are no multiple threads at the moment, so we can just do this
      ret = *count = (*count + delta);
   }
   return ret;
}
#endif

/** A minimalist cross-platform atomic-counter variable.
  * If compiled with -DMUSCLE_SINGLE_THREAD_ONLY, it degenerates to a regular old integer-counter variable,
  * which is very lightweight and portable, but of course will only work reliably in single-threaded environments.
  */
class MUSCLE_NODISCARD AtomicCounter MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.  The count value is initialized to the specified value.
     * @param count the value to initialize the atomic counter to.  Defaults to zero.
     */
   MUSCLE_CONSTEXPR AtomicCounter(int32 count = 0) : _count(count) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   AtomicCounter(const AtomicCounter & rhs) {SetCount(rhs.GetCount());}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   AtomicCounter & operator=(const AtomicCounter & rhs) {SetCount(rhs.GetCount()); return *this;}

   /** Atomically increments our counter by one.
     * Returns true iff the count's new value is 1; returns false
     *              if the count's new value is any other value.
     */
   MUSCLE_NODISCARD inline bool AtomicIncrement()
   {
#if defined(MUSCLE_SINGLE_THREAD_ONLY) || !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return (++_count == 1);
#elif defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
      return (DoMutexAtomicIncrement(&_count, 1) == 1);
#elif defined(WIN32)
      return (InterlockedIncrement(&_count) == 1);
#elif defined(__APPLE__)
      return (OSAtomicIncrement32Barrier(&_count) == 1);
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY)
      volatile int * p = &_count;
      int tmp;  // tmp will be set to the value after the increment
      asm volatile(
         "1:     lwarx   %0,0,%1\n"
         "       addic   %0,%0,1\n"
         "       stwcx.  %0,0,%1\n"
         "       bne-    1b"
         : "=&r" (tmp)
         : "r" (p)
         : "cc", "memory");
      return (tmp == 1);
#elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
      int value = 1;  // the increment-by value
      asm volatile(
         "lock; xaddl %%eax, %2;"
         :"=a" (value)                 // Output
         : "a" (value), "m" (_count)  // Input
         :"memory");
      return (value==0);  // at this point value contains the counter's pre-increment value
#else
# error "No atomic increment supplied for this OS!  Add it here in AtomicCount.h, remove -DMUSCLE_AVOID_CPLUSPLUS11 from your compiler-defines to use std::atomic, or put -DMUSCLE_SINGLE_THREAD_ONLY in your compiler-defines if you will not be using multithreading."
#endif
   }

   /** Atomically decrements our counter by one.
     * @returns true iff the new value of our count is 0;
     *               returns false if it is any other value
     */
   MUSCLE_NODISCARD inline bool AtomicDecrement()
   {
#if defined(MUSCLE_SINGLE_THREAD_ONLY) || !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return (--_count == 0);
#elif defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
      return (DoMutexAtomicIncrement(&_count, -1) == 0);
#elif defined(WIN32)
      return (InterlockedDecrement(&_count) == 0);
#elif defined(__APPLE__)
      return (OSAtomicDecrement32Barrier(&_count) == 0);
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY)
      volatile int * p = &_count;
      int tmp;   // tmp will be set to the value after the decrement
      asm volatile(
         "1:     lwarx   %0,0,%1\n"
         "       addic   %0,%0,-1\n"  // addic allows r0, addi doesn't
         "       stwcx.  %0,0,%1\n"
         "       bne-    1b"
         : "=&r" (tmp)
         : "r" (p)
         : "cc", "memory");
      return(tmp == 0);
#elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
      bool isZero;
      volatile int * p = &_count;
      asm volatile(
         "lock; decl (%1)\n"
         "sete %0"
         : "=q" (isZero)
         : "q" (p)
         : "cc", "memory"
         );
      return isZero;
#else
# error "No atomic decrement supplied for this OS!  Add it here in AtomicCount.h, or remove -DMUSCLE_AVOID_CPLUSPLUS11 from your compiler-defines to use std::atomic, or put -DMUSCLE_SINGLE_THREAD_ONLY in your compiler-defines if you will not be using multithreading."
#endif
   }

   /** A slightly more user-friendly wrapper aound a strong-compare-and-swap operation.
     * This method will attempt to atomically change this AtomicCounter's value
     * from (fromOldValue) to (toNewValue).
     * @param fromOldValue the value that the AtomicCounter must have at the start
     *                     of the atomic operation, for the operation to succeed.
     * @param toNewValue the value that the AtomicCounter will have at the end of the
     *                   atomic operationon success.
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT if the AtomicCounter's current
     *          value was something other than (fromOldValue) at the instant this method's
     *          atomic operation began.
     */
   status_t ConditionalSetCount(int32 fromOldValue, int32 toNewValue)
   {
#if defined(MUSCLE_SINGLE_THREAD_ONLY)
      return NonAtomicConditionalSetCount(fromOldValue, toNewValue);
#elif defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
      if (_muscleAtomicMutexes)
      {
         DECLARE_MUTEXGUARD(_muscleAtomicMutexes[(((uint32)((uintptr)&_count))/sizeof(int32))%MUSCLE_MUTEX_POOL_SIZE]);  // double-cast for AMD64
         return NonAtomicConditionalSetCount(fromOldValue, toNewValue);
      }
      else
      {
         // if _muscleAtomicMutexes isn't allocated, then we're in process-setup or process-shutdown, so there are no multiple threads at the moment, so we can just do this
         return NonAtomicConditionalSetCount(fromOldValue, toNewValue);
      }
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return _count.compare_exchange_strong(fromOldValue, toNewValue) ? B_NO_ERROR : B_BAD_OBJECT;
#elif defined(WIN32)
      return (InterlockedCompareExchange(&_count, toNewValue, fromOldValue) == fromOldValue) ? B_NO_ERROR : B_BAD_OBJECT;
#elif defined(__APPLE__)
      return OSAtomicCompareAndSwap32Barrier(fromOldValue, toNewValue, &_count) ? B_NO_ERROR : B_BAD_OBJECT;
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY)
      LogTime(MUSCLE_LOG_ERROR, "AtomicCounter::ConditionalSetCount() isn't implemented for PPC assembly!\n");  // just to make sure the user knows why his program isn't working
      return B_UNIMPLEMENTED;  // for now; if/when I need to implement this, I will
#elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
      LogTime(MUSCLE_LOG_ERROR, "AtomicCounter::ConditionalSetCount() isn't implemented for x86 assembly!\n");  // just to make sure the user knows why his program isn't working
      return B_UNIMPLEMENTED;  // for now; if/when I need to implement this, I will
#else
# error "No compare-and-swap routine supplied for this OS!  Add it here in AtomicCount.h, or remove -DMUSCLE_AVOID_CPLUSPLUS11 from your compiler-defines to use std::atomic, or put -DMUSCLE_SINGLE_THREAD_ONLY in your compiler-defines if you will not be using multithreading."
#endif
   }

   /** Sets the current value of this counter.
     * Be careful when using this function in multithreaded
     * environments, it can easily lead to race conditions
     * if you don't know what you are doing!
     * @param c the new count-value to set
     */
   void SetCount(int32 c) {_count = c;}

   /** Returns the current value of this counter.
     * Be careful when using this function in multithreaded
     * environments, it can easily lead to race conditions
     * if you don't know what you are doing!
     */
   MUSCLE_NODISCARD int32 GetCount() const
   {
#if !defined(MUSCLE_SINGLE_THREAD_ONLY) && !defined(MUSCLE_AVOID_CPLUSPLUS11)
      return (int32) _count.load();
#else
      return (int32) _count;
#endif
   }

private:
#if defined(MUSCLE_SINGLE_THREAD_ONLY)
   int32 _count;
#elif !defined(MUSCLE_AVOID_CPLUSPLUS11)
   std::atomic<int32> _count;
#elif defined(WIN32)
   volatile long _count;
#elif defined(__APPLE__)
   volatile int32_t _count;
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY) || defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
   volatile int _count;
#else
   volatile int32 _count;
#endif

#if defined(MUSCLE_SINGLE_THREAD_ONLY) || defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
   status_t NonAtomicConditionalSetCount(int32 fromOldValue, int32 toNewValue)
   {
      if (GetCount() != fromOldValue) return B_BAD_OBJECT;
      SetCount(toNewValue);
      return B_NO_ERROR;
   }
#endif
};

#if !defined(DOXYGEN_SHOULD_IGNORE_THIS) && !defined(MUSCLE_INLINE_LOGGING) && !defined(MUSCLE_MINIMALIST_LOGGING) && !defined(MUSCLE_DISABLE_LOGGING)
class AtomicCounter;
namespace muscle_private {extern AtomicCounter _maxLogThreshold;}  // implementation detail; exposed for efficiency
MUSCLE_NODISCARD inline int GetMaxLogLevel() {return muscle_private::_maxLogThreshold.GetCount();}
#endif

} // end namespace muscle

#endif
