#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/RefCount.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic usage of the muscle::Ref and muscle::RefCountable classes\n");
   printf("\n");
}

static int _myClassCounter = 0;  // this will keep track of how many MyClass objects currently exist

/** An example of a class we want to allocate objects of from the heap,
  * but still avoid any risk of memory leaks.
  */
class MyClass : public RefCountable
{
public:
   MyClass()
   {
      printf("MyClass default-constructor called, this=%p, _myClassCounter=%i\n", this, ++_myClassCounter);
   }

   MyClass(const MyClass & rhs)
   {
      printf("MyClass copy-constructor called, this=%p, rhs=%p, _myClassCounter=%i\n", this, &rhs, ++_myClassCounter);
   }

   void SayHello()
   {
      printf("MyClass object %p says hi!\n", this);
   }

   ~MyClass()
   {
      printf("MyClass destructor called, this=%p, _myClassCounter=%i\n", this, --_myClassCounter);
      if (_myClassCounter == 0) printf("\nAll MyClass objects have been destroyed, yay!\n");
   }
};
DECLARE_REFTYPES(MyClass);  // defines MyClassRef and ConstMyClassRef

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("At top of main\n");
   printf("\n");

   // Give ownership of new MyClass objects to a MyClassRef immediately
   // As soon as you've done that, you don't have to worry about leaks anymore
   MyClassRef mc1(new MyClass);
   MyClassRef mc2(new MyClass);

   // To get a pointer to the referenced object, you can call GetItemPointer()
   mc1.GetItemPointer()->SayHello();

   // But it's quicker to call the () operator which is a synonym for GetItemPointer()
   mc2()->SayHello();

   printf("\n");

   // Inner scope, just for demonstration purposes
   {
      printf("Entering inner scope\n");
      MyClassRef mc3(new MyClass);

      // It's okay to make copies of Ref objects as much as you want
      // (doing so doesn't copy the RefCountable they point to, but it 
      // does increase the RefCountable's reference count)
      MyClassRef mc4(mc1);
      MyClassRef mc5(mc2);
      MyClassRef mc6(mc3);

      printf("About to exit inner scope\n");
   }
   printf("Exited inner scope\n");

   printf("\n");

   // Re-setting a Ref to point to something else is ok
   printf("Re-targetting mc1 at a new object\n");
   mc1.SetRef(new MyClass);

   printf("\n");

   // Manually resetting a Ref to NULL is okay
   printf("Resetting mc1 to be a NULL ref\n");
   mc1.Reset();

   printf("\n");

   printf("At bottom of main()\n");
   printf("\n");
   return 0;
}
