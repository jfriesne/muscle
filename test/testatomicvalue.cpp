/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/AtomicCounter.h"
#include "system/AtomicValue.h"
#include "system/Thread.h"
#include "system/SetupSystem.h"
#include "util/String.h"

using namespace muscle;

static AtomicCounter _pleaseExit;

//un-comment this to see what happens without a functioning AtomicValue<> object
//#define DO_CONTROL_GROUP_TEST 1

#ifdef DO_CONTROL_GROUP_TEST
// Just an API-equivalent class that doesn't do any special logic, so I can watch things go wrong
template<typename T, uint32 junk=1> class FakeAtomicValue MUSCLE_FINAL_CLASS
{
public:
   FakeAtomicValue() {}
   status_t SetValue(const T & val) {_val = val; return B_NO_ERROR;}
   T GetValue() const {return _val;}

private:
   T _val;
};
static FakeAtomicValue<String> _atomicValue;
#else
static AtomicValue<String> _atomicValue;
#endif

class AtomicWriterThread : public Thread
{
public:
   AtomicWriterThread() : Thread(false) {/* empty */}

   status_t GetStatus() const {return _status;}

   virtual void InternalThreadEntry()
   {
      String temp;

      uint32 count = 0;
      while(_pleaseExit.GetCount() == 0)
      {
         char buf[128]; muscleSprintf(buf, "TAVT %p:  " UINT32_FORMAT_SPEC, this, ++count);
         temp = buf;

         muscleSprintf(buf, " / " UINT32_FORMAT_SPEC, temp.HashCode());
         temp += buf;

         const status_t ret = _atomicValue.SetValue(temp);
         if (ret.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "AtomicWriterThread:   AtomicValue::SetValue(%s) failed! [%s]\n", temp(), ret());
            _status |= ret;
         }

         (void) Snooze64(MillisToMicros(1));  // otherwise we flood the zone with updates and cause problems
      }
   }

private:
   status_t _status;
};

class AtomicReaderThread : public Thread
{
public:
   AtomicReaderThread() : Thread(false) {/* empty */}

   status_t GetStatus() const {return _status;}

   virtual void InternalThreadEntry()
   {
      uint32 dupCount = 0, totalCount = 0;
      String prevString;
      while(_pleaseExit.GetCount() == 0)
      {
         const String s = _atomicValue.GetValue();

         totalCount++;
         if (s == prevString) dupCount++;
         prevString = s;

         if ((totalCount%10000)==0) printf("TestReaderThread:  Read string [%s] (%.04f%% are duplicate values)        \r", s(), 100.0f*(((float)dupCount)/totalCount));
         const char * slash = strstr(s(), " / ");
         if (slash)
         {
            const uint32 allegedHashCode = (uint32) Atoull(slash+3);
            const uint32 actualHashCode  = s.Substring(0, (uint32) (slash-s())).HashCode();
            if (allegedHashCode != actualHashCode)
            {
               LogTime(MUSCLE_LOG_ERROR, "AtomicReaderThread:  ERROR: Read string [%s], expected hash code " UINT32_FORMAT_SPEC ", computed hash code " UINT32_FORMAT_SPEC "\n", s(), allegedHashCode, actualHashCode);
               _status |= B_LOGIC_ERROR;
            }
         }
      }
   }

private:
   status_t _status;
};


// This program exercises the AtomicValue class.
int main(int argc, char ** argv)
{
   const bool isFromScript = ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0));

   CompleteSetupSystem css;

   AtomicWriterThread writerThread;
   AtomicReaderThread readerThread;

   LogTime(MUSCLE_LOG_INFO, "AtomicValue torture test running; will end after one minute.\n");

   status_t ret;
   if ((writerThread.StartInternalThread().IsOK(ret))&&(readerThread.StartInternalThread().IsOK(ret)))
   {
      (void) Snooze64(isFromScript ? SecondsToMicros(10) : MinutesToMicros(1));
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Error starting Atomic thread! [%s]\n", ret());

   _pleaseExit.AtomicIncrement();

   readerThread.ShutdownInternalThread();
   writerThread.ShutdownInternalThread();

   ret |= readerThread.GetStatus();
   ret |= writerThread.GetStatus();

   if (ret.IsOK())
   {
      LogTime(MUSCLE_LOG_INFO, "Test completed successfully, bye!\n");
      return 0;
   }
   else
   {
      LogTime(MUSCLE_LOG_INFO, "Test detected error [%s], bye!\n", ret());
      return 10;
   }
}
