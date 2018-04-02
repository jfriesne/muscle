#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Directory.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::Directory class\n");
   printf("by printing out the contents of the directory/directories specified by its argument(s).\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   if (argc < 2)
   {
      printf("Usage:  ./example_1_basic_usage [directory] [...]\n");
      return 5;
   }

   for (int i=1; i<argc; i++)
   {
      Directory di(argv[i]);

      if (i>1) printf("\n");
      printf("Directory at path [%s] contains the following entries:\n", di.GetPath());
      while(di.GetCurrentFileName())
      {
         printf("   %s\n", di.GetCurrentFileName());
         di++;
      }
   }

   printf("\n");

   return 0;
}
