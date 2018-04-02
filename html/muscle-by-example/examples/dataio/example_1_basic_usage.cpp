#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/StdinDataIO.h"
#include "dataio/FileDataIO.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic blocking-I/O usage of the muscle::DataIO interface\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("This program will accept input from stdin and write it to a file named example_1_dataio_output.txt.\n");
   printf("So go ahead and type whatever you want, and press CTRL-D when you are done.\n");

   const char * outputFileName = "example_1_dataio_output.txt";
   StdinDataIO stdinIO(true);
   FileDataIO fileOutputIO(fopen(outputFileName, "w"));
   if (fileOutputIO.GetFile() == NULL)
   {
      printf("Error opening file %s for writing!  Output to file will be disabled.\n", outputFileName);
   }

   char buf[1024];
   int numBytesRead;
   while((numBytesRead = stdinIO.Read(buf, sizeof(buf))) > 0)
   {
      printf("Read %i bytes from stdin--writing them to the output file.\n", numBytesRead);
      if (fileOutputIO.Write(buf, numBytesRead) != numBytesRead) printf("Write to the output file failed!?\n");
   }

   printf("Program exiting.  Enter \"cat %s\" to see the file we wrote.\n", outputFileName);
   return 0;
}
