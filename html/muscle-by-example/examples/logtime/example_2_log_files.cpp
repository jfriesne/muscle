#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates basic logging to a file (in addition to stdout).\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   (void) SetFileLogName("example_2_log_file.txt");
   (void) SetFileLogLevel(MUSCLE_LOG_INFO);  // enable output-to-file of any lines with severity MUSCLE_LOG_INFO or higher

   LogTime(MUSCLE_LOG_INFO, "This text should appear both on stdout and in the file example_2_log_file.txt\n");
   LogTime(MUSCLE_LOG_INFO, "That way you can have a permanent record of all the things your program did wrong.\n");

   return 0;
}
