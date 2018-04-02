#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates how the contents of a Hashtable can be sorted by key or by value, or manually re-ordered.\n");
   printf("\n");
}

void PrintTable(const char * desc, const Hashtable<String, int> & table); // forward declaration; see bottom of file

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

/* This little program demonstrates the manipulation of the ordering of items in a Hashtable<String,int> */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   Hashtable<String, int> table;
   (void) table.Put("Five",     5);
   (void) table.Put("Ten",     10);
   (void) table.Put("Eight",    8);
   (void) table.Put("Fifteen", 15);
   (void) table.Put("Twelve",  12);
   (void) table.Put("Three",    3);

   PrintTable("Initial Table State", table);

   table.SortByKey();  // sorts keys alphabetically
   PrintTable("After calling table.SortByKey()", table);

   table.SortByValue();  // sorts values numerically
   PrintTable("After calling table.SortByValue()", table);

   table.SortByKey(MyCustomCompareFunctor());  // custom sort-compare-functor, sorts table by key-string-length!
   PrintTable("After calling table.SortByKey(MyCustomSortFunctor())", table);

   (void) table.MoveToFront("Five");
   PrintTable("After moving \"Five\" to the front", table);

   (void) table.MoveToBack("Eight");
   PrintTable("After moving \"Eight\" to the back", table);

   (void) table.MoveToBefore("Ten", "Three");
   PrintTable("After moving \"Ten\" to just before \"Three\"", table);

   (void) table.MoveToBehind("Fifteen", "Twelve");
   PrintTable("After moving \"Fifteen\" to just behind \"Twelve\"", table);

   return 0;
}

void PrintTable(const char * desc, const Hashtable<String, int> & table)
{
   printf("\n");
   printf("%s\n", desc);
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }
}
