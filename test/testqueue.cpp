/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/Queue.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

void PrintToStream(const Queue<int> & q);
void PrintToStream(const Queue<int> & q)
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

// This program exercises the Queue class.
int main(void) 
{
   CompleteSetupSystem css;  // needed for string-count stats

//#define TEST_SWAP_METHOD
#ifdef TEST_SWAP_METHOD
   while(1)
   {
      char qs1[512]; printf("Enter q1: "); fflush(stdout); if (fgets(qs1, sizeof(qs1), stdin) == NULL) qs1[0] = '\0';
      char qs2[512]; printf("Enter q2: "); fflush(stdout); if (fgets(qs2, sizeof(qs2), stdin) == NULL) qs2[0] = '\0';

      Queue<int> q1, q2;      
      StringTokenizer t1(qs1), t2(qs2);
      const char * s;
      while((s = t1()) != NULL) q1.AddTail(atoi(s));
      while((s = t2()) != NULL) q2.AddTail(atoi(s));
      printf("T1Before="); PrintToStream(q1);
      printf("T2Before="); PrintToStream(q2);
      printf("\n");
      printf("q1 <  q2 = %i\n",  q1<q2);
      printf("q1 <= q2 = %i\n", q1<=q2);
      printf("q1 >  q2 = %i\n", q1>q2);
      printf("q1 >= q2 = %i\n", q1>=q2);
      printf("q1 == q2 = %i\n", q1==q2);
      printf("q1 != q2 = %i\n", q1!=q2);
      printf("\n");

      q1.SwapContents(q2);
      printf("T1After="); PrintToStream(q1);
      printf("T2After="); PrintToStream(q2);
   }
#endif

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   {
      Queue<int> q {1,2,3,4,5};
      if (q.GetNumItems() != 5) {printf("Oh no, initialize list constructor didn't work!\n"); exit(10);}
      q = {6,7,8,9,10,11};
      if (q.GetNumItems() != 6) {printf("Oh no, initialize list assignment operator didn't work!\n"); exit(10);}
   }
#endif

   // FogBugz #10274:  make sure we flush the Queue's allocated buffer when setting the Queue to empty
   {
      // Watch the behavior of the buffer size
      Queue<int> q;
      uint32 numAllocedSlots = 0;
      for (int i=0; i<50000; i++)
      {
         q.AddTail(i);
         const uint32 newNumAlloced = q.GetNumAllocatedItemSlots();
         if (newNumAlloced != numAllocedSlots)
         {
            printf("i=%i q.GetNumItems()=" UINT32_FORMAT_SPEC " q.GetNumAllocatedItemSlots()=" UINT32_FORMAT_SPEC "\n", i, q.GetNumItems(), newNumAlloced);
            numAllocedSlots = newNumAlloced;
         }
      }
      if (q.ShrinkToFit().IsOK()) printf("After ShrinkToFit():  q.GetNumItems()=" UINT32_FORMAT_SPEC " q.GetNumAllocatedItemSlots()=" UINT32_FORMAT_SPEC "\n", q.GetNumItems(), q.GetNumAllocatedItemSlots());
                             else printf("ShrinkToFit() failed!\n");

      printf("Before setting equal to empty, q's allocated-slots size is: " UINT32_FORMAT_SPEC "\n", q.GetNumAllocatedItemSlots());
      q = GetDefaultObjectForType< Queue<int> >();
      printf(" After setting equal to empty, q's allocated-slots size is: " UINT32_FORMAT_SPEC "\n", q.GetNumAllocatedItemSlots());
   }

   // Test muscleSwap()
   {
      Queue<String> q1, q2;
      q1.AddTail("q1");
      q2.AddTail("q2");
      muscleSwap(q1, q2);
      if ((q1.GetNumItems() != 1)||(q2.GetNumItems() != 1)||(q1[0] != "q2")||(q2[0] != "q1"))
      {
         printf("Oh no, muscleSwap is broken for Message objects!\n");
         exit(10);
      }
      printf("muscleSwap() worked!\n");
   }


   const int testSize = 15;
   Queue<int> q;

   const int vars[] = {5,6,7,8,9,10,11,12,13,14,15};

   printf("ADDTAIL TEST\n");
   {
      for (int i=0; i<testSize; i++) 
      {
         TEST(q.AddTail(i));
         printf("len=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", q.GetNumItems(), q.GetNumAllocatedItemSlots());
      }
   } 

   printf("AddTail array\n");
   q.AddTailMulti(vars, ARRAYITEMS(vars));
   PrintToStream(q);

   printf("AddHead array\n");
   q.AddHeadMulti(vars, ARRAYITEMS(vars));
   PrintToStream(q);

   printf("REPLACEITEMAT TEST\n");
   {
      for (int i=0; i<testSize; i++) 
      {
         TEST(q.ReplaceItemAt(i, i+10));
         PrintToStream(q);
      }
   } 

   printf("INSERTITEMAT TEST\n");
   {
      for (int i=0; i<testSize; i++) 
      {
         TEST(q.InsertItemAt(i,i));
         PrintToStream(q);
      }
   }

   printf("REMOVEITEMAT TEST\n");
   {
      for (int i=0; i<testSize; i++) 
      {
         TEST(q.RemoveItemAt(i));
         PrintToStream(q);
      }
   }

   // Check that C++11's move semantics aren't stealing values they shouldn't
   {
      Queue<String> qq;
      String myStr = "Magic";
      qq.AddTail(myStr);
      if (myStr != "Magic") 
      {
         printf("Error, AddTail() stole my string!\n");
         exit(10);
      }
   }

   printf("SORT TEST 1\n");
   {
      q.Clear();
      for (int i=0; i<testSize; i++)
      {
         const int next = rand()%255;
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
         const int next = rand()%255;
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
      if (qq.AddTailMulti(iVars, ARRAYITEMS(iVars)).IsOK())
      {
         qq.RemoveDuplicateItems(); 
         for (uint32 i=0; i<qq.GetNumItems(); i++) printf("%u ", qq[i]);
         printf("\n");
      }
   }

   {
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Queue<int> iq; (void) iq.EnsureSize(NUM_ITEMS, true);
      double tally = 0.0;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         printf("SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) iq[i] = rand();  // we want this to be repeatable, hence srand(0)
         
         const uint64 startTime = GetRunTime64();
         iq.Sort();
         const uint64 elapsed = (GetRunTime64()-startTime);

         const double itemsPerSecond = ((double)NUM_ITEMS*((double)MICROS_PER_SECOND))/(elapsed);
         printf("   It took " UINT64_FORMAT_SPEC " microseconds to sort " UINT32_FORMAT_SPEC " items, so we sorted %f items per second\n", elapsed, NUM_ITEMS, itemsPerSecond);
         tally += itemsPerSecond;
      }
      printf("GRAND AVERAGE ITEMS PER SECOND WAS %f items per second\n", tally/NUM_RUNS);
   }

   PrintAndClearStringCopyCounts("Before String Sort Tests");
   {
      const uint32 NUM_ITEMS = 1000000;
      const uint32 NUM_RUNS  = 3;
      Queue<String> qq; (void) qq.EnsureSize(NUM_ITEMS, true);
      double tally = 0.0;
      for (uint32 t=0; t<NUM_RUNS; t++)
      {
         printf("STRING SORT SPEED TEST ROUND " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":\n", t+1, NUM_RUNS);

         srand(0); for (uint32 i=0; i<NUM_ITEMS; i++) qq[i] = String("FooBarBaz-%1").Arg(rand()).Pad(500);  // we want this to be repeatable, hence srand(0)
         
         const uint64 startTime = GetRunTime64();
         qq.Sort();
         const uint64 elapsed = (GetRunTime64()-startTime);

         const double itemsPerSecond = ((double)NUM_ITEMS*((double)MICROS_PER_SECOND))/(elapsed);
         printf("   It took " UINT64_FORMAT_SPEC " microseconds to sort " UINT32_FORMAT_SPEC " items, so we sorted %f items per second\n", elapsed, NUM_ITEMS, itemsPerSecond);
         tally += itemsPerSecond;
      }
      printf("STRING GRAND AVERAGE ITEMS PER SECOND WAS %f items per second\n", tally/NUM_RUNS);
   }
   PrintAndClearStringCopyCounts("After String Sort Tests");

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
      q.AddTailMulti(q2);
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
      q.AddHeadMulti(q2);
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

   printf("\nStress-testing Queue::Normalize()... this may take a minute\n");
   for (uint32 i=0; i<20000; i++)
   {
      Queue<int> qq;
      int counter = 0;
      for (uint32 j=0; j<i; j++)
      {
         switch(rand()%6)
         {
            case 0:  case 1: qq.AddTail(counter++); break;
            case 2:  case 3: qq.AddHead(counter++); break;
            case 4:          qq.RemoveHead();       break;
            case 5:          qq.RemoveTail();       break;
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
         MCRASH("ERROR IN NORMALIZE!");
      }

      delete [] compareArray;
   }
   printf("Queue test complete.\n");

   return 0;
}
