/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/ThreadPool.h"
#include "system/SetupSystem.h"

using namespace muscle;

class TestClient : public IThreadPoolClient
{
public:
   TestClient()
      : IThreadPoolClient(NULL) 
   {
      // empty
   }

   virtual void MessageReceivedFromThreadPool(const MessageRef & msgRef, uint32 numLeft)
   {
      char buf[20];
      printf("MessageFromOwner called in thread %s, msgRef=%p (what=" UINT32_FORMAT_SPEC "), numLeft=" UINT32_FORMAT_SPEC "\n", muscle_thread_id::GetCurrentThreadID().ToString(buf), msgRef(), msgRef()?msgRef()->what:666, numLeft);
      Snooze64(200000);
   }
};

// This program exercises the Thread class.
int main(void) 
{
   CompleteSetupSystem css;

   printf("Creating pool...\n"); fflush(stdout);

   ThreadPool pool;
   {
      printf("Sending TestClient Messages to pool...\n"); fflush(stdout);
      TestClient tcs[10];
      for (uint32 i=0; i<ARRAYITEMS(tcs); i++) tcs[i].SetThreadPool(&pool);
      for (uint32 i=0; i<10; i++) 
      {
         for (uint32 j=0; j<ARRAYITEMS(tcs); j++) 
         {
            MessageRef msg = GetMessageFromPool((j*100)+i);
            msg()->AddString("hey", "dude");
            tcs[j].SendMessageToThreadPool(msg);
         }
      }

      printf("Waiting for Messages to complete...\n");
      for (uint32 i=0; i<ARRAYITEMS(tcs); i++) tcs[i].SetThreadPool(NULL);  // this will block until all tc's Messages have been handled
      printf("Messages completed!\n");
   }

   printf("Exiting, bye!\n");
   return 0;
}
