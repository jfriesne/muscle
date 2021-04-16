/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/Thread.h"
#include "system/ThreadLocalStorage.h"
#include "system/SetupSystem.h"

using namespace muscle;

static ThreadLocalStorage<int> _tls; // just to test the ThreadLocalStorage class while we're at it

class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32)
   {
       // Some sanity checking
       if (this != Thread::GetCurrentThread()) printf("TestThread:  Error, GetCurrentThread() should return %p, actually returned %p\n", this, Thread::GetCurrentThread());
       if (IsCallerInternalThread() == false) printf("TestThread:  Error, IsCallerInternalThread() returned false!\n");

       const bool hasValue = (_tls.GetThreadLocalObject() != NULL);
       int * tls = _tls.GetOrCreateThreadLocalObject();
       MRETURN_OOM_ON_NULL(tls);

       if (hasValue == false) *tls = 12;
       if (msgRef()) {printf("threadTLS=%i: Internal thread saw: ", *tls); msgRef()->PrintToStream(); return B_NO_ERROR;}
                else {printf("threadTLS=%i: Internal thread exiting\n", *tls); return B_ERROR;}
   }
};

// This program exercises the Thread class.
int main(void) 
{
   CompleteSetupSystem css;

   int * tls = _tls.GetOrCreateThreadLocalObject();
   if (tls) *tls = 3;
       else MWARN_OUT_OF_MEMORY;

   TestThread t;
   printf("main thread: TestThread is %p (main thread is %p/%i)\n", &t, Thread::GetCurrentThread(), t.IsCallerInternalThread());

   status_t ret;
   if (t.SetThreadPriority(Thread::PRIORITY_LOWER).IsError(ret)) printf("Warning, SetThreadPriority(Thread::PRIORITY_LOWER) failed! [%s]\n", ret());  // just to see what happens

   if (t.StartInternalThread().IsOK())
   {
      char buf[256];
      while(fgets(buf, sizeof(buf), stdin))
      {
         if (buf[0] == 'q') break;
         MessageRef msg(GetMessageFromPool(1234));
         msg()->AddString("str", buf);
         t.SendMessageToInternalThread(msg);
      }
   }

   printf("Cleaning up (mainTLS=%i)...\n", *_tls.GetThreadLocalObject());  // make sure the TLS hasn't been changed by the other thread
   t.SendMessageToInternalThread(MessageRef());  // ask internal thread to shut down
   t.WaitForInternalThreadToExit();
   printf("Bye!\n");
   return 0;
}
