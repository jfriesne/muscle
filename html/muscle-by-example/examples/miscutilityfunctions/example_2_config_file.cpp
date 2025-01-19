#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"     // for GetByteBufferFromPool()
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates writing and reading of an ASCII config file\n");
   printf("using UnparseFile() and ParseFile()\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("First, we'll create a sample Message containing some imaginary configuration info.\n");
   printf("\n");

   // Basic settings demo
   Message myConfig;
   (void) myConfig.AddString("num_inputs",    "8");
   (void) myConfig.AddString("num_outputs",   "16");
   (void) myConfig.AddString("serial_number", "A1234B727");
   (void) myConfig.AddString("dist_license",  "BSD");
   (void) myConfig.AddString("arg with spaces",  "Yes, spaces require quote marks");

   // Demonstrate how we can store sub-settings-areas as child Messages
   Message subConfig;
   {
      (void) subConfig.AddString("run_mode",  "fast");
      (void) subConfig.AddString("debug",     "yes");
      (void) subConfig.AddString("max_mem",   "10 gigabytes");
      (void) subConfig.AddString("has spaces", "yes");

      Message subSubConfig;
      (void) subSubConfig.AddString("all the way", "down");
      (void) subConfig.AddMessage("turtles", subSubConfig);
   }
   (void) myConfig.AddMessage("run_flags", subConfig);

   printf("Here is the Message we are going to save as an ASCII text file:\n");
   myConfig.Print(stdout);

   FILE * fpOut = muscleFopen("test_config.txt", "w");
   if (fpOut == NULL)
   {
      printf("Error, couldn't open test_config.txt for writing!\n");
      return 10;
   }

   if (UnparseFile(myConfig, fpOut).IsOK())
   {
      printf("Wrote config to test_config.txt\n");
   }
   else printf("Erorr, UnparseFile() failed!?\n");

   fclose(fpOut);

   printf("\n");
   printf("Now let's print out the contents of the file we just wrote out, to see what it looks like:\n");

   FileDataIO fdio(muscleFopen("test_config.txt", "r"));
   ByteBufferRef fileContents = GetByteBufferFromPool(fdio);
   if (fileContents())
   {
      printf("------ snip ------\n");
      printf("%s", fileContents()->GetBuffer());
      printf("------ snip ------\n");
   }
   else printf("Error, couldn't read test_config.txt back into memory!?\n");

   printf("\n");
   printf("Now let's see if we can read the text file back into RAM as a Message again:\n");

   FILE * fpIn = muscleFopen("test_config.txt", "r");
   if (fpIn == NULL)
   {
      printf("Error, couldn't open test_config.txt for reading!\n");
      return 10;
   }

   Message readInMsg;
   if (ParseFile(fpIn, readInMsg).IsOK())
   {
      readInMsg.Print(stdout);
   }
   else printf("Error, ParseFile() failed!\n");

   fclose(fpIn);

   printf("\n");
   printf("Take a look at the test_config.txt file in this folder to see it for yourself!\n");

   return 0;
}
