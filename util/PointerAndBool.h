/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePointerAndBool_h
#define MusclePointerAndBool_h

#include "support/MuscleSupport.h"

namespace muscle {

/** This class abuses the fact that objects are word-aligned on most hardware to allow us to store
  * a boolean value in the low bit of the pointer.  This is useful for reducing the amount of memory
  * space a class object takes up -- on a 64-bit system it saves 7 and 7/8ths bytes for each
  * pointer+boolean pair used.  If you are compiling on a system where pointers-to-objects are not
  * required to be aligned to even addresses, you can define MUSCLE_AVOID_BITSTUFFING to force the
  * boolean to declared as a separate member variable (at the cost of increased memory usage, of course).
  */
template <class T> class PointerAndBool
{
public:
   /** Default constructor. */
   PointerAndBool() : _pointer(NULL)
#ifdef MUSCLE_AVOID_BITSTUFFING
      , _bool(false)
#endif
   {
      // empty
   }

   /** Constructor.
     * @param pointerVal the pointer value to hold
     * @param boolVal boolVal the boolean value to hold.
     */
   PointerAndBool(T * pointerVal, bool boolVal)
#ifdef MUSCLE_AVOID_BITSTUFFING
      : _pointer(pointerVal), _bool(boolVal)
#endif
   {
#ifndef MUSCLE_AVOID_BITSTUFFING
      SetPointerAndBool(pointerVal, boolVal);
#endif
   }

   /** Sets our held pointer value to the specified value.
     * @param pointerVal the new pointer value to store.  Must be an even value unless MUSCLE_AVOID_BITSTUFFING is defined.
     */
   void SetPointer(T * pointerVal) {SetPointerAndBool(pointerVal, GetBool());}

   /** Returns our held pointer value, as previous set in the constructor or via SetPointer().  */
   T * GetPointer() const
   {
#ifdef MUSCLE_AVOID_BITSTUFFING
      return _pointer;
#else
      return WithLowBitCleared(_pointer);
#endif
   }

   /** Sets the boolean value we hold.
     * @param boolVal The new boolean value to store.
     */
   void SetBool(bool boolVal) {SetPointerAndBool(GetPointer(), boolVal);}

   /** Sets both the pointer value and the boolean value at the same time.
     * @param pointer The new pointer value to store.
     * @param boolVal The new boolean value to store.
     */
   void SetPointerAndBool(T * pointer, bool boolVal)
   {
#ifdef MUSCLE_AVOID_BITSTUFFING
      _pointer = pointer;
      _bool    = boolVal;
#else
       CheckAlignment(pointer);
       _pointer = boolVal ? WithLowBitSet(pointer) : pointer;
#endif
   }

   /** Returns the boolean value we hold.  */
   bool GetBool() const
   {
#ifdef MUSCLE_AVOID_BITSTUFFING
      return _bool;
#else
      return IsLowBitSet(_pointer);
#endif
   }

   /** Returns this PointerAndBool to its default state (NULL, false) */
   void Reset()
   {
      _pointer = NULL;
#ifdef MUSCLE_AVOID_BITSTUFFING
      _bool = false;
#endif
   }

   /** Swaps this object's state with the state of (rhs) */
   void SwapContents(PointerAndBool & rhs)
   {
      muscleSwap(_pointer, rhs._pointer);
#ifdef MUSCLE_AVOID_BITSTUFFING
      muscleSwap(_bool, rhs._bool);
#endif
   }

private:
#ifndef MUSCLE_AVOID_BITSTUFFING
         T * WithLowBitSet(          T * ptr) const {return (T*)(((uintptr)ptr)| ((uintptr)0x1));}
         T * WithLowBitCleared(      T * ptr) const {return (T*)(((uintptr)ptr)&~((uintptr)0x1));}
   bool IsLowBitSet(           const T * ptr) const {return ((((uintptr)ptr)&((uintptr)0x1))!=0);}
   void CheckAlignment(        const T * ptr) const
   {
      (void) ptr;  // avoid compiler warning if MASSERT has been defined to a no-op
      MASSERT((IsLowBitSet(ptr) == false), "Unaligned pointer detected!  PointerAndBool's bit-stuffing code can't handle that.  Either align your pointers to even memory addresses, or recompile with -DMUSCLE_AVOID_BITSTUFFING.");
   }
#endif

   T * _pointer;
#ifdef MUSCLE_AVOID_BITSTUFFING
   bool _bool;
#endif
};

}; // end namespace muscle

#endif
