#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Hashtable.h"
#include "util/String.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateUDPSocket()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates how to use various types as keys in a Hashtable.\n");
   printf("The basic rules are:  simple POD types will \"just work\", but for non-POD\n");
   printf("types (e.g. classes) you need to define the == operator and a method named\n");
   printf("\"uint32 HashCode() const\" that returns a hash code for the object.\n");
   printf("\n");
}

/** Example of a class that is suitable for use as a Key type in a Hashtable */
class MyKeyClass
{
public:
   MyKeyClass() : _val1(0), _val2(0) {/* empty */}
   MyKeyClass(int val1, int val2) : _val1(val1), _val2(val2) {/* empty */}

   bool operator == (const MyKeyClass & rhs) const
   {
      return ((_val1 == rhs._val1)&&(_val2 == rhs._val2));
   }

   uint32 HashCode() const
   {
      // CalculateHashCode() is a nice utility function in MuscleSupport.h
      // that uses the MurmurHash2 algorithm to give well-behaved hash results.
      // You could just return (_val1+_val2) and that would work, but using
      // a lousy hash function like that could make your Hashtable lookups
      // less efficient.
      return CalculateHashCode(_val1) + CalculateHashCode(_val2);
   }

   // Just for easier debug output
   String ToString() const
   {
      return String("MyKeyClass(%1,%2)").Arg(_val1).Arg(_val2);
   }

private:
   int _val1;
   int _val2;
};

/* This little program demonstrates the semantics of the keys used in a Hashtable */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // In order to use a type as a Key in a Hashtable, that type must have
   // two properties:  There must be a way to compute a hash-code for any
   // give Key object, and it must be possible to compare two Key objects
   // for equality (using the == operator).
   //
   // If you want to call SortByKey() or use the type as a key in an 
   // OrderedKeysHashtable, the < operator must also be defined for the type.

   // For primitive/POD key types, the Hashtable class uses SFINAE
   // magic so that Hashtables with those keys will automatically "just work".
   // For example:
   Hashtable<int,    String> tableWithIntKeys;     // works!
   Hashtable<uint16, String> tableWithUint16Keys;  // works!
   Hashtable<uint32, String> tableWithUint32Keys;  // works!
   Hashtable<int32,  String> tableWithInt32Keys;   // works!
   Hashtable<char,   String> tableWithCharKeys;    // works!
   Hashtable<float,  String> tableWithFloatKeys;   // works! (but probably a bad idea!)
  
   // When using a "Proper class" as a Key type, you'll want to make
   // sure it has a working == operator and that it has a method
   // like this one defined:  
   //
   //   uint32 HashCode() const;
   // 
   // This method should return a 32-bit value based on the object's
   // current state; that value will be used to place the key/value
   // pair within the Hashtable's array.

   Hashtable<MyKeyClass, int> myTable;
   (void) myTable.Put(MyKeyClass(12, 23), 0);
   (void) myTable.Put(MyKeyClass(21, 22), 5);
   (void) myTable.Put(MyKeyClass(37, 19), 6);

   printf("myTable's contents are:\n");
   for (HashtableIterator<MyKeyClass, int> iter(myTable); iter.HasData(); iter++)
   {
      printf("   [%s] -> %i\n", iter.GetKey().ToString()(), iter.GetValue());
   }

   printf("\n");

   // Test retrieving a value using a MyKeyClass object as the key
   int retVal;
   if (myTable.Get(MyKeyClass(21, 22), retVal).IsOK()) 
   {
      printf("myTable.Get(MyKeyClass(21, 22) retrieved a key with value %i\n", retVal);
   }
   else printf("myTable.Get(MyKeyClass(21, 22) failed!\n");

   // You can even use pointers-to-objects as your keys as long as
   // the pointed-to-objects can be used as keys.  The pointer-keys
   // will generally work the same as the value-keys, but note that
   // you are responsible for making sure the pointers remain valid 
   // for the lifetime of the Hashtable!

   String s1 = "One";
   String s2 = "Two";
   String s3 = "Three";
   Hashtable<String *, int> ptrTable;
   ptrTable.Put(&s1, 1);
   ptrTable.Put(&s2, 2);
   ptrTable.Put(&s3, 3);

   printf("\n");
   printf("ptrTable's contents are:\n");
   for (HashtableIterator<String *, int> iter(ptrTable); iter.HasData(); iter++)
   {
      printf("   %s -> %i\n", iter.GetKey()->Cstr(), iter.GetValue());
   }

   printf("\n");

   // Refs can also be used as keys, if, you're in to that sort of thing.
   // Here's a Hashtable with ConstSocketRefs as keys!
   Hashtable<ConstSocketRef, Void> sockTable;
   for (int i=0; i<10; i++) sockTable.PutWithDefault(CreateUDPSocket());

   printf("sockTable's contents are:\n");
   for (HashtableIterator<ConstSocketRef, Void> iter(sockTable); iter.HasData(); iter++)
   {
      const Socket * s = iter.GetKey()();
      printf("   socket descriptor #%i\n", s ? s->GetFileDescriptor() : -1);
   }

   printf("\n");

   return 0;
}
