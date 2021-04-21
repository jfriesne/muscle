/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "zlib/TarFileWriter.h"

namespace muscle {

TarFileWriter :: TarFileWriter()
{
   Close();  // this will update our int64 member-variables to their appropriate defaults
}

TarFileWriter :: TarFileWriter(const char * outputFileName, bool append)
{
   (void) SetFile(outputFileName, append);  // SetFile() will call Close(), Close() will initialize our int64 member-variables to their appropriate defaults
}

TarFileWriter :: TarFileWriter(const DataIORef & dio)
{
   (void) SetFile(dio);  // SetFile() will call Close(), Close() will initialize our int64 member-variables to their appropriate defaults
}

TarFileWriter :: ~TarFileWriter()
{
   (void) Close();
}

status_t TarFileWriter :: Close()
{
   const status_t ret = _writerIO() ? FinishCurrentFileDataBlock() : B_NO_ERROR;

   _writerIO.Reset();
   _seekableWriterIO.Reset();

   _currentHeaderOffset = -1;
   _prestatedFileSize   = 0;
   _currentSeekPosition = 0;
   return ret;
}

void TarFileWriter :: SetFile(const DataIORef & dio)
{
   (void) Close();

   _writerIO = dio;
   (void) _seekableWriterIO.SetFromRefCountableRef(dio);  // _seekableWriterIO be a NULL Ref, and that's okay
}

status_t TarFileWriter :: SetFile(const char * outputFileName, bool append)
{
   (void) Close();

   if (outputFileName)
   {
      if (append == false) (void) DeleteFile(outputFileName);

      FILE * fpOut = muscleFopen(outputFileName, append?"ab":"wb");
      if (fpOut) 
      {
         FileDataIORef ioRef(newnothrow FileDataIO(fpOut));
         if (ioRef()) 
         {
            SetFile(ioRef);
            if (append) _currentSeekPosition = ioRef()->GetLength();
            return B_NO_ERROR;
         }
         else {fclose(fpOut); MWARN_OUT_OF_MEMORY;}
      }
      return B_ERRNO;
   }
   else return B_NO_ERROR;
}

static void WriteOctalASCII(uint8 * b, uint64 val, uint8 fieldSize)
{
   // gotta pad out the file data to the nearest block boundary!
   char formatStr[16]; muscleStrcpy(formatStr, UINT64_FORMAT_SPEC " ");

   char * pi = strchr(formatStr, 'u');
   if (pi) *pi = 'o';  // gotta use octal here!

   char tmp[256];
   muscleSprintf(tmp, formatStr, val);
   const int numChars = muscleMin((int)fieldSize, ((int)(strlen(tmp)+1)));  // include the NUL byte if possible
   uint8 * dStart = (b+fieldSize)-numChars;
   memcpy(dStart, tmp, numChars);

   // The if-test below shouldn't be necessary, but it's here to
   // avoid a spurious warning from gcc under Windows (per Mika)
   if (dStart > b) memset(b, '0', dStart-b);  // initial zeros
}

void TarFileWriter::UpdateCurrentHeaderChecksum()
{
   memset(&_currentHeaderBytes[148], ' ', 8);  // when calculating the checksum, the checksum field must be full of spaces

   uint32 checksum = 0;
   for (uint32 i=0; i<TAR_BLOCK_SIZE; i++) checksum += _currentHeaderBytes[i];
   WriteOctalASCII(&_currentHeaderBytes[148], checksum, 8);
}

status_t TarFileWriter :: FinishCurrentFileDataBlock()
{
   if (_writerIO() == NULL) return B_BAD_OBJECT;

   status_t ret;
   if (_currentHeaderOffset >= 0)
   {
      const uint64 currentFileLength = _currentSeekPosition-(_currentHeaderOffset+TAR_BLOCK_SIZE);
      const int64 extraBytes = (_currentSeekPosition%TAR_BLOCK_SIZE);
      if (extraBytes != 0)
      {
         const int64 numPadBytes = (TAR_BLOCK_SIZE-extraBytes);
         uint8 zeros[TAR_BLOCK_SIZE]; memset(zeros, 0, (size_t) numPadBytes);

         const uint32 numBytesWritten = _writerIO()->WriteFully(zeros, (uint32) numPadBytes);
         _currentSeekPosition += numBytesWritten;
         if (numBytesWritten != (uint32) numPadBytes) return B_IO_ERROR;
      }

      if (_seekableWriterIO())
      {
         WriteOctalASCII(&_currentHeaderBytes[124], currentFileLength, 12);
         UpdateCurrentHeaderChecksum();

         MRETURN_ON_ERROR(_seekableWriterIO()->Seek(_currentHeaderOffset, SeekableDataIO::IO_SEEK_SET));
         _currentSeekPosition = _currentHeaderOffset;
 
         const uint32 numBytesWritten = _seekableWriterIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes));
         _currentSeekPosition += _currentHeaderOffset;
         if (numBytesWritten != sizeof(_currentHeaderBytes)) return B_IO_ERROR;

         if (_seekableWriterIO()->Seek(0, SeekableDataIO::IO_SEEK_END).IsOK(ret)) _currentSeekPosition = _seekableWriterIO()->GetLength();
                                                                             else return ret;
      }
      else if (_prestatedFileSize != currentFileLength)
      {
         LogTime(MUSCLE_LOG_ERROR, "TarFileWriter::FinishCurrentFileDataBlock():  DataIO isn't seekable, and the current entry's  file-length (" UINT64_FORMAT_SPEC ") doesn't match the prestated file-length (" UINT64_FORMAT_SPEC ")!  Can't update the tar entry header!\n", currentFileLength, _prestatedFileSize);
         return B_BAD_ARGUMENT;
      }

      _currentHeaderOffset = -1;
      _prestatedFileSize   = 0;
   }
   return B_NO_ERROR;
}

status_t TarFileWriter :: WriteFileHeader(const char * fileName, uint32 fileMode, uint32 ownerID, uint32 groupID, uint64 modificationTime, int linkIndicator, const char * linkedFileName, uint64 prestatedFileSize)
{
   if ((strlen(fileName) > 100)||((linkedFileName)&&(strlen(linkedFileName)>100))) return B_BAD_ARGUMENT;  // string fields are only 100 chars long!

   MRETURN_ON_ERROR(FinishCurrentFileDataBlock());  // should pad out position out to a multiple of 512, if necessary

   if ((_currentSeekPosition%TAR_BLOCK_SIZE) != 0) return B_BAD_OBJECT;

   _currentHeaderOffset = _currentSeekPosition;
   memset(_currentHeaderBytes, 0, sizeof(_currentHeaderBytes));
   muscleStrncpy((char *)(&_currentHeaderBytes[0]), fileName, sizeof(_currentHeaderBytes)-1);

   WriteOctalASCII(&_currentHeaderBytes[100], fileMode, 8);
   WriteOctalASCII(&_currentHeaderBytes[108], ownerID, 8);
   WriteOctalASCII(&_currentHeaderBytes[116], groupID, 8);
   WriteOctalASCII(&_currentHeaderBytes[124], prestatedFileSize, 12);

   const uint64 secondsSince1970 = MicrosToSeconds(modificationTime);
   if (secondsSince1970 != 0) WriteOctalASCII(&_currentHeaderBytes[136], secondsSince1970, 12);

   _currentHeaderBytes[156] = (uint8) (linkIndicator+'0');
   if (linkedFileName) muscleStrncpy((char *)(&_currentHeaderBytes[157]), linkedFileName, sizeof(_currentHeaderBytes)-157);

   UpdateCurrentHeaderChecksum();

   // We write out the header as it is now, in order to keep the file offsets correct... but we'll rewrite it again later
   // when we know the actual file size.
   const uint32 numBytesWritten = _writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes));
   _currentSeekPosition += numBytesWritten;
   if (numBytesWritten == sizeof(_currentHeaderBytes))
   {
      _prestatedFileSize = prestatedFileSize;
      return B_NO_ERROR;
   }
   else return B_IO_ERROR;
}

status_t TarFileWriter :: WriteFileData(const uint8 * fileData, uint32 numBytes)
{
   if ((_writerIO() == NULL)||(_currentHeaderOffset < 0)) return B_BAD_OBJECT;

   const uint32 numBytesWritten = _writerIO()->WriteFully(fileData, numBytes);
   _currentSeekPosition += numBytesWritten;
   return (numBytesWritten == numBytes) ? B_NO_ERROR : B_IO_ERROR;
}

} // end namespace muscle
