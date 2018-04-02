#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NestCount.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic usage of the muscle::NestCount and muscle::NestCountGuard\n");
   printf("classes, by showing how DoSomethingElse() can behave differently based on who called it.\n");
   printf("\n");
}

class MyClass
{
public:
   MyClass() {/* empty */}

   void DoSomething()
   {
      NestCountGuard ncg(_inDoSomething);  // constructor increments the NestCount, destructor decrements it!

      printf("DoSomething() was called, and is about to call DoSomethingElse()\n");
      DoSomethingElse();
   }

   void DoSomethingElse()
   {
      // DoSomethingElse() can now tell whether it is being called by DoSomething() or by 
      // someone else, and take the appropriate action based on its context.
      if (_inDoSomething.IsInBatch()) printf("DoSomethingElse():  I was called by DoSomething()\n");
                                 else printf("DoSomethingElse():  I was called from somewhere else\n");
   }

private:
   NestCount _inDoSomething;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   MyClass c;

   printf("main() calling c.DoSomething():\n");
   c.DoSomething();

   printf("\n");
   printf("main() calling c.DoSomethingElse();\n");
   c.DoSomethingElse();

   printf("\n");
   return 0;
}
