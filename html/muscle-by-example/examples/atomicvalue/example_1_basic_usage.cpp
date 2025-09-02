#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/AtomicValue.h"
#include "system/Thread.h"
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()
#include "util/String.h"

using namespace muscle;

//#define MAKE_IT_BUGGY 1   // uncomment this to see what happens when we don't use the AtomicValue functionality... big trouble!
#ifdef MAKE_IT_BUGGY
static String _sharedString;  // directly accessing this from multiple threads will cause lots of race conditions
#else
static AtomicValue<String> _sharedString;  // our shared global variable; note that unsynchronized access to a bare String would not be thread-safe!
#endif

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This little program demonstrates basic usage of the muscle::AtomicValue class.\n");
   p.printf("\n");
   p.printf("This program will spawn a number of threads that will each periodically read from\n");
   p.printf("and/or write to to a single global String variable with no Mutex locking.\n");
   p.printf("\n");
#ifdef MAKE_IT_BUGGY
   p.printf("This program has been compiled with the MAKE_IT_BUGGY line uncommented, so\n");
   p.printf("it will run without the AtomicValue class and invoke undefined behavior due to race conditions.\n");
#else
   p.printf("Normally this would cause race conditions (e.g. garbage output, maybe crashing)\n");
   p.printf("But due to the AtomicValue class's internal ring-buffer, the calls to SetValue()\n");
   p.printf("are actually modifying a different location in memory than the location being\n");
   p.printf("read from by the calls to GetValue(), so no such problems occur.\n");
   p.printf("\n");
   p.printf("To see how this program would misbehave without the AtomicBuffer class, edit this file\n");
   p.printf("(atomicvalue/example_1_basic_usage.cpp) and uncomment the MAKE_IT_BUGGY #define at line 9.\n");
#endif
   p.printf("\n");
   p.printf("This test will run for one minute.  A successful run is one that doesn't print any error messages.\n");
   p.printf("\n");
}

// Test thread that does a lot of reading from and writing to the global variable _sharedString
class AtomicValueTestThread : public Thread
{
public:
   AtomicValueTestThread()
      : _testEndTime(GetRunTime64()+MinutesToMicros(1))
      , _nextSetTime(0)
   {
      // empty
   }

   virtual void InternalThreadEntry()
   {
      while(1)
      {
         const uint64 now = GetRunTime64();
         if (now >= _testEndTime) break;

#ifdef MAKE_IT_BUGGY
         const String curVal = _sharedString;  // race condition:  Another thread might be writing from _sharedString while we read from it!
#else
         const String curVal = _sharedString.GetValue();
#endif
         if (curVal != _previousReadValue)
         {
            //printf("AtomicValueTestThread %p:  Shared String value changed from [%s] to [%s]\n", this, _previousReadValue(), curVal());

            // Validate the read value against its checksum, so we can see if it has been corrupted by a race condition
            const char * chk = strstr(curVal(), " checksum=");
            if (chk)
            {
               const uint32 chkVal = (uint32) Atoull(chk+10);
               const uint32 expectedChk = String(curVal(), chk-curVal()).CalculateChecksum();
               if (chkVal != expectedChk) LogTime(MUSCLE_LOG_ERROR, "Thread %p:  Read string value [%s] contains checksum " UINT32_FORMAT_SPEC ", expected checksum " UINT32_FORMAT_SPEC ", corruption detected!\n", this, curVal(), chkVal, expectedChk);
            }
            else LogTime(MUSCLE_LOG_ERROR, "Thread %p:  Read string value [%s] doesn't contain any checksum!\n", this, curVal());

            _previousReadValue = curVal;
         }

         if (now >= _nextSetTime)
         {
            char tmpBuf[128]; muscleSprintf(tmpBuf, "Thread %p value " UINT64_FORMAT_SPEC ":" UINT32_FORMAT_SPEC, this, now, GetInsecurePseudoRandomNumber());
            String newStr = tmpBuf;

            muscleSprintf(tmpBuf, " checksum=" UINT32_FORMAT_SPEC, newStr.CalculateChecksum());
            newStr += tmpBuf;

#ifdef MAKE_IT_BUGGY
            _sharedString = newStr;  // race condition!  Another thread might be reading from _sharedString while we write to it!
#else
            const status_t ret = _sharedString.SetValue(newStr);
            if (ret.IsError()) LogTime(MUSCLE_LOG_ERROR, "AtomicValue<String>::SetValue() returned [%s]\n", ret());
#endif

            // Note that we can only get away with calling SetValue() every so often
            _nextSetTime = now + MillisToMicros(GetInsecurePseudoRandomNumber()%50);
         }
      }
   }

private:
   const uint64 _testEndTime;
   uint64 _nextSetTime;

   String _previousReadValue;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   AtomicValueTestThread threads[10];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].StartInternalThread();
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) (void) threads[i].WaitForInternalThreadToExit();

   printf("AtomicValue example is now exiting, bye!\n");
   return 0;
}
