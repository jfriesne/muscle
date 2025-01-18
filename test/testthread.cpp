/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/Thread.h"
#include "system/ThreadLocalStorage.h"
#include "system/SetupSystem.h"

using namespace muscle;

static ThreadLocalStorage<int> _tls; // just to test the ThreadLocalStorage class while we're at it

class TestThread : public Thread
{
public:
   TestThread(bool useMessagingSockets) : Thread(useMessagingSockets) {/* empty */}

   virtual void InternalThreadEntry()
   {
      while(true)
      {
         MessageRef msgRef;
         uint32 numLeft = 0;
         if (WaitForNextMessageFromOwner(msgRef, GetRunTime64()+SecondsToMicros(2), &numLeft).IsOK())
         {
            if (MessageReceivedFromOwner(msgRef, numLeft).IsError()) break;
         }
         else printf("WaitForNextMessageFromOwner() timed out after 2 seconds\n");
      }
   }

   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32)
   {
       // Some sanity checking
       if (this != Thread::GetCurrentThread()) printf("TestThread:  Error, GetCurrentThread() should return %p, actually returned %p\n", this, Thread::GetCurrentThread());
       if (IsCallerInternalThread() == false) printf("TestThread:  Error, IsCallerInternalThread() returned false!\n");

       const bool hasValue = (_tls.GetThreadLocalObject() != NULL);
       int * tls = _tls.GetOrCreateThreadLocalObject();
       MRETURN_OOM_ON_NULL(tls);

       if (hasValue == false) *tls = 12;
       if (msgRef()) {printf("threadTLS=%i: Internal thread saw: ",     *tls); msgRef()->Print(); return B_NO_ERROR;}
                else {printf("threadTLS=%i: Internal thread exiting\n", *tls);                    return B_SHUTTING_DOWN;}
   }
};

// This program exercises the Thread class.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   int * tls = _tls.GetOrCreateThreadLocalObject();
   if (tls) *tls = 3;
       else MWARN_OUT_OF_MEMORY;

   const bool useWaitCondition = ((argc>1)&&(strcmp(argv[1], "usewaitcondition") == 0));

   TestThread t(!useWaitCondition);
   printf("main thread: TestThread(%s) is %p (main thread is %p/%i)\n", useWaitCondition?"waitCondition":"sockets", &t, Thread::GetCurrentThread(), t.IsCallerInternalThread());

   status_t ret;
   if (t.SetThreadPriority(Thread::PRIORITY_LOWER).IsError(ret)) printf("Warning, SetThreadPriority(Thread::PRIORITY_LOWER) failed! [%s]\n", ret());  // just to see what happens

   const bool isFromScript = ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0));

   if (t.StartInternalThread().IsOK())
   {
      if (isFromScript)
      {
         for (uint32 i=0; i<20; i++)
         {
            MessageRef msg(GetMessageFromPool(1234));
            (void) msg()->AddString("str", "howdy");
            (void) t.SendMessageToInternalThread(msg);
            (void) Snooze64(MillisToMicros(100));
         }
      }
      else
      {
         char buf[256];
         while(fgets(buf, sizeof(buf), stdin))
         {
            if (buf[0] == 'q') break;
            MessageRef msg(GetMessageFromPool(1234));
            (void) msg()->AddString("str", buf);
            (void) t.SendMessageToInternalThread(msg);
         }
      }
   }
   else return 10;

   printf("Cleaning up (mainTLS=%i)...\n", *_tls.GetThreadLocalObject());  // make sure the TLS hasn't been changed by the other thread
   (void) t.SendMessageToInternalThread(MessageRef());  // ask internal thread to shut down
   (void) t.WaitForInternalThreadToExit();
   printf("Bye!\n");
   return 0;
}
