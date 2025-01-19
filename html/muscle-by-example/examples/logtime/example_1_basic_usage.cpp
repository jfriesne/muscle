#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates the basic functionality of the LogPlain() and LogTime() functions.\n");
   p.printf("\n");
}

static int SomeFunction(int val)
{
   printf("SomeFunction was called with val=%i\n", val);
   return val;
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   LogTime(MUSCLE_LOG_INFO,    "This is an informational message.\n");
   LogTime(MUSCLE_LOG_WARNING, "This is a warning message.\n");
   LogTime(MUSCLE_LOG_ERROR,   "This is an error message.\n");
   LogTime(MUSCLE_LOG_CRITICALERROR, "This is a critical error message.\n");

   const int numTypes  = 42;
   const float percent = 95.2f;

   LogTime(MUSCLE_LOG_INFO, "Log messages can have [%s]-style string interpolation ins them.\n", "printf");
   LogTime(MUSCLE_LOG_INFO, "Including all of the %i standard percent-token-types that %.1f%% of people expect.\n", numTypes, percent);

   LogTime( MUSCLE_LOG_INFO, "You can generate your log-lines ");
   LogPlain(MUSCLE_LOG_INFO, "across multiple function-calls ");
   LogPlain(MUSCLE_LOG_INFO, "by calling LogPlain() instead of LogTime()\n");

   LogTime(MUSCLE_LOG_DEBUG, "Default log threshold is MUSCLE_LOG_INFO, which is why you don't see this line printed.\n");
   LogTime(MUSCLE_LOG_DEBUG, "Filtered LogTime() calls don't evaluate their arguments, so SomeFunction(%i) isn't called here!\n", SomeFunction(5));

   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);  // lower the threshold for stdout-output to MUSCLE_LOG_DEBUG or greater
   LogTime(MUSCLE_LOG_DEBUG, "... but after calling SetConsoleLogLevel(MUSCLE_LOG_DEBUG), debug-level output will appear on stdout.\n");
   SetConsoleLogLevel(MUSCLE_LOG_TRACE);  // lower the threshold for stdout-output to MUSCLE_LOG_TRACE or greater
   LogTime(MUSCLE_LOG_TRACE, "... same thing goes for MUSCLE_LOG_TRACE-level output (which is suppressed by default)\n");

   return 0;
}
