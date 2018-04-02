#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates sprintf()/QString-style variable interpolation using the .Arg() method\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const String truth = String("The moon is made of %1 cheese, and weighs %2 pounds.").Arg("green").Arg(42);
   printf("%s\n", truth());

   const float exactWeight = 42.75f;
   printf("%s\n", String("Wait, actually it weighs %1 pounds.").Arg(exactWeight)());
   printf("%s\n", String("More precisely, its weight is more like %1 pounds.").Arg(exactWeight, "%.03f")());

   printf("%s\n", String("You can interpolate the same argument (like %1) more than once if you want (%1, I say!)").Arg("foobar")());

   printf("%s\n", String("You can interpolate up to 9 arguments at a time:  [%1] [%2] [%3] [%4] [%4] [%5] [%6] [%7] [%8] [%9]").Arg("one").Arg("two").Arg("three").Arg("four").Arg("five").Arg("six").Arg(7.0f).Arg(8).Arg(9)());
   printf("\n");
   return 0;
}
