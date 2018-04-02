#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"
#include "util/Directory.h"
#include "util/String.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates advanced file logging (including enforcement of\n");
   printf("maximum-log-file-sizes, log-file rotation, etc etc)\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   SetConsoleLogLevel(MUSCLE_LOG_TRACE); // log any messages of MUSCLE_LOG_TRACE or higher to stdout (i.e. log everything)

   (void) Directory::MakeDirectory("rotated_logs", true);  // make sure our logs-folder exists, otherwise we can't put logs there

   // Specify the parameters of our on-disk-logging
   const String logFileNamesPattern = "rotated_logs/rotated_log_file_%Y_%M_%D_%h_%m_%s.txt";
   SetFileLogName(logFileNamesPattern);        // tell the file-logger what kind of files to create
   SetOldLogFilesPattern(logFileNamesPattern); // tell the old-log-files-deleter what kind of old files he is allowed to delete
   SetFileLogMaximumSize(50000);               // don't allow a given log file to be more than 50000 bytes long
   SetMaxNumLogFiles(10);                      // only keep the most recent 30 log files, to avoid using up too much disk space
   //SetFileLogCompressionEnabled(true);       // .gz-compress each log file when closing it, to save disk space
   SetFileLogLevel(MUSCLE_LOG_TRACE);          // log any messages of MUSCLE_LOG_TRACE or higher to disk

   LogTime(MUSCLE_LOG_INFO, "Okay, we're ready to spam up the log files now... watch the rotated_logs sub-directory to see them all.\n");
   Snooze64(SecondsToMicros(3));

   uint32 count = 0;
   for (uint32 i=0; i<50; i++)
   {
      Snooze64(MillisToMicros(1100));  // just to give the timestamp time to increase by a second
      for (uint32 j=0; j<5000; j++)
      {
         LogTime(MUSCLE_LOG_INFO, "This is a spam log message, it's only here to demonstrate log rotation (Message #" UINT32_FORMAT_SPEC ")\n", count++);
      }
   }
   
   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Now that that's over, you can look in the rotated_logs sub-directory to see the final result.\n");

   return 0;
}
