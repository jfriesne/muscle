/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleOutputPrinter_h
#define MuscleOutputPrinter_h

#include "support/MuscleSupport.h"

namespace muscle {

class String;

/** This is a convenience class for code that wants to output text to either a String, to stdout, or to the Log.
  * By using this class, you only have to write that code once, rather than three times.
  */
class OutputPrinter
{
public:
   /** Constructor for "printing" that appends the printed text to a String
     * @param addToString the String to add text to
     */
   OutputPrinter(String & addToString) : _logSeverity(MUSCLE_LOG_NONE), _addToString(&addToString), _file(NULL), _indent(0), _isAtStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that logs the printed text via LogTime()
     * @param logSeverity the MUSCLE_LOG_* severity-level to log at
     */
   OutputPrinter(int logSeverity) : _logSeverity(logSeverity), _addToString(NULL), _file(NULL), _indent(0), _isAtStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that writes the printed text to a FILE
     * @param addToFile the file to add to
     * @note we do NOT take ownership of the file handle; it remains up to to the calling code to close it when appropriate.
     */
   OutputPrinter(FILE * addToFile) : _logSeverity(MUSCLE_LOG_NONE), _addToString(NULL), _file(addToFile), _indent(0), _isAtStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that writes to multiple targets
     * @param optLogSeverity if a value other than MUSCLE_LOG_NONE, printf()-style text will be passed to LogTime() and/or LogPlain() at this severity-level
     * @param optAddToString if non-NULL, the printf()-style text will be appended to this String
     * @param optWriteToFile if non-NULL, the printf()-style text will be fprintf()'d to this FILE
     * @param indent how many spaces we should automatically insert at the start of each line
     */
   OutputPrinter(int optLogSeverity, String * optAddToString, FILE * optWriteToFile, uint32 indent = 0) : _logSeverity(optLogSeverity), _addToString(optAddToString), _file(optWriteToFile), _indent(indent), _isAtStartOfLine(true) {/* empty */}

   /** Destructor -- doesn't do anything */
   ~OutputPrinter() {/* empty */}

   /** Writes the specified printf()-style text to a file, and/or appends it to a String and/or prints it to the Log.
     * @param fmt a printf()-style format-specified (followed by zero or more printf()-style arguments to be used for string interpolation)
     */
   MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
   void printf(const char * fmt, ...) const;

   /** Writes out a single character.
     * @param c the character to write
     * @param repeatCount the number of times this character should be written.  Defaults to 1.
     */
   void putc(char c, uint32 repeatCount=1) const;

   /** Writes out a single string.
     * @param s the NULL-terminated string to write
     * @param repeatCount the number of times this string should be written.  Defaults to 1.
     */
   void puts(const char * s, uint32 repeatCount=1) const;

   /** Calls fflush() on our output stream(s) where possible */
   void fflush() const;

   /** Returns the pointer to the String passed in to our constructor, or NULL. */
   String * GetAddToString() const {return _addToString;}

   /** Returns the pointer to the FILE passed in to our constructor, or NULL. */
   FILE * GetFile() const {return _file;}

   /** Returns the MUSCLE_LOG_SEVERITY value passed into our constructor, or MUSCLE_LOG_NONE. */
   int GetLogSeverity() const {return _logSeverity;}

   /** Returns the number of spaces this OutputPrinter will automatically place at the start of each line. */
   uint32 GetIndent() const {return _indent;}

   /** Convenience function:  Returns an OutputPrinter equal to this one but with a larger indent-value so that it will
     * print each line with more spaces at the front.
     * @param indent how many additional spaces to indent in the returned OutputPrinter.  Defaults to 3.
     */
   OutputPrinter WithIndent(uint32 indent = 3) const {return OutputPrinter(_logSeverity, _addToString, _file, _indent+indent);}

private:
   void putsAux(const char * s, uint32 numChars) const;

   int _logSeverity;
   String * _addToString;
   FILE * _file;
   uint32 _indent;
   mutable bool _isAtStartOfLine;
};

} // end namespace muscle

#endif
