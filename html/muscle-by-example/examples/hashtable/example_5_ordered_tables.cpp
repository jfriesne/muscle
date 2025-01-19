#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates the OrderedKeysHashtable and OrderedValuesHashtable classes\n");
   p.printf("They are subclasses of Hashtable, and work the same as a Hashtable except that they\n");
   p.printf("automatically keep their key/value pairs in sorted order as new items are inserted.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   // The OrderedKeysHashtable class is the same as the Hashtable class, except
   // that it makes sure to always keep its key/value pairs in sorted-by-keys
   // order.  That allows us to avoid having to call Sort(), at the cost of
   // making Put an O(N) operation instead of O(1).

   OrderedKeysHashtable<int, String> okTable;
   (void) okTable.Put(3, "Three");
   (void) okTable.Put(7, "Seven");
   (void) okTable.Put(1, "One");
   (void) okTable.Put(5, "Five");
   (void) okTable.Put(9, "Nine");
   (void) okTable.Put(4, "Four");
   (void) okTable.Put(8, "Eight");
   (void) okTable.Put(2, "Two");
   (void) okTable.Put(6, "Six");

   printf("Contents of OrderedKeysHashtable: (auto-sorted by key)\n");
   for (HashtableIterator<int, String> iter(okTable); iter.HasData(); iter++)
   {
      printf("   %i -> %s\n", iter.GetKey(), iter.GetValue()());
   }

   printf("\n");

   // The OrderedValuesHashtable class is much like the OrderedKeysHashtable class
   // except the key/value pairs are always kept sorted in sorted-by-values order.
   OrderedValuesHashtable<int, String> ovTable;
   (void) ovTable.Put(3, "Three");
   (void) ovTable.Put(1, "One");
   (void) ovTable.Put(4, "Four");
   (void) ovTable.Put(1, "One");
   (void) ovTable.Put(6, "Six");
   (void) ovTable.Put(9, "Nine");
   (void) ovTable.Put(8, "Eight");
   (void) ovTable.Put(2, "Two");
   (void) ovTable.Put(5, "Five");

   printf("Contents of OrderedValuesHashtable (Note value-strings are auto-sorted alphabetically):\n");
   for (HashtableIterator<int, String> iter(ovTable); iter.HasData(); iter++)
   {
      printf("   %i -> %s\n", iter.GetKey(), iter.GetValue()());
   }

   printf("\n");
   return 0;
}
