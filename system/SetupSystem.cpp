/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/SetupSystem.h"
#include "support/Flattenable.h"
#include "dataio/SeekableDataIO.h"
#include "reflector/SignalHandlerSession.h"  // for SetMainReflectServerCatchSignals()
#include "system/SystemInfo.h"             // for GetBuildFlags()
#include "util/ObjectPool.h"
#include "util/ByteBuffer.h"
#include "util/DebugTimer.h"
#include "util/CountedObject.h"
#include "util/MiscUtilityFunctions.h"     // for PrintHexBytes()
#include "util/NetworkUtilityFunctions.h"  // for IPAddressAndPort
#include "util/String.h"

#ifdef MUSCLE_ENABLE_SSL
# include <openssl/err.h>
# include <openssl/ssl.h>
# ifndef WIN32
#  include <pthread.h>
# endif
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
# if defined(__BEOS__) || defined(__HAIKU__)
#  include <signal.h>
# elif defined(__CYGWIN__)
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

// Commonly used error codes for status_t
const status_t B_OUT_OF_MEMORY(  "Out of Memory");
const status_t B_UNIMPLEMENTED(  "Unimplemented");
const status_t B_ACCESS_DENIED(  "Access Denied");
const status_t B_DATA_NOT_FOUND( "Data not Found");
const status_t B_FILE_NOT_FOUND( "File not Found");
const status_t B_BAD_ARGUMENT(   "Bad Argument");
const status_t B_BAD_DATA(       "Bad Data");
const status_t B_BAD_OBJECT(     "Bad Object");
const status_t B_TIMED_OUT(      "Timed Out");
const status_t B_IO_ERROR(       "I/O Error");
const status_t B_LOCK_FAILED(    "Lock Failed");
const status_t B_TYPE_MISMATCH(  "Type Mismatch");
const status_t B_ZLIB_ERROR(     "ZLib Error");
const status_t B_SSL_ERROR(      "SSL Error");
const status_t B_LOGIC_ERROR(    "Logic Error");

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
   if ((origOne)&&(memcmp(orig, origOne, numBytes)))      GoInsane(why, "(origOne)");
   if ((origTwo)&&(memcmp(orig, origTwo, numBytes)))      GoInsane(why, "(origTwo)");
}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
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
   PrintAndClearStringCopyCounts("At end of main()");
#endif
}

#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
void PrintAndClearStringCopyCounts(const char * optDesc)
{
   const uint32 * s = _stringOpCounts;  // just to save chars
   const uint32 totalCopies = s[STRING_OP_COPY_CTOR] + s[STRING_OP_PARTIAL_COPY_CTOR] + s[STRING_OP_SET_FROM_STRING];
   const uint32 totalMoves  = s[STRING_OP_MOVE_CTOR] + s[STRING_OP_MOVE_FROM_STRING];

   printf("String Op Counts [%s]\n", optDesc?optDesc:"Untitled");
   printf("# Default Ctors = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_DEFAULT_CTOR]);
   printf("# Cstr Ctors    = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_CSTR_CTOR]);
   printf("# Copy Ctors    = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_COPY_CTOR]);
   printf("# PtCopy Ctors  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_PARTIAL_COPY_CTOR]);
   printf("# Set from Cstr = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_SET_FROM_CSTR]);
   printf("# Set from Str  = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_SET_FROM_STRING]);
   printf("# Move Ctor     = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_MOVE_CTOR]);
   printf("# Move from Str = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_MOVE_FROM_STRING]);
   printf("# Dtors         = " UINT32_FORMAT_SPEC "\n", s[STRING_OP_DTOR]);
   printf("-----------------------------\n");
   printf("# Total Copies  = " UINT32_FORMAT_SPEC "\n", totalCopies);
   printf("# Total Moves   = " UINT32_FORMAT_SPEC "\n", totalMoves);
   printf("# Total Either  = " UINT32_FORMAT_SPEC "\n", totalCopies+totalMoves);
   printf("\n");

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

#if defined(__BEOS__) || defined(__HAIKU__)
   // empty
#elif defined(TARGET_PLATFORM_XENOMAI) && !defined(MUSCLE_AVOID_XENOMAI)
   // empty
#elif defined(WIN32)
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
#if defined(__BEOS__) || defined(__HAIKU__)
   // empty
#elif defined(TARGET_PLATFORM_XENOMAI) && !defined(MUSCLE_AVOID_XENOMAI)
   // empty
#elif defined(WIN32)
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
/** Gotta do a custom data structure because we can't use the standard new/delete/muscleAlloc()/muscleFree() memory operators,
  * because to do so would cause an infinite regress if they call Mutex::Lock() or Mutex::Unlock() (which they do)
  */
class MutexEventLog
{
public:
   // Note:  You MUST call Initialize() after creating a MutexEventLog object!
   MutexEventLog() {/* empty */}

   void Initialize(const muscle_thread_id & id) {_threadID = id; _headBlock = _tailBlock = NULL;}

   void AddEvent(bool isLock, const void * mutexPtr, const char * fileName, int fileLine)
   {
      if ((_tailBlock == NULL)||(_tailBlock->IsFull()))
      {
         MutexEventBlock * newBlock = static_cast<MutexEventBlock *>(malloc(sizeof(MutexEventBlock)));  // THIS LINE CAN ONLY CALL plain old malloc() and nothing else!!!
         if (newBlock)
         {
            newBlock->Initialize();
            if (_headBlock == NULL) _headBlock = newBlock;
            if (_tailBlock) _tailBlock->_nextBlock = newBlock;
            _tailBlock = newBlock;
         }
         else 
         {
            printf("MutexEventLog::AddEvent():  malloc() failed!\n");   // what else to do?  Even MWARN_OUT_OF_MEMORY isn't safe here
            return;
         }
      }
      if (_tailBlock) _tailBlock->AddEvent(isLock, mutexPtr, fileName, fileLine);
   }

   void PrintToStream() const
   {
      MutexEventBlock * meb = _headBlock;
      while(meb)
      {
         meb->PrintToStream(_threadID);
         meb = meb->_nextBlock;
      }
   }

private:
   class MutexEventBlock
   {
   public:
      MutexEventBlock() {/* empty */}

      void Initialize() {_validCount = 0; _nextBlock = NULL;}
      bool IsFull() const {return (_validCount == ARRAYITEMS(_events));}
      void AddEvent(bool isLock, const void * mutexPtr, const char * fileName, int fileLine) {_events[_validCount++] = MutexEvent(isLock, mutexPtr, fileName, fileLine);}
      void PrintToStream(const muscle_thread_id & tid) const {for (uint32 i=0; i<_validCount; i++) _events[i].PrintToStream(tid);}

   private:
      friend class MutexEventLog;

      class MutexEvent
      {
      public:
         MutexEvent() {/* empty */}

         MutexEvent(bool isLock, const void * mutexPtr, const char * fileName, uint32 fileLine) : _fileLine(fileLine | (isLock?(1L<<31):0)), _mutexPtr(mutexPtr)
         {
            const char * lastSlash = strrchr(fileName, '/');
            if (lastSlash) fileName = lastSlash+1;

            muscleStrncpy(_fileName, fileName, sizeof(_fileName));
            _fileName[sizeof(_fileName)-1] = '\0';
         }

         void PrintToStream(const muscle_thread_id & threadID) const
         {
            char buf[20];
            printf("%s: tid=%s m=%p loc=%s:" UINT32_FORMAT_SPEC "\n", (_fileLine&(1L<<31))?"mx_lock":"mx_unlk", threadID.ToString(buf), _mutexPtr, _fileName, (uint32)(_fileLine&~(1L<<31)));
         }

      private: 
         uint32 _fileLine; 
         const void * _mutexPtr;
         char _fileName[48];
      };

      MutexEventBlock * _nextBlock;
      uint32 _validCount;
      MutexEvent _events[4096];
   };

   muscle_thread_id _threadID;
   MutexEventBlock * _headBlock;
   MutexEventBlock * _tailBlock;
};

static ThreadLocalStorage<MutexEventLog> _mutexEventsLog(false);  // false argument is necessary otherwise we can't read the threads' logs after they've gone away!
static Mutex _mutexLogTableMutex;
static Queue<MutexEventLog *> _mutexLogTable;  // read at process-shutdown time (I use a Queue rather than a Hashtable because muscle_thread_id isn't usable as a Hashtable key)

void DeadlockFinder_LogEvent(bool isLock, const void * mutexPtr, const char * fileName, int fileLine)
{
   MutexEventLog * mel = _mutexEventsLog.GetThreadLocalObject();
   if (mel == NULL)
   {
      mel = static_cast<MutexEventLog *>(malloc(sizeof(MutexEventLog)));  // MUST CALL malloc() here to avoid inappropriate re-entrancy!
      if (mel)
      {
         mel->Initialize(muscle_thread_id::GetCurrentThreadID());
         _mutexEventsLog.SetThreadLocalObject(mel);
         if (_mutexLogTableMutex.Lock().IsOK())
         {
            _mutexLogTable.AddTail(mel);
            _mutexLogTableMutex.Unlock();
         }
      }
   }
   if (mel) mel->AddEvent(isLock, mutexPtr, fileName, fileLine);
       else printf("DeadlockFinder_LogEvent:  malloc failed!?\n");  // we can't even call MWARN_OUT_OF_MEMORY here
}

static void DeadlockFinder_ProcessEnding()
{
   MutexGuard mg(_mutexLogTableMutex);
   for (uint32 i=0; i<_mutexLogTable.GetNumItems(); i++) _mutexLogTable[i]->PrintToStream();
}

#endif

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
     DeadlockFinder_ProcessEnding();
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
   CRYPTO_set_locking_callback(openssl_locking_function);
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
      ERR_load_BIO_strings();
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
#if defined(__BEOS__) || defined(__HAIKU__)
   return system_time();
#elif defined(TARGET_PLATFORM_XENOMAI) && !defined(MUSCLE_AVOID_XENOMAI)
   return rt_timer_tsc2ns(rt_timer_tsc())/1000;
#elif defined(WIN32)
   uint64 ret = 0;
   if (_rtMutex.Lock().IsOK())
   {
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
      _rtMutex.Unlock();
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
#elif defined(MUSCLE_USE_LIBRT) && defined(_POSIX_MONOTONIC_CLOCK)
   struct timespec ts;
   return (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) ? (SecondsToMicros(ts.tv_sec)+NanosToMicros(ts.tv_nsec)) : 0; 
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
      if (_rtMutex.Lock().IsOK())
      {
         static uint32 _prevVal;
         static uint64 _wrapOffset = 0;
         
         struct tms junk = {0}; clock_t newTicks = (clock_t) times(&junk);
         const uint32 newVal = (uint32) newTicks;
         if (newVal < _prevVal) _wrapOffset += (((uint64)1)<<32);
         const uint64 ret = ((_wrapOffset+newVal)*MICROS_PER_SECOND)/_posixTicksPerSecond;  // convert to microseconds
         _prevVal = newTicks;

         _rtMutex.Unlock();
         return ret;
      }
      else return 0;  // Oops?
   }
#endif
}

static int64 _perProcessRunTimeOffset = 0;
uint64 GetRunTime64() {return GetRunTime64Aux()+_perProcessRunTimeOffset;}
void SetPerProcessRunTime64Offset(int64 offset) {_perProcessRunTimeOffset = offset;}
int64 GetPerProcessRunTime64Offset() {return _perProcessRunTimeOffset;}

#if !(defined(__BEOS__) || defined(__HAIKU__))
status_t Snooze64(uint64 micros)
{
   if (micros == MUSCLE_TIME_NEVER) 
   {
      while(Snooze64(DaysToMicros(1)).IsOK()) {/* empty */}
      return B_ERROR;  // we should never exit the while loop above; so if we got here, it's an error
   }

#if __ATHEOS__
   return (snooze(micros) >= 0) ? B_NO_ERROR : B_ERRNO;
#elif WIN32
   Sleep((DWORD)((micros/1000)+(((micros%1000)!=0)?1:0)));
   return B_NO_ERROR;
#elif defined(MUSCLE_USE_LIBRT) && defined(_POSIX_MONOTONIC_CLOCK) && !defined(__EMSCRIPTEN__)
   const struct timespec ts = {(time_t) MicrosToSeconds(micros), (time_t) MicrosToNanos(micros%MICROS_PER_SECOND)};
   return (clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL) == 0) ? B_NO_ERROR : B_ERRNO;
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

#endif  /* !__BEOS__ && !__HAIKU__ */

/** Defined here since every MUSCLE program will have to include this file anyway... */
uint64 GetCurrentTime64(uint32 timeType)
{
#ifdef WIN32
   FILETIME ft;
   GetSystemTimeAsFileTime(&ft);
   if (timeType == MUSCLE_TIMEZONE_LOCAL) (void) FileTimeToLocalFileTime(&ft, &ft);
   return __Win32FileTimeToMuscleTime(ft);
#else
# if defined(__BEOS__) || defined(__HAIKU__)
   uint64 ret = real_time_clock_usecs();
# else
   struct timeval tv;
   gettimeofday(&tv, NULL);
   uint64 ret = ConvertTimeValTo64(tv);
# endif
   if (timeType == MUSCLE_TIMEZONE_LOCAL)
   {
      time_t now = time(NULL);
# if defined(__BEOS__) && !defined(__HAIKU__)
      struct tm * tm = gmtime(&now);
# else
      struct tm gmtm;
      struct tm * tm = gmtime_r(&now, &gmtm);
# endif
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
   
   if (m) m->Unlock();
}

AbstractObjectRecycler :: ~AbstractObjectRecycler()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   // Remove us from the linked list
   if (_prev) _prev->_next = _next;
   if (_next) _next->_prev = _prev;
   if (_firstRecycler == this) _firstRecycler = _next;

   if (m) m->Unlock();
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
   if (m) m->Unlock();
}

void AbstractObjectRecycler :: GlobalPrintRecyclersToStream()
{
   Mutex * m = GetGlobalMuscleLock();
   if ((m)&&(m->Lock().IsError())) m = NULL;

   const AbstractObjectRecycler * r = _firstRecycler;
   while(r) 
   {
      r->PrintToStream();
      r = r->_next;
   }

   if (m) m->Unlock();
}

static CompleteSetupSystem * _activeCSS = NULL;
CompleteSetupSystem * CompleteSetupSystem :: GetCurrentCompleteSetupSystem() {return _activeCSS;}

CompleteSetupSystem :: CompleteSetupSystem(bool muscleSingleThreadOnly)
   : _threads(muscleSingleThreadOnly)
   , _prevInstance(_activeCSS)
   , _initialMemoryUsage((size_t) GetProcessMemoryUsage())
{
   _activeCSS = this;  // push us onto the stack
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

   _activeCSS = _prevInstance;  // pop us off the stack
}

uint32 DataIO :: WriteFully(const void * buffer, uint32 size)
{
   const uint8 * b = (const uint8 *)buffer;
   const uint8 * firstInvalidByte = b+size;
   while(b < firstInvalidByte)
   {
      const int32 bytesWritten = Write(b, (uint32)(firstInvalidByte-b));
      if (bytesWritten <= 0) break;
      b += bytesWritten;
   }
   return (uint32) (b-((const uint8 *)buffer));
}

uint32 DataIO :: ReadFully(void * buffer, uint32 size)
{
   uint8 * b = (uint8 *) buffer;
   uint8 * firstInvalidByte = b+size;
   while(b < firstInvalidByte)
   {
      const int32 bytesRead = Read(b, (uint32) (firstInvalidByte-b));
      if (bytesRead <= 0) break;
      b += bytesRead;
   }
   return (uint32) (b-((const uint8 *)buffer));
}

int64 SeekableDataIO :: GetLength()
{
   const int64 origPos = GetPosition();
   if ((origPos >= 0)&&(Seek(0, IO_SEEK_END).IsOK()))
   {
      const int64 ret = GetPosition();
      if (Seek(origPos, IO_SEEK_SET).IsOK()) return ret;
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
      muscleCopyOut(b, B_HOST_TO_LENDIAN_INT32(fs));
      Flatten(b+sizeof(uint32));
   }
   else Flatten(b);

   // And finally, write out the buffer
   const status_t ret = (outputStream.WriteFully(b, bufSize) == bufSize) ? B_NO_ERROR : B_IO_ERROR;
   delete [] bigBuf;
   return ret;
}

status_t Flattenable :: UnflattenFromDataIO(DataIO & inputStream, int32 optReadSize, uint32 optMaxReadSize)
{
   uint32 readSize = (uint32) optReadSize;
   if (optReadSize < 0)
   {
      uint32 leSize;
      if (inputStream.ReadFully(&leSize, sizeof(leSize)) != sizeof(leSize)) return B_IO_ERROR;
      readSize = (uint32) B_LENDIAN_TO_HOST_INT32(leSize);
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

   const status_t ret = (inputStream.ReadFully(b, readSize) == readSize) ? Unflatten(b, readSize) : B_IO_ERROR;
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
   copyFrom.Flatten(bigBuf ? bigBuf : smallBuf);
   const status_t ret = Unflatten(bigBuf ? bigBuf : smallBuf, flatSize);
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

#if defined(WIN32) || defined(BEOS_OLD_NETSERVER)
      ::closesocket(fd);
#else
      close(fd);
#endif
   }
}

const ConstSocketRef & GetInvalidSocket()
{
   static const ConstSocketRef _ref(&GetDefaultObjectForType<Socket>(), false);
   return _ref;
}

ConstSocketRef GetConstSocketRefFromPool(int fd, bool okayToClose, bool returnNULLOnInvalidFD)
{
   static ConstSocketRef::ItemPool _socketPool;

   if ((fd < 0)&&(returnNULLOnInvalidFD)) return ConstSocketRef();
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
      }
      else if (okayToClose) CloseSocket(fd);

      return ret;
   }
}

Socket :: ~Socket()
{
   Clear();
}

void Socket :: SetFileDescriptor(int newFD, bool okayToClose)
{
   if (newFD != _fd)
   {
      if (_okayToClose) CloseSocket(_fd);  // CloseSocket(-1) is a no-op, so no need to check fd twice
      _fd = newFD; 
   }
   _okayToClose = okayToClose;
}

static void FlushStringAsciiChars(String & s, int idx, char * ascBuf, char * hexBuf, uint32 count, uint32 numColumns)
{
   while(count<numColumns) ascBuf[count++] = ' ';
   ascBuf[count] = '\0';
   char tempBuf[32]; muscleSprintf(tempBuf, "%04i: ", idx); s += tempBuf;
   s += ascBuf;
   s += " [";
   s += hexBuf;
   s += "]\n";
   hexBuf[0] = '\0';
}

static void FlushAsciiChars(FILE * file, int idx, char * ascBuf, char * hexBuf, uint32 count, uint32 numColumns)
{
   while(count<numColumns) ascBuf[count++] = ' ';
   ascBuf[count] = '\0';
   fprintf(file, "%04i: %s [%s]\n", idx, ascBuf, hexBuf);
   hexBuf[0] = '\0';
}

static void FlushLogAsciiChars(int lvl, int idx, char * ascBuf, char * hexBuf, uint32 count, uint32 numColumns)
{
   while(count<numColumns) ascBuf[count++] = ' ';
   ascBuf[count] = '\0';
   LogTime(lvl, "%04i: %s [%s]\n", idx, ascBuf, hexBuf);
   hexBuf[0] = '\0';
}

void PrintHexBytes(const void * vbuf, uint32 numBytes, const char * optDesc, uint32 numColumns, FILE * optFile)
{
   if (optFile == NULL) optFile = stdout;

   const uint8 * buf = (const uint8 *) vbuf;

   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) fprintf(optFile, "%s: ", optDesc);
      fprintf(optFile, "[");
      if (buf) for (uint32 i=0; i<numBytes; i++) fprintf(optFile, "%s%02x", (i==0)?"":" ", buf[i]);
          else fprintf(optFile, "NULL buffer");
      fprintf(optFile, "]\n");
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      fprintf(optFile, "%s", headBuf);

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) fputc('-', optFile);
      fputc('\n', optFile);
      if (buf)
      {
         char * ascBuf = newnothrow_array(char, numColumns+1);
         char * hexBuf = newnothrow_array(char, hexBufSize);
         if ((ascBuf)&&(hexBuf))
         {
            ascBuf[0] = hexBuf[0] = '\0';

            uint32 idx = 0;
            while(idx<numBytes)
            {
               const uint8 c = buf[idx];
               ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
               const size_t hexBufLen = strlen(hexBuf);
               muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
               idx++;
               if ((idx%numColumns) == 0) FlushAsciiChars(optFile, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
            }
            const uint32 leftovers = (numBytes%numColumns);
            if (leftovers > 0) FlushAsciiChars(optFile, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
         }
         else MWARN_OUT_OF_MEMORY;

         delete [] ascBuf;
         delete [] hexBuf;
      }
      else fprintf(optFile, "NULL buffer\n");
   }
}

void PrintHexBytes(const ByteBuffer & bb, const char * optDesc, uint32 numColumns, FILE * optFile)
{
   PrintHexBytes(bb.GetBuffer(), bb.GetNumBytes(), optDesc, numColumns, optFile);
}

void PrintHexBytes(const ConstByteBufferRef & bbRef, const char * optDesc, uint32 numColumns, FILE * optFile)
{
   PrintHexBytes(bbRef()?bbRef()->GetBuffer():NULL, bbRef()?bbRef()->GetNumBytes():0, optDesc, numColumns, optFile);
}

void PrintHexBytes(const Queue<uint8> & buf, const char * optDesc, uint32 numColumns, FILE * optFile)
{
   if (optFile == NULL) optFile = stdout;

   const uint32 numBytes = buf.GetNumItems();
   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) fprintf(optFile, "%s: ", optDesc);
      fprintf(optFile, "[");
      for (uint32 i=0; i<numBytes; i++) fprintf(optFile, "%s%02x", (i==0)?"":" ", (unsigned int) buf[i]);
      fprintf(optFile, "]\n");
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      fprintf(optFile, "%s", headBuf);

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) fputc('-', optFile);
      fputc('\n', optFile);
      char * ascBuf = newnothrow_array(char, numColumns+1);
      char * hexBuf = newnothrow_array(char, hexBufSize);
      if ((ascBuf)&&(hexBuf))
      {
         ascBuf[0] = hexBuf[0] = '\0';

         uint32 idx = 0;
         while(idx<numBytes)
         {
            const uint8 c = buf[idx];
            ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
            const size_t hexBufLen = strlen(hexBuf);
            muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
            idx++;
            if ((idx%numColumns) == 0) FlushAsciiChars(optFile, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
         }
         const uint32 leftovers = (numBytes%numColumns);
         if (leftovers > 0) FlushAsciiChars(optFile, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
      }
      else MWARN_OUT_OF_MEMORY;

      delete [] ascBuf;
      delete [] hexBuf;
   }
}

void LogHexBytes(int logLevel, const void * vbuf, uint32 numBytes, const char * optDesc, uint32 numColumns)
{
   const uint8 * buf = (const uint8 *) vbuf;

   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) LogTime(logLevel, "%s: ", optDesc);
      Log(logLevel, "[");
      if (buf) for (uint32 i=0; i<numBytes; i++) Log(logLevel, "%s%02x", (i==0)?"":" ", buf[i]);
          else Log(logLevel, "NULL buffer");
      Log(logLevel, "]\n");
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      LogTime(logLevel, "%s", headBuf);

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) Log(logLevel, "-");
      Log(logLevel, "\n");
      if (buf)
      {
         char * ascBuf = newnothrow_array(char, numColumns+1);
         char * hexBuf = newnothrow_array(char, hexBufSize);
         if ((ascBuf)&&(hexBuf))
         {
            ascBuf[0] = hexBuf[0] = '\0';

            uint32 idx = 0;
            while(idx<numBytes)
            {
               const uint8 c = buf[idx];
               ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
               const size_t hexBufLen = strlen(hexBuf);
               muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
               idx++;
               if ((idx%numColumns) == 0) FlushLogAsciiChars(logLevel, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
            }
            const uint32 leftovers = (numBytes%numColumns);
            if (leftovers > 0) FlushLogAsciiChars(logLevel, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
         }
         else MWARN_OUT_OF_MEMORY;

         delete [] ascBuf;
         delete [] hexBuf;
      }
      else LogTime(logLevel, "NULL buffer\n");
   }
}

void LogHexBytes(int logLevel, const Queue<uint8> & buf, const char * optDesc, uint32 numColumns)
{
   const uint32 numBytes = buf.GetNumItems();
   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) LogTime(logLevel, "%s: ", optDesc);
      Log(logLevel, "[");
      for (uint32 i=0; i<numBytes; i++) Log(logLevel, "%s%02x", (i==0)?"":" ", buf[i]);
      Log(logLevel, "]\n");
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      Log(logLevel, "%s", headBuf);

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) Log(logLevel, "-");
      Log(logLevel, "\n");
      char * ascBuf = newnothrow_array(char, numColumns+1);
      char * hexBuf = newnothrow_array(char, hexBufSize);
      if ((ascBuf)&&(hexBuf))
      {
         ascBuf[0] = hexBuf[0] = '\0';

         uint32 idx = 0;
         while(idx<numBytes)
         {
            const uint8 c = buf[idx];
            ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
            const size_t hexBufLen = strlen(hexBuf);
            muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
            idx++;
            if ((idx%numColumns) == 0) FlushLogAsciiChars(logLevel, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
         }
         const uint32 leftovers = (numBytes%numColumns);
         if (leftovers > 0) FlushLogAsciiChars(logLevel, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
      }
      else MWARN_OUT_OF_MEMORY;

      delete [] ascBuf;
      delete [] hexBuf;
   }
}

void LogHexBytes(int logLevel, const ByteBuffer & bb, const char * optDesc, uint32 numColumns)
{
   LogHexBytes(logLevel, bb.GetBuffer(), bb.GetNumBytes(), optDesc, numColumns);
}

void LogHexBytes(int logLevel, const ConstByteBufferRef & bbRef, const char * optDesc, uint32 numColumns)
{
   LogHexBytes(logLevel, bbRef()?bbRef()->GetBuffer():NULL, bbRef()?bbRef()->GetNumBytes():0, optDesc, numColumns);
}

String HexBytesToAnnotatedString(const void * vbuf, uint32 numBytes, const char * optDesc, uint32 numColumns)
{
   String ret;

   const uint8 * buf = (const uint8 *) vbuf;
   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) {ret += optDesc; ret += ": ";}
      ret += '[';
      if (buf) for (uint32 i=0; i<numBytes; i++) {char zbuf[32]; muscleSprintf(zbuf, "%s%02x", (i==0)?"":" ", buf[i]); ret += zbuf;}
          else ret += "NULL buffer";
      ret += ']';
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      ret += headBuf;

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) ret += '-';
      ret += '\n';
      if (buf)
      {
         char * ascBuf = newnothrow_array(char, numColumns+1);
         char * hexBuf = newnothrow_array(char, hexBufSize);
         if ((ascBuf)&&(hexBuf))
         {
            ascBuf[0] = hexBuf[0] = '\0';

            uint32 idx = 0;
            while(idx<numBytes)
            {
               const uint8 c = buf[idx];
               ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
               const size_t hexBufLen = strlen(hexBuf);
               muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
               idx++;
               if ((idx%numColumns) == 0) FlushStringAsciiChars(ret, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
            }
            const uint32 leftovers = (numBytes%numColumns);
            if (leftovers > 0) FlushStringAsciiChars(ret, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
         }
         else MWARN_OUT_OF_MEMORY;

         delete [] ascBuf;
         delete [] hexBuf;
      }
      else ret += "NULL buffer";
   }
   return ret;
}

String HexBytesToAnnotatedString(const Queue<uint8> & buf, const char * optDesc, uint32 numColumns)
{
   String ret;

   const uint32 numBytes = buf.GetNumItems();
   if (numColumns == 0)
   {
      // A simple, single-line format
      if (optDesc) {ret += optDesc; ret += ": ";}
      ret += '[';
      for (uint32 i=0; i<numBytes; i++) {char xbuf[32]; muscleSprintf(xbuf, "%s%02x", (i==0)?"":" ", (unsigned int) buf[i]); ret += xbuf;}
      ret += ']';
   }
   else
   {
      // A more useful columnar format with ASCII sidebar
      char headBuf[256]; 
      muscleSprintf(headBuf, "--- %s (" UINT32_FORMAT_SPEC " bytes): ", ((optDesc)&&(strlen(optDesc)<200))?optDesc:"", numBytes);
      ret += headBuf;

      const int hexBufSize = (numColumns*8)+1;
      const int numDashes = 8+(4*numColumns)-(int)strlen(headBuf);
      for (int i=0; i<numDashes; i++) ret += '-';
      ret += '\n';
      char * ascBuf = newnothrow_array(char, numColumns+1);
      char * hexBuf = newnothrow_array(char, hexBufSize);
      if ((ascBuf)&&(hexBuf))
      {
         ascBuf[0] = hexBuf[0] = '\0';

         uint32 idx = 0;
         while(idx<numBytes)
         {
            const uint8 c = buf[idx];
            ascBuf[idx%numColumns] = muscleInRange(c,(uint8)' ',(uint8)'~')?c:'.';
            const size_t hexBufLen = strlen(hexBuf);
            muscleSnprintf(hexBuf+hexBufLen, hexBufSize-hexBufLen, "%s%02x", ((idx%numColumns)==0)?"":" ", (unsigned int)(((uint32)buf[idx])&0xFF));
            idx++;
            if ((idx%numColumns) == 0) FlushStringAsciiChars(ret, idx-numColumns, ascBuf, hexBuf, numColumns, numColumns);
         }
         const uint32 leftovers = (numBytes%numColumns);
         if (leftovers > 0) FlushStringAsciiChars(ret, numBytes-leftovers, ascBuf, hexBuf, leftovers, numColumns);
      }
      else MWARN_OUT_OF_MEMORY;

      delete [] ascBuf;
      delete [] hexBuf;
   }
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
   while((--s >= str)&&(ParseHexDigit(*s) >= 0))
   {
      ret  += base * ((uint64)ParseHexDigit(*s));
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
#define MURMUR2_MIX(h,k,m) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }
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
         }

         uint32 k = (t >> sr) | (d << sl);
         MURMUR2_MIX(h,k,m);

         data += align;
         numBytes -= align;

         //----------
         // Handle tail bytes
         switch(numBytes)
         {
            case 3: h ^= data[2] << 16; // fall through!
            case 2: h ^= data[1] << 8;  // fall through!
            case 1: h ^= data[0];       // fall through!
                    h *= m;
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

ObjectCounterBase :: ObjectCounterBase()
   : _prevCounter(NULL)
   , _nextCounter(NULL)
{
   if (_muscleLock)
   {
      MutexGuard mg(*_muscleLock);
      PrependObjectCounterBaseToGlobalCountersList();
   }
   else PrependObjectCounterBaseToGlobalCountersList();
}

ObjectCounterBase :: ~ObjectCounterBase()
{
   if (_muscleLock)
   {
      MutexGuard mg(*_muscleLock);
      RemoveObjectCounterBaseFromGlobalCountersList();
   }
   else RemoveObjectCounterBaseFromGlobalCountersList();
}

#endif

status_t GetCountedObjectInfo(Hashtable<const char *, uint32> & results)
{
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   Mutex * m = _muscleLock;
   if ((m==NULL)||(m->Lock().IsOK()))
   {
      status_t ret;

      const ObjectCounterBase * oc = _firstObjectCounter;
      while(oc)
      {
         (void) results.Put(oc->GetCounterTypeName(), oc->GetCount()).IsError(ret);
         oc = oc->GetNextCounter();
      }

      if (m) m->Unlock();
      return ret;
   }
   else return B_LOCK_FAILED;
#else
   (void) results;
   return B_UNIMPLEMENTED;
#endif
}

void PrintCountedObjectInfo()
{
#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   Hashtable<const char *, uint32> table;
   if (GetCountedObjectInfo(table).IsOK())
   {
      table.SortByKey();  // so they'll be printed in alphabetical order
      printf("Counted Object Info report follows: (" UINT32_FORMAT_SPEC " types counted)\n", table.GetNumItems());
      for (HashtableIterator<const char *, uint32> iter(table); iter.HasData(); iter++) printf("   %6" UINT32_FORMAT_SPEC_NOPERCENT " %s\n", iter.GetValue(), iter.GetKey());
   }
   else printf("PrintCountedObjectInfo:  GetCountedObjectInfo() failed!\n");
#else
   printf("Counted Object Info report not available, because MUSCLE was compiled without -DMUSCLE_ENABLE_OBJECT_COUNTING\n");
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
   q.AddTail("MUSCLE_ENABLE_SSL");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   q.AddTail("MUSCLE_AVOID_CPLUSPLUS11");
#endif

#ifdef MUSCLE_USE_CPLUSPLUS17
   q.AddTail("MUSCLE_USE_CPLUSPLUS17");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11_THREADS
   q.AddTail("MUSCLE_AVOID_CPLUSPLUS11_THREADS");
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD
   q.AddTail("MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD");
#endif

#ifdef MUSCLE_AVOID_IPV6
   q.AddTail("MUSCLE_AVOID_IPV6");
#endif

#ifdef MUSCLE_AVOID_STDINT
   q.AddTail("MUSCLE_AVOID_STDINT");
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY 
   q.AddTail("MUSCLE_SINGLE_THREAD_ONLY");
#endif

#ifdef MUSCLE_USE_EPOLL
   q.AddTail("MUSCLE_USE_EPOLL");
#endif

#ifdef MUSCLE_USE_POLL
   q.AddTail("MUSCLE_USE_POLL");
#endif

#ifdef MUSCLE_USE_KQUEUE
   q.AddTail("MUSCLE_USE_KQUEUE");
#endif

#ifdef MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS
   q.AddTail(String("MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS=%1").Arg(MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS));
#endif

#ifdef MUSCLE_CATCH_SIGNALS_BY_DEFAULT
   q.AddTail("MUSCLE_CATCH_SIGNALS_BY_DEFAULT");
#endif

#ifdef MUSCLE_USE_LIBRT
   q.AddTail("MUSCLE_USE_LIBRT");
#endif

#ifdef MUSCLE_AVOID_MULTICAST_API
   q.AddTail("MUSCLE_AVOID_MULTICAST_API");
#endif

#ifdef MUSCLE_DISABLE_KEEPALIVE_API
   q.AddTail("MUSCLE_DISABLE_KEEPALIVE_API");
#endif

#ifdef MUSCLE_64_BIT_PLATFORM
   q.AddTail("MUSCLE_64_BIT_PLATFORM");
#endif

#ifdef MUSCLE_USE_LLSEEK
   q.AddTail("MUSCLE_USE_LLSEEK");
#endif

#ifdef MUSCLE_PREFER_QT_OVER_WIN32
   q.AddTail("MUSCLE_PREFER_QT_OVER_WIN32");
#endif

#ifdef MUSCLE_ENABLE_MEMORY_PARANOIA
   q.AddTail(String("MUSCLE_ENABLE_MEMORY_PARANOIA=%1").Arg(MUSCLE_ENABLE_MEMORY_PARANOIA));
#endif

#ifdef MUSCLE_NO_EXCEPTIONS 
   q.AddTail("MUSCLE_NO_EXCEPTIONS");
#endif

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING 
   q.AddTail("MUSCLE_ENABLE_MEMORY_TRACKING");
#endif

#ifdef MUSCLE_AVOID_ASSERTIONS 
   q.AddTail("MUSCLE_AVOID_ASSERTIONS");
#endif

#ifdef MUSCLE_AVOID_SIGNAL_HANDLING
   q.AddTail("MUSCLE_AVOID_SIGNAL_HANDLING");
#endif

#ifdef MUSCLE_AVOID_INLINE_ASSEMBLY
   q.AddTail("MUSCLE_AVOID_INLINE_ASSEMBLY");
#endif

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING 
   q.AddTail("MUSCLE_ENABLE_ZLIB_ENCODING");
#endif

#ifdef MUSCLE_TRACE_CHECKPOINTS
   q.AddTail(String("MUSCLE_TRACE_CHECKPOINTS=%1").Arg(MUSCLE_TRACE_CHECKPOINTS));
#endif

#ifdef MUSCLE_DISABLE_MESSAGE_FIELD_POOLS 
   q.AddTail("MUSCLE_DISABLE_MESSAGE_FIELD_POOLS");
#endif

#ifdef MUSCLE_INLINE_LOGGING 
   q.AddTail("MUSCLE_INLINE_LOGGING");
#endif

#ifdef MUSCLE_DISABLE_LOGGING 
   q.AddTail("MUSCLE_DISABLE_LOGGING");
#endif

#ifdef MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS 
   q.AddTail("MUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS");
#endif

#ifdef MUSCLE_MUTEX_POOL_SIZE
   q.AddTail(String("MUSCLE_MUTEX_POOL_SIZE").Arg(MUSCLE_MUTEX_POOL_SIZE));
#endif

#ifdef MUSCLE_POWERPC_TIMEBASE_HZ
   q.AddTail(String("MUSCLE_POWERPC_TIMEBASE_HZ=%1").Arg(MUSCLE_POWERPC_TIMEBASE_HZ));
#endif

#ifdef MUSCLE_USE_PTHREADS 
   q.AddTail("MUSCLE_USE_PTHREADS");
#endif

#ifdef MUSCLE_USE_PTHREADS 
   q.AddTail("MUSCLE_USE_CPLUSPLUS11_THREADS");
#endif

#ifdef MUSCLE_DEFAULT_TCP_STALL_TIMEOUT
   q.AddTail("MUSCLE_DEFAULT_TCP_STALL_TIMEOUT=%1").Arg(MUSCLE_DEFAULT_TCP_STALL_TIMEOUT);
#endif

#ifdef MUSCLE_FD_SETSIZE
   q.AddTail(String("MUSCLE_FD_SETSIZE=%1").Arg(MUSCLE_FD_SETSIZE));
#endif

#ifdef MUSCLE_AVOID_NEWNOTHROW 
   q.AddTail("MUSCLE_AVOID_NEWNOTHROW");
#endif

#ifdef MUSCLE_AVOID_FORKPTY 
   q.AddTail("MUSCLE_AVOID_FORKPTY");
#endif

#ifdef MUSCLE_HASHTABLE_DEFAULT_CAPACITY
   q.AddTail(String("MUSCLE_HASHTABLE_DEFAULT_CAPACITY=%1").Arg(MUSCLE_HASHTABLE_DEFAULT_CAPACITY));
#endif

#ifdef SMALL_QUEUE_SIZE
   q.AddTail(String("SMALL_QUEUE_SIZE=%1").Arg(SMALL_QUEUE_SIZE));
#endif

#ifdef SMALL_MUSCLE_STRING_LENGTH
   q.AddTail(String("SMALL_MUSCLE_STRING_LENGTH=%1").Arg(SMALL_MUSCLE_STRING_LENGTH));
#endif

#ifdef MUSCLE_USE_QUERYPERFORMANCECOUNTER
   q.AddTail("MUSCLE_USE_QUERYPERFORMANCECOUNTER");
#endif

#ifdef MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
   q.AddTail("MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME");
#endif

#ifdef MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS
   q.AddTail("MUSCLE_LOG_VERBOSE_SOURCE_LOCATIONS");
#endif

#ifdef MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS
   q.AddTail(String("MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS=%1").Arg(MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS));
#endif

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
   q.AddTail("MUSCLE_ENABLE_DEADLOCK_FINDER");
#endif

#ifdef MUSCLE_DEFAULT_RUNTIME_DISABLE_DEADLOCK_FINDER
   q.AddTail("MUSCLE_DEFAULT_RUNTIME_DISABLE_DEADLOCK_FINDER");
#endif

#ifdef MUSCLE_POOL_SLAB_SIZE
   q.AddTail(String("MUSCLE_POOL_SLAB_SIZE=%1").Arg(MUSCLE_POOL_SLAB_SIZE));
#endif

#ifdef MUSCLE_AVOID_BITSTUFFING
   q.AddTail("MUSCLE_AVOID_BITSTUFFING");
#endif

#ifdef MUSCLE_AVOID_CHECK_THREAD_STACK_USAGE
   q.AddTail("MUSCLE_AVOID_CHECK_THREAD_STACK_USAGE");
#endif

#ifdef MUSCLE_ENABLE_OBJECT_COUNTING
   q.AddTail("MUSCLE_ENABLE_OBJECT_COUNTING");
#endif

#ifdef MUSCLE_AVOID_THREAD_LOCAL_STORAGE
   q.AddTail("MUSCLE_AVOID_THREAD_LOCAL_STORAGE");
#endif

#ifdef MUSCLE_AVOID_MINIMIZED_HASHTABLES
   q.AddTail("MUSCLE_AVOID_MINIMIZED_HASHTABLES");
#endif

#ifdef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
   q.AddTail("MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS");
#endif

#ifdef MUSCLE_FAKE_SHARED_MEMORY
   q.AddTail("MUSCLE_FAKE_SHARED_MEMORY");
#endif

#ifdef MUSCLE_COUNT_STRING_COPY_OPERATIONS
   q.AddTail("MUSCLE_COUNT_STRING_COPY_OPERATIONS");
#endif

#ifdef MUSCLE_AVOID_XENOMAI
   q.AddTail("MUSCLE_AVOID_XENOMAI");
#endif

#ifdef DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD
   q.AddTail(String("DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD=%1").Arg(DEBUG_LARGE_MEMORY_ALLOCATIONS_THRESHOLD));
#endif

#ifdef MUSCLE_AVOID_AUTOCHOOSE_SWAP
   q.AddTail("MUSCLE_AVOID_AUTOCHOOSE_SWAP");
#endif

#ifdef MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS
   q.AddTail("MUSCLE_RECORD_REFCOUNTABLE_ALLOCATION_LOCATIONS");
#endif

#ifdef MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION
   q.AddTail("MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION");
#endif

#ifdef MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION
   q.AddTail("MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION");
#endif

#ifdef MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES
   q.AddTail("MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES");
#endif

#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
   q.AddTail("MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT");
#endif
   return q;
}

void LogBuildFlags(int logLevel)
{
   if (GetMaxLogLevel() >= logLevel)
   {
      Queue<String> flagStrs = GetBuildFlags();
      for (uint32 i=0; i<flagStrs.GetNumItems(); i++) LogTime(logLevel, "MUSCLE code was compiled with preprocessor flag -D%s\n", flagStrs[i]());
   }
}

void PrintBuildFlags()
{
   Queue<String> flagStrs = GetBuildFlags();
   for (uint32 i=0; i<flagStrs.GetNumItems(); i++) printf("MUSCLE code was compiled with preprocessor flag -D%s\n", flagStrs[i]());
}

uint64 GetProcessMemoryUsage()
{
#if defined(__linux__)
   FILE* fp = fopen("/proc/self/statm", "r");
   if (fp)
   {
      char buf[256];
      if (fgets(buf, sizeof(buf), fp) == NULL) buf[0] = '\0';
      fclose(fp);

      // skip past the first number (total program size) and return Rss
      const char * firstSpace = strchr(buf, ' ');
      if (firstSpace) return Atoull(firstSpace+1)*sysconf(_SC_PAGESIZE);
   }
#elif defined(__APPLE__)
   mach_task_basic_info_data_t taskinfo; memset(&taskinfo, 0, sizeof(taskinfo));
   mach_msg_type_number_t outCount = MACH_TASK_BASIC_INFO_COUNT;
   if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskinfo, &outCount) == KERN_SUCCESS) return taskinfo.resident_size;
#elif defined(WIN32) && !defined(__MINGW32__)
   PROCESS_MEMORY_COUNTERS pmc;
   if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return pmc.WorkingSetSize;
#endif
   return 0;
}

} // end namespace muscle
