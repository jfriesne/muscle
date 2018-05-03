#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Queue.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates various minor convenience methods of the Queue class\n");
   printf("\n");
}

void PrintQueue(const char * desc, const Queue<String> & q); // forward declaration (see bottom of file)

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   Queue<String> sq;  // sigh
   sq.AddTail("Pear");
   sq.AddTail("banana");
   sq.AddTail("Apple");
   sq.AddTail("orange");
   sq.AddTail("grape");
   sq.AddTail("Berry 31");
   sq.AddTail("Berry 5");
   sq.AddTail("Berry 12");
#else
   Queue<String> sq = {"Pear", "banana", "Apple", "orange", "grape", "Berry 31", "Berry 5", "Berry 12"};
#endif

   PrintQueue("Initial Queue state", sq);

   String firstVal = sq.RemoveHeadWithDefault();  // removes first item and returns it, or "" if sq was empty
   printf("Popped firstVal:  [%s]\n", firstVal());

   String lastVal = sq.RemoveTailWithDefault();  // removes last item and returns it, or "" if sq was empty
   printf("Popped lastVal:   [%s]\n", lastVal());

   printf("\n");
   PrintQueue("Current Queue state A", sq);

   const String & firstValRef = sq.HeadWithDefault();  // reference to first item, or "" if sq was empty
   printf("Current first value is [%s]\n", firstValRef());

   const String & lastValRef = sq.TailWithDefault();  // reference to last item, or "" if sq was empty
   printf("Current last value is [%s]\n", lastValRef());
   printf("\n");

   String * newTail = sq.AddTailAndGet();   // adds new tail item and returns pointer to added item
   if (newTail) *newTail = "Schnozzberry";
           else printf("sq.AddTailAndGet() failed!?\n");

   PrintQueue("Current Queue state B", sq);

   printf("sq currently contains " UINT32_FORMAT_SPEC " items.\n", sq.GetNumItems());
   printf("\n");

   for (uint32 i=0; i<10; i++)
   {
      printf("sq.GetWithDefault(" UINT32_FORMAT_SPEC ", \"<doh>\") returned:  %s\n", i, sq.GetWithDefault(i, "<doh>")());
   }
   return 0;
}

void PrintQueue(const char * desc, const Queue<String> & q)
{
   printf("%s (" UINT32_FORMAT_SPEC " items in Queue):\n", desc, q.GetNumItems());
   for (uint32 i=0; i<q.GetNumItems(); i++) printf("   %s\n", q[i]());
   printf("\n");
}

