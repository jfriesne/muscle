/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>
#include "regex/SegmentedStringMatcher.h"
#include "util/StringTokenizer.h"

namespace muscle {

static SegmentedStringMatcherRef::ItemPool _segmentedStringMatcherPool;
SegmentedStringMatcherRef::ItemPool * GetSegmentedStringMatcherPool() {return &_segmentedStringMatcherPool;}

SegmentedStringMatcherRef GetSegmentedStringMatcherFromPool() {return SegmentedStringMatcherRef(_segmentedStringMatcherPool.ObtainObject());}
SegmentedStringMatcherRef GetSegmentedStringMatcherFromPool(const String & matchString, bool isSimpleFormat, const char * sc, uint32 maxSegments)
{
   SegmentedStringMatcherRef ret(_segmentedStringMatcherPool.ObtainObject());
   if ((ret())&&(ret()->SetPattern(matchString, isSimpleFormat, sc, maxSegments).IsError())) ret.Reset();
   return ret;
}

SegmentedStringMatcher :: SegmentedStringMatcher() : _negate(false)
{
   // empty
}

SegmentedStringMatcher :: SegmentedStringMatcher(const String & str, bool simple, const char * sc, uint32 maxSegments)
   : _negate(false)
{
   (void) SetPattern(str, simple, sc, maxSegments);
}

void SegmentedStringMatcher :: Clear()
{
   _negate = false;
   _pattern.Clear();
   _sepChars.Clear();
   _segments.Clear();
}

status_t SegmentedStringMatcher :: SetPattern(const String & s, bool isSimple, const char * sc, uint32 maxSegments)
{
   if ((isSimple)&&(s.StartsWith("~")))
   {
      MRETURN_ON_ERROR(SetPatternAux(s.Substring(1), isSimple, sc, maxSegments));
      SetNegate(true);
      return B_NO_ERROR;
   }
   else return SetPatternAux(s, isSimple, sc, maxSegments);
}

status_t SegmentedStringMatcher :: SetPatternAux(const String & s, bool isSimple, const char * optSepChars, uint32 maxSegments)
{
   Clear();

   status_t ret;
   const char * sepChars = optSepChars ? optSepChars : "/";
   StringTokenizer tok(s(), sepChars);
   const char * t;
   while((t = tok()) != NULL)
   {
      if (_segments.GetNumItems() == maxSegments) break;
      if ((isSimple)&&(strcmp(t, "*") == 0))
      {
         if (_segments.AddTail(StringMatcherRef()).IsError(ret)) {Clear(); return ret;}
      }
      else
      {
         StringMatcherRef subMatcherRef = GetStringMatcherFromPool(t, isSimple);
         if (subMatcherRef() == NULL) {Clear(); MRETURN_OUT_OF_MEMORY;}
         if (_segments.AddTail(subMatcherRef).IsError(ret)) {Clear(); return ret;}
      }
   }
   _pattern  = s;
   _sepChars = sepChars;
   return B_NO_ERROR;
}

bool SegmentedStringMatcher::MatchAux(const char * const str, bool prefixMatchOkay) const
{
   StringTokenizer tok(str, _sepChars());
   for (uint32 i=0; i<_segments.GetNumItems(); i++)
   {
      const char * t = tok();
      if (t == NULL) return false;

      const StringMatcher * sm = _segments[i]();
      if ((sm)&&(sm->Match(t) == false)) return false;
   }
   return prefixMatchOkay ? true : (tok()==NULL);
}

String SegmentedStringMatcher :: ToString() const
{
   String ret;
   if (_negate) ret += '~';

   for (uint32 i=0; i<_segments.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += (_sepChars.HasChars() ? _sepChars[0] : '/');
      const StringMatcher * sm = _segments[i]();
      ret += sm ? sm->ToString() : "*";
   }
   return ret;
}

bool SegmentedStringMatcher :: IsPatternUnique() const
{
   if ((_negate)||(_segments.IsEmpty())) return false; // not-some-pattern can match a number of strings; empty pattern matches all strings

   for (uint32 i=0; i<_segments.GetNumItems(); i++)
   {
      const StringMatcher * sm = _segments[i]();
      if ((sm == NULL)||(sm->IsPatternUnique() == false)) return false;
   }
   return true;
}

} // end namespace muscle
