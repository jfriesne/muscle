/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSegmentedStringMatcher_h
#define MuscleSegmentedStringMatcher_h

#include "regex/StringMatcher.h"

namespace muscle {

/** Similar to a StringMatcher, but this version segments both the wild card expression and
  * the paths to be matched against into sections.  For example, if the wild card expression
  * is "*foo/bar*" and the string to be matched against is "foot/ball", the SegmentedStringMatcher
  * will try to match "foo*" against "foot" and then "bar*" against "ball", instead of trying
  * to match "*foo/bar*" against "foot/ball".  This can be useful when you want to operate
  * on segmented paths with level-by-level matching semantics.
  */
class SegmentedStringMatcher MUSCLE_FINAL_CLASS : public RefCountable
{
public:
   /** Default Constructor. */
   SegmentedStringMatcher();

   /** A constructor that sets the given expression.  See SetPattern() for argument semantics.  */
   SegmentedStringMatcher(const String & matchString, bool isSimpleFormat = true, const char * segmentSeparatorChars = "/");

   /** Destructor */
   ~SegmentedStringMatcher();

   /**
    * Set a new wildcard pattern or regular expression for this SegmentedStringMatcher to use in future Match() calls.
    * @param expression The new globbing pattern or regular expression to match with.  It may be segmented using any of the
    *                   separator characters specified in our constructor.
    * @param isSimpleFormat If you wish to use the formal regex syntax, instead of the simple syntax, set isSimpleFormat to false.
    * @param segmentSeparatorChars The set of characters that denote sub-keys within the key string.  This string will be passed to
    *                              our StringTokenizer.  Defaults to "/".
    * @return B_NO_ERROR on success, B_ERROR on error (e.g. expression wasn't parsable, or out of memory)
    */
   status_t SetPattern(const String & expression, bool isSimpleFormat=true, const char * segmentSeparatorChars = "/");

   /** Returns the pattern String as it was previously passed in to SetPattern() */
   const String & GetPattern() const {return _pattern;}

   /** Returns true iff this SegmentedStringMatcher's pattern specifies exactly one possible string.
    *  (i.e. the pattern is just plain old text, with no wildcards or other pattern matching logic specified)
    */
   bool IsPatternUnique() const;

   /** Returns this SegmentedStringMatcher's separator chars, as passed in to the ctor or SetPattern(). */
   const String & GetSeparatorChars() const {return _sepChars;}

   /** Returns true iff (string) is matched by the current expression.
    * @param matchString a string to match against using our current expression.
    * @return true iff (matchString) matches, false otherwise.
    */
   bool Match(const char * const matchString) const {return _negate ? !MatchAux(matchString) : MatchAux(matchString);}

   /** Convenience method:  Same as above, but takes a String object instead of a (const char *). */
   inline bool Match(const String & matchString) const {return Match(matchString());}

   /** If set true, Match() will return the logical opposite of what
     * it would otherwise return; e.g. it will return true only when
     * the given string doesn't match the pattern.
     * Default state is false.  Note that this flag is also set by
     * SetPattern(..., true), based on whether or not the pattern
     * string starts with a tilde.
     */
   void SetNegate(bool negate) {_negate = negate;}

   /** Returns the current state of our negate flag. */
   bool IsNegate() const {return _negate;}

   /** Clears this SegmentedStringMatcher to its default state. */
   void Clear();

   /** Returns a human-readable string representing this StringMatcher, for debugging purposes. */
   String ToString() const;

private:
   bool MatchAux(const char * const matchString) const;

   String _pattern;
   String _sepChars;
   bool _negate;
   Queue<StringMatcherRef> _segments;
};
DECLARE_REFTYPES(SegmentedStringMatcher);

SegmentedStringMatcherRef::ItemPool * GetSegmentedStringMatcherPool();

/** Convenience method.  Returns a StringMatcher object from the default StringMatcher pool,
  * or a NULL reference on failure (out of memory?)
  */
SegmentedStringMatcherRef GetSegmentedStringMatcherFromPool();

/** Convenience method.  Obtains a StringMatcher object from the default StringMatcher pool, calls SetPattern() on it
  * with the given arguments, and returns it, or a NULL reference on failure (out of memory, or a parse error?)
  */
SegmentedStringMatcherRef GetSegmentedStringMatcherFromPool(const String & matchString, bool isSimpleFormat = true, const char * segmentSeparatorChars = "/");

}; // end namespace muscle

#endif
