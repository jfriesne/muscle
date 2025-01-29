#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/MiscUtilityFunctions.h"  // for GetInsecurePseudoRandomNumber()
#include "util/RefCount.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates a toy factory-function that randomly either\n");
   p.printf("succeeds and returns a valid reference to a newly constructed MyClass object,\n");
   p.printf("or fails and returns a B_ACCESS_DENIED error-code instead.\n");
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

MyClassRef MyFactoryFunction()
{
   if (GetInsecurePseudoRandomNumber(2) == 0)
   {
      return MyClassRef(new MyClass);
   }
   else return B_ACCESS_DENIED;  // simulate some kind of problem that prevents us from returning a valid/non-NULL Ref
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   srand((unsigned) time(NULL));

   MyClassRef ref = MyFactoryFunction();
   if (ref())
   {
      printf("MyFactoryFunction() succeeded, returned MyClassObject %p\n", ref());
   }
   else
   {
      printf("MyFactoryFunction() failed with error [%s]\n", ref.GetStatus()());
      if (ref.GetStatus() == B_ACCESS_DENIED) printf("Oh no, access was denied!\n");
   }

   return 0;
}
