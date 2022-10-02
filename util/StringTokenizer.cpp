/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/StringTokenizer.h"
#include "util/MiscUtilityFunctions.h"

namespace muscle {

StringTokenizer :: StringTokenizer(const char * tokenizeMe, const char * optSepChars, char escapeChar)
   : _allocedBufferOnHeap(false)
   , _prevSepWasHard(false)
   , _escapeChar(escapeChar)
   , _prevChar(escapeChar+1)
{
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
      SetBitChords(optSepChars);

      _nextToRead = _nextToWrite = _tokenizeMe = bufPtr;
      memcpy(bufPtr, tokenizeMe, _bufLen);
   }
   else DefaultInitialize();  // D'oh!
}

static char _dummyString[] = "";

StringTokenizer :: StringTokenizer(bool junk, char * tokenizeMe, const char * optSepChars, char escapeChar)
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
   SetBitChords(optSepChars);
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

   SetBitChords(NULL);
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

void StringTokenizer :: SetBitChords(const char * optSepChars)
{
   muscleClearArray(_softSepsBitChord);
   muscleClearArray(_hardSepsBitChord);
   if (optSepChars == NULL) optSepChars = STRING_TOKENIZER_DEFAULT_SEPARATOR_CHARS;
   for (const char * s = optSepChars; (*s != '\0'); s++) SetBit(IsBitSet(_softSepsBitChord, *s) ? _hardSepsBitChord : _softSepsBitChord, *s);
   for (uint32 i=0; i<ARRAYITEMS(_hardSepsBitChord); i++) _softSepsBitChord[i] &= ~_hardSepsBitChord[i];  // if it's hard, it can't be soft!
}

Queue<String> StringTokenizer :: Split(uint32 maxResults)
{
   Queue<String> ret;
   const char * t;
   while((ret.GetNumItems()<maxResults)&&((t=(*this)()) != NULL)) (void) ret.AddTail(t);
   return ret;
}

String StringTokenizer :: Join(const Queue<String> & tokenizedStrings, bool includeEmptyStrings, char joinChar, char escapeChar)
{
   String ret;
   for (uint32 i=0; i<tokenizedStrings.GetNumItems(); i++)
   {
      const String & subStr = tokenizedStrings[i];
      if ((includeEmptyStrings)||(subStr.HasChars()))
      {
         if (includeEmptyStrings?(i>0):ret.HasChars()) ret += joinChar;
         ret += (escapeChar != '\0') ? subStr.WithCharsEscaped(joinChar, escapeChar) : subStr;
      }
   }
   return ret;
}

} // end namespace muscle
