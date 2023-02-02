#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/RefCount.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates a toy factory-function that randomly either\n");
   printf("succeeds and returns a valid reference to a newly constructed MyClass object,\n");
   printf("or fails and returns a B_ACCESS_DENIED error-code instead.\n");
   printf("\n");
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
   if ((rand()%2) == 0)
   {
      MyClassRef ret(newnothrow MyClass);
      MRETURN_OOM_ON_NULL(ret());  // print an error message and return B_OUT_OF_MEMORY if ret() is NULL
      return ret;  // success!
   }
   else return B_ACCESS_DENIED;  // simulate some kind of problem that prevents us from returning a valid/non-NULL Ref
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   srand(time(NULL));

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
