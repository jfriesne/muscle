/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/StringTokenizer.h"

namespace muscle {

static char _dummyString[] = "";

StringTokenizer :: StringTokenizer(const char * tokenizeMe, const char * hardSeparators, const char * softSeparators)
   : _allocedBufferOnHeap(false)
   , _prevSepWasHard(false)
{
   if (tokenizeMe     == NULL) tokenizeMe     = "";
   if (hardSeparators == NULL) hardSeparators = "";
   if (softSeparators == NULL) softSeparators = "";
   
   const size_t tlen  = strlen(tokenizeMe)    +1; // +1 for the NUL byte
   const size_t sslen = strlen(softSeparators)+1; // ""
   const size_t hslen = strlen(hardSeparators)+1; // ""

   _bufLen = (uint32) (sslen+hslen+tlen);
   const bool doAlloc  = (_bufLen > sizeof(_smallStringBuf));  // only allocate from the heap if _smallStringBuf isn't big enough
   
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
      _hardSeparators = temp; memcpy(temp, hardSeparators, hslen); temp += hslen;
      _softSeparators = temp; memcpy(temp, softSeparators, sslen); temp += sslen;
      _next           = temp; memcpy(temp,     tokenizeMe, tlen);  //temp += tlen;
   }
   else DefaultInitialize();  // D'oh!
}

StringTokenizer :: StringTokenizer(bool junk, char * tokenizeMe, const char * hardSeparators, const char * softSeparators)
   : _allocedBufferOnHeap(false)
   , _prevSepWasHard(false)
   , _bufLen(0)
   , _hardSeparators(hardSeparators?hardSeparators:"")
   , _softSeparators(softSeparators?softSeparators:"")
   , _next(tokenizeMe?tokenizeMe:_dummyString)
{
   (void) junk;
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
   _bufLen              = 0;
   _hardSeparators      = "";
   _softSeparators      = "";
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
   _bufLen              = copyFrom._bufLen;

   if (copyFrom._allocedBufferOnHeap)
   {
      char * newBuf = newnothrow_array(char, copyFrom._bufLen);
      if (newBuf)
      {
         memcpy(newBuf, copyFrom._hardSeparators, copyFrom._bufLen); // copies all three strings at once!
         SetPointersAnalogousTo(newBuf, copyFrom);
      }
      else
      {
         MWARN_OUT_OF_MEMORY;
         DefaultInitialize(); // We're boned -- default-initialize everything just to avoid undefined behavior
      }
   }
   else if (copyFrom._hardSeparators == copyFrom._smallStringBuf)
   {
      // (copyFrom) is using the small-data optimization, so we should too
      memcpy(_smallStringBuf, copyFrom._smallStringBuf, copyFrom._bufLen);
      SetPointersAnalogousTo(_smallStringBuf, copyFrom);
   }
   else
   {
      // (copyFrom)'s pointers are pointing to user-provided memory, so just use the same pointers he is using
      _hardSeparators = copyFrom._hardSeparators;
      _softSeparators = copyFrom._softSeparators;
      _next           = copyFrom._next;
   } 
}

// Sets our points at the same offsets relative to (myNewBuf) that (copyFrom)'s pointers are relative to its _hardSeparators pointer
void StringTokenizer :: SetPointersAnalogousTo(char * myNewBuf, const StringTokenizer & copyFrom)
{
   const char * oldBuf = copyFrom._hardSeparators;

   _hardSeparators = myNewBuf;
   _softSeparators = myNewBuf + (copyFrom._softSeparators - oldBuf);
   _next           = myNewBuf + (copyFrom._next           - oldBuf);
}

void StringTokenizer :: DeletePrivateBufferIfNecessary()
{
   // must cast to (char *) or VC++ complains :^P
   if (_allocedBufferOnHeap) delete [] ((char *)_hardSeparators);  // yes, this holds all three strings in a single allocation
}

char * StringTokenizer :: GetNextToken()
{
   while((*_next)&&(IsSoftSeparatorChar(*_next))) _next++;
   if ((*_next)||(_prevSepWasHard))
   {
      _prevSepWasHard = false;
      char * ret = _next;

      // Advance _next to the next sep-char in the string
      while((*_next)&&(!IsHardSeparatorChar(*_next))&&(!IsSoftSeparatorChar(*_next))) _next++;

      if (*_next)
      {
         const bool wasHardSep = IsHardSeparatorChar(*_next);
         *_next++ = '\0';
         if ((wasHardSep)&&(*_next == '\0')) _prevSepWasHard = true;  // so that e.g. strings ending in a comma produce an extra empty-output
      }
      return ret;
   }

   return NULL;
}

char * StringTokenizer :: GetRemainderOfString()
{  
   while((*_next)&&(IsSoftSeparatorChar(*_next))) _next++;
   return (*_next) ? _next : NULL;  // and return from there
}

} // end namespace muscle
