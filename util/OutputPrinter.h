/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleOutputPrinter_h
#define MuscleOutputPrinter_h

#include "support/MuscleSupport.h"
#include "util/String.h"

namespace muscle {

/** This is a convenience class for code that wants to output text to either a String, to stdout, or to the Log.
  * By using this class, you only have to write that code once, rather than three times.
  */
class OutputPrinter
{
public:
   /** Constructor for "printing" that appends the printed text to a String
     * @param addToString the String to add text to
     */
   OutputPrinter(String & addToString) : _logSeverity(MUSCLE_LOG_NONE), _addToString(&addToString), _file(NULL), _isStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that logs the printed text via LogTime()
     * @param logSeverity the MUSCLE_LOG_* severity-level to log at
     */
   OutputPrinter(int logSeverity) : _logSeverity(logSeverity), _addToString(NULL), _file(NULL), _isStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that writes the printed text to a FILE
     * @param addToFile the file to add to
     */
   OutputPrinter(FILE * addToFile) : _logSeverity(MUSCLE_LOG_NONE), _addToString(NULL), _file(addToFile), _isStartOfLine(true) {/* empty */}

   /** Constructor for "printing" that writes to multiple targets
     * @param optLogSeverity if a value other thatn MUSCLE_LOG_NONE, printf()-style text will be passed to LogTime() and/or LogPlain() at this severity-level
     * @param optAddToString if non-NULL, the printf()-style text will be appended to this String
     * @param optWriteToFile if non-NULL, the printf()-style text will be fprintf()'d to this FILE
     */
   OutputPrinter(int optLogSeverity, String * optAddToString, FILE * optWriteToFile) : _logSeverity(optLogSeverity), _addToString(optAddToString), _file(optWriteToFile), _isStartOfLine(true) {/* empty */}

   /** Writes the specified printf()-style text to a file, and/or appends it to a String and/or prints it to the Log.
     * @param fmt a printf()-style format-specified (optionally followed by printf()-style arguments)
     */
   MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
   void printf(const char * fmt, ...) const;

   /** Returns a pointer to the String passed in to our constructor, or NULL. */
   String * GetString() const {return _addToString;}

   /** Returns a pointer to the FILE passed in to our constructor, or NULL. */
   FILE * GetFile() const {return _file;}

   /** Returns the MUSCLE_LOG_SEVERITY value passed into our constructor, or MUSCLE_LOG_NONE. */
   int GetLogSeverity() const {return _logSeverity;}

private:
   int _logSeverity;
   String * _addToString;
   FILE * _file;
   mutable bool _isStartOfLine;
};

} // end namespace muscle

#endif
