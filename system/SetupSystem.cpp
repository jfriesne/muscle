/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/SetupSystem.h"
#include "support/Flattenable.h"
#include "dataio/SeekableDataIO.h"
#include "reflector/SignalHandlerSession.h"  // for SetMainReflectServerCatchSignals()
#include "util/ObjectPool.h"
#include "util/ByteBuffer.h"
#include "util/DebugTimer.h"
#include "util/CountedObject.h"
#include "util/Hashtable.h"
#include "util/MiscUtilityFunctions.h"     // for PrintHexBytes()
#include "util/NetworkUtilityFunctions.h"  // for IPAddressAndPort
#include "util/OutputPrinter.h"
#include "util/String.h"

#ifdef MUSCLE_ENABLE_SSL
# include <openssl/err.h>
# include <openssl/ssl.h>
# ifndef WIN32
#  include <pthread.h>
# endif
#endif

#if defined(__APPLE__)
# include "AvailabilityMacros.h"  // so we can find out if this version of MacOS/X is new enough to include backtrace() and friends
#endif

#if (defined(__linux__) && !defined(ANDROID)) || (defined(MAC_OS_X_VERSION_10_5) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5))
# include <execinfo.h>
# define MUSCLE_USE_BACKTRACE 1
#endif

#ifdef __linux__
# include <time.h>   // for clock_nanosleep()
#endif

#ifdef WIN32
# if !(defined(__MINGW32__) || defined(__MINGW64__))
# pragma comment(lib, "ws2_32.lib")
# pragma comment(lib, "winmm.lib")
# pragma comment(lib, "iphlpapi.lib")
# pragma comment(lib, "version.lib")
# endif
# include <signal.h>
# include <mmsystem.h>
#else
# if defined(__CYGWIN__)
#  include <signal.h>
#  include <sys/select.h>
#  include <sys/signal.h>
#  include <sys/times.h>
# elif defined(__QNX__)
#  include <signal.h>
#  include <sys/times.h>
# elif defined(SUN) || defined(__sparc__) || defined(sun386)
#  include <signal.h>
#  include <sys/times.h>
#  include <limits.h>
# elif defined(ANDROID)
#  include <signal.h>
#  include <sys/times.h>
# else
#  include <signal.h>
#  include <sys/times.h>
# endif
#endif

#if defined(__BORLANDC__)
# include <math.h>
# include <float.h>
#endif

#if defined(__APPLE__)
# include <mach/mach.h>
# include <mach/mach_time.h>
# include <mach/message.h>      // for mach_msg_type_number_t
# include <mach/kern_return.h>  // for kern_return_t
# include <mach/task_info.h>
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# include "system/AtomicCounter.h"
# include "system/ThreadLocalStorage.h"
#endif

#ifdef WIN32
# include <psapi.h>     // for PROCESS_MEMORY_COUNTERS (yes, this include has to be down here)
#endif

namespace muscle {

#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
uint32 _stringOpCounts[NUM_STRING_OPS] = {0};
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
bool _muscleSingleThreadOnly = true;
#else
bool _muscleSingleThreadOnly = false;
#endif

#ifdef MUSCLE_CATCH_SIGNALS_BY_DEFAULT
bool _mainReflectServerCatchSignals = true;
#else
bool _mainReflectServerCatchSignals = false;
#endif

static Mutex * _muscleLock = NULL;
Mutex * GetGlobalMuscleLock() {return _muscleLock;}

#if defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
Mutex * _muscleAtomicMutexes = NULL;  // used by DoMutexAtomicIncrement()
#endif

static uint32 _threadSetupCount = 0;
#ifndef MUSCLE_SINGLE_THREAD_ONLY
static muscle_thread_id _mainThreadID;
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
# ifdef MUSCLE_DEFAULT_RUNTIME_DISABLE_DEADLOCK_FINDER
bool _enableDeadlockFinderPrints = false;
# else
bool _enableDeadlockFinderPrints = true;
# endif
#endif

static uint32 _failedMemoryRequestSize = MUSCLE_NO_LIMIT;  // start with an obviously-invalid guard value
uint32 GetAndClearFailedMemoryRequestSize();  // just to avoid a compiler warning
uint32 GetAndClearFailedMemoryRequestSize()
{
   const uint32 ret = _failedMemoryRequestSize; // yes, it's racy.  But I'll live with that for now.
   _failedMemoryRequestSize = MUSCLE_NO_LIMIT;
   return ret;
}
void SetFailedMemoryRequestSize(uint32 numBytes);  // just to avoid a compiler warning
void SetFailedMemoryRequestSize(uint32 numBytes) {_failedMemoryRequestSize = numBytes;}

static int swap_memcmp(const void * vp1, const void * vp2, uint32 numBytes)
{
   const uint8 * p1 = (const uint8 *) vp1;
   const uint8 * p2 = (const uint8 *) vp2;
   for (uint32 i=0; i<numBytes; i++)
   {
      const int diff = p2[numBytes-(i+1)]-p1[i];
      if (diff) return diff;
   }
   return 0;
}

// Implemented here so that every program doesn't have to link
// in MiscUtilityFunctions.cpp just for this function.
void ExitWithoutCleanup(int exitCode)
{
   _exit(exitCode);
}

void Crash()
{
#ifdef WIN32
   RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
#else
   abort();
#endif
}

static void GoInsane(const char * why, const char * why2 = NULL)
{
   printf("SanitySetupSystem:  MUSCLE COMPILATION RUNTIME SANITY CHECK FAILED!\n");
   printf("REASON:  %s %s\n", why, why2?why2:"");
   printf("PLEASE CHECK YOUR COMPILATION SETTINGS!  THIS PROGRAM WILL NOW EXIT.\n");
   fflush(stdout);
   ExitWithoutCleanup(10);
}

static void CheckOp(uint32 numBytes, const void * orig, const void * swapOne, const void * swapTwo, const void * origOne, const void * origTwo, const char * why)
{
   if ((swapOne)&&(swap_memcmp(orig, swapOne, numBytes))) GoInsane(why, "(swapOne)");
   if ((swapTwo)&&(swap_memcmp(orig, swapTwo, numBytes))) GoInsane(why, "(swapTwo)");
   if ((origOne)&&(memcmp(orig, origOne, numBytes) != 0)) GoInsane(why, "(origOne)");
   if ((origTwo)&&(memcmp(orig, origTwo, numBytes) != 0)) GoInsane(why, "(origTwo)");
}

#ifdef MUSCLE_AVOID_CPLUSPLUS11
namespace muscle_private
{
   const char * fake_nullptr[1] = {NULL};  // dummy nullptr, solely for the use of the C++03 hack-around in BitChord.h
}
#else
template<typename T> void VerifyTypeIsTrivial()
{
   if (std::is_trivial<T>::value == false)
   {
      char buf[512];
      muscleSprintf(buf, "ERROR:  Type %s was expected to be trivial, but std::is_trivial() returned false!\n", typeid(T).name());
      GoInsane(buf);
   }
}

template<typename T> void VerifyTypeIsNonTrivial()
{
   if (std::is_trivial<T>::value == true)
   {
      char buf[512];
      muscleSprintf(buf, "ERROR:  Type %s was expected to be non-trivial, but std::is_trivial() returned true!\n", typeid(T).name());
      GoInsane(buf);
   }
}
#endif

SanitySetupSystem :: SanitySetupSystem()
{
   // Make sure our data type lengths are as expected
   if (sizeof(uint8)  != 1) GoInsane("sizeof(uint8)  != 1");
   if (sizeof(int8)   != 1) GoInsane("sizeof(int8)   != 1");
   if (sizeof(uint16) != 2) GoInsane("sizeof(uint16) != 2");
   if (sizeof(int16)  != 2) GoInsane("sizeof(int16)  != 2");
   if (sizeof(uint32) != 4) GoInsane("sizeof(uint32) != 4");
   if (sizeof(int32)  != 4) GoInsane("sizeof(int32)  != 4");
   if (sizeof(uint64) != 8) GoInsane("sizeof(uint64) != 8");
   if (sizeof(int64)  != 8) GoInsane("sizeof(int64)  != 8");
   if (sizeof(float)  != 4) GoInsane("sizeof(float)  != 4");
   if (sizeof(double) != 8) GoInsane("sizeof(double) != 8");
   if (sizeof(uintptr) != sizeof(void *)) GoInsane("sizeof(uintptr) != sizeof(void *)");
   if (sizeof(ptrdiff) != sizeof(uintptr)) GoInsane("sizeof(ptrdiff) != sizeof(uintptr)");

   // Make sure our endian-ness info is correct
   static const uint32 one = 1;
   const bool testsLittleEndian = (*((const uint8 *) &one) == 1);

   // Make sure our endian-swap macros do what we expect them to
#if B_HOST_IS_BENDIAN
   if (testsLittleEndian) GoInsane("MUSCLE is compiled for a big-endian CPU, but host CPU is little-endian!?");
   else
   {
      {
         const uint16 orig = 0x1234;
         const uint16 HtoL = B_HOST_TO_LENDIAN_INT16(orig);
         const uint16 LtoH = B_LENDIAN_TO_HOST_INT16(orig);
         const uint16 HtoB = B_HOST_TO_BENDIAN_INT16(orig);  // should be a no-op
         const uint16 BtoH = B_BENDIAN_TO_HOST_INT16(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoL, &LtoH, &HtoB, &BtoH, "16-bit swap macro does not work!");
      }

      {
         const uint32 orig = 0x12345678;
         const uint32 HtoL = B_HOST_TO_LENDIAN_INT32(orig);
         const uint32 LtoH = B_LENDIAN_TO_HOST_INT32(orig);
         const uint32 HtoB = B_HOST_TO_BENDIAN_INT32(orig);  // should be a no-op
         const uint32 BtoH = B_BENDIAN_TO_HOST_INT32(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoL, &LtoH, &HtoB, &BtoH, "32-bit swap macro does not work!");
      }

      {
         const uint64 orig = (((uint64)0x12345678)<<32)|(((uint64)0x12312312));
         const uint64 HtoL = B_HOST_TO_LENDIAN_INT64(orig);
         const uint64 LtoH = B_LENDIAN_TO_HOST_INT64(orig);
         const uint64 HtoB = B_HOST_TO_BENDIAN_INT64(orig);  // should be a no-op
         const uint64 BtoH = B_BENDIAN_TO_HOST_INT64(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoL, &LtoH, &HtoB, &BtoH, "64-bit swap macro does not work!");
      }

      {
         const float orig  = -1234567.89012345f;
         const uint32 HtoL = B_HOST_TO_LENDIAN_IFLOAT(orig);
         const float  LtoH = B_LENDIAN_TO_HOST_IFLOAT(HtoL);
         const uint32 HtoB = B_HOST_TO_BENDIAN_IFLOAT(orig);  // should be a no-op
         const float  BtoH = B_BENDIAN_TO_HOST_IFLOAT(HtoB);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoL, NULL, &HtoB, &BtoH, "float swap macro does not work!");
         CheckOp(sizeof(orig), &orig, NULL,  NULL, &LtoH,  NULL, "float swap macro does not work!");
      }

      {
         const double orig = ((double)-1234567.89012345) * ((double)987654321.0987654321);
         const uint64 HtoL = B_HOST_TO_LENDIAN_IDOUBLE(orig);
         const double LtoH = B_LENDIAN_TO_HOST_IDOUBLE(HtoL);
         const uint64 HtoB = B_HOST_TO_BENDIAN_IDOUBLE(orig);  // should be a no-op
         const double BtoH = B_BENDIAN_TO_HOST_IDOUBLE(HtoB);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoL, NULL, &HtoB, &BtoH, "double swap macro does not work!");
         CheckOp(sizeof(orig), &orig,  NULL, NULL, &LtoH,  NULL, "double swap macro does not work!");
      }
   }
#else
   if (testsLittleEndian)
   {
      {
         const uint16 orig = 0x1234;
         const uint16 HtoB = B_HOST_TO_BENDIAN_INT16(orig);
         const uint16 BtoH = B_BENDIAN_TO_HOST_INT16(orig);
         const uint16 HtoL = B_HOST_TO_LENDIAN_INT16(orig);  // should be a no-op
         const uint16 LtoH = B_LENDIAN_TO_HOST_INT16(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoB, &BtoH, &HtoL, &LtoH, "16-bit swap macro does not work!");
      }

      {
         const uint32 orig = 0x12345678;
         const uint32 HtoB = B_HOST_TO_BENDIAN_INT32(orig);
         const uint32 BtoH = B_BENDIAN_TO_HOST_INT32(orig);
         const uint32 HtoL = B_HOST_TO_LENDIAN_INT32(orig);  // should be a no-op
         const uint32 LtoH = B_LENDIAN_TO_HOST_INT32(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoB, &BtoH, &HtoL, &LtoH, "32-bit swap macro does not work!");
      }

      {
         const uint64 orig = (((uint64)0x12345678)<<32)|(((uint64)0x12312312));
         const uint64 HtoB = B_HOST_TO_BENDIAN_INT64(orig);
         const uint64 BtoH = B_BENDIAN_TO_HOST_INT64(orig);
         const uint64 HtoL = B_HOST_TO_LENDIAN_INT64(orig);  // should be a no-op
         const uint64 LtoH = B_LENDIAN_TO_HOST_INT64(orig);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoB, &BtoH, &HtoL, &LtoH, "64-bit swap macro does not work!");
      }

      {
         const float orig  = -1234567.89012345f;
         const uint32 HtoB = B_HOST_TO_BENDIAN_IFLOAT(orig);
         const float  BtoH = B_BENDIAN_TO_HOST_IFLOAT(HtoB);
         const uint32 HtoL = B_HOST_TO_LENDIAN_IFLOAT(orig);  // should be a no-op
         const float  LtoH = B_LENDIAN_TO_HOST_IFLOAT(HtoL);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoB, NULL, &HtoL, &LtoH, "float swap macro does not work!");
         CheckOp(sizeof(orig), &orig,  NULL, NULL, &BtoH,  NULL, "float swap macro does not work!");
      }

      {
         const double orig = ((double)-1234567.89012345) * ((double)987654321.0987654321);
         const uint64 HtoB = B_HOST_TO_BENDIAN_IDOUBLE(orig);
         const double BtoH = B_BENDIAN_TO_HOST_IDOUBLE(HtoB);
         const uint64 HtoL = B_HOST_TO_LENDIAN_IDOUBLE(orig);  // should be a no-op
         const double LtoH = B_LENDIAN_TO_HOST_IDOUBLE(HtoL);  // should be a no-op
         CheckOp(sizeof(orig), &orig, &HtoB, NULL, &HtoL, &LtoH, "double swap macro does not work!");
         CheckOp(sizeof(orig), &orig,  NULL, NULL, &BtoH,  NULL, "double swap macro does not work!");
      }
   }
   else GoInsane("MUSCLE is compiled for a little-endian CPU, but host CPU is big-endian!?");
#endif

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   // Just because I'm paranoid and want to make sure that std::is_trivial() works the way I think it does --jaf
   VerifyTypeIsTrivial<int>();
   VerifyTypeIsTrivial<float>();
   VerifyTypeIsTrivial<double>();
   VerifyTypeIsTrivial<int8>();
   VerifyTypeIsTrivial<int16>();
   VerifyTypeIsTrivial<int32>();
   VerifyTypeIsTrivial<int64>();
   VerifyTypeIsTrivial<const char *>();
   VerifyTypeIsTrivial<const String *>();
   VerifyTypeIsTrivial<String *>();

   VerifyTypeIsNonTrivial<Point>();
   VerifyTypeIsNonTrivial<Rect>();
   VerifyTypeIsNonTrivial<String>();
   VerifyTypeIsNonTrivial<ByteBuffer>();
   VerifyTypeIsNonTrivial<ByteBufferRef>();
   VerifyTypeIsNonTrivial<DataIO>();
   VerifyTypeIsNonTrivial<DataIORef>();
   VerifyTypeIsNonTrivial<Socket>();
   VerifyTypeIsNonTrivial<ConstSocketRef>();
#endif

   // Make sure our pointer-size matches the defined/not-defined state of our MUSCLE_64_BIT_PLATFORM define
#ifdef MUSCLE_64_BIT_PLATFORM
   if (sizeof(void *) != 8) GoInsane("MUSCLE_64_BIT_PLATFORM is defined, but sizeof(void*) is not 8!");
#else
   if (sizeof(void *) != 4) GoInsane("MUSCLE_64_BIT_PLATFORM is not defined, and sizeof(void*) is not 4!");
#endif
}

SanitySetupSystem :: ~SanitySetupSystem()
{
#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
   PrintAndClearStringCopyCounts(stdout, "At end of main()");
#endif
}

#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
void PrintAndClearStringCopyCounts(const OutputPrinter & p, const char * optDesc)
{
   const uint32 * s = _stringOpCounts;  // just to save chars
   const uint32 totalCopies = s[STRING_OP_COPY_CTOR] + s[STRING_OP_PARTIAL_COPY_CTOR] + s[STRING_OP_SET_FROM_STRING];
   const uint32 totalMoves  = s[STRING_OP_MOVE_CTOR] + s[STRING_OP_MOVE_FROM_STRING];

   p.printf("String Op Counts [%s]\n", optDesc?optDesc:"Untitled");
   p.printf("# Default Ctors = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_DEFAULT_CTOR]);
   p.printf("# Cstr Ctors    = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_CSTR_CTOR]);
   p.printf("# Copy Ctors    = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_COPY_CTOR]);
   p.printf("# PtCopy Ctors  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_PARTIAL_COPY_CTOR]);
   p.printf("# PACopy Ctors  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_PREALLOC_COPY_CTOR]);
#ifdef __APPLE__
   p.printf("# CFCopy Ctors  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_CFSTR_COPY_CTOR]);
#endif
   p.printf("# Set from Cstr = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_SET_FROM_CSTR]);
   p.printf("# Set from Str  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_SET_FROM_STRING]);
   p.printf("# Move Ctor     = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_MOVE_CTOR]);
   p.printf("# Move from Str = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_MOVE_FROM_STRING]);
   p.printf("# Dtors         = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_DTOR]);
   p.printf("-----------------------------\n");
   p.printf("# Total Copies  = " UINT32_FORMAT_SPEC "\n", totalCopies);
   p.printf("# Total Moves   = " UINT32_FORMAT_SPEC "\n", totalMoves);
   p.printf("# Total Either  = " UINT32_FORMAT_SPEC "\n", totalCopies+totalMoves);
   p.printf("\n");

   for (uint32 i=0; i<NUM_STRING_OPS; i++) _stringOpCounts[i] = 0;  // reset counts for next time
}
#endif

MathSetupSystem :: MathSetupSystem()
{
#if defined(__BORLANDC__)
   _control87(MCW_EM,MCW_EM);  // disable floating point exceptions
#endif
}

MathSetupSystem :: ~MathSetupSystem()
{
   // empty
}

#if defined(WIN32)
static Mutex _rtMutex;  // used for serializing access inside GetRunTime64Aux(), if necessary
# if defined(MUSCLE_USE_QUERYPERFORMANCECOUNTER)
static uint64 _qpcTicksPerSecond = 0;
# endif
#elif defined(__APPLE__)
static mach_timebase_info_data_t _machTimebase = {0,0};
#elif defined(MUSCLE_USE_LIBRT) && defined(_POSIX_MONOTONIC_CLOCK)
   // empty
#else
static Mutex _rtMutex;  // used for serializing access inside GetRunTime64Aux(), if necessary
static clock_t _posixTicksPerSecond = 0;
#endif

static void InitClockFrequency()
{
#if defined(WIN32)
# if defined(MUSCLE_USE_QUERYPERFORMANCECOUNTER)
   LARGE_INTEGER tps;
   if (QueryPerformanceFrequency(&tps)) _qpcTicksPerSecond = tps.QuadPart;
                                   else MCRASH("QueryPerformanceFrequency() failed!");
# endif
#elif defined(__APPLE__)
   if ((mach_timebase_info(&_machTimebase) != KERN_SUCCESS)||(_machTimebase.denom <= 0)) MCRASH("mach_timebase_info() failed!");
#elif defined(MUSCLE_USE_LIBRT) && defined(_POSIX_MONOTONIC_CLOCK)
   // empty
#else
   _posixTicksPerSecond = sysconf(_SC_CLK_TCK);
   if (_posixTicksPerSecond < 0) MCRASH("sysconf(_SC_CLK_TCK) failed!");
#endif
}

TimeSetupSystem :: TimeSetupSystem()
{
   InitClockFrequency();
}

TimeSetupSystem :: ~TimeSetupSystem()
{
   // empty
}

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER

static String LockSequenceToString(const Queue<const void *> & seq)
{
   String ret;
   for (uint32 i=0; i<seq.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += ',';
      ret += String("%1").Arg(seq[i]);
   }
   return ret;
}

/** Records useful information about the locking of a particular mutex */
class MutexLockRecord
{
public:
   MutexLockRecord() {Set(NULL, NULL, 0, NUM_LOCK_ACTIONS);}
   MutexLockRecord(const void * mutexPtr, const char * fileName, uint32 fileLine, uint32 lockActionType) {Set(mutexPtr, fileName, fileLine, lockActionType);}

   void Set(const void * mutexPtr, const char * fileName, uint32 fileLine, uint32 lockActionType)
   {
      _fileLine       = fileLine;
      _mutexPtr       = mutexPtr;
      _lockActionType = lockActionType;

      if (fileName)
      {
         const char * lastSlash = strrchr(fileName, '/');
         if (lastSlash) fileName = lastSlash+1;

         lastSlash = strrchr(fileName, '\\');
         if (lastSlash) fileName = lastSlash+1;

         _fileName = fileName;
      }
      else _fileName = "<unspecified>";

#ifdef MUSCLE_USE_BACKTRACE
      if ((_mutexPtr)&&(GetMaxLogLevel() >= MUSCLE_LOG_DEBUG))
      {
         const int btRet = backtrace(_backtraceSymbols, ARRAYITEMS(_backtraceSymbols));
         if (btRet < 0) perror("backtrace");
                   else _numValidBacktraceSymbols = (uint32) btRet;
      }
      else _numValidBacktraceSymbols = 0;
#endif
   }

   bool operator == (const MutexLockRecord & rhs) const {return ((_fileLine == rhs._fileLine)&&(_mutexPtr == rhs._mutexPtr)&&(_lockActionType == rhs._lockActionType)&&(strcmp(_fileName, rhs._fileName)==0));}
   bool operator != (const MutexLockRecord & rhs) const {return !(*this==rhs);}

   uint32 HashCode() const
   {
      return CalculateHashCode(_fileLine) + GetLockActionType() + CalculateHashCode(_mutexPtr) + CalculateHashCode(_fileName, strlen(_fileName));
   }

   uint32 GetFileLine()           const {return _fileLine;}
   uint32 GetLockActionType()     const {return _lockActionType;}
   const void * GetMutexPointer() const {return _mutexPtr;}
   const char * GetFileName()     const {return _fileName;}

   String ToString() const {return String("mutex %1 @ %2:%3 %4").Arg(_mutexPtr).Arg(_fileName).Arg(_fileLine).Arg(GetLockActionTypeString(_lockActionType));}

   String GetStackTrace() const
   {
#ifdef MUSCLE_USE_BACKTRACE
      if (_numValidBacktraceSymbols > 0)
      {
         char ** strings = backtrace_symbols(_backtraceSymbols, _numValidBacktraceSymbols);
         if (strings)
         {
            String ret;
            for (uint32 i=3; i<_numValidBacktraceSymbols; i++)  // starting at #3 because the first three functions are just deadlock-recorder functions
            {
               if (ret.HasChars()) ret += " <- ";

               StringTokenizer tok(strings[i], " ");
               (void) tok(); // frame index
               (void) tok(); // library name
               (void) tok(); // hex address
               const char * fName = tok();
               if (fName)
               {
                  if (strstr(fName, "__thread_proxy") != NULL)
                  {
                     ret += "<pthread_entry>";  // because the pthreads scaffolding makes my eyes bleed
                     break;
                  }
                  else ret += GetUnmangledSymbolName(fName);
               }
               else ret += String("???");

            }
            free(strings);
            return ret;
         }
         else return "<backtrace_symbols() failed>";
      }
      else return "<No backtrace present>";
#else
      return GetEmptyString();
#endif
   }

private:
   static const char * GetLockActionTypeString(uint32 lockActionType)
   {
      static const char * _strs[] = {"Unlock", "UnlockShared", "Lock", "LockShared", "TryLock", "TryLockShared"};
      MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_strs, NUM_LOCK_ACTIONS);
      return (lockActionType < ARRAYITEMS(_strs)) ? _strs[lockActionType] : "???";
   }

   const void * _mutexPtr;
   const char * _fileName;
   uint32 _fileLine;
   uint32 _lockActionType;

#ifdef MUSCLE_USE_BACKTRACE
   void * _backtraceSymbols[32];
   uint32 _numValidBacktraceSymbols;
#endif
};

/** Gotta do a custom data structure because we can't use the standard new/delete/muscleAlloc()/muscleFree() memory operators,
  * because to do so would cause an infinite regress when they call through to Mutex::Lock() or Mutex::Unlock()
  */
class MutexLockRecordLog
{
public:
   MutexLockRecordLog(const muscle_thread_id & id)
      : _threadID(id)
      , _headMLSRecord(NULL)
      , _tailMLSRecord(NULL)
      , _numHeldLocks(0)
   {
      // empty
   }

   void AddEvent(uint32 lockActionType, const void * mutexPtr, const char * fileName, int fileLine)
   {
      switch(lockActionType)
      {
         case LOCK_ACTION_UNLOCK_EXCLUSIVE:
         case LOCK_ACTION_UNLOCK_SHARED:
         {
            // Remove the most recent instance of (mutexPtr) from our _heldLocks array
            const int32 mostRecentLockIdx = FindMostRecentInstanceOfHeldLock(mutexPtr);
            if (mostRecentLockIdx >= 0) RemoveHeldLockInstanceAt(mostRecentLockIdx);
            else
            {
               char tempBuf[20];
               printf("MutexLockRecordLog ERROR:  Thread [%s] is trying to unlock unacquired mutex %p at [%s:%i]!\n", _threadID.ToString(tempBuf), mutexPtr, fileName, fileLine);
            }
         }
         break;

         case LOCK_ACTION_LOCK_EXCLUSIVE:
         case LOCK_ACTION_LOCK_SHARED:
         case LOCK_ACTION_TRYLOCK_EXCLUSIVE:
         case LOCK_ACTION_TRYLOCK_SHARED:
         {
            if (_numHeldLocks >= ARRAYITEMS(_heldLocks))
            {
               char tempBuf[20];
               printf("MutexLockRecordLog ERROR: Maximum simultaneous-held-locks (" UINT32_FORMAT_SPEC ") reached by thread [%s] trying to lock mutex %p at [%s:%i]!\n", ARRAYITEMS(_heldLocks), _threadID.ToString(tempBuf), mutexPtr, fileName, fileLine);
            }
            else
            {
               _heldLocks[_numHeldLocks++].Set(mutexPtr, fileName, fileLine, lockActionType);

               if ((ContainsSequence(_heldLocks, _numHeldLocks) == false)&&((_tailMLSRecord == NULL)||(_tailMLSRecord->AddLockSequence(_heldLocks, _numHeldLocks).IsError())))
               {
                  MutexLockSequencesRecord * newBuf = (MutexLockSequencesRecord *) malloc(sizeof(MutexLockSequencesRecord));  // Gotta use malloc() here instead of new to avoid any potential GlobalMemoryAllocator callbacks, etc
                  if (newBuf)
                  {
                     MutexLockSequencesRecord * newMLSRecord = new (newBuf) MutexLockSequencesRecord;  // placement-new is required to construct the MutexLockSequencesRecord on top of the memory-buffer we allocated.
                     if (_headMLSRecord == NULL) _headMLSRecord = newMLSRecord;
                     if (_tailMLSRecord) _tailMLSRecord->_nextMLSRecord = newMLSRecord;
                     _tailMLSRecord = newMLSRecord;

                     status_t ret;
                     if (_tailMLSRecord->AddLockSequence(_heldLocks, _numHeldLocks).IsError(ret)) printf("MutexLockRecordLog ERROR:  AddLockSequence() failed [%s] for sequence of " UINT32_FORMAT_SPEC " locks\n", ret(), _numHeldLocks);
                  }
                  else
                  {
                     printf("MutexLockRecordLog ERROR:  malloc() of new MutexLockSequencesRecord buffer failed!\n");   // what else to do?  Even MWARN_OUT_OF_MEMORY isn't safe here
                     return;
                  }
               }
            }
         }
         break;

         default:
            printf("MutexLockRecordLog ERROR:  Unknown lockActionType " UINT32_FORMAT_SPEC " for mutex %p at [%s:%i]\n", lockActionType, mutexPtr, fileName, fileLine);
         break;
      }
   }

   void CaptureResults(Hashtable< Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > & capturedResults) const
   {
      MutexLockSequencesRecord * meb = _headMLSRecord;
      while(meb)
      {
         meb->CaptureResults(_threadID, capturedResults);
         meb = meb->_nextMLSRecord;
      }
   }

private:
   class MutexLockSequencesRecord
   {
   public:
      MutexLockSequencesRecord()
         : _nextMLSRecord(NULL)
         , _numValidRecords(0)
         , _numValidSequenceStartIndices(0)
      {
         // empty
      }

      status_t AddLockSequence(const MutexLockRecord * lockSequence, uint32 seqLen)
      {
         if (seqLen <= 1) return B_NO_ERROR;  // empty sequences and sequences containing just one mutex can't possibly cause a deadlock, so there's no need to store them
         if (((_numValidSequenceStartIndices+2) > ARRAYITEMS(_sequenceStartIndices))||((_numValidRecords + seqLen) > ARRAYITEMS(_events))) return B_OUT_OF_MEMORY;  // not enough room to add this sequence!

         _sequenceStartIndices[_numValidSequenceStartIndices++] = _numValidRecords;  // where our sequence starts at
         for (uint32 i=0; i<seqLen; i++) _events[_numValidRecords++] = lockSequence[i];
         _sequenceStartIndices[_numValidSequenceStartIndices]   = _numValidRecords;  // pre-write where the next sequence will start at

         return B_NO_ERROR;
      }

      void CaptureResults(muscle_thread_id threadID, Hashtable< Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > & capturedResults) const
      {
         Queue<const void *> q;
         Queue<MutexLockRecord> details;
         for (uint32 i=0; i<_numValidSequenceStartIndices; i++)
         {
            const uint32 sequenceStartIdx      = _sequenceStartIndices[i];
            const uint32 afterSequenceEndIndex = _sequenceStartIndices[i+1];  // yes, this will always be valid!  We pre-write it in AddLockSequence()
            const uint32 seqLen                = afterSequenceEndIndex-sequenceStartIdx;

            q.Clear();
            details.Clear();

            if ((q.EnsureSize(seqLen).IsOK())&&(details.EnsureSize(seqLen).IsOK()))
            {
               uint32 numTrailingTryLocks = 0;
               for (uint32 j=sequenceStartIdx; j<afterSequenceEndIndex; j++)
               {
                  const MutexLockRecord & mlr = _events[j];
                  const uint32 lat = mlr.GetLockActionType();
                  if ((lat != LOCK_ACTION_UNLOCK_EXCLUSIVE)&&(lat != LOCK_ACTION_UNLOCK_SHARED))
                  {
                     (void) q.AddTail(mlr.GetMutexPointer());  // guaranteed not to fail
                     (void) details.AddTail(mlr);              // guaranteed not to fail
                     if ((lat == LOCK_ACTION_TRYLOCK_EXCLUSIVE)||(lat == LOCK_ACTION_TRYLOCK_SHARED)) numTrailingTryLocks++;
                                                                                                 else numTrailingTryLocks = 0;
                  }
               }

               (void) q.RemoveTailMulti(numTrailingTryLocks);  // Trylocks at the end of the sequence don't count, as they won't block
               RemoveDuplicateItemsFromSequence(q);

               Hashtable<muscle_thread_id, Queue<MutexLockRecord> > * tab = (q.GetNumItems() > 1) ? capturedResults.GetOrPut(std_move_if_available(q)) : NULL;
               if (tab)
               {
                  const Queue<MutexLockRecord> * oldDetails = tab->Get(threadID);
                  if ((oldDetails == NULL)||(details.GetNumItems() < oldDetails->GetNumItems())) (void) tab->Put(threadID, details);
               }
            }
         }
      }

      // Gotta remove all the duplicates while keeping the first instances of each pointer in the same order as before
      void RemoveDuplicateItemsFromSequence(Queue<const void *> & seq) const
      {
         Hashtable<const void *, uint32> histogram(PreallocatedItemSlotsCount(seq.GetNumItems()));
         for (uint32 i=0; i<seq.GetNumItems(); i++)
         {
            uint32 * count = histogram.GetOrPut(seq[i]);
            if (count) (*count)++;
         }
         for (HashtableIterator<const void *, uint32> iter(histogram); iter.HasData(); iter++)
         {
            const void * deDupMe = iter.GetKey();
            if (iter.GetValue() > 1)
            {
               bool foundFirst = false;
               for (uint32 i=0; i<seq.GetNumItems(); i++)
               {
                  if (seq[i] == deDupMe)
                  {
                     if (foundFirst) (void) seq.RemoveItemAt(i--);
                                else foundFirst = true;
                  }
               }
            }
         }
      }

      bool ContainsSequence(const MutexLockRecord * lockSequence, uint32 seqLen) const
      {
         if (seqLen <= 1) return true;  // we don't store empty sequences or sequences containing only one Mutex, so there's no need to look for them; we'll just pretend they are present
         for (int32 i=_numValidSequenceStartIndices-1; i>=0; i--)  // backwards for better locality
         {
            const uint32 sequenceStartIdx    = _sequenceStartIndices[i];
            const uint32 afterSequenceEndIdx = _sequenceStartIndices[i+1];  // yes, this will always be valid!  We pre-write it in AddLockSequence()
            if (SequencesMatch(lockSequence, seqLen, &_events[sequenceStartIdx], afterSequenceEndIdx-sequenceStartIdx)) return true;
         }
         return false;
      }

   private:
      friend class MutexLockRecordLog;

      bool SequencesMatch(const MutexLockRecord * s1, uint32 s1Len, const MutexLockRecord * s2, uint32 s2Len) const
      {
         if (s1Len != s2Len) return false;
         for (uint32 i=0; i<s1Len; i++) if (s1[i] != s2[i]) return false;
         return true;
      }

      MutexLockSequencesRecord * _nextMLSRecord;

      enum {LOCK_RECORDS_PER_SEQUENCE_RECORD = 4096};

      MutexLockRecord _events[LOCK_RECORDS_PER_SEQUENCE_RECORD];
      uint32 _numValidRecords;

      uint16 _sequenceStartIndices[LOCK_RECORDS_PER_SEQUENCE_RECORD];
      uint32 _numValidSequenceStartIndices;
   };

   int32 FindMostRecentInstanceOfHeldLock(const void * mutexPtr) const
   {
      for (int32 i=_numHeldLocks-1; i>=0; i--) if (_heldLocks[i].GetMutexPointer() == mutexPtr) return i;
      return -1;
   }

   void RemoveHeldLockInstanceAt(uint32 idx)
   {
      for (uint32 i=idx; (i+1)<_numHeldLocks; i++) _heldLocks[i] = _heldLocks[i+1];
      _heldLocks[--_numHeldLocks].Set(NULL, NULL, 0, NUM_LOCK_ACTIONS);  // paranoia
   }

   bool ContainsSequence(const MutexLockRecord * lockSequence, uint32 seqLen) const
   {
      if (seqLen <= 1) return true;  // we don't store empty sequences or sequences containing only one Mutex, so there's no need to look for them; we'll just pretend they are present
      if ((_tailMLSRecord)&&(_tailMLSRecord->ContainsSequence(lockSequence, seqLen))) return true;  // check the most recent record first, because locality
      const MutexLockSequencesRecord * r = _headMLSRecord;  // if it wasn't there, then we'll check the others too
      while((r)&&(r != _tailMLSRecord))
      {
         if (r->ContainsSequence(lockSequence, seqLen)) return true;
         r = r->_nextMLSRecord;
      }
      return false;
   }

   muscle_thread_id _threadID;
   MutexLockSequencesRecord * _headMLSRecord;
   MutexLockSequencesRecord * _tailMLSRecord;

   enum {MAX_SIMULTANEOUS_HELD_LOCKS=256};  // if your code is holding more locks than this many locks at once, you've got bigger problems than I can solve :)
   MutexLockRecord _heldLocks[MAX_SIMULTANEOUS_HELD_LOCKS];
   uint32 _numHeldLocks;
};

static ThreadLocalStorage<MutexLockRecordLog> _mutexEventsLog(false);  // false argument is necessary otherwise we can't read the threads' logs after they've gone away!
static Mutex _mutexLogTableMutex;
static Hashtable<muscle_thread_id, MutexLockRecordLog *> _mutexLogTable(PreallocatedItemSlotsCount(4096));  // preallocated here to avoid a memory-allocation inside DeadlockFinder_AddEvent()

void DeadlockFinder_LogEvent(uint32 lockActionType, const void * mutexPtr, const char * fileName, int fileLine)
{
   MutexLockRecordLog * mel = _mutexEventsLog.GetThreadLocalObject();
   if (mel == NULL)
   {
      MutexLockRecordLog * newBuf = (MutexLockRecordLog *) malloc(sizeof(MutexLockRecordLog));  // MUST CALL malloc() here to avoid inappropriate re-entrancy!
      if (newBuf)
      {
         const muscle_thread_id tid = muscle_thread_id::GetCurrentThreadID();

         mel = new (newBuf) MutexLockRecordLog(tid);
         (void) _mutexEventsLog.SetThreadLocalObject(mel);

         DECLARE_MUTEXGUARD(_mutexLogTableMutex);
         (void) _mutexLogTable.Put(tid, mel);
      }
   }
   if (mel) mel->AddEvent(lockActionType, mutexPtr, fileName, fileLine);
       else printf("DeadlockFinder_LogEvent:  malloc of MutexLockRecordLog failed!?\n");  // we can't even call MWARN_OUT_OF_MEMORY here
}

static String ThreadsListToString(const Queue<muscle_thread_id> & threadsList)
{
   String ret;
   for (uint32 i=0; i<threadsList.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += ',';

      char buf[20];
      ret += threadsList[i].ToString(buf);
   }
   return ret;
}

// Returns true iff (seqA)'s locking-order is inconsistent with (seqB)'s locking-order
static bool SequencesAreInconsistent(const Queue<const void *> & seqA, const Queue<const void *> & seqB)
{
   const Queue<const void *> & largerQ  = (seqA.GetNumItems()  > seqB.GetNumItems()) ? seqA : seqB;
   const Queue<const void *> & smallerQ = (seqA.GetNumItems() <= seqB.GetNumItems()) ? seqA : seqB;

   for (uint32 i=0; i<largerQ.GetNumItems(); i++)
      for (uint32 j=0; ((j<i)&&(j<largerQ.GetNumItems())); j++)
      {
         const int32 smallerIPos = smallerQ.IndexOf(largerQ[i]);
         const int32 smallerJPos = smallerQ.IndexOf(largerQ[j]);
         if ((smallerIPos >= 0)&&(smallerJPos >= 0)&&((i<j) != (smallerIPos < smallerJPos))) return true;
      }

   return false;
}

// Returns true iff at least one details-string isn't a shared-string
static bool SequenceHasExclusiveLock(const Hashtable<muscle_thread_id, Queue<MutexLockRecord> > & table)
{
   for (HashtableIterator<muscle_thread_id, Queue<MutexLockRecord> > iter(table); iter.HasData(); iter++)
   {
      const Queue<MutexLockRecord> & q = iter.GetValue();
      for (uint32 i=0; i<q.GetNumItems(); i++)
      {
         switch(q[i].GetLockActionType())
         {
            case LOCK_ACTION_UNLOCK_EXCLUSIVE:
            case LOCK_ACTION_LOCK_EXCLUSIVE:
            case LOCK_ACTION_TRYLOCK_EXCLUSIVE:
               return true;

            default:
               // empty
            break;
         }
      }
   }
   return false;
}

static void PrintSequenceReport(const OutputPrinter & p, const char * desc, const Queue<const void *> & seq, const Hashtable<muscle_thread_id, Queue<MutexLockRecord> > & detailsTable)
{
   p.printf("  %s: [%s] was executed by " UINT32_FORMAT_SPEC " threads:\n", desc, LockSequenceToString(seq)(), detailsTable.GetNumItems());

   Hashtable<Queue<MutexLockRecord>, Queue<muscle_thread_id> > detailsToThreads;
   for (HashtableIterator<muscle_thread_id, Queue<MutexLockRecord> > iter(detailsTable); iter.HasData(); iter++) (void) detailsToThreads.GetOrPut(iter.GetValue())->AddTailIfNotAlreadyPresent(iter.GetKey());

   for (HashtableIterator<Queue<MutexLockRecord>, Queue<muscle_thread_id> > iter(detailsToThreads); iter.HasData(); iter++)
   {
      Queue<muscle_thread_id> & threadsList = iter.GetValue();
      threadsList.Sort();

      p.printf("    Thread%s [%s] locked mutexes in this order:\n", (threadsList.GetNumItems()==1)?"s":"", ThreadsListToString(threadsList)());
      const Queue<MutexLockRecord> & details = iter.GetKey();

#ifdef MUSCLE_USE_BACKTRACE
      const bool printStackTraces = (GetMaxLogLevel() >= MUSCLE_LOG_DEBUG);
#endif
      for (uint32 i=0; i<details.GetNumItems(); i++)
      {
         p.printf("       " UINT32_FORMAT_SPEC ": %s", i, details[i].ToString()());
         if (printStackTraces) p.printf(" [%s]\n", details[i].GetStackTrace()());
                          else p.printf("\n");
      }
   }
}

#endif

status_t PrintMutexLockingReport(const OutputPrinter & p)
{
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   Hashtable< Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > capturedResults;  // mutex-sequence -> (threadID -> details)
   {
      DECLARE_MUTEXGUARD(_mutexLogTableMutex);
      for (HashtableIterator<muscle_thread_id, MutexLockRecordLog *> iter(_mutexLogTable); iter.HasData(); iter++) iter.GetValue()->CaptureResults(capturedResults);
   }

   if (p.GetAddToString() == NULL) p.printf("\n");
   p.printf("------------------- " UINT32_FORMAT_SPEC " UNIQUE LOCK SEQUENCES DETECTED -----------------\n", capturedResults.GetNumItems());
   for (HashtableIterator< Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > iter(capturedResults); iter.HasData(); iter++)
      p.printf("LockSequence [%s] was executed by " UINT32_FORMAT_SPEC " threads\n", LockSequenceToString(iter.GetKey())(), iter.GetValue().GetNumItems());

   // Now we check for inconsistent locking order.  Two sequences are inconsistent with each other if they lock the same two mutexes
   // but lock them in a different order, and at least one lock-action in the sequence is exclusive.
   Hashtable<uint32, uint32> inconsistentSequencePairs;  // smaller key-position -> larger key-position
   {
      uint32 idxA = 0;
      for (HashtableIterator<Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > iterA(capturedResults); iterA.HasData(); iterA++,idxA++)
      {
         const bool iterAHasExclusiveLock = SequenceHasExclusiveLock(iterA.GetValue());

         uint32 idxB = 0;
         for (HashtableIterator<Queue<const void *>, Hashtable<muscle_thread_id, Queue<MutexLockRecord> > > iterB(capturedResults); ((idxB < idxA)&&(iterB.HasData())); iterB++,idxB++)
            if (((iterAHasExclusiveLock)||(SequenceHasExclusiveLock(iterB.GetValue())))&&(SequencesAreInconsistent(iterA.GetKey(), iterB.GetKey())))
              MRETURN_ON_ERROR(inconsistentSequencePairs.Put(idxB, idxA));
      }
   }

   bool foundProblems = false;
   if (inconsistentSequencePairs.HasItems())
   {
      p.printf("\n");
      p.printf("--------- WARNING: " UINT32_FORMAT_SPEC " INCONSISTENT LOCK SEQUENCE%s DETECTED --------------\n", inconsistentSequencePairs.GetNumItems(), (inconsistentSequencePairs.GetNumItems()==1)?"":"S");
      uint32 idx = 0;
      for (HashtableIterator<uint32, uint32> iter(inconsistentSequencePairs); iter.HasData(); iter++,idx++)
      {
         p.printf("\n");
         p.printf("INCONSISTENT LOCKING ORDER REPORT #" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " --------\n", idx+1, inconsistentSequencePairs.GetNumItems());
         const Queue<const void *> & seqA = *capturedResults.GetKeyAt(iter.GetKey());
         const Queue<const void *> & seqB = *capturedResults.GetKeyAt(iter.GetValue());
         PrintSequenceReport(p, "SequenceA", seqA, capturedResults[seqA]);
         PrintSequenceReport(p, "SequenceB", seqB, capturedResults[seqB]);
         foundProblems = true;
      }
   }

   if (foundProblems == false)
   {
      p.printf("\n");
      p.printf("No Mutex-acquisition ordering inconsistencies detected, yay!\n");
   }

   if (p.GetAddToString() == NULL) p.printf("\n\n");
#else
   p.printf("PrintMutexLockingReport:  MUSCLE_ENABLE_DEADLOCK_FINDER wasn't specified during compilation, so no locking-logs were recorded.\n");
#endif

   return B_NO_ERROR;
}

ThreadSetupSystem :: ThreadSetupSystem(bool muscleSingleThreadOnly)
{
   if (++_threadSetupCount == 1)
   {
#ifdef MUSCLE_SINGLE_THREAD_ONLY
      (void) muscleSingleThreadOnly;  // shut the compiler up
#else
      _mainThreadID = muscle_thread_id::GetCurrentThreadID();
      _muscleSingleThreadOnly = muscleSingleThreadOnly;
      if (_muscleSingleThreadOnly) _lock.Neuter();  // if we're single-thread, then this Mutex can be a no-op!
#endif
      _muscleLock = &_lock;

#if defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
      _muscleAtomicMutexes = newnothrow_array(Mutex, MUSCLE_MUTEX_POOL_SIZE);
      MASSERT(_muscleAtomicMutexes, "Could not allocate atomic mutexes!");
#endif
   }
   else if (IsCurrentThreadMainThread() == false) MCRASH("All SetupSystem objects should be declared in the same thread");
}

ThreadSetupSystem :: ~ThreadSetupSystem()
{
   if (--_threadSetupCount == 0)
   {
#if defined(MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS)
      delete [] _muscleAtomicMutexes; _muscleAtomicMutexes = NULL;
#endif
      _muscleLock = NULL;

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      (void) PrintMutexLockingReport(stdout);
#endif
   }
}

static uint32 _networkSetupCount = 0;

#if defined(MUSCLE_ENABLE_SSL) && !defined(MUSCLE_SINGLE_THREAD_ONLY)

// OpenSSL thread-safety-callback setup code provided by Tosha at
// http://stackoverflow.com/questions/3417706/openssl-and-multi-threads/12810000#12810000
# if defined(WIN32)
#  define OPENSSL_MUTEX_TYPE       HANDLE
#  define OPENSSL_MUTEX_SETUP(x)   (x) = CreateMutex(NULL, FALSE, NULL)
#  define OPENSSL_MUTEX_CLEANUP(x) CloseHandle(x)
#  define OPENSSL_MUTEX_LOCK(x)    WaitForSingleObject((x), INFINITE)
#  define OPENSSL_MUTEX_UNLOCK(x)  ReleaseMutex(x)
#  define OPENSSL_THREAD_ID        GetCurrentThreadId()
# else
#  define OPENSSL_MUTEX_TYPE       pthread_mutex_t
#  define OPENSSL_MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#  define OPENSSL_MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#  define OPENSSL_MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#  define OPENSSL_MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#  define OPENSSL_THREAD_ID        pthread_self()
# endif

/* This array will store all of the mutexes available to OpenSSL. */
static OPENSSL_MUTEX_TYPE *mutex_buf=NULL;

static void openssl_locking_function(int mode, int n, const char * file, int line)
{
   (void) file;
   (void) line;
   if (mode & CRYPTO_LOCK) OPENSSL_MUTEX_LOCK(mutex_buf[n]);
                      else OPENSSL_MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long openssl_id_function(void) {return ((unsigned long)OPENSSL_THREAD_ID);}

static int openssl_thread_setup(void)
{
   mutex_buf = (OPENSSL_MUTEX_TYPE *) malloc(CRYPTO_num_locks() * sizeof(OPENSSL_MUTEX_TYPE));
   if (!mutex_buf) return -1;

   for (int i=0;  i<CRYPTO_num_locks();  i++) OPENSSL_MUTEX_SETUP(mutex_buf[i]);

   CRYPTO_set_id_callback(openssl_id_function);
#ifndef _MSC_VER
   (void) openssl_id_function;       // just to avoid a compiler warning with newer OpenSSL versions where CRYPTO_set_id_callback() is a no-op macro
#endif

   CRYPTO_set_locking_callback(openssl_locking_function);
#ifndef _MSC_VER
   (void) openssl_locking_function;  // just to avoid a compiler warning with newer OpenSSL versions where CRYPTO_set_locking_callback() is a no-op macro
#endif

   return 0;
}

static int openssl_thread_cleanup(void)
{
   if (!mutex_buf) return -1;

   CRYPTO_set_id_callback(NULL);
   CRYPTO_set_locking_callback(NULL);

   for (int i=0;  i < CRYPTO_num_locks();  i++) OPENSSL_MUTEX_CLEANUP(mutex_buf[i]);

   free(mutex_buf);
   mutex_buf = NULL;

   return 0;
}

#endif

NetworkSetupSystem :: NetworkSetupSystem()
{
   if (++_networkSetupCount == 1)
   {
#ifdef WIN32
      WORD versionWanted = MAKEWORD(1, 1);
      WSADATA wsaData;
      if (WSAStartup(versionWanted, &wsaData) != 0) MCRASH("NetworkSetupSystem:  Could not initialize Winsock!");
#else
      struct sigaction sa;
      memset(&sa, 0, sizeof(sa));
      sa.sa_handler = SIG_IGN;
      if (sigaction(SIGPIPE, &sa, NULL) != 0) MCRASH("NetworkSetupSystem:  Could not ignore SIGPIPE signal!");
#endif

#ifdef MUSCLE_ENABLE_SSL
      SSL_load_error_strings();
      SSLeay_add_ssl_algorithms();
# if (OPENSSL_VERSION_MAJOR < 3)
      ERR_load_BIO_strings();
# endif
      SSL_library_init();
# ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (openssl_thread_setup() != 0) MCRASH("Error setting up thread-safety callbacks for OpenSSL!");
# endif
#endif
   }
}

NetworkSetupSystem :: ~NetworkSetupSystem()
{
   if (--_networkSetupCount == 0)
   {
#if defined(MUSCLE_ENABLE_SSL) && !defined(MUSCLE_SINGLE_THREAD_ONLY)
      (void) openssl_thread_cleanup();
#endif
#ifdef WIN32
      WSACleanup();
#endif
   }
}

#if defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY) && defined (MUSCLE_POWERPC_TIMEBASE_HZ)
static inline uint32 get_tbl() {uint32 tbl; asm volatile("mftb %0"  : "=r" (tbl) :); return tbl;}
static inline uint32 get_tbu() {uint32 tbu; asm volatile("mftbu %0" : "=r" (tbu) :); return tbu;}
#endif

/** Defined here since every MUSCLE program will have to include this file anyway... */
static uint64 GetRunTime64Aux()
{
#if defined(WIN32)
   uint64 ret = 0;
   DECLARE_MUTEXGUARD(_rtMutex);
# ifdef MUSCLE_USE_QUERYPERFORMANCECOUNTER
   if (_qpcTicksPerSecond == 0) InitClockFrequency();  // in case we got called before main()

   static int64 _brokenQPCOffset = 0;
   if (_brokenQPCOffset != 0) ret = (((uint64)timeGetTime())*1000) + _brokenQPCOffset;
   else
   {
      LARGE_INTEGER curTicks;
      if ((_qpcTicksPerSecond > 0)&&(QueryPerformanceCounter(&curTicks)))
      {
         const uint64 checkGetTime = ((uint64)timeGetTime())*1000;
         ret = (curTicks.QuadPart*MICROS_PER_SECOND)/_qpcTicksPerSecond;

         // Hack-around for evil Windows/hardware bug in QueryPerformanceCounter().
         // see http://support.microsoft.com/default.aspx?scid=kb;en-us;274323
         static uint64 _lastCheckGetTime = 0;
         static uint64 _lastCheckQPCTime = 0;
         if (_lastCheckGetTime > 0)
         {
            const uint64 getTimeElapsed = checkGetTime - _lastCheckGetTime;
            const uint64 qpcTimeElapsed = ret          - _lastCheckQPCTime;
            if ((muscleMax(getTimeElapsed, qpcTimeElapsed) - muscleMin(getTimeElapsed, qpcTimeElapsed)) > 500000)
            {
               //LogTime(MUSCLE_LOG_DEBUG, "QueryPerformanceCounter() is buggy, reverting to timeGetTime() method instead!\n");
               _brokenQPCOffset = (_lastCheckQPCTime-_lastCheckGetTime);
               ret = (((uint64)timeGetTime())*1000) + _brokenQPCOffset;
            }
         }
         _lastCheckGetTime = checkGetTime;
         _lastCheckQPCTime = ret;
      }
   }
# endif
   if (ret == 0)
   {
      static uint32 _prevVal    = 0;
      static uint64 _wrapOffset = 0;

      const uint32 newVal = (uint32) timeGetTime();
      if (newVal < _prevVal) _wrapOffset += (((uint64)1)<<32);
      ret = (_wrapOffset+newVal)*1000;  // convert to microseconds
      _prevVal = newVal;
   }
   return ret;
#elif defined(__APPLE__)
   if (_machTimebase.denom == 0) InitClockFrequency();  // in case we got called before main()
   return (uint64)((mach_absolute_time() * _machTimebase.numer) / (1000 * _machTimebase.denom));
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY) && defined(MUSCLE_POWERPC_TIMEBASE_HZ)
   while(1)
   {
      const uint32 hi1 = get_tbu();
      const uint32 low = get_tbl();
      const uint32 hi2 = get_tbu();
      if (hi1 == hi2)
      {
         // FogBugz #3199
         const uint64 cycles = ((((uint64)hi1)<<32)|((uint64)low));
         return ((cycles/MUSCLE_POWERPC_TIMEBASE_HZ)*MICROS_PER_SECOND)+(((cycles%MUSCLE_POWERPC_TIMEBASE_HZ)*(MICROS_PER_SECOND))/MUSCLE_POWERPC_TIMEBASE_HZ);
      }
   }
#elif defined(__EMSCRIPTEN__) || (defined(MUSCLE_USE_LIBRT) && defined(_POSIX_MONOTONIC_CLOCK))
# if defined(MUSCLE_USE_CLOCK_MONOTONIC_RAW)
   const clockid_t clkid = CLOCK_MONOTONIC_RAW;
# else
   const clockid_t clkid = CLOCK_MONOTONIC;
# endif
   struct timespec ts;
   return (clock_gettime(clkid, &ts) == 0) ? (SecondsToMicros(ts.tv_sec)+NanosToMicros(ts.tv_nsec)) : 0;
#else
   // default implementation:  use POSIX API
   if (_posixTicksPerSecond <= 0) InitClockFrequency();  // in case we got called before main()
   if (sizeof(clock_t) > sizeof(uint32))
   {
      // Easy case:  with a wide clock_t, we don't need to worry about it wrapping
      struct tms junk; clock_t newTicks = (clock_t) times(&junk);
      return ((((uint64)newTicks)*MICROS_PER_SECOND)/_posixTicksPerSecond);
   }
   else
   {
      // Oops, clock_t is skinny enough that it might wrap.  So we need to watch for that.
      DECLARE_MUTEXGUARD(_rtMutex);

      static uint32 _prevVal;
      static uint64 _wrapOffset = 0;

      struct tms junk; memset(&junk, 0, sizeof(junk));
      clock_t newTicks = (clock_t) times(&junk);
      const uint32 newVal = (uint32) newTicks;
      if (newVal < _prevVal) _wrapOffset += (((uint64)1)<<32);
      const uint64 ret = ((_wrapOffset+newVal)*MICROS_PER_SECOND)/_posixTicksPerSecond;  // convert to microseconds
      _prevVal = newTicks;

      return ret;
   }
#endif
}

static int64 _perProcessRunTimeOffset = 0;
uint64 GetRunTime64() {return GetRunTime64Aux()+_perProcessRunTimeOffset;}
void SetPerProcessRunTime64Offset(int64 offset) {_perProcessRunTimeOffset = offset;}
int64 GetPerProcessRunTime64Offset() {return _perProcessRunTimeOffset;}

status_t Snooze64(uint64 micros)
{
   // Hopefully this long-delay-chunk duration is long enough that waking up this often
   // won't be significant, but also short enough that we're unlikely to come across
   // any OS-specific sleep-API that can't handle it
   const uint64 longTimeMicros = DaysToMicros(1);
   while(micros > longTimeMicros)  // test must be strictly greater-than, or else we get infinite recursion
   {
      MRETURN_ON_ERROR(Snooze64(longTimeMicros));
      if (micros != MUSCLE_TIME_NEVER) micros -= longTimeMicros;
   }

#if WIN32
   Sleep((DWORD)((micros/1000)+(((micros%1000)!=0)?1:0)));
   return B_NO_ERROR;
#elif defined(__linux__)
   const struct timespec ts = {(time_t) MicrosToSeconds(micros), (time_t) MicrosToNanos(micros%MICROS_PER_SECOND)};
   return (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == 0) ? B_NO_ERROR : B_ERRNO;  // DO NOT specify CLOCK_MONOTONIC_RAW here, it won't work!
#elif defined(MUSCLE_USE_LIBRT)
   const struct timespec ts = {(time_t) MicrosToSeconds(micros), (long) MicrosToNanos(micros%MICROS_PER_SECOND)};
   return (nanosleep(&ts, NULL) == 0) ? B_NO_ERROR : B_ERRNO;
#else
   /** We can use select(), if nothing else */
   struct timeval waitTime;
   Convert64ToTimeVal(micros, waitTime);
   return (select(0, NULL, NULL, NULL, &waitTime) >= 0) ? B_NO_ERROR : B_ERRNO;
#endif
}

#ifdef WIN32
// Broken out so ParseHumanReadableTimeValues() can use it also
uint64 __Win32FileTimeToMuscleTime(const FILETIME & ft)
{
   union {
     uint64 ns100; /*time since 1 Jan 1601 in 100ns units */
     FILETIME ft;
   } theTime;
   theTime.ft = ft;

   static const uint64 TIME_DIFF = ((uint64)116444736)*NANOS_PER_SECOND;
   struct timeval tv;
   tv.tv_usec = (long)((theTime.ns100 / ((uint64)10)) % MICROS_PER_SECOND);
   tv.tv_sec  = (long)((theTime.ns100 - TIME_DIFF)    / (10*MICROS_PER_SECOND));
   return ConvertTimeValTo64(tv);
}
#endif

/** Defined here since every MUSCLE program will have to include this file anyway... */
uint64 GetCurrentTime64(uint32 timeType)
{
#ifdef WIN32
   FILETIME ft;
   GetSystemTimeAsFileTime(&ft);
   if (timeType == MUSCLE_TIMEZONE_LOCAL) (void) FileTimeToLocalFileTime(&ft, &ft);
   return __Win32FileTimeToMuscleTime(ft);
#else
   struct timeval tv;
   gettimeofday(&tv, NULL);
   uint64 ret = ConvertTimeValTo64(tv);
   if (timeType == MUSCLE_TIMEZONE_LOCAL)
   {
      time_t now = time(NULL);
      struct tm gmtm;
      struct tm * tm = gmtime_r(&now, &gmtm);
      if (tm)
      {
         ret += SecondsToMicros(now-mktime(tm));
         if (tm->tm_isdst>0) ret += HoursToMicros(1);  // FogBugz #4498
      }
   }
   return ret;
#endif
}

#if MUSCLE_TRACE_CHECKPOINTS > 0
static volatile uint32 _defaultTraceLocation[MUSCLE_TRACE_CHECKPOINTS];
volatile uint32 * _muscleTraceValues = _defaultTraceLocation;
uint32 _muscleNextTraceValueIndex = 0;

void SetTraceValuesLocation(volatile uint32 * location)
{
   _muscleTraceValues = location ? location : _defaultTraceLocation;
   _muscleNextTraceValueIndex = 0;
   for (uint32 i=0; i<MUSCLE_TRACE_CHECKPOINTS; i++) _muscleTraceValues[i] = 0;
}
#endif

static AbstractObjectRecycler * _firstRecycler = NULL;

AbstractObjectRecycler :: AbstractObjectRecycler()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   // Append us to the front of the linked list
   if (_firstRecycler) _firstRecycler->_prev = this;
   _prev = NULL;
   _next = _firstRecycler;
   _firstRecycler = this;

   if (m) (void) m->Unlock();
}

AbstractObjectRecycler :: ~AbstractObjectRecycler()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   // Remove us from the linked list
   if (_prev) _prev->_next = _next;
   if (_next) _next->_prev = _prev;
   if (_firstRecycler == this) _firstRecycler = _next;

   if (m) (void) m->Unlock();
}

void AbstractObjectRecycler :: GlobalFlushAllCachedObjects()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   // We restart at the head of the list anytime anything is flushed,
   // for safety.  When we get to the end of the list, everything has
   // been flushed.
   AbstractObjectRecycler * r = _firstRecycler;
   while(r) r = (r->FlushCachedObjects() > 0) ? _firstRecycler : r->_next;
   if (m) (void) m->Unlock();
}

void AbstractObjectRecycler :: GlobalPrintRecyclersToStream()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   const AbstractObjectRecycler * r = _firstRecycler;
   while(r)
   {
      r->Print(stdout);
      r = r->_next;
   }

   if (m) (void) m->Unlock();
}

void AbstractObjectRecycler :: GlobalPerformSanityCheck()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   const AbstractObjectRecycler * r = _firstRecycler;
   while(r)
   {
      r->PerformSanityCheck();
      r = r->_next;
   }

   if (m) (void) m->Unlock();
}

static CompleteSetupSystem * _activeCSS = NULL;
CompleteSetupSystem * CompleteSetupSystem :: GetCurrentCompleteSetupSystem() {return _activeCSS;}

CompleteSetupSystem :: CompleteSetupSystem(bool muscleSingleThreadOnly)
   : _threads(muscleSingleThreadOnly)
   , _prevInstance(_activeCSS)
   , _initialMemoryUsage(_activeCSS ? _activeCSS->_initialMemoryUsage : (size_t) GetProcessMemoryUsage())
{
   _activeCSS = this; // push us onto the CSS-stack
}

CompleteSetupSystem :: ~CompleteSetupSystem()
{
   // We'll assume that by this point all spawned threads are gone, and therefore mutex-ordering problems detected after this are not real problems.
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   _enableDeadlockFinderPrints = false;
#endif

   GenericCallbackRef r;
   while(_cleanupCallbacks.RemoveTail(r).IsOK()) (void) r()->Callback(NULL);

   AbstractObjectRecycler::GlobalFlushAllCachedObjects();

   if (_activeCSS != this) MCRASH("CompleteSetupSystem objects should be destroyed in the opposite order of how they were declared");
   _activeCSS = _prevInstance;  // pop us off the stack
}

status_t DataIO :: WriteFully(const void * buffer, uint32 size)
{
   status_t ret;

   const uint8 * b = (const uint8 *)buffer;
   const uint8 * firstInvalidByte = b+size;
   while(b < firstInvalidByte)
   {
      const io_status_t bytesWritten = Write(b, (uint32)(firstInvalidByte-b));
      ret |= bytesWritten.GetStatus();

      if (bytesWritten.GetByteCount() <= 0) break;
      b += bytesWritten.GetByteCount();
   }

   const uint32 numBytesWritten = (uint32) (b-((const uint8 *)buffer));
   return (numBytesWritten == size) ? B_NO_ERROR : (ret | B_IO_ERROR);
}

status_t DataIO :: ReadFully(void * buffer, uint32 size)
{
   uint8 * b                = (uint8 *) buffer;
   uint8 * firstInvalidByte = b+size;
   while(b < firstInvalidByte)
   {
      const io_status_t subRet = Read(b, (uint32)(firstInvalidByte-b));
      MRETURN_ON_ERROR(subRet);

      const uint32 byteCount = subRet.GetByteCount();  // guaranteed to be non-negative at this point
      if (byteCount > 0) b += byteCount;
                    else break; // EOF?
   }

   return (((uint32)(b-((uint8*)buffer))) < size) ? B_DATA_NOT_FOUND : B_NO_ERROR;  // for this method, we either read (size) bytes or we failed!
}

io_status_t DataIO :: ReadFullyUpTo(void * buffer, uint32 size)
{
   uint8 * b                = (uint8 *) buffer;
   uint8 * firstInvalidByte = b+size;
   while(b < firstInvalidByte)
   {
      const io_status_t subRet = Read(b, (uint32)(firstInvalidByte-b));
      if (subRet == B_END_OF_STREAM) break;  // end-of-stream isn't considered to be a real error by this method
      MRETURN_ON_ERROR(subRet);

      const uint32 byteCount = subRet.GetByteCount();  // guaranteed to be non-negative at this point
      if (byteCount > 0) b += byteCount;
                    else break; // EOF?
   }

   return io_status_t((int32)(b-((uint8*)buffer)));
}


int64 SeekableDataIO :: GetLength() const
{
   SeekableDataIO & nonConstThis = const_cast<SeekableDataIO &>(*this);  // so we can call Seek(), below

   const int64 origPos = GetPosition();  // save this so we can restore it afterwards
   if ((origPos >= 0)&&(nonConstThis.Seek(0, IO_SEEK_END).IsOK()))
   {
      const int64 ret = GetPosition();
      if (nonConstThis.Seek(origPos, IO_SEEK_SET).IsOK()) return ret;
   }
   return -1;  // error!
}

status_t Flattenable :: FlattenToDataIO(DataIO & outputStream, bool addSizeHeader) const
{
   uint8 smallBuf[256];
   uint8 * bigBuf = NULL;

   const uint32 fs = FlattenedSize();
   const uint32 bufSize = fs+(addSizeHeader?sizeof(uint32):0);

   uint8 * b;
   if (bufSize<=ARRAYITEMS(smallBuf)) b = smallBuf;
   else
   {
      b = bigBuf = newnothrow_array(uint8, bufSize);
      MRETURN_OOM_ON_NULL(bigBuf);
   }

   // Populate the buffer
   if (addSizeHeader)
   {
      DefaultEndianConverter::Export(fs, b);
      FlattenToBytes(b+sizeof(uint32), fs);
   }
   else FlattenToBytes(b, fs);

   // And finally, write out the buffer
   const status_t ret = outputStream.WriteFully(b, bufSize);
   delete [] bigBuf;
   return ret;
}

status_t Flattenable :: UnflattenFromDataIO(DataIO & inputStream, int32 optReadSize, uint32 optMaxReadSize)
{
   uint32 readSize = (uint32) optReadSize;
   if (optReadSize < 0)
   {
      uint32 leSize;
      MRETURN_ON_ERROR(inputStream.ReadFully(&leSize, sizeof(leSize)));

      readSize = DefaultEndianConverter::Import<uint32>(&leSize);
      if (readSize > optMaxReadSize) return B_BAD_DATA;
   }

   uint8 smallBuf[256];
   uint8 * bigBuf = NULL;
   uint8 * b;
   if (readSize<=ARRAYITEMS(smallBuf)) b = smallBuf;
   else
   {
      b = bigBuf = newnothrow_array(uint8, readSize);
      MRETURN_OOM_ON_NULL(bigBuf);
   }

   status_t ret = inputStream.ReadFully(b, readSize).GetStatus();
   if (ret.IsOK()) ret = UnflattenFromBytes(b, readSize);

   delete [] bigBuf;
   return ret;
}

status_t Flattenable :: CopyFromImplementation(const Flattenable & copyFrom)
{
   uint8 smallBuf[256];
   uint8 * bigBuf = NULL;
   const uint32 flatSize = copyFrom.FlattenedSize();
   if (flatSize > ARRAYITEMS(smallBuf))
   {
      bigBuf = newnothrow_array(uint8, flatSize);
      MRETURN_OOM_ON_NULL(bigBuf);
   }
   copyFrom.FlattenToBytes(bigBuf ? bigBuf : smallBuf, flatSize);

   const status_t ret = UnflattenFromBytes(bigBuf ? bigBuf : smallBuf, flatSize);
   delete [] bigBuf;
   return ret;
}

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
extern void NotifySocketMultiplexersThatSocketIsClosed(int fd);
#endif

// This function is now a private one, since it should no longer be necessary to call it
// from user code.  Instead, attach any socket file descriptors you create to ConstSocketRef
// objects by calling GetConstSocketRefFromPool(fd), and the file descriptors will be automatically
// closed when the last ConstSocketRef that references them is destroyed.
static void CloseSocket(int fd)
{
   if (fd >= 0)
   {
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
      // We have to do this, otherwise a socket fd value can get re-used before the next call
      // to WaitForEvents(), causing the SocketMultiplexers to fail to update their in-kernel state.
      NotifySocketMultiplexersThatSocketIsClosed(fd);
#endif

#if defined(WIN32)
      const int ret = ::closesocket(fd);
#else
      const int ret = close(fd);
#endif
      if (ret != 0) LogTime(MUSCLE_LOG_ERROR, "CloseSocket(%i) failed [%s]\n", fd, B_ERRNO());
   }
}

const ConstSocketRef & GetInvalidSocket()
{
   static const DummyConstSocketRef _ref(GetDefaultObjectForType<Socket>());
   return _ref;
}

ConstSocketRef GetConstSocketRefFromPool(int fd, bool okayToClose, bool returnNULLOnInvalidFD)
{
   static ConstSocketRef::ItemPool _socketPool;

   if ((fd < 0)&&(returnNULLOnInvalidFD)) return B_BAD_ARGUMENT;
   else
   {
      Socket * s = _socketPool.ObtainObject();
      ConstSocketRef ret(s);

      if (s)
      {
         s->SetFileDescriptor(fd, okayToClose);
#ifdef WIN32
         // FogBugz #9911:  Make the socket un-inheritable, since that
         // is the behavior you want 99% of the time.  (Anyone who wants
         // to inherit the socket will have to either avoid calling this
         // for those sockets, or call SetHandleInformation() again
         // afterwards to reinstate the inherit-handle flag)
         (void) SetHandleInformation((HANDLE)((ptrdiff)fd), HANDLE_FLAG_INHERIT, 0);
#endif
         return ret;
      }
      else
      {
         if (okayToClose) CloseSocket(fd);
         MRETURN_OUT_OF_MEMORY;
      }
   }
}

Socket :: ~Socket()
{
   Clear();
}

int Socket :: GetFamilyForFD(int fd)
{
   if (fd < 0) return SOCKET_FAMILY_INVALID;

#ifdef WIN32
   WSAPROTOCOL_INFOW pInfo;
   int pInfoSize = sizeof(pInfo);
   if (getsockopt((SOCKET)fd, SOL_SOCKET, SO_PROTOCOL_INFO, (char *)&pInfo, &pInfoSize) < 0) return SOCKET_FAMILY_INVALID;
   const int sockFamily = pInfo.iAddressFamily;
#else
   struct sockaddr sockAddr;  // "base class" is okay since all we care about is the family value
   muscle_socklen_t sockAddrLen = sizeof(sockAddr);
   if (getsockname(fd, &sockAddr, &sockAddrLen) < 0) return SOCKET_FAMILY_INVALID;
   const int sockFamily = sockAddr.sa_family;
#endif

   switch(sockFamily)
   {
      case AF_INET:  return SOCKET_FAMILY_IPV4;
#ifndef MUSCLE_AVOID_IPV6
      case AF_INET6: return SOCKET_FAMILY_IPV6;
#endif
      default:       return SOCKET_FAMILY_OTHER;
   }
}

void Socket :: SetFileDescriptor(int newFD, bool okayToClose)
{
   if (newFD != _fd)
   {
      if (_okayToClose) CloseSocket(_fd);  // CloseSocket(-1) is a no-op, so no need to check fd twice
      _fd     = newFD;
      _family = Socket::GetFamilyForFD(_fd);
   }
   _okayToClose = okayToClose;
}

static void FlushAsciiChars(const OutputPrinter & p, int idx, char * ascBuf, char * hexBuf, uint32 count, uint32 numColumns)
{
   while(count<numColumns) ascBuf[count++] = ' ';
   ascBuf[count] = '\0';
   p.printf("%04i: %s [%s]\n", idx, ascBuf, hexBuf);
   hexBuf[0] = '\0';
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

static void PrintHexBytesAux(const OutputPrinter & p, const void * vbuf1, uint32 numBytes1, const void * vbuf2, uint32 numBytes2, const char * optDesc, uint32 numColumns)
{
   const uint32 numBytes = numBytes1+numBytes2;
   const uint8 * buf1 = (const uint8 *) vbuf1;
   const uint8 * buf2 = (const uint8 *) vbuf2;

   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) p.printf("%s: ", optDesc);
      p.printf("[");
      if (buf1) for (uint32 i=0; i<numBytes; i++) p.printf("%s%02x", (i==0)?"":" ", (i<numBytes1)?buf1[i]:buf2[i-numBytes1]);
          else p.printf("NULL buffer");
      p.printf("]\n");
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256];
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      p.printf("%s", headBuf);

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) p.printf("-");
      p.printf("\n");
      if (buf1)
      {
         char * ascBuf = newnothrow_array(char, numColumns+1);
         char * hexBuf = newnothrow_array(char, hexBufSize);
         if ((ascBuf)&&(hexBuf))
         {
            ascBuf[0] = hexBuf[0] = '\0';

            uint32 idx = 0;
            while(idx<numBytes)
            {
               const uint8 c = (idx<numBytes1)?buf1[idx]:buf2[idx-numBytes1];
               ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
               const size_t hexBufLen = strlen(hexBuf);
               muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)c)&0xFF));
               idx++;
               if ((idx%numColumns) == 0) FlushAsciiChars(p, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
            }
            const uint32 leftovers = (numBytes%numColumns);
            if (leftovers > 0) FlushAsciiChars(p, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
         }
         else MWARN_OUT_OF_MEMORY;

         delete [] ascBuf;
         delete [] hexBuf;
      }
      else p.printf("NULL buffer\n");
   }
}

void PrintHexBytes(const OutputPrinter & p, const void * vbuf, uint32 numBytes, const char * optDesc, uint32 numColumns)
{
   PrintHexBytesAux(p, vbuf, numBytes, NULL, 0, optDesc, numColumns);
}

void PrintHexBytes(const OutputPrinter & p, const ByteBuffer & bb, const char * optDesc, uint32 numColumns)
{
   PrintHexBytes(p, bb.GetBuffer(), bb.GetNumBytes(), optDesc, numColumns);
}

void PrintHexBytes(const OutputPrinter & p, const ConstByteBufferRef & bbRef, const char * optDesc, uint32 numColumns)
{
   PrintHexBytes(p, bbRef()?bbRef()->GetBuffer():NULL, bbRef()?bbRef()->GetNumBytes():0, optDesc, numColumns);
}

static void PrintHexBytesAux(const OutputPrinter & p, const Queue<uint8> & buf, const char * optDesc, uint32 numColumns)
{
   uint32 numBytes1 = 0, numBytes2 = 0;
   const uint8 * buf1 = buf.GetArrayPointer(0, numBytes1);
   const uint8 * buf2 = buf.GetArrayPointer(1, numBytes2);
   PrintHexBytesAux(p, buf1, numBytes1, buf2, numBytes2, optDesc, numColumns);
}

void PrintHexBytes(const OutputPrinter & p, const Queue<uint8> & bytes, const char * optDesc, uint32 numColumns)
{
   PrintHexBytesAux(p, bytes, optDesc, numColumns);
}

String HexBytesToAnnotatedString(const void * vbuf, uint32 numBytes, const char * optDesc, uint32 numColumns)
{
   String ret;
   PrintHexBytesAux(ret, vbuf, numBytes, NULL, 0, optDesc, numColumns);
   return ret;
}

String HexBytesToAnnotatedString(const Queue<uint8> & buf, const char * optDesc, uint32 numColumns)
{
   String ret;
   PrintHexBytesAux(ret, buf, optDesc, numColumns);
   return ret;
}

String HexBytesToAnnotatedString(const ByteBuffer & bb, const char * optDesc, uint32 numColumns)
{
   return HexBytesToAnnotatedString(bb.GetBuffer(), bb.GetNumBytes(), optDesc, numColumns);
}

String HexBytesToAnnotatedString(const ConstByteBufferRef & bbRef, const char * optDesc, uint32 numColumns)
{
   return HexBytesToAnnotatedString(bbRef()?bbRef()->GetBuffer():NULL, bbRef()?bbRef()->GetNumBytes():0, optDesc, numColumns);
}

String HexBytesToString(const uint8 * buf, uint32 numBytes, bool withSpaces)
{
   String ret;
   if (ret.Prealloc(numBytes*(withSpaces?3:2)).IsOK())
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         if ((withSpaces)&&(i > 0)) ret += ' ';
         char b[32]; muscleSprintf(b, "%02x", buf[i]);
         ret += b;
      }
   }
   return ret;
}

String HexBytesToString(const ConstByteBufferRef & bbRef, bool withSpaces)
{
   return bbRef() ? HexBytesToString(*bbRef(), withSpaces) : String("(null)");
}

String HexBytesToString(const ByteBuffer & bb, bool withSpaces)
{
   return HexBytesToString(bb.GetBuffer(), bb.GetNumBytes(), withSpaces);
}

String HexBytesToString(const Queue<uint8> & bytes, bool withSpaces)
{
   const uint32 numBytes = bytes.GetNumItems();

   String ret;
   if (ret.Prealloc(numBytes*(withSpaces?3:2)).IsOK())
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         if ((withSpaces)&&(i > 0)) ret += ' ';
         char b[32]; muscleSprintf(b, "%02x", bytes[i]);
         ret += b;
      }
   }
   return ret;
}

DebugTimer :: DebugTimer(const String & title, uint64 mlt, uint32 startMode, int debugLevel)
   : _currentMode(startMode+1)
   , _title(title)
   , _minLogTime(mlt)
   , _debugLevel(debugLevel)
   , _enableLog(true)
{
   SetMode(startMode);
   _startTime = MUSCLE_DEBUG_TIMER_CLOCK;  // re-set it here so that we don't count the Hashtable initialization!
}

DebugTimer :: ~DebugTimer()
{
   if (_enableLog)
   {
      // Finish off the current mode
      uint64 * curElapsed = _modeToElapsedTime.Get(_currentMode);
      if (curElapsed) *curElapsed += MUSCLE_DEBUG_TIMER_CLOCK-_startTime;

      // And print out our stats
      for (HashtableIterator<uint32, uint64> iter(_modeToElapsedTime); iter.HasData(); iter++)
      {
         const uint64 nextTime = iter.GetValue();
         if (nextTime >= _minLogTime)
         {
            if (_debugLevel >= 0)
            {
               if (nextTime >= 1000) LogTime(_debugLevel, "%s: mode " UINT32_FORMAT_SPEC ": " UINT64_FORMAT_SPEC " milliseconds elapsed\n", _title(), iter.GetKey(), nextTime/1000);
                                else LogTime(_debugLevel, "%s: mode " UINT32_FORMAT_SPEC ": " UINT64_FORMAT_SPEC " microseconds elapsed\n", _title(), iter.GetKey(), nextTime);
            }
            else
            {
               // For cases where we don't want to call LogTime()
               if (nextTime >= 1000) printf("%s: mode " UINT32_FORMAT_SPEC ": " UINT64_FORMAT_SPEC " milliseconds elapsed\n", _title(), iter.GetKey(), nextTime/1000);
                                else printf("%s: mode " UINT32_FORMAT_SPEC ": " UINT64_FORMAT_SPEC " microseconds elapsed\n", _title(), iter.GetKey(), nextTime);
            }
         }
      }
   }
}

bool ParseBool(const String & word, bool defaultValue)
{
   static const char * _onWords[]  = {"on",  "enable",  "enabled",  "true",  "t", "y", "yes", "1"};
   static const char * _offWords[] = {"off", "disable", "disabled", "false", "f", "n", "no",  "0"};

   const String s = word.Trimmed().ToLowerCase();
   for (uint32 i=0; i<ARRAYITEMS(_onWords);  i++) if (s == _onWords[i])  return true;
   for (uint32 i=0; i<ARRAYITEMS(_offWords); i++) if (s == _offWords[i]) return false;
   return defaultValue;
}

/** Gotta define this myself, since atoll() isn't standard. :^(
  * Note that this implementation doesn't handle negative numbers!
  */
uint64 Atoull(const char * str)
{
   TCHECKPOINT;

   const char * s = str;
   if (muscleInRange(*s, '0', '9') == false) return 0;

   uint64 base = 1;
   uint64 ret  = 0;

   // Move to the last digit in the number
   while(muscleInRange(*s, '0', '9')) s++;

   // Then iterate back to the beginning, tabulating as we go
   while((--s >= str)&&(*s >= '0')&&(*s <= '9'))
   {
      ret  += base * ((uint64)(*s-'0'));
      base *= (uint64)10;
   }
   return ret;
}

// Returns the integer value of the specified hex char, or -1 if c is not a valid hex char (i.e. not 0-9, a-f, or A-F)
static int ParseHexDigit(char c)
{
        if (muscleInRange(c, '0', '9')) return (c-'0');
   else if (muscleInRange(c, 'a', 'f')) return 10+(c-'a');
   else if (muscleInRange(c, 'A', 'F')) return 10+(c-'A');
   else                                 return -1;
}

uint64 Atoxll(const char * str)
{
   if ((str[0] == '0')&&((str[1] == 'x')||(str[1] == 'X'))) str += 2;  // skip any optional "0x" prefix in "0xDEADBEEF"

   const char * s = str;
   if (ParseHexDigit(*s) < 0) return 0;

   uint64 base = 1;
   uint64 ret  = 0;

   // Move to the last digit in the number
   while(ParseHexDigit(*s) >= 0) s++;

   // Then iterate back to the beginning, tabulating as we go
   int hd;
   while((--s >= str)&&((hd = ParseHexDigit(*s)) >= 0))
   {
      ret  += base * (uint64)hd;
      base *= (uint64)16;
   }
   return ret;
}

int64 Atoll(const char * str)
{
   TCHECKPOINT;

   bool negative = false;
   const char * s = str;
   while(*s == '-')
   {
      negative = (negative == false);
      s++;
   }
   const int64 ret = (int64) Atoull(s);
   return negative ? -ret : ret;
}

/** Set the timer to record elapsed time to a different mode. */
void DebugTimer :: SetMode(uint32 newMode)
{
   if (newMode != _currentMode)
   {
      uint64 * curElapsed = _modeToElapsedTime.Get(_currentMode);
      if (curElapsed) *curElapsed += MUSCLE_DEBUG_TIMER_CLOCK-_startTime;

      _currentMode = newMode;
      (void) _modeToElapsedTime.GetOrPut(_currentMode, 0);
      _startTime = MUSCLE_DEBUG_TIMER_CLOCK;
   }
}

#ifdef MUSCLE_SINGLE_THREAD_ONLY
bool IsCurrentThreadMainThread() {return true;}
#else
bool IsCurrentThreadMainThread()
{
   if (_threadSetupCount > 0) return (_mainThreadID == muscle_thread_id::GetCurrentThreadID());
   else
   {
      MCRASH("IsCurrentThreadMainThread() cannot be called unless there is a CompleteSetupSystem object on the stack!");
      return false;  // to shut the compiler up
   }
}
#endif

uint32 CalculateHashCode(const void * key, uint32 numBytes, uint32 seed)
{
#define MURMUR2_MIX(h,k,m) { (k) *= (m); (k) ^= (k) >> (r); (k) *= (m); (h) *= (m); (h) ^= (k); }
   const uint32 m = 0x5bd1e995;
   const int32  r = 24;

   const unsigned char * data = (const unsigned char *)key;
   uint32 h = seed ^ numBytes;
   const uint32 align = ((uint32)((uintptr)data)) & 3;
   if ((align!=0)&&(numBytes >= 4))
   {
      // Pre-load the temp registers
      uint32 t = 0, d = 0;
      switch(align)
      {
         case 1: t |= data[2] << 16;  // fall through!
         case 2: t |= data[1] << 8;   // fall through!
         case 3: t |= data[0];        // fall through!
         default: /* empty */ break;
      }

      t <<= (8 * align);
      data     += 4-align;
      numBytes -= 4-align;

      int32 sl = 8 * (4-align);
      int32 sr = 8 * align;

      // Mix
      while(numBytes >= 4)
      {
         d = *(uint32 *)data;
         t = (t >> sr) | (d << sl);

         uint32 k = t;
         MURMUR2_MIX(h,k,m);
         t = d;

         data += 4;
         numBytes -= 4;
      }

      // Handle leftover data in temp registers
      d = 0;
      if(numBytes >= align)
      {
         switch(align)
         {
            case 3: d |= data[2] << 16;  // fall through
            case 2: d |= data[1] << 8;   // fall through
            case 1: d |= data[0];        // fall through
            default: /* empty */ break;
         }

         uint32 k = (t >> sr) | (d << sl);
         MURMUR2_MIX(h,k,m);

         data += align;
         // coverity[overflow_const : FALSE] - Coverity reports an underflow bug here but we know (numBytes >= align) so I don't see how that's possible --jaf
         numBytes -= align;

         //----------
         // Handle tail bytes
         switch(numBytes)
         {
            case 3: h ^= data[2] << 16; // fall through!
            case 2: h ^= data[1] << 8;  // fall through!
            case 1: h ^= data[0];       // fall through!
                    h *= m;
            default: /* empty */  break;
         };
      }
      else
      {
         switch(numBytes)
         {
            case 3: d |= data[2] << 16;  // fall through!
            case 2: d |= data[1] << 8;   // fall through!
            case 1: d |= data[0];        // fall through!
            case 0: h ^= (t >> sr) | (d << sl);
                    h *= m;
            default: /* empty */  break;
         }
      }

      h ^= h >> 13;
      h *= m;
      h ^= h >> 15;

      return h;
   }
   else
   {
      while(numBytes >= 4)
      {
         uint32 k = *(uint32 *)data;
         MURMUR2_MIX(h,k,m);
         data     += 4;
         numBytes -= 4;
      }

      //----------
      // Handle tail bytes

      switch(numBytes)
      {
         case 3: h ^= data[2] << 16; // fall through!
         case 2: h ^= data[1] << 8;  // fall through!
         case 1: h ^= data[0];       // fall through!
                 h *= m;
         default: /* empty */ break;
      };

      h ^= h >> 13;
      h *= m;
      h ^= h >> 15;

      return h;
   }
}

uint64 CalculateHashCode64(const void * key, unsigned int numBytes, unsigned int seed)
{
   const uint64 m = 0xc6a4a7935bd1e995;
   const int r = 47;

   uint64 h = seed ^ (numBytes * m);

   const uint64 * data = (const uint64 *)key;
   const uint64 * end = data + (numBytes/sizeof(uint64));

   while(data != end)
   {
      uint64 k = *data++;
      k *= m;
      k ^= k >> r;
      k *= m;
      h ^= k;
      h *= m;
   }

   const unsigned char * data2 = (const unsigned char*)data;
   switch(numBytes & 7)
   {
      case 7: h ^= uint64(data2[6]) << 48; // fall through!
      case 6: h ^= uint64(data2[5]) << 40; // fall through!
      case 5: h ^= uint64(data2[4]) << 32; // fall through!
      case 4: h ^= uint64(data2[3]) << 24; // fall through!
      case 3: h ^= uint64(data2[2]) << 16; // fall through!
      case 2: h ^= uint64(data2[1]) << 8;  // fall through!
      case 1: h ^= uint64(data2[0]);       // fall through!
              h *= m;
      default:  /* empty */ break;
   }

   h ^= h >> r;
   h *= m;
   h ^= h >> r;
   return h;
}

#ifdef MUSCLE_ENABLE_OBJECT_COUNTING

static ObjectCounterBase * _firstObjectCounter = NULL;

void ObjectCounterBase :: PrependObjectCounterBaseToGlobalCountersList()
{
   _nextCounter = _firstObjectCounter;
   if (_firstObjectCounter) _firstObjectCounter->_prevCounter = this;
   _firstObjectCounter = this;
}

void ObjectCounterBase :: RemoveObjectCounterBaseFromGlobalCountersList()
{
   if (_firstObjectCounter == this) _firstObjectCounter = _nextCounter;
   if (_prevCounter) _prevCounter->_nextCounter = _nextCounter;
   if (_nextCounter) _nextCounter->_prevCounter = _prevCounter;
}

ObjectCounterBase :: ObjectCounterBase(const char * objectCounterTypeName, uint32 sizeofObject)
   : _objectCounterTypeName(objectCounterTypeName)
   , _sizeofObject(sizeofObject)
   , _prevCounter(NULL)
   , _nextCounter(NULL)
{
   if (_muscleLock)
   {
      DECLARE_MUTEXGUARD(*_muscleLock);
      PrependObjectCounterBaseToGlobalCountersList();
   }
   else PrependObjectCounterBaseToGlobalCountersList();
}

ObjectCounterBase :: ~ObjectCounterBase()
{
   if (_muscleLock)
   {
      DECLARE_MUTEXGUARD(*_muscleLock);
      RemoveObjectCounterBaseFromGlobalCountersList();
   }
   else RemoveObjectCounterBaseFromGlobalCountersList();
}

#endif

status_t GetCountedObjectInfo(Hashtable<const char *, uint64> & results)
{
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   Mutex * m = _muscleLock;
   if ((m==NULL)||(m->Lock().IsOK()))
   {
      status_t ret;

      const ObjectCounterBase * oc = _firstObjectCounter;
      while(oc)
      {
         (void) results.Put(oc->GetCounterTypeName(), (((uint64)oc->GetSizeofObject())<<32)|((uint64)oc->GetCount())).IsError(ret);
         oc = oc->GetNextCounter();
      }

      if (m) (void) m->Unlock();
      return ret;
   }
   else return B_LOCK_FAILED;
#else
   (void) results;
   return B_UNIMPLEMENTED;
#endif
}

class CompareSizesFunctor
{
public:
   MUSCLE_NODISCARD int Compare(const uint64 & v1, const uint64 & v2, void *) const
   {
      const uint32 objCount1 = ((v1>>00) & 0xFFFFFFFF);
      const uint32 objCount2 = ((v2>>00) & 0xFFFFFFFF);
      const uint32 objSize1  = ((v1>>32) & 0xFFFFFFFF);
      const uint32 objSize2  = ((v2>>32) & 0xFFFFFFFF);
      return muscleCompare(((uint64)objSize1)*((uint64)objCount1), ((uint64)objSize2)*((uint64)objCount2));
   }
};

void PrintCountedObjectInfo(const OutputPrinter & p)
{
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   uint64 totalNumObjects = 0;
   uint64 totalNumBytes   = 0;
   Hashtable<const char *, uint64> table;
   if (GetCountedObjectInfo(table).IsOK())
   {
      table.SortByValue(CompareSizesFunctor());  // so they'll be printed in alphabetical order
      for (HashtableIterator<const char *, uint64> iter(table, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
      {
         const uint64 v        = iter.GetValue();
         const uint32 objSize  = ((v>>32) & 0xFFFFFFFF);
         const uint32 objCount = ((v>>00) & 0xFFFFFFFF);
         totalNumObjects += objCount;
         totalNumBytes   += ((uint64)objSize)*((uint64)objCount);
      }

      p.printf("Counted Object Info report follows: (" UINT32_FORMAT_SPEC " types counted, " UINT64_FORMAT_SPEC " total objects, %.02f total MB, average " UINT64_FORMAT_SPEC " bytes/object)\n", table.GetNumItems(), totalNumObjects, ((double)totalNumBytes)/(1024*1024), (uint64)((totalNumObjects>0)?(totalNumBytes/totalNumObjects):0));
      for (HashtableIterator<const char *, uint64> iter(table, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
      {
         const uint64 v        = iter.GetValue();
         const uint32 objSize  = ((v>>32) & 0xFFFFFFFF);
         const uint32 objCount = ((v>>00) & 0xFFFFFFFF);
         p.printf("   %6" UINT32_FORMAT_SPEC_NOPERCENT " %s (" UINT32_FORMAT_SPEC " bytes/object, %ikB used))\n", objCount, iter.GetKey(), objSize, (int)((512+(((uint64)objSize)*((uint64)objCount)))/1024));
      }
   }
   else p.printf("PrintCountedObjectInfo:  GetCountedObjectInfo() failed!\n");
#else
   p.printf("Counted Object Info report not available, because MUSCLE was compiled without -DMUSCLE_ENABLE_OBJECT_COUNTING\n");
#endif
}

void SetMainReflectServerCatchSignals(bool enable)
{
   _mainReflectServerCatchSignals = enable;
}

bool GetMainReflectServerCatchSignals()
{
   return _mainReflectServerCatchSignals;
}

Queue<String> GetBuildFlags()
{
   Queue<String> q;

#ifdef MUSCLE_ENABLE_SSL
   (void) q.AddTail("MUSCLE_ENABLE_SSL");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   (void) q.AddTail("MUSCLE_AVOID_CPLUSPLUS11");
#endif

#ifdef MUSCLE_USE_CPLUSPLUS17
   (void) q.AddTail("MUSCLE_USE_CPLUSPLUS17");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11_THREADS
   (void) q.AddTail("MUSCLE_AVOID_CPLUSPLUS11_THREADS");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD
   (void) q.AddTail("MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD");
#endif

#ifdef MUSCLE_AVOID_IPV6
   (void) q.AddTail("MUSCLE_AVOID_IPV6");
#endif

#ifdef MUSCLE_AVOID_STDINT
   (void) q.AddTail("MUSCLE_AVOID_STDINT");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
   (void) q.AddTail("MUSCLE_SINGLE_THREAD_ONLY");
#endif

#ifdef MUSCLE_USE_EPOLL
   (void) q.AddTail("MUSCLE_USE_EPOLL");
#endif

#ifdef MUSCLE_USE_POLL
   (void) q.AddTail("MUSCLE_USE_POLL");
#endif

#ifdef MUSCLE_USE_KQUEUE
   (void) q.AddTail("MUSCLE_USE_KQUEUE");
#endif

#ifdef MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS
   (void) q.AddTail(String("MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS=%1").Arg(MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS));
#endif

#ifdef MUSCLE_CATCH_SIGNALS_BY_DEFAULT
   (void) q.AddTail("MUSCLE_CATCH_SIGNALS_BY_DEFAULT");
#endif

#ifdef MUSCLE_USE_LIBRT
   (void) q.AddTail("MUSCLE_USE_LIBRT");
#endif

#ifdef MUSCLE_AVOID_MULTICAST_API
   (void) q.AddTail("MUSCLE_AVOID_MULTICAST_API");
#endif

#ifdef MUSCLE_DISABLE_KEEPALIVE_API
   (void) q.AddTail("MUSCLE_DISABLE_KEEPALIVE_API");
#endif

#ifdef MUSCLE_64_BIT_PLATFORM
   (void) q.AddTail("MUSCLE_64_BIT_PLATFORM");
#endif

#ifdef MUSCLE_USE_LLSEEK
   (void) q.AddTail("MUSCLE_USE_LLSEEK");
#endif

#ifdef MUSCLE_PREFER_QT_OVER_WIN32
   (void) q.AddTail("MUSCLE_PREFER_QT_OVER_WIN32");
#endif

#ifdef MUSCLE_ENABLE_MEMORY_PARANOIA
   (void) q.AddTail(String("MUSCLE_ENABLE_MEMORY_PARANOIA=%1").Arg(MUSCLE_ENABLE_MEMORY_PARANOIA));
#endif

#ifdef MUSCLE_NO_EXCEPTIONS
   (void) q.AddTail("MUSCLE_NO_EXCEPTIONS");
#endif

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   (void) q.AddTail("MUSCLE_ENABLE_MEMORY_TRACKING");
#endif

#ifdef MUSCLE_AVOID_ASSERTIONS
   (void) q.AddTail("MUSCLE_AVOID_ASSERTIONS");
#endif

#ifdef MUSCLE_AVOID_SIGNAL_HANDLING
   (void) q.AddTail("MUSCLE_AVOID_SIGNAL_HANDLING");
#endif

#ifdef MUSCLE_AVOID_INLINE_ASSEMBLY
   (void) q.AddTail("MUSCLE_AVOID_INLINE_ASSEMBLY");
#endif

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   (void) q.AddTail("MUSCLE_ENABLE_ZLIB_ENCODING");
#endif

#ifdef MUSCLE_TRACE_CHECKPOINTS
   (void) q.AddTail(String("MUSCLE_TRACE_CHECKPOINTS=%1").Arg(MUSCLE_TRACE_CHECKPOINTS));
#endif

#ifdef MUSCLE_DISABLE_MESSAGE_FIELD_POOLS
   (void) q.AddTail("MUSCLE_DISABLE_MESSAGE_FIELD_POOLS");
#endif

#ifdef MUSCLE_INLINE_LOGGING
   (void) q.AddTail("MUSCLE_INLINE_LOGGING");
#endif

#ifdef MUSCLE_DISABLE_LOGGING
   (void) q.AddTail("MUSCLE_DISABLE_LOGGING");
#endif

#ifdef MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS
   (void) q.AddTail("MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS");
#endif

#ifdef MUSCLE_MUTEX_POOL_SIZE
   (void) q.AddTail(String("MUSCLE_MUTEX_POOL_SIZE").Arg(MUSCLE_MUTEX_POOL_SIZE));
#endif

#ifdef MUSCLE_POWERPC_TIMEBASE_HZ
   (void) q.AddTail(String("MUSCLE_POWERPC_TIMEBASE_HZ=%1").Arg(MUSCLE_POWERPC_TIMEBASE_HZ));
#endif

#ifdef MUSCLE_USE_PTHREADS
   (void) q.AddTail("MUSCLE_USE_PTHREADS");
#endif

#ifdef MUSCLE_USE_PTHREADS
   (void) q.AddTail("MUSCLE_USE_CPLUSPLUS11_THREADS");
#endif

#ifdef MUSCLE_DEFAULT_TCP_STALL_TIMEOUT
   (void) q.AddTail("MUSCLE_DEFAULT_TCP_STALL_TIMEOUT=%1").Arg(MUSCLE_DEFAULT_TCP_STALL_TIMEOUT);
#endif

#ifdef MUSCLE_FD_SETSIZE
   (void) q.AddTail(String("MUSCLE_FD_SETSIZE=%1").Arg(MUSCLE_FD_SETSIZE));
#endif

#ifdef MUSCLE_AVOID_NEWNOTHROW
   (void) q.AddTail("MUSCLE_AVOID_NEWNOTHROW");
#endif

#ifdef MUSCLE_AVOID_FORKPTY
   (void) q.AddTail("MUSCLE_AVOID_FORKPTY");
#endif

#ifdef MUSCLE_HASHTABLE_DEFAULT_CAPACITY
   (void) q.AddTail(String("MUSCLE_HASHTABLE_DEFAULT_CAPACITY=%1").Arg(MUSCLE_HASHTABLE_DEFAULT_CAPACITY));
#endif

#ifdef SMALL_QUEUE_SIZE
   (void) q.AddTail(String("SMALL_QUEUE_SIZE=%1").Arg(SMALL_QUEUE_SIZE));
#endif

#ifdef SMALL_MUSCLE_STRING_LENGTH
   (void) q.AddTail(String("SMALL_MUSCLE_STRING_LENGTH=%1").Arg(SMALL_MUSCLE_STRING_LENGTH));
#endif

#ifdef MUSCLE_USE_QUERYPERFORMANCECOUNTER
   (void) q.AddTail("MUSCLE_USE_QUERYPERFORMANCECOUNTER");
#endif

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
   (void) q.AddTail("MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME");
#endif

#ifdef MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS
   (void) q.AddTail("MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS");
#endif

#ifdef MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS
   (void) q.AddTail(String("MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS=%1").Arg(MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS));
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   (void) q.AddTail("MUSCLE_ENABLE_DEADLOCK_FINDER");
#endif

#ifdef MUSCLE_DEFAULT_RUNTIME_DISABLE_DEADLOCK_FINDER
   (void) q.AddTail("MUSCLE_DEFAULT_RUNTIME_DISABLE_DEADLOCK_FINDER");
#endif

#ifdef MUSCLE_POOL_SLAB_SIZE
   (void) q.AddTail(String("MUSCLE_POOL_SLAB_SIZE=%1").Arg(MUSCLE_POOL_SLAB_SIZE));
#endif

#ifdef MUSCLE_AVOID_TAGGED_POINTERS
   (void) q.AddTail("MUSCLE_AVOID_TAGGED_POINTERS");
#endif

#ifdef MUSCLE_AVOID_CHECK_THREAD_STACK_USAGE
   (void) q.AddTail("MUSCLE_AVOID_CHECK_THREAD_STACK_USAGE");
#endif

#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   (void) q.AddTail("MUSCLE_ENABLE_OBJECT_COUNTING");
#endif

#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   (void) q.AddTail("MUSCLE_AVOID_THREAD_LOCAL_STORAGE");
#endif

#ifdef MUSCLE_AVOID_MINIMIZED_HASHTABLES
   (void) q.AddTail("MUSCLE_AVOID_MINIMIZED_HASHTABLES");
#endif

#ifdef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
   (void) q.AddTail("MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS");
#endif

#ifdef MUSCLE_FAKE_SHARED_MEMORY
   (void) q.AddTail("MUSCLE_FAKE_SHARED_MEMORY");
#endif

#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
   (void) q.AddTail("MUSCLE_COUNT_STRING_COPY_OPERATIONS");
#endif

#ifdef DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD
   (void) q.AddTail(String("DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD=%1").Arg(DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD));
#endif

#ifdef MUSCLE_AVOID_AUTOCHOOSE_SWAP
   (void) q.AddTail("MUSCLE_AVOID_AUTOCHOOSE_SWAP");
#endif

#ifdef MUSCLE_AVOID_NODISCARD
   (void) q.AddTail("MUSCLE_AVOID_NODISCARD");
#endif

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
   (void) q.AddTail("MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS");
#endif

#ifdef MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION
   (void) q.AddTail("MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION");
#endif

#ifdef MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION
   (void) q.AddTail("MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION");
#endif

#ifdef MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES
   (void) q.AddTail("MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES");
#endif

#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
   (void) q.AddTail("MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT");
#endif
   return q;
}

void PrintBuildFlags(const OutputPrinter & p)
{
   Queue<String> flagStrs = GetBuildFlags();
   for (uint32 i=0; i<flagStrs.GetNumItems(); i++) p.printf("MUSCLE code was compiled with preprocessor flag -D%s\n", flagStrs[i]());
}

uint64 GetProcessMemoryUsage()
{
#if defined(__linux__)
   FILE* fp = muscleFopen("/proc/self/statm", "r");
   if (fp)
   {
      size_t vmSize = 0, residentPages = 0, sharedPages = 0;
      const bool readData = (fscanf(fp, "%zu %zu %zu", &vmSize, &residentPages, &sharedPages) == 3);
      fclose(fp);
      return readData ? ((residentPages>sharedPages)?(residentPages-sharedPages):0)*sysconf(_SC_PAGESIZE) : 0;
   }
#elif defined(__APPLE__)
# if defined(MAC_OS_X_VERSION_10_11)  // The phys_footprint field was introduced in MacOS 10.11
   task_vm_info_data_t task_vm_info;
   mach_msg_type_number_t outCount = TASK_VM_INFO_COUNT;
   return (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&task_vm_info, &outCount) == KERN_SUCCESS) ? task_vm_info.phys_footprint : 0;
# else
   // This old method is less accurate as it only measure the memory currently resident in RAM; it doesn't include paged-out memory
   mach_task_basic_info_data_t taskinfo; memset(&taskinfo, 0, sizeof(taskinfo));
   mach_msg_type_number_t outCount = MACH_TASK_BASIC_INFO_COUNT;
   if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskinfo, &outCount) == KERN_SUCCESS) return taskinfo.resident_size;
# endif
#elif defined(WIN32) && !defined(__MINGW32__)
   PROCESS_MEMORY_COUNTERS_EX pmc;
   if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) return pmc.PrivateUsage;
#endif
   return 0;
}

String GetEnvironmentVariableValue(const String & envVarName, const String & defaultValue)
{
#ifdef _MSC_VER
   char s[4096];
   const DWORD res = GetEnvironmentVariableA(envVarName(), s, sizeof(s));
   return (res > 0) ? String(s) : defaultValue;
#else
   const char * s = getenv(envVarName());
   return s ? String(s) : defaultValue;
#endif
}

void OutputPrinter :: printf(const char * fmt, ...) const
{
   char buf[1024];  // big enough that it won't usually be exceeded, but small enough to fit on the stack

   // Note that (numChars1) gets set to the number of bytes that *should have*
   // been printed, not including the NUL terminator byte.
   // So if (buf) is too small, it's possible that (numChars1 >= sizeof(buf))
   va_list va1; va_start(va1, fmt);
#if __STDC_WANT_SECURE_LIB__
   const int numChars1 = _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, va1);
#elif WIN32
   const int numChars1 = _vsnprintf(  buf, sizeof(buf),            fmt, va1);
#else
   const int numChars1 =  vsnprintf(  buf, sizeof(buf),            fmt, va1);
#endif
   va_end(va1);

   if (numChars1 > 0)
   {
      if (((size_t)(numChars1+1)) >= sizeof(buf))
      {
         // Oops, (buf) wasn't large enough!  So we will
         // heap-allocate a large-enough buffer and try again.
         const size_t heapBufSize = numChars1+1;
         char * heapBuf = (char *) muscleAlloc(heapBufSize);
         if (heapBuf)
         {
            va_list va2; va_start(va2, fmt);
#if __STDC_WANT_SECURE_LIB__
            const int numChars2 = _vsnprintf_s(heapBuf, heapBufSize, _TRUNCATE, fmt, va2);
#elif WIN32
            const int numChars2 = _vsnprintf(  heapBuf, heapBufSize,            fmt, va2);
#else
            const int numChars2 =  vsnprintf(  heapBuf, heapBufSize,            fmt, va2);
#endif
            va_end(va2);

            if (numChars2 > 0)
            {
               heapBuf[heapBufSize-1] = '\0'; // paranoia
               putsAux(heapBuf, muscleMin((uint32)numChars2, (uint32)(heapBufSize-1)));
            }

            free(heapBuf);
         }
         else MWARN_OUT_OF_MEMORY;
      }
      else putsAux(buf, numChars1);  // easy case -- all the output fit into (buf)
   }
}

void OutputPrinter :: putc(char c, uint32 repeatCount) const
{
   uint32 numCharsWritten = 0;

   char buf[256];
   while(repeatCount > 0)
   {
      const uint32 charsToWrite = muscleMin(repeatCount, (uint32)(sizeof(buf)-1));
      if (numCharsWritten < charsToWrite)
      {
         memset(buf, c, charsToWrite);
         numCharsWritten = charsToWrite;
      }
      buf[charsToWrite] = '\0';
      if (c == '\n') putsAux(buf, charsToWrite);
                else putsAuxAux(buf, charsToWrite);  // no sense scanning for newlines, there aren't any
      repeatCount -= charsToWrite;
   }
}

void OutputPrinter :: fflush() const
{
   if (_file) ::fflush(_file);
   if (_logSeverity > MUSCLE_LOG_NONE) (void) LogFlush();
}

void OutputPrinter :: puts(const char * s, uint32 repeatCount) const
{
   const uint32 numChars = s ? (uint32)strlen(s) : 0;
   if (numChars > 0) for (uint32 i=0; i<repeatCount; i++) putsAux(s, numChars);
}

void OutputPrinter :: putsAux(const char * s, uint32 numChars) const
{
   if ((_indent > 0)||(_logSeverity > MUSCLE_LOG_NONE))
   {
      const char * orig = s;
      while((*s)&&((s-orig) < numChars))
      {
         // Find the next newline, if any
         const char * pc = s;
         while((*pc)&&((pc-orig)<numChars))
         {
            if (*pc == '\n')
            {
               pc++;
               break;
            }
            else pc++;
         }

         if (pc > s)  // semi-paranoia
         {
            putsAuxAux(s, (uint32)(pc-s));  // Call putsAuxAux() once for each line of text
            s = pc;
         }
         else break;  // paranoia:  avoid potential infinite loop
      }
   }
   else putsAuxAux(s, numChars); // no point in scanning for newlines if they won't affect our behavior anyway
}

void OutputPrinter :: putsAuxAux(const char * s, uint32 numChars) const
{
   if (_isAtStartOfLine)
   {
      if (_logSeverity > MUSCLE_LOG_NONE) LogTime(_logSeverity, GetEmptyString()(), "");  // generate the log-timestamp-preamble at the left edge, before the indent
      _isAtStartOfLine = false;
      if (_indent > 0) putc(' ', _indent);
   }

   if (_addToString) (void) _addToString->AppendChars(s, numChars);
   if (_logSeverity > MUSCLE_LOG_NONE)
   {
      char fmt[64] = "%."; muscleSnprintf(&fmt[2], sizeof(fmt)-2, UINT32_FORMAT_SPEC "s", numChars);  // keeping the %. separate to avoid confusing clang++
      LogPlain(_logSeverity, fmt, s);
   }
   if (_file) (void) fwrite(s, 1, numChars, _file);  // Calling fwrite() instead of fputs() since we already know the string's length

   _isAtStartOfLine = ((numChars > 0)&&(s[numChars-1] == '\n'));
}

} // end namespace muscle
