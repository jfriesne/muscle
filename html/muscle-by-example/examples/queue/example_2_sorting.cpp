#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Queue.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates sorting the contents of a muscle::Queue\n");
   printf("\n");
}

void PrintQueue(const char * desc, const Queue<int>    & q); // forward declaration (see bottom of file)
void PrintQueue(const char * desc, const Queue<String> & q); // forward declaration (see bottom of file)

/** Custom compare-functor object for demo purposes, compares two Strings solely by their length rather than by their contents */
class MyCustomCompareFunctor
{
public:
   int Compare(const String & s1, const String & s2, void *) const
   {
      // This function could be written as:   return muscleCompare(s1.Length(), s2.Length())
      // but I'm doing it the more explicit way to make it more obvious what is going on
      if (s1.Length() < s2.Length()) return -1;
      if (s1.Length() > s2.Length()) return +1;
      return 0;
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   Queue<int> iq;  // sigh
   iq.AddTail(3);
   iq.AddTail(1);
   iq.AddTail(4);
   iq.AddTail(1);
   iq.AddTail(5);
   iq.AddTail(9);
   iq.AddTail(2);
   iq.AddTail(6);
   iq.AddTail(2);
#else
   Queue<int> iq = {3, 1, 4, 1, 5, 9, 2, 6, 2};
#endif

   PrintQueue("int-Queue before sort", iq);

   iq.Sort();
   PrintQueue("int-Queue after sort", iq);

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

   PrintQueue("String-Queue initial state", sq);

   sq.Sort();
   PrintQueue("String-Queue after case-sensitive alphabetic sort", sq);

   sq.Sort(CaseInsensitiveStringCompareFunctor());
   PrintQueue("String-Queue after case-insensitive alphabetic sort", sq);

   sq.Sort(NumericAwareStringCompareFunctor());
   PrintQueue("String-Queue after number-aware case-sensitive sort", sq);

   sq.Sort(CaseInsensitiveNumericAwareStringCompareFunctor());
   PrintQueue("String-Queue after number-aware case-insensitive sort", sq);

   sq.Sort(MyCustomCompareFunctor());
   PrintQueue("String-Queue after sort-by-string-length", sq);

   return 0;
}

void PrintQueue(const char * desc, const Queue<int> & q)
{
   printf("%s (" UINT32_FORMAT_SPEC " items in Queue):\n", desc, q.GetNumItems());
   for (uint32 i=0; i<q.GetNumItems(); i++) printf("   %i\n", q[i]);
   printf("\n");
}

void PrintQueue(const char * desc, const Queue<String> & q)
{
   printf("%s (" UINT32_FORMAT_SPEC " items in Queue):\n", desc, q.GetNumItems());
   for (uint32 i=0; i<q.GetNumItems(); i++) printf("   %s\n", q[i]());
   printf("\n");
}

