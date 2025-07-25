/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.TXT file for details. */

#ifndef MuscleSysLog_h
#define MuscleSysLog_h

#include "support/MuscleSupport.h"

#ifndef MUSCLE_AVOID_CPLUSPLUS11
# include <atomic>
#endif
#ifdef MUSCLE_MINIMALIST_LOGGING
# include <stdarg.h>
#endif

namespace muscle {

/** @defgroup systemlog The SystemLog function API
 *  These functions are all defined in SysLog.h, and are stand-alone
 *  functions that provide console-logging and log-file generation functionality to MUSCLE programs.
 *  @{
 */

class String;
class LogCallbackArgs;
class OutputPrinter;

/** log level constants to use with SetLogLevel(), GetLogLevel() */
enum
{
   MUSCLE_LOG_NONE = 0,         /**< nothing ever gets logged at this level (default) */
   MUSCLE_LOG_CRITICALERROR,    /**< only things that should never ever happen */
   MUSCLE_LOG_ERROR,            /**< things that shouldn't usually happen */
   MUSCLE_LOG_WARNING,          /**< things that are suspicious */
   MUSCLE_LOG_INFO,             /**< things that the user might like to know */
   MUSCLE_LOG_DEBUG,            /**< things the programmer is debugging with */
   MUSCLE_LOG_TRACE,            /**< exhaustively detailed output */
   NUM_MUSCLE_LOGLEVELS         /**< guard value */
};

// Define this constant in your Makefile (ie -DMUSCLE_DISABLE_LOGGING) to turn all the
// Log commands into no-ops.
#ifdef MUSCLE_DISABLE_LOGGING
# define MUSCLE_INLINE_LOGGING

// No-op implementation of LogPlain()
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
static inline status_t LogPlainAux(int, const char * , ...) {return B_NO_ERROR;}
# define LogPlain(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogPlainAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

// No-op implementation of LogTime()
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
static inline status_t LogTimeAux(int, const char *, ...) {return B_NO_ERROR;}
# define LogTime(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogTimeAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

// No-op implementation of WarnOutOfMemory()
static inline void WarnOutOfMemory(const char *, int) {/* empty */}

// No-op implementation of LogFlush()
static inline void LogFlush() {/* empty */}

// No-op version of GetLogLevelName(), just returns a dummy string
static inline const char * GetLogLevelName(int /*logLevel*/) {return "<omitted>";}

// No-op version of GetLogLevelKeyword(), just returns a dummy string
static inline const char * GetLogLevelKeyword(int /*logLevel*/) {return "<omitted>";}

#else

// Define this constant in your Makefile (ie -DMUSCLE_MINIMALIST_LOGGING) if you don't want to have
// to link in SysLog.cpp and Hashtable.cpp and all the other stuff that is required for "real" logging.
# ifdef MUSCLE_MINIMALIST_LOGGING
#  define MUSCLE_INLINE_LOGGING

// Minimalist version of LogPlain(), just sends the output to stdout.
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
static inline status_t LogPlainAux(int, const char * fmt, ...) {va_list va; va_start(va, fmt); vprintf(fmt, va); va_end(va); return B_NO_ERROR;}
# define LogPlain(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogPlainAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

// Minimalist version of LogTime(), just sends a tiny header and the output to stdout.
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
static inline status_t LogTimeAux(int logLevel, const char * fmt, ...) {printf("%i: ", logLevel); va_list va; va_start(va, fmt); vprintf(fmt, va); va_end(va); return B_NO_ERROR;}
# define LogTime(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogTimeAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

// Minimalist version of WarnOutOfMemory()
static inline void WarnOutOfMemory(const char * file, int line) {printf("ERROR--MEMORY ALLOCATION FAILURE!  (%s:%i)\n", file, line);}

// Minimumist version of LogFlush(), just flushes stdout
static inline void LogFlush() {fflush(stdout);}

// Minimalist version of GetLogLevelName(), just returns a dummy string
MUSCLE_NODISCARD static inline const char * GetLogLevelName(int /*logLevel*/) {return "<omitted>";}

// Minimalist version of GetLogLevelKeyword(), just returns a dummy string
MUSCLE_NODISCARD static inline const char * GetLogLevelKeyword(int /*logLevel*/) {return "<omitted>";}
# endif
#endif

#ifdef MUSCLE_INLINE_LOGGING
MUSCLE_NODISCARD inline int ParseLogLevelKeyword(const char *)         {return MUSCLE_LOG_NONE;}
MUSCLE_NODISCARD inline int GetFileLogLevel()                          {return MUSCLE_LOG_NONE;}
// Note:  GetFileLogName() is not defined here for the inline-logging case, because it causes chicken-and-egg header problems
//MUSCLE_NODISCARD inline String GetFileLogName()                      {return "";}
MUSCLE_NODISCARD inline uint32 GetFileLogMaximumSize()                 {return MUSCLE_NO_LIMIT;}
MUSCLE_NODISCARD inline uint32 GetMaxNumLogFiles()                     {return MUSCLE_NO_LIMIT;}
MUSCLE_NODISCARD inline bool GetFileLogCompressionEnabled()            {return false;}
MUSCLE_NODISCARD inline int GetConsoleLogLevel()                       {return MUSCLE_LOG_INFO;}
MUSCLE_NODISCARD inline int GetMaxLogLevel()                           {return MUSCLE_LOG_INFO;}
inline void SetFileLogLevel(int)                                       {/* empty */}
inline void SetFileLogName(const String &)                             {/* empty */}
inline void SetFileLogMaximumSize(uint32)                              {/* empty */}
inline void SetOldLogFilesPattern(const String &)                      {/* empty */}
inline void SetMaxNumLogFiles(uint32)                                  {/* empty */}
inline void SetFileLogCompressionEnabled(bool)                         {/* empty */}
inline void SetConsoleLogLevel(int)                                    {/* empty */}
inline void SetConsoleLogToStderr(bool)                                {/* empty */}
inline void CloseCurrentLogFile()                                      {/* empty */}
inline status_t LogStackTrace(int logSeverity, uint32 maxDepth = 64) {(void) logSeverity; (void) maxDepth; printf("<stack trace omitted>\n"); return B_NO_ERROR;}
inline status_t PrintStackTrace(uint32 maxDepth = 64) {(void) maxDepth; fprintf(stdout, "<stack trace omitted>\n"); return B_NO_ERROR;}
#else

/** Returns the MUSCLE_LOG_* equivalent of the given keyword string
 *  @param keyword a string such as "debug", "log", or "info".
 *  @return A MUSCLE_LOG_* value
 */
MUSCLE_NODISCARD int ParseLogLevelKeyword(const char * keyword);

/** Returns the current log level for logging to a file.
 *  @return a MUSCLE_LOG_* value.
 */
MUSCLE_NODISCARD int GetFileLogLevel();

/** Returns the user-specified name for the file to log to.
 *  (note that this may be different from the log file name actually used,
 *  since the logging mechanism will choose a name when the log is first opened
 *  if no manually specified name was chosen)
 *  @return a file name or file path representing where the user would like the
 *          log file to be written.
 */
String GetFileLogName();

/** Returns the maximum size (in bytes) that we will allow the log file(s) to grow to.
  * Default is MUSCLE_NO_LIMIT, ie unlimited file size.
  */
MUSCLE_NODISCARD uint32 GetFileLogMaximumSize();

/** Returns the maximum number of log files that should be written out before
  * old log files start to be deleted.  Defaults to MUSCLE_NO_LIMIT, ie
  * never delete any log files.
  */
MUSCLE_NODISCARD uint32 GetMaxNumLogFiles();

/** Returns true if log files are to be gzip-compressed when they are closed;
  * or false if they should be left in raw text form.  Default value is false.
  */
MUSCLE_NODISCARD bool GetFileLogCompressionEnabled();

/** Returns the current log level for logging to stdout.
 *  @return a MUSCLE_LOG_* value.
 */
MUSCLE_NODISCARD int GetConsoleLogLevel();

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
namespace muscle_private
{
#ifdef MUSCLE_AVOID_CPLUSPLUS11
extern int _maxLogThreshold;
#else
extern std::atomic<int> _maxLogThreshold;
#endif
}
#endif

/** Returns the max of GetFileLogLevel() and GetConsoleLogLevel()
 *  @return a MUSCLE_LOG_* value
 */
MUSCLE_NODISCARD static inline int GetMaxLogLevel()
{
#ifdef MUSCLE_AVOID_CPLUSPLUS11
   return muscle_private::_maxLogThreshold;  // I guess we'll take our chances here
#else
   return muscle_private::_maxLogThreshold.load();
#endif
}

/** Sets the log filter level for logging to a file.
 *  Any calls to Log*() that specify a log level greater than (loglevel)
 *  will be suppressed.  Default level is MUSCLE_LOG_NONE (ie no file logging is done)
 *  @param loglevel The MUSCLE_LOG_* value to use in determining which log messages to save to disk.
 */
void SetFileLogLevel(int loglevel);

/** Forces the file logger to close any log file that it currently has open. */
void CloseCurrentLogFile();

/** Sets a user-specified name/path for the log file.  This name will
 *  be used whenever a log file is to be opened, instead of the default log file name.
 *  @param logName The string to use, or "" if you'd prefer a log file name be automatically generated.
 *                 Note that this string can contain any of the special tokens described by the
 *                 HumanReadableTimeValues::ExpandTokens() method, and these values will be expanded
 *                 out when the log file is opened.
 */
void SetFileLogName(const String & logName);

/** Sets a user-specified maximum size (in bytes) for the log file.  Once a log file has reached this size,
  * it will be closed and a new log file opened (note that the new log file's name will be the same
  * as the old log file, overwriting it, unless you specify a date/time token via SetFileLogName()
  * that will expand out differently).
  * Default state is no limit on log file size.  (aka MUSCLE_NO_LIMIT)
  * @param maxSizeBytes The maximum allowable log file size, or MUSCLE_NO_LIMIT to allow any size log file.
  */
void SetFileLogMaximumSize(uint32 maxSizeBytes);

/** Sets the path-pattern of files that the logger is allowed to assume are old log files, and therefore
  * is allowed to delete.
  * @param pattern The pattern to match against (eg "/var/log/mylogfiles-*.txt").  The matching will
  *                be done immediately/synchronously inside this call.
  */
void SetOldLogFilesPattern(const String & pattern);

/** Sets a user-specified maximum number of log files that should be written out before
  * the oldest log files start to be deleted.  This can help keep the filesystem
  * space taken up by log files limited to a finite amount.
  * Default state is no limit on the number of log files.  (aka MUSCLE_NO_LIMIT)
  * @param maxNumLogFiles The maximum allowable number of log files, or MUSCLE_NO_LIMIT to allow any number of log files.
  */
void SetMaxNumLogFiles(uint32 maxNumLogFiles);

/** Set this to true if you want log files to be compressed when they are closed (and given a .gz extension).
  * or false if they should be left in raw text form.
  * @param enable True to enable log file compression; false otherwise.
  */
void SetFileLogCompressionEnabled(bool enable);

/** Sets the log filter level for logging to stdout.
 *  Any calls to Log*() that specify a log level greater than (loglevel)
 *  will be suppressed.  Default level is MUSCLE_LOG_INFO.
 *  @param loglevel The MUSCLE_LOG_* value to use in determining which log messages to print to stdout.
 */
void SetConsoleLogLevel(int loglevel);

/** Sets a flag so that our log output goes to stderr instead of stdout.
  * @param toStderr true if you want log output to go to stderr rather than stdout; false to set it
  *                 back to going to stdout again.
  */
void SetConsoleLogToStderr(bool toStderr);

/** LogPlainAux() works the same as LogTimeAux(), except LogPlainAux() doesn't emit a time/date/severity preamble before
 *  the caller-specified text.  Typically called indirectly, via the LogPlain() macro.
 *  @param logLevel a MUSCLE_LOG_* value indicating the "severity" of this message.  This call will generate
 *                  log text only if (logLevel) is less than or equal to the value returned by GetMaxLogLevel().
 *  @param fmt A printf-style format string (eg "hello %s\n").  Note that \n is NOT added for you.
 *  @returns B_NO_ERROR on success, or another value if something went wrong.
 *  @note LogPlain() is implemented as a macro, so the arguments you pass to it will not be evaluated or passed to
 *        LogPlainAux() if the log-level you specified is not severe enough to pass the log-threshold (as specified
 *        by GetMaxLogLevel().  Therefore you should be careful that the arguments you pass to LogPlain() don't have
 *        side effects that your code depends on for correctness!
 */
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
status_t LogPlainAux(int logLevel, const char * fmt, ...);
# define LogPlain(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogPlainAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

/** Calls LogTime() with a critical "MEMORY ALLOCATION FAILURE" Message.
  * Note that you typically wouldn't call this function directly;
  * rather you should call the MWARN_OUT_OF_MEMORY macro and it will
  * call WarnOutOfMemory() for you, with the correct arguments.
  * @param file Name of the source file where the memory failure occurred.
  * @param line Line number where the memory failure occurred.
  */
void WarnOutOfMemory(const char * file, int line);

#ifdef MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS
# ifndef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
#  define MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME 1
# endif
#endif

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(5,6)
status_t LogTimeAux(const char * sourceFile, const char * optSourceFunction, int line, int logLevel, const char * fmt, ...);
# define LogTime(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogTimeAux(__FILE__, __FUNCTION__, __LINE__, logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))
#else
/** MUSCLE's primary function for logging.  Typically called indirectly, via the LogTime() macro.
 *  Automagically prepends a timestamp and status string to the caller-specified text.
 *  eg LogTime(MUSCLE_LOG_INFO, "Hello %s! I am %i.", "world", 42) would generate "[I 12/18 12:11:49] Hello world! I am 42."
 *  @param logLevel a MUSCLE_LOG_* value indicating the "severity" of this message.  This call will generate
 *                  log text only if (logLevel) is less than or equal to the value returned by GetMaxLogLevel().
 *  @param fmt A printf-style format string (eg "hello %s\n").  Note that \n is NOT added for you.
 *  @returns B_NO_ERROR on success, or another value if something went wrong.
 *  @note LogTime() is implemented as a macro, so the arguments you pass to it will not be evaluated or passed to LogTimeAux()
 *        if the log-level you specified is not severe enough to pass the log-threshold (as specified by GetMaxLogLevel().
 *        Therefore you should be careful that the arguments you pass to LogTime() don't have side effects that your
 *        code depends on for correctness!
 */
MUSCLE_PRINTF_ARGS_ANNOTATION_PREFIX(2,3)
status_t LogTimeAux(int logLevel, const char * fmt, ...);
# define LogTime(logLevel, ...) ((void)(((logLevel) <= muscle::GetMaxLogLevel()) ? muscle::LogTimeAux(logLevel, __VA_ARGS__) : muscle::B_NO_ERROR))

#endif

/** Ensures that all previously logged output is actually sent.  That is, it simply
 *  calls fflush() on any streams that we are logging to.
 */
void LogFlush();

/** Attempts to lock the Mutex that is used to serialize LogCallback calls.
  * Typically you won't need to call this function, as it is called for you
  * before any LogCallback calls are made.
  * @returns B_NO_ERROR on success or B_LOCK_FAILED on failure.
  * @note Be sure to call UnlockLog() when you are done!
  */
status_t LockLog();

/** Unlocks the Mutex that is used to serialize LogCallback calls.
  * Typically you won't need to call this function, as it is called for you
  * after any LogCallback calls are made.  The only time you need to call it
  * is after you've made a call to LockLog() and are now done with your critical
  * section.
  * @returns B_NO_ERROR on success or B_LOCK_FAILED on failure.
  */
status_t UnlockLog();

/** Returns a human-readable string for the given log level.
 *  @param logLevel A MUSCLE_LOG_* value
 *  @return A pretty human-readable description string such as "Informational" or "Warnings and Errors Only"
 */
MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * GetLogLevelName(int logLevel);

/** Returns a brief human-readable string for the given log level.
 *  @param logLevel A MUSCLE_LOG_* value
 *  @return A brief human-readable description string such as "info" or "warn"
 */
MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const char * GetLogLevelKeyword(int logLevel);

/** Writes a standard text string of the format "[L mm/dd hh:mm:ss]" into (buf).
 *  @param buf Char buffer to write into.  Should be at least 64 chars long.
 *  @param lca A LogCallbackArgs object containing the time stamp and severity that are appropriate to print.
 */
void GetStandardLogLinePreamble(char * buf, const LogCallbackArgs & lca);

/** Convenience method:  Prints a stack trace to stdout.
  * @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
  *                 64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
  * @returns B_NO_ERROR on success, or another value on failure.  (May return B_UNIMPLEMENTED if stack trace generation
  *          isn't implemented for the current OS:  currently, it's supported for Windows, MacOS/X and Linux)
  */
status_t PrintStackTrace(uint32 maxDepth = 64);

/** Logs out a stack trace, if possible.
 *  @note Currently only works under Linux and MacOS/X Leopard, and then only if -rdynamic is specified as a compile flag.
 *  @param logSeverity a MUSCLE_LOG_* value indicating the "severity" of this message.
 *  @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
 *                  64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
 *  @returns B_NO_ERROR on success, or B_UNIMPLEMENTED if a stack trace couldn't be logged because the platform doesn't support it.
 */
status_t LogStackTrace(int logSeverity, uint32 maxDepth = 64);

#endif  // !MUSCLE_INLINE_LOGGING

/** Prints out a stack trace using the passed-in OutputPrinter.
  * @param p the OutputPrinter to use for printing.  (e.g. pass in stdout to print to stdout, or a MUSCLE_LOG_* value or a String)
  * @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
  *                 64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
  * @returns B_NO_ERROR on success, or another value on failure.  (May return B_UNIMPLEMENTED if stack trace generation
  *          isn't implemented for the current OS:  currently, it's supported for Windows, MacOS/X and Linux)
  */
status_t PrintStackTrace(const OutputPrinter & p, uint32 maxDepth = 64);

/** Given a source location (eg as provided by the information in a LogCallbackArgs object),
  * returns a corresponding uint32 that represents a hash of that location.
  * The source code location can be later looked up by feeding this hash value
  * as a command line argument into the muscle/tests/findsourcecodelocations program.
  * @param fileName the filename associated with the location (eg as returned by the __FILE__ macro)
  * @param lineNumber the line number within the file.
  */
MUSCLE_NODISCARD uint32 GenerateSourceCodeLocationKey(const char * fileName, uint32 lineNumber);

/** Given a source-code location key (as returned by GenerateSourceCodeLocationKey()),
  * returns the standard human-readable representation of that value.  (eg "7QF2")
  * @param key the source-code-location-key, represented as a uint32.
  */
String SourceCodeLocationKeyToString(uint32 key);

/** Given a standard human-readable representation of a source-code-location
  * key (eg "7EF2"), returns the uint16 key value.  This is the inverse
  * function of SourceCodeLocationKeyToString().
  * @param s the source-code-location-key, represented as a human-readable string.
  */
MUSCLE_NODISCARD uint32 SourceCodeLocationKeyFromString(const String & s);

/** This class represents all the fields necessary to present a human with a human-readable time/date stamp.  Objects of this class are typically populated by the GetHumanReadableTimeValues() function, below. */
class MUSCLE_NODISCARD HumanReadableTimeValues MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor */
   HumanReadableTimeValues() : _year(0), _month(0), _dayOfMonth(0), _dayOfWeek(0), _hour(0), _minute(0), _second(0), _microsecond(0) {/* empty */}

   /** Explicit constructor
     * @param year The year value (eg 2005)
     * @param month The month value (January=0, February=1, etc)
     * @param dayOfMonth The day within the month (ranges from 0 to 30, inclusive)
     * @param dayOfWeek The day within the week (Sunday=0, Monday=1, etc)
     * @param hour The hour within the day (ranges from 0 to 23, inclusive)
     * @param minute The minute within the hour (ranges from 0 to 59, inclusive)
     * @param second The second within the minute (ranges from 0 to 59, inclusive)
     * @param microsecond The microsecond within the second (ranges from 0 to 999999, inclusive)
     */
   HumanReadableTimeValues(int year, int month, int dayOfMonth, int dayOfWeek, int hour, int minute, int second, int microsecond) : _year(year), _month(month), _dayOfMonth(dayOfMonth), _dayOfWeek(dayOfWeek), _hour(hour), _minute(minute), _second(second), _microsecond(microsecond) {/* empty */}

   /** Returns the year value (eg 2005) */
   MUSCLE_NODISCARD int GetYear() const {return _year;}

   /** Returns the month value (January=0, February=1, March=2, ..., December=11). */
   MUSCLE_NODISCARD int GetMonth() const {return _month;}

   /** Returns the day-of-month value (which ranges between 0 and 30, inclusive). */
   MUSCLE_NODISCARD int GetDayOfMonth() const {return _dayOfMonth;}

   /** Returns the day-of-week value (Sunday=0, Monday=1, Tuesday=2, Wednesday=3, Thursday=4, Friday=5, Saturday=6). */
   MUSCLE_NODISCARD int GetDayOfWeek() const {return _dayOfWeek;}

   /** Returns the hour value (which ranges between 0 and 23, inclusive). */
   MUSCLE_NODISCARD int GetHour() const {return _hour;}

   /** Returns the minute value (which ranges between 0 and 59, inclusive). */
   MUSCLE_NODISCARD int GetMinute() const {return _minute;}

   /** Returns the second value (which ranges between 0 and 59, inclusive). */
   MUSCLE_NODISCARD int GetSecond() const {return _second;}

   /** Returns the microsecond value (which ranges between 0 and 999999, inclusive). */
   MUSCLE_NODISCARD int GetMicrosecond() const {return _microsecond;}

   /** Sets the year value (eg 2005)
     * @param year new year value A.D. (in full four-digit form)
     */
   void SetYear(int year) {_year = year;}

   /** Sets the month value (January=0, February=1, March=2, ..., December=11).
     * @param month new month-in-year value [0-11]
     */
   void SetMonth(int month) {_month = month;}

   /** Sets the day-of-month value (which ranges between 0 and 30, inclusive).
     * @param dayOfMonth index of the new day-of-the-month field [0-30]
     */
   void SetDayOfMonth(int dayOfMonth) {_dayOfMonth = dayOfMonth;}

   /** Sets the day-of-week value (Sunday=0, Monday=1, Tuesday=2, Wednesday=3, Thursday=4, Friday=5, Saturday=6).
     * @param dayOfWeek index of the new day-of-the-week field [0-6]
     */
   void SetDayOfWeek(int dayOfWeek) {_dayOfWeek = dayOfWeek;}

   /** Sets the hour value (which ranges between 0 and 23, inclusive).
     * @param hour the new hour-of-the-day index [0-23]
     */
   void SetHour(int hour) {_hour = hour;}

   /** Sets the minute value (which ranges between 0 and 59, inclusive).
     * @param minute the new minute-of-the-hour index [0-59]
     */
   void SetMinute(int minute) {_minute = minute;}

   /** Sets the second value (which ranges between 0 and 59, inclusive).
     * @param second the new second-of-the-minute index [0-59]
     */
   void SetSecond(int second) {_second = second;}

   /** Sets the microsecond value (which ranges between 0 and 999999, inclusive).
     * @param microsecond the new microsecond-of-the-second index [0-999999]
     */
   void SetMicrosecond(int microsecond) {_microsecond = microsecond;}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator == (const HumanReadableTimeValues & rhs) const
   {
      return ((_year       == rhs._year)&&
              (_month      == rhs._month)&&
              (_dayOfMonth == rhs._dayOfMonth)&&
              (_dayOfWeek  == rhs._dayOfWeek)&&
              (_hour       == rhs._hour)&&
              (_minute     == rhs._minute)&&
              (_second     == rhs._second)&&
              (_microsecond == rhs._microsecond));
   }

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator != (const HumanReadableTimeValues & rhs) const {return !(*this==rhs);}

   /** This method will expand the following tokens in the specified String out to the following values:
     * %Y -> Current year (eg "2005")
     * %M -> Current month (eg "01" for January, up to "12" for December)
     * %Q -> Current month as a string (eg "January", "February", "March", etc)
     * %D -> Current day of the month (eg "01" through "31")
     * %d -> Current day of the month (eg "01" through "31") (synonym for %D)
     * %W -> Current day of the week (eg "1" through "7")
     * %w -> Current day of the week (eg "1" through "7") (synonym for %W)
     * %q -> Current day of the week as a string (eg "Sunday", "Monday", "Tuesday", etc)
     * %h -> Current hour (military style:  eg "00" through "23")
     * %m -> Current minute (eg "00" through "59")
     * %s -> Current second (eg "00" through "59")
     * %x -> Current microsecond (eg "000000" through "999999", inclusive)
     * %p -> Process ID of the current process
     * %r -> A random number between 0 and (2^64-1) (for spicing up the uniqueness of a filename)
     * %T -> A human-readable time/date stamp, for convenience (eg "January 01 2005 23:59:59")
     * %t -> A numeric time/date stamp, for convenience (eg "2005/01/01 15:23:59")
     * %f -> A filename-friendly numeric time/date stamp, for convenience (eg "2005-01-01_15h23m59")
     * %% -> A single percent sign.
     * @param s The string to expand the tokens of
     * @returns The same string, except with any and all of the above tokens expanded as described.
     */
   String ExpandTokens(const String & s) const;

   /** Returns a human-readable string showing the contents of this HumanReadableTimeValues object */
   String ToString() const;

private:
   int _year;
   int _month;
   int _dayOfMonth;
   int _dayOfWeek;
   int _hour;
   int _minute;
   int _second;
   int _microsecond;
};

/** When passing a uint64 as a time value, these tags help indicate what sort of time value it is. */
enum {
   MUSCLE_TIMEZONE_UTC = 0, /**< Universal Co-ordinated Time (formerly Greenwhich Mean Time) */
   MUSCLE_TIMEZONE_LOCAL    /**< Host machine's local time (depends on local time zone settings) */
};

/** Given a uint64 representing a time in microseconds since 1970,
  * (eg as returned by GetCurrentTime64()), returns the same value
  * as a set of more human-friendly units.
  *
  * @param timeUS a time in microseconds since 1970.  Note that the interpretation of this value depends on
  *               the value passed in to the (timeType) argument.
  * @param retValues On success, this object will be filled out with the various human-readable time/date value fields
  *                  that human beings like to read.  See the HumanReadableTimeValues class documentation for details.
  * @param timeType If set to MUSCLE_TIMEZONE_UTC (the default) then (timeUS) will be interpreted as being in UTC,
  *                 and will be converted to the local time zone as part of the conversion process.  If set to
  *                 MUSCLE_TIMEZONE_LOCAL, on the other hand, then (timeUS) will be assumed to be already
  *                 in the local time zone, and no time zone conversion will be done.
  *                 Note that the values returned are ALWAYS in reference to local time
  *                 zone -- the (timeType) argument governs how (timeUS) should be interpreted.
  *                 (timeType) does NOT control the meaning of the return values.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t GetHumanReadableTimeValues(uint64 timeUS, HumanReadableTimeValues & retValues, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** This function is the inverse operation of GetHumanReadableTimeValues().  Given a HumanReadableTimeValues object,
  * this function returns the corresponding microseconds-since-1970 value.
  * @param values The HumanReadableTimeValues object to examine
  * @param retTimeUS On success, the corresponding uint64 is written here (in microseconds-since-1970)
  * @param timeType If set to MUSCLE_TIMEZONE_UTC (the default) then (values) will be interpreted as being in UTC,
  *                 and (retTimeUS) be converted to the local time zone as part of the conversion process.  If set to
  *                 MUSCLE_TIMEZONE_LOCAL, on the other hand, then no time zone conversion will be done.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t GetTimeStampFromHumanReadableTimeValues(const HumanReadableTimeValues & values, uint64 & retTimeUS, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** Given a uint64 representing a time in microseconds since 1970,
  * (eg as returned by GetCurrentTime64()), returns an equivalent
  * human-readable time/date string.  The format of the returned
  * time string is "YYYY/MM/DD HH:MM:SS".
  * @param timeUS a time in microseconds since 1970.  Note that the interpretation of this value depends on
  *               the value passed in to the (timeType) argument.
  * @param timeType If set to MUSCLE_TIMEZONE_UTC (the default) then (timeUS) will be interpreted as being in UTC,
  *                 and will be converted to the local time zone as part of the conversion process.  If set to
  *                 MUSCLE_TIMEZONE_LOCAL, on the other hand, then (timeUS) will be assumed to be already
  *                 in the local time zone, and no time zone conversion will be done.
  * @returns The equivalent ASCII string, or "" on failure.
  */
String GetHumanReadableTimeString(uint64 timeUS, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** Does the inverse operation of GetHumanReadableTimeString():
  * Given a time string of the format "YYYY/MM/DD HH:MM:SS",
  * returns the equivalent time value in microseconds since 1970.
  * @param str An ASCII string representing a time.
  * @param timeType If set to MUSCLE_TIMEZONE_UTC (the default) then the returned value will be UTC.
  *                 If set to MUSCLE_TIMEZONE_LOCAL, on the other hand, then the returned value will
  *                 be expressed as a time of the local time zone.
  * @returns The equivalent time value, or zero on failure.
  */
MUSCLE_NODISCARD uint64 ParseHumanReadableTimeString(const String & str, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** Given a string that represents a time interval, returns the equivalent value in microsends.
  * A time interval should be expressed as a non-negative integer, optionally followed by
  * any of the following suffixes:
  *   us = microseconds
  *   ms = milliseconds
  *   s  = seconds
  *   m  = minutes
  *   h  = hours
  *   d  = days
  *   w  = weeks
  * If no suffix is supplied, the units are presumed to be in seconds.
  * @param str The string to parse
  * @returns an unsigned time interval value, in microseconds.
  * @note As a special case, the string "forever" will parse to MUSCLE_TIME_NEVER (aka the largest possible uint64).
  */
MUSCLE_NODISCARD uint64 ParseHumanReadableUnsignedTimeIntervalString(const String & str);

/** Works the same as ParseHumanReadableUnsignedTimeIntervalString(), except that it returns an
  * int64 instead of a uint64, and if the string starts with a - sign, a negative value will be returned.
  * @param str The string to parse
  * @returns a signed time-interval value, in microseconds
  * @note As a special case, the string "forever" will parse to the largest possible int64.
  */
MUSCLE_NODISCARD int64 ParseHumanReadableSignedTimeIntervalString(const String & str);

/** Given a time interval specified in microseconds, returns a human-readable
  * string representing that time, eg "3 weeks, 2 days, 1 hour, 25 minutes, 2 seconds, 350 microseconds"
  * @param micros The number of microseconds to describe, as an unsigned 64-bit integer.
  * @param maxClauses The maximum number of clauses to allow in the returned string.  For example, passing this in
  *                   as 1 might return "3 weeks", while passing this in as two might return "3 weeks, 2 days".
  *                   Default value is MUSCLE_NO_LIMIT, indicating that no maximum should be enforced.
  * @param minPrecisionMicros The maximum number of microseconds the routine is allowed to ignore
  *                           when generating its string.  For example, if this value was passed in
  *                           as 1000000, the returned string would describe the interval down to
  *                           the nearest second.  Defaults to zero for complete accuracy.
  * @param optRetIsAccurate If non-NULL, this value will be set to true if the returned string represents
  *                         (micros) down to the nearest microsecond, or false if the string is an approximation.
  * @param roundUp If specified true, and we have to round off to the nearest unit in order to return a more
  *                succinct human-readable string, then setting this to true will allow the figure in the final
  *                position to be rounded up to the nearest unit, rather than always being rounded down.
  * @returns a human-readable time interval description string.
  */
String GetHumanReadableUnsignedTimeIntervalString(uint64 micros, uint32 maxClauses = MUSCLE_NO_LIMIT, uint64 minPrecisionMicros = 0, bool * optRetIsAccurate = NULL, bool roundUp = false);

/** Works the same as GetHumanReadableUnsignedTimeIntervalString() except it interprets the passed-in microseconds value
  * as a signed integer rather than unsigned, and thus will yield a useful result for negative values
  * (eg "-3 seconds" rather than "54893 years").
  * @param micros The number of microseconds to describe, as a signed 64-bit integer.
  * @param maxClauses The maximum number of clauses to allow in the returned string.  For example, passing this in
  *                   as 1 might return "3 weeks", while passing this in as two might return "3 weeks, 2 days".
  *                   Default value is MUSCLE_NO_LIMIT, indicating that no maximum should be enforced.
  * @param minPrecisionMicros The maximum number of microseconds the routine is allowed to ignore
  *                           when generating its string.  For example, if this value was passed in
  *                           as 1000000, the returned string would describe the interval down to
  *                           the nearest second.  Defaults to zero for complete accuracy.
  * @param optRetIsAccurate If non-NULL, this value will be set to true if the returned string represents
  *                         (micros) down to the nearest microsecond, or false if the string is an approximation.
  * @param roundUp If specified true, and we have to round off to the nearest unit in order to return a more
  *                succinct human-readable string, then setting this to true will allow the figure in the final
  *                position to be rounded up to the nearest unit, rather than always being rounded down.
  * @returns a human-readable time interval description string.
  */
String GetHumanReadableSignedTimeIntervalString(int64 micros, uint32 maxClauses = MUSCLE_NO_LIMIT, uint64 minPrecisionMicros = 0, bool * optRetIsAccurate = NULL, bool roundUp = false);

/** @} */ // end of systemlog doxygen group

} // end namespace muscle

#endif
