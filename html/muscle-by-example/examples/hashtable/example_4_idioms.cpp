#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates  various minor convenience-methods in the Hashtable class.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   Hashtable<String, int> table;

   // If we know up-front a limit on the number of items we are likely to place
   // into the table, we can reserve that many slots in advance, and thereby 
   // avoid any chance of the Hashtable having to reallocate its internal array 
   // while we are adding items to it.  
   //
   // That avoids some inefficiency, and it also means we don't have
   // to worry about out-of-memory errors, or the memory-locations of key or 
   // value-items changing, while populating the table.

   if (table.EnsureSize(20).IsError()) MWARN_OUT_OF_MEMORY;

   // Put some initial data into the table
   table.Put("One", 1);
   table.Put("Two", 2);

   // table.GetWithDefault() returns a reference to the value of the specified
   // key, or a reference to a default-constructed value otherwise.
   const int & oneRef   = table.GetWithDefault("One");
   const int & twoRef   = table.GetWithDefault("Two");
   const int & threeRef = table.GetWithDefault("Three");
   printf("A: OneRef=%i twoRef=%i threeRef=%i\n", oneRef, twoRef, threeRef);

   printf("\n");

   // The [] operator is a synonym for GetWithDefault()
   printf("B: table[\"One\"]=%i table[\"Two\"]=%i table[\"Three\"]=%i\n", table["One"], table["Two"], table["Three"]);

   printf("\n");

   // GetOrPut() returns a pointer to the value of the given key, if the key is present
   // If the key isn't present, it places a key/value pair into the Hashtable and returns
   // a pointer to the (default-constructed) placed value.  This is very useful when 
   // demand-allocating records.
   int * pEight = table.GetOrPut("Eight");
   printf("C:  table.GetOrPut(\"Eight\") returned %p\n", pEight);
   if (pEight) *pEight = 8;
          else MWARN_OUT_OF_MEMORY;   // GetOrPut() returns NULL only on memory-exhaustion

   printf("\n");

   // The next time we call GetOrPut() we'll get a pointer to the existing value
   pEight = table.GetOrPut("Eight");
   printf("C:  Second call to table.GetOrPut(\"Eight\") returned %p (aka %i)\n", pEight, pEight?*pEight:666);

   printf("\n");

   // We can also call GetOrPut() with a suggested default-value which will be
   // placed into the key/value pair if the supplied key isn't already present.
   int * pNine = table.GetOrPut("Nine", 9);
   printf("C:  table.GetOrPut(\"Nine\", 9) returned %p (aka %i)\n", pNine, pNine?*pNine:666);

   printf("\n");

   // PutAndGet() is similar to GetOrPut() except it *always* places a value.
   // (if the key already existed in the table, its value will be overwritten)
   int * pTen = table.PutAndGet("Ten", 10);
   printf("D:  table.PutAndGet(\"Ten\", 10) returned %p (aka %i)\n", pTen, pTen?*pTen:666);

   // Demonstrate PutAndGet()'s overwrite of the previous value
   pTen = table.PutAndGet("Ten", 11);
   printf("E:  table.PutAndGet(\"Ten\", 11) returned %p (aka %i)\n", pTen, pTen?*pTen:666);

   // If you want a Hashtable with keys only and don't need values at all
   // (similar to e.g. a std::unordered_set<>), a good way to get that is
   // to use the Void class as your value-type.  A Void object is just a
   // placeholder that contains no data.
   Hashtable<String, Void> keysOnlyTable;
   (void) keysOnlyTable.PutWithDefault("Blue");
   (void) keysOnlyTable.PutWithDefault("Red");
   (void) keysOnlyTable.PutWithDefault("Green");

   printf("\n");

   return 0;
}
