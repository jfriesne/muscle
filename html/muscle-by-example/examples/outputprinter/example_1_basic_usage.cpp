#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/OutputPrinter.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This example demonstrates basic usage of the muscle::OutputPrinter class\n");
   p.printf("by printing out this message in several different ways via OutputPrinter object.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   // Print directly to stdout
   PrintExampleDescription(stdout);

   // Print via LogTime()/LogPlain() calls
   PrintExampleDescription(MUSCLE_LOG_INFO);

   // "Print" into a String object
   String s;
   PrintExampleDescription(s);
   printf("After PrintExampleDescription(s) returned, the String contains [%s]\n", s());

   return 0;
}
