#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for GetHumanReadableTimeString()
#include "util/String.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the wall/calendar-clock provided by GetCurrentTime64()\n");
   printf("\n");
   printf("Try changing your computer's system-clock date/time while this program is running.\n");
   printf("You should see the output of this program change when you do that.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   Snooze64(SecondsToMicros(1));

   while(true)
   {
      printf("\n");

      const uint64 nowLocal = GetCurrentTime64(MUSCLE_TIMEZONE_LOCAL);
      printf("Current Local time (micros-since-1970) is:  " UINT64_FORMAT_SPEC ", aka %s\n", nowLocal, GetHumanReadableTimeString(nowLocal, MUSCLE_TIMEZONE_LOCAL)());
      HumanReadableTimeValues localVals;
      if (GetHumanReadableTimeValues(nowLocal, localVals, MUSCLE_TIMEZONE_LOCAL).IsOK())
      {
         printf("  Local HumanReadableTimeValues=[%s]\n", localVals.ToString()());
      }

      const uint64 nowUTC = GetCurrentTime64(MUSCLE_TIMEZONE_UTC);  // default argument shown explicitly here for clarity
      printf("Current UTC   time (micros-since-1970) is:  " UINT64_FORMAT_SPEC ", aka %s\n", nowUTC, GetHumanReadableTimeString(nowUTC, MUSCLE_TIMEZONE_LOCAL)());
      HumanReadableTimeValues utcVals;
      if (GetHumanReadableTimeValues(nowUTC, utcVals, MUSCLE_TIMEZONE_LOCAL).IsOK())
      {
         printf("  UTC HumanReadableTimeValues=[%s]\n", utcVals.ToString()());
      }

      Snooze64(MillisToMicros(500));  // wait 500mS
   }

   return 0;
}
