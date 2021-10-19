/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/StringTokenizer.h"

namespace muscle {

static char _dummyString[] = "";

StringTokenizer :: StringTokenizer(const char * tokenizeMe, const char * hardSeparators, const char * softSeparators, char escapeChar)
   : _allocedBufferOnHeap(false)
   , _prevSepWasHard(false)
   , _escapeChar(escapeChar)
   , _prevChar(escapeChar+1)
{
   SetBitChord(_hardSepsBitChord, hardSeparators);
   SetBitChord(_softSepsBitChord, softSeparators);

   if (tokenizeMe == NULL) tokenizeMe = "";
   _bufLen = (uint32) strlen(tokenizeMe)+1; // +1 for the NUL byte
   const bool doAlloc = (_bufLen > sizeof(_smallStringBuf));  // only allocate from the heap if _smallStringBuf isn't big enough

   char * temp;
   if (doAlloc)
   {
      temp = newnothrow_array(char, _bufLen);
      if (temp) _allocedBufferOnHeap = true;
           else MWARN_OUT_OF_MEMORY;
   }
   else temp = _smallStringBuf;
   
   if (temp)
   {
      _next = _tokenizeMe = temp;
      memcpy(temp, tokenizeMe, _bufLen);
   }
   else DefaultInitialize();  // D'oh!
}

StringTokenizer :: StringTokenizer(bool junk, char * tokenizeMe, const char * hardSeparators, const char * softSeparators, char escapeChar)
   : _allocedBufferOnHeap(false)
   , _prevSepWasHard(false)
   , _escapeChar(escapeChar)
   , _prevChar(escapeChar+1)
   , _bufLen(0)
   , _tokenizeMe(tokenizeMe?tokenizeMe:_dummyString)
   , _next(tokenizeMe?tokenizeMe:_dummyString)
{
   (void) junk;
   SetBitChord(_hardSepsBitChord, hardSeparators);
   SetBitChord(_softSepsBitChord, softSeparators);
}

StringTokenizer :: StringTokenizer(const StringTokenizer & rhs)
{
   CopyDataToPrivateBuffer(rhs);
}

StringTokenizer & StringTokenizer :: operator = (const StringTokenizer & rhs)
{
   if (this != &rhs)
   {
      DeletePrivateBufferIfNecessary();
      CopyDataToPrivateBuffer(rhs);
   }
   return *this;
}

StringTokenizer :: ~StringTokenizer()
{
   DeletePrivateBufferIfNecessary();
}

void StringTokenizer :: DefaultInitialize()
{
   SetBitChord(_hardSepsBitChord, NULL);
   SetBitChord(_softSepsBitChord, NULL);

   _allocedBufferOnHeap = false;
   _prevSepWasHard      = false;
   _escapeChar          = '\0';
   _prevChar            = _escapeChar+1;
   _bufLen              = 0;
   _tokenizeMe          = _dummyString;
   _next                = _dummyString;
}

// Should only be called when all of our pointers are pointing to another StringTokenizer's buffer,
// and we want to allocate our own duplicate buffer and point to that instead
// Note that this method gets called from the copy-constructor while all member variables are still
// uninitialized so it needs to be careful not to read from any of this object's member variables!
void StringTokenizer :: CopyDataToPrivateBuffer(const StringTokenizer & copyFrom)
{
   _allocedBufferOnHeap = copyFrom._allocedBufferOnHeap;
   _prevSepWasHard      = copyFrom._prevSepWasHard;
   _escapeChar          = copyFrom._escapeChar;
   _prevChar            = copyFrom._prevChar;
   _bufLen              = copyFrom._bufLen;

   if (copyFrom._allocedBufferOnHeap)
   {
      char * newBuf = newnothrow_array(char, copyFrom._bufLen);
      if (newBuf)
      {
         memcpy(newBuf, copyFrom._tokenizeMe, copyFrom._bufLen); // copies all three strings at once!
         SetPointersAnalogousTo(newBuf, copyFrom);
      }
      else
      {
         MWARN_OUT_OF_MEMORY;
         DefaultInitialize(); // We're boned -- default-initialize everything just to avoid undefined behavior
      }
   }
   else if (copyFrom._tokenizeMe == copyFrom._smallStringBuf)
   {
      // (copyFrom) is using the small-data optimization, so we should too
      memcpy(_smallStringBuf, copyFrom._smallStringBuf, copyFrom._bufLen);
      SetPointersAnalogousTo(_smallStringBuf, copyFrom);
   }
   else
   {
      // (copyFrom)'s pointers are pointing to user-provided memory, so just use the same pointers he is using
      memcpy(_hardSepsBitChord, copyFrom._hardSepsBitChord, sizeof(_hardSepsBitChord));
      memcpy(_softSepsBitChord, copyFrom._softSepsBitChord, sizeof(_softSepsBitChord));
      _tokenizeMe = copyFrom._tokenizeMe;
      _next       = copyFrom._next;
   } 
}

// Sets our points at the same offsets relative to (myNewBuf) that (copyFrom)'s pointers are relative to its _hardSeparators pointer
void StringTokenizer :: SetPointersAnalogousTo(char * myNewBuf, const StringTokenizer & copyFrom)
{
   const char * oldBuf = copyFrom._tokenizeMe;

   memcpy(_hardSepsBitChord, copyFrom._hardSepsBitChord, sizeof(_hardSepsBitChord));
   memcpy(_softSepsBitChord, copyFrom._softSepsBitChord, sizeof(_softSepsBitChord));
   _tokenizeMe = myNewBuf;
   _next       = myNewBuf + (copyFrom._next - copyFrom._tokenizeMe);
}

void StringTokenizer :: DeletePrivateBufferIfNecessary()
{
   if (_allocedBufferOnHeap) delete [] _tokenizeMe;
}

char * StringTokenizer :: GetNextToken()
{
   MovePastSoftSeparatorChars();
   if ((*_next)||(_prevSepWasHard))
   {
      _prevSepWasHard = false;
      char * ret = _next;

      // Advance _next to the next sep-char in the string
      while((*_next)&&(!IsHardSeparatorChar(_prevChar, *_next))&&(!IsSoftSeparatorChar(_prevChar, *_next))) UpdatePrevChar(*_next++);

      if (*_next)
      {
         const bool wasHardSep = IsHardSeparatorChar(_prevChar, *_next);
         *_next++  = '\0';
         _prevChar = _escapeChar+1;
         if ((wasHardSep)&&(*_next == '\0')) _prevSepWasHard = true;  // so that e.g. strings ending in a comma produce an extra empty-output
      }
      return ret;
   }

   return NULL;
}

char * StringTokenizer :: GetRemainderOfString()
{  
   MovePastSoftSeparatorChars();
   return (*_next) ? _next : NULL;  // and return from there
}

void StringTokenizer :: SetBitChord(uint32 * bits, const char * seps)
{
   memset(bits, 0, sizeof(_hardSepsBitChord));
   if (seps) for (const char * s = seps; (*s != '\0'); s++) bits[(*s)/32] |= (1<<((*s)%32));
}

} // end namespace muscle
