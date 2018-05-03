#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/ByteBuffer.h"
#include "zlib/ZLibCodec.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic usage of the muscle::ZLibCodec class to deflate/inflate data\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's create a big buffer of raw data to test with
   const uint32 rawBufferSize = 100*1024;  // 100kB ought to be big enough
   uint8 bigBuffer[rawBufferSize];
   for (uint32 i=0; i<rawBufferSize; i++) bigBuffer[i] = (i%26)+'A';

   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Raw buffer size is " UINT32_FORMAT_SPEC " bytes.\n", sizeof(bigBuffer));

   // Now let's use a ZLibCodec to generate a deflated representation of same
   ZLibCodec codec(9);  // 9 == maximum compression level, because why not?  Modern CPUs are fast

   ByteBufferRef deflatedBuffer = codec.Deflate(bigBuffer, sizeof(bigBuffer), true);
   if (deflatedBuffer() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Deflate() failed, aborting!\n");
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Deflated buffer size is " UINT32_FORMAT_SPEC " bytes (%.01f%% space savings, yay!).\n", deflatedBuffer()->GetNumBytes(), 100.0f-(100.0f*(((float)deflatedBuffer()->GetNumBytes())/sizeof(bigBuffer))));

   // And finally, just to verify that the compression is lossless, we'll
   // re-generate our original data from the deflated buffer and make
   // sure the re-inflated buffer's contents match the original data.

   ByteBufferRef reinflatedBuffer = codec.Inflate(*deflatedBuffer());
   if (reinflatedBuffer() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Inflate() failed, aborting!\n");
      return 10;
   }

   if (reinflatedBuffer()->GetNumBytes() != sizeof(bigBuffer))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Reinflated buffer is the wrong size!  Expected " UINT32_FORMAT_SPEC ", got " UINT32_FORMAT_SPEC "!\n", sizeof(bigBuffer), reinflatedBuffer()->GetNumBytes());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Reinflated buffer size is " UINT32_FORMAT_SPEC " bytes.\n", reinflatedBuffer()->GetNumBytes());

   if (memcmp(bigBuffer, reinflatedBuffer()->GetBuffer(), reinflatedBuffer()->GetNumBytes()) == 0)
   {
      LogTime(MUSCLE_LOG_INFO, "Verified that the reinflated buffer's contents are the same as the original raw-data-buffer.\n");
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "The Reinflated buffer's contents are different from the original raw-data-buffer!?\n");

   return 0;
}
