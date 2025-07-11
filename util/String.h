/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleString_h
#define MuscleString_h

/* NOTE TO MACOS/X X-CODE USERS:  If you are trying to #include <string.h>
 * and X-Code is "helpfully" pulling in this file instead (because the
 * OS/X filesystem is case-insensitive), you can get around that problem
 * by adding "USE_HEADERMAP = NO" to your X-Code target settings.
 * ref:  http://lists.apple.com/archives/xcode-users/2004/Aug/msg00934.html
 *  --Jeremy
 */

#include <ctype.h>
#include "support/PseudoFlattenable.h"
#include "syslog/SysLog.h"
#include "system/GlobalMemoryAllocator.h"  // for muscleFree()
#include "util/Hashtable.h"

#ifdef __APPLE__
// Using a forward declaration rather than an #include here to avoid pulling in other things like Mac's
// Point and Rect typedefs, that can cause ambiguities with Muscle's Point and Rect classes.
struct __CFString;
typedef const struct __CFString * CFStringRef;
#endif

namespace muscle {

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
# ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
enum {
   STRING_OP_DEFAULT_CTOR = 0,
   STRING_OP_CSTR_CTOR,
   STRING_OP_COPY_CTOR,
   STRING_OP_PARTIAL_COPY_CTOR,
   STRING_OP_PREALLOC_COPY_CTOR,
   STRING_OP_CFSTR_COPY_CTOR,
   STRING_OP_SET_FROM_CSTR,
   STRING_OP_SET_FROM_STRING,
   STRING_OP_MOVE_CTOR,
   STRING_OP_MOVE_FROM_STRING,
   STRING_OP_DTOR,
   NUM_STRING_OPS
};
extern uint32 _stringOpCounts[NUM_STRING_OPS];
extern void PrintAndClearStringCopyCounts(const OutputPrinter & p, const char * optDesc = NULL);
#  define MUSCLE_INCREMENT_STRING_OP_COUNT(which) _stringOpCounts[which]++
# else
#  define MUSCLE_INCREMENT_STRING_OP_COUNT(which)
static inline void PrintAndClearStringCopyCounts(const OutputPrinter & p, const char * optDesc = NULL) {(void) p; (void) optDesc;}
# endif
#endif

class Point;
class Rect;

/** Same as strcmp(), except that it will sort numbers within the string numerically rather than lexically.
  * @param s1 The first of the two strings to compare using the number-aware comparison algorithm.
  * @param s2 The second of the two strings to compare using the number-aware comparison algorithm.
  * @returns 0 if the two strings are equal, >0 if (s2) is the "later" string, or <0 if (s1) is the "later" string.
  */
MUSCLE_NODISCARD int NumericAwareStrcmp(const char * s1, const char * s2);

/** Same as NumericAwareStrcmp(), but case-insensitive.
  * @param s1 The first of the two strings to compare using the case-insensitive number-aware comparison algorithm.
  * @param s2 The second of the two strings to compare using the case-insensitive number-aware comparison algorithm.
  * @returns 0 if the two strings are equal, >0 if (s2) is the "later" string, or <0 if (s1) is the "later" string.
  */
MUSCLE_NODISCARD int NumericAwareStrcasecmp(const char * s1, const char * s2);

/** Wrapper for strcasecmp(), which isn't always named the same on all OS's
  * @param s1 The first of the two strings to compare using the case-insensitive comparison algorithm.
  * @param s2 The second of the two strings to compare using the case-insensitive comparison algorithm.
  * @returns 0 if the two strings are equal, >0 if (s2) is the "later" string, or <0 if (s1) is the "later" string.
  */
MUSCLE_NODISCARD inline int Strcasecmp(const char * s1, const char * s2)
{
#ifdef WIN32
   return _stricmp(s1, s2);
#else
   return strcasecmp(s1, s2);
#endif
}

/** Wrapper for strncasecmp(), which isn't always named the same on all OS's
  * @param s1 The first of the two strings to compare using the case-insensitive comparison algorithm.
  * @param s2 The second of the two strings to compare using the case-insensitive comparison algorithm.
  * @param n The maximum number of bytes to compare (any bytes after the (nth) byte in the strings will be ignored)
  * @returns 0 if the two strings are equal, >0 if (s2) is the "later" string, or <0 if (s1) is the "later" string.
  */
MUSCLE_NODISCARD inline int Strncasecmp(const char * s1, const char * s2, size_t n)
{
#ifdef WIN32
   return _strnicmp(s1, s2, n);
#else
   return strncasecmp(s1, s2, n);
#endif
}

/** Wrapper for strcasestr(), which isn't always present on all OS's.
  * @param haystack The string to search in
  * @param needle The string to search for
  * @returns a pointer to (needle), if it exists inside (haystack), or NULL if it doesn't.
  * @note that the search is case-insensitive.
  */
MUSCLE_NODISCARD const char * Strcasestr(const char * haystack, const char * needle);

/** Extended wrapper for strcasestr(), which isn't always named the same on all OS's
  * @param haystack The string to search in
  * @param haystackLen The number of text-bytes that (haystack) is pointing to (ie strlen(haystack))
  * @param needle The string to search for
  * @param needleLen The number of text-bytes that (needle) is pointing to (ie strlen(needle))
  * @param searchBackwards If set true, the last instance of (needle) in the (haystack) will be returned rather than the first.
  * @returns a pointer to a (needle), if one exists inside (haystack), or NULL if it doesn't.
  * @note that the search is case-insensitive.
  */
MUSCLE_NODISCARD const char * StrcasestrEx(const char * haystack, uint32 haystackLen, const char * needle, uint32 needleLen, bool searchBackwards);

/** Convenience function:  Returns true iff the C string (haystack) starts with the string (prefix).
  * @param haystack the C string to check
  * @param haystackLen The length of (haystack), in bytes, not including the NUL terminator byte.
  * @param prefix the prefix to check for at the start of (haystack).  If NULL, this function will return true.
  * @param prefixLen The length of (prefix), in bytes, not including the NUL terminator byte.
  * @note the test is case-sensitive.
  */
MUSCLE_NODISCARD inline bool StrStartsWith(const char * haystack, uint32 haystackLen, const char * prefix, uint32 prefixLen)
{
   return ((prefix == NULL)||((haystackLen >= prefixLen)&&(strncmp(haystack, prefix, prefixLen) == 0)));
}

/** Convenience function:  Returns true iff the C string (haystack) ends with the string (suffix).
  * @param haystack the C string to check
  * @param haystackLen The length of (haystack), in bytes, not including the NUL terminator byte.
  * @param suffix the suffix to check for at the ends of (haystack).  If NULL, this function will return true.
  * @param suffixLen The length of (suffix), in bytes, not including the NUL terminator byte.
  * @note the test is case-sensitive.
  */
MUSCLE_NODISCARD inline bool StrEndsWith(const char * haystack, uint32 haystackLen, const char * suffix, uint32 suffixLen)
{
   return ((suffix == NULL)||((haystackLen >= suffixLen)&&(strcmp(haystack+(haystackLen-suffixLen), suffix) == 0)));
}

/** Convenience function:  Returns true iff the C string (haystack) starts with the string (prefix).
  * @param haystack the C string to check
  * @param haystackLen The length of (haystack), in bytes, not including the NUL terminator byte.
  * @param prefix the prefix to check for at the start of (haystack).  If NULL, this function will return true.
  * @param prefixLen The length of (prefix), in bytes, not including the NUL terminator byte.
  * @note the test is case-insensitive.
  */
MUSCLE_NODISCARD inline bool StrStartsWithIgnoreCase(const char * haystack, uint32 haystackLen, const char * prefix, uint32 prefixLen)
{
   return ((prefix == NULL)||((haystackLen >= prefixLen)&&(Strncasecmp(haystack, prefix, prefixLen) == 0)));
}

/** Convenience function:  Returns true iff the C string (haystack) ends with the string (suffix).
  * @param haystack the C string to check
  * @param haystackLen The length of (haystack), in bytes, not including the NUL terminator byte.
  * @param suffix the suffix to check for at the ends of (haystack).  If NULL, this function will return true.
  * @param suffixLen The length of (suffix), in bytes, not including the NUL terminator byte.
  * @note the test is case-insensitive.
  */
MUSCLE_NODISCARD inline bool StrEndsWithIgnoreCase(const char * haystack, uint32 haystackLen, const char * suffix, uint32 suffixLen)
{
   return ((suffix == NULL)||((haystackLen >= suffixLen)&&(Strcasecmp(haystack+(haystackLen-suffixLen), suffix) == 0)));
}

/** An arbitrary-length character-string class.  Represents a dynamically resizable, NUL-terminated ASCII string.
  * This class can be used to hold UTF8-encoded strings as well, but because the code in this class is not
  * UTF8-aware, certain operations (such as Reverse() and ToLowerCase()) may not do the right thing when used in
  * conjunction with non-ASCII UTF8 data.
  */
class MUSCLE_NODISCARD String MUSCLE_FINAL_CLASS : public PseudoFlattenable<String>
{
public:
   /** Constructor.
    *  @param str If non-NULL, the initial value for this String.
    *  @param maxLen The maximum number of characters to place into
    *                this String (not including the NUL terminator byte).
    *                Default is unlimited (ie scan the entire string no matter how long it is)
    */
   String(const char * str = NULL, uint32 maxLen = MUSCLE_NO_LIMIT)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(str?STRING_OP_CSTR_CTOR:STRING_OP_DEFAULT_CTOR);
      ClearShortStringBuffer();
      if (str) (void) SetCstr(str, maxLen);
   }

   /** Explicitely-sized constructor.
     * @param preallocatedBytesCount Contains the value to pass to Prealloc() as part of our construction
     * @param str If non-NULL, the initial value for this String.
     * @param maxLen The maximum number of characters to place into this String (not including the NUL terminator byte).
     *               Default is unlimited (ie scan the entire string no matter how long it is)
     */
   String(PreallocatedItemSlotsCount preallocatedBytesCount, const char * str = NULL, uint32 maxLen = MUSCLE_NO_LIMIT)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(str?STRING_OP_CSTR_CTOR:STRING_OP_DEFAULT_CTOR);
      ClearShortStringBuffer();
      (void) Prealloc(preallocatedBytesCount.GetNumItemSlotsToPreallocate());
      if (str) (void) SetCstr(str, maxLen);
   }

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   String(const String & rhs)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_COPY_CTOR);
      ClearShortStringBuffer();
      (void) SetFromString(rhs);
   }

   /** Pseudo-copy constructor.
     * @param rhs the String to make this String a copy of
     * @param extraBytesToPrealloc a count of additional bytes to preallocate up-front
     * @note this constructor is useful if you know you are going to add some additional bytes to the String immediately after
     *       construction, and you want to avoid an unnecessary memory reallocation
     */
   String(const String & rhs, PreallocatedItemSlotsCount extraBytesToPrealloc)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_PREALLOC_COPY_CTOR);
      ClearShortStringBuffer();
      (void) Prealloc(rhs.Length()+extraBytesToPrealloc.GetNumItemSlotsToPreallocate());
      (void) SetFromString(rhs);
   }

   /** This constructor sets this String to be a substring of the specified String.
     * @param str String to become a copy of.
     * @param beginIndex Index of the first character in (str) to include.
     * @param endIndex Index after the last character in (str) to include.
     *                 Defaults to a very large number, so that by default the entire remainder of the string is included.
     */
   String(const String & str, uint32 beginIndex, uint32 endIndex=MUSCLE_NO_LIMIT)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_PARTIAL_COPY_CTOR);
      ClearShortStringBuffer();
      (void) SetFromString(str, beginIndex, endIndex);
   }

#ifdef __APPLE__
   /** Special MACOS/X-only convenience constructor that sets our state from a UTF8 Core Foundation String
     * @param cfStringRef A CFStringRef that we will get our string value from
     */
   String(const CFStringRef & cfStringRef)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_CFSTR_COPY_CTOR);
      ClearShortStringBuffer();
      (void) SetFromCFStringRef(cfStringRef);
   }
#endif

   /** Destructor. */
   ~String()
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_DTOR);
      if (IsArrayDynamicallyAllocated()) _stringData._longStringData.FreeBuffer();
   }

   /** Assignment Operator.
     * @param val Pointer to the C-style string to copy from.  If NULL, this string will become an empty string.
     */
   String & operator = (const char * val)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_SET_FROM_CSTR);
      (void) SetCstr(val); return *this;
   }

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   String & operator = (const String & rhs)
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_SET_FROM_STRING);
      (void) SetFromString(rhs); return *this;
   }

   /** Append Operator.
    *  @param rhs A string to append to this string.
    */
   String & operator += (const String & rhs);

   /** Templated Append Operator.  Equivalent to calling += t.ToString().
     * @param t Reference to the class object to call ToString() on and then append.
     */
   template<class T> String & operator +=(const T & t) {(*this) += t.ToString(); return *this;}

   /** Append Operator.
    *  @param rhs A string to append to this string.  If NULL, this operation is a no-op.
    */
   String & operator += (char * rhs) {return ((*this)+=((const char *)rhs));}

   /** Append Operator.
    *  @param rhs A string to append to this string.  If NULL, this operation is a no-op.
    */
   String & operator += (const char * rhs);

   /** Append Operator.
    *  @param ch A character to append to this string.
    */
   String & operator += (char ch)
   {
      const uint32 length = Length();
      if (EnsureBufferSize(length+2, true, false).IsOK())
      {
         GetBuffer()[length] = ch;
         SetLength(length+1);
         WriteNULTerminatorByte();
      }
      return *this;
   }

   /** Templated Remove Operator.  Equivalent to calling -= t.ToString().
     * @param t Reference to the class object to call ToString() on and then remove.
     */
   template<class T> String & operator -=(const T & t) {(*this) -= t.ToString(); return *this;}

   /** Remove Operator.
    *  @param rhs A substring to remove from this string;  the
    *             last instance of the substring will be cut out.
    *             If (rhs) is not found, there is no effect.
    */
   String & operator -= (const String & rhs);

   /** Remove Operator.
    *  @param rhs A substring to remove from this string;  the
    *             last instance of the substring will be cut out.
    *             If (rhs) is not found, there is no effect.
    */
   String & operator -= (const char * rhs);

   /** Remove Operator.
    *  @param rhs A substring to remove from this string;  the
    *             last instance of the substring will be cut out.
    *             If (rhs) is not found, there is no effect.
    */
   String & operator -= (char * rhs) {return ((*this) -= ((const char *)rhs));}

   /** Remove Operator.
    *  @param ch A character to remove from this string;  the last
    *            instance of this char will be cut out.  If (ch) is
    *            not found, there is no effect.
    */
   String & operator -= (const char ch);

   /** Append 'Stream' Operator.
    *  @param rhs A String to append to this string.
    *  @return a non const String refrence to 'this' so you can chain appends.
    */
   String & operator << (const String & rhs) {return (*this += rhs);}

   /** Append 'Stream' Operator.
    *  @param rhs A const char* to append to this string.
    *  @return a non const String refrence to 'this' so you can chain appends.
    */
   String & operator << (const char * rhs) {return (*this += rhs);}

   /** Append 'Stream' Operator.
    *  @param rhs An int to append to this string.
    *  @return a non const String refrence to 'this' so you can chain appends.
    */
   String & operator << (int rhs);

   /** Append 'Stream' Operator.
    *  @param rhs A float to append to this string. Formatting is set at 2 decimals of precision.
    *  @return a non const String refrence to 'this' so you can chain appends.
    */
   String & operator << (float rhs);

   /** Append 'Stream' Operator.
    *  @param rhs A bool to append to this string. Converts to 'true' and 'false' strings appropriately.
    *  @return a non const String refrence to 'this' so you can chain appends.
    */
   String & operator << (bool rhs);

   /** Comparison Operator.  Returns true iff the two strings contain the same sequence of characters (as determined by memcmp()).
     * @param rhs A string to compare ourself with
     */
   bool operator == (const String & rhs) const {return ((this == &rhs)||((Length() == rhs.Length())&&(memcmp(Cstr(), rhs.Cstr(), Length()) == 0)));}

   /** Comparison Operator.  Returns true if the two strings are equal (as determined by strcmp())
     * @param rhs Pointer to a C string to compare with.  NULL pointers are considered a synonym for "".
     */
   bool operator == (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") == 0);}

   /** Comparison Operator.  Returns true if the two strings are not equal (as determined by memcmp())
     * @param rhs A string to compare ourself with
     */
   bool operator != (const String & rhs) const {return !(*this == rhs);}

   /** Comparison Operator.  Returns true if the two strings are not equal (as determined by strcmp())
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   bool operator != (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") != 0);}

   /** Comparison Operator.  Returns true if this string comes before (rhs) lexically (as determined by strcmp()).
     * @param rhs A string to compare ourself with
     */
   bool operator < (const String & rhs) const {return (this == &rhs) ? false : (strcmp(Cstr(), rhs.Cstr()) < 0);}

   /** Comparison Operator.  Returns true if this string comes before (rhs) lexically (as determined by strcmp()).
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   bool operator < (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") < 0);}

   /** Comparison Operator.  Returns true if this string comes after (rhs) lexically (as determined by strcmp()).
     * @param rhs A string to compare ourself with
     */
   bool operator > (const String & rhs) const {return (this == &rhs) ? false : (strcmp(Cstr(), rhs.Cstr()) > 0);}

   /** Comparison Operator.  Returns true if this string comes after (rhs) lexically.
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   bool operator > (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") > 0);}

   /** Comparison Operator.  Returns true if the two strings are equal, or this string comes before (rhs) lexically (as determined by strcmp()).
     * @param rhs A string to compare ourself with
     */
   bool operator <= (const String & rhs) const {return (this == &rhs) ? true : (strcmp(Cstr(), rhs.Cstr()) <= 0);}

   /** Comparison Operator.  Returns true if the two strings are equal, or this string comes before (rhs) lexically (as determined by strcmp()).
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   bool operator <= (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") <= 0);}

   /** Comparison Operator.  Returns true if the two strings are equal, or this string comes after (rhs) lexically (as determined by strcmp()).
     * @param rhs A string to compare ourself with
     */
   bool operator >= (const String & rhs) const {return (this == &rhs) ? true : (strcmp(Cstr(), rhs.Cstr()) >= 0);}

   /** Comparison Operator.  Returns true if the two strings are equal, or this string comes after (rhs) lexically (as determined by strcmp()).
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   bool operator >= (const char * rhs) const {return (strcmp(Cstr(), rhs?rhs:"") >= 0);}

   /** Array Operator.  Used to get easy access to the characters that make up this string.
    *  @param index Index of the character to return.  Be sure to only use valid indices!
    */
   MUSCLE_NODISCARD char operator [] (uint32 index) const {VerifyIndex(index); return Cstr()[index];}

   /** Array Operator.  Used to get easy access to the characters that make up this string.
    *  @param index Index of the character to set.  Be sure to only use valid indices!
    */
   MUSCLE_NODISCARD char & operator [] (uint32 index) {VerifyIndex(index); return GetBuffer()[index];}

   /** Adds a space to the end of this string. */
   void operator ++ (int) {(*this) += ' ';}

   /** Remove the last character from the end of this string.  It's a no-op if the string is already empty. */
   void operator -- (int) {TruncateChars(1);}

   /** Returns the character at the (index)'th position in the string.
    *  @param index A value between 0 and (Length()-1), inclusive.
    *  @return A character value.
    */
   MUSCLE_NODISCARD char CharAt(uint32 index) const {return operator[](index);}

   /** Compares this string to another string using strcmp()
     * @param rhs A string to compare ourself with
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int CompareTo(const String & rhs) const {return strcmp(Cstr(), rhs.Cstr());}

   /** Compares this string to a C string using strcmp()
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int CompareTo(const char * rhs) const {return strcmp(Cstr(), rhs?rhs:"");}

   /** Compares this string to another string using NumericAwareStrcmp()
     * @param rhs A string to compare ourself with
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int NumericAwareCompareTo(const String & rhs) const {return NumericAwareStrcmp(Cstr(), rhs.Cstr());}

   /** Compares this string to a C string using NumericAwareStrcmp()
     * @param rhs Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int NumericAwareCompareTo(const char * rhs) const {return NumericAwareStrcmp(Cstr(), rhs?rhs:"");}

   /** Returns a read-only C-style pointer to our held character string. */
   MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * Cstr() const {return IsArrayDynamicallyAllocated() ? _stringData._longStringData.Cstr() : _stringData._shortStringData.Cstr();}

   /** Convenience synonym for Cstr(). */
   MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * operator()() const {return Cstr();}

   /** Clears this string so that it contains no characters.  Equivalent to setting this string to "". */
   void Clear() {SetLength(0); WriteNULTerminatorByte();}

   /** Similar to Clear(), except this version also frees up any dynamically allocated character array we may have cached. */
   void ClearAndFlush();

   /** Shrinks our internally allocated buffer down so that it is just big enough to hold the current string (plus numExtraBytes)
     * @param numExtraBytes an additional number of bytes that the buffer should have room for.  Defaults to zero.
     * @returns B_NO_ERROR on success, or an error code on failure (although I don't know why this method would ever fail).
     */
   status_t ShrinkToFit(uint32 numExtraBytes = 0) {return EnsureBufferSize(FlattenedSize()+numExtraBytes, true, true);}

   /** Sets our state from the given C-style string.
     * @param str The new string to copy from.  If maxLen is negative, this may be NULL.
     * @param maxLen If set, the maximum number of characters to copy (not including the NUL
     *               terminator byte).  By default, all characters up to the first encountered
     *               NUL terminator byte will be copied out of (str).
     */
   status_t SetCstr(const char * str, uint32 maxLen = MUSCLE_NO_LIMIT);

   /** Sets our state from the given String.  This is similar to the copy constructor, except
     * that it allows you to optionally specify a maximum length, and it allows you to detect
     * out-of-memory errors.
     * @param str The new string to copy from.
     * @param beginIndex Index of the first character in (str) to include.
     *                   Defaults to zero, so that by default the entire string is included.
     * @param endIndex Index after the last character in (str) to include.
     *                 Defaults to a very large number, so that by default the entire remainder of the string is included.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t SetFromString(const String & str, uint32 beginIndex = 0, uint32 endIndex = MUSCLE_NO_LIMIT);

#ifdef __APPLE__
   /** MACOS/X-only convenience method:  Sets our string equal to the string pointed to by (cfStringRef).
     * @param cfStringRef A CFStringRef that we will get our string value from.  May be NULL (in which case we'll be set to an empty string)
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SetFromCFStringRef(const CFStringRef & cfStringRef);

   /** MACOS/X-only convenience method:  Returns a CFStringRef containing the same string that we have.
     * It becomes the caller's responsibility to CFRelease() the CFStringRef when he is done with it.
     * May return a NULL CFStringRef on failure (eg out of memory)
     */
   CFStringRef ToCFStringRef() const;
#endif

   /** Returns true iff this string is a zero-length string. */
   MUSCLE_NODISCARD bool IsEmpty() const {return (Length() == 0);}

   /** Returns true iff this string starts with a number.
     * @param allowNegativeValues if true, negative values will be considered as numbers also.  Defaults to true.
     */
   MUSCLE_NODISCARD bool StartsWithNumber(bool allowNegativeValues = true) const {const char * s = Cstr(); return ((muscleIsDigit(*s))||((allowNegativeValues)&&(s[0]=='-')&&(muscleIsDigit(s[1]))));}

   /** Returns true iff this string is not a zero-length string. */
   MUSCLE_NODISCARD bool HasChars() const {return (Length() > 0);}

   /** Returns true iff this string starts with (prefix)
     * @param c a character to check for at the end of this String.
     */
   MUSCLE_NODISCARD bool EndsWith(char c) const {const uint32 len = Length(); return ((len > 0)&&(Cstr()[len-1] == c));}

   /** Returns true iff this string ends with (suffix)
     * @param suffix a String to check for at the end of this String.
     */
   MUSCLE_NODISCARD bool EndsWith(const String & suffix) const {return StrEndsWith(Cstr(), Length(), suffix(), suffix.Length());}

   /** Returns true iff this string ends with (suffix)
     * @param suffix a String to check for at the end of this String.  NULL pointers are treated as a synonym for "".
     */
   MUSCLE_NODISCARD bool EndsWith(const char * suffix) const {return StrEndsWith(Cstr(), Length(), suffix, suffix?(uint32)strlen(suffix):0);}

   /** Returns true iff this string is equal to (string), as determined by strcmp().
     * @param str a String to compare this String with.
     */
   MUSCLE_NODISCARD bool Equals(const String & str) const {return (*this == str);}

   /** Returns true iff this string is equal to (str), as determined by strcmp().
     * @param str Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   MUSCLE_NODISCARD bool Equals(const char * str) const {return (*this == str);}

   /** Returns true iff this string contains a single character (c).
     * @param c a character to compare this String with.
     */
   MUSCLE_NODISCARD bool Equals(char c) const {return ((Length() == 1)&&(Cstr()[0] == c));}

   /** Returns the first index of (ch) in this string starting at or after (fromIndex), or -1 if not found.
     * @param ch A character to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int IndexOf(char ch, uint32 fromIndex = 0) const
   {
      const char * temp = (fromIndex < Length()) ? strchr(Cstr()+fromIndex, ch) : NULL;
      return temp ? (int)(temp - Cstr()) : -1;
   }

   /** Returns true iff (ch) is contained in this string.
     * @param ch A character to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD bool Contains(char ch, uint32 fromIndex = 0) const {return (IndexOf(ch, fromIndex) >= 0);}

   /** Returns true iff substring (str) is in this string starting at or after (fromIndex).
     * @param str A String to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD bool Contains(const String & str, uint32 fromIndex = 0) const {return (IndexOf(str, fromIndex) >=0);}

   /** Returns true iff the substring (str) is in this string starting at or after (fromIndex).
     * @param str Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD bool Contains(const char * str, uint32 fromIndex = 0) const {return (IndexOf(str, fromIndex) >= 0);}

   /** Returns the first index of substring (str) in this string starting at or after (fromIndex), or -1 if not found.
     * @param str A String to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int IndexOf(const String & str, uint32 fromIndex = 0) const
   {
      const char * temp = (fromIndex < Length()) ? strstr(Cstr()+fromIndex, str()) : NULL;
      return temp ? (int)(temp - Cstr()) : -1;
   }

   /** Returns the first index of substring (str) in this string starting at or after (fromIndex), or -1 if not found.
     * @param str Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int IndexOf(const char * str, uint32 fromIndex = 0) const
   {
      const char * temp = (fromIndex < Length()) ? strstr(Cstr()+fromIndex, str?str:"") : NULL;
      return temp ? (int)(temp - Cstr()) : -1;
   }

   /** Returns the last index of (ch) in this string starting at or after (fromIndex), or -1 if not found.
     * @param ch A character to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int LastIndexOf(char ch, uint32 fromIndex = 0) const
   {
      if (fromIndex < Length())
      {
         const char * s = Cstr()+fromIndex;
         const char * p = Cstr()+Length();
         while(--p >= s) if (*p == ch) return (int)(p-Cstr());
      }
      return -1;
   }

   /** Returns the last index of substring (str) in this string
     * @param str A String to look for in this string.
     */
   MUSCLE_NODISCARD int LastIndexOf(const String & str) const {return (str.Length() <= Length()) ? LastIndexOf(str, Length()-str.Length()) : -1;}

   /** Returns the last index of substring (str) in this string
     * @param str Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   MUSCLE_NODISCARD int LastIndexOf(const char * str) const
   {
      if (str == NULL) str = "";
      const uint32 strLen = (uint32) strlen(str);
      return (strLen <= Length()) ? LastIndexOf(str, Length()-strLen) : -1;
   }

   /** Returns the last index of substring (str) in this string starting at or after (fromIndex), or -1 if not found.
     * @param str A String to look for in this string.
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int LastIndexOf(const String & str, uint32 fromIndex) const;

   /** Returns the last index of substring (str) in this string starting at or after (fromIndex), or -1 if not found.
     * @param str Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     * @param fromIndex Index of the first character to start searching at in this String.  Defaults to zero (ie start from the first character)
     */
   MUSCLE_NODISCARD int LastIndexOf(const char * str, uint32 fromIndex) const;

   /** Returns the number of characters in the string (not including the terminating NUL byte) */
   MUSCLE_NODISCARD uint32 Length() const {return IsArrayDynamicallyAllocated() ? _stringData._longStringData.Length() : _stringData._shortStringData.Length();}

   /** Returns the number of bytes of storage we have allocated.  Note that this value will often
     * be greater than the value returned by Length(), since we allocate extra bytes to minimize
     * the number of reallocations that must be done as data is being added to a String.
     */
   MUSCLE_NODISCARD uint32 GetNumAllocatedBytes() const {return IsArrayDynamicallyAllocated() ? _stringData._longStringData.GetNumAllocatedBytes() : _stringData._shortStringData.GetNumAllocatedBytes();}

   /** Returns the number of instances of (c) in this string.
     * @param ch The character to count the number of instances of in this String.
     * @param fromIndex The index of the first character to begin counting at.  Defaults to zero (ie to the start of the String)
     */
   MUSCLE_NODISCARD uint32 GetNumInstancesOf(char ch, uint32 fromIndex=0) const;

   /** Returns the number of instances of (substring) in this string.
     * @param substring String to count the number of instances of in this String.
     * @param fromIndex The index of the first character to begin counting at.  Defaults to zero (ie to the start of the String)
     */
   MUSCLE_NODISCARD uint32 GetNumInstancesOf(const String & substring, uint32 fromIndex=0) const;

   /** Returns the number of instances of (substring) in this string.
     * @param substring C string to count the number of instances of in this String.
     * @param fromIndex The index of the first character to begin counting at.  Defaults to zero (ie to the start of the String)
     */
   MUSCLE_NODISCARD uint32 GetNumInstancesOf(const char * substring, uint32 fromIndex=0) const;

   /** Returns the Levenshtein distance between this string and (otherString).
     * @param otherString a String to calculate our Levenshtein distance to.
     * @param maxResult The maximum score to return.  (Setting a maximum score to return can
     *                  speed up execution time, as it allows this method to return early
     *                  as soon as the maximum score has been reached).  Defaults to MUSCLE_NO_LIMIT.
     * @returns The Levenshtein distance -- ie the number of single-character insertions, deletions,
     *          or character replacements it would take to convert one string into the other.
     */
   MUSCLE_NODISCARD uint32 GetDistanceTo(const String & otherString, uint32 maxResult = MUSCLE_NO_LIMIT) const;

   /** Returns the Levenshtein distance between this string and (otherString).
     * @param otherString a C string to calculate our Levenshtein distance to.  NULL pointers are considered a synonym for "".
     * @param maxResult The maximum score to return.  (Setting a maximum score to return can
     *                  speed up execution time, as it allows this method to return early
     *                  as soon as the maximum score has been reached).  Defaults to MUSCLE_NO_LIMIT.
     * @returns The Levenshtein distance -- ie the number of single-character insertions, deletions,
     *          or character replacements it would take to convert one string into the other.
     */
   MUSCLE_NODISCARD uint32 GetDistanceTo(const char * otherString, uint32 maxResult = MUSCLE_NO_LIMIT) const;

   /** Returns true iff this string starts with (c)
     * @param c The character to see if this string starts with or not
     */
   MUSCLE_NODISCARD bool StartsWith(char c) const {return (*Cstr() == c);}

   /** Returns true iff this string starts with (prefix)
     * @param prefix The prefix to see whether this string starts with or not
     */
   MUSCLE_NODISCARD bool StartsWith(const String & prefix) const {return StrStartsWith(Cstr(), Length(), prefix(), prefix.Length());}

   /** Returns true iff this string starts with (prefix)
     * @param prefix Pointer to a C string to compare to.  NULL pointers are considered a synonym for "".
     */
   MUSCLE_NODISCARD bool StartsWith(const char * prefix) const {return StrStartsWith(Cstr(), Length(), prefix, prefix?(uint32)strlen(prefix):0);}

   /** Returns a String that consists of some or all the chars in (str), followed by this String.
     * @param str The string to prepend
     * @param maxCharsToPrepend maximum number of characters to prepend.  Fewer chars may
     *                          be prepended if str.Length() is shorter than this value.
     *                          Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithPrepend(const String & str, uint32 maxCharsToPrepend = MUSCLE_NO_LIMIT) const {return WithInsert(0, str, maxCharsToPrepend);}

   /** Returns a String that consists of some or all the chars in (str), followed by this String.
     * @param str Pointer to a C string to prepend.  NULL pointers are considered a synonym for "".
     * @param maxCharsToPrepend maximum number of characters to prepend.  Fewer chars may
     *                          be prepended if strlen(str) is shorter than this value.
     *                          Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithPrepend(const char * str, uint32 maxCharsToPrepend = MUSCLE_NO_LIMIT) const {return WithInsert(0, str, maxCharsToPrepend);}

   /** Returns a String that consists of (count) copies of (c), followed by this string.
     * @param c The character to prepend.
     * @param count How many instances of (c) to prepend.  Defaults to 1.
     * @returns the resulting String
     */
   String WithPrepend(char c, uint32 count = 1) const {return WithInsertAux(0, c, count);}

   /** Similar to WithPrepend(), but this version will insert a separator string between our current content and the prepended string, if necessary.
     * @param str A string to prepended to the end of this string.
     * @param sep Pointer to the string used to separate words.  Defaults to " "
     * @returns the resulting String
     */
   String WithPrependedWord(const String & str, const char * sep = " ") const {return WithInsertedWord(0, str, sep);}

   /** Prepends up to (maxCharsToPrepend) chars from the given C string to this String.
     * @param str Pointer to the C string to prepend
     * @param maxCharsToPrepend maximum number of characters to prepend.  Fewer chars may
     *                          be prepended if strlen(str) is shorter than this value.
     *                          Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or an error code (B_OUT_OF_MEMORY?) on failure.
     */
   status_t PrependChars(const char * str, uint32 maxCharsToPrepend = MUSCLE_NO_LIMIT) {return InsertChars(0, str, maxCharsToPrepend);}

   /** Returns a String that consists of this String followed by some or all of the chars in (str).
     * @param str A string to append to the end of this string.
     * @param maxCharsToAppend maximum number of characters to append.  Fewer chars may be appended
     *                         if str.Length() is less than this value.  Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithAppend(const String & str, uint32 maxCharsToAppend = MUSCLE_NO_LIMIT) const {return WithInsert(MUSCLE_NO_LIMIT, str, maxCharsToAppend);}

   /** Returns a String that consists of this String followed by some or all of the chars in (str).
     * @param str Pointer to a C string to append.  NULL pointers are considered a synonym for "".
     * @param maxCharsToAppend maximum number of characters to append.  Fewer chars may be appended
     *                         if strlen(str) is less than this value.  Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithAppend(const char * str, uint32 maxCharsToAppend = MUSCLE_NO_LIMIT) const {return WithInsert(MUSCLE_NO_LIMIT, str, maxCharsToAppend);}

   /** Returns a String that consists of this string followed by (count) copies of (c).
     * @param c The character to append
     * @param count How many instances of (c) to append.  Defaults to 1.
     * @returns the resulting String
     */
   String WithAppend(char c, uint32 count = 1) const {return WithInsertAux(MUSCLE_NO_LIMIT, c, count);}

   /** Similar to WithAppend(), but this version will insert a separator between our current content and the appended string, if necessary.
     * @param str Pointer to a C string to return appended to this string.  NULL pointers are considered a synonym for "".
     * @param sep Pointer to the string used to separate the appended content from the existing content, in the returned String.  Defaults to " "
     * @returns the resulting String
     */
   String WithAppendedWord(const char * str, const char * sep = " ") const {return WithInsertedWord(MUSCLE_NO_LIMIT, str, sep);}

   /** Similar to WithAppend(), but this version will insert a separator between our current content and the appended string, if necessary.
     * @param str A string to append to the end of this string.
     * @param sep Pointer to the string used to separate the appended content from the existing content, in the returned String.  Defaults to " "
     * @returns the resulting String
     */
   String WithAppendedWord(const String & str, const char * sep = " ") const {return WithInsertedWord(MUSCLE_NO_LIMIT, str, sep);}

   /** Appends up to (maxCharsToAppend) chars from the given C string to this String.
     * @param str Pointer to the C string to append.  NULL pointers are considered a synonym for "".
     * @param maxCharsToAppend maximum number of characters to prepend.  Fewer chars may
     *                         be prepended if strlen(str) is shorter than this value.
     *                         Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or an error code (B_OUT_OF_MEMORY?) on failure.
     */
   status_t AppendChars(const char * str, uint32 maxCharsToAppend = MUSCLE_NO_LIMIT) {return InsertChars(MUSCLE_NO_LIMIT, str, maxCharsToAppend);}

   /** Returns a String that is equal to this String but with some or all of the chars in (str) inserted into it.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param str The String to insert
     * @param maxCharsToInsert maximum number of characters to insert.  Fewer chars may be inserted
     *                         if str.Length() is less than this value.  Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithInsert(uint32 insertAtIdx, const String & str, uint32 maxCharsToInsert = MUSCLE_NO_LIMIT) const {String ret = *this; (void) ret.InsertCharsAux(insertAtIdx, str(), muscleMin(str.Length(),maxCharsToInsert), 1); return ret;}

   /** Returns a String that is equal to this String but with some or all of the chars in (str) inserted into it.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param str Pointer to a C string to insert.  NULL pointers are considered a synonym for "".
     * @param maxCharsToInsert maximum number of characters to insert.  Fewer chars may be inserted
     *                         if strlen(str) is less than this value.  Defaults to MUSCLE_NO_LIMIT.
     * @returns the resulting String
     */
   String WithInsert(uint32 insertAtIdx, const char * str, uint32 maxCharsToInsert = MUSCLE_NO_LIMIT) const {String ret = *this; (void) ret.InsertCharsAux(insertAtIdx, str, str?muscleMin((uint32)strlen(str),maxCharsToInsert):0, 1); return ret;}

   /** Returns a String that is equal to this String but with zero or more copies of (c) inserted into it.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param c The character to insert
     * @param count How many instances of (c) to insert.  Defaults to 1.
     * @returns the resulting String
     */
   String WithInsert(uint32 insertAtIdx, char c, uint32 count = 1) const {return WithInsertAux(insertAtIdx, c, count);}

   /** Similar to WithInsert(), but this version will insert separator strings between our current content and the inserted string, if necessary.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param str Pointer to the C string to insert
     * @param sep Pointer to the string used to separate words.  Defaults to " "
     * @returns the resulting String
     */
   String WithInsertedWord(uint32 insertAtIdx, const String & str, const char * sep = " ") const {return WithInsertedWordAux(insertAtIdx, str(), str.Length(), sep);}

   /** Similar to WithInsert(), but this version will insert separator strings between our current content and the inserted string, if necessary.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param str Pointer to the C string to insert.  NULL pointers are considered a synonym for "".
     * @param sep Pointer to the string used to separate words.  Defaults to " "
     * @returns the resulting String
     */
   String WithInsertedWord(uint32 insertAtIdx, const char * str, const char * sep = " ") const {return WithInsertedWordAux(insertAtIdx, str, str?(uint32)strlen(str):0, sep);}

   /** Inserts up to (maxCharsToAppend) chars from the given C string into the given offset
     * inside this String.
     * @param insertAtIdx Position within this string to insert (0 == prepend, 1 == after the first
     *                    character, and so on).  If this value is greater than or equal to the
     *                    number of characters in this String, the new characters will be appended.
     * @param str Pointer to the C string to insert.  NULL pointers are considered a synonym for "".
     * @param maxCharsToInsert maximum number of characters to insert.  Fewer chars than this
     *                         may be prepended if strlen(str) is less than this value.
     *                         Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or an error code (B_OUT_OF_MEMORY?) on failure.
     */
   status_t InsertChars(uint32 insertAtIdx, const char * str, uint32 maxCharsToInsert = MUSCLE_NO_LIMIT);

   /** Returns a String that is like this string, but padded out to the specified minimum length with (padChar).
    *  @param minLength Minimum length that the returned string should be.
    *  @param padOnRight If true, (padChar)s will be added to the right; if false (the default), they will be added on the left.
    *  @param padChar The character to pad out the string with.  Defaults to ' '.
    *  @returns the new, padded String.
    */
   String PaddedBy(uint32 minLength, bool padOnRight = false, char padChar = ' ') const;

   /** Returns a String that is the same as this one, except that the beginning of each line in the string has (numIndentChars)
     * instances of (indentChar) prepended to it.
     * @param numIndentChars How many indent characters to prepend to each line
     * @param indentChar The character to use to make the indentations.  Defaults to ' '.
     * @returns the indented string.
     */
   String IndentedBy(uint32 numIndentChars, char indentChar = ' ') const;

   /** Returns a String that consists of only the last part of this string, starting with index (beginIndex).  Does not modify the string it is called on.
     * @param beginIndex the index of the first character to include in the returned substring
     */
   String Substring(uint32 beginIndex) const {return String(*this, beginIndex);}

   /** Returns a String that consists of only the characters in this string from range (beginIndex) to (endIndex-1).  Does not modify the string it is called on.
     * @param beginIndex the index of the first character to include in the returned substring
     * @param endIndex the index after the final character to include in the returned substring (if set to MUSCLE_NO_LIMIT, or any other too-large-value,
     *                 returned substring will include thi entire string starting with (beginIndex)
     */
   String Substring(uint32 beginIndex, uint32 endIndex) const {return String(*this, beginIndex, endIndex);}

   /** Returns a String that consists of only the last part of this string, starting with the first character after the last instance of (markerString).
    *  If (markerString) is not found in the string, then this entire String is returned.
    *  For example, String("this is a test").Substring("is a") returns " test".
    *  Does not modify the string it is called on.
    *  @param markerString the substring to return the suffix string that follows
    */
   String Substring(const String & markerString) const
   {
      const int idx = LastIndexOf(markerString);
      return (idx >= 0) ? String(*this, idx+markerString.Length()) : *this;
   }

   /** @copydoc String::Substring(const String &) const */
   String Substring(const char * markerString) const
   {
      const int idx = LastIndexOf(markerString);
      return (idx >= 0) ? String(*this, idx+(int)strlen(markerString)) : *this;  // if (idx >= 0), then we know markerString is non-NULL
   }

   /** Returns a String that consists of only the characters in the string from range (beginIndex) until the character just before
    *  the first character in (markerString).  If (markerString) is not found, then the entire substring starting at (beginIndex) is returned.
    *  For example, String("this is a test").Substring(1, "is a") returns "his ".
    *  Does not modify the string it is called on.
    *  @param beginIndex the first character in our string to search at
    *  @param markerString the substring to return the suffix string that follows
    */
   String Substring(uint32 beginIndex, const String & markerString) const {return String(*this, beginIndex, (uint32) IndexOf(markerString, beginIndex));}

   /** @copydoc String::Substring(uint32, const String &) const */
   String Substring(uint32 beginIndex, const char * markerString) const {return String(*this, beginIndex, (uint32) IndexOf(markerString, beginIndex));}

   /** Returns an all lower-case version of this string.  Does not modify the string it is called on. */
   String ToLowerCase() const;

   /** Returns an all upper-case version of this string.  Does not modify the string it is called on. */
   String ToUpperCase() const;

   /** Returns a version of this string where All Words Are In Lower Case Except For The First Letter. */
   String ToMixedCase() const;

   /** Returns an version of this string that has all leading and trailing whitespace removed.  Does not modify the string it is called on. */
   String Trimmed() const;

   /** Swaps the state of this string with (swapWithMe).  Very efficient since little or no data copying is required.
     * @param swapWithMe the String to swap contents with
     */
   inline void SwapContents(String & swapWithMe) MUSCLE_NOEXCEPT {muscleSwap(_stringData, swapWithMe._stringData);}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   String(String && rhs) MUSCLE_NOEXCEPT
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_MOVE_CTOR);
      ClearShortStringBuffer();
      SwapContents(rhs);
   }

   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   String & operator =(String && rhs) MUSCLE_NOEXCEPT
   {
      MUSCLE_INCREMENT_STRING_OP_COUNT(STRING_OP_MOVE_FROM_STRING);
      SwapContents(rhs);
      return *this;
   }
#endif

   /** Like CompareTo(), but case insensitive.
     * @param s the String to compare this string to
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int CompareToIgnoreCase(const String & s) const {return Strcasecmp(Cstr(), s());}

   /** Like CompareTo(), but case insensitive.
     * @param s the String to compare this string to
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int CompareToIgnoreCase(const char * s) const {return Strcasecmp(Cstr(), s?s:"");}

   /** Like NumericAwareCompareTo(), but case insensitive.
     * @param s the String to compare this string to
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int NumericAwareCompareToIgnoreCase(const String & s) const {return NumericAwareStrcasecmp(Cstr(), s());}

   /** Like NumericAwareCompareTo(), but case insensitive.
     * @param s the String to compare this string to
     * @returns 0 if the two strings are equal, a negative value if this string is "first", or a positive value of (rhs) is "first"
     */
   MUSCLE_NODISCARD int NumericAwareCompareToIgnoreCase(const char * s) const {return NumericAwareStrcasecmp(Cstr(), s?s:"");}

   /** Like EndsWith(), but case insensitive.
     * @param c a character to check for at the end of this String.
     */
   MUSCLE_NODISCARD bool EndsWithIgnoreCase(char c) const {return (HasChars())&&(muscleToLower(Cstr()[Length()-1]) == muscleToLower(c));}

   /** Like EndsWith(), but case insensitive.
     * @param s a suffix to check for at the end of this String.
     */
   MUSCLE_NODISCARD bool EndsWithIgnoreCase(const String & s) const {return StrEndsWithIgnoreCase(Cstr(), Length(), s(), s.Length());}

   /** Like EndsWith(), but case insensitive.
     * @param s a suffix to check for at the end of this String.
     */
   MUSCLE_NODISCARD bool EndsWithIgnoreCase(const char * s) const {return StrEndsWithIgnoreCase(Cstr(), Length(), s, s?(uint32)strlen(s):0);}

   /** Like Equals(), but case insensitive.
     * @param s a string to check for (case-insensitive) equality with this String
     */
   MUSCLE_NODISCARD bool EqualsIgnoreCase(const String & s) const {return ((this==&s)||((Length()==s.Length())&&(Strcasecmp(Cstr(), s()) == 0)));}

   /** Like Equals(), but case insensitive.
     * @param s a string to check for (case-insensitive) equality with this String
     */
   MUSCLE_NODISCARD bool EqualsIgnoreCase(const char * s) const {if (s==NULL) s=""; return ((Cstr()==s)||(Strcasecmp(Cstr(), s) == 0));}

   /** Like Equals(), but case insensitive.
     * @param c a character to check for (case-insensitive) equality with this String.
     */
   MUSCLE_NODISCARD bool EqualsIgnoreCase(char c) const {return ((Length()==1)&&(muscleToLower(Cstr()[0])==muscleToLower(c)));}

   /** Like Contains(), but case insensitive.
     * @param s A String to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD bool ContainsIgnoreCase(const String & s, uint32 f = 0) const {return (IndexOfIgnoreCase(s, f) >= 0);}

   /** Like Contains(), but case insensitive.
     * @param s A String to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD bool ContainsIgnoreCase(const char * s, uint32 f = 0) const {return (IndexOfIgnoreCase(s, f) >= 0);}

   /** Like Contains(), but case insensitive.
     * @param ch A character to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD bool ContainsIgnoreCase(char ch, uint32 f = 0) const {return (IndexOfIgnoreCase(ch, f) >= 0);}

   /** Like IndexOf(), but case insensitive.
     * @param s A String to look for in this string.
     * @param f Index of the first character to start searching at in this String.
     */
   MUSCLE_NODISCARD int IndexOfIgnoreCase(const String & s, uint32 f = 0) const;

   /** Like IndexOf(), but case insensitive.
     * @param s A string to look for in this string.
     * @param f Index of the first character to start searching at in this String.
     */
   MUSCLE_NODISCARD int IndexOfIgnoreCase(const char * s, uint32 f = 0) const;

   /** Like IndexOf(), but case insensitive.
     * @param ch A character to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD int IndexOfIgnoreCase(char ch, uint32 f = 0) const;

   /** Like LastIndexOf(), but case insensitive.
     * @param s A String to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD int LastIndexOfIgnoreCase(const String & s, uint32 f = 0) const;

   /** Like LastIndexOf(), but case insensitive.
     * @param s A string to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD int LastIndexOfIgnoreCase(const char * s, uint32 f = 0) const;

   /** Like LastIndexOf(), but case insensitive.
     * @param ch A character to look for in this string.
     * @param f Index of the first character to start searching at in this String.  Defaults to zero.
     */
   MUSCLE_NODISCARD int LastIndexOfIgnoreCase(char ch, uint32 f = 0) const;

   /** Like StartsWith(), but case insensitive.
     * @param c The character to see if this string starts with or not
     */
   MUSCLE_NODISCARD bool StartsWithIgnoreCase(char c) const {return ((Length() > 0)&&(muscleToLower(Cstr()[0]) == muscleToLower(c)));}

   /** Like StartsWith(), but case insensitive.
     * @param s The prefix to see whether this string starts with or not
     */
   MUSCLE_NODISCARD bool StartsWithIgnoreCase(const String & s) const {return StrStartsWithIgnoreCase(Cstr(), Length(), s(), s.Length());}

   /** Like StartsWith(), but case insensitive.
     * @param s The prefix to see whether this string starts with or not
     */
   MUSCLE_NODISCARD bool StartsWithIgnoreCase(const char * s) const {return StrStartsWithIgnoreCase(Cstr(), Length(), s, s?(uint32)strlen(s):0);}

   /** @copydoc DoxyTemplate::HashCode() const */
   MUSCLE_NODISCARD inline uint32 HashCode() const {return CalculateHashCode(Cstr(), Length());}

   /** @copydoc DoxyTemplate::HashCode64() const */
   MUSCLE_NODISCARD inline uint64 HashCode64() const {return CalculateHashCode64(Cstr(), Length());}

   /** Replaces all instances of (oldChar) in this string with (newChar).
     * @param replaceMe The character to search for.
     * @param withMe The character to replace all occurrences of (replaceMe) with.
     * @param maxReplaceCount The maximum number of characters that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @param fromIndex Index of the first character to start replacing characters at.  Defaults to 0.
     * @returns The number of characters that were successfully replaced.
     */
   uint32 Replace(char replaceMe, char withMe, uint32 maxReplaceCount = MUSCLE_NO_LIMIT, uint32 fromIndex=0);

   /** Same as Replace(), but instead of modifying this object, it returns a modified copy, and the called object remains unchanged.
     * @param replaceMe The character to search for.
     * @param withMe The character to replace all occurrences of (replaceMe) with.
     * @param maxReplaceCount The maximum number of characters that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @param fromIndex Index of the first character to start replacing characters at.  Defaults to 0.
     * @returns The modified string.
     */
   String WithReplacements(char replaceMe, char withMe, uint32 maxReplaceCount = MUSCLE_NO_LIMIT, uint32 fromIndex=0) const;

   /** Replaces all instances of (replaceMe) in this string with (withMe).
     * @param replaceMe The substring to search for.
     * @param withMe The substring to replace all occurrences of (replaceMe) with.
     * @param maxReplaceCount The maximum number of substrings that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @param fromIndex Index of the first character to start replacing characters at.  Defaults to 0.
     * @returns The number of substrings that were successfully replaced, or -1 if the operation failed (out of memory)
     */
   int32 Replace(const String & replaceMe, const String & withMe, uint32 maxReplaceCount = MUSCLE_NO_LIMIT, uint32 fromIndex=0);

   /** Same as Replace(), but instead of modifying this object, it returns a modified copy, and the called object remains unchanged.
     * @param replaceMe The substring to search for.
     * @param withMe The substring to replace all occurrences of (replaceMe) with.
     * @param maxReplaceCount The maximum number of substrings that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @param fromIndex Index of the first character to start replacing characters at.  Defaults to 0.
     * @returns The modified string.
     */
   String WithReplacements(const String & replaceMe, const String & withMe, uint32 maxReplaceCount = MUSCLE_NO_LIMIT, uint32 fromIndex=0) const;

   /** Replaces all instances in this String of each key in (beforeToAfter) with its corresponding value.
     * @param beforeToAfter a table of before/after pairs to replace.
     * @param maxReplaceCount The maximum number of substrings that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @returns the number of strings that were replaced, or -1 if there was an error (out of memory?)
     * @see WithReplacements(const Hashtable<String, String> &) for more details.
     */
   int32 Replace(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount = MUSCLE_NO_LIMIT);

   /** Returns a String equal to this one, except with a set of search-and-replace operations simultaneously performed.
     * @param beforeToAfter the set of search-and-replace operations to perform.  Each keys in this table is a substring
     *                      to be searched for, and each key's associated value is the string to replace that substring with.
     * @param maxReplaceCount The maximum number of substrings that should be replaced.  Defaults to MUSCLE_NO_LIMIT.
     * @note In the case where a key in the supplied Hashtable is a prefix of another key in the Hashtable,
     *       the key/value pair that comes first in the Hashtable's iteration-order will get priority -- that is, in
     *       any case where either of the two key/value pairs could have been used, the algorithm will apply the key/value
     *       pair that appeared first in the Hashtable's iteration-ordering.
     * @note By performing multiple search-and-replace operations simultaneously, we can give correct results in
     *       certain cases where a series of single-string search-and-replace operations would not.  For example,
     *       String("1,2,3,4").WithReplacements("1","2").WithReplacements("2","3") would unhelpfully return "3,3,3,4"
     *       while the corresponding single call to this method would return the desired result "2,3,3,4".
     */
   String WithReplacements(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount = MUSCLE_NO_LIMIT) const;

   /** Returns a String equal to this one, except with all instances of any of the characters specified in
     * (charsToEscape) escaped by inserting an instance of (escapeChar) just before them (unless they already
     * had an instance of (escapeChar) before them, in which case they will be left as-is)
     * Also any freestanding-instances of (escapeChar) will also be escaped the same way.
     * @param charsToEscape a string containing all of the character(s) you wish to have escaped.
     * @param escapeChar the character to use as the escape character.  Defaults to a backslash ('\\').
     * @returns the escaped String.
     * @note this method is designed for use in combination with the escapeChar feature of the StringTokenizer
     *       class; ie you'd use this method to add escape-characters to literals in a String that you plan
     *       to parse later on using a StringTokenizer to which you have passed the same (escapeChar) argument.
     */
   String WithCharsEscaped(const char * charsToEscape, char escapeChar = '\\') const;

   /** Same as the previous method, except this version lets you specify a single escape-character directly as a char.
     * @param charToEscape the character to you want escaped in the String.
     * @param escapeChar the character to use as the escape character.  Defaults to a backslash ('\\').
     * @returns the escaped String.
     * @note this method is designed for use in combination with the escapeChar feature of the StringTokenizer
     *       class; ie you'd use this method to add escape-characters to literals in a String that you plan
     *       to parse later on using a StringTokenizer to which you have passed the same (escapeChar) argument.
     */
   String WithCharsEscaped(char charToEscape, char escapeChar = '\\') const
   {
      const char s[] = {charToEscape, '\0'};
      return WithCharsEscaped(s, escapeChar);
   }

   /** Reverses the order of all characters in the string, so that eg "string" becomes "gnirts" */
   void Reverse();

   /** Part of the Flattenable pseudo-interface.
    *  @return false
    */
   MUSCLE_NODISCARD bool IsFixedSize() const {return false;}

   /** Part of the Flattenable pseudo-interface.
    *  @return B_STRING_TYPE
    */
   MUSCLE_NODISCARD uint32 TypeCode() const {return B_STRING_TYPE;}

   /** Part of the Flattenable pseudo-interface.
    *  @return Length()+1  (the +1 is for the terminating NUL byte)
    */
   MUSCLE_NODISCARD uint32 FlattenedSize() const {return Length()+1;}

   /** Part of the Flattenable pseudo-interface.  Flattens our string into (buffer).
    *  @param flat the DataFlattener to use to write out the flattened data bytes.
    *  @note The clever secret here is that a flattened String is just a C-style
    *        zero-terminated char array, and can be used interchangably as such.
    */
   void Flatten(DataFlattener flat) const {flat.WriteBytes(reinterpret_cast<const uint8 *>(Cstr()), FlattenedSize());}

   /** Unflattens a String from (buf).
    *  @param unflat the DataUnflattener we will read our string from (as a NUL-terminated ASCII string)
    *  @return B_NO_ERROR on success, or another value on failure.
    */
   status_t Unflatten(DataUnflattener & unflat) {return SetCstr(unflat.ReadCString());}

   /** Makes sure that we have pre-allocated enough space for a NUL-terminated string
    *  at least (numChars) bytes long (not including the NUL byte).
    *  If not, this method will try to allocate the space.
    *  @param numChars How much space to pre-allocate, in ASCII characters.
    *  @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t Prealloc(uint32 numChars) {return EnsureBufferSize(numChars+1, true, false);}

   /** Removes (numCharsToTruncate) characters from the end of this string.
     * @param numCharsToTruncate How many characters to truncate.  If greater than the
     *                           length of this String, this String will become empty.
     */
   void TruncateChars(uint32 numCharsToTruncate) {const uint32 length = Length(); SetLength(length-muscleMin(length, numCharsToTruncate)); WriteNULTerminatorByte();}

   /** Makes sure this string is no longer than (maxLength) characters long by truncating
     * any extra characters, if necessary
     * @param maxLength Maximum length that this string should be allowed to be.
     */
   void TruncateToLength(uint32 maxLength) {SetLength(muscleMin(Length(), maxLength)); WriteNULTerminatorByte();}

   /** Returns a String like this string, but with the appropriate %# tokens
     * replaced with a textual representation of the values passed in as (value).
     * For example, String("%1 is a %2").Arg(13).Arg("bakers dozen") would return
     * the string "13 is a bakers dozen".
     * @param value The value to replace in the string.
     * @param fmt The format parameter to pass to muscleSprintf().  Note that if you supply your
     *            own format string, you'll need to be careful since a badly chosen format
     *            string could lead to a crash or out-of-bounds memory overwrite.  In particular,
     *            the format string should match the type specified, and should not lead to
     *            more than 256 bytes being written.
     * @returns a new String with the appropriate tokens replaced.
     */
   String Arg(bool value, const char * fmt = "%s") const;

   /** As above, but for char values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(char value, const char * fmt = "%i") const;

   /** As above, but for unsigned char values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(unsigned char value, const char * fmt = "%u") const;

   /** As above, but for short values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(short value, const char * fmt = "%i") const;

   /** As above, but for unsigned shot values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(unsigned short value, const char * fmt = "%u") const;

   /** As above, but for int values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(int value, const char * fmt = "%i") const;

   /** As above, but for unsigned int values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(unsigned int value, const char * fmt = "%u") const;

   /** As above, but for long values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(long value, const char * fmt = "%li") const;

   /** As above, but for unsigned long values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(unsigned long value, const char * fmt = "%lu") const;

   /** As above, but for long long values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(long long value, const char * fmt = INT64_FORMAT_SPEC) const;

   /** As above, but for unsigned long long values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(unsigned long long value, const char * fmt = UINT64_FORMAT_SPEC) const;

   /** As above, but for float values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(float value, const char * fmt) const;

   /** As above, but for float values.
     * Returns a String representation of the given floating point value,
     * with up to (maxDigitsAfterDecimal) digits appearing after the decimal point.
     * @param f the floating-point value to return a String representation of.
     * @param minDigitsAfterDecimal the minimum number of digits to display after the decimal
     *                              point.  Defaults to 0 (meaning try to use as few as possible)
     * @param maxDigitsAfterDecimal the maximum number of digits to display after the decimal
     *                              point.  Defaults to MUSCLE_NO_LIMIT (meaning that no particular
     *                              maximum value should be enforced)
     * @note Trailing zeroes will be omitted from the string, as will the decimal point itself, if it
     *       would otherwise be the last character in the String.
     */
   String Arg(float f, uint32 minDigitsAfterDecimal = 0, uint32 maxDigitsAfterDecimal = MUSCLE_NO_LIMIT) const {return Arg((double)f, minDigitsAfterDecimal, maxDigitsAfterDecimal);}

   /** As above, but for double values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(double value, const char * fmt) const;

   /** As above, but for double values.
     * Returns a String representation of the given floating point value,
     * with up to (maxDigitsAfterDecimal) digits appearing after the decimal point.
     * @param f the floating-point value to return a String representation of.
     * @param minDigitsAfterDecimal the minimum number of digits to display after the decimal
     *                              point.  Defaults to 0 (meaning try to use as few as possible)
     * @param maxDigitsAfterDecimal the maximum number of digits to display after the decimal
     *                              point.  Defaults to MUSCLE_NO_LIMIT (meaning that no particular
     *                              maximum value should be enforced)
     * @note Trailing zeroes will be omitted from the string, as will the decimal point itself, if it
     *       would otherwise be the last character in the String.
     */
   String Arg(double f, uint32 minDigitsAfterDecimal = 0, uint32 maxDigitsAfterDecimal = MUSCLE_NO_LIMIT) const;

   /** As above, but for string values.
     * @param value the string to replace any instances of %1 with (or %2 if %1 isn't present, or etc)
     */
   String Arg(const String & value) const;

   /** As above, but for Point values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(const Point & value, const char * fmt = "%f,%f") const;

   /** As above, but for Rect values.
     * @copydoc Arg(bool, const char *) const
     */
   String Arg(const Rect & value, const char * fmt = "%f,%f,%f,%f") const;

   /** As above, but for C-string values.
     * @param value the string to replace any instances of "%1" with (or "%2" if "%1" isn't present, or etc)
     */
   String Arg(const char * value) const;

   /** As above, but for non-const C-string values.
     * @param value the string to replace any instances of "%1" with (or "%2" if "%1" isn't present, or etc)
     */
   String Arg(char * value) const {return Arg((const char *)value);}

   /** As above, but for printing pointer values.
     * @param value the pointer-value whose string representation we should replace any instances of %1 with (or %2 if %1 isn't present, or etc)
     */
   String Arg(const void * value) const;

   /** A templated convenience-method for any class-object with a ToString() method that returns a String.
     * Equivalent to calling Arg(t.ToString()).
     * @param t Reference to the class object to call ToString() on.
     */
   template<class T> String Arg(const T & t) const {return Arg(t.ToString());}

   /** A templated convenience-method for a pointer of any type.
     * @param tptr A pointer of some arbitrary type.
     * @returns the String corresponding to the object (tptr) points to, or an empty String if (tptr) is NULL.
     */
   template<class T> String Arg(const T * tptr) const {return tptr ? Arg(*tptr) : ArgAux("");}

   /** A templated convenience-method for a const pointer of any type.
     * Equivalent to calling Arg(const_cast<const T *>(tpr)).
     * @param tptr A pointer of some arbitrary type.
     * @returns the String corresponding to the object (tptr) points to, or an empty String if (tptr) is NULL.
     */
   template<class T> String Arg(T * tptr) const {return Arg(const_cast<const T *>(tptr));}

   /** If this string already ends with the specified character, returns this string verbatim.
     * Otherwise, returns a String equivalent to this one but with the specified character appended.
     * @param c The char we want to be sure is at the end of the returned String.
     */
   String WithSuffix(char c) const {return EndsWith(c) ? *this : WithAppend(c);}

   /** If this string already ends with the specified string, returns this string verbatim.
     * Otherwise, returns a String equivalent to this one but with the specified string appended.
     * @param str The string we want to be sure is at the end of the returned String.
     */
   String WithSuffix(const String & str) const {return EndsWith(str) ? *this : WithAppend(str);}

   /** If this string already begins with the specified character, returns this string verbatim.
     * Otherwise, returns a String equivalent to this one but with the specified character prepended.
     * @param c The character we want to be sure is at the beginning of the returned String.
     */
   String WithPrefix(char c) const {return StartsWith(c) ? *this : WithPrepend(c);}

   /** If this string already begins with the specified string, returns this string verbatim.
     * Otherwise, returns a String equivalent to this one but with the specified string prepended.
     * @param str The string we want to be sure is at the beginning of the returned String.
     */
   String WithPrefix(const String & str) const {return StartsWith(str) ? *this : WithPrepend(str);}

   /** Returns a String like this one, but with any characters (c) removed from the end.
     * @param c The char we want to be sure is not at the end of the returned String.
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all trailing (c) chars.
     */
   String WithoutSuffix(char c, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any instances of (str) removed from the end.
     * @param str The substring we want to be sure is not at the end of the returned String.
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all trailing (str) substrings.
     * @note if (str) is empty, this method will return (*this).
     */
   String WithoutSuffix(const String & str, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any characters (c) removed from the beginning.
     * @param c The char we want to be sure is not at the beginning of the returned String.
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all starting (c) chars.
     */
   String WithoutPrefix(char c, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any instances of (str) removed from the beginning.
     * @param str The substring we want to be sure is not at the beginning of the returned String.
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all starting (str) substrings.
     * @note if (str) is empty, this method will return (*this).
     */
   String WithoutPrefix(const String & str, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any characters (c) removed from the end.
     * @param c The char we want to be sure is not at the end of the returned String (case-insensitive).
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all trailing (c) chars.
     */
   String WithoutSuffixIgnoreCase(char c, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any instances of (str) removed from the end.
     * @param str The substring we want to be sure is not at the end of the returned String (case-insensitive).
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all trailing (str) substrings.
     * @note if (str) is empty, this method will return (*this).
     */
   String WithoutSuffixIgnoreCase(const String & str, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any characters (c) removed from the beginning.
     * @param c The char we want to be sure is not at the beginning of the returned String (case-insensitive).
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all starting (c) chars.
     */
   String WithoutPrefixIgnoreCase(char c, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String like this one, but with any instances of (str) removed from the beginning.
     * @param str The substring we want to be sure is not at the beginning of the returned String (case-insensitive).
     * @param maxToRemove Maximum number of instances of (c) to remove from the returned String.
     *                    Defaults to MUSCLE_NO_LIMIT, ie remove all starting (str) substrings.
     * @note if (str) is empty, this method will return (*this).
     */
   String WithoutPrefixIgnoreCase(const String & str, uint32 maxToRemove = MUSCLE_NO_LIMIT) const;

   /** Returns a String equal to this one, but with any integer/numeric suffix removed.
     * For example, if you called this method on the String "Joe-54", this method would return "Joe".
     * @param optRetRemovedSuffixValue If non-NULL, the numeric suffix that was removed will be returned here.
     *                                 If there was no numeric suffix, zero will be written here.
     * @note that negative numeric suffixes aren't supported -- ie plus or minus is not considered to be part of the numeric-suffix.
     */
   String WithoutNumericSuffix(uint32 * optRetRemovedSuffixValue = NULL) const;

   /** If this string ends in a numeric value, returns that value; otherwise returns (defaultValue).
     *  For example, ParseNumericSuffix("Joe-54") would return 54.
     *  @param defaultValue the value to return if no numeric suffix is found.  Defaults to zero.
     *  @note that negative numeric suffixes aren't supported -- ie plus or minus is not considered to be part of the numeric-suffix.
     */
   MUSCLE_NODISCARD uint32 ParseNumericSuffix(uint32 defaultValue = 0) const;

   /** Returns a 32-bit checksum corresponding to this String's contents.
     * Note that this method method is O(N).
     */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const {return muscle::CalculateChecksum((const uint8 *) Cstr(), Length());}

   /** Returns true iff the given pointer points into our held character array.
     * @param s A character pointer.  It will not be dereferenced by this call.
     */
   MUSCLE_NODISCARD bool IsCharInLocalArray(const char * s) const {const char * b = Cstr(); return muscleInRange(s, b, b+Length());}

   /** Returns the maximum number of characters of String-data (not including the NUL terminator byte) that a String object
     * can hold without having to do a heap allocation.
     */
   MUSCLE_NODISCARD static inline MUSCLE_CONSTEXPR uint32 GetMaxShortStringLength() {return sizeof(ShortStringData)-1;}  // -1 for _ssoFreeBytesLeft (which is also a NUL byte if need be)

   /** @copydoc DoxyTemplate::Print(const OutputPrinter &) const */
   void Print(const OutputPrinter & p) const {p.printf("%s", Cstr());}

private:
   status_t InsertCharsAux(uint32 insertAtIdx, const char * str, uint32 numCharsToInsert, uint32 insertCount);
   String WithInsertAux(uint32 insertAtIdx, char c, uint32 numChars) const;
   String WithInsertedWordAux(uint32 insertAtIdx, const char * str, uint32 numChars, const char * sep) const;
   MUSCLE_NODISCARD bool IsSpaceChar(char c) const {return ((c==' ')||(c=='\t')||(c=='\r')||(c=='\n'));}
   status_t EnsureBufferSize(uint32 newBufLen, bool retainValue, bool allowShrink);
   String ArgAux(const char * buf) const;
   MUSCLE_NODISCARD bool IsArrayDynamicallyAllocated() const {return (_stringData._shortStringData.IsShortStringDataValid() == false);}
   MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL char * GetBuffer() {return IsArrayDynamicallyAllocated() ? _stringData._longStringData.GetBuffer() : _stringData._shortStringData.GetBuffer();}
   MUSCLE_NODISCARD static uint32 GetNextBufferSize(uint32 bufLen);
   void ClearShortStringBuffer() {_stringData._shortStringData.Clear();}
   void SetLength(uint32 len)
   {
      if (IsArrayDynamicallyAllocated()) _stringData._longStringData.SetLength(len);
                                    else _stringData._shortStringData.SetLength(len);
   }

   void WriteNULTerminatorByte() {GetBuffer()[Length()] = '\0';}
   MUSCLE_NODISCARD int32 ReplaceAux(const Hashtable<String, String> & beforeToAfter, uint32 maxReplaceCount, String & writeTo) const;

   // Our data-layout for non-SSO "long" strings.  The intent is for this to be 16 bytes long (on a 64-bit system)
   struct LongStringData
   {
      void SetLength(uint32 len) {_strlen = len;}
      MUSCLE_NODISCARD uint32 Length() const {return _strlen;}

      MUSCLE_NODISCARD uint32 GetNumAllocatedBytes() const {return B_LENDIAN_TO_HOST_INT32(_encBufLen) & ~((uint32)(1<<31));}

      void SetBuffer(char * newBuffer, uint32 newBufLen, uint32 oldStrlen)
      {
         _bigBuffer = newBuffer;
         _strlen    = oldStrlen;
         _encBufLen = B_HOST_TO_LENDIAN_INT32(newBufLen) | ((uint32)(1<<31));
      }

      void FreeBuffer() {muscleFree(_bigBuffer);}  // no need to clear member-variables as ClearShortStringBuffer() will be called right after this

      MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL char * GetBuffer() {return _bigBuffer;}
      MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * Cstr() const {return _bigBuffer;}

      char * ReallocBuffer(uint32 newLen) {return (char *) muscleRealloc(_bigBuffer, newLen);}

   private:
      char * _bigBuffer;  // pointer to our heap-allocated buffer
      uint32 _strlen;     // cached value of strlen(_bigBuffer)
      uint32 _encBufLen;  // number of bytes pointed to by (_bigBuffer).  Aliases with _ssoFreeBytesLeft, so it must be last in the struct, must be little-endian, and its high bit must be set!
   };

   class ShortStringDataSizeChecker;

   // Our data-layout for SSO "short" strings.
   struct ShortStringData
   {
      MUSCLE_NODISCARD bool IsShortStringDataValid() const {return ((_ssoFreeBytesLeft & 0x80) == 0);}  // if the high bit is set that means the String is in long-data mode

      void Clear()
      {
         _smallBuffer[0]   = '\0';                      // NUL-terminate the SSO string buffer
         _ssoFreeBytesLeft = GetMaxShortStringLength(); // all bytes available
      }

      void SetLength(uint32 len) {_ssoFreeBytesLeft = (uint8) (((uint32)sizeof(_smallBuffer))-len);}
      MUSCLE_NODISCARD uint32 Length() const {return ((uint32)sizeof(_smallBuffer))-_ssoFreeBytesLeft;}

      MUSCLE_NODISCARD uint32 GetNumAllocatedBytes() const {return sizeof(*this);}  // because our _ssoFreeBytesLeft byte can double as a NUL byte
      MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL char * GetBuffer() {return _smallBuffer;}
      MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * Cstr() const {return _smallBuffer;}

      void SetBuffer(const char * srcBytes, uint32 srcStrlen)
      {
         memcpy(_smallBuffer, srcBytes, srcStrlen);
         _smallBuffer[srcStrlen] = '\0';  // make sure we're NUL terminated (could be an issue if we're shrinking)
         SetLength(srcStrlen);
      }

      void Truncate(uint32 newStrlen)
      {
         _smallBuffer[newStrlen] = '\0';
         SetLength(muscleMin(newStrlen, Length()));
      }

   private:
      friend class ShortStringDataSizeChecker;

      char _smallBuffer[sizeof(LongStringData)-sizeof(uint8)];
      uint8 _ssoFreeBytesLeft;  // Aliases with most-significant-byte of _encBufLen; 0 when _smallBuffer is full of chars, it's also the NUL byte
   };

#if !defined(MUSCLE_AVOID_CPLUSPLUS11)
   class ShortStringDataSizeChecker
   {
   private:
      static_assert(sizeof(ShortStringData) == sizeof(LongStringData), "sizeof(ShortStringData) != sizeof(LongStringData)");
      static_assert(sizeof(ShortStringData) == sizeof(ShortStringData::_smallBuffer)+1, "sizeof(ShortStringData) != sizeof(ShortStringData::_smallBuffer)+1");
   };
#endif

#ifdef __clang_analyzer__
   struct StringData {  // ClangSA gets confused by unions, so we'll avoid SSO during Clang analysis
#else
   union StringData {
#endif
      LongStringData  _longStringData;
      ShortStringData _shortStringData;
   } _stringData;

   void VerifyIndex(uint32 index) const
   {
#ifdef MUSCLE_AVOID_ASSERTIONS
      (void) index;  // avoid compiler warnings
#else
      MASSERT(index < Length(), "String Index Out Of Bounds");
#endif
   }
};

/** A custom compare functor for case-insensitive comparisons of Strings. */
class MUSCLE_NODISCARD CaseInsensitiveStringCompareFunctor
{
public:
   /** Compares the two String's strcmp() style, returning zero if they are equal, a negative value if (item1) comes first, or a positive value if (item2) comes first.
     * @param s1 the first String to compare
     * @param s2 the second String to compare
     */
   MUSCLE_NODISCARD int Compare(const String & s1, const String & s2, void *) const {return s1.CompareToIgnoreCase(s2);}
};

/** A custom compare functor for numeric-aware comparisons of Strings. */
class MUSCLE_NODISCARD NumericAwareStringCompareFunctor
{
public:
   /** Compares the two String's strcmp() style, returning zero if they are equal, a negative value if (item1) comes first, or a positive value if (item2) comes first.
     * @param s1 the first String to compare
     * @param s2 the second String to compare
     */
   MUSCLE_NODISCARD int Compare(const String & s1, const String & s2, void *) const {return s1.NumericAwareCompareTo(s2);}
};

/** A custom compare functor for case-insensitive numeric-aware comparisons of Strings. */
class MUSCLE_NODISCARD CaseInsensitiveNumericAwareStringCompareFunctor
{
public:
   /** Compares the two String's strcmp() style, returning zero if they are equal, a negative value if (item1) comes first, or a positive value if (item2) comes first.
     * @param s1 the first String to compare
     * @param s2 the second String to compare
     */
   MUSCLE_NODISCARD int Compare(const String & s1, const String & s2, void *) const {return s1.NumericAwareCompareToIgnoreCase(s2);}
};

/** Convenience method:  returns a string with no characters in it (a.k.a. "") */
inline const String & GetEmptyString() {return GetDefaultObjectForType<String>();}

template<class T> inline String operator+(const String & lhs, const T & rhs) {return rhs.ToString().WithPrepend(lhs);}
inline String operator+(const String & lhs, const String & rhs)  {String ret; (void) ret.Prealloc(lhs.Length()+rhs.Length());                ret = lhs; ret += rhs; return ret;}
inline String operator+(const String & lhs, const char *rhs)     {String ret; (void) ret.Prealloc(lhs.Length()+(rhs?(uint32)strlen(rhs):0)); ret = lhs; ret += rhs; return ret;}
inline String operator+(const String & lhs,       char *rhs)     {String ret; (void) ret.Prealloc(lhs.Length()+(rhs?(uint32)strlen(rhs):0)); ret = lhs; ret += rhs; return ret;}
inline String operator+(const char * lhs,   const String & rhs)  {String ret; (void) ret.Prealloc((lhs?(uint32)strlen(lhs):0)+rhs.Length()); ret = lhs; ret += rhs; return ret;}
inline String operator+(      char * lhs,   const String & rhs)  {String ret; (void) ret.Prealloc((lhs?(uint32)strlen(lhs):0)+rhs.Length()); ret = lhs; ret += rhs; return ret;}
inline String operator+(const String & lhs, char rhs)            {String ret; (void) ret.Prealloc(lhs.Length()+1);                           ret = lhs; ret += rhs; return ret;}
inline String operator+(char lhs,           const String & rhs)  {String ret; (void) ret.Prealloc(1+rhs.Length());         (void) ret.SetCstr(&lhs, 1); ret += rhs; return ret;}
template<class T> inline String operator-(const String & lhs, const T & rhs) {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(const String & lhs, const String & rhs)  {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(const String & lhs, const char *rhs)     {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(const String & lhs,       char *rhs)     {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(const char *lhs,    const String & rhs)  {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(      char *lhs,    const String & rhs)  {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(const String & lhs, char rhs)            {String ret = lhs; ret -= rhs; return ret;}
inline String operator-(char lhs,           const String & rhs)  {String ret; (void) ret.SetCstr(&lhs, 1); ret -= rhs; return ret;}

/** Convenience function (so I can just type String s = ToString(obj) instead of obj.Print(s) if I want)
  * @param t the object to call Print(s) on
  * @returns the resulting String object
  */
template<class T> String ToString(const T & t) {String s; t.Print(s); return s;}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
/** Convenience function (so I can just type ToString(obj) instead of obj.Print(s) if I want)
  * @param t the object to call Print() on
  * @param args some additional arguments that should also be forwarded verbatim to the Print() call
  * @returns the resulting String object
  */
template<class T, typename... Args> String ToString(const T & t, Args&&... args)
{
   String s;
   t.Print(s, std::forward<Args>(args)...);
   return s;
}
#endif

} // end namespace muscle

#endif
