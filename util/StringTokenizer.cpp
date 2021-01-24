/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/StringTokenizer.h"

namespace muscle {

static char _dummyString[] = "";

StringTokenizer :: StringTokenizer(const char * tokenizeMe, const char * hardSeparators, const char * softSeparators)
   : _alloced(false)
   , _prevSepWasHard(false)
{
   if (tokenizeMe     == NULL) tokenizeMe     = "";
   if (hardSeparators == NULL) hardSeparators = "";
   if (softSeparators == NULL) softSeparators = "";
   
   const size_t tlen   = strlen(tokenizeMe);
   const size_t sslen  = strlen(softSeparators);
   const size_t hslen  = strlen(hardSeparators);
   const size_t bufLen = (sslen+1)+(hslen+1)+(tlen+1);
   const bool doAlloc  = (bufLen > sizeof(_smallStringBuf));  // only allocate from the heap if _smallStringBuf isn't big enough
   
   char * temp;
   if (doAlloc)
   {  
      temp = newnothrow_array(char, bufLen);
      if (temp) _alloced = true;
           else WARN_OUT_OF_MEMORY;
   }
   else temp = _smallStringBuf;
   
   if (temp)
   {  
      const char * firstInvalidByte = temp+bufLen;
      
      _next = temp;
      _hardSeparators = temp;
      muscleStrncpy(_next, hardSeparators, firstInvalidByte-_next);
      _next += (hslen+1);
      
      _softSeparators = _next;
      muscleStrncpy(_next, softSeparators, firstInvalidByte-_next);
      _next += (sslen+1);
      
      muscleStrncpy(_next, tokenizeMe, firstInvalidByte-_next);
   }
   else _hardSeparators = _softSeparators = _next = _dummyString;
}

StringTokenizer :: StringTokenizer(bool junk, char * tokenizeMe, const char * hardSeparators, const char * softSeparators)
   : _alloced(false)
   , _prevSepWasHard(false)
   , _hardSeparators(hardSeparators?hardSeparators:"")
   , _softSeparators(softSeparators?softSeparators:"")
   , _next(tokenizeMe?tokenizeMe:_dummyString)
{
   (void) junk;
}

StringTokenizer :: ~StringTokenizer()
{
   // must cast to (char *) or VC++ complains :^P
   if (_alloced) delete [] ((char *)_hardSeparators);  // yes, this holds all three strings in a single allocation
}

char * StringTokenizer :: GetNextToken()
{
   while((*_next)&&(IsSoftSeparatorChar(*_next))) _next++;
   if ((*_next)||(_prevSepWasHard))
   {
      _prevSepWasHard = false;
      char * ret = _next;  // points either to data-char or to a hard-separator-char

      // Move until next sep-char (soft or hard)
      while((*_next)&&((!IsHardSeparatorChar(*_next))&&(!IsSoftSeparatorChar(*_next)))) _next++;
      if (*_next)
      {
         const bool wasHardSep = IsHardSeparatorChar(*_next);
         *_next = '\0';
         _next++;
         if ((wasHardSep)&&(*_next == '\0')) _prevSepWasHard = true;  // so that e.g. strings ending in a comma produce an extra empty-output
      }
      return ret;
   }

   return NULL;
}

char * StringTokenizer :: GetRemainderOfString()
{  
   // Move until first non-sep char
   while((*_next)&&(IsSoftSeparatorChar(*_next))) _next++;
   return (*_next) ? _next : NULL;  // and return from there
   
   return NULL;
}

} // end namespace muscle
