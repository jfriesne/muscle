/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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

SegmentedStringMatcher::SegmentedStringMatcher() : _negate(false)
{
   // empty
} 

SegmentedStringMatcher :: SegmentedStringMatcher(const String & str, bool simple, const char * sc, uint32 maxSegments)
   : _negate(false)
{
   (void) SetPattern(str, simple, sc, maxSegments);
}

SegmentedStringMatcher :: ~SegmentedStringMatcher()
{
   // empty
}

void SegmentedStringMatcher :: Clear()
{
   _negate = false;
   _pattern.Clear();   
   _sepChars.Clear();   
   _segments.Clear();
}

status_t SegmentedStringMatcher::SetPattern(const String & s, bool isSimple, const char * sc, uint32 maxSegments) 
{
   Clear();

   status_t ret;
   StringTokenizer tok(s(), sc);
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
   _sepChars = sc;
   return B_NO_ERROR;
}

bool SegmentedStringMatcher::MatchAux(const char * const str) const
{
   StringTokenizer tok(str, _sepChars());
   for (uint32 i=0; i<_segments.GetNumItems(); i++) 
   {
      const char * t = tok();
      if (t == NULL) return false;

      const StringMatcher * sm = _segments[i]();
      if ((sm)&&(sm->Match(t) == false)) return false;
   }
   return true;
}

String SegmentedStringMatcher :: ToString() const
{
   String ret;
   for (uint32 i=0; i<_segments.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += '/'; 
      const StringMatcher * sm = _segments[i]();
      ret += sm ? sm->ToString() : "*";
   }
   return ret; 
}

} // end namespace muscle
