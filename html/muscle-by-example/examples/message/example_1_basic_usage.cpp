#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates basic usage of the muscle::Message class to store data.\n");
   p.printf("\n");
}

enum {
   COMMAND_CODE_ORDER_PIZZA = 1887074913 // 'pzza' (arbitrary value generated by muscle/test/calctypecode.cpp)
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   // Let's create a Message and add some data to it
   Message orderPizzaMsg(COMMAND_CODE_ORDER_PIZZA);
   (void) orderPizzaMsg.AddInt32( "size_inches", 16);       // Error checking ommitted for clarity
   (void) orderPizzaMsg.AddBool(  "vegan",       false);    // feh!
   (void) orderPizzaMsg.AddString("toppings",    "cheese");
   (void) orderPizzaMsg.AddString("toppings",    "pepperoni");
   (void) orderPizzaMsg.AddString("toppings",    "mushrooms");
   (void) orderPizzaMsg.AddFloat( "price",       16.50f);   // in this scenario, the user gets to specify the price he wants to pay!?

   // Let's review our order
   printf("Our pizza-order Message is:\n");
   orderPizzaMsg.Print(stdout);

   // Now let's flatten the Message into a ByteBuffer and see what it looks like as flattened data
   ByteBuffer buf(orderPizzaMsg.FlattenedSize());
   (void) orderPizzaMsg.FlattenToByteBuffer(buf);

   printf("\n");
   printf("In Flattened/serialized form, the data looks like this:\n");
   PrintHexBytes(stdout, buf);

   // Next we'll parse the flattened bytes back in to a separate Message object, just to show that we can
   Message anotherMsg;
   if (anotherMsg.UnflattenFromByteBuffer(buf).IsOK())
   {
      printf("\n");
      printf("Unflattened the ByteBuffer back into anotherMsg.  anotherMsg now contains this:\n");
      anotherMsg.Print(stdout);
   }
   else printf("Error, unable to Unflatten the byte-buffer back to anotherMsg?!\n");

   printf("\n");

   printf("What-code of (anotherMsg) is " UINT32_FORMAT_SPEC " (aka '%s')\n", anotherMsg.what, GetTypeCodeString(anotherMsg.what)());
   printf("\n");
   printf("ORDER SUMMARY:\n");

   // And finally we'll extract some values from (anotherMsg) programatically, just to demonstrate how
   int32 sizeInches;
   if (anotherMsg.FindInt32("size_inches", sizeInches).IsOK())
   {
      printf("The customer wants a " INT32_FORMAT_SPEC "-inch pizza.\n", sizeInches);
   }
   else printf("size_inches wasn't specified!?\n");

   // Here's a convenient way of retrieving a value from the Message, or a default value if no value is present
   const float price = anotherMsg.GetFloat("price", 19.99f);
   printf("The user expects to pay $%.02f for this pizza.\n", price);
   printf("The pizza is to be %s\n", anotherMsg.GetBool("vegan") ? "VEGAN" : "non-vegan");

   // And we'll list out all of the toppings (note multiple values in a single field here!)
   String nextTopping;
   for (int32 i=0; anotherMsg.FindString("toppings", i, nextTopping).IsOK(); i++)
   {
      printf("User specified topping:  %s\n", nextTopping());
   }

   printf("\n");
   return 0;
}
