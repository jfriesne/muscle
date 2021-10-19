/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStringTokenizer_h
#define MuscleStringTokenizer_h

#include "support/MuscleSupport.h"

namespace muscle {

/** String tokenizer class, similar to Java's java.util.StringTokenizer.  This class is used
  * to interpret a specified character string as a series of sub-strings, with each sub-string
  * differentiated from its neighbors by the presence of one or more of the specified separator-tokens 
  * between the two sub-strings.
  */
class StringTokenizer MUSCLE_FINAL_CLASS
{
public:
   /** Initializes the StringTokenizer to parse (tokenizeMe), which should be a string of tokens (e.g. words) separated by any 
    *  of the characters specified in (separators)
    *  @param tokenizeMe the string to tokenize.  If NULL is passed in, an empty string ("") is assumed.
    *  @param hardSeparators ASCII string representing a list of characters to interpret as "hard" substring-separators.
    *                        Defaults to ","; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param softSeparators ASCII string representing a list of characters to interpret as "soft" substring-separators.
    *                        Defaults to " \t\r\n"; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param escapeChar If specified as non-zero, separator-chars appearing immediately after this char will not be used as separators.
    *                    Defaults to zero.  (Note that the escapeChar will still appear in the tokenized substrings, though!)
    *  @note the difference between a "soft" separator and a "hard" separator is that a contiguous series of soft separators
    *        is counted as a single separator, while a contiguous series of hard separators is counted as separating one or
    *        more empty strings.  For example, given the default separator-arguments listed above, ",A,B,,,C,D" would be tokenized 
    *        as ("", "A", "B", "", "", "C", "D"), while "  A B  C   D  " would be tokenized as ("A", "B", "C", "D").
    */
   StringTokenizer(const char * tokenizeMe, const char * hardSeparators = ",", const char * softSeparators = " \t\r\n", char escapeChar = '\0');

   /** This Constructor is the same as above, only with this one you allow the StringTokenizer to modify (tokenizeMe) directly, 
    *  rather than making a copy of the string first and modifying that.  (it's a bit more efficient as no dynamic allocations
    *  need to be done; however it does modify (tokenizeMe) in the process)
    *  @param junk Ignored; it's only here to disambiguate the two constructors.
    *  @param tokenizeMe The string to tokenize.  This string will get munged!
    *  @param hardSeparators ASCII string representing a list of characters to interpret as "hard" substring-separators.
    *                        Defaults to ","; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param softSeparators ASCII string representing a list of characters to interpret as "soft" substring-separators.
    *                        Defaults to " \t\r\n"; passing in NULL is the same as passing in "" (i.e. no separators of this type).
    *  @param escapeChar If specified as non-zero, separator-chars appearing immediately after this char will not be used as separators.
    *                    Defaults to zero.  (Note that the escapeChar will still appear in the tokenized substrings, though!)
    *  @note the difference between a "soft" separator and a "hard" separator is that a contiguous series of soft separators
    *        is counted as a single separator, while a contiguous series of hard separators is counted as separating one or
    *        more empty strings.  For example, given the default separator-arguments listed above, ",A,B,,,C,D" would be tokenized 
    *        as ("", "A", "B", "", "", "C", "D"), while "  A B  C   D  " would be tokenized as ("A", "B", "C", "D").
    */
   StringTokenizer(bool junk, char * tokenizeMe, const char * hardSeparators = ",", const char * softSeparators = " \t\r\n", char escapeChar = '\0');

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   StringTokenizer(const StringTokenizer & rhs);

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   StringTokenizer & operator = (const StringTokenizer & rhs);

   /** Destructor */
   ~StringTokenizer();

   /** Returns the next token in the parsed string, or NULL if there are no more tokens left */
   char * GetNextToken();

   /** Convenience synonym for GetNextToken() */
   char * operator()() {return GetNextToken();}
 
   /** Returns the remainder of the string, starting with the next token,
    *  or NULL if there are no more tokens in the string.
    *  Doesn't affect the next return value of GetNextToken(), though.
    */
   char * GetRemainderOfString();

   /** Returns a string representing the set of hard-separator-chars we are using (or "" if none) */
   const char * GetHardSeparatorChars() const {return _hardSeparators;}

   /** Returns a string representing the set of soft-separator-chars we are using (or "" if none) */
   const char * GetSoftSeparatorChars() const {return _softSeparators;}

   /** Returns the separator-escape character that was passed in to our constructor (or '\0' if none) */
   char GetEscapeChar() const {return _escapeChar;}

private:
   bool IsHardSeparatorChar(char prevChar, char c) const {return (((_escapeChar=='\0')||(prevChar!=_escapeChar))&&(strchr(_hardSeparators, c) != NULL));}
   bool IsSoftSeparatorChar(char prevChar, char c) const {return (((_escapeChar=='\0')||(prevChar!=_escapeChar))&&(strchr(_softSeparators, c) != NULL));}

   void DefaultInitialize();
   void DeletePrivateBufferIfNecessary();
   void CopyDataToPrivateBuffer(const StringTokenizer & copyFrom);
   void SetPointersAnalogousTo(char * myNewBuf, const StringTokenizer & copyFrom);
   void MovePastSoftSeparatorChars() {while((*_next)&&(IsSoftSeparatorChar(_prevChar, *_next))) UpdatePrevChar(*_next++);}
   void UpdatePrevChar(char c) {_prevChar = (_prevChar==_escapeChar)?'\0':c;}

   bool _allocedBufferOnHeap;
   bool _prevSepWasHard;
   char _escapeChar;
   char _prevChar;

   uint32 _bufLen;

   const char * _hardSeparators;
   const char * _softSeparators;
   char * _next;

   char _smallStringBuf[128];
};

} // end namespace muscle

#endif
