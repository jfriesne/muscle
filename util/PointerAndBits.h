/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePointerAndBits_h
#define MusclePointerAndBits_h

#include "support/MuscleSupport.h"

#ifdef __clang_analyzer__
# ifndef MUSCLE_AVOID_BITSTUFFING
#  define MUSCLE_AVOID_BITSTUFFING // poor ClangSA just can't handle the bit-stuffing
# endif
#endif

namespace muscle {

/** This class holds a pointer as well as a small set of miscellaneous data-bits, within a single word.
  *
  * It does this by stealing the least-significant bit(s) of the pointer to hold the bit-values,
  * which means it only works if the passed-in pointer is sufficiently well-aligned that the
  * low bits of the pointer are always zero.  If you attempt to pass in a pointer that is
  * not sufficiently well-aligned, a runtime assertion failure will be triggered.
  *
  * If you are compiling on a system where pointers-to-objects are not sufficiently well-aligned
  * for this to trick to work, you can define MUSCLE_AVOID_BITSTUFFING to force the
  * bits to be declared as a separate member-variable (at the cost of slightly increased memory
  * usage, of course).
  * @tparam T the type of object that our pointer will point to.
  * @tparam NumBits the number of low-bits of the pointer we will appropriate for data-storage.
  *                 Usually this number shouldn't be greater than 1 or 2.
  */
template <class T, unsigned int NumBits> class PointerAndBits
{
public:
   /** Default constructor.  Sets this object to a NULL pointer with all data-bits set to zero. */
   PointerAndBits() : _pointer(0)
#ifdef MUSCLE_AVOID_BITSTUFFING
      , _dataBits(0)
#endif
   {
      // empty
   }

   /** Constructor.
     * @param pointerVal the pointer value to hold
     * @param dataBits a bit-chord of bits to store along with the pointer.  Only the low bits may be set in this value!
     */
   PointerAndBits(T * pointerVal, uintptr dataBits) {SetPointerAndBits(pointerVal, dataBits);}

   /** Sets our held pointer value to the specified new value.  The current bit-chord values are retained.
     * @param pointerVal the new pointer value to store.  Must be a sufficiently well-aligned value, unless MUSCLE_AVOID_BITSTUFFING is defined.
     */
   void SetPointer(T * pointerVal) {SetPointerAndBits(pointerVal, GetBits());}

   /** Returns our held pointer value, as previously set in the constructor or via SetPointer().  */
   T * GetPointer() const
   {
#ifdef MUSCLE_AVOID_BITSTUFFING
      return (T*) _pointer;
#else
      return (T*) (_pointer & ~_allDataBitsMask);
#endif
   }

   /** Sets our bit-chord of data-bits to the specified new value.  The current pointer-value is retained.
     * @param bits The new bit-chord to store.  Only the lowest (NumBits) bits may be set in this value, unless MUSCLE_AVOID_BITSTUFFING is defined.
     */
   void SetBits(uintptr bits) {SetPointerAndBits(GetPointer(), bits);}

   /** Returns our current bit-chord of data-bits */
   uintptr GetBits() const
   {
#ifdef MUSCLE_AVOID_BITSTUFFING
      return _dataBits;
#else
      return (_pointer & _allDataBitsMask);
#endif
   }

   /** Convenience method:  Sets one of our bits to a new boolean value
     * @param whichBit the index of the bit to set.  Must be less than (NumBits).
     * @param bitValue True to set the bit, or false to clear it.
     */
   void SetBit(unsigned int whichBit, bool bitValue)
   {
      MASSERT(whichBit<NumBits, "PointerAndBits::SetBit()  Invalid bit-index!");
      SetPointerAndBits(GetPointer(), bitValue ? (GetBits() | (1<<whichBit)) : (GetBits() & ~(1<<whichBit)));
   }

   /** Sets both the pointer-value and the bit-chord.
     * @param pointer The new pointer-value to store.  This pointer must be sufficiently well-aligned that its (NumBits) lower bits are all zero!
     * @param bits The new bit-chord to store.  Only the lowest (NumBits) bits may be set in this value, unless MUSCLE_AVOID_BITSTUFFING is defined.
     */
   void SetPointerAndBits(T * pointer, uintptr bits)
   {
      const uintptr pVal = (uintptr) pointer;

      MASSERT(((bits & ~_allDataBitsMask) == 0), "SetPointerAndBits():  Bad bit-chord detected!  Bit-chords passed to PointerAndBits can only have the low (NumBits) bits set!");

#ifdef MUSCLE_AVOID_BITSTUFFING
      _pointer  = pVal;
      _dataBits = bits;
#else
      MASSERT(((pVal & _allDataBitsMask) == 0), "SetPointerAndBits():  Unaligned pointer detected!  PointerAndBits' bit-stuffing code can't handle that.  Either align your pointers so the low bits are always zero, or recompile with -DMUSCLE_AVOID_BITSTUFFING.");
      _pointer = pVal | bits;
#endif
   }

   /** Convenience method:  Returns the state of the (nth) bit, as a boolean.
     * @param whichBit the index of the bit to retrieve the value of.  Must be less than (NumBits).
     */
   bool GetBit(unsigned int whichBit) const
   {
      MASSERT(whichBit<NumBits, "PointerAndBits::GetBit()  Invalid boolean index!");
#ifdef MUSCLE_AVOID_BITSTUFFING
      return ((_dataBits & (1<<whichBit)) != 0);
#else
      return (( _pointer & (1<<whichBit)) != 0);
#endif
   }

   /** Returns this PointerAndBits to its default state (i.e. (NULL, 0)) */
   void Reset()
   {
      _pointer = 0;
#ifdef MUSCLE_AVOID_BITSTUFFING
      _dataBits = 0;
#endif
   }

   /** Swaps this object's state with the state of (rhs)
     * @param rhs another PointerAndBits to swap our contents with
     */
   void SwapContents(PointerAndBits & rhs) MUSCLE_NOEXCEPT
   {
      muscleSwap(_pointer, rhs._pointer);
#ifdef MUSCLE_AVOID_BITSTUFFING
      muscleSwap(_dataBits, rhs._dataBits);
#endif
   }

   /** Returns a hash code for this object */
   uint32 HashCode() const
   {
      uint32 ret = CalculateHashCode(_pointer);
#ifdef MUSCLE_AVOID_BITSTUFFING
      ret += CalculateHashCode(_dataBits);
#endif
      return ret;
   }

private:
   static const uintptr _allDataBitsMask = ((NumBits > 0) ? ((1<<NumBits)-1) : 0); // bit-chord with all allowed data-bits in it set; used for masking

   uintptr _pointer;
#ifdef MUSCLE_AVOID_BITSTUFFING
   uintptr _dataBits;
#endif
};

/** Convenience method:  Given a bit-index and a boolean-value, returns a bit-chord with the corresponding bit set, or 0 if the boolean is false.
  * @param whichBit the index of the bit to (possibly) set.
  * @param b a boolean value.  If true, we'll return a bit-chord with the (whichBit)'th bit set.  If false, we'll return 0.
  */
static inline uintptr BooleanToBitChord(unsigned int whichBit, bool b) {return b ? (((uintptr)1)<<whichBit) : (uintptr)0;}

/** Convenience method:  Returns a bit-chord with the lowest bit set, or 0
  * @param b0 the boolean value to encode as the least-significant bit
  */
static inline MUSCLE_CONSTEXPR uintptr BooleansToBitChord(bool b0) {return BooleanToBitChord(0, b0);}

/** Convenience method:  Returns a bit-chord with the lowest bits set, or 0
  * @param b0 the boolean value to encode as the least-significant bit
  * @param b1 the boolean value to encode as the second-least-significant bit
  */
static inline MUSCLE_CONSTEXPR uintptr BooleansToBitChord(bool b0, bool b1) {return BooleanToBitChord(1, b1) | BooleansToBitChord(b0);}

/** Convenience method:  Returns a bit-chord with the lowest bits set, or 0
  * @param b0 the boolean value to encode as the least-significant bit
  * @param b1 the boolean value to encode as the second-least-significant bit
  * @param b2 the boolean value to encode as the third-least-significant bit
  */
static inline MUSCLE_CONSTEXPR uintptr BooleansToBitChord(bool b0, bool b1, bool b2) {return BooleanToBitChord(2, b2) | BooleansToBitChord(b0, b1);}

/** Convenience method:  Returns a bit-chord with the lowest bits set, or 0
  * @param b0 the boolean value to encode as the least-significant bit
  * @param b1 the boolean value to encode as the second-least-significant bit
  * @param b2 the boolean value to encode as the third-least-significant bit
  * @param b3 the boolean value to encode as the fourth-least-significant bit
  */
static inline MUSCLE_CONSTEXPR uintptr BooleansToBitChord(bool b0, bool b1, bool b2, bool b3) {return BooleanToBitChord(3, b3) | BooleansToBitChord(b0, b1, b2);}

} // end namespace muscle

#endif
