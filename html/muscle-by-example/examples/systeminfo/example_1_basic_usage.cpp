#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "system/SystemInfo.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the SystemInfo API to gather system details.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Here's some information about your system:\n");

   printf("   This program is running under the following OS:  %s\n", GetOSName());

   uint32 numProcs;
   if (GetNumberOfProcessors(numProcs).IsOK())
   {
      printf("   This computer has " UINT32_FORMAT_SPEC " processing cores.\n", numProcs);
   }
   else printf("   Error retrieving number of processing cores on this computer!\n");

   printf("   The file-path separator this computer's OS is:  %s\n", GetFilePathSeparator());
   printf("\n");

   const char * pathTypeNames[NUM_SYSTEM_PATHS] = {
      "Current Directory:          ",
      "This Executable's Location: ",
      "Temp-files Directory:       ",
      "User's Home Directory:      ",
      "User's Desktop Directory:   ",
      "User's Documents Directory: ",
      "System's Root Directory:    "
   };
   for (uint32 i=0; i<ARRAYITEMS(pathTypeNames); i++)
   {
      String pathVal;
      if (GetSystemPath(i, pathVal).IsOK())
      {
         printf("   %s %s\n", pathTypeNames[i], pathVal());
      }
      else printf("Error retrieving %s\n", pathTypeNames[i]);
   }

   printf("\n");
   printf("Build flags our code was compiled with follow:\n");
   PrintBuildFlags();

   return 0;
}
