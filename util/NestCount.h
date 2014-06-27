/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleNestCount_h
#define MuscleNestCount_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This class represents a counter of nested function calls.  It is essentially just a uint32,
  * but made into a class so that it can be auto-initialized to zero, protected from arbitrary
  * value changes, etc. 
  */
class NestCount
{
public:
   /** Default constructor.  Sets the nest count to zero. */
   NestCount() : _count(0) {/* empty */}

   /** Increments our value, and returns true iff the new value is one. */
   bool Increment() {return (++_count == 1);}

   /** Decrements our value, and returns true iff the new value is zero. */
   bool Decrement() {MASSERT(_count>0, "NestCount Decremented to below zero!"); return (--_count == 0);}

   /** Returns the current value */
   uint32 GetCount() const {return _count;}

   /** Returns true iff nesting is currently active (i.e. if our counter is non-zero) */
   bool IsInBatch() const {return (_count > 0);}

   /** Returns true iff we are in the outermost nesting level of the batch */
   bool IsOutermost() const {return (_count == 1);}

   /** Sets the count to the specified value.  In general it should not be necessary to call
     * this method, so don't call it unless you know what you are doing!
     */
   void SetCount(uint32 c) {_count = c;}

private:
   uint32 _count;
};

/** A trivial little class that simply increments the NestCount in its constructor
 *  and decrements the NestCount in its destructor.  It's useful for reliably
 *  tracking call-nest-counts in functions with multiple return() points.
 */
class NestCountGuard
{
public:
   /** Constructor.  Increments the specified counter value.
     * @param count A reference to the uint32 that our constructor will
     *              increment, and our destructor will decrement.
     */
   NestCountGuard(NestCount & count) : _count(count) {(void) _count.Increment();}

   /** Destructor.  Decrements our associated counter value. */
   ~NestCountGuard() {(void) _count.Decrement();}

   /** Returns our NestCount object's current count. */
   uint32 GetNestCount() const {return _count.GetCount();}

   /** Returns true iff nesting is currently active (i.e. if our counter is non-zero) */
   bool IsInBatch() const {return _count.IsInBatch();}

   /** Returns true iff we are the outermost of the nested calls to our NestCount */
   bool IsOutermost() const {return _count.IsOutermost();}

private:
   NestCount & _count;
};

}; // end namespace muscle

#endif
