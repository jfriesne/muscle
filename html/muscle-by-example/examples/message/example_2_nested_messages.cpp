#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates nesting of Messages by extending the previous Message example to include the user's address info in a sub-Message\n");
   printf("\n");
}

enum {
   COMMAND_CODE_ORDER_PIZZA = 1887074913, // 'pzza' (arbitrary value generated by muscle/test/calctypecode.cpp)
   COMMAND_CODE_DELIVERY_INFO
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's create a Message and add some data to it
   Message orderPizzaMsg(COMMAND_CODE_ORDER_PIZZA);
   (void) orderPizzaMsg.AddInt32( "size_inches", 16);       // Error checking ommitted for clarity
   (void) orderPizzaMsg.AddBool(  "vegan",       false);    // feh!
   (void) orderPizzaMsg.AddString("toppings",    "cheese");
   (void) orderPizzaMsg.AddString("toppings",    "pepperoni");
   (void) orderPizzaMsg.AddString("toppings",    "mushrooms");
   (void) orderPizzaMsg.AddFloat( "price",       16.50f);   // in this scenario, the user gets to specify the price he wants to pay!?

   // Now let's add the user's delivery info -- we'll use a MessageRef to avoid an unnecessary data-copy in the AddMessage() call, below
   MessageRef deliveryInfoMsg = GetMessageFromPool(COMMAND_CODE_DELIVERY_INFO);
   (void) deliveryInfoMsg()->AddString("name",    "Hungry Joe");
   (void) deliveryInfoMsg()->AddString("address", "20 West Montecito Ave");
   (void) deliveryInfoMsg()->AddString("city",    "Sierra Madre");
   (void) deliveryInfoMsg()->AddString("state",   "California");
   (void) deliveryInfoMsg()->AddInt32("zip_code", 91024);
   (void) orderPizzaMsg.AddMessage("delivery_info", deliveryInfoMsg);

   // Let's review our order
   printf("Our pizza-order Message is:\n");
   orderPizzaMsg.PrintToStream();

   // Now let's flatten the Message into a ByteBuffer and see what it looks like as flattened data
   const uint32 opmFlatSize = orderPizzaMsg.FlattenedSize();
   ByteBuffer buf(opmFlatSize);
   orderPizzaMsg.Flatten(buf.GetBuffer(), opmFlatSize);

   printf("\n");
   printf("In Flattened/serialized form, the data looks like this:\n");
   PrintHexBytes(buf);

   // Next we'll parse the flattened bytes back in to a separate Message object, just to show that we can
   Message anotherMsg;
   if (anotherMsg.Unflatten(buf.GetBuffer(), buf.GetNumBytes()).IsOK())
   {
      printf("\n");
      printf("Unflattened the ByteBuffer back into anotherMsg.  anotherMsg now contains this:\n");
      anotherMsg.PrintToStream();
   }
   else printf("Error, unable to Unflatten the byte-buffer back to anotherMsg?!\n");

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

   // And dig out the delivery-info sub-Message
   MessageRef anotherSubMsg;
   if (anotherMsg.FindMessage("delivery_info", anotherSubMsg).IsOK())
   {
      printf("\n");
      printf("DELIVER TO:\n");
      printf("      name = %s\n", anotherSubMsg()->GetString("name")());
      printf("   address = %s\n", anotherSubMsg()->GetString("address")());
      printf("      city = %s\n", anotherSubMsg()->GetString("city")());
      printf("     state = %s\n", anotherSubMsg()->GetString("state")());
      printf("  ZIP code = " INT32_FORMAT_SPEC "\n", anotherSubMsg()->GetInt32("zip_code"));
   }
   else printf("No delivery_info sub-Message was present in (anotherMsg) !?\n");

   printf("\n");
   return 0;
}
