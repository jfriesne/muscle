#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/Thread.h"
#include "system/WaitCondition.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example program demonstrates basic usage of the muscle::WaitCondition class to block a thread until Notify() is called.\n");
   printf("\n");
}

/** This class will just call Wait() on the supplied WaitCondition, and then exit after Wait() returns */
class MyThread : public Thread
{
public:
   MyThread(const WaitCondition & waitCondition) : _waitCondition(waitCondition) {/* empty */}

protected:
   virtual void InternalThreadEntry()
   {
      LogTime(MUSCLE_LOG_INFO, "MyThread %p is waiting inside WaitCondition::Wait() now.\n", this);

      const status_t ret = _waitCondition.Wait();
      LogTime(MUSCLE_LOG_INFO, "MyThread %p:  Wait() returned [%s], exiting now!\n", this, ret());
   }

private:
   const WaitCondition & _waitCondition;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   status_t ret;

   WaitCondition waitCondition;

   MyThread theThread(waitCondition);
   if (theThread.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, couldn't start the internal thread!? [%s]\n", ret());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Main thread: Press return to call Notify() on the WaitCondition...\n");

   char buf[128];
   if (fgets(buf, sizeof(buf), stdin) != NULL) {/* empty */}

   ret = waitCondition.Notify();
   LogTime(MUSCLE_LOG_INFO, "WaitCondition::Notify() returned [%s], now waiting for MyThread to exit.\n", ret());

   (void) theThread.ShutdownInternalThread();

   LogTime(MUSCLE_LOG_INFO, "Main thread:  MyThread has exited, ending program.\n");

   return 0;
}
