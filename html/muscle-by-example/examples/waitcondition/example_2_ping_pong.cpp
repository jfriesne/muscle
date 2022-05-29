#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/Thread.h"
#include "system/WaitCondition.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example program spawns two threads and lets them play a few rounds of ping-pong using Wait() and Notify()\n");
   printf("\n");
}

/** This class will just call Wait() on the supplied WaitCondition, and then exit after Wait() returns */
class MyThread : public Thread
{
public:
   /** Constructor
     * @param description a human-readable description of this thread
     * @param waitCondition we'll call Wait() on this WaitCondition
     * @param notifyCondition we'll call Notify() on this WaitCondition
     */
   MyThread(const char * description, const WaitCondition & waitCondition, const WaitCondition & notifyCondition)
      : _description(description)
      , _waitCondition(waitCondition)
      , _notifyCondition(notifyCondition)
      , _countdown(10)
   {
      /* empty */
   }

   // Public only so that main() can start the game by throwing out the first pitch
   void Notify()
   {
      const status_t ret = _notifyCondition.Notify();
      LogTime(MUSCLE_LOG_INFO, "MyThread [%s]:  Notify() returned [%s]\n", _description, ret());
   }

protected:
   virtual void InternalThreadEntry()
   {
      while(_countdown > 0)
      {
         LogTime(MUSCLE_LOG_INFO, "MyThread [%s] is now waiting inside Wait() of WaitCondition %p.\n", _description, &_waitCondition);

         status_t ret = _waitCondition.Wait();
         --_countdown;
         LogTime(MUSCLE_LOG_INFO, "MyThread [%s]:  Wait() returned [%s], reducing the countdown to %i and calling Notify()!\n", _description, ret(), _countdown);

         Notify();
      }
   }

private:
   const char * _description;
   const WaitCondition & _waitCondition;
   const WaitCondition & _notifyCondition;
   int _countdown;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   status_t ret;

   WaitCondition waitConditions[2];

   MyThread threadA("Thread A", waitConditions[0], waitConditions[1]);
   if (threadA.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, couldn't start the first internal thread!? [%s]\n", ret());
      return 10;
   }

   MyThread threadB("Thread B", waitConditions[1], waitConditions[0]);
   if (threadB.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Error, couldn't start the second internal thread!? [%s]\n", ret());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Main thread:  Calling Notify() on Thread A to start the ping-pong match!\n");
   threadA.Notify();

   (void) threadA.ShutdownInternalThread();
   (void) threadB.ShutdownInternalThread();

   LogTime(MUSCLE_LOG_INFO, "Main thread:  both threads have exited, ending program.\n");

   return 0;
}
