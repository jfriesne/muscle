/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePointerAndBool_h
#define MusclePointerAndBool_h

#include "support/MuscleSupport.h"

#ifdef __clang_analyzer__
# ifndef MUSCLE_AVOID_BITSTUFFING
#  define MUSCLE_AVOID_BITSTUFFING // poor ClangSA just can't handle the bit-stuffing
# endif
#endif

#if defined(MUSCLE_AVOID_BITSTUFFING) && !defined(MUSCLE_AVOID_DOUBLE_BITSTUFFING)
# define MUSCLE_AVOID_DOUBLE_BITSTUFFING  // if you can't do single-bitstuffing, we'll assume you can't do double-bitstuffing either
#endif

namespace muscle {

/** This class abuses the fact that objects are word-aligned on most hardware to allow us to store
  * a boolean value in the low bit of the pointer.  This is useful for reducing the amount of memory
  * space a class object takes up -- on a 64-bit system it saves 7 and 7/8ths bytes for each
  * pointer+boolean pair used.  If you are compiling on a system where pointers-to-objects are not
  * required to be aligned to even addresses, you can define MUSCLE_AVOID_BITSTUFFING to force the
  * boolean to be declared as a separate member variable (at the cost of increased memory usage, of course).
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
     * @param boolVal the boolean value to hold.
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

   /** Swaps this object's state with the state of (rhs)
     * @param rhs another PointerAndBool to swap our contents with
     */
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

/** This class is similar to PointerAndBool except that this class holds a pointer
  * and *two* boolean values, stored in the two lowest bits of the pointer.
  * This is useful for reducing the amount of memory space a class object takes up,
  * but it only works on a system where pointers are required to be 4-byte-word aligned.
  * If you are compiling on a system where pointers-to-objects are not required to be
  * aligned multiples of 4 bytes, you can define MUSCLE_AVOID_DOUBLE_BITSTUFFING to force the
  * booleans to be declared as a separate member variable (at the cost of increased memory usage, of course).
  * Note that if MUSCLE_AVOID_BITSTUFFING is defined, then MUSCLE_AVOID_DOUBLE_BITSTUFFING
  * will also be implicitly defined.
  */
template <class T> class PointerAndBools
{
public:
   /** Default constructor. */
   PointerAndBools() : _pointer(NULL)
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      , _bits(0)
#endif
   {
      // empty
   }

   /** Constructor.
     * @param pointerVal the pointer value to hold
     * @param boolVal1 the first boolean value to hold.
     * @param boolVal2 the second boolean value to hold.
     */
   PointerAndBools(T * pointerVal, bool boolVal1, bool boolVal2)
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      : _pointer(pointerVal), _bits(BoolsToBits(boolVal1, boolVal2))
#endif
   {
#ifndef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      SetPointerAndBools(pointerVal, boolVal1, boolVal2);
#endif
   }

   /** Sets our held pointer value to the specified value.
     * @param pointerVal the new pointer value to store.  Must be an even value unless MUSCLE_AVOID_DOUBLE_BITSTUFFING is defined.
     */
   void SetPointer(T * pointerVal) {SetPointerAndBools(pointerVal, GetBool1(), GetBool2());}

   /** Returns our held pointer value, as previous set in the constructor or via SetPointer().  */
   T * GetPointer() const
   {
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      return _pointer;
#else
      return WithLowBitsCleared(_pointer);
#endif
   }

   /** Sets the first boolean value we hold.
     * @param boolVal1 The new first-boolean value to store.
     */
   void SetBool1(bool boolVal1) {SetPointerAndBools(GetPointer(), boolVal1, GetBool2());}

   /** Sets the second boolean value we hold.
     * @param boolVal2 The new second-boolean value to store.
     */
   void SetBool2(bool boolVal2) {SetPointerAndBools(GetPointer(), GetBool1(), boolVal2);}

   /** Sets both the pointer value and the boolean value at the same time.
     * @param pointer The new pointer value to store.
     * @param boolVal1 The new first boolean value to store.
     * @param boolVal2 The new second boolean value to store.
     */
   void SetPointerAndBools(T * pointer, bool boolVal1, bool boolVal2)
   {
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      _pointer = pointer;
      _bits    = BoolsToBits(boolVal1, boolVal2);
#else
       CheckAlignment(pointer);
       _pointer = WithLowBitsSet(pointer, boolVal1, boolVal2);
#endif
   }

   /** Returns the first boolean value we hold.  */
   bool GetBool1() const
   {
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      return ((_bits & 0x01) != 0);
#else
      return IsFirstLowBitSet(_pointer);
#endif
   }

   /** Returns the second boolean value we hold.  */
   bool GetBool2() const
   {
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      return ((_bits & 0x02) != 0);
#else
      return IsSecondLowBitSet(_pointer);
#endif
   }

   /** Returns this PointerAndBools to its default state (NULL, false) */
   void Reset()
   {
      _pointer = NULL;
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      _bits = 0;
#endif
   }

   /** Swaps this object's state with the state of (rhs)
     * @param rhs another PointerAndBools to swap our contents with
     */
   void SwapContents(PointerAndBools & rhs)
   {
      muscleSwap(_pointer, rhs._pointer);
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
      muscleSwap(_bits, rhs._bits);
#endif
   }

private:
   uint8 BoolsToBits(bool b1, bool b2) const {return ((b1?0x01:0x00)|(b2?0x02:0x00));}

#ifndef MUSCLE_AVOID_DOUBLE_BITSTUFFING
         T * WithLowBitsSet(          T * ptr, bool b1, bool b2) const {return (T*)(((uintptr)ptr) | ((uintptr)BoolsToBits(b1, b2)));}
         T * WithLowBitsCleared(      T * ptr) const {return (T*)(((uintptr)ptr)&~((uintptr)0x3));}
   bool IsFirstLowBitSet(       const T * ptr) const {return ((((uintptr)ptr)&((uintptr)0x1))!=0);}
   bool IsSecondLowBitSet(      const T * ptr) const {return ((((uintptr)ptr)&((uintptr)0x2))!=0);}
   bool AreAnyLowBitsSet(       const T * ptr) const {return ((((uintptr)ptr)&((uintptr)0x3))!=0);}
   void CheckAlignment(         const T * ptr) const
   {
      (void) ptr;  // avoid compiler warning if MASSERT has been defined to a no-op
      MASSERT((AreAnyLowBitsSet(ptr) == false), "Unaligned pointer detected!  PointerAndBools' bit-stuffing code can't handle that.  Either align your pointers to four-byte memory addresses, or recompile with -DMUSCLE_AVOID_DOUBLE_BITSTUFFING.");
   }
#endif

   T * _pointer;
#ifdef MUSCLE_AVOID_DOUBLE_BITSTUFFING
   uint8 _bits;
#endif
};

} // end namespace muscle

#endif
