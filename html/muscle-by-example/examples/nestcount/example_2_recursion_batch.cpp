#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NestCount.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates how a NestCount and NestCountGuard can be used to\n");
   printf("run special routines at the beginning and/or the end of the execution of a\n");
   printf("a tree of recursive-method-calls.\n");
   printf("\n");
}

class MyClass
{
public:
   MyClass() {/* empty */}

   void RecursiveMethod(int maxDepth, int leftPadLen = 1)
   {
      NestCountGuard ncg(_inRecursiveMethod);

      if (ncg.IsOutermost())
      {
         // Our special enter-the-recursion-subtree code, perhaps do some one-time setup here?
         printf("---- AT BEGINNING OF BATCH OPERATION ----\n");
      }

      const String padStr = String().Pad(leftPadLen);

      printf("%sA RecursiveMethod() is currently at recursion depth " UINT32_FORMAT_SPEC "\n", padStr(), _inRecursiveMethod.GetCount());

      if (leftPadLen < maxDepth) RecursiveMethod(maxDepth, leftPadLen+1);  // RECURSE HERE!

      printf("%sB RecursiveMethod() is currently at recursion depth " UINT32_FORMAT_SPEC "\n", padStr(), _inRecursiveMethod.GetCount());

      if (ncg.IsOutermost())
      {
         // Our special enter-the-recursion-subtree code, perhaps do some one-time cleanup here?
         printf("---- AT BEGINNING OF BATCH OPERATION ----\n");
      }
   }

private:
   NestCount _inRecursiveMethod;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   MyClass c;

   printf("main() calling c.RecursiveMethod() the first time:\n");
   c.RecursiveMethod(5);

   printf("\n");
   printf("main() calling c.RecursiveMethod() the second time:\n");
   c.RecursiveMethod(10);

   printf("\n");
   return 0;
}
