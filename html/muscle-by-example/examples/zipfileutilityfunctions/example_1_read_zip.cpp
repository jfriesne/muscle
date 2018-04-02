#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/ZipFileUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using ZipFileUtilityFunctions to read in and parse a .zip file\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   if (argc != 2)
   {
      LogTime(MUSCLE_LOG_INFO, "Usage:   ./example_1_read_zip some_zip_file.zip\n");
      return 10;
   }

   MessageRef msg = ReadZipFile(argv[1]);
   if (msg())
   {
      LogTime(MUSCLE_LOG_INFO, "Read file [%s] as a .zip file.  The contents of the file are:\n", argv[1]);
      msg()->PrintToStream();
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't read file [%s], perhaps it is not a .zip file?\n", argv[1]);

   return 0;
}
