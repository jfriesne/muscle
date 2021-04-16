#include <math.h>    // for sin()

#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/TimeUtilityFunctions.h"
#include "util/DebugTimer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates various minor time-related features available in the TimeUtilityFunctions API\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   printf("The OnceEvery() function returns true once every so-many microseconds.  It can be used to generate output at a measured pace:\n");
   uint64 prevTime = 0;
   int count = 0;
   for (int i=0; i<100000000; i++)
   {
      count += 37;
      if (OnceEvery(MillisToMicros(200), prevTime)) printf("At i=%i, count is %i\n", i, count);
   }

   printf("\n");
   printf("The PRINT_CALLS_PER_SECOND macro will print out, twice per second, how many times per second it is being called:\n");
   for (int i=0; i<100000000; i++)
   {
      count += 37;
      PRINT_CALLS_PER_SECOND("wow"); 
   }

   printf("\n");
   printf("The DebugTimer will tell you how long it lived for (useful for measuring how long a block of code took to execute):\n");
   {
      DebugTimer tm("timer", 0);
      for (int i=0; i<100000000; i++) count += (int) sin(i);
   }

   printf("\n");
   printf("The DebugTimer can also be set to various modes, and at the end will tell you how long it spent in each mode:\n");
   {
      DebugTimer tm("timer", 0);

      tm.SetMode(0);
      for (int i=0; i<10000000; i++) count += (int) sin(i);

      tm.SetMode(1);
      for (int i=0; i<10000000; i++) count += (int) cos(i);

      tm.SetMode(2);
      for (int i=0; i<10000000; i++) count += (int) tan(i);
   }


   printf("\n");
   printf("Final count is %i\n", count);  // this line is just here to keep the optimizer from out-clevering me
   printf("\n");

   return 0;
}
