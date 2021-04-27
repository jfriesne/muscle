/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "message/Message.h"
#include "system/SetupSystem.h"
#include "system/Thread.h"
#include "util/Hashtable.h"
#include "util/MiscUtilityFunctions.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

int _state = 0;

static void bomb(const char * fmt, ...);
void bomb(const char * fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   vprintf(fmt, va);
   va_end(va);
   LogTime(MUSCLE_LOG_CRITICALERROR, "EXITING DUE TO ERROR (state = %i)!\n", _state);
   ExitWithoutCleanup(10);
}

// This class tests the thread-safety of multiple Threads iterating over the fields of a Message simultaneously.
class TestThread : public Thread
{
public:
   explicit TestThread(const MessageRef & msg)
      : _msg(msg) 
   {
      // empty
   }

protected:
   virtual void InternalThreadEntry()
   {
      uint32 totalCount = 0;
      const uint64 endTime = GetRunTime64()+SecondsToMicros(10);
      while(GetRunTime64()<endTime)
      {
         uint32 count = 0;
         for (MessageFieldNameIterator fnIter(*_msg(), B_ANY_TYPE); fnIter.HasData(); fnIter++) 
         {
            count++;
            totalCount++;
            if ((rand()%5)==0)
            {
               // Make sure re-entrant sub-iterations work correctly
               for (MessageFieldNameIterator fnIter2(*_msg(), B_ANY_TYPE); fnIter2.HasData(); fnIter2++) totalCount++;
            }
         }
         if (count != 100) printf("WTF " UINT32_FORMAT_SPEC "\n", count);
      }

      char buf[20];
      printf("totalCount=" UINT32_FORMAT_SPEC " for thread %s\n", totalCount, muscle_thread_id::GetCurrentThreadID().ToString(buf));
   }

private:
   MessageRef _msg;
};

static int DoThreadTest();
static int DoThreadTest()
{
   MessageRef testMsg = GetMessageFromPool(1234);
   for (int i=0; i<100; i++) testMsg()->AddInt32(String("field-%1").Arg(i), i);

   printf("BEGIN THREAD-SAFETY TEST!\n");

   const int NUM_THREADS=30;
   Thread * threads[NUM_THREADS];
   for (uint32 i=0; i<ARRAYITEMS(threads); i++)
   {
      threads[i] = new TestThread(testMsg);
      if (threads[i]->StartInternalThread().IsError()) printf("Error starting thread!\n");
   }
   for (uint32 i=0; i<ARRAYITEMS(threads); i++) 
   {
      threads[i]->WaitForInternalThreadToExit();
      delete threads[i];
   }

   printf("END THREAD-SAFETY TEST!\n");

   return 0;
}

static int DoInteractiveTest();
static int DoInteractiveTest()
{
   OrderedKeysHashtable<int, String> table;

   // Prepopulate the table, for convenience
   for (int i=9; i>=0; i--)
   {
      char buf[32];
      muscleSprintf(buf, "%i", i);
      table.Put(i, buf);
   }

   while(true)
   {
      {
         bool first = true;
         printf("\nCurrent contents: ");
         for (HashtableIterator<int, String> iter(table); iter.HasData(); iter++)
         {
            const String & nextValue = iter.GetValue();
            MASSERT((atoi(nextValue()) == iter.GetKey()), "value/key mismatch A!\n");
            printf("%s%i", first?"":", ", iter.GetKey());
            first = false;
         }
         printf(" (size=" UINT32_FORMAT_SPEC ")\nEnter command: ", table.GetNumItems()); fflush(stdout);
      }

      char buf[512];
      if (fgets(buf, sizeof(buf), stdin))
      {
         StringTokenizer tok(buf, NULL, " ");
         const char * arg0 = tok();
         const char * arg1 = tok(); 
         const char * arg2 = tok(); 

         char command = arg0 ? arg0[0] : '\0';

         // For extra fun, let's put a half-way-done iterator here to see what happens
         printf("Concurrent: ");
         HashtableIterator<int, String> iter(table);
         bool first = true;
         if (table.HasItems())
         {
            const int32 offset = table.GetNumItems()/2;
            for (int i=offset-1; i>=0; i--) 
            {
               MASSERT(iter.HasData(), "Not enough keys in table!?!?\n");
               MASSERT((atoi(iter.GetValue()()) == iter.GetKey()), "value/key mismatch B!\n");
               printf("%s%i", first?"":", ", iter.GetKey());
               first = false;
               iter++;
            }
         }

         switch(command)
         { 
            case 'F':  
               if ((arg1)&&(arg2))
               {
                  printf("%s(%s %i before %i)", first?"":", ", (table.MoveToBefore(atoi(arg1),atoi(arg2)).IsOK()) ? "Befored" : "FailedToBefored", atoi(arg1), atoi(arg2));
                  first = false;
               }
               else printf("(No arg1 or arg2!)");
            break;
            
            case 'B':  
               if ((arg1)&&(arg2))
               {
                  printf("%s(%s %i behind %i)", first?"":", ", (table.MoveToBehind(atoi(arg1),atoi(arg2)).IsOK()) ? "Behinded" : "FailedToBehind", atoi(arg1), atoi(arg2));
                  first = false;
               }
               else printf("(No arg1 or arg2!)");
            break;
            
            case 'm':  
               if ((arg1)&&(arg2))
               {
                  printf("%s(%s %i to position %i)", first?"":", ", (table.MoveToPosition(atoi(arg1),atoi(arg2)).IsOK()) ? "Positioned" : "FailedToPosition", atoi(arg1), atoi(arg2));
                  first = false;
               }
               else printf("(No arg1 or arg2!)");
            break;
            
            case 'f':  
               if (arg1)
               {
                  printf("%s(%s %i)", first?"":", ", (table.MoveToFront(atoi(arg1)).IsOK()) ? "Fronted" : "FailedToFront", atoi(arg1));
                  first = false;
               }
               else printf("(No arg1!)");
            break;
            
            case 'b':  
               if (arg1)
               {
                  printf("%s(%s %i)", first?"":", ", (table.MoveToBack(atoi(arg1)).IsOK()) ? "Backed" : "FailedToBack", atoi(arg1));
                  first = false;
               }
               else printf("(No arg1!)");
            break;
            
            case 'p':  
               if (arg1)
               {
                  printf("%s(%s %i)", first?"":", ", (table.Put(atoi(arg1), arg1).IsOK()) ? "Put" : "FailedToPut", atoi(arg1));
                  first = false;
               }
               else printf("(No arg1!)");
            break;
            
            case 'r':  
               if (arg1)
               {
                  printf("%s(%s %i)", first?"":", ", (table.Remove(atoi(arg1)).IsOK()) ? "Removed" : "FailedToRemove", atoi(arg1));
                  first = false;
               }
               else printf("(No arg1!)");
            break;

            case 'R':
               if (arg1)
               {
                  printf("%s(%s %i)", first?"":", ", (table.Reposition(atoi(arg1)).IsOK()) ? "Repositioned" : "FailedToReposition", atoi(arg1));
                  first = false;
               }
               else printf("(No arg1!)");
            break;

            case 'c':  
               printf("%s(Clearing table)", first?"":", ");
               table.Clear();
               first = false;
            break;

            case 's':
               printf("%s(Sorting table)", first?"":", ");
               table.SortByKey();
               first = false;
            break;

            case 'q':  
               printf("%sQuitting\n", first?"":", ");
               return 0;
            break;

            default:
               printf("%s(Unknown command)", first?"":", ");
            break;
         }

         while(iter.HasData())
         {
            MASSERT((atoi(iter.GetValue()()) == iter.GetKey()), "value/key mismatch C!\n");
            printf("%s%i", first?"":", ", iter.GetKey());
            first = false;
            iter++;
         }
         printf("\n");
      }
      else return 0;
   }
}

static void CheckTable(Hashtable<int, Void> & table, uint32 numItems, bool backwards)
{
   uint32 count = 0;
   int last = backwards ? RAND_MAX : 0;
   for (HashtableIterator<int, Void> iter(table,backwards?HTIT_FLAG_BACKWARDS:0); iter.HasData(); iter++)
   {
      if (backwards ? (last < iter.GetKey()) : (last > iter.GetKey())) printf("ERROR!  Sort out of order in %s traversal!!!!\n", backwards?"backwards":"forwards");
      last = iter.GetKey();
      count++;
   }
   if (count != numItems) printf("ERROR!  Count is different!  " UINT32_FORMAT_SPEC " vs " UINT32_FORMAT_SPEC "\n", count, numItems);
}

static void TestIteratorSanityOnRemoval(bool backwards)
{
   LogTime(MUSCLE_LOG_INFO, "Testing iterator sanity (direction=%s)\n", backwards?"backwards":"forwards");

   static const uint32 COUNT = 100;
   for (uint32 i=0; i<COUNT; i++)
   {
      Hashtable<int32, int32> table;
      for (uint32 j=0; j<COUNT; j++) table.Put(j, j+COUNT);

      uint32 numPairsFound = 0;
      int32 prevKey = backwards ? (int32)COUNT : -1;
      LogTime(MUSCLE_LOG_DEBUG, " Beginning traversal...\n");
      for(HashtableIterator<int32,int32> iter(table, backwards?HTIT_FLAG_BACKWARDS:0); iter.HasData(); iter++)
      {
         const int32 expectedKey = (backwards?(prevKey-1):(prevKey+1));
         const int32 gotKey      = iter.GetKey();
         const int32 gotValue    = iter.GetValue();

         LogTime(MUSCLE_LOG_TRACE, "  Iter returned " INT32_FORMAT_SPEC " -> " INT32_FORMAT_SPEC "\n", iter.GetKey(), iter.GetValue());
         if (gotKey != expectedKey)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Expected key=" INT32_FORMAT_SPEC ", got key=" INT32_FORMAT_SPEC " (value=" UINT32_FORMAT_SPEC ")\n", expectedKey, gotKey, gotValue);
            ExitWithoutCleanup(10);
         }

         if ((gotKey%(i+1))==0) {LogTime(MUSCLE_LOG_TRACE, "    -> Deleting key=" INT32_FORMAT_SPEC "\n", gotKey); table.Remove(gotKey);}

         numPairsFound++;
         prevKey = gotKey;
      }
      if (numPairsFound != COUNT)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Expected to iterate across " UINT32_FORMAT_SPEC " pairs, only saw " UINT32_FORMAT_SPEC "!\n", COUNT, numPairsFound);
         ExitWithoutCleanup(10);
      }
   }
}

template<class T> void TestMuscleSwap(const char * desc)
{
   T m1, m2;
   m1.Put("m1", "m1");
   m2.Put("m2", "m2");

   char buf[256];
   muscleSprintf(buf, "Before muscleSwap[%s] test", desc);
   PrintAndClearStringCopyCounts(buf);

   muscleSwap(m1, m2);

   muscleSprintf(buf, "After muscleSwap[%s] test", desc);
   PrintAndClearStringCopyCounts(buf);

   if ((m1.GetWithDefault("m2") != "m2")||(m2.GetWithDefault("m1") != "m1")||(m1.GetNumItems() != 1)||(m2.GetNumItems() != 1))
   {
      printf("Oh no, muscleSwap is broken for %s objects!\n", desc);
      exit(10);
   }

   printf("muscleSwap() test for [%s] passed!\n", desc);
}

static void AddTally(Hashtable<String, double> & tallies, const char * verb, uint64 startTime, uint32 numItems)
{
   const uint64 elapsed = (GetRunTime64()-startTime);
   double itemsPerSecond = ((double)numItems*((double)MICROS_PER_SECOND))/elapsed;
   printf("   It took " UINT64_FORMAT_SPEC " microseconds to %s " UINT32_FORMAT_SPEC " items, so we %s %.0f items per second\n", elapsed, verb, numItems, verb, itemsPerSecond);
   *(tallies.GetOrPut(verb)) += itemsPerSecond;
}


// This program exercises the Hashtable class.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message tempMsg; if (ParseArgs(argc, argv, tempMsg).IsOK()) HandleStandardDaemonArgs(tempMsg);

   if (tempMsg.HasName("inter")) return DoInteractiveTest();

   // Make sure that setting equal to an empty Hashtable clears the buffer (FogBugz #10274)
   {
      Hashtable<String,String> table;
      for (int32 i=0; i<1000; i++) table.Put(String("xxx%1").Arg(i), "foo");
      printf("After population of " UINT32_FORMAT_SPEC " items, table size is " UINT32_FORMAT_SPEC "\n", table.GetNumItems(), table.GetNumAllocatedItemSlots());

      if (table.ShrinkToFit().IsOK()) printf("After shrink-to-fit, table allocation is " UINT32_FORMAT_SPEC " for " UINT32_FORMAT_SPEC " items\n", table.GetNumAllocatedItemSlots(), table.GetNumItems());
                                 else printf("Shrink-to-fit failed!?\n");

      printf("Before copy-from-empty, table allocation is " UINT32_FORMAT_SPEC "\n", table.GetNumAllocatedItemSlots()); 
      table = GetDefaultObjectForType< Hashtable<String,String> > ();
      printf(" After copy-from-empty, table allocation is " UINT32_FORMAT_SPEC "\n", table.GetNumAllocatedItemSlots()); 
   }

   // Test C++11 move semantics to make sure they aren't stealing
   {
      const String key = "key";
      const String value = "value";
      Hashtable<String,String> table;
      table.Put(key, value);
      if (key != "key") {printf("ERROR, Hashtable stole my key!\n"); exit(10);}
      if (value != "value") {printf("ERROR, Hashtable stole my value!\n"); exit(10);}
   }

   // Test muscleSwap()
   TestMuscleSwap<Hashtable<String,String> >("Hashtable");
   TestMuscleSwap<OrderedKeysHashtable<String,String> >("OrderedKeysHashtable");
   TestMuscleSwap<OrderedValuesHashtable<String,String> >("OrderedValuesHashtable");

   // Test iterator behaviour when deleting keys
   TestIteratorSanityOnRemoval(false);
   TestIteratorSanityOnRemoval(true);

   {
      LogTime(MUSCLE_LOG_INFO, "Testing a keys-only Hashtable value...\n");

      Hashtable<int, Void> keysOnly;
      printf("sizeof(keysOnly)=%u\n", (unsigned int) sizeof(keysOnly));
      keysOnly.PutWithDefault(1);
      keysOnly.PutWithDefault(2);
      keysOnly.PutWithDefault(5);
      keysOnly.PutWithDefault(10);
      for (HashtableIterator<int, Void> iter(keysOnly); iter.HasData(); iter++)  printf("key=%i\n", iter.GetKey());
   }

   {
      LogTime(MUSCLE_LOG_INFO, "Testing Tuple as a Hashtable key...\n");

      // A quick test of the Tuple class as a Hashtable key
      typedef Tuple<2,int> MyType;
      Hashtable<MyType, int> tupleTable;

      MyType a; a[0] = 5; a[1] = 6;
      MyType b; b[0] = 7; b[1] = 8;
      tupleTable.Put(a, 1);
      tupleTable.Put(b, 2);
      for (HashtableIterator<MyType, int> iter(tupleTable); iter.HasData(); iter++)  
      {  
         const MyType & key = iter.GetKey();
         printf("key=%i,%i val=%i\n", key[0], key[1], iter.GetValue());
      }
      int * ra = tupleTable.Get(a);
      int * rb = tupleTable.Get(b);
      printf("tuple: ra=[%i] rb=[%i]\n", ra?*ra:666, rb?*rb:666);
   }

   {
      LogTime(MUSCLE_LOG_INFO, "Testing Rect as a Hashtable key...\n");

      // A quick test of the Tuple class as a Hashtable key
      Hashtable<Rect, int> tupleTable;

      const Rect a(1,2,3,4);
      const Rect b(5,6,7,8);
      tupleTable.Put(a, 1);
      tupleTable.Put(b, 2);
      for (HashtableIterator<Rect, int> iter(tupleTable); iter.HasData(); iter++)  
      {  
         const Rect & key = iter.GetKey();
         printf("key=%f,%f,%f,%f val=%i\n", key.left(), key.top(), key.right(), key.bottom(), iter.GetValue());
      }
      int * ra = tupleTable.Get(a);
      int * rb = tupleTable.Get(b);
      printf("Rect: ra=[%p] rb=[%p]\n", ra, rb);
   }

   {
      LogTime(MUSCLE_LOG_INFO, "Testing Point as a Hashtable key...\n");

      // A quick test of the Tuple class as a Hashtable key
      Hashtable<Point, int> tupleTable;

      const Point a(9,10);
      const Point b(-11,-12);
      tupleTable.Put(a, 1);
      tupleTable.Put(b, 2);
      for (HashtableIterator<Point, int> iter(tupleTable); iter.HasData(); iter++)  
      {  
         const Point & key = iter.GetKey();
         printf("key=%f,%f val=%i\n", key.x(), key.y(), iter.GetValue());
      }
      int * ra = tupleTable.Get(a);
      int * rb = tupleTable.Get(b);
      printf("Point: ra=[%p] rb=[%p]\n", ra, rb);
   }

   {
      LogTime(MUSCLE_LOG_INFO, "Preparing large table for sort...\n");

      const uint32 numItems = 100000;
      Hashtable<int, Void> table; (void) table.EnsureSize(100000);
      for (uint32 i=0; i<numItems; i++) table.PutWithDefault((int)rand());
      const uint32 actualNumItems = table.GetNumItems();  // may be smaller than numItems, due to duplicate values!
      (void) table.CountAverageLookupComparisons(true);

      LogTime(MUSCLE_LOG_INFO, "Sorting...\n");
      const uint64 start = GetRunTime64();
      table.SortByKey();
      const uint64 end = GetRunTime64();

      LogTime(MUSCLE_LOG_INFO, "Time to sort " UINT32_FORMAT_SPEC " items: " UINT64_FORMAT_SPEC "ms\n", numItems, (end-start)/1000);

      // Check the resulting sorted table for correctness in both directions
      CheckTable(table, actualNumItems, false);
      CheckTable(table, actualNumItems, true);
   }

   // Test the sort algorithm for efficiency and correctness
   {
      LogTime(MUSCLE_LOG_INFO, "Preparing large table for sort...\n");

      const uint32 numItems = 100000;
      Hashtable<int, Void> table;
      for (uint32 i=0; i<numItems; i++) table.PutWithDefault((int)rand());
      const uint32 actualNumItems = table.GetNumItems();  // may be smaller than numItems, due to duplicate values!

      LogTime(MUSCLE_LOG_INFO, "Sorting...\n");
      const uint64 start = GetRunTime64();
      table.SortByKey();
      const uint64 end = GetRunTime64();

      LogTime(MUSCLE_LOG_INFO, "Time to sort " UINT32_FORMAT_SPEC " items: " UINT64_FORMAT_SPEC "ms\n", numItems, (end-start)/1000);

      // Check the resulting sorted table for correctness in both directions
      CheckTable(table, actualNumItems, false);
      CheckTable(table, actualNumItems, true);
   }

   Hashtable<String, String> table;
   {
      table.Put("Hello", "World");
      table.Put("Peanut Butter", "Jelly");
      table.Put("Ham", "Eggs");
      table.Put("Pork", "Beans");
      table.Put("Slash", "Dot");
      table.Put("Data", "Mining");    // will be overwritten and moved to the end by PutAtBack() below
      table.PutAtFront("TestDouble", "ThisShouldBeFirst");
      table.Put("Abbot", "Costello");
      table.Put("Laurel", "Hardy");
      table.Put("Thick", "Thin");
      table.Put("Butter", "Parkay");
      table.Put("Total", "Carnage");
      table.Put("Summer", "Time");
      table.Put("Terrible", "Twos");
      table.PutAtBack("Data", "ThisShouldBeLast");  // should overwrite Data->Mining and move it to the end
      table.PutBefore(String("Margarine"), String("Butter"), "ThisShouldBeBeforeButter");
      table.PutBehind(String("Oil"),       String("Butter"), "ThisShouldBeAfterButter");

      {
         LogTime(MUSCLE_LOG_INFO, "String Table constents\n");
         for (HashtableIterator<String, String> iter(table); iter.HasData(); iter++) LogTime(MUSCLE_LOG_INFO,"[%s] -> [%s]\n", iter.GetKey()(), iter.GetValue()());
      }

      table.CountAverageLookupComparisons(true);

      printf("table[\"Summer\"] = [%s]\n", table["Summer"]());
      printf("table[\"Butter\"] = [%s]\n", table["Butter"]());
      printf("table[\"Total\"]  = [%s]\n", table["Total"]());
      printf("table[\"Winter\"] = [%s] (should be blank!)\n", table["Winter"]());

      if (table.GetNumItems() != 16)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "String table has %i entries in it, expected 16!\n", table.GetNumItems());
         ExitWithoutCleanup(10);
      }

      {
         LogTime(MUSCLE_LOG_INFO, "Test partial backwards iteration\n");
         for (HashtableIterator<String, String> iter(table, "Slash", HTIT_FLAG_BACKWARDS); iter.HasData(); iter++) LogTime(MUSCLE_LOG_INFO,"[%s] -> [%s]\n", iter.GetKey()(), iter.GetValue()());
      }

      String lookup;
      if (table.Get(String("Hello"), lookup).IsOK()) LogTime(MUSCLE_LOG_DEBUG, "Hello -> %s\n", lookup());
                                                else bomb("Lookup 1 failed.\n");
      if (table.Get(String("Peanut Butter"), lookup).IsOK()) LogTime(MUSCLE_LOG_DEBUG, "Peanut Butter -> %s\n", lookup());
                                                        else bomb("Lookup 2 failed.\n");


      LogTime(MUSCLE_LOG_INFO, "Testing delete-as-you-go traveral\n");
      for (HashtableIterator<String, String> st(table); st.HasData(); st++)
      {
         LogTime(MUSCLE_LOG_INFO, "t3 = %s -> %s (tableSize=" UINT32_FORMAT_SPEC ")\n", st.GetKey()(), st.GetValue()(), table.GetNumItems());
         if (table.Remove(st.GetKey()).IsError()) bomb("Could not remove string!\n");
#if 0
         for (HashtableIterator<String,String> st2(table); st2.HasData(); st2++) printf("  tx = %s -> %s\n", nextKeyString(), nextValueString());
#endif
      }

      Hashtable<uint32, const char *> sillyTable;
      sillyTable.Put(15, "Fifteen");
      sillyTable.Put(100, "One Hundred");
      sillyTable.Put(150, "One Hundred and Fifty");
      sillyTable.Put(200, "Two Hundred");
      sillyTable.Put((uint32)-1, "2^32 - 1!");
      if (sillyTable.ContainsKey((uint32)-1) == false) bomb("large value failed!");

      const char * tempStr = NULL;
      sillyTable.Get(100, tempStr);
      sillyTable.Get(101, tempStr); // will fail
      printf("100 -> %s\n", tempStr);

      printf("Entries in sillyTable:\n");
      for (HashtableIterator<uint32, const char *> it(sillyTable); it.HasData(); it++)
      {
         const char * nextValue = NULL;
         status_t ret = sillyTable.Get(it.GetKey(), nextValue);
         printf("%i %s: " UINT32_FORMAT_SPEC " -> %s\n", it.HasData(), ret(), it.GetKey(), nextValue);
      }
   }
   table.Clear();   

   {
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Hashtable<int, int> testCopy;
      Hashtable<String, double> tallies;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         Hashtable<int, int> iTable; (void) iTable.EnsureSize(NUM_ITEMS);
         printf("SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         uint64 startTime = GetRunTime64();
         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) iTable.Put(rand(), rand());  // we want this to be repeatable, hence srand(0)
         AddTally(tallies, "place", startTime, NUM_ITEMS);
         
         startTime = GetRunTime64();
         iTable.SortByValue();
         AddTally(tallies, "sort", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         testCopy = iTable;  // just to make sure copying a table works
         AddTally(tallies, "copy", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         if (testCopy != iTable) bomb("Copy was not the same!");
         AddTally(tallies, "compare", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         if (testCopy.IsEqualTo(iTable, true) == false) bomb("Copy was not the same, considering ordering!");
         AddTally(tallies, "o-compare", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         iTable.Clear();
         AddTally(tallies, "clear", startTime, NUM_ITEMS);
      }
      printf("GRAND AVERAGES OVER ALL " UINT32_FORMAT_SPEC " RUNS ARE:\n", NUM_RUNS); 
      for (HashtableIterator<String, double> iter(tallies); iter.HasData(); iter++) printf("   %f items/second for %s\n", iter.GetValue()/NUM_RUNS, iter.GetKey()());
   }

   // Now some timing test with String keys and values, for testing of the C++11 move semantics
   PrintAndClearStringCopyCounts("Before String Sort test");
   {
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Hashtable<String, String> testCopy;
      Hashtable<String, double> tallies;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         Hashtable<String, String> sTable; (void) sTable.EnsureSize(NUM_ITEMS);
         printf("STRING SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         uint64 startTime = GetRunTime64();
         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) sTable.Put(String("%1").Arg(rand()), String("%1").Arg(rand()));  // we want this to be repeatable, hence srand(0)
         AddTally(tallies, "place", startTime, NUM_ITEMS);
         
         startTime = GetRunTime64();
         sTable.SortByValue();
         AddTally(tallies, "sort", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         testCopy = sTable;  // just to make sure copying a table works
         AddTally(tallies, "copy", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         if (testCopy != sTable) bomb("Copy was not the same!");
         AddTally(tallies, "compare", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         if (testCopy.IsEqualTo(sTable, true) == false) bomb("Copy was not the same, considering ordering!");
         AddTally(tallies, "o-compare", startTime, NUM_ITEMS);

         startTime = GetRunTime64();
         sTable.Clear();
         AddTally(tallies, "clear", startTime, NUM_ITEMS);
      }
      printf("STRING GRAND AVERAGES OVER ALL " UINT32_FORMAT_SPEC " RUNS ARE:\n", NUM_RUNS); 
      for (HashtableIterator<String, double> iter(tallies); iter.HasData(); iter++) printf("   STRING %f items/second for %s\n", iter.GetValue()/NUM_RUNS, iter.GetKey()());
   }
   PrintAndClearStringCopyCounts("After String Sort test");

   printf("Begin torture test!\n");
   _state = 4;
   {
      bool fastClear = false;
      Hashtable<String, uint32> t;
      for (uint32 numEntries=1; numEntries < 1000; numEntries++)
      {
         uint32 half = numEntries/2;
         bool ok = true;

         printf(UINT32_FORMAT_SPEC " ", numEntries); fflush(stdout);
         _state = 5;
         {
            for(uint32 i=0; i<numEntries; i++)
            {
               char temp[300];
               muscleSprintf(temp, UINT32_FORMAT_SPEC, i);
               if (t.Put(temp, i).IsError())
               {
                  printf("Whoops, (hopefully simulated) memory failure!  (Put(" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") failed) ... recovering\n", i, numEntries);

                  ok = false;
                  numEntries--;  // let's do this one over
                  half = i;    // so the remove code won't freak out about not everything being there
                  break;
               }
            }
         }

         if (ok)
         {
            //printf("Checking that all entries are still there...\n");
            _state = 6;
            {
               if (t.GetNumItems() != numEntries) bomb("ERROR, WRONG SIZE %i vs %i!\n", t.GetNumItems(), numEntries);
               for (int32 i=((int32)numEntries)-1; i>=0; i--)
               {
                  char temp[300];
                  muscleSprintf(temp, UINT32_FORMAT_SPEC, i);
                  uint32 tv = 0;
                  if (t.Get(temp, tv).IsError()) bomb("ERROR, MISSING KEY [%s]\n", temp);
                  if (tv != ((uint32)i)) bomb("ERROR, WRONG KEY %s != " UINT32_FORMAT_SPEC "!\n", temp, tv);
               }
            }
      
            //printf("Iterating through table...\n");
            _state = 7;
            {
               uint32 count = 0;
               for (HashtableIterator<String, uint32> iter(t); iter.HasData(); iter++)
               {
                  char buf[300];
                  muscleSprintf(buf, UINT32_FORMAT_SPEC, count);
                  if (iter.GetKey() != buf) bomb("ERROR:  iteration was wrong, item " UINT32_FORMAT_SPEC " was [%s] not [%s]!\n", count, iter.GetKey()(), buf);
                  if (iter.GetValue() != count) bomb("ERROR:  iteration value was wrong, item " UINT32_FORMAT_SPEC " was " UINT32_FORMAT_SPEC " not " UINT32_FORMAT_SPEC "!i!\n", count, iter.GetValue(), count);
                  count++;
               }
            }

            //printf("Removing the second half of the entries...\n");
            _state = 8;
            {
               for (uint32 i=half; i<numEntries; i++)
               {
                  char temp[64];
                  muscleSprintf(temp, UINT32_FORMAT_SPEC, i);
                  uint32 tv = 0;  // just to shut the compiler up
                  if (t.Remove(temp, tv).IsError()) bomb("ERROR, MISSING REMOVE KEY [%s] A\n", temp);
                  if (tv != i) bomb("ERROR, REMOVE WAS WRONG VALUE " UINT32_FORMAT_SPEC "\n", tv);
               }
            }

            //printf("Iterating over only first half...\n");
            _state = 9;
            {
               uint32 sum = 0; 
               for (uint32 i=0; i<half; i++) sum += i;

               uint32 count = 0, checkSum = 0;
               for (HashtableIterator<String, uint32> iter(t); iter.HasData(); iter++)
               {
                  count++;
                  checkSum += iter.GetValue();
               }
               if (count != half) bomb("ERROR: Count mismatch " UINT32_FORMAT_SPEC " vs " UINT32_FORMAT_SPEC "!\n", count, numEntries);
               if (checkSum != sum)     bomb("ERROR: Sum mismatch " UINT32_FORMAT_SPEC " vs " UINT32_FORMAT_SPEC "!\n", sum, checkSum);
            }
         }

         //printf("Clearing Table (%s)\n", fastClear ? "Quickly" : "Slowly"); 
         _state = 10;
         if (fastClear) t.Clear();
         else
         {
            for (uint32 i=0; i<half; i++)
            {
               char temp[300];
               muscleSprintf(temp, UINT32_FORMAT_SPEC, i);
               uint32 tv = 0;  // just to shut the compiler up
               if (t.Remove(temp, tv).IsError()) bomb("ERROR, MISSING REMOVE KEY [%s] (" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") B\n", temp, i, half);
               if (tv != i) bomb("ERROR, REMOVE WAS WRONG VALUE " UINT32_FORMAT_SPEC "\n", tv);
            }
         }

         HashtableIterator<String, uint32> paranoia(t);
         if (paranoia.HasData()) bomb("ERROR, ITERATOR CONTAINED ITEMS AFTER CLEAR!\n");

         if (t.HasItems()) bomb("ERROR, SIZE WAS NON-ZERO (" UINT32_FORMAT_SPEC ") AFTER CLEAR!\n", t.GetNumItems());
         fastClear = !fastClear;
      }
      printf("Finished torture test successfully!\n");
   }

#ifdef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
   printf("Thread-safe hashtable iterators were disabled at compile time, so I won't test them!\n");
   return 0;
#else
   return DoThreadTest();
#endif
}
