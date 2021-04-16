#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/RefCount.h"
#include "util/ObjectPool.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a RefCountable class in conjunction with an ObjectPool to minimize object (de)allocations at runtime\n");
   printf("\n");
}

static int _myClassCounter = 0;  // let's keep track of how many MyClass objects there are

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

   ~MyClass()
   {
      printf("MyClass destructor called, this=%p, _myClassCounter=%i\n", this, --_myClassCounter);
      if (_myClassCounter == 0) printf("\nAll MyClass objects have been destroyed, yay!\n");
   }
};
DECLARE_REFTYPES(MyClass);  // defines MyClassRef and ConstMyClassRef

// To avoid constant calls to new and delete, we'll set up this little "recycling program"
// Note that ObjectPool uses a slab-allocator, so it will allocate a bunch of objects at
// once when necessary (enough objects to fill up one 4KB page of RAM) and then dole them
// out as the program needs them.
static ObjectPool<MyClass> _myClassPool;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("At top of main\n");
   printf("\n");

   // Give ownership of new MyClass objects to a MyClassRef immediately
   // As soon as you've done that, you don't have to worry about leaks anymore
   MyClassRef mc1(_myClassPool.ObtainObject());
   MyClassRef mc2(_myClassPool.ObtainObject());

   printf("\n");

   // Simluate a program doing stuff that needs a lot of MyClass objects at the same time
   for (int i=0; i<100; i+=5)
   {
      // Grab a number of MyClass objects from the ObjectPool for our use here
      Queue<MyClassRef> q;
      for (int j=0; j<i; j++) q.AddTail(MyClassRef(_myClassPool.ObtainObject()));

      printf("   Iteration %i of the loop is (pretending to use) %i MyClass objects\n", i, i);
      q.Clear();  // not strictly necessary since (q) is about to go out of scope anyway
   }

   printf("\n");
   printf("At bottom of main()\n");
   printf("\n");
   return 0;
}
