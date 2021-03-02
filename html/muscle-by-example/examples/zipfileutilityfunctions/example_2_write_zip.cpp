#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/ZipFileUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using ZipFileUtilityFunctions to write out a .zip file\n");
   printf("\n");
}

static void AddFileToDir(Message & dirMsg, const String & fileName, uint32 fileLenBytes)
{
   // Add an uninitialized raw-data field to the Message with the given name
   if (dirMsg.AddData(fileName, B_RAW_TYPE, NULL, fileLenBytes).IsOK())
   {
      // And then populate the raw-data field with some data
      void * rawData;
      uint32 rawDataLen;  // should be the same as (fileLenBytes) but I'm paranoid
      if (dirMsg.FindDataPointer(fileName, B_RAW_TYPE, &rawData, &rawDataLen).IsOK())
      {
         uint8 * b = (uint8 *) rawData;
         const char dummyBuf[] = "All work and no play make Jack a dull boy.  ";
         for (uint32 i=0; i<rawDataLen; i++) b[i] = dummyBuf[i%(sizeof(dummyBuf)-1)];
      }
   }
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   if (argc != 2)
   {
      LogTime(MUSCLE_LOG_INFO, "Usage:   ./example_2_write_zip new_zip_file_name.zip\n");
      return 10;
   }

   /** Let's create some data to populate .zip file */
   Message someDataMsg;
   {
      MessageRef test1Dir = GetMessageFromPool();
      AddFileToDir(*test1Dir(), "some_data_1.bin", 1024);
      AddFileToDir(*test1Dir(), "some_data_2.bin", 1024);
      AddFileToDir(*test1Dir(), "some_data_3.bin", 1024);
      someDataMsg.AddMessage("sub_dir", test1Dir);

      AddFileToDir(someDataMsg, "blah_blah.txt", 512);
      AddFileToDir(someDataMsg, "nerf_nerf.txt", 512);
   }

   status_t ret;
   if (WriteZipFile(argv[1], someDataMsg).IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "Wrote file [%s] as a .zip file.  Run \"unzip -l %s\" to see its contents.\n", argv[1], argv[1]);
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Error writing [%s] as a .zip file! [%s]\n", argv[1], ret());

   return 0;
}
