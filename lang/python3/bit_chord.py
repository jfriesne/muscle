"""
Python implementation of the MUSCLE BitChord class.
"""

__author__    = "Jeremy Friesner (jaf@meyersound.com)"
__version__   = "$Revision: 1.18 $"
__date__      = "$Date: 2020/03/31 07:24:00 $"
__copyright__ = "Copyright (c) 2000-2020 Meyer Sound Laboratories Inc"
__license__   = "See enclosed LICENSE.TXT file"

import array
import struct

NUM_BITS_PER_WORD      = 32  # We'll store our bits in groups of 32
WORD_WITH_ALL_BITS_SET = 0xFFFFFFFF

B_BITCHORD_TYPE= 1112818504 # 'BTCH': BitChord type

class BitChord:
   """Python implementation of the MUSCLE BitChord class.

   This class holds a fixed-length string of bits.
   """
   def __init__(self, NumBits=NUM_BITS_PER_WORD):
      self._numBits  = NumBits
      self._numWords = self.__bitCountToWordCount(self._numBits)
      self._words    = array.array('L', [0]*self._numWords)

   def IsBitSet(self, whichBit):
      return (self._words[self.__getWhichWord(whichBit)] & (1<<self.__getWhichBitWithinWord(whichBit))) != 0  
  
   def SetBit(self, whichBit, newBitValue = True):
      whichWord          = self.__getWhichWord(whichBit)
      whichBitWithinWord = self.__getWhichBitWithinWord(whichBit)
      if (newBitValue):
         self._words[whichWord] |= (1<<whichBitWithinWord)
      else:
         self._words[whichWord] &= ~(1<<whichBitWithinWord)
      
   def ClearBit(self, whichBit):
      self.SetBit(whichBit, False)

   def ClearAllBits(self):
       for i in range(len(self._words)):
          self._words[i] = 0

   def SetAllBits(self):
       for i in range(len(self._words)):
         self._words[i] = WORD_WITH_ALL_BITS_SET
       self.__clearUnusedBits()

   def ToggleAllBits(self):
       for i in range(len(self._words)):
         self._words[i] ^= WORD_WITH_ALL_BITS_SET
       self.__clearUnusedBits()

   def ToggleBit(self, whichBit):
      self.SetBit(whichBit, not self.IsBitSet(whichBit))

   def SetWord(self, whichWord, newWordValue):
      self._words[whichWord] = newWordValue

   def GetWord(self, whichWord):
      return self._words[whichWord]

   def AreAnyBitsSet(self):
       for i in range(len(self._words)):
         if (self._words[i] != 0):
            return True
      
   def AreAllBitsSet(self):
      if ((self._numBits%NUM_BITS_PER_WORD) == 0):
         for i in range(len(self._words)):
            if (self._words[i] != WORD_WITH_ALL_BITS_SET):
               return False
      else:
         for i in range(len(self._words)-1):
            if (self._words[i] != WORD_WITH_ALL_BITS_SET):
               return False
         for j in range((self.GetNumWords()-1)*NUM_BITS_PER_WORD, self._numBits):
            if (not self.IsBitSet(j)):
               return False
      return True

   def GetAndClearBit(self, whichBit):
      ret = self.IsBitSet(whichBit)
      self.ClearBit(whichBit)
      return ret

   def GetAndSetBit(self, whichBit):
      ret = self.IsBitSet(whichBit)
      self.SetBit(whichBit)
      return ret

   def GetAndToggleBit(self, whichBit):
      ret = self.IsBitSet(whichBit)
      self.ToggleBit(whichBit)
      return ret

   def GetNumBits(self):
      return self._numBits

   def GetNumWords(self):
      return self._numWords
      
   def TypeCode(self):
      return B_BITCHORD_TYPE

   def AllowsTypeCode(self, tc):
      return (tc == TypeCode())

   def FlattenedSize(self):
      return 4+((NUM_BITS_PER_WORD/8)*self._numWords)

   def Flatten(self, outFile):
      outFile.write(struct.pack("<L", self._numBits))
      for word in self._words:
         outFile.write(struct.pack("<L", word))

   def Unflatten(self, inFile):
      numEncodedBits  = struct.unpack("<L", inFile.read(4))[0]
      numEncodedWords = self.__bitCountToWordCount(numEncodedBits)
      self.ClearAllBits()
      for i in range(0, numEncodedWords):
         nextWord = struct.unpack("<L", inFile.read(4))[0]
         if (i < len(self._words)):
            self._words[i] = nextWord
      self.__clearUnusedBits()

   def __getWhichWord(self, whichBit):
      return (whichBit//NUM_BITS_PER_WORD)

   def __getWhichBitWithinWord(self, whichBit):
      return (whichBit%NUM_BITS_PER_WORD)

   def __clearUnusedBits(self):
      numLeftoverBits = self._numBits%NUM_BITS_PER_WORD
      if (numLeftoverBits > 0):
         for i in range(self._numBits, self._numWords*NUM_BITS_PER_WORD):
            self.ClearBit(i)

   def __bitCountToWordCount(self, numBits):
      return (numBits+(NUM_BITS_PER_WORD-1))//NUM_BITS_PER_WORD

   def __repr__(self):
      s = 'BitChord with %i bits:' % self._numBits
      for word in self._words:
         s = s + " 0x%x" % word
      return s

# Test stub.  Just writes a flattened BitChord out to a file, then reads in back in.
if __name__ == "__main__":
   bc = BitChord(129)
   bc.SetBit(0)
   bc.ToggleBit(35)
   print("%s"%bc)
   #bc.ToggleAllBits()
   print(bc._words)
   print(bc.IsBitSet(5))
   print(bc.AreAnyBitsSet())
   print(bc.AreAllBitsSet())

   print("Flattening: %s", bc)
   outFile = open("test.bitchord", "wb")   
   bc.Flatten(outFile)
   outFile.close()

   print("Unflattening...")
   inFile = open("test.bitchord", "rb")
   bc2 = BitChord(200)
   bc2.Unflatten(inFile)
   inFile.close()
   print("Unflattened:  %s" % bc2)
