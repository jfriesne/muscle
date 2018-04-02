#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the basic functionality of the Log() and LogTime() functions.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   LogTime(MUSCLE_LOG_INFO,    "This is an informational message.\n");
   LogTime(MUSCLE_LOG_WARNING, "This is a warning message.\n");
   LogTime(MUSCLE_LOG_ERROR,   "This is an error message.\n");
   LogTime(MUSCLE_LOG_CRITICALERROR, "This is a critical error message.\n");

   const int numTypes  = 42;
   const float percent = 95.0f;

   LogTime(MUSCLE_LOG_INFO, "Log messages can have %s-style string interpolation ins them.\n", "printf");
   LogTime(MUSCLE_LOG_INFO, "Including all of the %i standard percent-token-types that %.0f%% of people expect.\n", numTypes, percent);

   LogTime(MUSCLE_LOG_INFO, "You can generate your log-lines");
   Log(MUSCLE_LOG_INFO, " across multiple function-calls");
   Log(MUSCLE_LOG_INFO, " by calling Log() instead of LogTime()\n");

   LogTime(MUSCLE_LOG_DEBUG, "Default log threshold is MUSCLE_LOG_INFO, which is why you don't see this line printed.\n");
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);  // lower the threshold for stdout-output to MUSCLE_LOG_DEBUG or greater
   LogTime(MUSCLE_LOG_DEBUG, "... but after calling SetConsoleLogLevel(MUSCLE_LOG_DEBUG), debug-level output will appear on stdout.\n");
   SetConsoleLogLevel(MUSCLE_LOG_TRACE);  // lower the threshold for stdout-output to MUSCLE_LOG_TRACE or greater
   LogTime(MUSCLE_LOG_TRACE, "... same thing goes for MUSCLE_LOG_TRACE-level output (which is suppressed by default)\n");
   
   return 0;
}
