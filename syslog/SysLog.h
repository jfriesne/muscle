/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.TXT file for details. */

#ifndef MuscleSysLog_h
#define MuscleSysLog_h

#include "support/MuscleSupport.h"

#ifdef MUSCLE_MINIMALIST_LOGGING
# include <stdarg.h>
#endif

namespace muscle {

class String;
class LogCallbackArgs;

/** log level constants to use with SetLogLevel(), GetLogLevel() */
enum
{
   MUSCLE_LOG_NONE = 0,         // nothing ever gets logged at this level (default)
   MUSCLE_LOG_CRITICALERROR,    // only things that should never ever happen
   MUSCLE_LOG_ERROR,            // things that shouldn't usually happen
   MUSCLE_LOG_WARNING,          // things that are suspicious
   MUSCLE_LOG_INFO,             // things that the user might like to know
   MUSCLE_LOG_DEBUG,            // things the programmer is debugging with
   MUSCLE_LOG_TRACE,            // exhaustively detailed output
   NUM_MUSCLE_LOGLEVELS
}; 

// Define this constant in your Makefile (i.e. -DMUSCLE_DISABLE_LOGGING) to turn all the
// Log commands into no-ops.
#ifdef MUSCLE_DISABLE_LOGGING
# define MUSCLE_INLINE_LOGGING

// No-op implementation of Log()
static inline status_t Log(int, const char * , ...) {return B_NO_ERROR;}

// No-op implementation of LogTime()
static inline status_t LogTime(int, const char *, ...) {return B_NO_ERROR;}

// No-op implementation of WarnOutOfMemory()
static inline void WarnOutOfMemory(const char *, int) {/* empty */}

// No-op implementation of LogFlush()
static inline status_t LogFlush() {return B_NO_ERROR;}

// No-op implementation of LogStackTrace()
static inline status_t LogStackTrace(int level = MUSCLE_LOG_INFO, uint32 maxLevel=64) {(void) level; (void) maxLevel; return B_NO_ERROR;}

// No-op implementation of PrintStackTrace()
static inline status_t PrintStackTrace(FILE * fpOut = NULL, uint32 maxLevel=64) {(void) fpOut; (void) maxLevel; return B_NO_ERROR;}

// No-op implementation of GetStackTracke(), just return B_NO_ERROR
static inline status_t GetStackTrace(String & retStr, uint32 maxDepth = 64) {(void) retStr; (void) maxDepth; return B_NO_ERROR;}

// No-op version of GetLogLevelName(), just returns a dummy string
static inline const char * GetLogLevelName(int /*logLevel*/) {return "<omitted>";}

// No-op version of GetLogLevelKeyword(), just returns a dummy string
static inline const char * GetLogLevelKeyword(int /*logLevel*/) {return "<omitted>";}
#else

// Define this constant in your Makefile (i.e. -DMUSCLE_MINIMALIST_LOGGING) if you don't want to have
// to link in SysLog.cpp and Hashtable.cpp and all the other stuff that is required for "real" logging.
# ifdef MUSCLE_MINIMALIST_LOGGING
#  define MUSCLE_INLINE_LOGGING

// Minimalist version of Log(), just sends the output to stdout.
static inline status_t Log(int, const char * fmt, ...) {va_list va; va_start(va, fmt); vprintf(fmt, va); va_end(va); return B_NO_ERROR;}

// Minimalist version of LogTime(), just sends a tiny header and the output to stdout.
static inline status_t LogTime(int logLevel, const char * fmt, ...) {printf("%i: ", logLevel); va_list va; va_start(va, fmt); vprintf(fmt, va); va_end(va); return B_NO_ERROR;}

// Minimalist version of WarnOutOfMemory()
static inline void WarnOutOfMemory(const char * file, int line) {printf("ERROR--OUT OF MEMORY!  (%s:%i)\n", file, line);}

// Minimumist version of LogFlush(), just flushes stdout
static inline status_t LogFlush() {fflush(stdout); return B_NO_ERROR;}

// Minimalist version of LogStackTrace(), just prints a dummy string
static inline status_t LogStackTrace(int level = MUSCLE_LOG_INFO, uint32 maxDepth = 64) {(void) level; (void) maxDepth; printf("<stack trace omitted>\n"); return B_NO_ERROR;}

// Minimalist version of PrintStackTrace(), just prints a dummy string
static inline status_t PrintStackTrace(FILE * optFile = NULL, uint32 maxDepth = 64) {(void) maxDepth; fprintf(optFile?optFile:stdout, "<stack trace omitted>\n"); return B_NO_ERROR;}

// Minimalist version of GetStackTracke(), just returns B_NO_ERROR
static inline status_t GetStackTrace(String & /*retStr*/, uint32 maxDepth = 64) {(void) maxDepth; return B_NO_ERROR;}

// Minimalist version of GetLogLevelName(), just returns a dummy string
static inline const char * GetLogLevelName(int /*logLevel*/) {return "<omitted>";}

// Minimalist version of GetLogLevelKeyword(), just returns a dummy string
static inline const char * GetLogLevelKeyword(int /*logLevel*/) {return "<omitted>";}
# endif
#endif

#ifdef MUSCLE_INLINE_LOGGING
inline int ParseLogLevelKeyword(const char *)         {return MUSCLE_LOG_NONE;}
inline int GetFileLogLevel()                          {return MUSCLE_LOG_NONE;}
// Note:  GetFileLogName() is not defined here for the inline-logging case, because it causes chicken-and-egg header problems
//inline String GetFileLogName()                      {return "";}
inline uint32 GetFileLogMaximumSize()                 {return MUSCLE_NO_LIMIT;}
inline uint32 GetMaxNumLogFiles()                     {return MUSCLE_NO_LIMIT;}
inline bool GetFileLogCompressionEnabled()            {return false;}
inline int GetConsoleLogLevel()                       {return MUSCLE_LOG_NONE;}
inline int GetMaxLogLevel()                           {return MUSCLE_LOG_NONE;}
inline status_t SetFileLogLevel(int)                  {return B_NO_ERROR;}
inline status_t SetFileLogName(const String &)        {return B_NO_ERROR;}
inline status_t SetFileLogMaximumSize(uint32)         {return B_NO_ERROR;}
inline status_t SetOldLogFilesPattern(const String &) {return B_NO_ERROR;}
inline status_t SetMaxNumLogFiles(uint32)             {return B_NO_ERROR;}
inline status_t SetFileLogCompressionEnabled(bool)    {return B_NO_ERROR;}
inline status_t SetConsoleLogLevel(int)               {return B_NO_ERROR;}
inline void CloseCurrentLogFile()                     {/* empty */}
#else

/** Returns the MUSCLE_LOG_* equivalent of the given keyword string 
 *  @param keyword a string such as "debug", "log", or "info".
 *  @return A MUSCLE_LOG_* value
 */
int ParseLogLevelKeyword(const char * keyword);

/** Returns the current log level for logging to a file.
 *  @return a MUSCLE_LOG_* value.
 */
int GetFileLogLevel();

/** Returns the user-specified name for the file to log to.
 *  (note that this may be different from the log file name actually used,
 *  since the logging mechanism will choose a name when the log is first opened
 *  if no manually specified name was chosen)
 *  @return a file name or file path representing where the user would like the 
 *          log file to be written.
 */
String GetFileLogName();

/** Returns the maximum size (in bytes) that we will allow the log file(s) to grow to.
  * Default is MUSCLE_NO_LIMIT, i.e. unlimited file size.
  */
uint32 GetFileLogMaximumSize();

/** Returns the maximum number of log files that should be written out before
  * old log files start to be deleted.  Defaults to MUSCLE_NO_LIMIT, i.e.
  * never delete any log files.
  */
uint32 GetMaxNumLogFiles();

/** Returns true if log files are to be gzip-compressed when they are closed;
  * or false if they should be left in raw text form.  Default value is false.
  */
bool GetFileLogCompressionEnabled();

/** Returns the current log level for logging to stdout.
 *  @return a MUSCLE_LOG_* value.
 */
int GetConsoleLogLevel();

/** Returns the max of GetFileLogLevel() and GetConsoleLogLevel()
 *  @return a MUSCLE_LOG_* value
 */
int GetMaxLogLevel();

/** Sets the log filter level for logging to a file.  
 *  Any calls to Log*() that specify a log level greater than (loglevel)
 *  will be suppressed.  Default level is MUSCLE_LOG_NONE (i.e. no file logging is done)
 *  @param loglevel The MUSCLE_LOG_* value to use in determining which log messages to save to disk.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t SetFileLogLevel(int loglevel);

/** Forces the file logger to close any log file that it currently has open. */
void CloseCurrentLogFile();

/** Sets a user-specified name/path for the log file.  This name will
 *  be used whenever a log file is to be opened, instead of the default log file name.
 *  @param logName The string to use, or "" if you'd prefer a log file name be automatically generated.
 *                 Note that this string can contain any of the special tokens described by the
 *                 HumanReadableTimeValues::ExpandTokens() method, and these values will be expanded
 *                 out when the log file is opened.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t SetFileLogName(const String & logName);

/** Sets a user-specified maximum size for the log file.  Once a log file has reached this size,
  * it will be closed and a new log file opened (note that the new log file's name will be the same
  * as the old log file, overwriting it, unless you specify a date/time token via SetFileLogName()
  * that will expand out differently).
  * Default state is no limit on log file size.  (aka MUSCLE_NO_LIMIT)
  * @param maxSizeBytes The maximum allowable log file size, or MUSCLE_NO_LIMIT to allow any size log file.
  * @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
  */
status_t SetFileLogMaximumSize(uint32 maxSizeBytes);

/** Sets the path-pattern of files that the logger is allowed to assume are old log files, and therefore
  * is allowed to delete.
  * @param pattern The pattern to match against (e.g. "/var/log/mylogfiles-*.txt").  The matching will
  *                be done immediately/synchronously inside this call.
  * @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
  */
status_t SetOldLogFilesPattern(const String & pattern);

/** Sets a user-specified maximum number of log files that should be written out before
  * the oldest log files start to be deleted.  This can help keep the filesystem
  * space taken up by log files limited to a finite amount.
  * Default state is no limit on the number of log files.  (aka MUSCLE_NO_LIMIT)
  * @param maxNumLogFiles The maximum allowable number of log files, or MUSCLE_NO_LIMIT to allow any number of log files.
  * @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
  */
status_t SetMaxNumLogFiles(uint32 maxNumLogFiles);

/** Set this to true if you want log files to be compressed when they are closed (and given a .gz extension).
  * or false if they should be left in raw text form.
  * @param enable True to enable log file compression; false otherwise.
  * @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
  */
status_t SetFileLogCompressionEnabled(bool enable);

/** Sets the log filter level for logging to stdout.
 *  Any calls to Log*() that specify a log level greater than (loglevel)
 *  will be suppressed.  Default level is MUSCLE_LOG_INFO.
 *  @param loglevel The MUSCLE_LOG_* value to use in determining which log messages to print to stdout.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t SetConsoleLogLevel(int loglevel);

/** Same semantics as printf, only outputs to the log file/console instead
 *  @param logLevel a MUSCLE_LOG_* value indicating the "severity" of this message.
 *  @param fmt A printf-style format string (e.g. "hello %s\n").  Note that \n is NOT added for you.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t Log(int logLevel, const char * fmt, ...);

/** Calls LogTime() with a critical "OUT OF MEMORY" Message.
  * Note that you typically wouldn't call this function directly;
  * rather you should call the WARN_OUT_OF_MEMORY macro and it will
  * call WarnOutOfMemory() for you, with the correct arguments.
  * @param file Name of the source file where the memory failure occurred.
  * @param line Line number where the memory failure occurred.
  */
void WarnOutOfMemory(const char * file, int line);

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
# if defined(_MSC_VER)
#  define LogTime(logLevel, ...)     _LogTime(__FILE__, __FUNCTION__, __LINE__, logLevel, __VA_ARGS__)
# elif defined(__GNUC__)
#  define LogTime(logLevel, args...) _LogTime(__FILE__, __FUNCTION__, __LINE__, logLevel, args)
# else
#  define LogTime(logLevel, args...) _LogTime(__FILE__, "",           __LINE__, logLevel, args)
# endif
status_t _LogTime(const char * sourceFile, const char * optSourceFunction, int line, int logLevel, const char * fmt, ...);
#else

/** Formatted.  Automagically prepends a timestamp and status string to your string.
 *  e.g. LogTime(MUSCLE_LOG_INFO, "Hello %s!", "world") would generate "[I 12/18 12:11:49] Hello world!"
 *  @param logLevel a MUSCLE_LOG_* value indicating the "severity" of this message.
 *  @param fmt A printf-style format string (e.g. "hello %s\n").  Note that \n is NOT added for you.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t LogTime(int logLevel, const char * fmt, ...);

#endif

/** Ensures that all previously logged output is actually sent.  That is, it simply 
 *  calls fflush() on any streams that we are logging to.
 *  @returns B_NO_ERROR on success, or B_ERROR if the log lock couldn't be locked for some reason.
 */
status_t LogFlush();

/** Attempts to lock the Mutex that is used to serialize LogCallback calls.
  * Typically you won't need to call this function, as it is called for you
  * before any LogCallback calls are made.
  * @returns B_NO_ERROR on success or B_ERROR on failure.
  * @note Be sure to call UnlockLog() when you are done!
  */
status_t LockLog();

/** Unlocks the Mutex that is used to serialize LogCallback calls.
  * Typically you won't need to call this function, as it is called for you
  * after any LogCallback calls are made.  The only time you need to call it
  * is after you've made a call to LockLog() and are now done with your critical
  * section.
  * @returns B_NO_ERROR on success or B_ERROR on failure.
  */
status_t UnlockLog();

/** This is similar to LogStackTrace(), except that the stack trace is printed directly
  * to stdout (or another file you specify) instead of via calls to Log() and LogTime().  
  * This call is handy when you need to print a stack trace in situations where the log
  * isn't available.
  * @param optFile If non-NULL, the text will be printed to this file.  If left as NULL, stdout will be used as a default.
  * @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
  *                 64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
  * @note This function is currently only implemented under Linux and MacOS/X Leopard; for other OS's, this function is a no-op.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t PrintStackTrace(FILE * optFile = NULL, uint32 maxDepth = 64);

/** Logs out a stack trace, if possible.  Returns B_ERROR if not.
 *  @note Currently only works under Linux and MacOS/X Leopard, and then only if -rdynamic is specified as a compile flag.
 *  @param logLevel a MUSCLE_LOG_* value indicating the "severity" of this message.
 *  @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
 *                  64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
 *  @returns B_NO_ERROR on success, or B_ERROR if a stack trace couldn't be logged because the platform doesn't support it.
 */
status_t LogStackTrace(int logLevel = MUSCLE_LOG_INFO, uint32 maxDepth = 64);

/** Similar to LogStackTrace(), except that the current stack trace is returned as a String
  * instead of being printed out anywhere.
  * @param retStr On success, the stack trace is written to this String object.
  * @param maxDepth The maximum number of levels of stack trace that we should print out.  Defaults to
  *                 64.  The absolute maximum is 256; if you specify a value higher than that, you will still get 256.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  * @note This function is currently only implemented under Linux and MacOS/X Leopard; for other OS's, this function is a no-op.
  */
status_t GetStackTrace(String & retStr, uint32 maxDepth = 64);

/** Returns a human-readable string for the given log level.
 *  @param logLevel A MUSCLE_LOG_* value
 *  @return A pretty human-readable description string such as "Informational" or "Warnings and Errors Only"
 */
const char * GetLogLevelName(int logLevel);

/** Returns a brief human-readable string for the given log level.
 *  @param logLevel A MUSCLE_LOG_* value
 *  @return A brief human-readable description string such as "info" or "warn"
 */
const char * GetLogLevelKeyword(int logLevel);

/** Writes a standard text string of the format "[L mm/dd hh:mm:ss]" into (buf).
 *  @param buf Char buffer to write into.  Should be at least 64 chars long.
 *  @param lca A LogCallbackArgs object containing the time stamp and severity that are appropriate to print.
 */
void GetStandardLogLinePreamble(char * buf, const LogCallbackArgs & lca);

/** Given a source location (e.g. as provided by the information in a LogCallbackArgs object),
  * returns a corresponding uint32 that represents a hash of that location.
  * The source code location can be later looked up by feeding this hash value
  * as a command line argument into the muscle/tests/findsourcecodelocations program.
  */
uint32 GenerateSourceCodeLocationKey(const char * fileName, uint32 lineNumber);

/** Given a source-code location key (as returned by GenerateSourceCodeLocationKey()),
  * returns the standard human-readable representation of that value.  (e.g. "7QF2")
  */
String SourceCodeLocationKeyToString(uint32 key);

/** Given a standard human-readable representation of a source-code-location
  * key (e.g. "7EF2"), returns the uint16 key value.  This is the inverse
  * function of SourceCodeLocationKeyToString().
  */
uint32 SourceCodeLocationKeyFromString(const String & s);

#endif

/** This class represents all the fields necessary to present a human with a human-readable time/date stamp.  Objects of this class are typically populated by the GetHumanReadableTimeValues() function, below. */
class HumanReadableTimeValues
{
public:
   /** Default constructor */
   HumanReadableTimeValues() : _year(0), _month(0), _dayOfMonth(0), _dayOfWeek(0), _hour(0), _minute(0), _second(0), _microsecond(0) {/* empty */}

   /** Explicit constructor
     * @param year The year value (e.g. 2005)
     * @param month The month value (January=0, February=1, etc)
     * @param dayOfMonth The day within the month (ranges from 0 to 30, inclusive)
     * @param dayOfWeek The day within the week (Sunday=0, Monday=1, etc)
     * @param hour The hour within the day (ranges from 0 to 23, inclusive)
     * @param minute The minute within the hour (ranges from 0 to 59, inclusive)
     * @param second The second within the minute (ranges from 0 to 59, inclusive)
     * @param microsecond The microsecond within the second (ranges from 0 to 999999, inclusive)
     */
   HumanReadableTimeValues(int year, int month, int dayOfMonth, int dayOfWeek, int hour, int minute, int second, int microsecond) : _year(year), _month(month), _dayOfMonth(dayOfMonth), _dayOfWeek(dayOfWeek), _hour(hour), _minute(minute), _second(second), _microsecond(microsecond) {/* empty */}

   /** Returns the year value (e.g. 2005) */
   int GetYear() const {return _year;}

   /** Returns the month value (January=0, February=1, March=2, ..., December=11). */
   int GetMonth() const {return _month;}

   /** Returns the day-of-month value (which ranges between 0 and 30, inclusive). */
   int GetDayOfMonth() const {return _dayOfMonth;}

   /** Returns the day-of-week value (Sunday=0, Monday=1, Tuesday=2, Wednesday=3, Thursday=4, Friday=5, Saturday=6). */
   int GetDayOfWeek() const {return _dayOfWeek;}

   /** Returns the hour value (which ranges between 0 and 23, inclusive). */
   int GetHour() const {return _hour;}

   /** Returns the minute value (which ranges between 0 and 59, inclusive). */
   int GetMinute() const {return _minute;}

   /** Returns the second value (which ranges between 0 and 59, inclusive). */
   int GetSecond() const {return _second;}

   /** Returns the microsecond value (which ranges between 0 and 999999, inclusive). */
   int GetMicrosecond() const {return _microsecond;}

   /** Sets the year value (e.g. 2005) */
   void SetYear(int year) {_year = year;}

   /** Sets the month value (January=0, February=1, March=2, ..., December=11). */
   void SetMonth(int month) {_month = month;}

   /** Sets the day-of-month value (which ranges between 0 and 30, inclusive). */
   void SetDayOfMonth(int dayOfMonth) {_dayOfMonth = dayOfMonth;}

   /** Sets the day-of-week value (Sunday=0, Monday=1, Tuesday=2, Wednesday=3, Thursday=4, Friday=5, Saturday=6). */
   void SetDayOfWeek(int dayOfWeek) {_dayOfWeek = dayOfWeek;}

   /** Sets the hour value (which ranges between 0 and 23, inclusive). */
   void SetHour(int hour) {_hour = hour;}

   /** Sets the minute value (which ranges between 0 and 59, inclusive). */
   void SetMinute(int minute) {_minute = minute;}

   /** Sets the second value (which ranges between 0 and 59, inclusive). */
   void SetSecond(int second) {_second = second;}

   /** Sets the microsecond value (which ranges between 0 and 999999, inclusive). */
   void SetMicrosecond(int microsecond) {_microsecond = microsecond;}

   /** Equality operator. */
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

   /** Inequality operator */
   bool operator != (const HumanReadableTimeValues & rhs) const {return !(*this==rhs);}

   /** This method will expand the following tokens in the specified String out to the following values:
     * %Y -> Current year (e.g. "2005")
     * %M -> Current month (e.g. "01" for January, up to "12" for December)
     * %Q -> Current month as a string (e.g. "January", "February", "March", etc)
     * %D -> Current day of the month (e.g. "01" through "31")
     * %d -> Current day of the month (e.g. "01" through "31") (synonym for %D)
     * %W -> Current day of the week (e.g. "1" through "7")
     * %w -> Current day of the week (e.g. "1" through "7") (synonym for %W)
     * %q -> Current day of the week as a string (e.g. "Sunday", "Monday", "Tuesday", etc)
     * %h -> Current hour (military style:  e.g. "00" through "23")
     * %m -> Current minute (e.g. "00" through "59")
     * %s -> Current second (e.g. "00" through "59")
     * %x -> Current microsecond (e.g. "000000" through "999999", inclusive)
     * %r -> A random number between 0 and (2^64-1) (for spicing up the uniqueness of a filename)
     * %T -> A human-readable time/date stamp, for convenience (e.g. "January 01 2005 23:59:59")
     * %t -> A numeric time/date stamp, for convenience (e.g. "2005/01/01 15:23:59")
     * %f -> A filename-friendly numeric time/date stamp, for convenience (e.g. "2005-01-01_15h23m59")
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
   MUSCLE_TIMEZONE_UTC = 0, // Universal Co-ordinated Time (formerly Greenwhich Mean Time)
   MUSCLE_TIMEZONE_LOCAL    // Host machine's local time (depends on local time zone settings)
};

/** Given a uint64 representing a time in microseconds since 1970,
  * (e.g. as returned by GetCurrentTime64()), returns the same value
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
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t GetHumanReadableTimeValues(uint64 timeUS, HumanReadableTimeValues & retValues, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** This function is the inverse operation of GetHumanReadableTimeValues().  Given a HumanReadableTimeValues object,
  * this function returns the corresponding microseconds-since-1970 value.
  * @param values The HumanReadableTimeValues object to examine
  * @param retTimeUS On success, the corresponding uint64 is written here (in microseconds-since-1970)
  * @param timeType If set to MUSCLE_TIMEZONE_UTC (the default) then (values) will be interpreted as being in UTC, 
  *                 and (retTimeUS) be converted to the local time zone as part of the conversion process.  If set to 
  *                 MUSCLE_TIMEZONE_LOCAL, on the other hand, then no time zone conversion will be done.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t GetTimeStampFromHumanReadableTimeValues(const HumanReadableTimeValues & values, uint64 & retTimeUS, uint32 timeType = MUSCLE_TIMEZONE_UTC);

/** Given a uint64 representing a time in microseconds since 1970,
  * (e.g. as returned by GetCurrentTime64()), returns an equivalent 
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
uint64 ParseHumanReadableTimeString(const String & str, uint32 timeType = MUSCLE_TIMEZONE_UTC);

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
  * As a special case, the string "forever" will parse to MUSCLE_TIME_NEVER.
  * If no suffix is supplied, the units are presumed to be in seconds.
  * @param str The string to parse 
  * @returns a time interval value, in microseconds.
  */
uint64 ParseHumanReadableTimeIntervalString(const String & str);

/** Given a time interval specified in microseconds, returns a human-readable
  * string representing that time, e.g. "3 weeks, 2 days, 1 hour, 25 minutes, 2 seconds, 350 microseconds"
  * @param micros The number of microseconds to describe
  * @param maxClauses The maximum number of clauses to allow in the returned string.  For example, passing this in
  *                   as 1 might return "3 weeks", while passing this in as two might return "3 weeks, 2 days".
  *                   Default value is MUSCLE_NO_LIMIT, indicating that no maximum should be enforced.
  * @param minPrecisionMicros The maximum number of microseconds the routine is allowed to ignore
  *                           when generating its string.  For example, if this value was passed in
  *                           as 1000000, the returned string would describe the interval down to
  *                           the nearest second.  Defaults to zero for complete accuracy.
  * @param optRetIsAccurate If non-NULL, this value will be set to true if the returned string represents
  *                         (micros) down to the nearest microsecond, or false if the string is an approximation.
  * @returns a human-readable time interval description string.
  */
String GetHumanReadableTimeIntervalString(uint64 micros, uint32 maxClauses = MUSCLE_NO_LIMIT, uint64 minPrecisionMicros = 0, bool * optRetIsAccurate = NULL);

}; // end namespace muscle

#endif
