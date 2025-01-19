#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/OutputPrinter.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates the indentation abilility of the muscle::OutputPrinter class\n");
   p.printf("by printing out text from a simple recursive function\n");
   p.printf("\n");
}

static void RecursivePrint(const OutputPrinter & p, int recurseCount)
{
   if (recurseCount > 0)
   {
      p.printf("RecursivePrint called with recurseCount=%i, about to recurse\n", recurseCount);
      RecursivePrint(p.WithIndent(3), recurseCount-1);
      p.printf("RecursivePrint recursive call returned (recurseCount=%i)\n", recurseCount);
   }
   else p.printf("Recursion base case reached!\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   RecursivePrint(MUSCLE_LOG_INFO, 5);
   return 0;
}
