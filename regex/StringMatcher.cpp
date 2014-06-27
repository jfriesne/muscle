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
   if ((ret())&&(ret()->SetPattern(matchString, isSimpleFormat) != B_NO_ERROR)) ret.Reset();
   return ret;
}

StringMatcher::StringMatcher() : _bits(0)
{
   // empty
} 

StringMatcher :: StringMatcher(const String & str, bool simple) : _bits(0)
{
   (void) SetPattern(str, simple);
}

StringMatcher :: StringMatcher(const StringMatcher & rhs) : RefCountable(rhs), _bits(0)
{
   *this = rhs;
}

StringMatcher :: ~StringMatcher()
{
   Reset();
}

void StringMatcher :: Reset()
{
   if (IsBitSet(STRINGMATCHER_BIT_REGEXVALID)) regfree(&_regExp);
   _bits = 0;
   _ranges.Clear();
   _pattern.Clear();
}

StringMatcher & StringMatcher :: operator = (const StringMatcher & rhs)
{
   (void) SetPattern(rhs._pattern, rhs.IsBitSet(STRINGMATCHER_BIT_SIMPLE));
   return *this;
}

status_t StringMatcher :: SetPattern(const String & s, bool isSimple) 
{
   TCHECKPOINT;

   _pattern = s;
   SetBit(STRINGMATCHER_BIT_SIMPLE, isSimple);

   SetBit(STRINGMATCHER_BIT_CANMATCHMULTIPLEVALUES, isSimple?CanWildcardStringMatchMultipleValues(_pattern):HasRegexTokens(_pattern));

   const char * str = _pattern();
   String regexPattern;
   _ranges.Clear();
   if (isSimple)
   {
      // Special case:  if the first char is a tilde, ignore it, but set the negate-bit.
      if (str[0] == '~')
      {
         SetBit(STRINGMATCHER_BIT_NEGATE, true);
         str++;
      }
      else SetBit(STRINGMATCHER_BIT_NEGATE, false);

      // Special case for strings of form e.g. "<15-23>", which is interpreted to
      // match integers in the range 15-23, inclusive.  Yeah, it's an ungraceful
      // hack, but also quite useful since regex won't do that in general.
      if (str[0] == '<')
      {
         const char * rBracket = strchr(str+1, '>');
         if ((rBracket)&&(*(rBracket+1)=='\0'))   // the right-bracket must be the last char in the string!
         {
            StringTokenizer clauses(&str[1], ",");
            const char * clause;
            while((clause=clauses()) != NULL)
            {
               uint32 min = 0, max = MUSCLE_NO_LIMIT;  // defaults to be used in case one side of the dash is missing (e.g. "-36" means <=36, or "36-" means >=36)

               const char * dash = strchr(clause, '-');
               if (dash)
               {
                  String beforeDash;
                  if (dash>clause) {beforeDash.SetCstr(clause, dash-clause); beforeDash = beforeDash.Trim();}

                  String afterDash(dash+1); afterDash = afterDash.Trim();

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
   else SetBit(STRINGMATCHER_BIT_NEGATE, false);

   // Free the old regular expression, if any
   if (IsBitSet(STRINGMATCHER_BIT_REGEXVALID))
   {
      regfree(&_regExp);
      SetBit(STRINGMATCHER_BIT_REGEXVALID, false);
   }

   // And compile the new one
   if (_ranges.IsEmpty())
   {
      bool isValid = (regcomp(&_regExp, regexPattern.HasChars() ? regexPattern() : str, REG_EXTENDED) == 0);
      SetBit(STRINGMATCHER_BIT_REGEXVALID, isValid);
      return isValid ? B_NO_ERROR : B_ERROR;
   }
   else return B_NO_ERROR;  // for range queries, we don't need a valid regex
}

bool StringMatcher :: Match(const char * const str) const
{
   TCHECKPOINT;

   bool ret = false;  // pessimistic default

   if (_ranges.IsEmpty())
   {
      if (IsBitSet(STRINGMATCHER_BIT_REGEXVALID)) ret = (regexec(&_regExp, str, 0, NULL, 0) != REG_NOMATCH);
   }
   else if (muscleInRange(str[0], '0', '9'))
   {
      uint32 id = (uint32) atoi(str);
      for (uint32 i=0; i<_ranges.GetNumItems(); i++) 
      {
         const IDRange & r = _ranges[i];
         if (muscleInRange(id, r.GetMin(), r.GetMax())) {ret = true; break;}
      }
   }

   return IsBitSet(STRINGMATCHER_BIT_NEGATE) ? (!ret) : ret;
}

String StringMatcher :: ToString() const
{
   String s;
   if (IsBitSet(STRINGMATCHER_BIT_NEGATE)) s = "~";

   if (_ranges.IsEmpty()) return s+_pattern;
   else
   {
      s += '<';
      for (uint32 i=0; i<_ranges.GetNumItems(); i++) 
      {
         if (i > 0) s += ',';
         const IDRange & r = _ranges[i];
         uint32 min = r.GetMin();
         uint32 max = r.GetMax();
         char buf[128];
         if (max > min) sprintf(buf, UINT32_FORMAT_SPEC "-" UINT32_FORMAT_SPEC, min, max);
                   else sprintf(buf, UINT32_FORMAT_SPEC, min);
         s += buf;
      }
      s += '>';
      return s;
   }
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
   uint32 len = s.Length();
   String ret; (void) ret.Prealloc(len);
   bool lastWasEscape = false;
   for (uint32 i=0; i<len; i++)
   {
      char c = s[i];
      bool isEscape = (c == '\\');
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

bool CanWildcardStringMatchMultipleValues(const char * str)
{
   bool prevCharWasEscape = false;
   const char * s = str;
   while(*s)
   {
      bool isEscape = ((*s == '\\')&&(prevCharWasEscape == false));
      if ((isEscape == false)&&(*s != '-')&&(prevCharWasEscape == false)&&(IsRegexToken(*s, (s==str)))) return true;
      prevCharWasEscape = isEscape;
      s++;
   }
   return false;
}


bool MakeRegexCaseInsensitive(String & str)
{
   TCHECKPOINT;

   bool changed = false;
   String ret;
   for (uint32 i=0; i<str.Length(); i++)
   {
     char next = str[i];
     if ((next >= 'A')&&(next <= 'Z'))
     {
        char buf[5];
        sprintf(buf, "[%c%c]", next, next+('a'-'A'));
        ret += buf;
        changed = true;
     }
     else if ((next >= 'a')&&(next <= 'z'))
     {
        char buf[5];
        sprintf(buf, "[%c%c]", next, next+('A'-'a'));
        ret += buf;
        changed = true;
     }
     else ret += next;
   }
   if (changed) str = ret;
   return changed;
}

}; // end namespace muscle
