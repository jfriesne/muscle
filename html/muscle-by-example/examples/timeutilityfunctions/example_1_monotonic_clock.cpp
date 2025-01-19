#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates the monotonic clock provided by GetRunTime64()\n");
   p.printf("\n");
   p.printf("Try changing your computer's system-clock date/time while this program is running.\n");
   p.printf("You should not see any effect on the output of this program when you do that.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   (void) Snooze64(SecondsToMicros(1));

   while(true)
   {
      printf("Current monotonic clock time is:  " UINT64_FORMAT_SPEC "\n", GetRunTime64());
      (void) Snooze64(MillisToMicros(200));  // wait 200mS
   }

   return 0;
}
