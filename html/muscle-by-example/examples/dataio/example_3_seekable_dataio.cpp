#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/FileDataIO.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the use of a SeekableDataIO object (in particular, a FileDataIO) to write/seek/read in a file.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   FileDataIO fileDataIO(fopen("example_3_seekable_dataio.txt", "w+"));  // "w+" lets us write, read, and update
   if (fileDataIO.GetFile() == NULL)
   {
      printf("Unable to open example_3_seekable_dataio.txt for writing!\n");
   }
  
   // First we'll write out some data to the file, just so we have something to play with
   for (int i=0; i<50; i++)
   {
      const char lineOfText[] = "All work and no play makes jack a dull boy\n";
      (void) fileDataIO.Write(lineOfText, sizeof(lineOfText)-1);  // -1 to avoid writing the NUL terminator byte
   }
   printf("Wrote 50 lines of text to example_3_seekable_dataio.txt; total file size is now " INT64_FORMAT_SPEC "\n", fileDataIO.GetLength());

   status_t ret;
   if (fileDataIO.Seek(666, SeekableDataIO::IO_SEEK_SET).IsError(ret))
   {
      printf("Error, Seek() failed! [%s]\n", ret());
   }

   printf("After Seek(), our read/write head is now positioned at offset " INT64_FORMAT_SPEC " from the top of the file.\n", fileDataIO.GetPosition());

   const char someOtherText[] = "\n\n   WHAT HAVE YOU DONE WITH ME?   \n\n";
   if (fileDataIO.Write(someOtherText, sizeof(someOtherText)-1) != (sizeof(someOtherText)-1))
   {
      printf("Error writing text into the middle of the file!?\n");
   }

   printf("Now we will read the contents of the file and print it to stdout:\n\n");

   if (fileDataIO.Seek(0, SeekableDataIO::IO_SEEK_SET).IsError(ret))
   {
      printf("Error, Seek() to top failed! [%s]\n", ret());
   }

   char inputBuf[1024];
   int numBytesRead;
   while((numBytesRead = fileDataIO.Read(inputBuf, sizeof(inputBuf)-1)) >= 0)
   {
      inputBuf[numBytesRead] = '\0';
      printf("%s", inputBuf);
   }
   
   printf("\n");
   printf("Program exiting\n");
   printf("\n");

   return 0;
}
