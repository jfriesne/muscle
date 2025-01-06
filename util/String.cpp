/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "util/Queue.h"
#include "util/String.h"
#include "support/Point.h"
#include "support/Rect.h"

#ifdef __APPLE__
# include <CoreFoundation/CFString.h>
#endif

namespace muscle {

const char * StrcasestrEx(const char * haystack, uint32 haystackLen, const char * needle, uint32 needleLen, bool searchBackwards)
{
   if ((haystack==NULL)||(needle==NULL)) return NULL;

   if (haystackLen >= needleLen)
   {
      const uint32 searchLen = haystackLen-(needleLen-1);
      if (searchBackwards)
      {
         for (int32 i=(searchLen-1); i>=0; i--) if (Strncasecmp(haystack+i, needle, needleLen) == 0) return haystack+i;
      }
      else
      {
         for (uint32 i=0; i<searchLen; i++) if (Strncasecmp(haystack+i, needle, needleLen) == 0) return haystack+i;
      }
   }
   return NULL;
}

const char * Strcasestr(const char * haystack, const char * needle)
{
#ifdef WIN32
   return StrcasestrEx(haystack, (uint32)strlen(haystack), needle, (uint32)strlen(needle), false);
#else
   return strcasestr(haystack, needle);
#endif
}

int String :: IndexOfIgnoreCase(char ch, uint32 f) const
{
   const char lowerChar = (char) tolower(ch);
   const char upperChar = (char) toupper(ch);
   if (lowerChar == upperChar) return IndexOf(ch, f);
   else
   {
      const char * p = Cstr();
      for (uint32 i=f; i<Length(); i++) if ((p[i] == lowerChar)||(p[i] == upperChar)) return i;
      return -1;
   }
}

int String :: IndexOfIgnoreCase(const String & s, uint32 f) const
{
   const char * p = (f<Length())?StrcasestrEx(Cstr()+f, Length()-f, s(), s.Length(), false):NULL;
   return p?(int)(p-Cstr()):-1;
}

int String :: IndexOfIgnoreCase(const char * s, uint32 f) const
{
   if (s==NULL) s="";

   const char * p = (f<Length()) ? StrcasestrEx(Cstr()+f, Length()-f, s, (uint32)strlen(s), false) : NULL;
   return p ? ((int)(p-Cstr())): -1;
}

int String :: LastIndexOfIgnoreCase(const String & s, uint32 f) const
{
   const char * p = (f<Length()) ? StrcasestrEx(Cstr()+f, Length()-f, s(), s.Length(), true) : NULL;
   return p ? ((int)(p-Cstr())) : -1;
}

int String :: LastIndexOfIgnoreCase(const char * s, uint32 f) const
{
   if (s==NULL) s="";

   const char * p = (f<Length()) ? StrcasestrEx(Cstr()+f, Length()-f, s, (uint32)strlen(s), true) : NULL;
   return p ? ((int)(p-Cstr())) : -1;
}

int String :: LastIndexOfIgnoreCase(char ch, uint32 f) const
{
   const char lowerChar = (char) tolower(ch);
   const char upperChar = (char) toupper(ch);
   if (lowerChar == upperChar) return LastIndexOf(ch, f);
   else
   {
      const char * p = Cstr();
      for (int32 i=((int32)Length())-1; i>=(int32)f; i--) if ((p[i] == lowerChar)||(p[i] == upperChar)) return i;
      return -1;
   }
}

bool String :: EndsWithIgnoreCase(const char * s) const
{
   if (s==NULL) s="";

   const uint32 slen = (uint32) strlen(s);
   return ((Length() >= slen)&&(Strcasecmp(Cstr()+(Length()-slen), s) == 0));
}


void String :: ClearAndFlush()
{
   if (IsArrayDynamicallyAllocated()) muscleFree(_strData._bigBuffer);
   ClearSmallBuffer();
   _bufferLen = sizeof(_strData._smallBuffer);
   _length = 0;
}

status_t String :: SetFromString(const String & s, uint32 firstChar, uint32 afterLastChar)
{
   afterLastChar = muscleMin(afterLastChar, s.Length());
   const uint32 len = (afterLastChar > firstChar) ? (afterLastChar-firstChar) : 0;
   if (len > 0)
   {
      MRETURN_ON_ERROR(EnsureBufferSize(len+1, false, false));  // guaranteed not to realloc in the (&s==this) case

      char * b = GetBuffer();
      memmove(b, s()+firstChar, len);  // memmove() is used in case (&s==this)
      b[len]  = '\0';
      _length = len;
   }
   else ClearAndFlush();

   return B_NO_ERROR;
}

status_t String :: SetCstr(const char * str, uint32 maxLen)
{
   // If (str)'s got a NUL byte before maxLen, make (maxLen) smaller.
   // We can't call strlen(str) because we don't have any guarantee that the NUL
   // byte even exists!  Without a NUL byte, strlen() could run off into the weeds...
   uint32 sLen = 0;
   if (str) {while((sLen<maxLen)&&(str[sLen] != '\0')) sLen++;}
   maxLen = muscleMin(maxLen, sLen);
   if (maxLen > 0)
   {
      if (str[maxLen-1] != '\0') maxLen++;  // make room to add the NUL byte if necessary

      MRETURN_ON_ERROR(EnsureBufferSize(maxLen, false, false));  // guaranteed not to realloc in the IsCharInLocalArray(str) case

      char * b = GetBuffer();
      memmove(b, str, maxLen-1);  // memmove() is used in case (str) points into our array
      b[maxLen-1] = '\0';
      _length = maxLen-1;
   }
   else Clear();

   return B_NO_ERROR;
}

String &
String :: operator+=(const String &other)
{
   const uint32 otherLen = other.Length();  // save this value first, in case (&other==this)
   if ((otherLen > 0)&&(EnsureBufferSize(Length()+otherLen+1, true, false).IsOK()))
   {
      memmove(GetBuffer()+_length, other(), otherLen+1);  // memmove() is used in case (&other==this)
      _length += otherLen;
   }
   return *this;
}

String &
String :: operator+=(const char * other)
{
   if (other == NULL) other = "";
   const uint32 otherLen = (uint32) strlen(other);
   if (otherLen > 0)
   {
      if (IsCharInLocalArray(other)) return operator+=(String(other,otherLen));  // avoid potential free-ing of (other) inside EnsureBufferSize()
      if (EnsureBufferSize(Length()+otherLen+1, true, false).IsOK())
      {
         memcpy(GetBuffer()+_length, other, otherLen+1);
         _length += otherLen;
      }
   }
   return *this;
}

String & String :: operator-=(const char aChar)
{
   const int idx = LastIndexOf(aChar);
   if (idx >= 0)
   {
      char * b = GetBuffer();
      memmove(b+idx, b+idx+1, _length-idx);
      --_length;
   }
   return *this;
}

String & String :: operator-=(const String &other)
{
        if (*this == other) Clear();
   else if (other.HasChars())
   {
      const int idx = LastIndexOf(other);
      if (idx >= 0)
      {
         const uint32 newEndIdx = idx+other.Length();
         char * b = GetBuffer();
         memmove(b+idx, b+newEndIdx, 1+_length-newEndIdx);
         _length -= other.Length();
      }
   }
   return *this;
}

String & String :: operator-=(const char * other)
{
   const int otherLen = ((other)&&(*other)) ? (int)strlen(other) : 0;
   if (otherLen > 0)
   {
      const int idx = LastIndexOf(other);
      if (idx >= 0)
      {
         const uint32 newEndIdx = idx+otherLen;
         char * b = GetBuffer();
         memmove(b+idx, b+newEndIdx, 1+_length-newEndIdx);
         _length -= otherLen;
      }
   }
   return *this;
}

String & String :: operator<<(int rhs)
{
   char buf[64];
   muscleSprintf(buf, "%d", rhs);
   return *this << buf;
}

String & String :: operator<<(float rhs)
{
   char buf[64];
   muscleSprintf(buf, "%.2f", rhs);
   return *this << buf;
}

String & String :: operator<<(bool rhs)
{
   const char * val = rhs ? "true" : "false";
   return *this << val;
}

void String :: Reverse()
{
   if (HasChars())
   {
      uint32 from = 0;
      uint32 to = Length()-1;
      char * b = GetBuffer();
      while(from<to) muscleSwap(b[from++], b[to--]);
   }
}

uint32 String :: Replace(char findChar, char replaceChar, uint32 maxReplaceCount, uint32 fromIndex)
{
   uint32 ret = 0;
   if ((findChar != replaceChar)&&(fromIndex < Length()))
   {
      char * c = GetBuffer()+fromIndex;
      while((*c)&&(maxReplaceCount > 0))
      {
         if (*c == findChar)
         {
            *c = replaceChar;
            maxReplaceCount--;
            ret++;
         }
         c++;
      }
   }
   return ret;
}

String String :: WithReplacements(char replaceMe, char withMe, uint32 maxReplaceCount, uint32 fromIndex) const
{
   String ret = *this;
   ret.Replace(replaceMe, withMe, maxReplaceCount, fromIndex);
   return ret;
}

static bool IsSeparatorChar(uint8 c, const uint32 * sepBits) {return ((sepBits[c/32] & (1<<(c%32))) != 0);}

String String :: WithCharsEscaped(const char * charsToEscape, char escapeChar) const
{
   if (charsToEscape == NULL) charsToEscape = "";

   uint32 sepBits[8]; memset(sepBits, 0, sizeof(sepBits));
   {
      for (const char * pc = charsToEscape; *pc != '\0'; pc++)
      {
         const uint8 c = *pc;
         sepBits[c/32] |= (1<<(c%32));
      }
   }

   uint32 numSeps = 0;
   for (const char * pc = Cstr(); *pc != '\0'; pc++) if (IsSeparatorChar(*pc, sepBits)) numSeps++;

   const uint32 numBackslashes = GetNumInstancesOf(escapeChar);
   if ((numSeps == 0)&&(numBackslashes == 0)) return *this;  // nothing to do!

   String escapedName(PreallocatedItemSlotsCount(Length()+(2*(numSeps+numBackslashes))));  // conservative estimate, to avoid any reallocs below

   const char * thisStr   = Cstr();
   bool prevCharWasEscape = false;
   char actualPrevChar    = '\0';
   for (uint32 i=0; i<Length(); i++)
   {
      const char curChar  = thisStr[i];
      const char nextChar = thisStr[i+1];
      if (prevCharWasEscape == false)
      {
              if (IsSeparatorChar(curChar, sepBits))                                                        escapedName += escapeChar;
         else if ((curChar == escapeChar)&&(nextChar != escapeChar)&&(!IsSeparatorChar(nextChar, sepBits))) escapedName += escapeChar;
      }

      escapedName       += curChar;
      prevCharWasEscape  = ((curChar == escapeChar)&&(actualPrevChar != escapeChar));
      actualPrevChar     = curChar;
   }
   return escapedName;
}

int32 String :: Replace(const String & replaceMe, const String & withMe, uint32 maxReplaceCount, uint32 fromIndex)
{
   TCHECKPOINT;

   if (maxReplaceCount == 0)  return 0;
   if (fromIndex >= Length()) return 0;
   if (replaceMe.IsEmpty())   return 0;  // can't replace an empty string, that's silly!
   if (replaceMe  == withMe)  return muscleMin(maxReplaceCount, GetNumInstancesOf(replaceMe, fromIndex));  // no changes necessary!
   if (&replaceMe == this)    return (fromIndex==0)?(SetFromString(withMe).IsOK()?1:-1):0;                 // avoid self-entanglement
   if (&withMe    == this)    return Replace(replaceMe, String(withMe), maxReplaceCount, fromIndex);       // avoid self-entanglement

   String temp;
   const int32 perInstanceDelta = ((int32)withMe.Length())-((int32)replaceMe.Length());
   if (perInstanceDelta > 0)
   {
      // If we are replacing a shorter string with a longer string, we'll have to do a copy-and-swap
      const uint32 numInstances = muscleMin(GetNumInstancesOf(replaceMe, fromIndex), maxReplaceCount);
      if (numInstances == 0) return 0;  // no changes necessary!
      if (temp.Prealloc(Length()+(perInstanceDelta*numInstances)).IsError()) return -1;
   }

   // This code works for both the in-place and the copy-over modes!
   int32 ret = 0;
   const char * readPtr = Cstr();
   char * writePtr = (perInstanceDelta > 0) ? temp.GetBuffer() : NULL;
   if (writePtr) for (uint32 i=0; i<fromIndex; i++) *writePtr++ = *readPtr++;
            else readPtr += fromIndex;
   while(1)
   {
      char * nextReplaceMe = (maxReplaceCount>0) ? strstr((char *) readPtr, (char *) replaceMe()) : NULL;
      if (nextReplaceMe)
      {
         ret++;
         if (writePtr)
         {
            const uint32 numBytes = (uint32) (nextReplaceMe-readPtr);
            if (perInstanceDelta != 0) memmove(writePtr, readPtr, numBytes);
            writePtr += numBytes;
         }
         else writePtr = nextReplaceMe;

         memcpy(writePtr, withMe(), withMe.Length());
         readPtr  = nextReplaceMe + replaceMe.Length();
         writePtr += withMe.Length();
         maxReplaceCount--;
      }
      else
      {
         if (writePtr)
         {
            // Finish up
            const uint32 numBytes = (uint32) (Cstr()+Length()-readPtr);
            if (perInstanceDelta != 0) memmove(writePtr, readPtr, numBytes);
            writePtr += numBytes;
            *writePtr = '\0';
            if (perInstanceDelta > 0)
            {
               temp._length = (uint32) (writePtr-temp());
               SwapContents(temp);
            }
            else _length = (uint32) (writePtr-Cstr());
         }
         return ret;
      }
   }
   return ret;  // just to shut the compiler up; we never actually get here
}

String String :: WithReplacements(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount) const
{
   String writeTo;
   return (ReplaceAux(beforeToAfter, maxReplaceCount, writeTo) > 0) ? writeTo : *this;
}

int32 String :: Replace(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount)
{
   String writeTo;
   const int32 ret = ReplaceAux(beforeToAfter, maxReplaceCount, writeTo);
   if (ret > 0) SwapContents(writeTo);
   return ret;
}

int32 String :: ReplaceAux(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount, String & writeTo) const
{
   if ((maxReplaceCount == 0)||(beforeToAfter.IsEmpty())||(IsEmpty())) return 0;  // nothing to do!

   const uint32 origStrLength = Length();
   const uint32 numPairs      = beforeToAfter.GetNumItems();

   Queue<const String *> beforeStrs;
   if (beforeStrs.EnsureSize(beforeToAfter.GetNumItems()).IsError()) return -1;
   for (HashtableIterator<String, String> iter(beforeToAfter); iter.HasData(); iter++) if ((iter.GetKey().HasChars())&&(beforeStrs.AddTail(&iter.GetKey()).IsError())) return -1;  // AddTail() won't fail, but clang-tidy doesn't know that

   // Build up a map of what substrings to replace at what offsets into the original-string
   Hashtable<uint32, uint32> sourceOffsetToPairIndex;
   {
      Queue<const char *> states;
      if (states.EnsureSize(numPairs, true).IsError()) return -1; // so we won't have to worry about reallocs below
      for (uint32 i=0; i<numPairs; i++) states[i] = beforeStrs[i]->Cstr();  // NOLINT (I can't figure out why clang-tidy is worried about this, so I'll tape over it for now)

      for (uint32 i=0; i<origStrLength; i++)
      {
         const char c = (*this)[i];
         for (uint32 j=0; j<numPairs; j++)
         {
            if (*states[j] != c) states[j] = beforeStrs[j]->Cstr();  // match failed: back to initial state!
            if ((*states[j] == c)&&(*(++states[j]) == '\0'))
            {
               // We got to the NUL byte so we found a match for this before-string!  Record where and what it is for later
               uint32 * pairIdx = sourceOffsetToPairIndex.GetOrPut(1+i-beforeStrs[j]->Length(), MUSCLE_NO_LIMIT);
               if (pairIdx) *pairIdx = muscleMin(*pairIdx, j);  // earlier key/value pairs get precedence when there are two matches at the same offset
                       else return -1;
            }
         }
      }
   }
   if (sourceOffsetToPairIndex.IsEmpty()) return 0;  // nothing to do!

   // Now let's precalculate what our post-update string's length will be so we won't have to reallocate its buffer later
   uint32 finalStringLength = origStrLength;
   {
      uint32 rc = maxReplaceCount;
      for (uint32 i=0; ((rc>0)&&(i<origStrLength)); i++)
      {
         const uint32 * beforeIdx = sourceOffsetToPairIndex.Get(i);
         if (beforeIdx)
         {
            const String & before = *beforeStrs[*beforeIdx];    // guaranteed non-NULL
            const String & after  = *beforeToAfter.Get(before); // guaranteed non-NULL

            finalStringLength += (after.Length()-before.Length());
            i += (before.Length()-1);  // -1 to account for the fact the for-loop will increment i also
            if (rc != MUSCLE_NO_LIMIT) rc--;
         }
      }
   }

   writeTo.Clear();  // paranoia
   if (writeTo.Prealloc(finalStringLength).IsError()) return -1;

   // Finally we can assemble our actual updated string
   uint32 rc = 0;
   for (uint32 i=0; i<origStrLength; i++)
   {
      const uint32 * pairIdx = (maxReplaceCount > 0) ? sourceOffsetToPairIndex.Get(i) : NULL;
      if (pairIdx)
      {
         const String & before = *beforeStrs[*pairIdx];
         writeTo += beforeToAfter[before];
         i += (before.Length()-1);  // -1 since our for-loop will also increment the counter
         if (maxReplaceCount != MUSCLE_NO_LIMIT) maxReplaceCount--;
         rc++;
      }
      else writeTo += (*this)[i];
   }

   // This shouldn't ever happen, but I'm checking anyway just so any bugs in the above code will show up with an obvious symptom
   if (writeTo.Length() != finalStringLength)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "String::WithReplacements():  Final string length is " UINT32_FORMAT_SPEC ", expected " UINT32_FORMAT_SPEC "\n", writeTo.Length(), finalStringLength);
      MCRASH("WithReplacements::Wrong String Length");
   }

   return rc;
}

String String :: WithReplacements(const String & replaceMe, const String & withMe, uint32 maxReplaceCount, uint32 fromIndex) const
{
   String ret = *this;
   ret.Replace(replaceMe, withMe, maxReplaceCount, fromIndex);
   return ret;
}

int String :: LastIndexOf(const String &s2, uint32 fromIndex) const
{
   TCHECKPOINT;

   if (s2.IsEmpty()) return Length()-1;
   if (fromIndex >= Length()) return -1;
   for (int i=fromIndex; i>=0; i--) if (strncmp(Cstr()+i, s2.Cstr(), s2.Length()) == 0) return i;
   return -1;
}

int String :: LastIndexOf(const char * s2, uint32 fromIndex) const
{
   TCHECKPOINT;

   if (s2 == NULL) s2 = "";
   const uint32 s2Len = (uint32) strlen(s2);
   if (s2Len == 0) return Length()-1;
   if (fromIndex >= Length()) return -1;
   for (int i=fromIndex; i>=0; i--) if (strncmp(Cstr()+i, s2, s2Len) == 0) return i;
   return -1;
}

String String :: ToLowerCase() const
{
   String ret(*this);
   char * b = ret.GetBuffer();
   for (uint32 i=0; i<ret.Length(); i++) b[i] = (char)tolower(b[i]);
   return ret;
}

String String :: ToMixedCase() const
{
   bool prevCharWasLetter = false;
   String ret(*this);
   char * b = ret.GetBuffer();
   for (uint32 i=0; i<ret.Length(); i++)
   {
      char & c = b[i];
      const bool charIsLetter = (muscleInRange(c, 'a', 'z'))||(muscleInRange(c, 'A', 'Z'))||(muscleInRange(c, '0', '9'));  // yes, digits count as letters, dontcha know
      c = prevCharWasLetter ? (char)tolower(c) : (char)toupper(c);
      prevCharWasLetter = charIsLetter;
   }
   return ret;
}

String String :: ToUpperCase() const
{
   String ret(*this);
   char * b = ret.GetBuffer();
   for (uint32 i=0; i<ret.Length(); i++) b[i] = (char)toupper(b[i]);
   return ret;
}

String String :: Trimmed() const
{
   TCHECKPOINT;

   const int32 len = (int32) Length();
   const char * s = Cstr();
   int32 startIdx; for (startIdx = 0;     startIdx<len;    startIdx++) if (!IsSpaceChar(s[startIdx])) break;
   int32 endIdx;   for (endIdx   = len-1; endIdx>startIdx; endIdx--)   if (!IsSpaceChar(s[endIdx]))   break;
   return String(*this, (uint32)startIdx, (uint32)(endIdx+1));
}

uint32 String :: GetNumInstancesOf(char ch, uint32 fromIndex) const
{
   if (fromIndex >= Length()) return 0;

   uint32 ret = 0;
   for (const char * s = Cstr()+fromIndex; (*s != '\0'); s++) if (*s == ch) ret++;
   return ret;
}

uint32 String :: GetNumInstancesOf(const String & substring, uint32 fromIndex) const
{
   TCHECKPOINT;

   uint32 ret = 0;
   if (substring.HasChars())
   {
      uint32 lastIdx = fromIndex;
      int32 idx;
      while((idx = IndexOf(substring, lastIdx)) >= 0)
      {
         ret++;
         lastIdx = idx + substring.Length();
      }
   }
   return ret;
}

uint32 String :: GetNumInstancesOf(const char * substring, uint32 fromIndex) const
{
   TCHECKPOINT;

   if (substring == NULL) substring = "";
   uint32 ret = 0;
   const uint32 substringLength = (uint32) strlen(substring);
   if (substringLength > 0)
   {
      uint32 lastIdx = fromIndex;
      int32 idx;
      while((idx = IndexOf(substring, lastIdx)) >= 0)
      {
         ret++;
         lastIdx = idx + substringLength;
      }
   }
   return ret;
}

String String :: WithPrepend(const String & str, uint32 count) const
{
   TCHECKPOINT;

   if (&str == this) return WithPrepend(String(str), count);  // avoid self-entanglement

   String ret;
   const uint32 newLen = (count*str.Length())+Length();
   if (ret.Prealloc(newLen).IsOK())
   {
      char * b = ret.GetBuffer();

      if (str.HasChars())
      {
         for (uint32 i=0; i<count; i++)
         {
            memcpy(b, str(), str.Length());
            b += str.Length();
         }
      }
      if (HasChars())
      {
         memcpy(b, Cstr(), Length());
         b += Length();
      }

      char * afterBuf = ret.GetBuffer();
      ret._length = (uint32)(b-afterBuf);
      afterBuf[ret._length] = '\0';   // terminate the string
   }
   return ret;
}

String String :: WithPrepend(const char * str, uint32 count) const
{
   TCHECKPOINT;

        if ((str == NULL)||(*str == '\0')) return *this;
   else if (IsCharInLocalArray(str))       return WithPrepend(String(str), count);  // avoid self-entanglement!
   else
   {
      const uint32 sLen = (uint32) strlen(str);
      String ret;
      const uint32 newLen = (count*sLen)+Length();
      if (ret.Prealloc(newLen).IsOK())
      {
         char * b = ret.GetBuffer();

         if (sLen > 0)
         {
            for (uint32 i=0; i<count; i++)
            {
               memcpy(b, str, sLen);  // NOLINT(bugprone-not-null-terminated-result) -- the NUL-termination is done below
               b += sLen;
            }
         }
         if (HasChars())
         {
            memcpy(b, Cstr(), Length());
            b += Length();
         }
         char * afterBuf = ret.GetBuffer();
         ret._length = (uint32)(b-afterBuf);
         afterBuf[ret._length] = '\0';   // terminate the string
      }
      return ret;
   }
}

String String :: WithAppend(const String & str, uint32 count) const
{
   TCHECKPOINT;

   if (&str == this) return WithAppend(String(str), count);  // avoid self-entanglement

   String ret;
   const uint32 newLen = Length()+(count*str.Length());
   if (ret.Prealloc(newLen).IsOK())
   {
      char * b = ret.GetBuffer();
      if (HasChars())
      {
         memcpy(b, Cstr(), Length());
         b += Length();
      }
      if (str.HasChars())
      {
         for (uint32 i=0; i<count; i++)
         {
            memcpy(b, str(), str.Length());
            b += str.Length();
         }
      }
      char * afterBuf = ret.GetBuffer();
      ret._length = (uint32)(b-afterBuf);
      afterBuf[ret._length] = '\0';   // terminate the string
   }
   return ret;
}

String String :: WithAppend(const char * str, uint32 count) const
{
   TCHECKPOINT;

        if ((str == NULL)||(*str == '\0')) return *this;
   else if (IsCharInLocalArray(str))       return WithAppend(String(str), count);  // avoid self-entanglement!
   else
   {
      const uint32 sLen = (uint32) strlen(str);
      String ret;
      const uint32 newLen = Length()+(count*sLen);
      if (ret.Prealloc(newLen).IsOK())
      {
         char * b = ret.GetBuffer();
         if (HasChars())
         {
            memcpy(b, Cstr(), Length());
            b += Length();
         }
         if (sLen > 0)
         {
            for (uint32 i=0; i<count; i++)
            {
               memcpy(b, str, sLen);  // NOLINT(bugprone-not-null-terminated-result) -- the NUL-termination is done below
               b += sLen;
            }
         }
         char * afterBuf = ret.GetBuffer();
         ret._length = (uint32) (b-afterBuf);
         afterBuf[ret._length] = '\0';   // terminate the string
      }
      return ret;
   }
}

String String :: WithAppendedWord(const char * str, const char * sep) const
{
   if ((str == NULL)||(*str == '\0')) return *this;
   if ((HasChars())&&(strncmp(str, sep, strlen(sep)) != 0)&&(EndsWith(sep) == false)) return String(*this).WithAppend(sep).WithAppend(str);
                                                                                 else return String(*this).WithAppend(str);
}

String String :: WithAppendedWord(const String & str, const char * sep) const
{
   if (str.IsEmpty()) return *this;
   if ((HasChars())&&(str.StartsWith(sep) == false)&&(EndsWith(sep) == false)) return String(*this).WithAppend(sep).WithAppend(str);
                                                                          else return String(*this).WithAppend(str);
}

static uint32 NextPowerOfTwo(uint32 n)
{
   // This code was stolen from:  http://stackoverflow.com/a/1322548/131930

   n--;
   n |= n >> 1;   // Divide by 2^k for consecutive doublings of k up to 32,
   n |= n >> 2;   // and then or the results.
   n |= n >> 4;
   n |= n >> 8;
   n |= n >> 16;
   n++;           // The result is a number of 1 bits equal to the number
                  // of bits in the original number, plus 1. That's the
                  // next highest power of 2.
   return n;
}

static uint32 GetNextBufferSize(uint32 bufLen)
{
   // For very small strings, we'll try to conserve memory by betting that they won't expand much more
   if (bufLen < 32) return bufLen+SMALL_MUSCLE_STRING_LENGTH;

   static const uint32 STRING_PAGE_SIZE       = 4096;
   static const uint32 STRING_MALLOC_OVERHEAD = 12;  // we assume that malloc() might need as many as 12 bytes for book keeping

   // For medium-length strings, do a geometric expansion to reduce the number of reallocations
   uint32 geomLen = NextPowerOfTwo((bufLen-1)*2);
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   geomLen -= sizeof(size_t);  // so that the internal allocation size will be a power of two, after GlobalMemoryAllocator.cpp adds its header bytes
#endif
   if (geomLen < (STRING_PAGE_SIZE-STRING_MALLOC_OVERHEAD)) return geomLen;

   // For large (multi-page) allocations, we'll increase by one page.  According to Trolltech, modern implementations
   // of realloc() don't actually copy the entire large buffer, they just rearrange the memory map and add
   // a new page to the end, so this will be more efficient than it appears.
   const uint32 curNumPages = (bufLen+STRING_MALLOC_OVERHEAD)/STRING_PAGE_SIZE;
   return ((curNumPages+1)*STRING_PAGE_SIZE)-STRING_MALLOC_OVERHEAD;
}

// This method tries to ensure that at least (newBufLen) chars
// are available for storing data in.  (requestedBufLen) should include
// the terminating NUL.  If (retainValue) is true, the current string value
// will be retained; otherwise it should be set right after this call returns...
status_t String :: EnsureBufferSize(uint32 requestedBufLen, bool retainValue, bool allowShrink)
{
   if (allowShrink ? (requestedBufLen == _bufferLen) : (requestedBufLen <= _bufferLen)) return B_NO_ERROR;

   // If we're doing a first-time allocation or a shrink, allocate exactly the number of the bytes requested.
   // If it's a re-allocation, allocate more than requested in the hopes avoiding another realloc in the future.
   char * newBuf = NULL;
   const bool arrayWasDynamicallyAllocated = IsArrayDynamicallyAllocated();
   const uint32 newBufLen = ((allowShrink)||(requestedBufLen <= SMALL_MUSCLE_STRING_LENGTH+1)||((IsEmpty())&&(!arrayWasDynamicallyAllocated))) ? requestedBufLen : GetNextBufferSize(requestedBufLen);
   if (newBufLen == 0)
   {
      ClearAndFlush();
      return B_NO_ERROR;
   }

   const bool goToSmallBufferMode = ((allowShrink)&&(newBufLen <= (SMALL_MUSCLE_STRING_LENGTH+1)));
   const uint32 newMaxLength = newBufLen-1;
   if (retainValue)
   {
      if (arrayWasDynamicallyAllocated)
      {
         if (goToSmallBufferMode)
         {
            char * bigBuffer = _strData._bigBuffer; // guaranteed not to be equal to _strData._smallBuffer.  Gotta make this copy now because the memcpy() below will munge the pointer
            memcpy(_strData._smallBuffer, bigBuffer, newBufLen);  // copy the data back into our inline array
            _strData._smallBuffer[newMaxLength] = '\0';  // make sure we're terminated (could be an issue if we're shrinking)
            _length    = muscleMin(_length, newMaxLength);
            _bufferLen = sizeof(_strData._smallBuffer);
            muscleFree(bigBuffer);   // get rid of the dynamically allocated array we were using before
            return B_NO_ERROR;       // return now to avoid setting _strData._bigBuffer below
         }
         else
         {
            // Here we call muscleRealloc() to (hopefully) avoid unnecessary data copying
            newBuf = (char *) muscleRealloc(_strData._bigBuffer, newBufLen);
            MRETURN_OOM_ON_NULL(newBuf);
         }
      }
      else
      {
         if (goToSmallBufferMode)
         {
            // In the was-small-buffer, still-is-small-buffer case, all we need to do is truncate the string
            _strData._smallBuffer[newMaxLength] = '\0';
            _length = muscleMin(Length(), newMaxLength);
            // Setting _bufferLen isn't necessary here, since we are already in small-mode
            return B_NO_ERROR;       // return now to avoid setting _strData._bigBuffer below
         }
         else
         {
            // Oops, muscleRealloc() won't do in this case.... we'll just have to copy the bytes over
            newBuf = (char *) muscleAlloc(newBufLen);
            MRETURN_OOM_ON_NULL(newBuf);
            memcpy(newBuf, GetBuffer(), muscleMin(Length()+1, newBufLen));
         }
      }
   }
   else
   {
      // If the caller doesn't care about retaining this String's value, then it's
      // probably cheaper just to allocate a new buffer and free the old one.
      if (goToSmallBufferMode)
      {
         ClearAndFlush();    // might as well just dump everything
         return B_NO_ERROR;  // return now to avoid setting _strData._bigBuffer below
      }
      else
      {
         newBuf = (char *) muscleAlloc(newBufLen);
         MRETURN_OOM_ON_NULL(newBuf);
         newBuf[0] = '\0';  // avoid potential user-visible garbage bytes
         if (arrayWasDynamicallyAllocated) muscleFree(_strData._bigBuffer);
      }
   }

   newBuf[muscleMin(Length(), newMaxLength)] = '\0';   // ensure new char array is terminated (it might not be if allowShrink is true)
   _strData._bigBuffer = newBuf;
   _bufferLen          = newBufLen;

   return B_NO_ERROR;
}

String String :: PaddedBy(uint32 minLength, bool padOnRight, char padChar) const
{
   if (Length() < minLength)
   {
      const uint32 padLen = minLength-Length();
      String temp; temp += padChar;
      return (padOnRight) ? WithAppend(temp, padLen) : WithPrepend(temp, padLen);
   }
   else return *this;
}

String String :: IndentedBy(uint32 numIndentChars, char indentChar) const
{
   if ((numIndentChars == 0)||(indentChar == '\0')) return *this;

   const String pad = String().PaddedBy(numIndentChars);
   String ret;
   if ((StartsWith('\r'))||(StartsWith('\n'))) ret = pad;

   bool seenChars = false;
   const char * s = Cstr();
   while(*s)
   {
           if ((*s == '\n')||(*s == '\r')) seenChars = false;
      else if (seenChars == false)
      {
         ret += pad;
         seenChars = true;
      }
      ret += *s;
      s++;
   }
   return ret;
}

String String :: WithoutSuffix(char c, uint32 maxToRemove) const
{
   String ret = *this;
   while(ret.EndsWith(c))
   {
      ret--;
      if (--maxToRemove == 0) break;
   }
   return ret;
}

String String :: WithoutSuffix(const String & str, uint32 maxToRemove) const
{
   if (str.IsEmpty()) return *this;

   String ret = *this;
   while(ret.EndsWith(str))
   {
      ret.TruncateChars(str.Length());
      if (--maxToRemove == 0) break;
   }
   return ret;
}

String String :: WithoutPrefix(char c, uint32 maxToRemove) const
{
   uint32 numInitialChars = 0;
   const uint32 len = Length();
   for (uint32 i=0; i<len; i++)
   {
      if ((*this)[i] == c)
      {
         numInitialChars++;
         if (--maxToRemove == 0) break;
      }
      else break;
   }

   return Substring(numInitialChars);
}

String String :: WithoutPrefix(const String & str, uint32 maxToRemove) const
{
   if ((str.IsEmpty())||(StartsWith(str) == false)) return *this;

   String ret = *this;
   while(ret.StartsWith(str))
   {
      ret = ret.Substring(str.Length());
      if (--maxToRemove == 0) break;
   }
   return ret;
}

String String :: WithoutSuffixIgnoreCase(char c, uint32 maxToRemove) const
{
   if (EndsWithIgnoreCase(c) == false) return *this;

   String ret = *this;
   while(ret.EndsWithIgnoreCase(c))
   {
      ret--;
      if (--maxToRemove == 0) break;
   }
   return ret;
}

String String :: WithoutSuffixIgnoreCase(const String & str, uint32 maxToRemove) const
{
   if ((str.IsEmpty())||(EndsWithIgnoreCase(str) == false)) return *this;

   String ret = *this;
   while(ret.EndsWithIgnoreCase(str))
   {
      ret.TruncateChars(str.Length());
      if (--maxToRemove == 0) break;
   }
   return ret;
}

String String :: WithoutPrefixIgnoreCase(char c, uint32 maxToRemove) const
{
   const char cU = (char) toupper(c);
   const char cL = (char) tolower(c);

   String ret = *this;
   uint32 numInitialChars = 0;
   for (uint32 i=0; i<ret.Length(); i++)
   {
      const char n = (*this)[i];
      if ((n==cU)||(n==cL))
      {
         numInitialChars++;
         if (--maxToRemove == 0) break;
      }
      else break;
   }
   return Substring(numInitialChars);
}

String String :: WithoutPrefixIgnoreCase(const String & str, uint32 maxToRemove) const
{
   if ((str.IsEmpty())||(StartsWithIgnoreCase(str) == false)) return *this;

   String ret = *this;
   while(ret.StartsWithIgnoreCase(str))
   {
      ret = ret.Substring(str.Length());
      if (--maxToRemove == 0) break;
   }
   return ret;
}

#define ARG_IMPLEMENTATION   \
   char buf[256];            \
   muscleSprintf(buf, fmt, value); \
   return ArgAux(buf)

String String :: Arg(bool value, const char * fmt) const
{
   if (strstr(fmt, "%s")) return ArgAux(value?"true":"false");
                     else {ARG_IMPLEMENTATION;}
}

String String :: Arg(char value,               const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(unsigned char value,      const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(short value,              const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(unsigned short value,     const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(int value,                const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(unsigned int value,       const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(long value,               const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(unsigned long int value,  const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(long long value,          const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(unsigned long long value, const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(float value,              const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(double value,             const char * fmt) const {ARG_IMPLEMENTATION;}
String String :: Arg(const String & value)                       const {return ArgAux(value());}
String String :: Arg(const char * value)                         const {return ArgAux(value);}

String String :: Arg(double f, uint32 minDigitsAfterDecimal, uint32 maxDigitsAfterDecimal) const
{
   char buf[256];
   if (maxDigitsAfterDecimal == MUSCLE_NO_LIMIT) muscleSprintf(buf, "%f", f);
   else
   {
      char formatBuf[128];
      muscleSprintf(formatBuf, "%%.0" UINT32_FORMAT_SPEC "f", maxDigitsAfterDecimal);
      muscleSprintf(buf, formatBuf, f);
   }

   String s = buf;

   // Start by removing all trailing zeroes from the String
   if (s.Contains('.')) while(s.EndsWith('0')) s--;
   if (minDigitsAfterDecimal == 0)
   {
      if (s.EndsWith('.')) s--;  // Remove trailing . for integer-style display
   }
   else
   {
      // Pad with 0's if necessary to meet the minDigitsAfterDecimal requirement
      int32 dotIdx = s.LastIndexOf('.');
      if (dotIdx < 0) {s += '.'; dotIdx = s.Length()-1;}  // semi-paranoia

      const uint32 numDigitsPresent = (s.Length()-dotIdx)-1;
      for(uint32 i=numDigitsPresent; i<minDigitsAfterDecimal; i++) s += '0';
   }
   return ArgAux(s());
}

String String :: Arg(const Point & value, const char * fmt) const
{
   char buf[512];
   muscleSprintf(buf, fmt, value.x(), value.y());
   return ArgAux(buf);
}

String String :: Arg(const Rect & value, const char * fmt) const
{
   char buf[512];
   muscleSprintf(buf, fmt, value.left(), value.top(), value.right(), value.bottom());
   return ArgAux(buf);
}

String String :: Arg(const void * value) const
{
   char buf[128];
   muscleSprintf(buf, "%p", value);
   return ArgAux(buf);
}

String String :: ArgAux(const char * buf) const
{
   TCHECKPOINT;

   int32 lowestArg = -1;
   const char * s = Cstr();
   while(*s != '\0')
   {
      if (*s == '%')
      {
         s++;
         if (muscleInRange(*s, '0', '9'))
         {
            const int32 val = (int32) atol(s);
            lowestArg = (lowestArg < 0) ? val : muscleMin(val, lowestArg);
            while(muscleInRange(*s, '0', '9')) s++;
         }
      }
      else s++;
   }

   if (lowestArg >= 0)
   {
      char token[64];
      muscleSprintf(token, "%%" INT32_FORMAT_SPEC, lowestArg);
      String ret(*this);
      (void) ret.Replace(token, buf);
      return ret;
   }
   else return *this;
}

uint32 String :: ParseNumericSuffix(uint32 defaultValue) const
{
   const char * s = Cstr();
   const char * n = s+Length();  // initially points to the NUL terminator
   while((n>s)&&(muscleInRange(*(n-1), '0', '9'))) n--;  // move back to first char of numeric suffix
   return muscleInRange(*n, '0', '9') ? (uint32) atol(n) : defaultValue;
}

String String :: WithoutNumericSuffix(uint32 * optRemovedSuffixValue) const
{
   String ret(*this);
   String suffix;
   while(ret.HasChars())
   {
      const char c = ret[ret.Length()-1];
      if (muscleInRange(c, '0', '9'))
      {
         suffix += c;
         ret--;
      }
      else break;
   }
   if (optRemovedSuffixValue)
   {
      suffix.Reverse();
      *optRemovedSuffixValue = (uint32) atol(suffix());
   }
   return ret;
}

// Stolen from:  https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C.2B.2B
static uint32 GetLevenshteinDistanceAux(const char *shortString, uint32 shortStringLen, const char * longString, uint32 longStringLen, uint32 maxResult)
{
#ifdef __clang_analyzer__
   assert(shortStringLen<=longStringLen);
#endif

   const uint32 allocLen = shortStringLen+1;
#ifdef __clang_analyzer__
   uint32 * columns = newnothrow_array(uint32,allocLen);  // ClangSA doesn't like my tempBuf trick
#else
   uint32 tempBuf[256];  // We'll try to avoid using the heap for small strings
   uint32 * columns = (allocLen > ARRAYITEMS(tempBuf)) ? newnothrow_array(uint32,allocLen) : tempBuf;
#endif
   if (columns == NULL)
   {
      MWARN_OUT_OF_MEMORY;
      return maxResult;
   }

   for (uint32 x=0; x<allocLen; x++) columns[x] = x;
   for (uint32 x=1; x<=longStringLen; x++)
   {
      columns[0] = x;
      for (uint32 y=1, lastdiag=(x-1); y<=shortStringLen; y++)
      {
         const uint32 olddiag = columns[y];
         columns[y] = muscleMin(columns[y]+1, columns[y-1]+1, lastdiag+((shortString[y-1]==longString[x-1])?0:1));
         lastdiag = olddiag;
      }
      if (columns[shortStringLen] >= maxResult) break;
   }

   const uint32 ret = columns[shortStringLen];
#ifdef __clang_analyzer__
   delete [] columns;  // ClangSA doesn't like my tempBuf trick
#else
   if (allocLen > ARRAYITEMS(tempBuf)) delete [] columns;
#endif
   return muscleMin(ret, maxResult);
}

static uint32 GetLevenshteinDistance(const char *s1, uint32 s1len, const char *s2, uint32 s2len, uint32 maxResult)
{
   // Minimize the amount of heap memory required, by swapping the arguments if (s2) is shorter than (s1).
   // Since the function is commutative, this won't affect the resulting values
   return (s2len < s1len)
        ? GetLevenshteinDistanceAux(s2, s2len, s1, s1len, maxResult)
        : GetLevenshteinDistanceAux(s1, s1len, s2, s2len, maxResult);
}

uint32 String :: GetDistanceTo(const String & otherString, uint32 maxResult) const
{
   return GetLevenshteinDistance(Cstr(), Length(), otherString(), otherString.Length(), maxResult);
}

uint32 String :: GetDistanceTo(const char * otherString, uint32 maxResult) const
{
   return GetLevenshteinDistance(Cstr(), Length(), otherString?otherString:"", otherString?(uint32)strlen(otherString):(uint32)0, maxResult);
}

#ifdef __APPLE__
// Congratulations to Apple for making a seemingly trivial operation as painful as it could possibly be
status_t String :: SetFromCFStringRef(const CFStringRef & cfStringRef)
{
   status_t ret = B_NO_ERROR;
   if (cfStringRef != NULL)
   {
      char tempBuf[256]; // try to avoid a call to muscleAlloc() in most cases
      const uint32 allocLen = ((uint32)CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfStringRef), kCFStringEncodingUTF8))+1;
      const bool doAlloc    = (allocLen > sizeof(tempBuf));

      char * str = doAlloc ? (char *)muscleAlloc(allocLen) : tempBuf;
      MRETURN_OOM_ON_NULL(str);

      ret = CFStringGetCString(cfStringRef, str, allocLen, kCFStringEncodingUTF8) ? SetCstr(str) : B_ERROR("CFStringGetCString() failed");
      if (doAlloc) muscleFree(str);
   }
   else Clear();

   return ret;
}

CFStringRef String :: ToCFStringRef() const
{
   return CFStringCreateWithCString(NULL, Cstr(), kCFStringEncodingUTF8);
}
#endif

/* strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
   Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.   (JAF-note:  I have altered
      this software slightly -- it is not the same as the original code!)
   3. This notice may not be removed or altered from any source distribution.
*/

/* These are defined as macros to make it easier to adapt this code to
 * different characters types or comparison functions. */
static inline int  nat_isdigit(char a) {return isdigit((unsigned char) a);}
static inline int  nat_isspace(char a) {return isspace((unsigned char) a);}
static inline char nat_toupper(char a) {return (char) toupper((unsigned char) a);}

static int nat_compare_right(char const *a, char const *b)
{
   int bias = 0;

   /* The longest run of digits wins.  That aside, the greatest
      value wins, but we can't know that it will until we've scanned
      both numbers to know that they have the same magnitude, so we
      remember it in BIAS. */
   for (;; a++, b++)
   {
           if (!nat_isdigit(*a)  &&  !nat_isdigit(*b)) return bias;
      else if (!nat_isdigit(*a)) return -1;
      else if (!nat_isdigit(*b)) return +1;
      else if (*a < *b)
      {
         if (!bias) bias = -1;
      }
      else if (*a > *b)
      {
          if (!bias) bias = +1;
      }
      else if ((!*a)&&(!*b)) return bias;
   }
   return 0;
}

static int nat_compare_left(char const *a, char const *b)
{
   /* Compare two left-aligned numbers: the first to have a different value wins. */
   while(1)
   {
          if ((!nat_isdigit(*a))&&(!nat_isdigit(*b))) return 0;
     else if (!nat_isdigit(*a)) return -1;
     else if (!nat_isdigit(*b)) return +1;
     else if (*a < *b)          return -1;
     else if (*a > *b)          return +1;
     ++a; ++b;
   }
   return 0;
}

static int strnatcmp0(char const *a, char const *b, int fold_case)
{
   int ai = 0;
   int bi = 0;
   while(1)
   {
      char ca = a[ai];
      char cb = b[bi];

      /* skip over leading spaces or zeros */
      while(nat_isspace(ca)) ca = a[++ai];
      while(nat_isspace(cb)) cb = b[++bi];

      /* process run of digits */
      if ((nat_isdigit(ca))&&(nat_isdigit(cb)))
      {
         const int fractional = (ca == '0' || cb == '0');
         if (fractional)
         {
            const int result = nat_compare_left(a+ai, b+bi);
            if (result != 0) return result;
         }
         else
         {
            const int result = nat_compare_right(a+ai, b+bi);
            if (result != 0) return result;
         }
      }

      /* The strings compare the same.  Call strcmp() to break the tie. */
      if (!ca && !cb) return strcmp(a,b);

      if (fold_case)
      {
         ca = nat_toupper(ca);
         cb = nat_toupper(cb);
      }

           if (ca < cb) return -1;
      else if (ca > cb) return +1;

      ++ai; ++bi;
   }
}

static int strnatcmp(char const *a, char const *b)     {return strnatcmp0(a, b, 0);}
static int strnatcasecmp(char const *a, char const *b) {return strnatcmp0(a, b, 1);}

int NumericAwareStrcmp(const char * s1, const char * s2)     {return strnatcmp(    s1, s2);}
int NumericAwareStrcasecmp(const char * s1, const char * s2) {return strnatcasecmp(s1, s2);}

} // end namespace muscle
