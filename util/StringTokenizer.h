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
   /** Initializes the StringTokenizer to parse (tokenizeMe), which 
    *  should be a string of tokens (e.g. words) separated by any 
    *  of the characters specified in (separators)
    *  @param tokenizeMe the string to break up into 'words'.
    *  @param separators ASCII string representing a list of characters to interpret a substring-separators.
    *                    Defaults to ", \t\r\n".
    */
   StringTokenizer(const char * tokenizeMe, const char * separators = ", \t\r\n") : _alloced(false)
   {
      const size_t tlen   = strlen(tokenizeMe);
      const size_t slen   = strlen(separators);
      const size_t bufLen = slen+1+tlen+1;
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
         muscleStrncpy(temp, separators, bufLen);
         _seps = temp;
         _next = temp + slen + 1;
         muscleStrncpy(_next, tokenizeMe, bufLen-(slen+1));
      }
      else _seps = _next = NULL;
   }

   /** This Constructor is the same as above, only with this one you allow
    *  the StringTokenizer to modify (tokenizeMe) directly, rather
    *  than making a copy of the string first and modifying that.  (it's a bit more efficient)
    *  @param junk Ignored; it's only here to disambiguate the two constructors.
    *  @param tokenizeMe The string to tokenize.  This string will get munged!
    *  @param separators ASCII string representing a list of characters to interpret as word-separator characters.
    *                    Defaults to ", \t" (where "\t" is of course the tab character)
    */
   StringTokenizer(bool junk, char * tokenizeMe, const char * separators = ", \t\r\n") : _alloced(false), _seps(separators), _next(tokenizeMe)
   {
      (void) junk;
   }

   /** Destructor */
   ~StringTokenizer()
   {
      // must cast to (char *) or VC++ complains :^P
      if (_alloced) delete [] ((char *)_seps);
   }

   /** Returns the next token in the parsed string, or NULL if there are no more tokens left */
   char * GetNextToken()
   {
      if (_seps)
      {
         // Move until first non-sep char
         while((*_next)&&(strchr(_seps, *_next) != NULL)) _next++;
         if (*_next)
         {
            char * ret = _next;

            // Move until next sep-char
            while((*_next)&&(strchr(_seps, *_next) == NULL)) _next++;
            if (*_next) 
            {
               *_next = '\0';
               _next++;
            }
            return ret;
         }
      }
      return NULL;
   }

   /** Convenience synonym for GetNextToken() */
   char * operator()() {return GetNextToken();}
 
   /** Returns the remainder of the string, starting with the next token,
    *  or NULL if there are no more tokens in the string.
    *  Doesn't affect the next return value of GetNextToken(), though.
    */
   char * GetRemainderOfString()
   {
      if (_seps)
      {
         // Move until first non-sep char
         while((*_next)&&(strchr(_seps, *_next) != NULL)) _next++;
         return (*_next) ? _next : NULL;  // and return from there
      }
      return NULL;
   }

private:
   StringTokenizer(const StringTokenizer &);   // unimplemented on purpose
   StringTokenizer & operator = (const StringTokenizer &);  // unimplemented on purpose

   bool _alloced;
   const char * _seps;
   char * _next;
   char _smallStringBuf[128];
};

} // end namespace muscle

#endif
