#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates various ways to iterate over the contents of a Hashtable.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Hashtable<String,int> iteration examples\n");

   Hashtable<String, int> table;
   (void) table.Put("Three",    3);
   (void) table.Put("Five",     5);   // The (void) is just to indicate
   (void) table.Put("Eight",    8);   // that I am deliberately ignoring
   (void) table.Put("Ten",     10);   // the return values of the Put() calls
   (void) table.Put("Fifteen", 15);

   printf("\n");
   printf("Standard iteration:\n");
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");
   printf("Backwards iteration:\n");
   for (HashtableIterator<String, int> iter(table, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");
   printf("Iteration starting at key \"Eight\":\n");
   for (HashtableIterator<String, int> iter(table, "Eight", 0); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");
   printf("Backwards Iteration starting at key \"Eight\":\n");
   for (HashtableIterator<String, int> iter(table, "Eight", HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");
   printf("Iteration while removing any key/value pairs whose value is even:\n");
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      if ((iter.GetValue() % 2) == 0)
      {
         printf("   REMOVING PAIR [%s]->%i\n", iter.GetKey()(), iter.GetValue());
         (void) table.Remove(iter.GetKey());
      }
      else printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");
   printf("Final iteration (after removals)\n");
   for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
   {
      printf("   Key=[%s] -> Value=%i\n", iter.GetKey()(), iter.GetValue());
   }

   printf("\n");

   return 0;
}
