/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>
#include "regex/StringMatcher.h"
#include "util/String.h"
#include "util/StringTokenizer.h"

namespace muscle {

static StringMatcherRef::ItemPool _stringMatcherPool;
StringMatcherRef::ItemPool * GetStringMatcherPool() {return &_stringMatcherPool;}

StringMatcherRef GetStringMatcherFromPool() {return StringMatcherRef(_stringMatcherPool.ObtainObject());}

StringMatcherRef GetStringMatcherFromPool(const String & matchString, bool isSimpleFormat)
{
   StringMatcherRef ret(_stringMatcherPool.ObtainObject());
   if ((ret())&&(ret()->SetPattern(matchString, isSimpleFormat).IsError())) ret.Reset();
   return ret;
}

void StringMatcher :: Reset()
{
   if (_flags.IsBitSet(STRINGMATCHER_FLAG_REGEXVALID)) regfree(&_regExp);
   _flags.ClearAllBits();
   _ranges.Clear();
   _pattern.Clear();
}

StringMatcher & StringMatcher :: operator = (const StringMatcher & rhs)
{
   (void) SetPattern(rhs._pattern, rhs._flags.IsBitSet(STRINGMATCHER_FLAG_SIMPLE));
   return *this;
}

// Returns a String containing only the numeric digits in (s)
static String DigitsOnly(const String & s)
{
   String ret;
   for (uint32 i=0; i<s.Length(); i++) if (muscleInRange(s[i], '0', '9')) ret += s[i];
   return ret;
}

status_t StringMatcher :: SetPattern(const String & s, bool isSimple) 
{
   TCHECKPOINT;

   _pattern = s;
   _flags.SetBit(STRINGMATCHER_FLAG_SIMPLE, isSimple);

   bool onlyWildcardCharsAreCommas = false;
   _flags.SetBit(STRINGMATCHER_FLAG_CANMATCHMULTIPLEVALUES, isSimple?CanWildcardStringMatchMultipleValues(_pattern, &onlyWildcardCharsAreCommas):HasRegexTokens(_pattern));

   const char * str = _pattern();
   String regexPattern;
   _ranges.Clear();
   if (isSimple)
   {
      // Special case:  if the first char is a tilde, ignore it, but set the negate-bit.
      if (str[0] == '~')
      {
         _flags.SetBit(STRINGMATCHER_FLAG_NEGATE);
         str++;
      }
      else _flags.ClearBit(STRINGMATCHER_FLAG_NEGATE);

      // Special case as of MUSCLE v6.12:  if the first char is a backtick, ignore it, but parse the remaining string as a regex instead (for Mika)
      if (str[0] == '`') str++;  // note that I deliberately do not clear the STRINGMATCHER_FLAG_SIMPLE bit, since the backtick-prefix is not part of the official regex spec
      else
      {
         // Special case for strings of form e.g. "<15-23>", which is interpreted to
         // match integers in the range 15-23, inclusive.  Yeah, it's an ungraceful
         // hack, but also quite useful since regex won't do that in general.
         if (str[0] == '<')
         {
            const char * rBracket = strchr(str+1, '>');
            if ((rBracket)&&(*(rBracket+1)=='\0'))   // the right-bracket must be the last char in the string!
            {
               StringTokenizer clauses(&str[1], ",", NULL);
               const char * clause;
               while((clause=clauses()) != NULL)
               {
                  uint32 min = 0, max = MUSCLE_NO_LIMIT;  // defaults to be used in case one side of the dash is missing (e.g. "-36" means <=36, or "36-" means >=36)

                  const char * dash = strchr(clause, '-');
                  if (dash)
                  {
                     String beforeDash;
                     if (dash>clause) {beforeDash.SetCstr(clause, (int32)(dash-clause)); beforeDash = DigitsOnly(beforeDash);}

                     String afterDash(dash+1); afterDash = DigitsOnly(afterDash);

                     if (beforeDash.HasChars()) min = atoi(beforeDash());
                     if (afterDash.HasChars())  max = atoi(afterDash());
                  }
                  else if (clause[0] != '>') min = max = atoi(String(clause).Trim()());
                  
                  _ranges.AddTail(IDRange(min,max));
               }
            }
         }

         if (_ranges.IsEmpty())
         {
            if ((str[0] == '\\')&&(str[1] == '<')) str++;  // special case escape of initial < for "\<15-23>"

            regexPattern = "^(";

            bool escapeMode = false;
            for (const char * ptr = str; *ptr != '\0'; ptr++)
            {
               char c = *ptr;

               if (escapeMode) escapeMode = false;
               else
               {
                  switch(c)
                  {
                     case ',':  c = '|';              break;  // commas are treated as union-bars
                     case '.':  regexPattern += '\\'; break;  // dots are considered literals, so escape those
                     case '+':  regexPattern += '\\'; break;  // pluses are considered literals, so escape those
                     case '*':  regexPattern += '.';  break;  // hmmm.
                     case '?':  c = '.';              break;  // question marks mean any-single-char
                     case '\\': escapeMode = true;    break;  // don't transform the next character!
                  }
               }
               regexPattern += c;
            }
            if (escapeMode) regexPattern += '\\';  // just in case the user left a trailing backslash
            regexPattern += ")$";
         }
      }
   }
   else _flags.ClearBit(STRINGMATCHER_FLAG_NEGATE);

   // Free the old regular expression, if any
   if (_flags.IsBitSet(STRINGMATCHER_FLAG_REGEXVALID))
   {
      regfree(&_regExp);
      _flags.ClearBit(STRINGMATCHER_FLAG_REGEXVALID);
   }

   _flags.SetBit(STRINGMATCHER_FLAG_UVLIST, (onlyWildcardCharsAreCommas)&&(_ranges.IsEmpty())&&(_flags.IsBitSet(STRINGMATCHER_FLAG_NEGATE) == false));

   // And compile the new one
   if (_ranges.IsEmpty())
   {
      status_t ret;
      const int rc = regcomp(&_regExp, regexPattern.HasChars() ? regexPattern() : str, REG_EXTENDED);
           if (rc == REG_ESPACE) {ret = B_OUT_OF_MEMORY; MWARN_OUT_OF_MEMORY;}
      else if (rc != 0)           ret = B_BAD_ARGUMENT;  // we'll assume other return-values from regcomp() all indicate a parse-failure

      _flags.SetBit(STRINGMATCHER_FLAG_REGEXVALID, ret.IsOK());
      return ret;
   }
   else return B_NO_ERROR;  // for range queries, we don't need a valid regex
}

bool StringMatcher :: Match(const char * const str) const
{
   TCHECKPOINT;

   bool ret = false;  // pessimistic default

   if (_ranges.IsEmpty())
   {
      if (_flags.IsBitSet(STRINGMATCHER_FLAG_REGEXVALID)) ret = (regexec(&_regExp, str, 0, NULL, 0) != REG_NOMATCH);
   }
   else if (muscleInRange(str[0], '0', '9'))
   {
      const uint32 id = (uint32) atoi(str);
      for (uint32 i=0; i<_ranges.GetNumItems(); i++) 
      {
         const IDRange & r = _ranges[i];
         if (muscleInRange(id, r.GetMin(), r.GetMax())) {ret = true; break;}
      }
   }

   return _flags.IsBitSet(STRINGMATCHER_FLAG_NEGATE) ? (!ret) : ret;
}

String StringMatcher :: ToString() const
{
   String s;
   if (_flags.IsBitSet(STRINGMATCHER_FLAG_NEGATE)) s = "~";

   if (_ranges.IsEmpty()) return s+_pattern;
   else
   {
      s += '<';
      for (uint32 i=0; i<_ranges.GetNumItems(); i++) 
      {
         if (i > 0) s += ',';
         const IDRange & r = _ranges[i];
         const uint32 min  = r.GetMin();
         const uint32 max  = r.GetMax();
         char buf[128];
         if (max > min) muscleSprintf(buf, UINT32_FORMAT_SPEC "-" UINT32_FORMAT_SPEC, min, max);
                   else muscleSprintf(buf, UINT32_FORMAT_SPEC, min);
         s += buf;
      }
      s += '>';
      return s;
   }
}

void StringMatcher :: SwapContents(StringMatcher & withMe)
{
   muscleSwap(_flags,   withMe._flags);
   muscleSwap(_pattern, withMe._pattern);
   muscleSwap(_regExp,  withMe._regExp);
   muscleSwap(_ranges,  withMe._ranges);
}

bool IsRegexToken(char c, bool isFirstCharInString)
{
   switch(c)
   {
      // muscle 2.50:  fixed to match exactly the chars specified in muscle/regex/regex/regcomp.c
      case '[': case ']': case '*': case '?': case '\\': case ',': case '|': case '(': case ')':
      case '=': case '^': case '+': case '$': case '{':  case '}': case '-':  // note:  deliberately not including ':'
        return true;

      case '<': case '~':   // these chars are only special if they are the first character in the string
         return isFirstCharInString; 
 
      default:
         return false;
   }
}

String EscapeRegexTokens(const String & s, const char * optTokens)
{
   TCHECKPOINT;

   const char * str = s.Cstr();

   String ret;
   bool isFirst = true;
   while(*str)
   {
     if (optTokens ? (strchr(optTokens, *str) != NULL) : IsRegexToken(*str, isFirst)) ret += '\\';
     isFirst = false;
     ret += *str;
     str++;
   }
   return ret;
}

String RemoveEscapeChars(const String & s)
{
   const uint32 len = s.Length();
   String ret; (void) ret.Prealloc(len);
   bool lastWasEscape = false;
   for (uint32 i=0; i<len; i++)
   {
      const char c = s[i];
      const bool isEscape = (c == '\\');
      if ((lastWasEscape)||(isEscape == false)) ret += c;
      lastWasEscape = ((isEscape)&&(lastWasEscape == false));
   }
   return ret;
}

bool HasRegexTokens(const char * str)
{
   bool isFirst = true;
   while(*str)
   {
      if (IsRegexToken(*str, isFirst)) return true;
      else 
      {
         str++;
         isFirst = false;
      }
   }
   return false;
}

bool CanWildcardStringMatchMultipleValues(const char * str, bool * optRetOnlySpecialCharIsCommas)
{
   if (optRetOnlySpecialCharIsCommas) *optRetOnlySpecialCharIsCommas = false;  // default
   if (str[0] == '`') return true;  // Backtick prefix means what follows is a regex, so we'll assume it can match multiple items

   bool sawComma          = false;
   bool prevCharWasEscape = false;
   const char * s         = str;
   while(*s)
   {
      const bool isEscape = ((*s == '\\')&&(prevCharWasEscape == false));
      if ((isEscape == false)&&(*s != '-')&&(prevCharWasEscape == false)&&(IsRegexToken(*s, (s==str))))
      {
         if ((*s == ',')&&(optRetOnlySpecialCharIsCommas != NULL)) sawComma = true;
                                                              else return true;
      }
      prevCharWasEscape = isEscape;
      s++;
   }

   if (optRetOnlySpecialCharIsCommas) *optRetOnlySpecialCharIsCommas = sawComma;  // if we got here there were no other regexChars
   return sawComma;
}

bool MakeRegexCaseInsensitive(String & str)
{
   TCHECKPOINT;

   bool changed = false;
   String ret;
   for (uint32 i=0; i<str.Length(); i++)
   {
     const char next = str[i];
     if ((next >= 'A')&&(next <= 'Z'))
     {
        char buf[5];
        muscleSprintf(buf, "[%c%c]", next, next+('a'-'A'));
        ret += buf;
        changed = true;
     }
     else if ((next >= 'a')&&(next <= 'z'))
     {
        char buf[5];
        muscleSprintf(buf, "[%c%c]", next, next+('A'-'a'));
        ret += buf;
        changed = true;
     }
     else ret += next;
   }
   if (changed) str = ret;
   return changed;
}

} // end namespace muscle
