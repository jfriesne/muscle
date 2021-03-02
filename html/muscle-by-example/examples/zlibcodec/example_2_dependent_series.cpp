#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/ZLibCodec.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a muscle::ZLibCodec to deflate a stream of data more efficiently by using dependent-coding\n");
   printf("\n");
}

static ByteBufferRef GenerateMoreRawData()
{
   ByteBufferRef ret = GetByteBufferFromPool(10*1024);
   if (ret())
   {
      static uint8 v = 0;

      uint8 * b = ret()->GetBuffer();
      for (uint32 i=0; i<ret()->GetNumBytes(); i++) *b++ = v++;
   }
   return ret;
}

// Returns true iff the inflated data matches the pattern we originally
// generated inside GenerateMoreRawData()
status_t VerifyInflatedData(const ByteBuffer & inflatedData)
{
   const uint8 * b       = inflatedData.GetBuffer();
   const uint32 numBytes = inflatedData.GetNumBytes();

   static uint8 v = 0;
   for (uint32 i=0; i<numBytes; i++)
   {
      if (b[i] != v) return B_LOGIC_ERROR;  // wtf?
      v++;
   }

   return B_NO_ERROR;
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   FileDataIO outputFile(fopen("./example_2_output.bin", "wb"));
   if (outputFile.GetFile() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't open output file for write, aborting!\n");
      return 10;
   }
   
   // Generate a series of raw-data-buffers and save them to the output file.
   // Note that in order to improve the compression efficiency, only the first
   // generated buffer is generated using the "independent=true" (third) parameter
   // to Deflate().  That way subsequent buffers can re-use the compression-dictionary
   // of the previous buffers, thus allowing for more efficient compression.
   //
   // We can do that only because we know the file will be read back in-order.
   // If the buffers needed to be inflateable in any order, we'd need to keep
   // them independent of each other.
   ZLibCodec codec(9);
   uint32 rawBytesWritten = 0, deflatedBytesWritten = 0;
   for (int i=0; i<100; i++)
   {
      ByteBufferRef rawData = GenerateMoreRawData();
      if (rawData())
      {
         ByteBufferRef deflatedData = codec.Deflate(*rawData(), (i==0));
         if (deflatedData())
         {
            // Write the size of the deflated buffer into the file for framing purposes
            uint32 leDeflatedBufSize = B_HOST_TO_LENDIAN_INT32(deflatedData()->GetNumBytes());
            if (outputFile.WriteFully(&leDeflatedBufSize, sizeof(leDeflatedBufSize)) == sizeof(leDeflatedBufSize)) deflatedBytesWritten += sizeof(leDeflatedBufSize);

            // Write the actual deflated data into the file
            if (outputFile.WriteFully(deflatedData()->GetBuffer(), deflatedData()->GetNumBytes()) == deflatedData()->GetNumBytes())
            {
               rawBytesWritten      += rawData()->GetNumBytes();
               deflatedBytesWritten += deflatedData()->GetNumBytes();
            }
            else LogTime(MUSCLE_LOG_ERROR, "Write() failed!?\n");
         }
         else LogTime(MUSCLE_LOG_ERROR, "Deflate() failed!?\n");
      }
      else LogTime(MUSCLE_LOG_ERROR, "GenerateMoreRawData() failed!?\n");
   }

   LogTime(MUSCLE_LOG_INFO, "Write " UINT32_FORMAT_SPEC " bytes of deflated data to the output file, representing " UINT32_FORMAT_SPEC " bytes of raw data\n", deflatedBytesWritten, rawBytesWritten);

   outputFile.Shutdown();  // make sure the output-file-handle is closed before we try to read the file back in

   // Now we'll read the file back in, inflate the data, and verify that the inflated data matches the original raw data
   FileDataIO inputFile(fopen("./example_2_output.bin", "rb"));
   if (inputFile.GetFile() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't open output file for read, aborting!\n");
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "Re-Opened output file for reading, to verify it...\n");
   
   uint32 rawBytesRead = 0;
   while(true)
   {
      uint32 leBufSize;
      if (inputFile.ReadFully(&leBufSize, sizeof(leBufSize)) != sizeof(leBufSize)) break;  // EOF?

      uint32 bufSize = B_LENDIAN_TO_HOST_INT32(leBufSize);

      ByteBufferRef deflatedData = GetByteBufferFromPool(bufSize);
      if (deflatedData() == NULL) return 10;  // out of memory?
     
      if (inputFile.ReadFully(deflatedData()->GetBuffer(), deflatedData()->GetNumBytes()) != deflatedData()->GetNumBytes())
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to read full buffer of deflated data, corrupt file?\n");
         return 10;
      }

      ByteBufferRef inflatedData = codec.Inflate(*deflatedData());
      if (VerifyInflatedData(*inflatedData()).IsOK()) rawBytesRead += inflatedData()->GetNumBytes();
      else
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Verification of re-inflated data failed at offset " UINT64_FORMAT_SPEC ", corrupt file?\n", rawBytesRead);
         return 10;
      }
   }

   if (rawBytesRead != rawBytesWritten) 
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "The amount of re-inflated data read (" UINT64_FORMAT_SPEC " bytes) didn't match the amount written (" UINT64_FORMAT_SPEC " bytes)!  Corrupt data?\n", rawBytesRead, rawBytesWritten);
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "The output file was verified to contain the same raw data that was generated.\n");
   
   return 0;
}
