/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleBitChord_h
#define MuscleBitChord_h

#include "support/Void.h"
#include "support/MuscleSupport.h"
#include "support/PseudoFlattenable.h"
#include "util/String.h"

namespace muscle {

#if defined(MUSCLE_AVOID_CPLUSPLUS11) && !defined (MUSCLE_AVOID_CPLUSPLUS11_BITCHORD)
# define MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
#endif

/** A templated class for implement an N-bit-long bit-chord.  Useful for doing efficient parallel boolean operations
  * on bits-strings of lengths that can't fit in any of the standard integer types, and also for holding bit-shifted 
  * boolean flags in a "safe" container so that you can query or manipulate the flags via human-readable method-calls 
  * instead of easy-to-get-wrong bit-shifting operators.
  *
  * @note that the TagClass template-parameter's value isn't directly used for anything; it's provided only as a way 
  *       to help make unrelated BitChords' template-instantiations unique and not-implicitly-convertible to each other 
  *       even if they happen to specify the same value for the NumBits template-parameter.  See the 
  *       DECLARE_BITCHORD_FLAGS_TYPE macro at the bottom of BitChord.h for more information.
  */
template <uint32 NumBits, class TagClass=Void> class BitChord : public PseudoFlattenable
{
public:
   /** Default constructor */
   BitChord() {ClearAllBits();}

#ifndef MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
   /** Variadic constructor; takes a list of bit-indices indicating which bits should be set in this BitChord.
     * @param bits any number of bit-index arguments may be supplied, and the correspond bits will be set.
     * @note for example, BitChord bc(a, b, c); is equivalent to BitChord bc; bc.SetBit(a); bc.SetBit(b); bc.SetBit(c);
     * @note this constructor can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> explicit BitChord(Bits... bits) {ClearAllBits(); const int arr[] {bits...}; for (auto bit : arr) SetBit(bit);}
#endif

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   BitChord(const BitChord & rhs) {*this = rhs;}

   /** Destructor */
   ~BitChord() {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   BitChord & operator =(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] = rhs._words[i]; return *this;}

   /** Returns the state of the specified bit
     * @param whichBit the index of the bit to query (e.g. 0 indicates the first bit, 1 indicates the second bit, 2 indicates the third bit, and so on)
     * @returns true iff the bit was set
     */
   bool IsBitSet(uint32 whichBit) const
   {
      MASSERT(whichBit < NumBits, "BitChord::IsBitSet:  whichBit was out of range!\n");
      return IsBitSetUnchecked(whichBit);
   }

   /** Sets the state of the specified bit to 1.
     * @param whichBit the index of the bit to set to 1 (e.g. 0 indicates the first bit, 1 indicates the second bit, 2 indicates the third bit, and so on)
     */
   void SetBit(uint32 whichBit) 
   {
      MASSERT(whichBit < NumBits, "BitChord::SetBit:  whichBit was out of range!\n");
      SetBitUnchecked(whichBit);
   }

   /** Sets the state of the specified bit to the specified boolean value.
     * @param whichBit the index of the bit to set (e.g. 0 indicates the first bit, 1 indicates the second bit, 2 indicates the third bit, and so on)
     * @param newValue true to set the bit, or false to clear it
     */
   void SetBit(uint32 whichBit, bool newValue) 
   {
      MASSERT(whichBit < NumBits, "BitChord::SetBit:  whichBit was out of range!\n");
      SetBitUnchecked(whichBit, newValue);
   }

   /** Clears the state of the specified bit
     * @param whichBit the index of the bit to set (e.g. 0 indicates the first bit, 1 indicates the second bit, 2 indicates the third bit, and so on)
     */
   void ClearBit(uint32 whichBit) 
   {
      MASSERT(whichBit < NumBits, "BitChord::ClearBit:  whichBit was out of range!\n");
      ClearBitUnchecked(whichBit);
   }

   /** Toggles the state of the specified bit from 1 to 0, or vice-versa
     * @param whichBit the index of the bit to toggle (e.g. 0 indicates the first bit, 1 indicates the second bit, 2 indicates the third bit, and so on)
     */
   void ToggleBit(uint32 whichBit) 
   {
      MASSERT(whichBit < NumBits, "BitChord::ToggleBit:  whichBit was out of range!\n");
      SetBitUnchecked(whichBit, !IsBitSetUnchecked(whichBit));
   }

   /** Sets all our bits to false */
   void ClearAllBits() {for (uint32 i=0; i<NUM_WORDS; i++) _words[i] = 0;}

   /** Sets all our bits to true */
   void SetAllBits() 
   {
      for (uint32 i=0; i<NUM_WORDS; i++) _words[i] = ((uint32)-1);
      ClearUnusedBits();
   }

   /** Inverts the set/clear state of all our bits */
   void ToggleAllBits() 
   {
      for (uint32 i=0; i<NUM_WORDS; i++) _words[i] = ~_words[i];
      ClearUnusedBits();
   }

   /** Returns true iff at least one bit is set in this bit-chord. */
   bool AreAnyBitsSet() const
   {
      for (uint32 i=0; i<NUM_WORDS; i++) if (_words[i] != 0) return true;
      return false; 
   }

   /** Returns true iff all bits in this bit-chord are set. */
   bool AreAllBitsSet() const
   {
      if ((NumBits%NUM_BITS_PER_WORD) == 0)
      {
         for (uint32 i=0; i<NUM_WORDS; i++) if (_words[i] != ((uint32)-1)) return false;
      }
      else
      {
         for (uint32 i=0; i<NUM_WORDS-1; i++) if (_words[i] != ((uint32)-1)) return false;
         for (uint32 j=(NUM_WORDS-1)*NUM_BITS_PER_WORD; j<NumBits; j++) if (IsBitSet(j) == false) return false;
      }
      return true;
   }

   /** Convenience method:  Returns the current value of the given bit,
     * and clears the bit as a side-effect.
     * @param whichBit the index of the bit to return and then clear.
     */
   bool GetAndClearBit(uint32 whichBit)
   {
      const bool ret = IsBitSet(whichBit);
      ClearBit(whichBit);
      return ret;
   }

   /** Convenience method:  Returns the current value of the given bit,
     * and set the bit as a side-effect.
     * @param whichBit the index of the bit to return and then set.
     */
   bool GetAndSetBit(uint32 whichBit)
   {
      const bool ret = IsBitSet(whichBit);
      SetBit(whichBit);
      return ret;
   }

   /** Convenience method:  Returns the current value of the given bit,
     * and toggles the bit as a side-effect.
     * @param whichBit the index of the bit to return and then toggles.
     */
   bool GetAndToggleBit(uint32 whichBit)
   {
      const bool ret = IsBitSet(whichBit);
      ToggleBit(whichBit);
      return ret;
   }

   /** Returns true iff at least one bit is unset in this bit-chord. */
   bool AreAnyBitsUnset() const {return (AreAllBitsSet() == false);}

   /** Returns true iff all bits in this bit-chord are unset */
   bool AreAllBitsUnset() const {return (AreAnyBitsSet() == false);}

   /** @copydoc DoxyTemplate::operator|=(const DoxyTemplate &) */
   BitChord & operator |=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] |= rhs._words[i]; return *this;}

   /** @copydoc DoxyTemplate::operator&=(const DoxyTemplate &) */
   BitChord & operator &=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] &= rhs._words[i]; return *this;}

   /** @copydoc DoxyTemplate::operator^=(const DoxyTemplate &) */
   BitChord & operator ^=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] ^= rhs._words[i]; return *this;}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator ==(const BitChord & rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) if (_words[i] != rhs._words[i]) return false;} return true;}

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator !=(const BitChord & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator<(const DoxyTemplate &) const */
   bool operator < (const BitChord &rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) {if (_words[i] < rhs._words[i]) return true; if (_words[i] > rhs._words[i]) return false;}} return false;}

   /** @copydoc DoxyTemplate::operator>(const DoxyTemplate &) const */
   bool operator > (const BitChord &rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) {if (_words[i] > rhs._words[i]) return true; if (_words[i] < rhs._words[i]) return false;}} return false;}

   /** @copydoc DoxyTemplate::operator<=(const DoxyTemplate &) const */
   bool operator <=(const BitChord &rhs) const {return !(*this > rhs);}

   /** @copydoc DoxyTemplate::operator>=(const DoxyTemplate &) const */
   bool operator >=(const BitChord &rhs) const {return !(*this < rhs);}

   /** Returns a BitChord that is the bitwise-inverse of this BitChord (i.e. all bits flipped). */
   BitChord operator ~() const 
   {
      BitChord ret; for (uint32 i=0; i<NUM_WORDS; i++) ret._words[i] = ~_words[i]; 
      ret.ClearUnusedBits();  // don't let (ret) get into a non-normalized state
      return ret;
   }

   /** Part of the pseudo-Flattenable API:  Returns false (because even though all BitChords of a given  
     * BitChord template-instantiation will always have the same flattened-size, different template-instantiations
     * of BitChords can have different flattened-sizes and they all share the same type-code)
     */
   bool IsFixedSize() const {return false;}

   /** Part of the pseudo-Flattenable API:  Returns B_BITCHORD_TYPE. */
   uint32 TypeCode() const {return B_BITCHORD_TYPE;}

   /** Returns true iff (tc) equals B_BITCHORD_TYPE.
     * @param tc the type code to examine
     */
   bool AllowsTypeCode(uint32 tc) const {return (TypeCode()==tc);}

   /** @copydoc DoxyTemplate::FlattenedSize() const */
   uint32 FlattenedSize() const {return sizeof(uint32)+(NUM_WORDS*sizeof(uint32));}

   /** @copydoc DoxyTemplate::Flatten(uint8 *) const */
   void Flatten(uint8 * buffer) const
   {
      muscleCopyOut(buffer, B_HOST_TO_LENDIAN_INT32(NumBits));  // just so we can handle versioning issues more intelligently later on
      buffer += sizeof(uint32);
      for (uint32 i=0; i<NUM_WORDS; i++) muscleCopyOut(&buffer[i*sizeof(int32)], B_HOST_TO_LENDIAN_INT32(_words[i]));
   }

   /** @copydoc DoxyTemplate::Unflatten(const uint8 *, uint32) */
   status_t Unflatten(const uint8 * buffer, uint32 size)
   {
      if (size < sizeof(uint32)) return B_BAD_DATA;  // not enough data to even read the #-of-valid-bits header?

      const uint32 numBitsToRead = muscleMin(B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer)), NumBits);
      buffer += sizeof(uint32); size -= sizeof(uint32);

      const uint32 numWordsToRead = (numBitsToRead+NUM_BITS_PER_WORD-1)/NUM_BITS_PER_WORD;
      if (size < numWordsToRead) return B_BAD_DATA;
         
      for (uint32 i=0; i<numWordsToRead; i++) _words[i] = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(&buffer[i*sizeof(uint32)]));

      ClearUnusedBits();   // make sure we didn't read in non-zero values for any bits that we don't use
      for (uint32 i=numBitsToRead; i<NumBits; i++) ClearBit(i);  // any bits that we didn't read (because the data was too short) should be cleared
      return B_NO_ERROR;
   }
 
   /** @copydoc DoxyTemplate::CalculateChecksum() const */
   uint32 CalculateChecksum() const 
   {
      uint32 ret = 0;
      for (uint32 i=0; i<NUM_WORDS; i++) ret += ((i+1)*_words[i]);
      return ret;
   }

   /** @copydoc DoxyTemplate::HashCode() const */
   uint32 HashCode() const {return CalculateChecksum();}

#ifndef MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
   /** Equivalent to calling SetBit() multiple times; once per supplied argument.
     * e.g. calling SetBits(a,b,c) is equivalent to calling SetBit(a);SetBit(b);SetBit(c).
     * @param bits a list of bit-indices indicating which bit(s) to set
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> void SetBits(Bits... bits) {const int arr[] {bits...}; for (auto bit : arr) SetBit(bit);}

   /** Equivalent to calling ClearBit() multiple times; once per supplied argument.
     * e.g. calling ClearBits(a,b,c) is equivalent to calling ClearBit(a);ClearBit(b);ClearBit(c).
     * @param bits a list of bit-indices indicating which bit(s) to unset
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> void ClearBits(Bits... bits) {const int arr[] {bits...}; for (auto bit : arr) ClearBit(bit);}

   /** Equivalent to calling ToggleBit() multiple times; once per supplied argument.
     * e.g. calling ToggleBits(a,b,c) is equivalent to calling ToggleBit(a);ToggleBit(b);ToggleBit(c).
     * @param bits a list of bit-indices indicating which bit(s) to toggle
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> void ToggleBits(Bits... bits) {const int arr[] {bits...}; for (auto bit : arr) ToggleBit(bit);}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices have been set.
     * @param bits a list of bit-indices indicating which bit(s) to toggle
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> BitChord WithBits(Bits... bits) const {BitChord ret(*this); const int arr[] {bits...}; for (auto bit : arr) ret.SetBit(bit); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices have been cleared.
     * @param bits a list of bit-indices indicating which bit(s) to toggle
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> BitChord WithoutBits(Bits... bits) const {BitChord ret(*this); const int arr[] {bits...}; for (auto bit : arr) ret.ClearBit(bit); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices have been toggled.
     * @param bits a list of bit-indices indicating which bit(s) to toggle
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> BitChord WithToggledBits(Bits... bits) const {BitChord ret(*this); const int arr[] {bits...}; for (auto bit : arr) ret.ToggleBit(bit); return ret;}

   /** Convenience method.  Returns true if at least one of the specified bits is set.
     * @param bits a list of bit-indices indicating which bit(s) to test
     * @returns true iff at least one of the specified bits is set
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> bool AreAnyOfTheseBitsSet(Bits... bits) const {const int arr[] {bits...}; for (auto bit : arr) if (IsBitSet(bit)) return true; return false;}

   /** Convenience method.  Returns true if at least one of the specified bits is set.
     * @param bits a list of bit-indices indicating which bit(s) to test
     * @returns true iff every one of the specified bits is set
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> bool AreAllOfTheseBitsSet(Bits... bits) const {const int arr[] {bits...}; for (auto bit : arr) if (IsBitSet(bit) == false) return false; return true;}

   /** Convenience method.  Returns true if at least one of the specified bits is unset.
     * @param bits a list of bit-indices indicating which bit(s) to test
     * @returns true iff at least one of the specified bits is unset
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> bool AreAnyOfTheseBitsUnset(Bits... bits) const {const int arr[] {bits...}; for (auto bit : arr) if (IsBitSet(bit) == false) return true; return false;}

   /** Convenience method.  Returns true if at least one of the specified bits is unset.
     * @param bits a list of bit-indices indicating which bit(s) to test
     * @returns true iff every one of the specified bits is unset
     * @note this method can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bits> bool AreAllOfTheseBitsUnset(Bits... bits) const {const int arr[] {bits...}; for (auto bit : arr) if (IsBitSet(bit)) return false; return true;}

   /** Pseudo-constructor:  Returns a BitChord whose contents are copied from the supplied list of 32-bit words. 
     * @param words a list of uint32s to copy into our internal array.  The number of arguments must be equal to NUM_WORDS.
     * @note this constructor can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Words> static BitChord FromWords(Words... words)
   {
      const uint32 arr[] {words...}; 
      static_assert(ARRAYITEMS(arr) == NUM_WORDS, "Wrong number of 32-bit-word arguments was supplied to BitChord::FromWords()");

      uint32 i = 0;
      BitChord ret;
      for (auto w : arr) ret.SetWord(i++, w);
      return ret;
   }

   /** Pseudo-constructor:  Returns a BitChord whose contents are copied from the supplied list of 32-bit words. 
     * @param bytes a list of uint8s to copy into our internal array.  The number of arguments must be equal to NUM_BYTES.
     * @note this constructor can be used (with up to 32 arguments) even when MUSCLE_AVOID_CPLUSPLUS11 is defined, via clever ifdef and macro magic
     */
   template<typename ...Bytes> static BitChord FromBytes(Bytes... bytes)
   {
      const uint8 arr[] {bytes...}; 
      static_assert(ARRAYITEMS(arr) == NUM_BYTES, "Wrong number of 8-bit-byte arguments was supplied to BitChord::FromBytes()");

      uint32 i = 0;
      BitChord ret;
      for (auto b : arr) ret.SetByte(i++, b);
      return ret;
   }

   /** Pseudo-constructor:  Returns a BitChord with all of its bits set EXCEPT the bits specified as arguments.
     * @param bits a list of bit-indices that should remain cleared
     */
   template<typename ...Bits> static BitChord WithAllBitsSetExceptThese(Bits... bits)
   {
      const int arr[] {bits...}; 

      BitChord ret = BitChord::WithAllBitsSet();
      for (auto b : arr) ret.ClearBit(b);
      return ret;
   }
#endif

   /** Sets all the bits in this BitChord that are set in (bits)
     * @param bits a set of bits indicating which bit-positions to set in (*this)
     */
   void SetBits(const BitChord & bits) {for (uint32 i=0; i<NUM_WORDS; i++) _words[i] |= bits._words[i];}

   /** Sets all the bits in this BitChord that are set in (bits)
     * @param bits a set of bits indicating which bit-positions to set in (*this)
     * @param set if true, the specified bits will be set; if false, they will be cleared.
     */
   void SetBits(const BitChord & bits, bool set) {if (set) SetBits(bits); else ClearBits(bits);}

   /** Clears all the bits in this BitChord that are set in (bits)
     * @param bits a set of bits indicating which bit-positions to clear in (*this)
     */
   void ClearBits(const BitChord & bits) {for (uint32 i=0; i<NUM_WORDS; i++) _words[i] &= ~bits._words[i];}

   /** Toggles all the bits in this BitChord that are set in (bits)
     * @param bits a set of bits indicating which bit-positions to toggle in (*this)
     */
   void ToggleBits(const BitChord & bits) {for (uint32 i=0; i<NUM_WORDS; i++) _words[i] ^= bits._words[i];}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bit at the specified index has been set.
     * @param whichBit the index of the bit to set in the returned object.
     */
   BitChord WithBit(uint32 whichBit) const {BitChord ret(*this); ret.SetBit(whichBit); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bit at the specified index has been cleared.
     * @param whichBit the index of the bit to clear in the returned object.
     */
   BitChord WithoutBit(uint32 whichBit) const {BitChord ret(*this); ret.ClearBit(whichBit); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bit at the specified index has been toggled.
     * @param whichBit the index of the bit to toggled in the returned object.
     */
   BitChord WithToggledBit(uint32 whichBit) const {BitChord ret(*this); ret.ToggleBit(whichBit); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices have been set.
     * @param whichBits BitChord representing the indices to set.
     */
   BitChord WithBits(const BitChord & whichBits) const {BitChord ret(*this); ret.SetBits(whichBits); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices has been cleared.
     * @param whichBits the indices of the bits to clear in the returned object.
     */
   BitChord WithoutBits(const BitChord & whichBits) const {BitChord ret(*this); ret.ClearBits(whichBits); return ret;}

   /** Convenience method.  Returns a BitChord that is identical to this one, except that the
     * bits at the specified indices have been toggled.
     * @param whichBits the indices of the bits to toggle in the returned object.
     */
   BitChord WithToggledBits(const BitChord & whichBits) const {BitChord ret(*this); ret.ToggleBits(whichBits); return ret;}

   /** Convenience method.  Returns true if at least one of the specified bits is set.
     * @param bits a BitChord indicating which bit(s) to test
     * @returns true iff at least one of specified bits specified in (bits) is also set in (this)
     */
   bool AreAnyOfTheseBitsSet(const BitChord & bits) const {BitChord b(*this); b &= bits; return b.AreAnyBitsSet();}

   /** Convenience method.  Returns true if at least one of the specified bits is set.
     * @param bits a BitChord indicating which bit(s) to test
     * @returns true iff every one of the bits specified in (bits) is also set in (this)
     */
   bool AreAllOfTheseBitsSet(const BitChord & bits) const {BitChord b(*this); b &= bits; return (b==bits);}

   /** Returns a BitChord identical to this one, except that the bits specified in the
     * (whichBits) argument have been set or cleared, depending on the (setBits) argument
     * @param whichBits the bits to set (or clear)
     * @param setBits true iff (whichBits) should be set; false if they should be cleared.
     */
   BitChord WithOrWithoutBits(const BitChord & whichBits, bool setBits) const {return setBits ? WithBits(whichBits) : WithoutBits(whichBits);}

   /** Returns a BitChord with all bits cleared (aka a default-constructed BitChord). */
   static BitChord WithAllBitsCleared() {return BitChord();}

   /** Returns a BitChord with all of its bits set. */
   static BitChord WithAllBitsSet() {BitChord ret; return ~ret;}

   /** Returns a BitChord just like this one, except with all of the bits toggled to their boolean inverse. */
   BitChord WithAllBitsToggled() const {return ~(*this);}

   /** Returns the number of bits that are represented by this bit-chord, as specified in the template arguments */
   static MUSCLE_CONSTEXPR uint32 GetNumBitsInBitChord() {return NumBits;}

   /** Returns the number of 8-bit-bytes that are represented by this bit-chord */
   static MUSCLE_CONSTEXPR uint32 GetNumBytesInBitChord() {return NUM_BYTES;}

   /** Returns the number of 32-bit-words that are represented by this bit-chord */
   static MUSCLE_CONSTEXPR uint32 GetNumWordsInBitChord() {return NUM_WORDS;}

   /** Returns a fixed-length hexadecimal representation of this bit-chord. */
   String ToHexString() const
   {
      String ret; (void) ret.Prealloc(1+(NUM_BYTES*3));
      for (int32 i=NUM_BYTES-1; i>=0; i--)
      {
         char buf[4]; muscleSprintf(buf, "%s%02x", (ret.IsEmpty())?"":" ", GetByte(i));
         ret += buf;
      }
      return ret;
   }
 
   /** Returns a fixed-length binary representation of this bit-chord. */
   String ToBinaryString() const
   {
      String ret; (void) ret.Prealloc(NumBits+1);
      for (int32 i=NumBits-1; i>=0; i--) ret += IsBitSet(i)?'1':'0';
      return ret;
   }
 
   /** Sets a given 32-bit word full of bits in our internal words-array.
     * Don't call this unless you know what you're doing!
     * @param whichWord index of the word to set
     * @param wordValue the new value for the specified word.
     */
   void SetWord(uint32 whichWord, uint32 wordValue)
   {
      MASSERT(whichWord < NUM_WORDS, "BitChord::SetWord:  whichWord was out of range!\n");
      _words[whichWord] = wordValue;
      if ((whichWord+1) == NUM_WORDS) ClearUnusedBits();  // keep us normalized
   }

   /** Returns the nth 32-bit word from our internal words-array.
     * Don't call this unless you know what you're doing!
     * @param whichWord index of the word to return
     */
   uint32 GetWord(uint32 whichWord) const
   {
      MASSERT(whichWord < NUM_WORDS, "BitChord::GetWord:  whichWord was out of range!\n");
      return _words[whichWord];
   }

   /** Sets a given 8-bit byte in our internal words-array.
     * Don't call this unless you know what you're doing!
     * @param whichByte index of the 8-bit byte to set
     * @param byteValue the new value for the specified byte.
     */
   void SetByte(uint32 whichByte, uint32 byteValue)
   {
      MASSERT(whichByte < NUM_BYTES, "BitChord::SetByte:  whichByte was out of range!\n");

      const uint32 bitShiftOffset = (whichByte*NUM_BITS_PER_BYTE);
      uint32 & word = _words[whichByte/NUM_BYTES_PER_WORD];
      word &= ~(((uint32)0xFF)      << bitShiftOffset);
      word |=  (((uint32)byteValue) << bitShiftOffset);
      if ((whichByte+1) == NUM_BYTES) ClearUnusedBits();  // keep us normalized
   }

   /** Returns the nth 8-bit byte from our internal words-array.
     * Don't call this unless you know what you're doing!
     * @param whichByte index of the byte to return
     */
   uint8 GetByte(uint32 whichByte) const 
   {
      MASSERT(whichByte < NUM_BYTES, "BitChord::GetByte:  whichByte was out of range!\n");
      return (uint8) ((_words[whichByte/NUM_BYTES_PER_WORD]>>((whichByte%NUM_BYTES_PER_WORD)*NUM_BITS_PER_BYTE)) & 0xFF);
   }

private:
#ifdef MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
   int OneIffBitIsSet(  uint32 whichBit) const {return IsBitSet(whichBit)?1:0;}
   int OneIffBitIsUnset(uint32 whichBit) const {return IsBitSet(whichBit)?0:1;}
#else
   static constexpr uint32 GetWordWithFirstNBitsSet(int numBits)
   {
      return (numBits <= 0) ? ((uint32)0) : ((((uint32)1)<<(numBits-1)) | GetWordWithFirstNBitsSet(numBits-1));
   }
#endif

   bool IsBitSetUnchecked(uint32 whichBit) const {return ((_words[whichBit/NUM_BITS_PER_WORD] & (1<<(whichBit%NUM_BITS_PER_WORD))) != 0);}
   void ClearBitUnchecked(uint32 whichBit) {_words[whichBit/NUM_BITS_PER_WORD] &= ~(1<<(whichBit%NUM_BITS_PER_WORD));}
   void SetBitUnchecked(  uint32 whichBit) {_words[whichBit/NUM_BITS_PER_WORD] |=  (1<<(whichBit%NUM_BITS_PER_WORD));}
   void SetBitUnchecked(  uint32 whichBit, bool newValue)
   {
      if (newValue) SetBitUnchecked(whichBit);
               else ClearBitUnchecked(whichBit);
   }

   void ClearUnusedBits()
   {
      const uint32 numLeftoverBits = NumBits%NUM_BITS_PER_WORD;
      if (numLeftoverBits > 0)
      {
         uint32 & lastWord = _words[NUM_WORDS-1];
#ifdef MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
         if (lastWord != 0) for (uint32 i=NumBits; i<(NUM_WORDS*NUM_BITS_PER_WORD); i++) ClearBitUnchecked(i);
#else
         lastWord &= GetWordWithFirstNBitsSet(numLeftoverBits);  // O(1) implementation
#endif
      }
   }

   enum {NUM_BITS_PER_BYTE  = 8};
   enum {NUM_BYTES_PER_WORD = sizeof(uint32)};
   enum {NUM_BITS_PER_WORD  = (NUM_BYTES_PER_WORD*NUM_BITS_PER_BYTE)};
   enum {NUM_WORDS          = (NumBits+NUM_BITS_PER_WORD-1)/NUM_BITS_PER_WORD};
   enum {NUM_BYTES          = (NumBits+NUM_BITS_PER_BYTE-1)/NUM_BITS_PER_BYTE};

   uint32 _words[NUM_WORDS];

public:
   // This is the hacked-for-C++03 implementation of our variadic methods; it's here for backwards compatibility with old compilers only!
   // It works the same as the above variadic implementation, but only for up to 32 arguments (and it's macro-based, so it's a total eyesore, sorry)
#ifdef MUSCLE_AVOID_CPLUSPLUS11_BITCHORD
# ifndef DOXYGEN_SHOULD_IGNORE_THIS

#define BC_ARGS_1 uint32 b1
#define BC_ARGS_2 uint32 b1, uint32 b2
#define BC_ARGS_3 uint32 b1, uint32 b2, uint32 b3
#define BC_ARGS_4 uint32 b1, uint32 b2, uint32 b3, uint32 b4
#define BC_ARGS_5 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5
#define BC_ARGS_6 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6
#define BC_ARGS_7 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7
#define BC_ARGS_8 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8
#define BC_ARGS_9 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9
#define BC_ARGS_10 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10
#define BC_ARGS_11 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11
#define BC_ARGS_12 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12
#define BC_ARGS_13 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13
#define BC_ARGS_14 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14
#define BC_ARGS_15 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15
#define BC_ARGS_16 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16
#define BC_ARGS_17 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17
#define BC_ARGS_18 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18
#define BC_ARGS_19 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19
#define BC_ARGS_20 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20
#define BC_ARGS_21 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21
#define BC_ARGS_22 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22
#define BC_ARGS_23 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23
#define BC_ARGS_24 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24
#define BC_ARGS_25 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25
#define BC_ARGS_26 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26
#define BC_ARGS_27 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27
#define BC_ARGS_28 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27, uint32 b28
#define BC_ARGS_29 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27, uint32 b28, uint32 b29
#define BC_ARGS_30 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27, uint32 b28, uint32 b29, uint32 b30
#define BC_ARGS_31 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27, uint32 b28, uint32 b29, uint32 b30, uint32 b31
#define BC_ARGS_32 uint32 b1, uint32 b2, uint32 b3, uint32 b4, uint32 b5, uint32 b6, uint32 b7, uint32 b8, uint32 b9, uint32 b10, uint32 b11, uint32 b12, uint32 b13, uint32 b14, uint32 b15, uint32 b16, uint32 b17, uint32 b18, uint32 b19, uint32 b20, uint32 b21, uint32 b22, uint32 b23, uint32 b24, uint32 b25, uint32 b26, uint32 b27, uint32 b28, uint32 b29, uint32 b30, uint32 b31, uint32 b32

#define BC_CALL_1(o,cn) {o.cn(b1);}
#define BC_CALL_2(o,cn) {o.cn(b1); o.cn(b2);}
#define BC_CALL_3(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3);}
#define BC_CALL_4(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4);}
#define BC_CALL_5(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5);}
#define BC_CALL_6(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6);}
#define BC_CALL_7(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7);}
#define BC_CALL_8(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8);}
#define BC_CALL_9(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9);}
#define BC_CALL_10(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10);}
#define BC_CALL_11(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11);}
#define BC_CALL_12(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12);}
#define BC_CALL_13(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13);}
#define BC_CALL_14(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14);}
#define BC_CALL_15(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15);}
#define BC_CALL_16(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16);}
#define BC_CALL_17(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17);}
#define BC_CALL_18(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18);}
#define BC_CALL_19(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19);}
#define BC_CALL_20(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20);}
#define BC_CALL_21(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21);}
#define BC_CALL_22(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22);}
#define BC_CALL_23(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23);}
#define BC_CALL_24(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24);}
#define BC_CALL_25(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25);}
#define BC_CALL_26(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26);}
#define BC_CALL_27(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27);}
#define BC_CALL_28(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27); o.cn(b28);}
#define BC_CALL_29(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27); o.cn(b28); o.cn(b29);}
#define BC_CALL_30(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27); o.cn(b28); o.cn(b29); o.cn(b30);}
#define BC_CALL_31(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27); o.cn(b28); o.cn(b29); o.cn(b30); o.cn(b31);}
#define BC_CALL_32(o,cn) {o.cn(b1); o.cn(b2); o.cn(b3); o.cn(b4); o.cn(b5); o.cn(b6); o.cn(b7); o.cn(b8); o.cn(b9); o.cn(b10); o.cn(b11); o.cn(b12); o.cn(b13); o.cn(b14); o.cn(b15); o.cn(b16); o.cn(b17); o.cn(b18); o.cn(b19); o.cn(b20); o.cn(b21); o.cn(b22); o.cn(b23); o.cn(b24); o.cn(b25); o.cn(b26); o.cn(b27); o.cn(b28); o.cn(b29); o.cn(b30); o.cn(b31); o.cn(b32);} 

#define BC_CALL2_1(o,cn) {o.cn(0,b1);}
#define BC_CALL2_2(o,cn) {o.cn(0,b1); o.cn(1,b2);}
#define BC_CALL2_3(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3);}
#define BC_CALL2_4(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4);}
#define BC_CALL2_5(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5);}
#define BC_CALL2_6(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6);}
#define BC_CALL2_7(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7);}
#define BC_CALL2_8(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8);}
#define BC_CALL2_9(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9);}
#define BC_CALL2_10(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10);}
#define BC_CALL2_11(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11);}
#define BC_CALL2_12(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12);}
#define BC_CALL2_13(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13);}
#define BC_CALL2_14(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14);}
#define BC_CALL2_15(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15);}
#define BC_CALL2_16(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16);}
#define BC_CALL2_17(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17);}
#define BC_CALL2_18(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18);}
#define BC_CALL2_19(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19);}
#define BC_CALL2_20(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20);}
#define BC_CALL2_21(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21);}
#define BC_CALL2_22(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22);}
#define BC_CALL2_23(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23);}
#define BC_CALL2_24(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24);}
#define BC_CALL2_25(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25);}
#define BC_CALL2_26(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26);}
#define BC_CALL2_27(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27);}
#define BC_CALL2_28(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27); o.cn(27,b28);}
#define BC_CALL2_29(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27); o.cn(27,b28); o.cn(28,b29);}
#define BC_CALL2_30(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27); o.cn(27,b28); o.cn(28,b29); o.cn(29,b30);}
#define BC_CALL2_31(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27); o.cn(27,b28); o.cn(28,b29); o.cn(29,b30); o.cn(30,b31);}
#define BC_CALL2_32(o,cn) {o.cn(0,b1); o.cn(1,b2); o.cn(2,b3); o.cn(3,b4); o.cn(4,b5); o.cn(5,b6); o.cn(6,b7); o.cn(7,b8); o.cn(8,b9); o.cn(9,b10); o.cn(10,b11); o.cn(11,b12); o.cn(12,b13); o.cn(13,b14); o.cn(14,b15); o.cn(15,b16); o.cn(16,b17); o.cn(17,b18); o.cn(18,b19); o.cn(19,b20); o.cn(20,b21); o.cn(21,b22); o.cn(22,b23); o.cn(23,b24); o.cn(24,b25); o.cn(25,b26); o.cn(26,b27); o.cn(27,b28); o.cn(28,b29); o.cn(29,b30); o.cn(30,b31); o.cn(31,b32);} 

#define BC_SUM_1(o,cn) (o.cn(b1))
#define BC_SUM_2(o,cn) (o.cn(b1)+o.cn(b2))
#define BC_SUM_3(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3))
#define BC_SUM_4(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4))
#define BC_SUM_5(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5))
#define BC_SUM_6(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6))
#define BC_SUM_7(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7))
#define BC_SUM_8(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8))
#define BC_SUM_9(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9))
#define BC_SUM_10(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10))
#define BC_SUM_11(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11))
#define BC_SUM_12(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12))
#define BC_SUM_13(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13))
#define BC_SUM_14(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14))
#define BC_SUM_15(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15))
#define BC_SUM_16(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16))
#define BC_SUM_17(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17))
#define BC_SUM_18(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18))
#define BC_SUM_19(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19))
#define BC_SUM_20(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20))
#define BC_SUM_21(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21))
#define BC_SUM_22(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22))
#define BC_SUM_23(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23))
#define BC_SUM_24(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24))
#define BC_SUM_25(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25))
#define BC_SUM_26(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26))
#define BC_SUM_27(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27))
#define BC_SUM_28(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27)+o.cn(b28))
#define BC_SUM_29(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27)+o.cn(b28)+o.cn(b29))
#define BC_SUM_30(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27)+o.cn(b28)+o.cn(b29)+o.cn(b30))
#define BC_SUM_31(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27)+o.cn(b28)+o.cn(b29)+o.cn(b30)+o.cn(b31))
#define BC_SUM_32(o,cn) (o.cn(b1)+o.cn(b2)+o.cn(b3)+o.cn(b4)+o.cn(b5)+o.cn(b6)+o.cn(b7)+o.cn(b8)+o.cn(b9)+o.cn(b10)+o.cn(b11)+o.cn(b12)+o.cn(b13)+o.cn(b14)+o.cn(b15)+o.cn(b16)+o.cn(b17)+o.cn(b18)+o.cn(b19)+o.cn(b20)+o.cn(b21)+o.cn(b22)+o.cn(b23)+o.cn(b24)+o.cn(b25)+o.cn(b26)+o.cn(b27)+o.cn(b28)+o.cn(b29)+o.cn(b30)+o.cn(b31)+o.cn(b32))

# define BC_VARIADIC_CTOR(X)      explicit BitChord(BC_ARGS_##X)                 {ClearAllBits(); BC_CALL_##X((*this), SetBit);}
# define BC_SET_BITS(X)           void SetBits(BC_ARGS_##X)                      {BC_CALL_##X((*this), SetBit);}
# define BC_CLEAR_BITS(X)         void ClearBits(BC_ARGS_##X)                    {BC_CALL_##X((*this), ClearBit);}
# define BC_TOGGLE_BITS(X)        void ToggleBits(BC_ARGS_##X)                   {BC_CALL_##X((*this), ToggleBit);}
# define BC_WITH_BITS(X)          BitChord WithBits(BC_ARGS_##X)           const {BitChord ret(*this); BC_CALL_##X(ret, SetBit);    return ret;}
# define BC_WITHOUT_BITS(X)       BitChord WithoutBits(BC_ARGS_##X)        const {BitChord ret(*this); BC_CALL_##X(ret, ClearBit);  return ret;}
# define BC_WITH_TOGGLED_BITS(X)  BitChord WithToggledBits(BC_ARGS_##X)    const {BitChord ret(*this); BC_CALL_##X(ret, ToggleBit); return ret;}
# define BC_WITH_ALL_EXCEPT(X)    static BitChord WithAllBitsSetExceptThese(BC_ARGS_##X) {BitChord ret; ret.SetAllBits(); BC_CALL_##X(ret, ClearBit); return ret;}
# define BC_FROM_BYTES(X)         static BitChord FromBytes(BC_ARGS_##X)         {BitChord ret; BC_CALL2_##X(ret, SetByte);  return ret;}
# define BC_FROM_WORDS(X)         static BitChord FromWords(BC_ARGS_##X)         {BitChord ret; BC_CALL2_##X(ret, SetWord);  return ret;}
# define BC_ARE_ANY_BITS_SET(X)   bool AreAnyOfTheseBitsSet(BC_ARGS_##X)   const {return BC_SUM_##X((*this), OneIffBitIsSet)   >  0;}
# define BC_ARE_ALL_BITS_SET(X)   bool AreAllOfTheseBitsSet(BC_ARGS_##X)   const {return BC_SUM_##X((*this), OneIffBitIsUnset) == 0;}
# define BC_ARE_ANY_BITS_UNSET(X) bool AreAnyOfTheseBitsUnset(BC_ARGS_##X) const {return BC_SUM_##X((*this), OneIffBitIsUnset) >  0;}
# define BC_ARE_ALL_BITS_UNSET(X) bool AreAllOfTheseBitsUnset(BC_ARGS_##X) const {return BC_SUM_##X((*this), OneIffBitIsSet)   == 0;}
# define BC_DECLARE_ALL(X) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32)

   // These macros expand out to all 32 supported overrides of each method...
   BC_DECLARE_ALL(BC_VARIADIC_CTOR);
   BC_DECLARE_ALL(BC_SET_BITS);
   BC_DECLARE_ALL(BC_CLEAR_BITS);
   BC_DECLARE_ALL(BC_TOGGLE_BITS);
   BC_DECLARE_ALL(BC_WITH_BITS);
   BC_DECLARE_ALL(BC_WITHOUT_BITS);
   BC_DECLARE_ALL(BC_WITH_TOGGLED_BITS);
   BC_DECLARE_ALL(BC_WITH_ALL_EXCEPT);
   BC_DECLARE_ALL(BC_FROM_BYTES);
   BC_DECLARE_ALL(BC_FROM_WORDS);
   BC_DECLARE_ALL(BC_ARE_ANY_BITS_SET);
   BC_DECLARE_ALL(BC_ARE_ALL_BITS_SET);
   BC_DECLARE_ALL(BC_ARE_ANY_BITS_UNSET);
   BC_DECLARE_ALL(BC_ARE_ALL_BITS_UNSET);
# endif
#endif
};

/** Binary bitwise-OR operator for two BitChord objects
  * @param lhs The first BitChord object to OR together
  * @param rhs The first BitChord object to OR together
  * @returns a BitChord whose bits are the union of the bits of the two arguments
  */
template<uint32 NumBits, class TagClass> const BitChord<NumBits,TagClass> operator | (const BitChord<NumBits,TagClass> & lhs, const BitChord<NumBits,TagClass> & rhs) {BitChord<NumBits,TagClass> ret(lhs); ret |= rhs; return ret;}

/** Binary bitwise-AND operator for two BitChord objects
  * @param lhs The first BitChord object to AND together
  * @param rhs The first BitChord object to AND together
  * @returns a BitChord whose bits are the intersection of the bits of the two arguments
  */
template<uint32 NumBits, class TagClass> const BitChord<NumBits,TagClass> operator & (const BitChord<NumBits,TagClass> & lhs, const BitChord<NumBits,TagClass> & rhs) {BitChord<NumBits,TagClass> ret(lhs); ret &= rhs; return ret;}

/** Binary bitwise-XOR operator for two BitChord objects
  * @param lhs The first BitChord object to XOR together
  * @param rhs The first BitChord object to XOR together
  * @returns a BitChord whose bits are the XOR of the bits of the two arguments
  */
template<uint32 NumBits, class TagClass> const BitChord<NumBits,TagClass> operator ^ (const BitChord<NumBits,TagClass> & lhs, const BitChord<NumBits,TagClass> & rhs) {BitChord<NumBits,TagClass> ret(lhs); ret ^= rhs; return ret;}

/** This macros declares a unique BitChord-type with a specified number of bits.
  * @param typeName the name of the new type e.g. (MySpecialFlags)
  * @param numBitsInType the number of bits a BitChord of this type will represent
  * @note Example usage:  enum {OPTION_A=0, OPTION_B, OPTION_C, NUM_OPTIONS}; DECLARE_BITCHORD_FLAGS_TYPE(MyOptionFlags, NUM_OPTIONS);
  */
#define DECLARE_BITCHORD_FLAGS_TYPE(typeName, numBitsInType) struct _bitchord_tag_class_##typeName##_##numBitsInType {}; typedef BitChord<numBitsInType, _bitchord_tag_class_##typeName##_##numBitsInType> typeName;

} // end namespace muscle

#endif
