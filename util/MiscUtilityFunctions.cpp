/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <fcntl.h>

#ifdef __APPLE__
# include <sys/signal.h>
# include <sys/types.h>
#else
# include <signal.h>
#endif

#ifdef __linux__
# include <sched.h>
#endif

#ifndef WIN32
# include <sys/stat.h>  // for umask()
#endif

#include "reflector/StorageReflectConstants.h"  // for PR_COMMAND_BATCH, PR_NAME_KEYS
#include "util/ByteBuffer.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/StringTokenizer.h"
#include "util/Directory.h"
#include "util/FilePathInfo.h"

namespace muscle {

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   extern bool _enableDeadlockFinderPrints;
#endif

extern bool _mainReflectServerCatchSignals;  // from SetupSystem.cpp

static status_t ParseArgAux(const String & a, Message * optAddToMsg, Queue<String> * optAddToQueue, bool cs)
{
   // Remove any initial dashes
   String argName = a.Trim();
   const char * s = argName();
   if (s > argName()) argName = argName.Substring(s-argName);

   if (optAddToQueue) return optAddToQueue->AddTail(argName);
   else
   {
      const int equalsAt = argName.IndexOf('=');
      String argValue;
      if (equalsAt >= 0)
      {
         argValue = argName.Substring(equalsAt+1).Trim();  // this must be first!
         argName  = argName.Substring(0, equalsAt).Trim();
         if (cs == false) argName = argName.ToLowerCase();
      }
      if (argName.HasChars())
      { 
         // Don't allow the parsing to fail just because the user specified a section name the same as a param name!
         uint32 tc;
         static const String _escapedQuote = "\\\"";
         static const String _quote = "\"";
         argName.Replace(_escapedQuote, _quote);
         argValue.Replace(_escapedQuote, _quote);
         if ((optAddToMsg->GetInfo(argName, &tc).IsOK())&&(tc != B_STRING_TYPE)) (void) optAddToMsg->RemoveName(argName);
         return optAddToMsg->AddString(argName, argValue);
      }
      else return B_NO_ERROR;
   }
}
status_t ParseArg(const String & a, Message & addTo, bool cs)       {return ParseArgAux(a, &addTo, NULL, cs);}
status_t ParseArg(const String & a, Queue<String> & addTo, bool cs) {return ParseArgAux(a, NULL, &addTo, cs);}

static String QuoteAndEscapeStringIfNecessary(const String & s)
{
   String tmp = s;
   tmp.Replace("\"", "\\\"");
   return ((tmp.IndexOf(' ') >= 0)||(tmp.IndexOf('\t') >= 0)||(tmp.IndexOf('\r') >= 0)||(tmp.IndexOf('\n') >= 0)) ? tmp.Prepend('\"').Append('\"') : tmp;
}

String UnparseArgs(const Message & argsMsg)
{
   String ret, next, tmp;
   for (MessageFieldNameIterator it(argsMsg, B_STRING_TYPE); it.HasData(); it++)
   {
      const String & fn = it.GetFieldName();
      const String * ps;
      for (int32 i=0; argsMsg.FindString(fn, i, &ps).IsOK(); i++)
      {
         tmp = QuoteAndEscapeStringIfNecessary(fn);
         if (ps->HasChars())
         {
            next  = tmp;
            next += '=';
            next += QuoteAndEscapeStringIfNecessary(*ps);
         }
         else next = tmp;

         if (next.HasChars())
         {
            if (ret.HasChars()) ret += ' ';
            ret += next;
         }
         next.Clear();
      }
   }
   return ret;
}

String UnparseArgs(const Queue<String> & args, uint32 startIdx, uint32 endIdx)
{
   String ret;
   endIdx = muscleMin(endIdx, args.GetNumItems());
   for (uint32 i=startIdx; i<endIdx; i++)
   {
      String subRet = args[i];
      subRet.Replace("\"", "\\\"");
      if (subRet.IndexOf(' ') >= 0) subRet = subRet.Append("\"").Prepend("\"");
      if (ret.HasChars()) ret += ' ';
      ret += subRet;
   }
   return ret;
}

static status_t ParseArgsAux(const String & line, Message * optAddToMsg, Queue<String> * optAddToQueue, bool cs)
{
   TCHECKPOINT;

   const String trimmed = line.Trim();
   const uint32 len = trimmed.Length();

   // First, we'll pre-process the string into a StringTokenizer-friendly
   // form, by replacing all quoted spaces with gunk and removing the quotes
   String tokenizeThis; 
   MRETURN_ON_ERROR(tokenizeThis.Prealloc(len));

   const char GUNK_CHAR      = (char) 0x01;
   bool lastCharWasBackslash = false;
   bool inQuotes = false;
   for (uint32 i=0; i<len; i++)
   {
      char c = trimmed[i];
      if ((lastCharWasBackslash == false)&&(c == '\"')) inQuotes = !inQuotes;
      else 
      {
         if ((inQuotes == false)&&(c == '#')) break;  // comment to EOL
         tokenizeThis += ((inQuotes)&&(c == ' ')) ? GUNK_CHAR : c;
      }
      lastCharWasBackslash = (c == '\\');
   }
   StringTokenizer tok(tokenizeThis(), NULL);   // soft/whitespace separators only
   const char * t = tok();
   while(t)
   {
      String n(t);
      n.Replace(GUNK_CHAR, ' ');

      // Check to see if the next token is the equals sign...
      const char * next = tok();
      if ((next)&&(next[0] == '='))
      {
         if (next[1] != '\0')
         {
            // It's the "x =5" case (2 tokens)
            String n2(next);
            n2.Replace(GUNK_CHAR, ' ');
            MRETURN_ON_ERROR(ParseArgAux(n+n2, optAddToMsg, optAddToQueue, cs));
            t = tok();
         }
         else
         {
            // It's the "x = 5" case (3 tokens)
            next = tok();  // find out what's after the equal sign
            if (next)
            {
               String n3(next);
               n3.Replace(GUNK_CHAR, ' ');
               MRETURN_ON_ERROR(ParseArgAux(n+"="+n3, optAddToMsg, optAddToQueue, cs));
               t = tok();
            }
            else 
            {
               MRETURN_ON_ERROR(ParseArgAux(n, optAddToMsg, optAddToQueue, cs));  // for the "x =" case, just parse x and ignore the equals
               t = NULL;
            }
         }
      }
      else if (n.EndsWith('='))
      {
         // Try to attach the next keyword
         String n4(next);
         n4.Replace(GUNK_CHAR, ' ');
         MRETURN_ON_ERROR(ParseArgAux(n+n4, optAddToMsg, optAddToQueue, cs));
         t = tok();
      }
      else
      {
         // Nope, it's just the normal case
         MRETURN_ON_ERROR(ParseArgAux(n, optAddToMsg, optAddToQueue, cs));
         t = next;
      }
   }
   return B_NO_ERROR;
}
status_t ParseArgs(const String & line, Message & addTo, bool cs)       {return ParseArgsAux(line, &addTo, NULL, cs);}
status_t ParseArgs(const String & line, Queue<String> & addTo, bool cs) {return ParseArgsAux(line, NULL, &addTo, cs);}

status_t ParseArgs(int argc, char ** argv, Message & addTo, bool cs)
{
   for (int i=0; i<argc; i++) MRETURN_ON_ERROR(ParseArg(argv[i], addTo, cs));
   return B_NO_ERROR;
}

status_t ParseArgs(const Queue<String> & args, Message & addTo, bool cs)
{
   for (uint32 i=0; i<args.GetNumItems(); i++) MRETURN_ON_ERROR(ParseArg(args[i], addTo, cs));
   return B_NO_ERROR;
}

status_t ParseArgs(int argc, char ** argv, Queue<String> & addTo, bool cs)
{
   for (int i=0; i<argc; i++) MRETURN_ON_ERROR(ParseArg(argv[i], addTo, cs));
   return B_NO_ERROR;
}

static status_t ParseFileAux(StringTokenizer * optTok, FILE * fpIn, Message * optAddToMsg, Queue<String> * optAddToQueue, char * scratchBuf, uint32 bufSize, bool cs)
{
   while(1)
   {
      const char * lineOfText = optTok ? optTok->GetNextToken() : (fpIn ? fgets(scratchBuf, bufSize, fpIn) : NULL);
      if (lineOfText == NULL) break;

      String checkForSection(lineOfText);
      checkForSection = optAddToMsg ? checkForSection.Trim() : "";  // sections are only supported for Messages, not Queue<String>'s
      if (cs == false) checkForSection = checkForSection.ToLowerCase();
      if (((checkForSection == "begin")||(checkForSection.StartsWith("begin ")))&&(optAddToMsg))  // the check for (optAddToMsg) isn't really necessary, but it makes clang++ happy
      {
         checkForSection = checkForSection.Substring(6).Trim();
         const int32 hashIdx = checkForSection.IndexOf('#');
         if (hashIdx >= 0) checkForSection = checkForSection.Substring(0, hashIdx).Trim();
         
         // Don't allow the parsing to fail just because the user specified a section name the same as a param name!
         uint32 tc;
         if ((optAddToMsg->GetInfo(checkForSection, &tc).IsOK())&&(tc != B_MESSAGE_TYPE)) (void) optAddToMsg->RemoveName(checkForSection);

         MessageRef subMsg = GetMessageFromPool();
         MRETURN_OOM_ON_NULL(subMsg());
         MRETURN_ON_ERROR(optAddToMsg->AddMessage(checkForSection, subMsg));
         MRETURN_ON_ERROR(ParseFileAux(optTok, fpIn, subMsg(), optAddToQueue, scratchBuf, bufSize, cs));
      }
      else if ((checkForSection == "end")||(checkForSection.StartsWith("end "))) return B_NO_ERROR;
      else MRETURN_ON_ERROR(ParseArgsAux(lineOfText, optAddToMsg, optAddToQueue, cs));
   }
   return B_NO_ERROR;
}

static status_t ParseFileAux(const String * optInStr, FILE * fpIn, Message * optAddToMsg, Queue<String> * optAddToQueue, bool cs)
{
   TCHECKPOINT;

   if (optInStr)
   {
      StringTokenizer tok(optInStr->Cstr(), NULL, "\r\n");
      return (tok.GetRemainderOfString() != NULL) ? ParseFileAux(&tok, NULL, optAddToMsg, optAddToQueue, NULL, 0, cs) : B_BAD_ARGUMENT;
   } 
   else
   {
      const int bufSize = 2048;
      char * buf = newnothrow_array(char, bufSize);
      MRETURN_OOM_ON_NULL(buf);

      const status_t ret = ParseFileAux(NULL, fpIn, optAddToMsg, optAddToQueue, buf, bufSize, cs);
      delete [] buf;
      return ret;
   }
}
status_t ParseFile(FILE * fpIn, Message & addTo, bool cs)            {return ParseFileAux(NULL, fpIn, &addTo, NULL,   cs);}
status_t ParseFile(FILE * fpIn, Queue<String> & addTo, bool cs)      {return ParseFileAux(NULL, fpIn, NULL,   &addTo, cs);}
status_t ParseFile(const String & s, Message & addTo, bool cs)       {return ParseFileAux(&s,   NULL, &addTo, NULL,   cs);}
status_t ParseFile(const String & s, Queue<String> & addTo, bool cs) {return ParseFileAux(&s,   NULL, NULL,   &addTo, cs);}

static void AddUnparseFileLine(FILE * optFile, String * optString, const String & indentStr, const String & s)
{
#ifdef WIN32
   static const char * eol = "\r\n";
#else
   static const char * eol = "\n";
#endif

   if (optString) 
   {
      *optString += indentStr;
      *optString += s;
      *optString += eol;
   }
   else fprintf(optFile, "%s%s%s", indentStr(), s(), eol);
}

static status_t UnparseFileAux(const Message & readFrom, FILE * optFile, String * optString, uint32 indentLevel)
{
   if ((optFile == NULL)&&(optString == NULL)) return B_BAD_ARGUMENT;

   const String indentStr = String().Pad(indentLevel);
   Message scratchMsg;
   for (MessageFieldNameIterator fnIter(readFrom); fnIter.HasData(); fnIter++)
   {
      const String & fn = fnIter.GetFieldName();
      uint32 tc;
      if (readFrom.GetInfo(fn, &tc).IsOK())
      {
         switch(tc)
         {
            case B_MESSAGE_TYPE:
            {
               MessageRef nextVal;
               for (uint32 i=0; readFrom.FindMessage(fn, i, nextVal).IsOK(); i++)
               {
                  AddUnparseFileLine(optFile, optString, indentStr, String("begin %1").Arg(fn));
                  MRETURN_ON_ERROR(UnparseFileAux(*nextVal(), optFile, optString, indentLevel+3));
                  AddUnparseFileLine(optFile, optString, indentStr, "end");
               }
            }
            break;

            case B_STRING_TYPE:
            {
               const String * nextVal;
               for (uint32 i=0; readFrom.FindString(fn, i, &nextVal).IsOK(); i++)
               {
                  scratchMsg.Clear(); MRETURN_ON_ERROR(scratchMsg.AddString(fn, *nextVal));
                  AddUnparseFileLine(optFile, optString, indentStr, UnparseArgs(scratchMsg));
               }
            }
            break;

            default:
               // do nothing
            break;
         }
      }
      else return B_LOGIC_ERROR;  // should never happen
   }
   return B_NO_ERROR;
}

status_t UnparseFile(const Message & readFrom, FILE * file) {return UnparseFileAux(readFrom, file, NULL, 0);}
String UnparseFile(const Message & readFrom) {String s; return (UnparseFileAux(readFrom, NULL, &s, 0).IsOK()) ? s : "";}

static status_t ParseConnectArgAux(const String & s, uint32 startIdx, uint16 & retPort, bool portRequired)
{
   const int32 colIdx = s.IndexOf(':', startIdx);
   const char * pStr = (colIdx>=0)?(s()+colIdx+1):NULL;
   if ((pStr)&&(muscleInRange(*pStr, '0', '9')))
   {
      const uint16 p = (uint16) atoi(pStr);
      if (p > 0) retPort = p;
      return B_NO_ERROR; 
   }
   else return portRequired ? B_BAD_ARGUMENT : B_NO_ERROR;
}

status_t ParseConnectArg(const Message & args, const String & fn, String & retHost, uint16 & retPort, bool portRequired, uint32 argIdx)
{
   const String * s;
   status_t ret;
   return (args.FindString(fn, argIdx, &s).IsOK(ret)) ? ParseConnectArg(*s, retHost, retPort, portRequired) : ret;
}

status_t ParseConnectArg(const String & s, String & retHost, uint16 & retPort, bool portRequired)
{
#ifndef MUSCLE_AVOID_IPV6
   const int32 rBracket = s.StartsWith('[') ? s.IndexOf(']') : -1;
   if (rBracket >= 0)
   {
      // If there are brackets, they are assumed to surround the address part, e.g. "[::1]:9999"
      retHost = s.Substring(1,rBracket);
      return ParseConnectArgAux(s, rBracket+1, retPort, portRequired);
   }
   else if (s.GetNumInstancesOf(':') != 1)  // I assume IPv6-style address strings never have exactly one colon in them
   {  
      retHost = s;
      return portRequired ? B_BAD_ARGUMENT : B_NO_ERROR;
   }  
#endif

   retHost = s.Substring(0, ":");
   return ParseConnectArgAux(s, retHost.Length(), retPort, portRequired);
}

status_t ParsePortArg(const Message & args, const String & fn, uint16 & retPort, uint32 argIdx)
{
   TCHECKPOINT;

   status_t ret;
   const char * v;
   if (args.FindString(fn, argIdx, &v).IsOK(ret))
   {
      const uint16 r = (uint16) atoi(v);
      if (r > 0)
      {
         retPort = r;
         return B_NO_ERROR;
      }
      else return B_BAD_ARGUMENT;
   }
   return ret;
}

#if defined(__linux__) || defined(__APPLE__)
static void CrashSignalHandler(int sig)
{
   // Uninstall this handler, to avoid the possibility of an infinite regress
   signal(SIGSEGV, SIG_DFL);
   signal(SIGBUS,  SIG_DFL);
   signal(SIGILL,  SIG_DFL);
   signal(SIGABRT, SIG_DFL);
   signal(SIGFPE,  SIG_DFL); 

   printf("MUSCLE CrashSignalHandler called with signal %i... I'm going to print a stack trace, then kill the process.\n", sig);
   PrintStackTrace();
   printf("Crashed MUSCLE process aborting now.... bye!\n");
   fflush(stdout);
   abort();
}
#endif

#if defined(MUSCLE_USE_MSVC_STACKWALKER) && !defined(MUSCLE_INLINE_LOGGING)
extern void _Win32PrintStackTraceForContext(FILE * outFile, CONTEXT * context, uint32 maxDepth);

LONG Win32FaultHandler(struct _EXCEPTION_POINTERS * ExInfo)
{
   SetUnhandledExceptionFilter(NULL);  // uninstall the handler to avoid the possibility of an infinite regress

   const char * faultDesc = "";
   switch(ExInfo->ExceptionRecord->ExceptionCode)
   {
      case EXCEPTION_ACCESS_VIOLATION      : faultDesc = "ACCESS VIOLATION"         ; break;
      case EXCEPTION_DATATYPE_MISALIGNMENT : faultDesc = "DATATYPE MISALIGNMENT"    ; break;
      case EXCEPTION_FLT_DIVIDE_BY_ZERO    : faultDesc = "FLT DIVIDE BY ZERO"       ; break;
      default                              : faultDesc = "(unknown)"                ; break;
    }

    const int    faultCode   = ExInfo->ExceptionRecord->ExceptionCode;
    const PVOID  CodeAddress = ExInfo->ExceptionRecord->ExceptionAddress;
    printf("****************************************************\n");
    printf("*** A Program Fault occurred:\n");
    printf("*** Error code %08X: %s\n", faultCode, faultDesc);
    printf("****************************************************\n");
#ifdef MUSCLE_64_BIT_PLATFORM
    printf("***   Address: %08llX\n", (uintptr)CodeAddress);
#else
    printf("***   Address: %08X\n", (uintptr)CodeAddress);
#endif
    printf("***     Flags: %08X\n", ExInfo->ExceptionRecord->ExceptionFlags);
    _Win32PrintStackTraceForContext(stdout, ExInfo->ContextRecord, MUSCLE_NO_LIMIT);
    printf("Crashed MUSCLE process aborting now.... bye!\n");
    fflush(stdout);

    return EXCEPTION_CONTINUE_SEARCH;  // now crash in the usual way
}
#endif

#ifdef __linux__
static status_t SetRealTimePriority(const char * priStr, bool useFifo)
{
   struct sched_param schedparam; memset(&schedparam, 0, sizeof(schedparam));
   const int pri = (strlen(priStr) > 0) ? atoi(priStr) : 11;
   schedparam.sched_priority = pri;

   const char * desc = useFifo ? "SCHED_FIFO" : "SCHED_RR";
   if (sched_setscheduler(0, useFifo?SCHED_FIFO:SCHED_RR, &schedparam) == 0)
   {
      LogTime(MUSCLE_LOG_INFO, "Set process to real-time (%s) priority %i\n", desc, pri);
      return B_NO_ERROR;
   }
   else 
   {
      LogTime(MUSCLE_LOG_ERROR, "Could not invoke real time (%s) scheduling priority %i [%s]\n", desc, pri, B_ERRNO());
      return B_ACCESS_DENIED;
   }
}
#endif

void HandleStandardDaemonArgs(const Message & args)
{
   TCHECKPOINT;

#ifndef WIN32
   if (args.HasName("disablestderr"))
   {
      LogTime(MUSCLE_LOG_INFO, "Suppressing all further output to stderr!\n");
      close(STDERR_FILENO);
   }
   if (args.HasName("disablestdout"))
   {
      LogTime(MUSCLE_LOG_INFO, "Suppressing all further output to stdout!\n");
      close(STDOUT_FILENO);
   }
#endif

   // Do this first, so that the stuff below will affect the right process.
   const char * n;
   if (args.FindString("daemon", &n).IsOK())
   {
      LogTime(MUSCLE_LOG_INFO, "Spawning off a daemon-child...\n");
      status_t ret;
      if (BecomeDaemonProcess(NULL, n[0] ? n : "/dev/null").IsError(ret))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Could not spawn daemon-child process! [%s]\n", ret());
         ExitWithoutCleanup(10);
      }
   }

#ifdef WIN32
   const String * consoleStr = args.GetStringPointer("console");
   if (consoleStr) Win32AllocateStdioConsole(consoleStr->Cstr());
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   {
      const char * df = args.GetCstr("deadlockfinder");
      if (df) _enableDeadlockFinderPrints = ParseBool(df, true);
   }
#endif

   const char * value;
   if (args.FindString("displaylevel", &value).IsOK())
   {
      int ll = ParseLogLevelKeyword(value);
      if (ll >= 0) SetConsoleLogLevel(ll);
              else LogTime(MUSCLE_LOG_INFO, "Error, unknown display log level type [%s]\n", value);
   }

   if ((args.FindString("oldlogfilespattern", &value).IsOK())&&(*value != '\0')) SetOldLogFilesPattern(value);

   if ((args.FindString("maxlogfiles", &value).IsOK())||(args.FindString("maxnumlogfiles", &value).IsOK()))
   {
      const uint32 maxNumFiles = (uint32) atol(value);
      if (maxNumFiles > 0) SetMaxNumLogFiles(maxNumFiles);
                      else LogTime(MUSCLE_LOG_ERROR, "Please specify a maxnumlogfiles value that is greater than zero.\n");
   }

   if (args.FindString("logfile", &value).IsOK())
   {
      SetFileLogName(value);
      if (GetFileLogLevel() == MUSCLE_LOG_NONE) SetFileLogLevel(MUSCLE_LOG_INFO); // no sense specifying a name and then not logging anything!
   }

   if (args.FindString("filelevel", &value).IsOK())
   {
      const int ll = ParseLogLevelKeyword(value);
      if (ll >= 0) SetFileLogLevel(ll);
              else LogTime(MUSCLE_LOG_INFO, "Error, unknown file log level type [%s]\n", value);
   }

   if (args.FindString("maxlogfilesize", &value).IsOK())
   {
      const uint32 maxSizeKB = (uint32) atol(value);
      if (maxSizeKB > 0) SetFileLogMaximumSize(maxSizeKB*1024);
                    else LogTime(MUSCLE_LOG_ERROR, "Please specify a maxlogfilesize in kilobytes, that is greater than zero.\n");
   }

   if ((args.HasName("compresslogfile"))||(args.HasName("compresslogfiles"))) SetFileLogCompressionEnabled(true);

   if (args.FindString("localhost", &value).IsOK())
   {
      const IPAddress ip = Inet_AtoN(value);
      if (ip != invalidIP)
      {
         char ipbuf[64]; Inet_NtoA(ip, ipbuf);
         LogTime(MUSCLE_LOG_INFO, "IP address [%s] will be used as the localhost address.\n", ipbuf);
         SetLocalHostIPOverride(ip);
      }
      else LogTime(MUSCLE_LOG_ERROR, "Error parsing localhost IP address [%s]!\n", value);
   }

   if (args.FindString("dnscache", &value).IsOK())
   {
      const uint64 micros = ParseHumanReadableTimeIntervalString(value);
      if (micros > 0)
      {
         uint32 maxCacheSize = 1024;
         if (args.FindString("dnscachesize", &value).IsOK()) maxCacheSize = (uint32) atol(value);
         LogTime(MUSCLE_LOG_INFO, "Setting DNS cache parameters to " UINT32_FORMAT_SPEC " entries, expiration period is %s\n", maxCacheSize, GetHumanReadableTimeIntervalString(micros)());
         SetHostNameCacheSettings(maxCacheSize, micros);
      }
      else LogTime(MUSCLE_LOG_ERROR, "Unable to parse time interval string [%s] for dnscache argument!\n", value);
   }

   if ((args.HasName("debugcrashes"))||(args.HasName("debugcrash")))
   {
#if defined(__linux__) || defined(__APPLE__)
      LogTime(MUSCLE_LOG_INFO, "Enabling stack-trace printing when a crash occurs.\n");
      signal(SIGSEGV, CrashSignalHandler);
      signal(SIGBUS,  CrashSignalHandler);
      signal(SIGILL,  CrashSignalHandler);
      signal(SIGABRT, CrashSignalHandler);
      signal(SIGFPE,  CrashSignalHandler); 
#elif MUSCLE_USE_MSVC_STACKWALKER
# ifndef MUSCLE_INLINE_LOGGING
      LogTime(MUSCLE_LOG_INFO, "Enabling stack-trace printing when a crash occurs.\n");
      SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER) Win32FaultHandler);
# endif
#else
      LogTime(MUSCLE_LOG_ERROR, "Can't enable stack-trace printing when a crash occurs, that feature isn't supported on this platform!\n");
#endif
   }

#if defined(__linux__) || defined(__APPLE__)
   {
      const char * niceStr = NULL; (void) args.FindString("nice", &niceStr);
      const char * meanStr = NULL; (void) args.FindString("mean", &meanStr);

      const int32 niceLevel = niceStr ? ((strlen(niceStr) > 0) ? atoi(niceStr) : 5) : 0;
      const int32 meanLevel = meanStr ? ((strlen(meanStr) > 0) ? atoi(meanStr) : 5) : 0;
      const int32 effectiveLevel = niceLevel-meanLevel;

      if (effectiveLevel)
      {
         errno = 0;  // the only reliable way to check for an error here :^P
         const int ret = nice(effectiveLevel);  // I'm only looking at the return value to shut gcc 4.4.3 up
         if (errno != 0) LogTime(MUSCLE_LOG_WARNING, "Could not change process execution priority to " INT32_FORMAT_SPEC " (ret=%i) [%s].\n", effectiveLevel, ret, B_ERRNO());
                    else LogTime(MUSCLE_LOG_INFO, "Process is now %s (niceLevel=%i)\n", (effectiveLevel<0)?"mean":"nice", effectiveLevel);
      }
   }
#endif

#ifdef __linux__
   const char * priStr;
        if (args.FindString("realtime",      &priStr).IsOK()) SetRealTimePriority(priStr, false);
   else if (args.FindString("realtime_rr",   &priStr).IsOK()) SetRealTimePriority(priStr, false);
   else if (args.FindString("realtime_fifo", &priStr).IsOK()) SetRealTimePriority(priStr, true);
#endif

#ifdef MUSCLE_CATCH_SIGNALS_BY_DEFAULT
# ifdef MUSCLE_AVOID_SIGNAL_HANDLING
#  error "MUSCLE_CATCH_SIGNALS_BY_DEFAULT and MUSCLE_AVOID_SIGNAL_HANDLING are mutually exclusive compiler flags... you can not specify both!"
# endif
   if (args.HasName("dontcatchsignals"))
   {
      _mainReflectServerCatchSignals = false;
      LogTime(MUSCLE_LOG_DEBUG, "Controlled shutdowns (via Control-C) disabled in the main thread.\n");
   }
#else
   if (args.HasName("catchsignals"))
   {
# ifdef MUSCLE_AVOID_SIGNAL_HANDLING
      LogTime(MUSCLE_LOG_ERROR, "Can not enable controlled shutdowns, MUSCLE_AVOID_SIGNAL_HANDLING was specified during compilation!\n");
# else
      _mainReflectServerCatchSignals = true;
      LogTime(MUSCLE_LOG_DEBUG, "Controlled shutdowns (via Control-C) enabled in the main thread.\n");
# endif
   }
#endif

   if (args.HasName("printnetworkinterfaces"))
   {
      Queue<NetworkInterfaceInfo> infos;
      if (GetNetworkInterfaceInfos(infos).IsOK())
      {
         printf("--- Network interfaces on this machine are as follows: ---\n");
         for (uint32 i=0; i<infos.GetNumItems(); i++) printf("  %s\n", infos[i].ToString()());
         printf("--- (end of list) ---\n");
      }
   }
}

static bool _isDaemonProcess = false;
bool IsDaemonProcess() {return _isDaemonProcess;}

/* Source code stolen from UNIX Network Programming, Volume 1
 * Comments from the Unix FAQ
 */
#ifdef WIN32
status_t SpawnDaemonProcess(bool &, const char *, const char *, bool) 
{ 
   return B_UNIMPLEMENTED;  // Win32 can't do this trick, he's too lame  :^(
}
#else
status_t SpawnDaemonProcess(bool & returningAsParent, const char * optNewDir, const char * optOutputTo, bool createIfNecessary)
{
   TCHECKPOINT;

   // Here are the steps to become a daemon:
   // 1. fork() so the parent can exit, this returns control to the command line or shell invoking
   //    your program. This step is required so that the new process is guaranteed not to be a process
   //    group leader. The next step, setsid(), fails if you're a process group leader.
   pid_t pid = fork();
   if (pid < 0) return B_ERRNO;
   if (pid > 0) 
   {
      returningAsParent = true;
      return B_NO_ERROR;
   }
   else returningAsParent = false; 

   // 2. setsid() to become a process group and session group leader. Since a controlling terminal is
   //    associated with a session, and this new session has not yet acquired a controlling terminal
   //    our process now has no controlling terminal, which is a Good Thing for daemons.
   setsid();

   // 3. fork() again so the parent, (the session group leader), can exit. This means that we, as a
   //    non-session group leader, can never regain a controlling terminal.
   signal(SIGHUP, SIG_IGN);
   pid = fork();
   if (pid < 0) return B_ERRNO;
   if (pid > 0) ExitWithoutCleanup(0);

   // 4. chdir("/") can ensure that our process doesn't keep any directory in use. Failure to do this
   //    could make it so that an administrator couldn't unmount a filesystem, because it was our
   //    current directory. [Equivalently, we could change to any directory containing files important
   //    to the daemon's operation.]
   if ((optNewDir)&&(chdir(optNewDir) != 0)) return B_ERRNO;

   // 5. umask(0) so that we have complete control over the permissions of anything we write.
   //    We don't know what umask we may have inherited. [This step is optional]
   (void) umask(0);

   // 6. close() fds 0, 1, and 2. This releases the standard in, out, and error we inherited from our parent
   //    process. We have no way of knowing where these fds might have been redirected to. Note that many
   //    daemons use sysconf() to determine the limit _SC_OPEN_MAX. _SC_OPEN_MAX tells you the maximun open
   //    files/process. Then in a loop, the daemon can close all possible file descriptors. You have to
   //    decide if you need to do this or not. If you think that there might be file-descriptors open you should
   //    close them, since there's a limit on number of concurrent file descriptors.
   // 7. Establish new open descriptors for stdin, stdout and stderr. Even if you don't plan to use them,
   //    it is still a good idea to have them open. The precise handling of these is a matter of taste;
   //    if you have a logfile, for example, you might wish to open it as stdout or stderr, and open `/dev/null'
   //    as stdin; alternatively, you could open `/dev/console' as stderr and/or stdout, and `/dev/null' as stdin,
   //    or any other combination that makes sense for your particular daemon.
   const mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;
   const int nullfd = open("/dev/null", O_RDWR, mode);
   if (nullfd >= 0) dup2(nullfd, STDIN_FILENO);

   int outfd = -1;
   if (optOutputTo) 
   {
      outfd = open(optOutputTo, O_WRONLY | (createIfNecessary ? O_CREAT : 0), mode);
      if (outfd < 0) LogTime(MUSCLE_LOG_ERROR, "BecomeDaemonProcess():  Could not open %s to redirect stdout, stderr [%s]\n", optOutputTo, B_ERRNO());
   }
   if (outfd >= 0)
   {
      (void) dup2(outfd, STDOUT_FILENO);
      (void) dup2(outfd, STDERR_FILENO);
   }

   _isDaemonProcess = true;
   return B_NO_ERROR;
}
#endif

status_t BecomeDaemonProcess(const char * optNewDir, const char * optOutputTo, bool createIfNecessary)
{
   bool isParent = false;  // set to false to avoid compiler warning
   const status_t ret = SpawnDaemonProcess(isParent, optNewDir, optOutputTo, createIfNecessary);
   if ((ret.IsOK())&&(isParent)) ExitWithoutCleanup(0);
   return ret;
}

void RemoveANSISequences(String & s)
{
   TCHECKPOINT;

   static const char _escapeBuf[] = {0x1B, '[', '\0'};
   static String _escape; if (_escape.IsEmpty()) _escape = _escapeBuf;

   while(true)
   {
      const int32 idx = s.IndexOf(_escape);  // find the next escape sequence
      if (idx >= 0)
      {
         const char * data = s()+idx+2;  // move past the ESC char and the [ char
         switch(data[0])
         {
            case 's': case 'u': case 'K':   // these are single-letter codes, so
               data++;                      // just skip over them and we are done
            break;

            case '=':
               data++;
            // fall through!
            default:
               // For numeric codes, keep going until we find a non-digit that isn't a semicolon
               while((muscleInRange(*data, '0', '9'))||(*data == ';')) data++;
               if (*data) data++;  // and skip over the trailing letter too.
            break;
         }
         s = s.Substring(0, idx) + s.Substring((uint32)(data-s()));  // remove the escape substring
      }
      else break;
   }
}

String CleanupDNSLabel(const String & s, const String & optAdditionalAllowedChars)
{
   const uint32 len = muscleMin(s.Length(), (uint32)63);  // DNS spec says maximum 63 chars per label!
   String ret; if (ret.Prealloc(len).IsError()) return ret;
  
   const char * p = s();
   for (uint32 i=0; i<len; i++)
   {
      const char c = p[i];
      switch(c)
      {
         case '\'': case '\"': case '(': case ')': case '[': case ']': case '{': case '}':
            // do nothing -- we will omit delimiters
         break;

         default:
                 if ((muscleInRange(c, '0', '9'))||(muscleInRange(c, 'A', 'Z'))||(muscleInRange(c, 'a', 'z'))||(optAdditionalAllowedChars.Contains(c))) ret += c;
            else if ((ret.HasChars())&&(ret.EndsWith('-') == false)) ret += '-';
         break;
      }
   }
   while(ret.EndsWith('-')) ret -= '-';  // remove any trailing dashes
   return ret.Trim();
}

String CleanupDNSPath(const String & orig, const String & optAdditionalAllowedChars)
{
   String ret; (void) ret.Prealloc(orig.Length());

   const char * s;
   StringTokenizer tok(orig(), NULL, ".");
   while((s = tok()) != NULL)
   {
      String cleanTok = CleanupDNSLabel(s, optAdditionalAllowedChars);
      if (cleanTok.HasChars())
      {
         if (ret.HasChars()) ret += '.';
         ret += cleanTok;
      }
   }
   return ret;
}

status_t NybbleizeData(const uint8 * b, uint32 numBytes, String & retString)
{
   MRETURN_ON_ERROR(retString.Prealloc(numBytes*2));

   retString.Clear();
   for (uint32 i=0; i<numBytes; i++)
   {
      const uint8 c = b[i];
      retString += ((c>>0)&0x0F)+'A';
      retString += ((c>>4)&0x0F)+'A';
   }
   return B_NO_ERROR;
}

status_t NybbleizeData(const ByteBuffer & buf, String & retString)
{
   return NybbleizeData(buf.GetBuffer(), buf.GetNumBytes(), retString);
}

status_t DenybbleizeData(const String & nybbleizedText, ByteBuffer & retBuf)
{
   const uint32 numBytes = nybbleizedText.Length();
   if ((numBytes%2)!=0)
   {
      LogTime(MUSCLE_LOG_ERROR, "DenybblizeData:  Nybblized text [%s] has an odd length; that shouldn't ever happen!\n", nybbleizedText());
      return B_BAD_DATA;
   }

   MRETURN_ON_ERROR(retBuf.SetNumBytes(numBytes/2, false));

   uint8 * b = retBuf.GetBuffer();
   for (uint32 i=0; i<numBytes; i+=2)
   {
      const char c1 = nybbleizedText[i+0];
      const char c2 = nybbleizedText[i+1];
      if ((muscleInRange(c1, 'A', 'P') == false)||(muscleInRange(c2, 'A', 'P') == false))
      {
         LogTime(MUSCLE_LOG_ERROR, "DenybblizeData:  Nybblized text [%s] contains characters other than A through P!\n", nybbleizedText());
         return B_BAD_DATA;
      }
      *b++ = (uint8) (((c1-'A')<<0)|((c2-'A')<<4));
   }
   return B_NO_ERROR;
}

String NybbleizeString(const String & s)
{
   String retStr;
   ByteBuffer inBuf; inBuf.AdoptBuffer(s.Length(), (uint8 *) s());

   const status_t ret = NybbleizeData(inBuf, retStr);
   (void) inBuf.ReleaseBuffer();

   if (ret.IsError()) retStr.Clear();
   return retStr;
}

String DenybbleizeString(const String & ns)
{
   ByteBuffer outBuf;
   return (DenybbleizeData(ns, outBuf).IsOK()) ? String((const char *) outBuf.GetBuffer(), outBuf.GetNumBytes()) : String();
}

const uint8 * MemMem(const uint8 * lookIn, uint32 numLookInBytes, const uint8 * lookFor, uint32 numLookForBytes)
{
        if (numLookForBytes == 0)              return lookIn;  // hmm, existential questions here
   else if (numLookForBytes == numLookInBytes) return (memcmp(lookIn, lookFor, numLookInBytes) == 0) ? lookIn : NULL;
   else if (numLookForBytes < numLookInBytes)
   {
      const uint32 scanLength = (1+numLookInBytes-numLookForBytes);
      for (uint32 i=0; i<scanLength; i++)
      {
         const uint8 * li = &lookIn[i];
         if ((*li == *lookFor)&&(memcmp(li, lookFor, numLookForBytes) == 0)) return li;  // FogBugz #9877
      }
   }
   return NULL;
}

String HexBytesToString(const uint8 * buf, uint32 numBytes)
{
   String ret;
   if (ret.Prealloc(numBytes*3).IsOK())
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         if (i > 0) ret += ' ';
         char b[32]; muscleSprintf(b, "%02x", buf[i]);
         ret += b;
      }
   }
   return ret;
}

String HexBytesToString(const ConstByteBufferRef & bbRef)
{
   return bbRef() ? HexBytesToString(*bbRef()) : String("(null)");
}

String HexBytesToString(const ByteBuffer & bb)
{
   return HexBytesToString(bb.GetBuffer(), bb.GetNumBytes());
}

String HexBytesToString(const Queue<uint8> & bytes)
{
   const uint32 numBytes = bytes.GetNumItems();

   String ret;
   if (ret.Prealloc(numBytes*3).IsOK())
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         if (i > 0) ret += ' ';
         char b[32]; muscleSprintf(b, "%02x", bytes[i]);
         ret += b;
      }
   }
   return ret;
}

ByteBufferRef ParseHexBytes(const char * buf)
{
   ByteBufferRef bb = GetByteBufferFromPool((uint32)strlen(buf));
   if (bb())
   {
      uint8 * b = bb()->GetBuffer();
      uint32 count = 0;
      StringTokenizer tok(buf, NULL);  // soft/whitespace separators only
      const char * next;
      while((next = tok()) != NULL) 
      {
         if (strlen(next) > 0) 
         {
                 if (next[0] == '/') b[count++] = next[1];
            else if (next[0] == '\\')
            {
               // handle standard C escaped-control-chars conventions also (\r, \n, \t, etc)
               char c = 0;
               switch(next[1])
               {
                  case 'a':  c = 0x07; break;
                  case 'b':  c = 0x08; break;
                  case 'f':  c = 0x0C; break;
                  case 'n':  c = 0x0A; break;
                  case 'r':  c = 0x0D; break;
                  case 't':  c = 0x09; break;
                  case 'v':  c = 0x0B; break;
                  case '\\': c = 0x5C; break;
                  case '\'': c = 0x27; break;
                  case '"':  c = 0x22; break;
                  case '?':  c = 0x3F; break;
               }
               b[count++] = c;
            }
            else b[count++] = (uint8) strtol(next, NULL, 16);
         }
      }
      bb()->SetNumBytes(count, true);
   }
   return bb;
}

status_t AssembleBatchMessage(MessageRef & batchMsg, const MessageRef & newMsg, bool prepend)
{
   if (batchMsg() == NULL)
   {
      batchMsg = newMsg;
      return B_NO_ERROR;
   }
   else if (batchMsg()->what == PR_COMMAND_BATCH) return prepend ? batchMsg()->PrependMessage(PR_NAME_KEYS, newMsg) : batchMsg()->AddMessage(PR_NAME_KEYS, newMsg);
   else
   {
      MessageRef newBatchMsg = GetMessageFromPool(PR_COMMAND_BATCH);
      MRETURN_OOM_ON_NULL(newBatchMsg());

      status_t ret;
      if ((newBatchMsg()->AddMessage(PR_NAME_KEYS, prepend?newMsg:batchMsg).IsOK(ret))&&(newBatchMsg()->AddMessage(PR_NAME_KEYS, prepend?batchMsg:newMsg).IsOK(ret)))
      {
         batchMsg = newBatchMsg;
         return B_NO_ERROR;
      }
      else return ret;
   }
}

bool FileExists(const char * filePath)
{
   FILE * fp = muscleFopen(filePath, "rb");
   const bool ret = (fp != NULL);  // gotta take this value before calling fclose(), or cppcheck complains
   if (fp) fclose(fp);
   return ret;
}

status_t RenameFile(const char * oldPath, const char * newPath)
{
   return (rename(oldPath, newPath) == 0) ? B_NO_ERROR : B_ERRNO;
}

static status_t CopyDirectoryRecursive(const char * oldDirPath, const char * newDirPath)
{
   if (strcmp(oldDirPath, newDirPath) == 0) return B_NO_ERROR;  // paranoia: Copying a directory onto itself is a no-op

   Directory srcDir(oldDirPath);
   if (srcDir.IsValid() == false) return B_FILE_NOT_FOUND;

   MRETURN_ON_ERROR(Directory::MakeDirectory(newDirPath, true, true));

   const String srcDirPath = srcDir.GetPath();

   String dstDirPath;
   {
      // In an inner scope simply to keep (dstDir) off the stack during any recursion
      Directory dstDir(newDirPath);
      if (dstDir.IsValid() == false) return B_FILE_NOT_FOUND;
      dstDirPath = dstDir.GetPath();
   }

   const char * curSourceName;
   while((curSourceName = srcDir.GetCurrentFileName()) != NULL)
   {
      if ((strcmp(curSourceName, ".") != 0)&&(strcmp(curSourceName, "..") != 0)) MRETURN_ON_ERROR(CopyFile((srcDirPath+curSourceName)(), (dstDirPath+curSourceName)(), true));
      srcDir++;
   }

   return B_NO_ERROR;
}

status_t CopyFile(const char * oldPath, const char * newPath, bool allowCopyFolder)
{
   if (strcmp(oldPath, newPath) == 0) return B_NO_ERROR;  // Copying something onto itself is a no-op

   if (allowCopyFolder)
   {
      const FilePathInfo oldFPI(oldPath);
      if (oldFPI.IsDirectory()) return CopyDirectoryRecursive(oldPath, newPath);
   }

   FILE * fpIn = muscleFopen(oldPath, "rb");
   if (fpIn == NULL) return B_ERRNO;

   status_t ret = B_NO_ERROR;  // optimistic default
   FILE * fpOut = muscleFopen(newPath, "wb");
   if (fpOut)
   {
      while(1)
      {
         char buf[4*1024];
         const size_t bytesRead = fread(buf, 1, sizeof(buf), fpIn);
         if ((bytesRead < sizeof(buf))&&(feof(fpIn) == false))
         {
            ret = B_ERRNO;
            break;
         }
         
         const size_t bytesWritten = fwrite(buf, 1, bytesRead, fpOut);
         if (bytesWritten < bytesRead)
         {
            ret = B_ERRNO;
            break;
         }
         if (feof(fpIn)) break;
      }
      fclose(fpOut);
   }
   else ret = B_ERRNO;

   fclose(fpIn);

   if ((fpOut)&&(ret.IsError())) (void) DeleteFile(newPath);  // clean up on error
   return ret;
}

status_t DeleteFile(const char * filePath)
{
#ifdef _MSC_VER
   const int unlinkRet = _unlink(filePath);
#else
   const int unlinkRet = unlink(filePath);
#endif
   return (unlinkRet == 0) ? B_NO_ERROR : B_ERRNO;
}

String GetHumanReadableProgramNameFromArgv0(const char * argv0)
{
   String ret = argv0;

#ifdef __APPLE__
   ret = ret.Substring(0, ".app/");  // we want the user-visible name, not the internal name!
#endif

#ifdef __WIN32__
   ret = ret.Substring("\\").Substring(0, ".exe");
#else
   ret = ret.Substring("/");
#endif
   return ret;
}

#ifdef WIN32
void Win32AllocateStdioConsole(const char * optArg)
{
   const String optOutFile = optArg;

   const char * conInStr  = optOutFile.HasChars() ? NULL         : "conin$";
   const char * conOutStr = optOutFile.HasChars() ? optOutFile() : "conout$";
   if (optOutFile.IsEmpty()) AllocConsole();  // no sense creating a DOS window if we're only outputting to a file anyway

   // Hopefully-temporary work-around for Windows not wanting to send stdout and stderr to the same file
   String conErrHolder;  // don't move this!  It needs to stay here
   const char * conErrStr = NULL;
   if (optOutFile.HasChars())
   {
      const int lastDotIdx = optOutFile.LastIndexOf('.');

      if (lastDotIdx > 0)
         conErrHolder = optOutFile.Substring(0, lastDotIdx) + "_stderr" + optOutFile.Substring(lastDotIdx);
      else
         conErrHolder = optOutFile + "_stderr.txt";

      conErrStr = conErrHolder();
   }
   else conErrStr = conOutStr;  // for the output-to-console-window case, where redirecting both stdout and stderr DOES work

# if __STDC_WANT_SECURE_LIB__
   FILE * junk;
   if (conInStr)  (void) freopen_s(&junk, conInStr,  "r", stdin);
   if (conOutStr) (void) freopen_s(&junk, conOutStr, "w", stdout);
   if (conErrStr) (void) freopen_s(&junk, conErrStr, "w", stderr);
# else
   if (conInStr)  (void) freopen(conInStr,  "r", stdin);
   if (conOutStr) (void) freopen(conOutStr, "w", stdout);
   if (conErrStr) (void) freopen(conErrStr, "w", stderr);
# endif
}
#endif

#if defined(__linux__) || defined(__APPLE__)
static double ParseMemValue(const char * b)
{
   while((*b)&&(muscleInRange(*b, '0', '9') == false)) b++;
   return muscleInRange(*b, '0', '9') ? atof(b) : -1.0;
}
#endif

float GetSystemMemoryUsagePercentage()
{
#if defined(__linux__)
   FILE * fpIn = muscleFopen("/proc/meminfo", "r");
   if (fpIn)
   {
      double memTotal = -1.0, memFree = -1.0, buffered = -1.0, cached = -1.0;
      char buf[512];
      while((fgets(buf, sizeof(buf), fpIn) != NULL)&&((memTotal<=0.0)||(memFree<0.0)||(buffered<0.0)||(cached<0.0)))
      {
              if (strncmp(buf, "MemTotal:", 9) == 0) memTotal = ParseMemValue(buf+9);
         else if (strncmp(buf, "MemFree:",  8) == 0) memFree  = ParseMemValue(buf+8);
         else if (strncmp(buf, "Buffers:",  8) == 0) buffered = ParseMemValue(buf+8);
         else if (strncmp(buf, "Cached:",   7) == 0) cached   = ParseMemValue(buf+7);
      }
      fclose(fpIn);

      if ((memTotal > 0.0)&&(memFree >= 0.0)&&(buffered >= 0.0)&&(cached >= 0.0))
      {
         const double memUsed = memTotal-(memFree+buffered+cached);
         return (float) (memUsed/memTotal);
      }
   }
#elif defined(__APPLE__)
   FILE * fpIn = popen("/usr/bin/vm_stat", "r");
   if (fpIn)
   {
      double pagesUsed = 0.0, totalPages = 0.0;
      char buf[512];
      while(fgets(buf, sizeof(buf), fpIn) != NULL)
      {
         if (strncmp(buf, "Pages", 5) == 0)
         {
            const double val = ParseMemValue(buf);
            if (val >= 0.0)
            {
               if ((strncmp(buf, "Pages wired", 11) == 0)||(strncmp(buf, "Pages active", 12) == 0)) pagesUsed += val;
               totalPages += val;
            }
         }
         else if (strncmp(buf, "Mach Virtual Memory Statistics", 30) != 0) break;  // Stop at "Translation Faults", we don't care about anything at or below that
      }
      pclose(fpIn);

      if (totalPages > 0.0) return (float) (pagesUsed/totalPages);
   }
#elif defined(WIN32) && !defined(__MINGW32__)
   MEMORYSTATUSEX stat; memset(&stat, 0, sizeof(stat));
   stat.dwLength = sizeof(stat);
   if (GlobalMemoryStatusEx(&stat)) return ((float)stat.dwMemoryLoad)/100.0f;
#endif
   return -1.0f;
}

bool ParseBool(const String & word, bool defaultValue)
{
   static const char * _onWords[]  = {"on",  "enable",  "enabled",  "true",  "t", "y", "yes", "1"};
   static const char * _offWords[] = {"off", "disable", "disabled", "false", "f", "n", "no",  "0"};

   const String s = word.Trim().ToLowerCase();
   for (uint32 i=0; i<ARRAYITEMS(_onWords);  i++) if (s == _onWords[i])  return true;
   for (uint32 i=0; i<ARRAYITEMS(_offWords); i++) if (s == _offWords[i]) return false;
   return defaultValue; 
}

} // end namespace muscle
