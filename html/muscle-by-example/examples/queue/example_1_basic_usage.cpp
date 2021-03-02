#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Queue.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic usage of the muscle::Queue class to store an ordered list of data items.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Basic Queue declaration and appending:\n");
   {
#ifdef MUSCLE_AVOID_CPLUSPLUS11
      Queue<int> q; q.AddTail(1); q.AddTail(2); q.AddTail(3);  // support for pre-C++11 compilers
#else
      Queue<int> q = {1, 2, 3};   // initial Queue contents
#endif
      (void) q.AddTail(5);    // The (void) is just to indicate
      (void) q.AddTail(10);   // that I am deliberately ignoring
      (void) q.AddTail(15);   // the return values of the AddTail() calls
      (void) q.AddTail(20);

      for (uint32 i=0; i<q.GetNumItems(); i++)
      {
         printf("   Value at position #" UINT32_FORMAT_SPEC " is:  %i\n", i, q[i]);
      }
   }

   printf("\n");
   printf("Basic Queue declaration and prepending:\n");
   {
#ifdef MUSCLE_AVOID_CPLUSPLUS11
      Queue<int> q; q.AddTail(1); q.AddTail(2); q.AddTail(3);  // support for pre-C++11 compilers
#else
      Queue<int> q = {1, 2, 3};   // initial Queue contents
#endif
      (void) q.AddHead(5);   // note we are adding each item 
      (void) q.AddHead(10);  // *before* the first item in the Queue!
      (void) q.AddHead(15);
      (void) q.AddHead(20);

      for (uint32 i=0; i<q.GetNumItems(); i++)
      {
         printf("   Value at position #" UINT32_FORMAT_SPEC " is:  %i\n", i, q[i]);
      }
   }

   printf("\n");
   printf("FIFO demonstration:\n");
   {
#ifdef MUSCLE_AVOID_CPLUSPLUS11
      Queue<int> q; // support for pre-C++11 compilers
      q.AddTail(101);
      q.AddTail(102);
      q.AddTail(103);
      q.AddTail(104);
      q.AddTail(105);
#else
      Queue<int> q = {101, 102, 103, 104, 105};
#endif

      printf("   Initial Queue contents:");
      for (uint32 j=0; j<q.GetNumItems(); j++)
      {
         printf(" " UINT32_FORMAT_SPEC, q[j]);
      }
      printf("\n");
      printf("\n");

      for (int i=0; i<5; i++)
      {
         if (q.AddTail(i).IsOK())
         {
            printf("   Added %i to the tail of the Queue...\n", i);
         }

         int poppedFromHead;
         if (q.RemoveHead(poppedFromHead).IsOK())
         {
            printf("   Popped %i from the head of the Queue...\n", poppedFromHead);
         }

         printf("   Current Queue contents:");
         for (uint32 j=0; j<q.GetNumItems(); j++)
         {
            printf(" " UINT32_FORMAT_SPEC, q[j]);
         }
         printf("\n");
         printf("\n");
      }
   }

   return 0;
}
