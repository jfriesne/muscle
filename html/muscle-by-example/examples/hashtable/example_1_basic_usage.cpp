#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the basic usage of a Hashtable for storing and retrieving key->value pairs.\n");
   printf("\n");
}

/* This little program demonstrates basic usage of the muscle::Hashtable class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Basic Hashtable<String,int> declaration and population:\n");

   Hashtable<String, int> table;
   (void) table.Put("Five",     5);   // The (void) is just to indicate
   (void) table.Put("Ten",     10);   // that I am deliberately ignoring
   (void) table.Put("Eight",    8);   // the return values of the Put() calls
   (void) table.Put("Fifteen", 15);
   (void) table.Put("Three",    3);

   printf("   The table currently has " UINT32_FORMAT_SPEC " key/value pairs in it.\n", table.GetNumItems());
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }
   printf("Note that the ordering of the iteration matches the order that we called Put() on the items!\n");

   // Query to see if certain are present in the table
   printf("\n");
   printf("table.ContainsKey(\"Five\") returned %i\n", table.ContainsKey("Five"));
   printf("table.ContainsKey(\"Six\")  returned %i\n", table.ContainsKey("Six"));
   printf("\n");

   // Look up the value associated with a key
   int aVal;
   if (table.Get("Ten", aVal).IsOK())
   {
      printf("A: The value associated with key \"Ten\" was %i\n", aVal);
   }
   else printf("A: Weird, table didn't contain any key named \"Ten\" !?\n");

   printf("\n");

   // A similar lookup, except here we avoid having to copy the value
   int * pVal = table.Get("Ten");
   if (pVal)
   {
      printf("B: The value associated with key \"Ten\" was %i\n", *pVal);

      // Now let's change the key's associated value to something else
      *pVal = 11;
      printf("B: Changed value associated with key \"Ten\" to 11, just for fun.\n");
   }
   else printf("B: Weird, table didn't contain any key named \"Ten\" !?\n");

   printf("\n");

   // Now let's remove a key/value pair from the table
   int removedValue;
   if (table.Remove("Fifteen", removedValue).IsOK())
   {
      printf("C: Removed key \"Fifteen\" from the table, and its associated value (%i)\n", removedValue);
   }
   else printf("C: Weird, key \"Fifteen\" wasn't in the table!?\n");
   
   printf("\n");

   // We can remove a key/value pair without even caring what the value was...
   if (table.Remove("Eight").IsOK())
   {
      printf("D: Removed key \"Eight\" from the table\n");
   }
   else printf("D: table.Remove(\"Eight\") failed!?\n");

   printf("\n");
   printf("After our changes, the table now has " UINT32_FORMAT_SPEC " items.  Its current contents are:\n", table.GetNumItems());
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   // Removing all key/value pairs from a Hashtable is straightforward:
   table.Clear();

   printf("\n");
   return 0;
}
