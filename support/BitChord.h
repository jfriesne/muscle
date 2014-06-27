/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleBitChord_h
#define MuscleBitChord_h

#include "util/String.h"
#include "support/MuscleSupport.h"

namespace muscle {

/** A templated class for implement an N-bit-long bit-chord.  Useful for doing efficient parallel boolean operations
  * in bit-lengths that can't fit in any of the standard integer types.
  */
template <unsigned int NumBits> class BitChord
{
public:
   /** Default ctor;  All bits are set to false. */
   BitChord() {ClearAllBits();}

   /** Copy constructor */
   BitChord(const BitChord & copyMe) {*this = copyMe;}

   /** Destructor */
   ~BitChord() {/* empty */}

   /** Assignment operator. */
   BitChord & operator =(const BitChord & rhs) {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] = rhs._words[i];} return *this;}

   /** Returns a BitChord that is the bitwise inverse of this BitChord. */
   BitChord operator ~() const 
   {
      BitChord ret; for (uint32 i=0; i<NUM_WORDS; i++) ret._words[i] = ~_words[i]; 
      ret.ClearExtraBits();  // don't let (ret) get into a non-normalized state
      return ret;
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
      if (NumBits == NUM_BITS_PER_WORD*NUM_WORDS)
      {
         for (uint32 i=0; i<NUM_WORDS; i++) if (_words[i] != ~0) return false;
         return true; 
      }
      else
      {
         for (uint32 i=0; i<NUM_WORDS-1; i++) if (_words[i] != ~0) return false;
         for (uint32 j=(NUM_WORDS-1)*NUM_BITS_PER_WORD; j<NumBits; j++) if (GetBit(j) == false) return false;
         return true;
      }
   }

   /** OR's all bits in (rhs) into their counterparts in this object. */
   BitChord & operator |=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] |= rhs._words[i]; return *this;}

   /** AND's all bits in (rhs) into their counterparts in this object. */
   BitChord & operator &=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] &= rhs._words[i]; return *this;}

   /** XOR's all bits in (rhs) into their counterparts in this object. */
   BitChord & operator ^=(const BitChord & rhs) {for (int i=0; i<NUM_WORDS; i++) _words[i] ^= rhs._words[i]; return *this;}

   /** Returns true iff all indices in this object are equal to their counterparts in (rhs). */
   bool operator ==(const BitChord & rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) if (_words[i] != rhs._words[i]) return false;} return true;}

   /** Returns true iff any indices in this object are not equal to their counterparts in (rhs). */
   bool operator !=(const BitChord & rhs) const {return !(*this == rhs);}

   /** Comparison Operator.  Returns true if this bit-chord comes before (rhs) lexically. */
   bool operator < (const BitChord &rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) {if (_words[i] < rhs._words[i]) return true; if (_words[i] > rhs._words[i]) return false;}} return false;}

   /** Comparison Operator.  Returns true if this bit-chord comes after (rhs) lexically. */
   bool operator > (const BitChord &rhs) const {if (this != &rhs) {for (int i=0; i<NUM_WORDS; i++) {if (_words[i] > rhs._words[i]) return true; if (_words[i] < rhs._words[i]) return false;}} return false;}

   /** Comparison Operator.  Returns true if the two bit-chord are equal, or this bit-chord comes before (rhs) lexically. */
   bool operator <=(const BitChord &rhs) const {return !(*this > rhs);}

   /** Comparison Operator.  Returns true if the two bit-chord are equal, or this bit-chord comes after (rhs) lexically. */
   bool operator >=(const BitChord &rhs) const {return !(*this < rhs);}

   /** Returns the state of the specified bit */
   bool GetBit(unsigned int whichBit) const
   {
      MASSERT(whichBit < NumBits, "BitChord::GetBit:  whichBit was out of range!\n");
      return ((_words[whichBit/NUM_BITS_PER_WORD] & (1L<<(whichBit%NUM_BITS_PER_WORD))) != 0);
   }

   /** Sets the state of the specified bit */
   void SetBit(unsigned int whichBit, bool value) 
   {
      MASSERT(whichBit < NumBits, "BitChord::SetBit:  whichBit was out of range!\n");
      unsigned int & w = _words[whichBit/NUM_BITS_PER_WORD];
      unsigned int ch = 1L<<(whichBit%NUM_BITS_PER_WORD);
      if (value)  w |= ch;
             else w &= ~ch;
   }

   /** Sets all our bits to false */
   void ClearAllBits() {for (uint32 i=0; i<NUM_WORDS; i++) _words[i] = 0;}

   /** Sets all our bits to true */
   void SetAllBits() 
   {
      for (uint32 i=0; i<NUM_WORDS; i++) _words[i] = ~0;
      ClearExtraBits();
   }

   /** Returns the number of bits that are represented by this bit-chord, as specified in the template arguments */
   uint32 GetNumBitsInBitChord() const {return NumBits;}

   /** Returns a hexadecimal representation of this bit-chord. */
   String ToHexString() const
   {
      String ret; (void) ret.Prealloc(1+(8*NumBits/3));
      const uint8 * b = (const uint8 *) _words;
      bool foundNonZero = false;
      for (int32 i=NUM_WORDS*sizeof(unsigned int)-1; i>=0; i--)
      {
         uint8 c = b[i];
         if (c != 0) foundNonZero = true;
         if (foundNonZero)
         {
            char buf[4]; 
            sprintf(buf, "%s%02x", (ret.IsEmpty())?"":" ", c);
            ret += buf;
         }
      }
      return ret;
   }
 
private:
   void ClearExtraBits()
   {
      if ((NumBits != NUM_BITS_PER_WORD*NUM_WORDS)&&(_words[NUM_WORDS-1] != 0))
         for (unsigned int i=(NUM_WORDS*NUM_BITS_PER_WORD)-1; i>=NumBits; i--)
            SetBit(i, false);
   }

   enum {NUM_BITS_PER_WORD = sizeof(unsigned int)*8};
   enum {NUM_WORDS = (NumBits+NUM_BITS_PER_WORD-1)/NUM_BITS_PER_WORD};
   unsigned int _words[NUM_WORDS];
};

#define DECLARE_ADDITIONAL_BITCHORD_OPERATORS(N) \
  inline const BitChord<N> operator | (const BitChord<N> & lhs, const BitChord<N> & rhs) {BitChord<N> ret(lhs); ret |= rhs; return ret;} \
  inline const BitChord<N> operator & (const BitChord<N> & lhs, const BitChord<N> & rhs) {BitChord<N> ret(lhs); ret &= rhs; return ret;} \
  inline const BitChord<N> operator ^ (const BitChord<N> & lhs, const BitChord<N> & rhs) {BitChord<N> ret(lhs); ret ^= rhs; return ret;}

}; // end namespace muscle

#endif
