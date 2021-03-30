#include <fcntl.h>                // for the S_IR* macros
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "zlib/TarFileWriter.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This demonstrates basic usage of the muscle::TarFileWriter class by writing out a .tar file\n");
   printf("\n");
}

static status_t WriteFakeFileDataToTarFile(TarFileWriter & writer, const char * fakeFileName)
{
#ifdef WIN32
   const uint32 fileMode = 0777;  // Windows doesn't have the S_* macros defined :(
#else
   const uint32 fileMode = S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
#endif

   MRETURN_ON_ERROR(writer.WriteFileHeader(fakeFileName, fileMode, 0, 0, GetCurrentTime64(), TarFileWriter::TAR_LINK_INDICATOR_NORMAL_FILE, NULL, 0));

   // Generate some fake data for our fake file to contain
   uint8 fakeDataBuf[1024];
   const uint8 dummyBuf[] = "All work and no play make Jack a dull boy.  ";
   for (uint32 i=0; i<sizeof(fakeDataBuf); i++) fakeDataBuf[i] = dummyBuf[i%(sizeof(dummyBuf)-1)];

   return writer.WriteFileData(fakeDataBuf, sizeof(fakeDataBuf));
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   status_t ret;

   const char * outputFileName = "./example_output.tar";
   TarFileWriter writer(outputFileName, false);

   const char * fakeFileNames[] = {
      "file1.bin",
      "file2.bin",
      "file3.bin"
   };
   for (uint32 i=0; i<ARRAYITEMS(fakeFileNames); i++) 
   {
      if (WriteFakeFileDataToTarFile(writer, fakeFileNames[i]).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "Error writing fake file data for [%s] to .tar file [%s], aborting! [%s]\n", fakeFileNames[i], outputFileName, ret());
         return 10;
      }
   }

   LogTime(MUSCLE_LOG_INFO, "Output file [%s] created.\n", outputFileName);
   LogTime(MUSCLE_LOG_INFO, "Run \"tar tvf %s\" list its contents.\n", outputFileName);
   LogTime(MUSCLE_LOG_INFO, "Run \"tar xvf %s\" un-tar its contents.\n", outputFileName);
 
   printf("\n");
   return 0;
}
