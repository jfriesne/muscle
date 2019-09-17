/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStringMatcher_h
#define MuscleStringMatcher_h

#include <sys/types.h>
#include "support/BitChord.h"
#include "util/Queue.h"
#include "util/RefCount.h"
#include "util/String.h"

#ifdef __BEOS__
# if __POWERPC__
#  include "regex/regex/regex.h"  // use included regex if system doesn't provide one
# else
#  include <regex.h>
# endif
#else
# ifdef WIN32
#  include "regex/regex/regex.h"
# else
#  include <regex.h>
# endif
#endif

namespace muscle {

/** This class uses the regex library to implement "simple" string matching (similar to filename globbing in bash) as well as full regular expression pattern-matching. */
class StringMatcher MUSCLE_FINAL_CLASS : public RefCountable
{
public:
   /** Default Constructor. */
   StringMatcher() {/* empty */}

   /** A constructor that sets the given expression.  See SetPattern() for argument semantics.
     * @param expression the pattern string to use in future pattern-matches.
     * @param isSimpleFormat if true, a simple globbing syntax is expected in (expression).  
     *                       Otherwise, the full regex syntax will be expected.  Defaults to true.
     */
   StringMatcher(const String & expression, bool isSimpleFormat = true) {(void) SetPattern(expression, isSimpleFormat);}
    
   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   StringMatcher(const StringMatcher & rhs) : RefCountable(rhs) {*this = rhs;}

   /** Destructor */
   ~StringMatcher() {Reset();}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator == (const StringMatcher & rhs) const {return ((_flags == rhs._flags)&&(_pattern == rhs._pattern));}

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator != (const StringMatcher & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   StringMatcher & operator = (const StringMatcher & rhs);

   /** 
    * Set a new wildcard pattern or regular expression for this StringMatcher to use in future Match() calls.
    *
    * Simple patterns specified here also have a special case:  If you set a pattern of the form 
    * "<x-y>", where x and y are ASCII representations of positive integers, then this StringMatcher will only 
    * match ASCII representations of integers in that range.  So "<19-21>" would match "19", "20", and "21" only.
    * Also, "<-19>" will match integers less than or equal to 19, and "<21->" will match all integers greater
    * than or equal to 21.  "<->" will match everything, same as "*".
    *
    * Note:  In v5.84 and above, this syntax has been extended further to allow multiple ranges.  For example,
    * "<19-21,25,30-50>" would match the IDs 19 through 21, 25, and 30-50, all inclusive.
    *
    * Note:  In v6.12 and above, the presence of a backtick character (`) at the front of the string will
    * force the string to be parsed as a regex rather than a wildcard pattern.  (The backtick itself will not 
    * be included in the regex string).  That way raw regex strings can be used in contexts that would otherwise
    * not permit the user to specify isSimpleFormat=false (e.g. in MUSCLE subscription strings)
    *
    * Also, simple patterns that begin with a tilde (~) will be logically negated, e.g. ~A* will match all
    * strings that do NOT begin with the character A.
    *
    * @param expression The new globbing pattern or regular expression to match with.
    * @param isSimpleFormat If you wish to use the formal regex syntax, 
    *                       instead of the simple syntax, set isSimpleFormat to false.
    * @return B_NO_ERROR on success, or B_BAD_ARGUMENT if the expression wasn't parsable, or B_OUT_OF_MEMORY.
    */
   status_t SetPattern(const String & expression, bool isSimpleFormat=true);
    
   /** Returns the pattern String as it was previously passed in to SetPattern() */
   const String & GetPattern() const {return _pattern;}

   /** Returns true iff this StringMatcher's pattern specifies exactly one possible string.
    *  (i.e. the pattern is just plain old text, with no wildcards or other pattern matching logic specified)
    *  @note StringMatchers using numeric ranges are never considered unique, because e.g. looking up the
    *        string "<5>" in a Hashtable would not return a child node whose node-name is "5".
    */
   bool IsPatternUnique() const {return ((_ranges.IsEmpty())&&(_flags.AreAnyOfTheseBitsSet(STRINGMATCHER_FLAG_CANMATCHMULTIPLEVALUES,STRINGMATCHER_FLAG_NEGATE) == false));}

   /** Returns true iff this StringMatcher's pattern specifies a comma-separated list of one or more non-wildcarded substrings. */
   bool IsPatternListOfUniqueValues() const {return _flags.IsBitSet(STRINGMATCHER_FLAG_UVLIST);}

   /** Returns true iff (string) is matched by the current expression.
    * @param matchString a string to match against using our current expression.
    * @return true iff (matchString) matches, false otherwise.
    */
   bool Match(const char * const matchString) const;
    
   /** Convenience method:  Same as above, but takes a String object instead of a (const char *).
     * @param matchString a string to match against using our current expression.
     * @return true iff (matchString) matches, false otherwise.
     */
   inline bool Match(const String & matchString) const {return Match(matchString());}

   /** If set true, Match() will return the logical opposite of what
     * it would otherwise return; e.g. it will return true only when
     * the given string doesn't match the pattern.
     * Default state is false.  Note that this flag is also set by
     * SetPattern(..., true), based on whether or not the pattern
     * string starts with a tilde.
     * @param negate true if we should negate our Match()-output; false for the regular logic
     */
   void SetNegate(bool negate) {_flags.SetBit(STRINGMATCHER_FLAG_NEGATE, negate);}

   /** Returns the current state of our negate flag. */
   bool IsNegate() const {return _flags.IsBitSet(STRINGMATCHER_FLAG_NEGATE);}

   /** Returns the true iff our current pattern is of the "simple" variety, or false if it is of the "official regex" variety. */
   bool IsSimple() const {return _flags.IsBitSet(STRINGMATCHER_FLAG_SIMPLE);}

   /** Resets this StringMatcher to the state it would be in if created with default arguments. */
   void Reset();

   /** Returns a human-readable string representing this StringMatcher, for debugging purposes. */
   String ToString() const;

   /** @copydoc DoxyTemplate::HashCode() const */
   inline uint32 HashCode() const {return _pattern.HashCode() + _flags.HashCode();}

   /** Efficiently swaps our state with the state of the StringMatcher passed in as an argument.
     * @param withMe the StringMatcher whose state should be swapped with our own.
     */
   void SwapContents(StringMatcher & withMe);

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   StringMatcher(StringMatcher && rhs) {SwapContents(rhs);}

   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   StringMatcher & operator =(StringMatcher && rhs) {SwapContents(rhs); return *this;}
#endif

private:
   enum {
      STRINGMATCHER_FLAG_REGEXVALID = 0,
      STRINGMATCHER_FLAG_NEGATE,
      STRINGMATCHER_FLAG_CANMATCHMULTIPLEVALUES,
      STRINGMATCHER_FLAG_SIMPLE,
      STRINGMATCHER_FLAG_UVLIST,
      NUM_STRINGMATCHER_FLAGS
   };
   DECLARE_BITCHORD_FLAGS_TYPE(StringMatcherFlags, NUM_STRINGMATCHER_FLAGS);

   class IDRange
   {
   public:
      IDRange() : _min(0), _max(0) {/* empty */}
      IDRange(uint32 min, uint32 max) : _min(muscleMin(min,max)), _max(muscleMax(min,max)) {/* empty */}

      uint32 GetMin() const {return _min;}
      uint32 GetMax() const {return _max;}

   private:
      uint32 _min; 
      uint32 _max; 
   };

   StringMatcherFlags _flags;
   String _pattern;
   regex_t _regExp;
   Queue<IDRange> _ranges;
}; 
DECLARE_REFTYPES(StringMatcher);

/** Returns a point to a singleton ObjectPool that can be used
 *  to minimize the number of StringMatcher allocations and deletions
 *  by recycling the StringMatcher objects 
 */
StringMatcherRef::ItemPool * GetStringMatcherPool();

/** Convenience method.  Returns a StringMatcher object from the default StringMatcher pool,
  * or a NULL reference on failure (out of memory?)
  */
StringMatcherRef GetStringMatcherFromPool();

/** Convenience method.  Obtains a StringMatcher object from the default StringMatcher pool, calls SetPattern() on it
  * with the given arguments, and returns it, or a NULL reference on failure (out of memory, or a parse error?)
  */
StringMatcherRef GetStringMatcherFromPool(const String & matchString, bool isSimpleFormat = true);

// Some regular expression utility functions

/** Returns (str), except the returned string has a backslash inserted in front of any char in (str) 
 *  that is "special" to the regex pattern matching.
 *  @param str The string to check for special regex chars.
 *  @param optTokens If non-NULL, only characters in this string will be treated as regex tokens
 *                   and escaped.  If left as NULL (the default), then the standard set of regex
 *                   tokens will be escaped.
 *  @returns the modified String with escaped regex chars
 */
String EscapeRegexTokens(const String & str, const char * optTokens = NULL);

/** This does essentially the opposite of EscapeRegexTokens():  It removes from the string
  * any backslashes that are not immediately preceded by another backslash.
  */
String RemoveEscapeChars(const String & str);

/** Returns true iff any "special" chars are found in (str).
 *  @param str The string to check for special regex chars.
 *  @return True iff any special regex chars were found in (str).
 */
bool HasRegexTokens(const char * str);

/** As above, but takes a String object instead of a (const char *) */
inline bool HasRegexTokens(const String & str) {return HasRegexTokens(str());}

/** Returns true iff the specified wild card pattern string can match more than one possible value-string.
 *  @param str The pattern string to check
 *  @param optRetIsCommasOnly If non-NULL, the boolean this points to will
 *         be set to true if the only wildcard characters seen in this string
 *         were commas, or false if there are other wildcard characters as well.
 *  @return True iff the string might match more than one value-string; false if not.
 */
bool CanWildcardStringMatchMultipleValues(const char * str, bool * optRetIsCommasOnly = NULL);

/** As above, but takes a String object instead of a (const char *) */
inline bool CanWildcardStringMatchMultipleValues(const String & str, bool * optRetIsCommasOnly = NULL) {return CanWildcardStringMatchMultipleValues(str(), optRetIsCommasOnly);}

/** Returns true iff (c) is a regular expression "special" char as far as StringMatchers are concerned.
 *  @param c an ASCII char
 *  @param isFirstCharInString true iff (c) is the first char in a pattern string.  (important because
 *                                  StringMatchers recognize certain chars as special only if they are the first in the string)
 *  @return true iff (c) is a special regex char.
 */
bool IsRegexToken(char c, bool isFirstCharInString);

/** Given a regular expression, makes it case insensitive by
 *  replacing every occurance of a letter with a upper-lower combo,
 *  e.g. Hello -> [Hh][Ee][Ll][Ll][Oo]
 *  @param str a string to check for letters, and possibly modify to make case-insensitive
 *  @return true iff anything was changed, false if no changes were necessary.
 */
bool MakeRegexCaseInsensitive(String & str);

/** Given a regular expression, returns a case insensitive version.
 *  Same as MakeRegexCaseInsensitive(), except the value is returned directly.
 *  @param str a String to modify to make case-insensitive
 *  @return the modified String.
 */
inline String ToCaseInsensitive(const String & str) {String r = str; MakeRegexCaseInsensitive(r); return r;}

} // end namespace muscle

#endif
