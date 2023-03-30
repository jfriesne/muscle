#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "message/Message.h"
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("In this program, we demonstrate the AddFlat()/FindFlat() idiom for adding a C++ object to a Message as a flattened-byte-buffer.\n");
   printf("\n");
}

enum {
   COMMAND_CODE_ORDER_PIZZA = 1887074913, // 'pzza' (arbitrary value generated by muscle/test/calctypecode.cpp)
   COMMAND_CODE_DELIVER_INFO
};

/** This class represents the delivery information necessary to deliver a pizza.  We'll use it to demonstrate
  * the lower-level AddFlat()/FindFlat() method to add a class's data to a Message.  Note that this method
  * is a bit more elaborate to use and maintain, but it reduces the size of the resulting Message.
  */
class DeliveryInfo : public Flattenable
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

   // Returns false because we contain-variable-length Strings, and thus our FlattenedSize() method can return different values */
   virtual bool IsFixedSize() const {return false;}

   virtual uint32 TypeCode() const {return COMMAND_CODE_DELIVER_INFO;}

   // Returns the number of bytes our Flatten() method will write out if called on this object
   virtual uint32 FlattenedSize() const
   {
      return _name.FlattenedSize()    +
             _address.FlattenedSize() +
             _city.FlattenedSize()    +
             _state.FlattenedSize()   +
             sizeof(_zipCode);
   }

   virtual void Flatten(DataFlattener flat) const
   {
      flat.WriteFlat(_name);
      flat.WriteFlat(_address);
      flat.WriteFlat(_city);
      flat.WriteFlat(_state);
      flat.WriteInt32(_zipCode);
   }

   virtual status_t Unflatten(DataUnflattener & unflat)
   {
      _name    = unflat.ReadFlat<String>();
      _address = unflat.ReadFlat<String>();
      _city    = unflat.ReadFlat<String>();
      _state   = unflat.ReadFlat<String>();
      _zipCode = unflat.ReadInt32();
      return unflat.GetStatus();
   }

   // Print our current state to stdout
   void PrintToStream() const
   {
      printf("      name = %s\n", _name());
      printf("   address = %s\n", _address());
      printf("      city = %s\n", _city());
      printf("     state = %s\n", _state());
      printf("  ZIP code = " INT32_FORMAT_SPEC "\n", _zipCode);
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

   // Now let's add the user's delivery info using AddFlat() and a Flattenable DeliveryInfo class
   DeliveryInfo deliveryInfo("Hungry Joe", "20 West Montecito Ave", "Sierra Madre", "California", 91024);
   (void) orderPizzaMsg.AddFlat("delivery_info", deliveryInfo);  // calls GetMessageFromPool(), SaveToArchive(), and AddMessage() for us

   // Let's review our order
   printf("Our pizza-order Message is:\n");
   orderPizzaMsg.PrintToStream();

   // Now let's flatten the Message into a ByteBuffer and see what it looks like as flattened data
   ByteBuffer buf(orderPizzaMsg.FlattenedSize());
   (void) orderPizzaMsg.FlattenToByteBuffer(buf);

   printf("\n");
   printf("In Flattened/serialized form, the data looks like this:\n");
   PrintHexBytes(buf);

   // Next we'll parse the flattened bytes back in to a separate Message object, just to show that we can
   Message anotherMsg;
   if (anotherMsg.UnflattenFromByteBuffer(buf).IsOK())
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

   // And dig out the flattened DeliveryInfo object using FindFlat()
   DeliveryInfo anotherDeliveryInfo;
   if (anotherMsg.FindFlat("delivery_info", anotherDeliveryInfo).IsOK())
   {
      printf("\n");
      printf("DELIVER TO:\n");
      anotherDeliveryInfo.PrintToStream();
   }
   else printf("No delivery_info sub-Message was present in (anotherMsg) !?\n");

   printf("\n");
   return 0;
}
