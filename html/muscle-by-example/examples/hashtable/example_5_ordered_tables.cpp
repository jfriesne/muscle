#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the OrderedKeysHashtable and OrderedValuesHashtable classes\n");
   printf("They are subclasses of Hashtable, and work the same as a Hashtable except that they\n");
   printf("automatically keep their key/value pairs in sorted order as new items are inserted.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // The OrderedKeysHashtable class is the same as the Hashtable class, except
   // that it makes sure to always keep its key/value pairs in sorted-by-keys
   // order.  That allows us to avoid having to call Sort(), at the cost of
   // making Put an O(N) operation instead of O(1).

   OrderedKeysHashtable<int, String> okTable;
   okTable.Put(3, "Three"); 
   okTable.Put(7, "Seven"); 
   okTable.Put(1, "One"); 
   okTable.Put(5, "Five"); 
   okTable.Put(9, "Nine"); 
   okTable.Put(4, "Four"); 
   okTable.Put(8, "Eight"); 
   okTable.Put(2, "Two"); 
   okTable.Put(6, "Six"); 

   printf("Contents of OrderedKeysHashtable: (auto-sorted by key)\n");
   for (HashtableIterator<int, String> iter(okTable); iter.HasData(); iter++)
   {
      printf("   %i -> %s\n", iter.GetKey(), iter.GetValue()());
   }

   printf("\n");

   // The OrderedValuesHashtable class is much like the OrderedKeysHashtable class
   // except the key/value pairs are always kept sorted in sorted-by-values order.
   OrderedValuesHashtable<int, String> ovTable;
   ovTable.Put(3, "Three"); 
   ovTable.Put(1, "One"); 
   ovTable.Put(4, "Four"); 
   ovTable.Put(1, "One"); 
   ovTable.Put(6, "Six"); 
   ovTable.Put(9, "Nine"); 
   ovTable.Put(8, "Eight"); 
   ovTable.Put(2, "Two"); 
   ovTable.Put(5, "Five"); 

   printf("Contents of OrderedValuesHashtable (Note value-strings are auto-sorted alphabetically):\n");
   for (HashtableIterator<int, String> iter(ovTable); iter.HasData(); iter++)
   {
      printf("   %i -> %s\n", iter.GetKey(), iter.GetValue()());
   }

   printf("\n");
   return 0;
}
