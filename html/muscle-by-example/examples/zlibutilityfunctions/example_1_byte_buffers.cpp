#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/ZLibUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the use of DeflateByteBuffer() and InflateByteBuffer() to deflate/inflate data\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's create a big buffer of raw data to test with
   ByteBufferRef rawDataBuffer = GetByteBufferFromPool(100*1024);
   for (uint32 i=0; i<rawDataBuffer()->GetNumBytes(); i++) rawDataBuffer()->GetBuffer()[i] = (i%26)+'A';

   LogTime(MUSCLE_LOG_INFO, "Raw buffer size is " UINT32_FORMAT_SPEC " bytes.\n", rawDataBuffer()->GetNumBytes());

   // Now let's get a deflated version of that ByteBuffer
   ByteBufferRef deflatedBuffer = DeflateByteBuffer(rawDataBuffer, 9);
   if (deflatedBuffer() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "DeflateByteBuffer() failed, aborting!\n");
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Deflated buffer size is " UINT32_FORMAT_SPEC " bytes (%.01f%% space savings, yay!).\n", deflatedBuffer()->GetNumBytes(), 100.0f-(100.0f*(((float)deflatedBuffer()->GetNumBytes())/rawDataBuffer()->GetNumBytes())));

   // And finally, just to verify that the compression is lossless, we'll
   // re-generate our original data from the deflated buffer and make
   // sure the re-inflated buffer's contents match the original data.

   ByteBufferRef reinflatedBuffer = InflateByteBuffer(deflatedBuffer);
   if (reinflatedBuffer() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "InflateByteBuffer() failed, aborting!\n");
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Reinflated buffer size is " UINT32_FORMAT_SPEC " bytes.\n", reinflatedBuffer()->GetNumBytes());

   if ((*reinflatedBuffer()) == (*rawDataBuffer()))
   {
      LogTime(MUSCLE_LOG_INFO, "Verified that the reinflated buffer's contents are the same as the original raw-data-buffer.\n");
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "The Reinflated buffer's contents are different from the original raw-data-buffer!?\n");

   return 0;
}
