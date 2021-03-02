#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/RefCount.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates which Ref assignment-conversions are allowed and which are compile-time errors\n");
   printf("(the rules are very similar to the rules enforced by the compiler for raw pointers)\n");
   printf("\n");
}

/** An example of a class we want to allocate objects of from the heap,
  * but still avoid any risk of memory leaks.
  */
class MyBaseClass : public RefCountable
{
public:
   MyBaseClass() {}
   ~MyBaseClass() {}
};
DECLARE_REFTYPES(MyBaseClass);  // defines MyBaseClassRef and ConstMyBaseClassRef

/** A subclass of MyBaseClass */
class MySubClass : public MyBaseClass
{
public:
   MySubClass() {}
   ~MySubClass() {}
};
DECLARE_REFTYPES(MySubClass);  // defines MySubClassRef and ConstMySubClassRef

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   MyBaseClassRef mc1(new MyBaseClass);

   MyBaseClassRef mc2 = mc1;       // Copying a Ref is ok
   ConstMyBaseClassRef mc3 = mc1;  // Initializing a ConstRef from a Ref (adding Const) is ok

#ifdef COMMENTED_OUT_BECAUSE_THIS_WONT_COMPILE
   MyBaseClassRef mc4 = mc3; // Initializing a Ref from a ConstRef is a compile-time-error!
#endif

   // But if you absolutely MUST cast away const (and like to live dangerously), you can:
   MyBaseClassRef mc4 = CastAwayConstFromRef(mc3);  // danger will robinson!

   MySubClassRef sc1(new MySubClass);

   MyBaseClassRef mc5 = sc1;   // Initializing a base-class ref from a subclass-ref is ok

   RefCountableRef rc1 = sc1;  // Initializing a RefCountableRef from any Ref is always okay
   RefCountableRef rc2 = sc1.GetRefCountableRef(); // another way to do the same thing

#ifdef COMMENTED_OUT_BECAUSE_THIS_WONT_COMPILE
   MySubClassRef sc2 = mc1;   // Initializing a sub-class ref from a baseclass-ref is a compile-time-error!
#endif

   // But if you really want to down-cast a baseclass-ref to a subclass-ref, you can do it;
   MySubClassRef sc2(mc1.GetRefCountableRef(), true);
   // Note that sc2 may end up being a NULL Ref if the implicit dynamic_cast<SubClass*>(mc1()) call failed!

   // Another way to do the same thing
   MySubClassRef sc3;
   if (sc3.SetFromRefCountableRef(mc5.GetRefCountableRef()).IsOK())
   {
      printf("SetFromRefCountableRef succeeded, sc3 now points to MySubClass object %p\n", sc3());
   }
   else printf("SetFromRefCountableRef failed!  mc5 wasn't pointing to a MySubClass object!\n");

   // And if you're feeling super-aggressive, you can even do it without the
   // call to dynamic_cast -- but beware:  if you're wrong about the validity
   // of the downcast, you'll get undefined behavior here!
   MySubClassRef sc4;
   sc4.SetFromRefCountableRefUnchecked(mc5);

   printf("\n");
   return 0;
}
