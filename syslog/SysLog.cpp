/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MUSCLE_AVOID_CPLUSPLUS11
# include <random>  // for random_device
#endif

#include "dataio/FileDataIO.h"
#include "regex/StringMatcher.h"
#include "syslog/LogCallback.h"
#include "system/SetupSystem.h"
#include "system/SystemInfo.h"  // for GetFilePathSeparator()
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "util/Hashtable.h"
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()
#include "util/NestCount.h"
#include "util/String.h"
#include "util/StringTokenizer.h"

#if !defined(MUSCLE_INLINE_LOGGING) && defined(MUSCLE_ENABLE_ZLIB_ENCODING)
# include "zlib.h"  // deliberately pathless, to avoid mixing captive headers with system libz
#endif

namespace muscle {

// Win32 doesn't have localtime_r, so we have to roll our own
#if defined(WIN32)
static inline struct tm * muscle_localtime_r(time_t * clock, struct tm * result)
{
   // Note that in Win32, (ret) points to thread-local storage, so this really
   // is thread-safe despite the fact that it looks like it isn't!
#if __STDC_WANT_SECURE_LIB__
   (void) localtime_s(result, clock);
   return result;
#else
   struct tm temp;
   struct tm * ret = localtime_r(clock, &temp);
   if (ret) *result = *ret;
   return ret;
#endif
}
static inline struct tm * muscle_gmtime_r(time_t * clock, struct tm * result)
{
   // Note that in Win32, (ret) points to thread-local storage, so this really
   // is thread-safe despite the fact that it looks like it isn't!
#if __STDC_WANT_SECURE_LIB__
   (void) gmtime_s(result, clock);
   return result;
#else
   struct tm temp;
   struct tm * ret = gmtime_r(clock, &temp);
   if (ret) *result = *ret;
   return ret;
#endif
}
#else
static inline struct tm * muscle_localtime_r(time_t * clock, struct tm * result) {return localtime_r(clock, result);}
static inline struct tm * muscle_gmtime_r(   time_t * clock, struct tm * result) {return gmtime_r(clock, result);}
#endif

#ifndef MUSCLE_INLINE_LOGGING

static NestCount _inLogPreamble;

static const char * const _logLevelNames[] = {
   "None",
   "Critical Errors Only",
   "Errors Only",
   "Warnings and Errors Only",
   "Informational",
   "Debug",
   "Trace"
};

static const char * const _logLevelKeywords[] = {
   "none",
   "critical",
   "errors",
   "warnings",
   "info",
   "debug",
   "trace"
};

namespace muscle_private
{
#ifdef MUSCLE_AVOID_CPLUSPLUS11
int _maxLogThreshold = MUSCLE_LOG_INFO;  // I guess we'll take our chances here
#else
std::atomic<int> _maxLogThreshold(MUSCLE_LOG_INFO);
#endif
};

DefaultConsoleLogger :: DefaultConsoleLogger(int defaultLogThreshold)
   : LogCallback(defaultLogThreshold)
   , _firstCall(true)
   , _logToStderr(false)
{
    // empty
}

void DefaultConsoleLogger :: Log(const LogCallbackArgs & a, va_list & argList)
{
   if (_firstCall)
   {
      _firstCall = false;

      // Useful for cases where we can't wait for the logtostderr
      // command line argument to get parsed, because by then some
      // output has already gone to stdout and e.g. our .tgz file now
      // has some ASCII text where its tar-file headers should be :/
#ifdef _MSC_VER
# pragma warning( push )
# pragma warning( disable: 4996 )
#endif
      if (getenv("MUSCLE_LOG_TO_STDERR") != NULL) _logToStderr = true;
#ifdef _MSC_VER
# pragma warning( pop )
#endif
   }

   FILE * fpOut = GetConsoleOutputStream();
   vfprintf(fpOut, a.GetText(), argList);
   fflush(fpOut);
}

void DefaultConsoleLogger :: Flush()
{
   fflush(GetConsoleOutputStream());
}

DefaultFileLogger :: DefaultFileLogger(int defaultLogThreshold)
   : LogCallback(defaultLogThreshold)
   , _maxLogFileSize(MUSCLE_NO_LIMIT)
   , _maxNumLogFiles(MUSCLE_NO_LIMIT)
   , _compressionEnabled(false)
   , _logFileOpenAttemptFailed(false)
#ifdef WIN32
   , _lastGetAttributesTime(0)
#endif
{
   // empty
}

DefaultFileLogger :: ~DefaultFileLogger()
{
   CloseLogFile();
}

void DefaultFileLogger :: Log(const LogCallbackArgs & a, va_list & argList)
{
   if (EnsureLogFileCreated(a).IsOK())
   {
      vfprintf(_logFile.GetFile(), a.GetText(), argList);
      _logFile.FlushOutput();

#ifdef WIN32
      // Hack fix: Force Windows to at least occasionally update the file-size indicator
      // https://stackoverflow.com/questions/76049891/how-to-programatically-force-windows-to-update-a-files-reported-on-disk-size-w
      if (OnceEvery(SecondsToMicros(1), _lastGetAttributesTime)) (void) GetFileAttributesA(_activeLogFileName());
#endif

      if ((_maxLogFileSize != MUSCLE_NO_LIMIT)&&(_inLogPreamble.IsInBatch() == false))  // wait until we're outside the preamble to avoid breaking up lines too much
      {
         const int64 curFileSize = _logFile.GetPosition();
         if ((curFileSize < 0)||(curFileSize >= (int64)_maxLogFileSize))
         {
            const uint32 tempStoreSize = _maxLogFileSize;
            _maxLogFileSize = MUSCLE_NO_LIMIT;  // otherwise we'd recurse indefinitely here!
            CloseLogFile();
            _maxLogFileSize = tempStoreSize;
            (void) EnsureLogFileCreated(a);  // force the opening of the new log file right now, so that the open message show up in the right order
         }
      }
   }
}

void DefaultFileLogger :: Flush()
{
   _logFile.FlushOutput();
}

uint32 DefaultFileLogger :: AddPreExistingLogFiles(const String & filePattern)
{
   String dirPart, filePart;
   const int32 lastSlash = filePattern.LastIndexOf(GetFilePathSeparator());
   if (lastSlash >= 0)
   {
      dirPart = filePattern.Substring(0, lastSlash);
      filePart = filePattern.Substring(lastSlash+1);
   }
   else
   {
      dirPart  = ".";
      filePart = filePattern;
   }

   Hashtable<String, uint64> pathToTime;
   if (filePart.HasChars())
   {
      StringMatcher sm(filePart);

      Directory d(dirPart());
      if (d.IsValid())
      {
         const char * nextName;
         while((nextName = d.GetCurrentFileName()) != NULL)
         {
            const String fn = nextName;
            if (sm.Match(fn))
            {
               const String fullPath = dirPart+GetFilePathSeparator()+fn;
               FilePathInfo fpi(fullPath());
               if ((fpi.Exists())&&(fpi.IsRegularFile())) (void) pathToTime.Put(fullPath, fpi.GetCreationTime());
            }
            d++;
         }
      }

      // Now we sort by creation time...
      pathToTime.SortByValue();

      // And add the results to our _oldFileNames queue.  That way when the log file is opened, the oldest files will be deleted (if appropriate)
      for (HashtableIterator<String, uint64> iter(pathToTime); iter.HasData(); iter++) (void) _oldLogFileNames.AddTail(iter.GetKey());
   }
   return pathToTime.GetNumItems();
}

static status_t OpenLogFileForWriting(String & logFileName, FileDataIO & fdio)
{
   // First, try to open the log file using its name verbatim
   fdio.SetFile(muscleFopen(logFileName(), "wx"));  // x means "fail if the file already exists"
   if (fdio.GetFile() != NULL) return B_NO_ERROR;

   // If that didn't work, perhaps someone else already has a log file open with that name.
   // Let's see if we can open a log file with an alternate name instead.
   const int32 lastDotIdx = logFileName.LastIndexOf('.');
   for (int i=0; i<10; i++)  // we'll only try a few times; since if we're trying to e.g. write to a read-only partition, this will never work anyway
   {
      const String infix         = String("%1").Arg((i<8)?((uint64)(i+2)):GetRunTime64());  // insert this additional text into the filename to (hopefully) make a unique filename
      const String alternateName = (lastDotIdx >= 0) ? String("%1_%2.%3").Arg(logFileName.Substring(0, lastDotIdx)).Arg(infix).Arg(logFileName.Substring(lastDotIdx+1)) : String("%1_%2").Arg(logFileName).Arg(infix);
      fdio.SetFile(muscleFopen(alternateName(), "wx"));  // x means "fail if the file already exists"
      if (fdio.GetFile() != NULL)
      {
         logFileName = std_move_if_available(alternateName);
         return B_NO_ERROR;
      }
   }

   return B_IO_ERROR;  // no luck :(
}

status_t DefaultFileLogger :: EnsureLogFileCreated(const LogCallbackArgs & a)
{
   if ((_logFile.GetFile() == NULL)&&(_logFileOpenAttemptFailed == false))
   {
      String logFileName = _prototypeLogFileName;
      if (logFileName.IsEmpty()) logFileName = "%f.log";

      HumanReadableTimeValues hrtv; (void) GetHumanReadableTimeValues(SecondsToMicros(a.GetWhen()), hrtv);
      logFileName = hrtv.ExpandTokens(logFileName);

      (void) OpenLogFileForWriting(logFileName, _logFile);
      if (_logFile.GetFile() != NULL)
      {
#ifdef WIN32
         _lastGetAttributesTime = GetRunTime64();
#endif
         _activeLogFileName = std_move_if_available(logFileName);
         LogTime(MUSCLE_LOG_DEBUG, "Created Log file [%s]\n", _activeLogFileName());

         while(_oldLogFileNames.GetNumItems() >= _maxNumLogFiles)
         {
            const char * c = _oldLogFileNames.Head()();
                 if (remove(c) == 0)  LogTime(MUSCLE_LOG_DEBUG, "Deleted old Log file [%s]\n", c);
            else if (errno != ENOENT) LogTime(MUSCLE_LOG_ERROR, "Error [%s] deleting old Log file [%s]\n", B_ERRNO(), c);
            (void) _oldLogFileNames.RemoveHead();
         }

         const String headerString = GetLogFileHeaderString(a);
         if (headerString.HasChars()) fprintf(_logFile.GetFile(), "%s\n", headerString());
      }
      else
      {
         _activeLogFileName.Clear();
         _logFileOpenAttemptFailed = true;  // avoid an indefinite number of log-failed messages
         LogTime(MUSCLE_LOG_ERROR, "Failed to open Log file [%s], logging to file is now disabled. [%s]\n", logFileName(), B_ERRNO());
      }
   }
   return (_logFile.GetFile() != NULL) ? B_NO_ERROR : B_IO_ERROR;
}

void DefaultFileLogger :: CloseLogFile()
{
   if (_logFile.GetFile())
   {
      LogTime(MUSCLE_LOG_DEBUG, "Closing Log file [%s]\n", _activeLogFileName());
      String oldFileName = _activeLogFileName;  // default file to delete later, will be changed if/when we've made the .gz file
      _activeLogFileName.Clear();   // do this first to avoid reentrancy issues
      _logFile.Shutdown();

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
      if (_compressionEnabled)
      {
         FileDataIO inIO(muscleFopen(oldFileName(), "rb"));
         if (inIO.GetFile() != NULL)
         {
            const String gzName = oldFileName + ".gz";
            gzFile gzOut = gzopen(gzName(), "wb9"); // 9 for maximum compression
            if (gzOut != Z_NULL)
            {
               bool ok = true;

               const uint32 bufSize = 128*1024;
               char * buf = new char[bufSize];
               while(1)
               {
                  const int32 bytesRead = inIO.Read(buf, bufSize).GetByteCount();
                  if (bytesRead < 0) break;  // EOF

                  const int bytesWritten = gzwrite(gzOut, buf, bytesRead);
                  if (bytesWritten <= 0)
                  {
                     ok = false;  // write error, oh dear
                     break;
                  }
               }
               delete [] buf;

               gzclose(gzOut);

               if (ok)
               {
                  inIO.Shutdown();
                  if (remove(oldFileName()) != 0) LogTime(MUSCLE_LOG_ERROR, "Error deleting log file [%s] after compressing it to [%s] [%s]!\n", oldFileName(), gzName(), B_ERRNO());
                  oldFileName = std_move_if_available(gzName);
               }
               else
               {
                  if (remove(gzName()) != 0) LogTime(MUSCLE_LOG_ERROR, "Error deleting gzip'd log file [%s] after compression failed! [%s]\n", gzName(), B_ERRNO());
               }
            }
            else LogTime(MUSCLE_LOG_ERROR, "Could not open compressed Log file [%s]! [%s]\n", gzName(), B_ERRNO());
         }
         else LogTime(MUSCLE_LOG_DEBUG, "Could not reopen Log file [%s] to compress it! [%s]\n", oldFileName(), B_ERRNO());  // set to MUSCLE_LOG_DEBUG to avoid creating a log file in race conditions
      }
#endif
      if ((_maxNumLogFiles != MUSCLE_NO_LIMIT)&&(_oldLogFileNames.Contains(oldFileName) == false)) (void) _oldLogFileNames.AddTail(oldFileName);  // so we can delete it later
   }
}

LogLineCallback :: LogLineCallback()
   : _writeTo(_buf)
{
   _buf[0] = '\0';
   _buf[sizeof(_buf)-1] = '\0';  // just in case vsnsprintf() has to truncate
}

LogLineCallback :: ~LogLineCallback()
{
   // empty
}

void LogLineCallback :: LogAux(const LogCallbackArgs & a, va_list & argList)
{
   TCHECKPOINT;

   // Generate the new text
   const size_t sizeOfBuffer = (sizeof(_buf)-1)-(_writeTo-_buf);  // the -1 is for the guaranteed NUL terminator
#if __STDC_WANT_SECURE_LIB__
   const int bytesAttempted = _vsnprintf_s(_writeTo, sizeOfBuffer, _TRUNCATE, a.GetText(), argList);
#elif WIN32
   const int bytesAttempted =   _vsnprintf(_writeTo, sizeOfBuffer, a.GetText(),            argList);
#else
   const int bytesAttempted =    vsnprintf(_writeTo, sizeOfBuffer, a.GetText(),            argList);
#endif

   const bool wasTruncated = (bytesAttempted != (int)strlen(_writeTo));  // do not combine with above line!

   // Log any newly completed lines
   char * logFrom  = _buf;
   char * searchAt = _writeTo;
   while(true)
   {
      char * nextReturn = strchr(searchAt, '\n');
      if (nextReturn)
      {
         *nextReturn = '\0';  // terminate the string

         LogLine(LogCallbackArgs(a, logFrom));

         searchAt = logFrom = nextReturn+1;
      }
      else
      {
         // If we ran out of buffer space and no carriage returns were detected,
         // then we need to just dump what we have and move on, there's nothing else we can do
         if (wasTruncated)
         {
            LogLine(LogCallbackArgs(a, logFrom));

            _buf[0] = '\0';
            _writeTo = searchAt = logFrom = _buf;
         }
         break;
      }
   }

   // And finally, move any remaining incomplete lines back to the beginning of the array, for next time
   if (logFrom > _buf)
   {
      const int slen = (int) strlen(logFrom);
      memmove(_buf, logFrom, slen+1);  // include NUL byte
      _writeTo = &_buf[slen];          // point to our just-moved NUL byte
   }
   else _writeTo = strchr(searchAt, '\0');

   _lastLog = a;
}

void LogLineCallback :: Flush()
{
   TCHECKPOINT;

   if (_writeTo > _buf)
   {
      LogLine(LogCallbackArgs(_lastLog, _buf));
      _writeTo = _buf;
      _buf[0] = '\0';
   }
}

static Mutex _logMutex;
static Hashtable<LogCallbackRef, Void> _logCallbacks;
static DefaultConsoleLogger _dcl;
static DefaultFileLogger _dfl(MUSCLE_LOG_NONE);  // logging-to-file is disabled by default

status_t LockLog()
{
   return _logMutex.Lock();
}

status_t UnlockLog()
{
   return _logMutex.Unlock();
}

const char * GetLogLevelName(int ll)
{
   return ((ll>=0)&&(ll<(int) ARRAYITEMS(_logLevelNames))) ? _logLevelNames[ll] : "???";
}

const char * GetLogLevelKeyword(int ll)
{
   return ((ll>=0)&&(ll<(int) ARRAYITEMS(_logLevelKeywords))) ? _logLevelKeywords[ll] : "???";
}

int ParseLogLevelKeyword(const char * keyword)
{
   for (uint32 i=0; i<ARRAYITEMS(_logLevelKeywords); i++) if (strcmp(keyword, _logLevelKeywords[i]) == 0) return i;
   return -1;
}

int GetFileLogLevel()
{
   return _dfl.GetLogLevelThreshold();
}

String GetFileLogName()
{
   return _dfl.GetFileLogName();
}

uint32 GetFileLogMaximumSize()
{
   return _dfl.GetMaxLogFileSize();
}

uint32 GetMaxNumLogFiles()
{
   return _dfl.GetMaxNumLogFiles();
}

bool GetFileLogCompressionEnabled()
{
   return _dfl.GetFileCompressionEnabled();
}

int GetConsoleLogLevel()
{
   return _dcl.GetLogLevelThreshold();
}

void SetFileLogName(const String & logName)
{
   DECLARE_MUTEXGUARD(_logMutex);
   _dfl.SetLogFileName(logName);
   LogTime(MUSCLE_LOG_DEBUG, "File log name set to: %s\n", logName());
}

void SetOldLogFilesPattern(const String & pattern)
{
   DECLARE_MUTEXGUARD(_logMutex);

   const uint32 numAdded = _dfl.AddPreExistingLogFiles(pattern);
   LogTime(MUSCLE_LOG_DEBUG, "Old Log Files pattern set to: [%s] (" UINT32_FORMAT_SPEC " files matched)\n", pattern(), numAdded);
}

void SetFileLogMaximumSize(uint32 maxSizeBytes)
{
   DECLARE_MUTEXGUARD(_logMutex);

   _dfl.SetMaxLogFileSize(maxSizeBytes);
   if (maxSizeBytes == MUSCLE_NO_LIMIT) LogTime(MUSCLE_LOG_DEBUG, "File log maximum size set to: (unlimited).\n");
                                   else LogTime(MUSCLE_LOG_DEBUG, "File log maximum size set to: " UINT32_FORMAT_SPEC " bytes.\n", maxSizeBytes);
}

void SetMaxNumLogFiles(uint32 maxNumLogFiles)
{
   DECLARE_MUTEXGUARD(_logMutex);

   _dfl.SetMaxNumLogFiles(maxNumLogFiles);
   if (maxNumLogFiles == MUSCLE_NO_LIMIT) LogTime(MUSCLE_LOG_DEBUG, "Maximum number of log files set to: (unlimited).\n");
                                     else LogTime(MUSCLE_LOG_DEBUG, "Maximum number of log files to: " UINT32_FORMAT_SPEC "\n", maxNumLogFiles);
}

void SetFileLogCompressionEnabled(bool enable)
{
   DECLARE_MUTEXGUARD(_logMutex);

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   _dfl.SetFileCompressionEnabled(enable);
   LogTime(MUSCLE_LOG_DEBUG, "File log compression %s.\n", enable?"enabled":"disabled");
#else
   if (enable) LogTime(MUSCLE_LOG_CRITICALERROR, "Can not enable log file compression, MUSCLE was compiled without MUSCLE_ENABLE_ZLIB_ENCODING specified!\n");
#endif
}

void CloseCurrentLogFile()
{
   DECLARE_MUTEXGUARD(_logMutex);
   _dfl.CloseLogFile();
}

// Note that the LogLock must be locked when this method is called!
static void UpdateMaxLogLevel()
{
   int maxLogThreshold = muscleMax(_dfl.GetLogLevelThreshold(), _dcl.GetLogLevelThreshold());
   for (HashtableIterator<LogCallbackRef, Void> iter(_logCallbacks, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      const LogCallback * cb = iter.GetKey()();
      if (cb) maxLogThreshold = muscleMax(maxLogThreshold, cb->GetLogLevelThreshold());
   }

   muscle_private::_maxLogThreshold = maxLogThreshold;
}

void SetFileLogLevel(int logLevel)
{
   DECLARE_MUTEXGUARD(_logMutex);
   _dfl.SetLogLevelThreshold(logLevel);
   LogTime(MUSCLE_LOG_DEBUG, "File logging level set to: %s\n", GetLogLevelName(logLevel));
}

void SetConsoleLogLevel(int logLevel)
{
   DECLARE_MUTEXGUARD(_logMutex);
   _dcl.SetLogLevelThreshold(logLevel);
   LogTime(MUSCLE_LOG_DEBUG, "Console logging level set to: %s\n", GetLogLevelName(logLevel));
}

void SetConsoleLogToStderr(bool toStderr)
{
   DECLARE_MUTEXGUARD(_logMutex);

   _dcl.SetConsoleLogToStderr(toStderr);
   LogTime(MUSCLE_LOG_DEBUG, "Console logging target set to: %s\n", _dcl.GetConsoleLogToStderr()?"stderr":"stdout");
}

void LogCallback :: SetLogLevelThreshold(int logLevel)
{
   DECLARE_MUTEXGUARD(_logMutex);
   _logLevelThreshold = logLevel;
   UpdateMaxLogLevel();
}

void GetStandardLogLinePreamble(char * buf, const LogCallbackArgs & a)
{
   const size_t MINIMUM_PREAMBLE_BUF_SIZE_PER_DOCUMENTATION = 64;
   struct tm ltm;
   time_t when = a.GetWhen();
   struct tm * temp = muscle_localtime_r(&when, &ltm);
#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
# ifdef MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS
   const char * fn = a.GetSourceFile();
   const char * lastSlash = fn ? strrchr(fn, '/') : NULL;
#  ifdef WIN32
   const char * lastBackSlash = fn ? strrchr(fn, '\\') : NULL;
   if ((lastBackSlash)&&((lastSlash == NULL)||(lastBackSlash > lastSlash))) lastSlash = lastBackSlash;
#  endif
   if (lastSlash) fn = lastSlash+1;

   static const size_t suffixSize = 16;
   muscleSnprintf(buf, MINIMUM_PREAMBLE_BUF_SIZE_PER_DOCUMENTATION-suffixSize, "[%c %02i/%02i %02i:%02i:%02i] [%s", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec, fn);
   char buf2[suffixSize];
   muscleSnprintf(buf2, sizeof(buf2), ":%i] ", a.GetSourceLineNumber());
   strncat(buf, buf2, MINIMUM_PREAMBLE_BUF_SIZE_PER_DOCUMENTATION);
# else
   muscleSnprintf(buf, MINIMUM_PREAMBLE_BUF_SIZE_PER_DOCUMENTATION, "[%c %02i/%02i %02i:%02i:%02i] [%s] ", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec, SourceCodeLocationKeyToString(GenerateSourceCodeLocationKey(a.GetSourceFile(), a.GetSourceLineNumber()))());
#endif
#else
   muscleSnprintf(buf, MINIMUM_PREAMBLE_BUF_SIZE_PER_DOCUMENTATION, "[%c %02i/%02i %02i:%02i:%02i] ", GetLogLevelName(a.GetLogLevel())[0], temp->tm_mon+1, temp->tm_mday, temp->tm_hour, temp->tm_min, temp->tm_sec);
#endif
}

static NestCount _inWarnOutOfMemory;  // avoid potential infinite recursion if we are logging out-of-memory errors but our LogCallbacks try to allocate memory to do the log operation :^P

#define DO_LOGGING_PREAMBLE(cb)          \
{                                        \
   NestCountGuard g(_inLogPreamble);     \
   va_list argList;                      \
   va_start(argList, fmt);               \
   const LogCallbackArgs lca(when, ll, sourceFile, sourceFunction, sourceLine, buf); \
   GetStandardLogLinePreamble(buf, lca); \
   (cb).Log(lca, argList);               \
   va_end(argList);                      \
}

#define DO_LOGGING_CALLBACKS(ll)                                                             \
   for (HashtableIterator<LogCallbackRef, Void> iter(_logCallbacks); iter.HasData(); iter++) \
   {                                                                                         \
      LogCallback * cb = iter.GetKey()();                                                    \
      if ((cb)&&((ll) <= cb->GetLogLevelThreshold())) DO_LOGGING_CALLBACK((*cb));            \
   }

#define DO_LOGGING_CALLBACK(cb) \
{                               \
   va_list argList;             \
   va_start(argList, fmt);      \
   (cb).Log(LogCallbackArgs(when, ll, sourceFile, sourceFunction, sourceLine, fmt), argList); \
   va_end(argList);             \
}

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
status_t LogTimeAux(const char * sourceFile, const char * sourceFunction, int sourceLine, int ll, const char * fmt, ...)
#else
status_t LogTimeAux(int ll, const char * fmt, ...)
#endif
{
   DECLARE_MUTEXGUARD(_logMutex);
   if (_inWarnOutOfMemory.GetCount() < 2)  // avoid potential infinite recursion (while still allowing the first Out-of-memory message to attempt to get into the log)
   {
      // First, log the preamble
      const time_t when = time(NULL);
      char buf[128];

#ifndef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
      static const char * sourceFile     = "";
      static const char * sourceFunction = "";
      static const int sourceLine        = -1;
#endif

      // First, send to the log file
      if (ll <= _dfl.GetLogLevelThreshold())
      {
         DO_LOGGING_PREAMBLE(_dfl);
         DO_LOGGING_CALLBACK(_dfl);
      }

      // Then, send to the display
      if (ll <= _dcl.GetLogLevelThreshold())
      {
         DO_LOGGING_PREAMBLE(_dcl);
         DO_LOGGING_CALLBACK(_dcl);  // must be outside of the braces!
      }

      // Then log the actual message as supplied by the user
      DO_LOGGING_CALLBACKS(ll);
   }
   return B_NO_ERROR;
}

void LogFlush()
{
   DECLARE_MUTEXGUARD(_logMutex);
   for (HashtableIterator<LogCallbackRef, Void> iter(_logCallbacks); iter.HasData(); iter++)
   {
      const LogCallbackRef & lcr = iter.GetKey();
      if (lcr()) lcr()->Flush();
   }
}

status_t LogPlainAux(int ll, const char * fmt, ...)
{
   DECLARE_MUTEXGUARD(_logMutex);
   {
      // No way to get these, since #define Log() as a macro causes
      // nasty namespace collisions with other methods/functions named Log()
      static const char * sourceFile     = "";
      static const char * sourceFunction = "";
      static const int sourceLine        = -1;

      const time_t when = time(NULL);
      if (ll <= _dfl.GetLogLevelThreshold()) DO_LOGGING_CALLBACK(_dfl);
      if (ll <= _dcl.GetLogLevelThreshold()) DO_LOGGING_CALLBACK(_dcl);
      DO_LOGGING_CALLBACKS(ll);
   }
   return B_NO_ERROR;
}

status_t PutLogCallback(const LogCallbackRef & cb)
{
   DECLARE_MUTEXGUARD(_logMutex);
   const status_t ret = _logCallbacks.PutWithDefault(cb);
   UpdateMaxLogLevel();
   return ret;
}

void ClearLogCallbacks()
{
   DECLARE_MUTEXGUARD(_logMutex);
   _logCallbacks.Clear();
   UpdateMaxLogLevel();
}

status_t RemoveLogCallback(const LogCallbackRef & cb)
{
   DECLARE_MUTEXGUARD(_logMutex);
   const status_t ret = _logCallbacks.Remove(cb);
   UpdateMaxLogLevel();
   return ret;
}

extern uint32 GetAndClearFailedMemoryRequestSize();

void WarnOutOfMemory(const char * file, int line)
{
   // Yes, this technique is open to race conditions and other lossage.
   // But it will work in the one-error-only case, which is good enough
   // for now.
   NestCountGuard ncg(_inWarnOutOfMemory);  // avoid potential infinite recursion if LogCallbacks called by LogTime() try to allocate more memory and also fail
   LogTime(MUSCLE_LOG_CRITICALERROR, "ERROR--MEMORY ALLOCATION FAILURE!  (" INT32_FORMAT_SPEC " bytes at %s:%i)\n", GetAndClearFailedMemoryRequestSize(), file, line);

   if (_inWarnOutOfMemory.IsOutermost())
   {
      static uint64 _prevCallTime = 0;
      if (OnceEvery(SecondsToMicros(5), _prevCallTime)) (void) PrintStackTrace();
   }
}

#endif  // !MUSCLE_INLINE_LOGGING

// Our 26-character alphabet of usable symbols
#define NUM_CHARS_IN_KEY_ALPHABET (sizeof(_keyAlphabet)-1)  // -1 because the NUL terminator doesn't count
static const char _keyAlphabet[] = "2346789BCDFGHJKMNPRSTVWXYZ";  // FogBugz #5808: vowels and some numerals omitted to avoid ambiguity and inadvertent swearing
static const uint32 _keySpaceSize = NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET * NUM_CHARS_IN_KEY_ALPHABET;

uint32 GenerateSourceCodeLocationKey(const char * fileName, uint32 lineNumber)
{
#ifdef WIN32
   const char * lastSlash = strrchr(fileName, '\\');
#else
   const char * lastSlash = strrchr(fileName, '/');
#endif
   if (lastSlash) fileName = lastSlash+1;

   return ((CalculateHashCode(fileName,(uint32)strlen(fileName))+lineNumber)%(_keySpaceSize-1))+1;  // note that 0 is not considered a valid key value!
}

String SourceCodeLocationKeyToString(uint32 key)
{
   if (key == 0) return "";                  // 0 is not a valid key value
   if (key >= _keySpaceSize) return "????";  // values greater than or equal to our key space size are errors

   char buf[5]; buf[4] = '\0';
   for (int32 i=3; i>=0; i--)
   {
      buf[i] = _keyAlphabet[key % NUM_CHARS_IN_KEY_ALPHABET];
      key /= NUM_CHARS_IN_KEY_ALPHABET;
   }
   return buf;
}

uint32 SourceCodeLocationKeyFromString(const String & ss)
{
   String s = ss.ToUpperCase().Trimmed();
   if (s.Length() != 4) return 0;  // codes must always be exactly 4 characters long!

   s.Replace('0', 'O');
   s.Replace('1', 'I');
   s.Replace('5', 'S');

   uint32 ret  = 0;
   uint32 base = 1;
   for (int32 i=3; i>=0; i--)
   {
      const char * p = strchr(_keyAlphabet, s[i]);
      if (p == NULL) return 0;  // invalid character!

      const int whichChar = (int) (p-_keyAlphabet);
      ret += (whichChar*base);
      base *= NUM_CHARS_IN_KEY_ALPHABET;
   }
   return ret;
}


#ifdef WIN32
static const uint64 _windowsDiffTime = ((uint64)116444736)*NANOS_PER_SECOND; // add (1970-1601) to convert to Windows time base
#endif

status_t GetHumanReadableTimeValues(uint64 timeUS, HumanReadableTimeValues & v, uint32 timeType)
{
   TCHECKPOINT;

   if (timeUS == MUSCLE_TIME_NEVER) return B_BAD_ARGUMENT;

   const int microsLeft = (int)(timeUS % MICROS_PER_SECOND);

#ifdef WIN32
   // Borland's localtime() function is buggy, so we'll use the Win32 API instead.
   const uint64 winTime = (timeUS*10) + _windowsDiffTime;  // Convert to (100ns units)

   FILETIME fileTime;
   fileTime.dwHighDateTime = (DWORD) ((winTime>>32) & 0xFFFFFFFF);
   fileTime.dwLowDateTime  = (DWORD) ((winTime>> 0) & 0xFFFFFFFF);

   SYSTEMTIME st;
   if (FileTimeToSystemTime(&fileTime, &st))
   {
      if (timeType == MUSCLE_TIMEZONE_UTC)
      {
         TIME_ZONE_INFORMATION tzi;
         if ((GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)||(SystemTimeToTzSpecificLocalTime(&tzi, &st, &st) == false)) return B_ERRNO;
      }
      v = HumanReadableTimeValues(st.wYear, st.wMonth-1, st.wDay-1, st.wDayOfWeek, st.wHour, st.wMinute, st.wSecond, microsLeft);
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#else
   struct tm ltm, gtm;
   time_t timeS = (time_t) MicrosToSeconds(timeUS);  // timeS is seconds since 1970
   struct tm * ts = (timeType == MUSCLE_TIMEZONE_UTC) ? muscle_localtime_r(&timeS, &ltm) : muscle_gmtime_r(&timeS, &gtm);  // only convert if it isn't already local
   if (ts)
   {
      v = HumanReadableTimeValues(ts->tm_year+1900, ts->tm_mon, ts->tm_mday-1, ts->tm_wday, ts->tm_hour, ts->tm_min, ts->tm_sec, microsLeft);
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#endif
}

#ifdef WIN32
static bool MUSCLE_TzSpecificLocalTimeToSystemTime(LPTIME_ZONE_INFORMATION tzi, LPSYSTEMTIME st)
{
# if defined(__BORLANDC__) || defined(MUSCLE_USING_OLD_MICROSOFT_COMPILER) || defined(__MINGW32__)
#  if defined(_MSC_VER)
   typedef BOOL (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  elif defined(__MINGW32__) || defined(__MINGW64__)
   typedef BOOL WINAPI (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  else
   typedef WINBASEAPI BOOL WINAPI (*TzSpecificLocalTimeToSystemTimeProc) (IN LPTIME_ZONE_INFORMATION lpTimeZoneInformation, IN LPSYSTEMTIME lpLocalTime, OUT LPSYSTEMTIME lpUniversalTime);
#  endif

   // Some compilers' headers don't have this call, so we have to do it the hard way
   HMODULE lib = LoadLibrary(TEXT("kernel32.dll"));
   if (lib == NULL) return false;

   TzSpecificLocalTimeToSystemTimeProc tzProc = (TzSpecificLocalTimeToSystemTimeProc) GetProcAddress(lib, "TzSpecificLocalTimeToSystemTime");
   const bool ret = ((tzProc)&&(tzProc(tzi, st, st)));
   FreeLibrary(lib);
   return ret;
# else
   return (TzSpecificLocalTimeToSystemTime(tzi, st, st) != 0);
# endif
}
#endif

status_t GetTimeStampFromHumanReadableTimeValues(const HumanReadableTimeValues & v, uint64 & retTimeStamp, uint32 timeType)
{
   TCHECKPOINT;

#ifdef WIN32
   SYSTEMTIME st; memset(&st, 0, sizeof(st));
   st.wYear         = v.GetYear();
   st.wMonth        = v.GetMonth()+1;
   st.wDayOfWeek    = v.GetDayOfWeek();
   st.wDay          = v.GetDayOfMonth()+1;
   st.wHour         = v.GetHour();
   st.wMinute       = v.GetMinute();
   st.wSecond       = v.GetSecond();
   st.wMilliseconds = v.GetMicrosecond()/1000;

   if (timeType == MUSCLE_TIMEZONE_UTC)
   {
      TIME_ZONE_INFORMATION tzi;
      if ((GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)||(MUSCLE_TzSpecificLocalTimeToSystemTime(&tzi, &st) == false)) return B_ERRNO;
   }

   FILETIME fileTime;
   if (SystemTimeToFileTime(&st, &fileTime))
   {
      retTimeStamp = (((((uint64)fileTime.dwHighDateTime)<<32)|((uint64)fileTime.dwLowDateTime))-_windowsDiffTime)/10;
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#else
   struct tm ltm; memset(&ltm, 0, sizeof(ltm));
   ltm.tm_sec  = v.GetSecond();       /* seconds after the minute [0-60] */
   ltm.tm_min  = v.GetMinute();       /* minutes after the hour [0-59] */
   ltm.tm_hour = v.GetHour();         /* hours since midnight [0-23] */
   ltm.tm_mday = v.GetDayOfMonth()+1; /* day of the month [1-31] */
   ltm.tm_mon  = v.GetMonth();        /* months since January [0-11] */
   ltm.tm_year = v.GetYear()-1900;    /* years since 1900 */
   ltm.tm_wday = v.GetDayOfWeek();    /* days since Sunday [0-6] */
   ltm.tm_isdst = -1;  /* Let mktime() decide whether summer time is in effect */

   const time_t tm = ((uint64)((timeType == MUSCLE_TIMEZONE_UTC) ? mktime(&ltm) : timegm(&ltm)));
   if (tm == -1) return B_ERRNO;

   retTimeStamp = SecondsToMicros(tm);
   return B_NO_ERROR;
#endif
}


String HumanReadableTimeValues :: ToString() const
{
   return ExpandTokens("%T");  // Yes, this must be here in the .cpp file!
}

String HumanReadableTimeValues :: ExpandTokens(const String & origString) const
{
   if (origString.IndexOf('%') < 0) return origString;

   String newString = origString;
   (void) newString.Replace("%%", "%");  // do this first!
   (void) newString.Replace("%T", "%Q %D %Y %h:%m:%s");
   (void) newString.Replace("%t", "%Y/%M/%D %h:%m:%s");
   (void) newString.Replace("%f", "%Y-%M-%D_%hh%mm%s");

   static const char * _daysOfWeek[]   = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
   static const char * _monthsOfYear[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

   (void) newString.Replace("%Y", String("%1").Arg(GetYear()));
   (void) newString.Replace("%M", String("%1").Arg(GetMonth()+1,      "%02i"));
   (void) newString.Replace("%Q", String("%1").Arg(_monthsOfYear[muscleClamp(GetMonth(), 0, (int)(ARRAYITEMS(_monthsOfYear)-1))]));
   (void) newString.Replace("%D", String("%1").Arg(GetDayOfMonth()+1, "%02i"));
   (void) newString.Replace("%d", String("%1").Arg(GetDayOfMonth()+1, "%02i"));
   (void) newString.Replace("%W", String("%1").Arg(GetDayOfWeek()+1,  "%02i"));
   (void) newString.Replace("%w", String("%1").Arg(GetDayOfWeek()+1,  "%02i"));
   (void) newString.Replace("%q", String("%1").Arg(_daysOfWeek[muscleClamp(GetDayOfWeek(), 0, (int)(ARRAYITEMS(_daysOfWeek)-1))]));
   (void) newString.Replace("%h", String("%1").Arg(GetHour(),           "%02i"));
   (void) newString.Replace("%m", String("%1").Arg(GetMinute(),         "%02i"));
   (void) newString.Replace("%s", String("%1").Arg(GetSecond(),         "%02i"));
   (void) newString.Replace("%x", String("%1").Arg(GetMicrosecond(),    "%06i"));

   if (newString.Contains("%r"))
   {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      std::random_device device;
      std::mt19937_64 generator(device());
      std::uniform_int_distribution<uint64> distribution;
#endif

      while(newString.Contains("%r"))
      {
#ifdef MUSCLE_AVOID_CPLUSPLUS11
         const uint32 r1 = GetInsecurePseudoRandomNumber(); // the old, not-very-good way to generate a sort-of-random number
         const uint32 r2 = GetInsecurePseudoRandomNumber(); // we rely on the calling code to call srand() beforehand, if desired
         const uint64 rn = (((uint64)r1)<<32)|((uint64)r2);
#else
         const uint64 rn = distribution(generator);          // the newer (since C++11) and much improved way
#endif

         char buf[64]; muscleSprintf(buf, UINT64_FORMAT_SPEC, rn);
         if (newString.Replace("%r", buf, 1) <= 0) break;
      }
   }

   if (newString.Contains("%p"))
   {
#ifdef WIN32
      const uint32 processID = (uint32) GetCurrentProcessId();
#else
      const uint32 processID = (uint32) ((unsigned long)getpid());
#endif
      (void) newString.Replace("%p", String("%1").Arg(processID));
   }

   return newString;
}

// I separated these Strings out into constants mainly just to avoid a spurious compiler warning --jaf
static const String _foreverStr("forever");
static const String _neverStr("never");
static const String _infStr("inf");

String GetHumanReadableTimeString(uint64 timeUS, uint32 timeType)
{
   TCHECKPOINT;

   if (timeUS == MUSCLE_TIME_NEVER) return ("(never)");
   else
   {
      HumanReadableTimeValues v;
      if (GetHumanReadableTimeValues(timeUS, v, timeType).IsOK())
      {
         char buf[256];
         muscleSprintf(buf, "%02i/%02i/%02i %02i:%02i:%02i", v.GetYear(), v.GetMonth()+1, v.GetDayOfMonth()+1, v.GetHour(), v.GetMinute(), v.GetSecond());
         return String(buf);
      }
      return "";
   }
}

#ifdef WIN32
extern uint64 __Win32FileTimeToMuscleTime(const FILETIME & ft);  // from SetupSystem.cpp
#endif

uint64 ParseHumanReadableTimeString(const String & s, uint32 timeType)
{
   TCHECKPOINT;

   if (s.IndexOfIgnoreCase(_neverStr) >= 0) return MUSCLE_TIME_NEVER;

   StringTokenizer tok(s(), "//::" STRING_TOKENIZER_DEFAULT_SOFT_SEPARATOR_CHARS);
   const char * year   = tok();
   const char * month  = tok();
   const char * day    = tok();
   const char * hour   = tok();
   const char * minute = tok();
   const char * second = tok();

#if defined(WIN32) && defined(WINXP)
   SYSTEMTIME st; memset(&st, 0, sizeof(st));
   st.wYear      = (WORD) (year   ? atoi(year)   : 0);
   st.wMonth     = (WORD) (month  ? atoi(month)  : 0);
   st.wDay       = (WORD) (day    ? atoi(day)    : 0);
   st.wHour      = (WORD) (hour   ? atoi(hour)   : 0);
   st.wMinute    = (WORD) (minute ? atoi(minute) : 0);
   st.wSecond    = (WORD) (second ? atoi(second) : 0);

   if (timeType == MUSCLE_TIMEZONE_UTC)
   {
      TIME_ZONE_INFORMATION tzi;
      if (GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) (void) MUSCLE_TzSpecificLocalTimeToSystemTime(&tzi, &st);
   }

   FILETIME fileTime;
   return (SystemTimeToFileTime(&st, &fileTime)) ? __Win32FileTimeToMuscleTime(fileTime) : 0;
#else
   struct tm st; memset(&st, 0, sizeof(st));
   st.tm_sec  = second ? atoi(second)    : 0;
   st.tm_min  = minute ? atoi(minute)    : 0;
   st.tm_hour = hour   ? atoi(hour)      : 0;
   st.tm_mday = day    ? atoi(day)       : 0;
   st.tm_mon  = month  ? atoi(month)-1   : 0;
   st.tm_year = year   ? atoi(year)-1900 : 0;
   time_t timeS = mktime(&st);
   if (timeType == MUSCLE_TIMEZONE_LOCAL)
   {
      struct tm ltm;
      struct tm * t = muscle_gmtime_r(&timeS, &ltm);
      if (t) timeS += (timeS-mktime(t));
   }
   return SecondsToMicros(timeS);
#endif
}

enum {
   TIME_UNIT_MICROSECOND,
   TIME_UNIT_MILLISECOND,
   TIME_UNIT_SECOND,
   TIME_UNIT_MINUTE,
   TIME_UNIT_HOUR,
   TIME_UNIT_DAY,
   TIME_UNIT_WEEK,
   TIME_UNIT_MONTH,
   TIME_UNIT_YEAR,
   NUM_TIME_UNITS
};

static const uint64 MICROS_PER_DAY = DaysToMicros(1);

static const uint64 _timeUnits[] = {
   1,                       // micros -> micros
   1000,                    // millis -> micros
   MICROS_PER_SECOND,       // secs   -> micros
   60*MICROS_PER_SECOND,    // mins   -> micros
   60*60*MICROS_PER_SECOND, // hours  -> micros
   MICROS_PER_DAY,          // days   -> micros
   7*MICROS_PER_DAY,        // weeks  -> micros
   30*MICROS_PER_DAY,       // months -> micros (well, sort of -- we assume a month is always 30  days, which isn't really true)
   365*MICROS_PER_DAY       // years  -> micros (well, sort of -- we assume a years is always 365 days, which isn't really true)
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_timeUnits, NUM_TIME_UNITS);

static const char * _timeUnitNames[] = {
   "microsecond",
   "millisecond",
   "second",
   "minute",
   "hour",
   "day",
   "week",
   "month",
   "year",
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_timeUnitNames, NUM_TIME_UNITS);

static bool IsFloatingPointNumber(const char * d)
{
   while(1)
   {
           if (*d == '.')          return true;
      else if (!muscleIsDigit(*d)) return false;
      else                         d++;
   }
}

static uint64 GetTimeUnitMultiplier(const String & l, uint64 defaultValue)
{
   uint64 multiplier = defaultValue;
   String tmp(l); tmp = tmp.ToLowerCase();
        if ((tmp.StartsWith("us"))||(tmp.StartsWith("micro"))) multiplier = _timeUnits[TIME_UNIT_MICROSECOND];
   else if ((tmp.StartsWith("ms"))||(tmp.StartsWith("milli"))) multiplier = _timeUnits[TIME_UNIT_MILLISECOND];
   else if (tmp.StartsWith("mo"))                              multiplier = _timeUnits[TIME_UNIT_MONTH];
   else if (tmp.StartsWith("s"))                               multiplier = _timeUnits[TIME_UNIT_SECOND];
   else if (tmp.StartsWith("m"))                               multiplier = _timeUnits[TIME_UNIT_MINUTE];
   else if (tmp.StartsWith("h"))                               multiplier = _timeUnits[TIME_UNIT_HOUR];
   else if (tmp.StartsWith("d"))                               multiplier = _timeUnits[TIME_UNIT_DAY];
   else if (tmp.StartsWith("w"))                               multiplier = _timeUnits[TIME_UNIT_WEEK];
   else if (tmp.StartsWith("y"))                               multiplier = _timeUnits[TIME_UNIT_YEAR];
   return multiplier;
}

uint64 ParseHumanReadableUnsignedTimeIntervalString(const String & s)
{
   if ((s.EqualsIgnoreCase(_foreverStr))||(s.EqualsIgnoreCase(_neverStr))||(s.StartsWithIgnoreCase(_infStr))) return MUSCLE_TIME_NEVER;

   /** Find the first digit in the string */
   const char * digits = s();
   while((*digits)&&(muscleIsDigit(*digits) == false)) digits++;
   if (*digits == '\0') return GetTimeUnitMultiplier(s, 0);  // in case the string is just "second" or "hour" or etc.

   /** Find first letter after the digits */
   const char * letters = digits;
   while((*letters)&&(muscleIsAlpha(*letters) == false)) letters++;
   if (*letters == '\0') letters = "s";  // default to seconds

   const uint64 multiplier = GetTimeUnitMultiplier(letters, _timeUnits[TIME_UNIT_SECOND]);   // default units is seconds

   const char * afterLetters = letters;
   while((*afterLetters)&&((*afterLetters==',')||(muscleIsAlpha(*afterLetters)||(muscleIsSpace(*afterLetters))))) afterLetters++;

   uint64 ret = IsFloatingPointNumber(digits) ? (uint64)(atof(digits)*multiplier) : (Atoull(digits)*multiplier);
   if (*afterLetters) ret += ParseHumanReadableUnsignedTimeIntervalString(afterLetters);
   return ret;
}

static const int64 _largestSigned64BitValue = 0x7FFFFFFFFFFFFFFFLL;  // closest we can get to MUSCLE_TIME_NEVER

int64 ParseHumanReadableSignedTimeIntervalString(const String & s)
{
   const bool isNegative = s.StartsWith('-');
   const uint64 unsignedVal = ParseHumanReadableUnsignedTimeIntervalString(isNegative ? s.Substring(1) : s);
   return (unsignedVal == MUSCLE_TIME_NEVER) ? _largestSigned64BitValue : (isNegative ? -((int64)unsignedVal) : ((int64)unsignedVal));
}

String GetHumanReadableUnsignedTimeIntervalString(uint64 intervalUS, uint32 maxClauses, uint64 minPrecision, bool * optRetIsAccurate, bool roundUp)
{
   if (intervalUS == MUSCLE_TIME_NEVER) return _foreverStr;

   // Find the largest unit that is still smaller than (micros)
   uint32 whichUnit = TIME_UNIT_MICROSECOND;
   for (uint32 i=0; i<NUM_TIME_UNITS; i++)
   {
      if (_timeUnits[whichUnit] < intervalUS) whichUnit++;
                                         else break;
   }
   if ((whichUnit >= NUM_TIME_UNITS)||((whichUnit > 0)&&(_timeUnits[whichUnit] > intervalUS))) whichUnit--;

   const uint64 unitSizeUS       = _timeUnits[whichUnit];
   const uint64 leftover         = intervalUS%unitSizeUS;
   const bool willAddMoreClauses = ((leftover>minPrecision)&&(maxClauses>1));
   const uint64 numUnits         = (intervalUS/unitSizeUS)+(((roundUp)&&(willAddMoreClauses==false)&&(leftover>=(unitSizeUS/2)))?1:0);
   char buf[256]; muscleSprintf(buf, UINT64_FORMAT_SPEC " %s%s", numUnits, _timeUnitNames[whichUnit], (numUnits==1)?"":"s");
   String ret = buf;

   if (leftover > 0)
   {
      if (willAddMoreClauses) ret += GetHumanReadableUnsignedTimeIntervalString(leftover, maxClauses-1, minPrecision, optRetIsAccurate).WithPrepend(", ");
                         else if (optRetIsAccurate) *optRetIsAccurate = false;
   }
   else if (optRetIsAccurate) *optRetIsAccurate = true;

   return ret;
}

String GetHumanReadableSignedTimeIntervalString(int64 intervalUS, uint32 maxClauses, uint64 minPrecision, bool * optRetIsAccurate, bool roundUp)
{
   if (intervalUS == _largestSigned64BitValue) return _foreverStr;  // since we can't use MUSCLE_TIME_NEVER with a signed value, as it comes out as -1

   String ret;
   if (intervalUS < 0) ret += '-';
   return ret+GetHumanReadableUnsignedTimeIntervalString(muscleAbs(intervalUS), maxClauses, minPrecision, optRetIsAccurate, roundUp);
}

} // end namespace muscle

