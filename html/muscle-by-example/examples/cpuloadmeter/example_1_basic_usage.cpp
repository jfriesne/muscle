#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/CPULoadMeter.h"
#include "util/TimeUtilityFunctions.h" // for MillisToMicros()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates using the CPULoadMeter class to measure the host computer's CPU usage percentage.\n");
   printf("\n");
}

/* This little program demonstrates basic usage of the muscle::CPULoadMeter class */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   CPULoadMeter meter;
   while(true)
   {
      Snooze64(MillisToMicros(250));
      LogTime(MUSCLE_LOG_INFO, "Current CPU usage is %.0f%%\n", meter.GetCPULoad()*100.0f);
   }

   return 0;
}
