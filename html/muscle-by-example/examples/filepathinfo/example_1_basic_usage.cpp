#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/FilePathInfo.h"
#include "util/String.h"
#include "util/TimeUtilityFunctions.h"  // for GetHumanReadableTimeString()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::FilePathInfo class\n");
   printf("by printing out info about the files/directories specified by its argument(s).\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   if (argc < 2)
   {
      printf("Usage:  ./example_1_basic_usage [filepath] [...]\n");
      return 5;
   }

   for (int i=1; i<argc; i++)
   {
      if (i>1) printf("\n");
      printf("For file path [%s]:\n", argv[i]);

      const FilePathInfo fpi(argv[i]);
      printf("   fpi.Exists()              returned:  %i\n", fpi.Exists());
      printf("   fpi.IsRegularFile()       returned:  %i\n", fpi.IsRegularFile());
      printf("   fpi.IsDirectory()         returned:  %i\n", fpi.IsDirectory());
      printf("   fpi.IsSymLink()           returned:  %i\n", fpi.IsSymLink());
      printf("   fpi.GetFileSize()         returned:  " UINT64_FORMAT_SPEC "\n", fpi.GetFileSize());
      printf("   fpi.GetAccessTime()       returned:  " UINT64_FORMAT_SPEC " (aka %s)\n", fpi.GetAccessTime(),       GetHumanReadableTimeString(fpi.GetAccessTime())());
      printf("   fpi.GetModificationTime() returned:  " UINT64_FORMAT_SPEC " (aka %s)\n", fpi.GetModificationTime(), GetHumanReadableTimeString(fpi.GetModificationTime())());
      printf("   fpi.GetCreationTime()     returned:  " UINT64_FORMAT_SPEC " (aka %s)\n", fpi.GetCreationTime(),     GetHumanReadableTimeString(fpi.GetCreationTime())());
   }

   printf("\n");
   return 0;
}
