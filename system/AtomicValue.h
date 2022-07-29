/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAtomicValue_h
#define MuscleAtomicValue_h

#if defined(MUSCLE_AVOID_CPLUSPLUS11)
# error "The AtomicValue class requires C++11 or later to use, be sure you have C++11 or later mode set on your compiler!
#endif

#include <atomic>
#include "support/MuscleSupport.h"

namespace muscle {

/** This class is useful as a way to safe, lock-free way to set a non-trivial value in
  * one thread and read that value from a different thread.
  *
  * It works by storing newly passed-in values to different locations, so that when the
  * value is updated by the writing-thread, it writes to a different memory location
  * than the one that the reading-thread might be in the middle of reading from.
  * That way, the reading-thread's "old" copy of the value is in no danger of being
  * modified while the reading-thread is in the middle of using it.
  */
template<typename T, uint32 ATOMIC_BUFFER_SIZE=8> class AtomicValue MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  Our value will be default-initialized. */
   AtomicValue() : _readIndex(0), _writeIndex(0) {/* empty */}

   /** Explicit constructor.
     * @param val the initial value for our held value
     */
   AtomicValue(const T & val) : _readIndex(0), _writeIndex(0) {_buffer[_readIndex] = val;}

   /** Returns a copy of the current state of our held value */
   T GetValue() const {return _buffer[_readIndex % ATOMIC_BUFFER_SIZE];}

   /** Returns a read-only reference to the current state of our held value.
     * @note that this reference may not remain valid for long, so if you call this
     *       method, be sure to read any data you need from the reference quickly.
     */
   const T & GetValueRef() const {return _buffer[_readIndex % ATOMIC_BUFFER_SIZE];}

   /** Attempts to set our held value to a new value in a thread-safe fashion.
     * @param newValue the new value to set
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT if we couldn't perform the set because our buffer-queue was full
     */
   status_t SetValue(const T & newValue)
   {
      uint32 oldReadIndex = _readIndex.load() % ATOMIC_BUFFER_SIZE;
      while(1)
      {
         const uint32 newWriteIndex = (++_writeIndex % ATOMIC_BUFFER_SIZE);
         if (newWriteIndex == oldReadIndex) return B_BAD_OBJECT;  // out of buffer space!

         _buffer[newWriteIndex] = newValue;
         if (_readIndex.compare_exchange_strong(oldReadIndex, newWriteIndex, std::memory_order_release, std::memory_order_relaxed)) break;
      }
      return B_NO_ERROR;
   }

   /** Returns the size of our internal values-array, as specified by our template argument */
   uint32 GetNumValues() const {return ATOMIC_BUFFER_SIZE;}

   /** Returns a pointer to our internal-values array.  Don't call this unless you know what you are doing! */
   T * GetInternalValuesArray() {return _buffer;}

   /** Returns a read-only pointer to our internal-values array.  Don't call this unless you know what you are doing! */
   const T * GetInternalValuesArray() const {return _buffer;}

private:
   std::atomic<uint32> _readIndex;
   std::atomic<uint32> _writeIndex;

   T _buffer[ATOMIC_BUFFER_SIZE];
};

} // end namespace muscle

#endif
