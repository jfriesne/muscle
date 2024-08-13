/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.TXT file for details. */

#ifndef MuscleLogCallback_h
#define MuscleLogCallback_h

#include "dataio/FileDataIO.h"
#include "syslog/SysLog.h"
#include "util/CountedObject.h"
#include "util/RefCount.h"
#include "util/String.h"

namespace muscle {

class LogLineCallback;

/** This class encapsulates the information that is sent to the Log() and LogLine() callback methods of the LogCallback and LogLineCallback classes.  By putting all the information into a class object, we only have to push one parameter onto the stack with each call instead of many. */
class LogCallbackArgs MUSCLE_FINAL_CLASS
{
public:
   /** Default Constructor */
   LogCallbackArgs() : _when(0), _logLevel(MUSCLE_LOG_INFO), _sourceFile(""), _sourceFunction(""), _sourceLine(0), _text(""), _argList(NULL), _dummyArgListInitialized(false) {/* empty */}

   /** Constructor
     * @param when Timestamp for this log message, in (seconds past 1970) format.
     * @param logLevel The MUSCLE_LOG_* severity level of this log message
     * @param sourceFile The name of the source code file that contains the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as "".
     * @param sourceFunction The name of the source code function that contains the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as "".
     * @param sourceLine The line number of the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as -1.
     * @param literalText A literal/exact text string to log.
     */
   LogCallbackArgs(const time_t & when, int logLevel, const char * sourceFile, const char * sourceFunction, int sourceLine, const char * literalText) : _when(when), _logLevel(logLevel), _sourceFile(sourceFile), _sourceFunction(sourceFunction), _sourceLine(sourceLine), _text(literalText), _argList(NULL), _dummyArgListInitialized(false) {/* empty */}

   /** Constructor
     * @param when Timestamp for this log message, in (seconds past 1970) format.
     * @param logLevel The MUSCLE_LOG_* severity level of this log message
     * @param sourceFile The name of the source code file that contains the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as "".
     * @param sourceFunction The name of the source code function that contains the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as "".
     * @param sourceLine The line number of the LogLine() call that generated this callback.
     *                   Note that this parameter will only be valid if -DMUSCLE_INCLUDE_SOURCE_CODE_LOCATION_IN_LOGTIME
     *                   was defined when muscle was compiled.  Otherwise this value may be passed as -1.
     * @param formatString A printf-style text-format-specifier string.  May contain percent-tokens such as
                               "%s" and "%i" that will be expanded according to the provided va_list, before the text is logged.
     * @param argList Reference to a va_list object that will be used to expand (formatString).
     */
   LogCallbackArgs(const time_t & when, int logLevel, const char * sourceFile, const char * sourceFunction, int sourceLine, const char * formatString, va_list & argList) : _when(when), _logLevel(logLevel), _sourceFile(sourceFile), _sourceFunction(sourceFunction), _sourceLine(sourceLine), _text(formatString), _argList(&argList), _dummyArgListInitialized(false) {/* empty */}

   /** Destructor */
   ~LogCallbackArgs() {if (_dummyArgListInitialized) va_end(_dummyArgList);}

   /** Returns the timestamp indicating when this message was generated, in (seconds since 1970) format. */
   MUSCLE_NODISCARD const time_t & GetWhen() const {return _when;}

   /** Returns the MUSCLE_LOG_* severity level of this log message. */
   MUSCLE_NODISCARD int GetLogLevel() const {return _logLevel;}

   /** Returns the name of the source code file that contains the LogLine() call that generated this callback, or "" if it's not available.  */
   MUSCLE_NODISCARD const char * GetSourceFile() const {return _sourceFile;}

  /** Returns the name of the source code function that contains the LogLine() call that generated this callback,
    * or "" if it's not available.
    */
   MUSCLE_NODISCARD const char * GetSourceFunction() const {return _sourceFunction;}

   /** Returns the line number of the LogLine() call that generated this callback, or -1 if it's not available. */
   MUSCLE_NODISCARD int GetSourceLineNumber() const {return _sourceLine;}

   /** Returns the format text if this object is being passed in a Log() callback.  If this object is being passed
     * in a LogLine() callback, this will be the verbatim text of the line.
     */
   MUSCLE_NODISCARD const char * GetText() const {return _text;}

   /** In a Log() callback, this returns a reference to a va_list object that can be used to expand (text).
     * In a LogLine() callback, this method should not be called as it isn't useful for anything in that context.
     */
   MUSCLE_NODISCARD va_list & GetArgList() const {return _argList ? *_argList : GetDummyArgList("");}

private:
   friend class LogLineCallback;

   LogCallbackArgs(const LogCallbackArgs & copyMe, const char * optNewLiteralText) : _when(copyMe._when), _logLevel(copyMe._logLevel), _sourceFile(copyMe._sourceFile), _sourceFunction(copyMe._sourceFunction), _sourceLine(copyMe._sourceLine), _text(optNewLiteralText ? optNewLiteralText : copyMe._text), _argList(NULL), _dummyArgListInitialized(false) {/* empty */}

   MUSCLE_NODISCARD va_list & GetDummyArgList(const char * dummyFormat, ...) const
   {
      if (_dummyArgListInitialized == false)
      {
         va_start(_dummyArgList, dummyFormat);
         _dummyArgListInitialized = true;
      }
      return _dummyArgList;
   }

   time_t _when;
   int _logLevel;
   const char * _sourceFile;
   const char * _sourceFunction;
   int _sourceLine;
   const char * _text;
   va_list * _argList;

   mutable va_list _dummyArgList;
   mutable bool _dummyArgListInitialized;
};

/** Callback object that can be added with PutLogCallback()
 *  Whenever LogPlain() or LogTime()  are called, all added LogCallback
 *  objects (that are interested in log messages of that severity)
 *  will have their Log() callback-methods called.  All calls to Log()
 *  are synchronized via a process-global Mutex, so they will be thread-safe.
 */
class LogCallback : public RefCountable
{
public:
   /** Constructor
     * @param defaultLogLevelThreshold the default logging threshold; log levels less severe than
     *                                 this will not be processed by this LogCallback (unless you
     *                                 later call SetLogLevelThreshold() to change the value)
     *                                 Defaults to MUSCLE_LOG_INFO.
     */
   LogCallback(int defaultLogLevelThreshold = MUSCLE_LOG_INFO) : _logLevelThreshold(defaultLogLevelThreshold) {/* empty */}

   /** Destructor, to keep C++ honest */
   virtual ~LogCallback() {/* empty */}

   /** Callback method.  Called whenever a message is logged with Log() or LogTime().
    *  @param a LogCallbackArgs object containing all the arguments to this method.
    */
   virtual void Log(const LogCallbackArgs & a) = 0;

   /** Callback method.  When this method is called, the callback should flush any
     * held buffers out.  (ie call fflush() or whatever)
     */
   virtual void Flush() = 0;

   /** Sets our current MUSCLE_LOG_* log level threshold to a different value.
     * @param logLevelThreshold the new MUSCLE_LOG_* log level threshold to use.
     * @returns B_NO_ERROR on success, or an error value on failure.
     * Logging calls whose severity-value is greater than this value will not be passed to this callback.
     */
   void SetLogLevelThreshold(int logLevelThreshold);

   /** Returns our current MUSCLE_LOG_* log level threshold.
     * Logging calls whose severity-value is greater than this value will not be passed to this callback.
     * Default value is MUSCLE_LOG_INFO.
     */
   MUSCLE_NODISCARD int GetLogLevelThreshold() const {return _logLevelThreshold;}

private:
   int _logLevelThreshold;

   DECLARE_COUNTED_OBJECT(LogCallback);
};
DECLARE_REFTYPES(LogCallback);

/** Specialization of LogCallback that parses the Log() calls
 *  into nicely formatted lines of text, then calls LogLine()
 *  to hand them to your code.  Easier than having lots of classes
 *  that all have to do this themselves.  Assumes that all log
 *  lines will be less than 2048 characters long.
 */
class LogLineCallback : public LogCallback
{
public:
   /** Constructor */
   LogLineCallback();

   /** Destructor */
   virtual ~LogLineCallback();

   /** Implemented to call LogLine() when appropriate
     * @param a all of the information about this log call (severity, text, etc)
     */
   virtual void Log(const LogCallbackArgs & a) {return LogAux(a);}

   /** Implemented to call LogLine() when appropriate */
   virtual void Flush();

protected:
   /** This will be called whenever a fully-formed line of log text is ready.
     * implement it to do whatever you like with the text.
     * @param a The log callback arguments.  The (format) string
     *          in this case will always be a literal string that can be printed verbatim.
     */
   virtual void LogLine(const LogCallbackArgs & a) = 0;

private:
   void LogAux(const LogCallbackArgs & a);

   LogCallbackArgs _lastLog; // stored for use by Flush()
   char * _writeTo;     // points to the next spot in (_buf) to muscleSprintf() into
   char _buf[2048];     // where we assemble our text

   DECLARE_COUNTED_OBJECT(LogLineCallback);
};
DECLARE_REFTYPES(LogLineCallback);

/** Add a custom LogCallback object to the global log callbacks set.
 *  @param cbRef Reference to a LogCallback object.
 *  @returns B_NO_ERROR on success, or B_LOCK_FAILED if the log-lock couldn't be locked.
 */
status_t PutLogCallback(const LogCallbackRef & cbRef);

/** Removes the given callback from our list.
 *  @param cbRef Reference of the LogCallback to remove from the callback list.
 *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the given callback wasn't found, or B_LOCK_FAILED if the log-lock couldn't be locked.
 */
status_t RemoveLogCallback(const LogCallbackRef & cbRef);

/** Removes all custom log callbacks from the callback-set */
void ClearLogCallbacks();

/** This class is used to send log information to stdout.  An object of this class is instantiated
  * and used internally by MUSCLE, so typically you don't need to instantiate one yourself, but the class
  * is exposed here anyway in case it might come in useful for other reasons.
  */
class DefaultConsoleLogger : public LogCallback
{
public:
   /** Constructor
     * @param defaultLogLevelThreshold the default logging threshold; log levels less severe than
     *                                 this will not be processed by this LogCallback (unless you
     *                                 later call SetLogLevelThreshold() to change the value)
     *                                 Defaults to MUSCLE_LOG_INFO.
     */
   DefaultConsoleLogger(int defaultLogLevelThreshold = MUSCLE_LOG_INFO);

   virtual void Log(const LogCallbackArgs & a);
   virtual void Flush();

   /** Sets whether we should log to stderr instead of stdout.
     * @param toStderr true to log to stderr; false to log to stdout.  (Default is false)
     */
   void SetConsoleLogToStderr(bool toStderr) {_logToStderr = toStderr;}

   /** Returns true iff we are currently set to log to stderr instead of stdout */
   MUSCLE_NODISCARD bool GetConsoleLogToStderr() const {return _logToStderr;}

private:
   FILE * GetConsoleOutputStream() const {return _logToStderr ? stderr : stdout;}
   bool _firstCall;
   bool _logToStderr;
};
DECLARE_REFTYPES(DefaultConsoleLogger);

/** This class is used to send log information to a file, rotate log files, etc.  An object of this class
  * is instantiated and used internally by MUSCLE, so typically you don't need to instantiate one yourself,
  * but the class is exposed here anyway in case it comes in useful for other reasons (eg for creating and
  * rotating a separate set of log files in an additional directory)
  */
class DefaultFileLogger : public LogCallback
{
public:
   /** Constructor
     * @param defaultLogLevelThreshold the default logging threshold; log levels less severe than
     *                                 this will not be processed by this LogCallback (unless you
     *                                 later call SetLogLevelThreshold() to change the value)
     *                                 Defaults to MUSCLE_LOG_INFO.
     */
   DefaultFileLogger(int defaultLogLevelThreshold = MUSCLE_LOG_INFO);

   virtual ~DefaultFileLogger();

   virtual void Log(const LogCallbackArgs & a);

   virtual void Flush();

   /** Specify a pattern of already-existing log files to include in our log-file-history.
     * This would be called at startup, in case there are old log files already extant from previous runs.
     * @param filePattern a filepath with wildcards indicating which files to match on.
     * @returns the number of existing files found and added to our files-list.
     */
   uint32 AddPreExistingLogFiles(const String & filePattern);

   /** Returns the name of the log file we will output to.  Default is an empty string (ie file logging disabled) */
   MUSCLE_NODISCARD const String & GetFileLogName() const {return _prototypeLogFileName;}

   /** Returns the maximum size of the log file we will output to.  When the file reaches this size we'll create another.  Default is MUSCLE_NO_LIMIT (aka no maximum size). */
   MUSCLE_NODISCARD uint32 GetMaxLogFileSize() const {return _maxLogFileSize;}

   /** Returns the maximum number of log files we will keep present on disk at once (before starting to delete the old ones).  Default is MUSCLE_NO_LIMIT (aka no maximum number of files) */
   MUSCLE_NODISCARD uint32 GetMaxNumLogFiles() const {return _maxNumLogFiles;}

   /** Returns whether or not we should compress old log files to save disk space.  Defaults to false. */
   MUSCLE_NODISCARD bool GetFileCompressionEnabled() {return _compressionEnabled;}

   /** Sets the name of the file to log to.
     * @param logName File name/path (including %-tokens as necessary)
     */
   void SetLogFileName(const String & logName) {_prototypeLogFileName = logName;}

   /** Sets the maximum log file size, in bytes.
     * @param maxSizeBytes New maximum log file size.
     */
   void SetMaxLogFileSize(uint32 maxSizeBytes) {_maxLogFileSize = maxSizeBytes;}

   /** Sets the maximum allowed number of log files.
     * @param maxNumLogFiles New maximum number of log files, or MUSCLE_NO_LIMIT.
     */
   void SetMaxNumLogFiles(uint32 maxNumLogFiles) {_maxNumLogFiles = maxNumLogFiles;}

   /** Set whether old log files should be gzip-compressed, or not.
     * @param enable True if old log files should be compressed, or false otherwise.
     */
   void SetFileCompressionEnabled(bool enable) {_compressionEnabled = enable;}

   /** Forces the closing of any log file that we currently have open. */
   void CloseLogFile();

protected:
   /** May be overridden by a subclass to generate a line of text that will be placed at the top of
     * each generated log file.  Default implementation always returns an empty string.
     * @param a Info about the first log message that will be placed into the new file.
     */
   virtual String GetLogFileHeaderString(const LogCallbackArgs & a) const {(void) a; return GetEmptyString();}

private:
   status_t EnsureLogFileCreated(const LogCallbackArgs & a);

   String _prototypeLogFileName;
   uint32 _maxLogFileSize;
   uint32 _maxNumLogFiles;
   bool _compressionEnabled;

   String _activeLogFileName;
   FileDataIO _logFile;
   bool _logFileOpenAttemptFailed;
   Queue<String> _oldLogFileNames;

#ifdef WIN32
   uint64 _lastGetAttributesTime;
#endif
};
DECLARE_REFTYPES(DefaultFileLogger);

} // end namespace muscle

#endif
