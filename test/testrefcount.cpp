/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "system/Thread.h"
#include "util/String.h"
#include "util/RefCount.h"
#include "util/Queue.h"
#include "util/Hashtable.h"
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

class TestItem : public RefCountable
{
public:
   TestItem()  {/* empty */}
   explicit TestItem(const String & name) {SetName(name);}
   ~TestItem() {_name = "Dead";}  // Just to make dead-item-usage detection a bit easier

   const String & GetName() const {return _name;}
   void SetName(const String & name) {_name = name;}

private:
   String _name;
};
DECLARE_REFTYPES(TestItem);
static TestItemRef::ItemPool _pool;

class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   virtual void InternalThreadEntry()
   {
      char buf[128]; muscleSprintf(buf, "TestThread-%p", this);
      const String prefix = buf;
      Queue<TestItemRef> q;
      bool keepGoing = 1;
      uint32 counter = 0;
      while(keepGoing)
      {
         const uint32 x = rand() % 10000;
         while(q.GetNumItems() < x) 
         {
            TestItemRef tRef(_pool.ObtainObject());
            if (tRef())
            {
               muscleSprintf(buf, "-" UINT32_FORMAT_SPEC, ++counter);
               tRef()->SetName(prefix+buf);
               q.AddTail(tRef);
            }
            else MWARN_OUT_OF_MEMORY; 
         }
         while(q.GetNumItems() > x) q.RemoveTail();

         // Check to make sure no other threads are overwriting our objects
         for (uint32 i=0; i<q.GetNumItems(); i++) 
         {
            if (q[i]()->GetName().StartsWith(prefix) == false)
            {
               printf("ERROR, thread %p expected prefix [%s], saw [%s] at position " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", this, prefix(), q[i]()->GetName()(), i, q.GetNumItems());
               ExitWithoutCleanup(10);
            }
         }
 
         // Check to see if the main thread wants us to exit yet
         MessageRef r;
         while(WaitForNextMessageFromOwner(r, 0) >= 0) if (r() == NULL) keepGoing = false;
      }
   }
};

// This program exercises the Ref class.
int main(void) 
{
   CompleteSetupSystem setupSystem;

   printf("sizeof(TestItemRef)=%i\n", (int)sizeof(TestItemRef));

   {
      printf("Checking queue...\n");
      Queue<TestItemRef> q;
      printf("Adding refs...\n");
      for (int i=0; i<10; i++) 
      {
         char temp[50]; muscleSprintf(temp, "%i", i);
         TestItemRef tr(new TestItem(temp));
         ConstTestItemRef ctr(tr);
         ConstTestItemRef t2(ctr);
         q.AddTail(tr);
      }
      printf("Removing refs...\n");
      while(q.HasItems()) q.RemoveHead();
      printf("Done with queue test!\n");
   }

   {
      printf("Checking hashtable\n");
      Hashtable<String, TestItemRef> ht;
      printf("Adding refs...\n");
      for (int i=0; i<10; i++) 
      {
         char temp[50]; muscleSprintf(temp, "%i", i);
         ht.Put(String(temp), TestItemRef(new TestItem(temp)));
      }
      printf("Removing refs...\n");
      for (int i=0; i<10; i++) 
      {
         char temp[50]; muscleSprintf(temp, "%i", i);
         ht.Remove(String(temp));
      }
      printf("Done with hash table test!\n");
   }
    
   printf("Beginning multithreaded object usage test...\n");
   {
      const uint32 NUM_THREADS = 50;
      TestThread threads[NUM_THREADS]; 
      for (uint32 i=0; i<NUM_THREADS; i++) threads[i].StartInternalThread();
      Snooze64(SecondsToMicros(10));
      for (uint32 i=0; i<NUM_THREADS; i++) threads[i].ShutdownInternalThread();
      printf("Multithreaded object usage test complete.\n");
   }

   printf("testrefcount complete, bye!\n");
   return 0;
}
