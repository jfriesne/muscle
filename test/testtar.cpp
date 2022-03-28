/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <fcntl.h>  // for the S_IR* macros
#include "dataio/FileDataIO.h"
#include "system/SetupSystem.h"
#include "zlib/TarFileWriter.h"
#include "util/Directory.h"
#include "util/FilePathInfo.h"
#include "zlib/ZLibDataIO.h"

using namespace muscle;

static uint32 GetDefaultFileMode()
{
#ifdef WIN32
   return 0777;  // Windows doesn't have the S_* macros defined :(
#else
   return S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
#endif
}

static status_t AddFileToTar(TarFileWriter & tarFileWriter, const String & entryPath, const String & filePath, const FilePathInfo & fpi, uint64 currentTime)
{
   FILE * fpIn = fopen(filePath(), "rb");
   if (fpIn)
   {
      FileDataIO inputFile(fpIn);
      MRETURN_ON_ERROR(tarFileWriter.WriteFileHeader(entryPath(), GetDefaultFileMode(), 0/*ownerID*/, 0/*groupID*/, currentTime, TarFileWriter::TAR_LINK_INDICATOR_NORMAL_FILE, NULL, fpi.GetFileSize()));

      uint64 bytesWritten = 0;
      uint8 buf[64*1024];
      while(bytesWritten < fpi.GetFileSize())
      {
         const int32 bytesRead = inputFile.Read(buf, sizeof(buf));
         if (bytesRead < 0)
         {
            LogTime(MUSCLE_LOG_ERROR, "Error reading from file [%s]\n", filePath());
            return B_IO_ERROR;
         }
         MRETURN_ON_ERROR(tarFileWriter.WriteFileData(buf, bytesRead));
         bytesWritten += bytesRead; 
      }
      return B_NO_ERROR;  // we could call tarFileWriter.FinishCurrentFileDataBlock() here but it should also work without doing so, so I won't --jaf
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't open input file [%s]\n", filePath());
      return B_IO_ERROR;
   }
}

static status_t AddDirectoryToTar(TarFileWriter & tarFileWriter, const String & entryPath, const String & folderPath, uint64 currentTime)
{
   Directory dir;
   MRETURN_ON_ERROR(dir.SetDir(folderPath()));

   const char * nextEntry;
   while((nextEntry = dir.GetCurrentFileName()) != NULL)
   {
      const String fn = nextEntry;
      if ((fn != ".")&&(fn != ".."))
      {
         const String newEntryPath =  entryPath.AppendWord(fn, "/").WithoutPrefix("./");
         const String newFilePath  = folderPath.AppendWord(fn, "/");
         const FilePathInfo fpi(newFilePath());
         if (fpi.IsDirectory()) {MRETURN_ON_ERROR(AddDirectoryToTar(tarFileWriter, newEntryPath, newFilePath,      currentTime));}
                           else {MRETURN_ON_ERROR(     AddFileToTar(tarFileWriter, newEntryPath, newFilePath, fpi, currentTime));}
      }

      dir++;
   }
   return B_NO_ERROR;
}

// This program tests the TarFileWriter class by having it write out a .tar or .tgz file for the specified files/folders
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc <= 2)
   {
      printf("Usage:  ./testtar tarfile.tar [filename] [filename] [foldername] [...]\n");
      return 0;
   }

   const String outputFileName  = argv[1];

   FILE * fpOut = fopen(outputFileName(), "wb");
   if (fpOut == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't open output file [%s]\n", outputFileName());
      return 10;
   }

   const uint64 currentTime = GetCurrentTime64();

   DataIORef outputFile(new FileDataIO(fpOut));
   if ((outputFileName.EndsWith(".tgz"))||(outputFileName.EndsWith(".tar.gz"))) outputFile.SetRef(new GZLibDataIO(outputFile));

   TarFileWriter tarFileWriter(outputFile);
   for (int i=2; i<argc; i++)
   {
      const String nextReadFile = argv[i];
      const FilePathInfo fpi(nextReadFile());
      if (fpi.Exists())
      {
         const status_t ret = fpi.IsDirectory() ? AddDirectoryToTar(tarFileWriter, nextReadFile, nextReadFile, currentTime) : AddFileToTar(tarFileWriter, nextReadFile, nextReadFile, fpi, currentTime);
         if (ret.IsError())
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Error adding input %s [%s] : [%s]\n", fpi.IsDirectory()?"folder":"file", nextReadFile(), ret());
            return 10;
         }
      }
      else LogTime(MUSCLE_LOG_ERROR, "Input file [%s] doesn't exist\n", nextReadFile());
   }

   LogTime(MUSCLE_LOG_INFO, "Created file [%s]\n", outputFileName());
   return 0;
}
