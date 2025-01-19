#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/RefCount.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This demonstrates the use of a \"dummy Ref\" in conjunction with a stack object.\n");
   p.printf("This Ref won't ever call delete on the pointer you pass in to its constructor\n");
   p.printf("\n");
}

class MyClass : public RefCountable
{
public:
   MyClass()  {printf("MyClass constructor called for object %p\n", this);}
   ~MyClass() {printf("MyClass destructor called for object %p\n", this);}

   void SayHello() {printf("MyClass object %p says hello!\n", this);}
};
DECLARE_REFTYPES(MyClass);  // defines MyClassRef and ConstMyClassRef

void SomeFunctionThatTakesAMyClassRef(const MyClassRef & myClassRef)
{
   myClassRef()->SayHello();
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   // Say we've got an API (like SomeFunctionThatTakesAMyClassRef(), above)
   // that takes a MyClassRef as an argument.  As long as our MyClass object
   // is on the heap, we can call it the standard way:
   MyClassRef mc1(new MyClass);
   SomeFunctionThatTakesAMyClassRef(mc1);

   // But what if our MyClass object is on the stack?
   MyClass stackObject;

   // ... and we *still* want to pass it to SomeFunctionThatTakesAMyClassRef()?

#ifdef COMMENTED_OUT_BECAUSE_THIS_WOULD_CAUSE_UNDEFINED_BEHAVIOR
   // We mustn't do this, because the MyClassRef would try to
   // delete the stackObject when it was done, and that would
   // cause a crash or other undefined behavior.
   MyClassRef thisIsABadIdea(&stackObject);
#endif

   // But we can do this.  The "Dummy" version of the Ref
   // class knows never to try to delete or recycle anything;
   // instead, it should just act like a raw C pointer
   // and leave the object's destruction up to the calling code
   // to handle.
   DummyMyClassRef stackRef(stackObject);

   // Now we can pass the object to our function.
   SomeFunctionThatTakesAMyClassRef(stackRef);

   // Note that this is still a bit dangerous, because if
   // SomeFunctionThatTakesAMyClassRef() were to retain a
   // copy of the passed in MyClassRef object (e.g. copy it
   // into a static variable or something) then it is liable
   // to end up holding a dangling-pointer after 'stackObject'
   // is destroyed.  So use this technique with caution, as
   // it introduces the same hazards as using raw C pointers.

   return 0;
}
