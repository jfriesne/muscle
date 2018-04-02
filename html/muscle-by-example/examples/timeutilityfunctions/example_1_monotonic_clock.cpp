#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the monotonic clock provided by GetRunTime64()\n");
   printf("\n");
   printf("Try changing your computer's system-clock date/time while this program is running.\n");
   printf("You should not see any effect on the output of this program when you do that.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   Snooze64(SecondsToMicros(1));

   while(true)
   {
      printf("Current monotonic clock time is:  " UINT64_FORMAT_SPEC "\n", GetRunTime64());
      Snooze64(MillisToMicros(200));  // wait 200mS
   }

   return 0;
}
