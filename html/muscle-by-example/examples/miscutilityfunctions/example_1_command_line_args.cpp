#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates how command-line arguments get parsed into a Message by ParseArgs().\n");
   p.printf("\n");
   p.printf("Try running this program with various command line arguments\n");
   p.printf("e.g. ./example_1_command_line_args foo bar baz=blorp baz=burf\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc == 1)
   {
      PrintExampleDescription(stdout);
      return 5;
   }

   printf("argc=%i\n", argc);
   for (int i=0; i<argc; i++) printf("   argv[%i] = %s\n", i, argv[i]);

   printf("\n");
   printf("ParseArgs() parsed those arguments into a Message that looks like this:\n");
   printf("\n");

   Message msg;
   if (ParseArgs(argc, argv, msg).IsOK()) msg.Print(stdout);
                                     else printf("ParseArgs() failed!\n");

   return 0;
}
