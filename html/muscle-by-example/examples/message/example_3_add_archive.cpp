#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("In this program, we demonstrate the AddArchiveMessage()/FindArchiveMessage() idiom for mapping a C++ class to a sub-Message\n");
   printf("\n");
}

enum {
   COMMAND_CODE_ORDER_PIZZA = 1887074913, // 'pzza' (arbitrary value generated by muscle/test/calctypecode.cpp)
   COMMAND_CODE_DELIVER_INFO
};

/** This class represents the delivery information necessary to deliver a pizza.  We'll use it to demonstrate
  * the SaveToArchive()/SetFromArchive() idiom and how that can be used in conjunction with the Message class.
  */
class DeliveryInfo
{
public:
   DeliveryInfo() : _zipCode(0)
   {
      // empty
   }

   DeliveryInfo(const String & name, const String & address, const String & city, const String & state, int32 zipCode)
      : _name(name), _address(address), _city(city), _state(state), _zipCode(zipCode)
   {
      // empty
   }

   // Idiom:  SaveToArchive() saves our current state into the specified Message object
   status_t SaveToArchive(Message & msg) const
   {
      // Note that | operator means these Add*() calls won't necessarily be executed in the order they are
      // listed in -- so don't use it if order-of-additions is important (e.g. when adding multiple values
      // to the same field-name)
      return msg.AddString("name",    _name)
           | msg.AddString("address", _address)
           | msg.AddString("city",    _city)
           | msg.AddString("state",   _state)
           | msg.AddInt32("zip_code", _zipCode);
   }

   // Idiom:  SetFromArchive() replaces our current state with the state held in the specified Message object
   status_t SetFromArchive(const Message & msg)
   {
      _name    = msg.GetString("name");
      _address = msg.GetString("address");
      _city    = msg.GetString("city");
      _state   = msg.GetString("state");
      _zipCode = msg.GetInt32("zip_code");
      return B_NO_ERROR;
   }

   // Print our current state to stdout
   void Print(const OutputPrinter & p) const
   {
      p.printf("      name = %s\n", _name());
      p.printf("   address = %s\n", _address());
      p.printf("      city = %s\n", _city());
      p.printf("     state = %s\n", _state());
      p.printf("  ZIP code = " INT32_FORMAT_SPEC "\n", _zipCode);
   }

private:
   String _name;
   String _address;
   String _city;
   String _state;
   int32 _zipCode;
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

   // Now let's add the user's delivery info
   DeliveryInfo deliveryInfo("Hungry Joe", "20 West Montecito Ave", "Sierra Madre", "California", 91024);
   (void) orderPizzaMsg.AddArchiveMessage("delivery_info", deliveryInfo);  // calls GetMessageFromPool(), SaveToArchive(), and AddMessage() for us

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
   DeliveryInfo anotherDeliveryInfo;
   if (anotherMsg.FindArchiveMessage("delivery_info", anotherDeliveryInfo).IsOK())
   {
      printf("\n");
      printf("DELIVER TO:\n");
      anotherDeliveryInfo.Print(stdout);
   }
   else printf("No delivery_info sub-Message was present in (anotherMsg) !?\n");

   printf("\n");
   return 0;
}
