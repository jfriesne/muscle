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

   char * bufPtr;
   if (doAlloc)
   {
      bufPtr = newnothrow_array(char, _bufLen);
      if (bufPtr) _allocedBufferOnHeap = true;
             else MWARN_OUT_OF_MEMORY;
   }
   else bufPtr = _smallStringBuf;
   
   if (bufPtr)
   {
      _nextToRead = _nextToWrite = _tokenizeMe = bufPtr;
      memcpy(bufPtr, tokenizeMe, _bufLen);
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
   , _nextToRead(_tokenizeMe)
   , _nextToWrite(_tokenizeMe)
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
   _allocedBufferOnHeap = false;
   _prevSepWasHard      = false;
   _escapeChar          = '\0';
   _prevChar            = _escapeChar+1;
   _bufLen              = 0;
   _tokenizeMe          = _dummyString;
   _nextToRead          = _dummyString;
   _nextToWrite         = _dummyString;

   SetBitChord(_hardSepsBitChord, NULL);
   SetBitChord(_softSepsBitChord, NULL);
}

// Note that this method gets called from the copy-constructor while all member variables are still
// uninitialized, so it needs to be careful not to read from any of this object's member variables!
void StringTokenizer :: CopyDataToPrivateBuffer(const StringTokenizer & copyFrom)
{
   memcpy(_hardSepsBitChord, copyFrom._hardSepsBitChord, sizeof(_hardSepsBitChord));
   memcpy(_softSepsBitChord, copyFrom._softSepsBitChord, sizeof(_softSepsBitChord));

   _prevSepWasHard = copyFrom._prevSepWasHard;
   _escapeChar     = copyFrom._escapeChar;
   _prevChar       = copyFrom._prevChar;
   _bufLen         = copyFrom._bufLen;

   if (_bufLen <= sizeof(_smallStringBuf))
   {
      _allocedBufferOnHeap = false;
      memcpy(_smallStringBuf, copyFrom._tokenizeMe, _bufLen);
      SetPointersAnalogousTo(_smallStringBuf, copyFrom);
   }
   else
   {
      char * newBuf = newnothrow_array(char, copyFrom._bufLen);
      if (newBuf)
      {
         _allocedBufferOnHeap = true;
         memcpy(newBuf, copyFrom._tokenizeMe, copyFrom._bufLen);
         SetPointersAnalogousTo(newBuf, copyFrom);
      }
      else
      {
         MWARN_OUT_OF_MEMORY;
         DefaultInitialize(); // We're boned -- default-initialize everything just to avoid undefined behavior
      }
   }
}

// Sets our points at the same offsets relative to (myNewBuf) that (copyFrom)'s pointers are relative to its _hardSeparators pointer
void StringTokenizer :: SetPointersAnalogousTo(char * myNewBuf, const StringTokenizer & copyFrom)
{
   const char * oldBuf = copyFrom._tokenizeMe;
   _tokenizeMe  = myNewBuf;
   _nextToRead  = myNewBuf + (copyFrom._nextToRead  - copyFrom._tokenizeMe);
   _nextToWrite = myNewBuf + (copyFrom._nextToWrite - copyFrom._tokenizeMe);
}

void StringTokenizer :: DeletePrivateBufferIfNecessary()
{
   if (_allocedBufferOnHeap) delete [] _tokenizeMe;
}

char * StringTokenizer :: GetNextToken()
{
   MovePastSoftSeparatorChars();
   if ((*_nextToRead)||(_prevSepWasHard))
   {
      _prevSepWasHard = false;
      char * ret = _nextToRead;

      // Advance _nextToRead to the next sep-char in the string
      while((*_nextToRead)&&(!IsHardSeparatorChar(_prevChar, *_nextToRead))&&(!IsSoftSeparatorChar(_prevChar, *_nextToRead))) Advance();

      if (*_nextToRead)
      {
         _prevSepWasHard = IsHardSeparatorChar(_prevChar, *_nextToRead);
         *_nextToRead++ = '\0';
         *_nextToWrite  = '\0';
         _nextToWrite = _nextToRead;
         _prevChar    = _escapeChar+1;
      }
      else if (_nextToRead > _nextToWrite) *_nextToWrite = '\0';

      return ret;
   }

   return NULL;
}

char * StringTokenizer :: GetRemainderOfString()
{  
   MovePastSoftSeparatorChars();
   return (*_nextToRead) ? _nextToRead : NULL;  // and return from there
}

void StringTokenizer :: SetBitChord(uint32 * bits, const char * seps)
{
   memset(bits, 0, sizeof(_hardSepsBitChord));
   if (seps) for (const char * s = seps; (*s != '\0'); s++) bits[(*s)/32] |= (1<<((*s)%32));
}

} // end namespace muscle
