/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()
#include "util/Queue.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) {printf("Test failed, line %i\n",__LINE__); ExitWithoutCleanup(10);}

void Print(const Queue<int> & q);
void Print(const Queue<int> & q)
{
   printf("Queue state is:\n");
   for (uint32 i=0; i<q.GetNumItems(); i++)
   {
/*
      int val;
      TEST(q.GetItemAt(i, val));
      printf(UINT32_FORMAT_SPEC " -> %i\n",i,val);
*/
      printf(UINT32_FORMAT_SPEC " -> %i\n",i,q[i]);
   }
}

#define TEST_MOVE_OPERATORS 1
#ifdef TEST_MOVE_OPERATORS
class MovableItem
{
public:
   MovableItem() {printf("MovableItem DEFAULT CTOR this=%p\n", this);}
   ~MovableItem() {printf("MovableItem DTOR this=%p\n", this);}

   MovableItem(const MovableItem & rhs) {printf("MovableItem COPY CTOR this=%p rhs=%p\n", this, &rhs);}
   MovableItem(const MovableItem && rhs) {printf("MovableItem MOVE CTOR this=%p rhs=%p\n", this, &rhs);}

   MovableItem & operator =(const MovableItem & rhs) {printf("MovableItem ASSIGNMENT OPERATOR this=%p rhs=%p\n", this, &rhs); return *this;}
   MovableItem & operator =(MovableItem && rhs) {printf("MovableItem MOVE OPERATOR this=%p rhs=%p\n", this, &rhs); return *this;}
};
#endif

static status_t UnitTestQueue(bool isFromScript)
{
   CompleteSetupSystem css;  // needed for string-count stats

#ifdef TEST_MOVE_OPERATORS
   Queue<MovableItem> miq;

   printf("Adding 10 MovableItems\n");
   for (uint32 i=0; i<10; i++) (void) miq.AddTail();

   MovableItem temp;
   if (miq.RemoveTail(temp).IsOK()) printf("RemoveTail() succeeded\n");
   if (miq.RemoveHead(temp).IsOK()) printf("RemovedHead() succeeded\n");
   if (miq.RemoveItemAt(1, temp).IsOK()) printf("RemoveItemAt() succeeded\n");
#endif

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   {
      Queue<int> q {1,2,3,4,5};
      if (q.GetNumItems() != 5) {printf("Oh no, initialize list constructor didn't work!\n"); return B_LOGIC_ERROR;}
      q = {6,7,8,9,10,11};
      if (q.GetNumItems() != 6) {printf("Oh no, initialize list assignment operator didn't work!\n"); return B_LOGIC_ERROR;}
   }
#endif

   // FogBugz #10274:  make sure we flush the Queue's allocated buffer when setting the Queue to empty
   {
      // Watch the behavior of the buffer size
      Queue<int> q;
      uint32 numAllocedSlots = 0;
      for (int i=0; i<50000; i++)
      {
         MPRINT_ON_ERROR("AddTail", q.AddTail(i));
         const uint32 newNumAlloced = q.GetNumAllocatedItemSlots();
         if (newNumAlloced != numAllocedSlots)
         {
            printf("i=%i q.GetNumItems()=" UINT32_FORMAT_SPEC " q.GetNumAllocatedItemSlots()=" UINT32_FORMAT_SPEC "\n", i, q.GetNumItems(), newNumAlloced);
            numAllocedSlots = newNumAlloced;
         }
      }
      MRETURN_ON_ERROR(q.ShrinkToFit());
      printf("After ShrinkToFit():  q.GetNumItems()=" UINT32_FORMAT_SPEC " q.GetNumAllocatedItemSlots()=" UINT32_FORMAT_SPEC "\n", q.GetNumItems(), q.GetNumAllocatedItemSlots());

      printf("Before setting equal to empty, q's allocated-slots size is: " UINT32_FORMAT_SPEC "\n", q.GetNumAllocatedItemSlots());
      q = GetDefaultObjectForType< Queue<int> >();
      printf(" After setting equal to empty, q's allocated-slots size is: " UINT32_FORMAT_SPEC "\n", q.GetNumAllocatedItemSlots());
   }

   // Test muscleSwap()
   {
      Queue<String> q1, q2;
      (void) q1.AddTail("q1");
      (void) q2.AddTail("q2");
      printf("Before swap, hashes are q1=" UINT32_FORMAT_SPEC ", q2=" UINT32_FORMAT_SPEC "\n", q1.HashCode(), q2.HashCode());
      muscleSwap(q1, q2);
      if ((q1.GetNumItems() != 1)||(q2.GetNumItems() != 1)||(q1[0] != "q2")||(q2[0] != "q1"))
      {
         printf("Oh no, muscleSwap is broken for Message objects!\n");
         return B_LOGIC_ERROR;
      }
      printf("After swap, hashes are q1=" UINT32_FORMAT_SPEC ", q2=" UINT32_FORMAT_SPEC "\n", q1.HashCode(), q2.HashCode());
      printf("muscleSwap() worked!\n");
   }


   const int testSize = 15;
   Queue<int> q;

   const int vars[] = {5,6,7,8,9,10,11,12,13,14,15};

   printf("ADDTAIL TEST hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   {
      for (int i=0; i<testSize; i++)
      {
         TEST(q.AddTail(i));
         printf("len=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", q.GetNumItems(), q.GetNumAllocatedItemSlots());
      }
   }

   printf("AddTail array hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   MPRINT_ON_ERROR("AddTailMulti", q.AddTailMulti(vars, ARRAYITEMS(vars)));
   Print(q);

   printf("AddHead array hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   MPRINT_ON_ERROR("AddHeadMulti", q.AddHeadMulti(vars, ARRAYITEMS(vars)));
   Print(q);

   printf("REPLACEITEMAT TEST hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   {
      for (int i=0; i<testSize; i++)
      {
         TEST(q.ReplaceItemAt(i, i+10));
         Print(q);
      }
   }

   printf("INSERTITEMAT TEST hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   {
      for (int i=0; i<testSize; i++)
      {
         TEST(q.InsertItemAt(i,i));
         Print(q);
      }
   }

   printf("REMOVEITEMAT TEST hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   {
      for (int i=0; i<testSize; i++)
      {
         TEST(q.RemoveItemAt(i));
         Print(q);
      }
   }

   // Check that C++11's move semantics aren't stealing values they shouldn't
   {
      Queue<String> qq;
      String myStr = "Magic";
      (void) qq.AddTail(myStr);
      if (myStr != "Magic")
      {
         printf("Error, AddTail() stole my string!\n");
         return B_LOGIC_ERROR;
      }
   }

   printf("SORT TEST 1 hash=" UINT32_FORMAT_SPEC "\n", q.HashCode());
   {
      q.Clear();
      for (int i=0; i<testSize; i++)
      {
         const int next = (int) GetInsecurePseudoRandomNumber(255);
         TEST(q.AddTail(next));
         printf("Added item %i = %i\n", i, q[i]);
      }
      printf("sorting ints...\n");
      q.Sort();
      for (int j=0; j<testSize; j++) printf("Now item %i = %i\n", j, q[j]);
   }

   printf("SORT TEST 2\n");
   {
      Queue<String> q2;
      for (int i=0; i<testSize; i++)
      {
         const int next = (int) GetInsecurePseudoRandomNumber(255);
         char buf[64];
         muscleSprintf(buf, "%i", next);
         TEST(q2.AddTail(buf));
         printf("Added item %i = %s\n", i, q2[i].Cstr());
      }
      printf("sorting strings...\n");
      q2.Sort();
      for (int j=0; j<testSize; j++) printf("Now item %i = %s\n", j, q2[j].Cstr());
   }

   printf("REMOVE DUPLICATES test\n");
   {
      Queue<int> qq;
      const int iVars[] = {9,2,3,5,8,3,5,6,6,7,2,3,4,6,8,9,3,5,6,4,3,2,1};
      MRETURN_ON_ERROR(qq.AddTailMulti(iVars, ARRAYITEMS(iVars)));

      qq.RemoveDuplicateItems();
      for (uint32 i=0; i<qq.GetNumItems(); i++) printf("%u ", qq[i]);
      printf("\n");
   }

   if (!isFromScript)
   {
      const uint32 NUM_ITEMS = 300000000;
      const uint32 NUM_RUNS  = 3;
      Queue<uint32> iq; (void) iq.EnsureSize(NUM_ITEMS, true);
      double tally = 0.0;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         printf("SUM SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) iq[i] = GetInsecurePseudoRandomNumber();  // we want this to be repeatable, hence srand(0)

         uint64 count = 0;
         const uint64 startTime = GetRunTime64();
         for (uint32 i=0; i<iq.GetNumItems(); i++) count += iq[i];
         const uint64 elapsed = (GetRunTime64()-startTime);

         const double itemsPerSecond = ((double)NUM_ITEMS*((double)MICROS_PER_SECOND))/(elapsed);
         printf("   It took " UINT64_FORMAT_SPEC " microseconds to sum " UINT32_FORMAT_SPEC " items, so we summed %f items per second:  " UINT64_FORMAT_SPEC "\n", elapsed, NUM_ITEMS, itemsPerSecond, count);
         tally += itemsPerSecond;
      }
      printf("GRAND AVERAGE ITEMS PER SECOND WAS %f items per second\n", tally/NUM_RUNS);
   }

   if (!isFromScript)
   {
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Queue<uint32> iq; (void) iq.EnsureSize(NUM_ITEMS, true);
      double tally = 0.0;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         printf("SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) iq[i] = GetInsecurePseudoRandomNumber();  // we want this to be repeatable, hence srand(0)

         const uint64 startTime = GetRunTime64();
         iq.Sort();
         const uint64 elapsed = (GetRunTime64()-startTime);

         const double itemsPerSecond = ((double)NUM_ITEMS*((double)MICROS_PER_SECOND))/(elapsed);
         printf("   It took " UINT64_FORMAT_SPEC " microseconds to sort " UINT32_FORMAT_SPEC " items, so we sorted %f items per second\n", elapsed, NUM_ITEMS, itemsPerSecond);
         tally += itemsPerSecond;
      }
      printf("GRAND AVERAGE ITEMS PER SECOND WAS %f items per second\n", tally/NUM_RUNS);
   }

   if (!isFromScript)
   {
      PrintAndClearStringCopyCounts(stdout, "Before String Sort Tests");
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Queue<String> qq; (void) qq.EnsureSize(NUM_ITEMS, true);
      double tally = 0.0;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         printf("STRING SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) qq[i] = String("FooBarBaz-%1").Arg(GetInsecurePseudoRandomNumber()).PaddedBy(500);  // we want this to be repeatable, hence srand(0)

         const uint64 startTime = GetRunTime64();
         qq.Sort();
         const uint64 elapsed = (GetRunTime64()-startTime);

         const double itemsPerSecond = ((double)NUM_ITEMS*((double)MICROS_PER_SECOND))/(elapsed);
         printf("   It took " UINT64_FORMAT_SPEC " microseconds to sort " UINT32_FORMAT_SPEC " items, so we sorted %f items per second\n", elapsed, NUM_ITEMS, itemsPerSecond);
         tally += itemsPerSecond;
      }
      printf("STRING GRAND AVERAGE ITEMS PER SECOND WAS %f items per second\n", tally/NUM_RUNS);
      PrintAndClearStringCopyCounts(stdout, "After String Sort Tests");
   }

   printf("REVERSE TEST\n");
   {
      q.Clear();
      for (int i=0; i<testSize; i++) TEST(q.AddTail(i));
      q.ReverseItemOrdering();
      for (int j=0; j<testSize; j++) printf("After reverse, %i->%i\n", j, q[j]);
   }

   printf("CONCAT TEST 1\n");
   {
      q.Clear();
      Queue<int> q2;
      for (int i=0; i<testSize; i++)
      {
         TEST(q.AddTail(i));
         TEST(q2.AddTail(i+100));
      }
      MRETURN_ON_ERROR(q.AddTailMulti(q2));
      for (uint32 j=0; j<q.GetNumItems(); j++) printf("After concat, " UINT32_FORMAT_SPEC "->%i\n", j, q[j]);
   }

   printf("CONCAT TEST 2\n");
   {
      q.Clear();
      Queue<int> q2;
      for (int i=0; i<testSize; i++)
      {
         TEST(q.AddTail(i));
         TEST(q2.AddTail(i+100));
      }
      MRETURN_ON_ERROR(q.AddHeadMulti(q2));
      for (uint32 j=0; j<q.GetNumItems(); j++) printf("After concat, " UINT32_FORMAT_SPEC "->%i\n", j, q[j]);
   }
   {
      printf("GetArrayPointer() test\n");
      uint32 len = 0;
      int * a;
      for (uint32 i=0; (a = q.GetArrayPointer(i, len)) != NULL; i++)
      {
         printf("SubArray " UINT32_FORMAT_SPEC ": " UINT32_FORMAT_SPEC " items: ", i, len);
         for (uint32 j=0; j<len; j++) printf("%i, ", a[j]);
         printf("\n");
      }
   }

   if (!isFromScript)
   {
      for (uint32 i=0; i<20000; i++)
      {
         printf("\nStress-testing Queue::Normalize()... this may take a minute\n");
         Queue<int> qq;
         int counter = 0;
         for (uint32 j=0; j<i; j++)
         {
            switch(GetInsecurePseudoRandomNumber(6))
            {
               case 0:  case 1: MPRINT_ON_ERROR("AddTail",    qq.AddTail(counter++)); break;
               case 2:  case 3: MPRINT_ON_ERROR("AddHead",    qq.AddHead(counter++)); break;
               case 4:  {const status_t r = qq.RemoveHead(); if (r != B_DATA_NOT_FOUND) {MPRINT_ON_ERROR("RemoveHead", r);} break;}
               case 5:  {const status_t r = qq.RemoveTail(); if (r != B_DATA_NOT_FOUND) {MPRINT_ON_ERROR("RemoveTail", r);} break;}
            }
         }

         int * compareArray = new int[qq.GetNumItems()];
         for (uint32 j=0; j<qq.GetNumItems(); j++) compareArray[j] = qq[j];
         qq.Normalize();

         const int * a = qq.HeadPointer();
         if (memcmp(compareArray, a, qq.GetNumItems()*sizeof(int)))
         {
            printf("ERROR IN NORMALIZE!\n");
            for (uint32 j=0; j<qq.GetNumItems(); j++) printf("   Expected %i, got %i (qi=%i at " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ")\n", compareArray[j], a[j], qq[j], j, qq.GetNumItems());
            delete [] compareArray;
            return B_LOGIC_ERROR;
         }

         delete [] compareArray;
      }
   }

   printf("Queue test complete.\n");
   return B_NO_ERROR;
}

int main(int argc, char ** argv)
{
   const status_t ret = UnitTestQueue((argc>=2)&&(strcmp(argv[1], "fromscript")==0));
   if (ret.IsOK())
   {
      LogTime(MUSCLE_LOG_INFO, "testqueue passed, exiting!\n");
      return 0;
   }
   else
   {
      LogTime(MUSCLE_LOG_INFO, "testqueue failed [%s], exiting!\n", ret());
      return 10;
   }
}
