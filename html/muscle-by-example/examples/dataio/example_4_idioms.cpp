#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/FileDataIO.h"
#include "util/ByteBuffer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates some handy idioms/tricks using the DataIO classes\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Read the entire contents of a file into a ByteBuffer, in two lines:\n");
   const char * inputFileName = "example_4_idioms.cpp";
   FileDataIO inputFdio(muscleFopen(inputFileName, "r"));
   ByteBufferRef bbRef = GetByteBufferFromPool(inputFdio);
   if (bbRef())
   {
      printf("Here are the contents of this program's source file, as a hex dump:\n");
      bbRef()->PrintToStream();
   }
   else
   {
      printf("Error, couldn't read input file [%s]\n", inputFileName);
      return 10;
   }

   printf("\n\n");
   printf("Write a ByteBuffer out to a file in two lines:\n");
   const char * outputFileName = "example_4_output.txt";
   FileDataIO outputFdio(muscleFopen(outputFileName, "w"));
   const status_t wfRet = outputFdio.WriteFully(bbRef()->GetBuffer(), bbRef()->GetNumBytes());
   if (wfRet.IsOK()) printf("Wrote " UINT32_FORMAT_SPEC " bytes of data to [%s]\n", bbRef()->GetNumBytes(), outputFileName);
                else printf("Error [%s] writing " UINT32_FORMAT_SPEC " bytes of data to [%s]\n", wfRet(), bbRef()->GetNumBytes(), outputFileName);

   return 0;
}
