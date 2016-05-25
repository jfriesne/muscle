/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "zlib/TarFileWriter.h"

namespace muscle {

TarFileWriter :: TarFileWriter() : _currentHeaderOffset(-1)
{
   // empty
}

TarFileWriter :: TarFileWriter(const char * outputFileName, bool append) : _currentHeaderOffset(-1)
{
   (void) SetFile(outputFileName, append);
}

TarFileWriter :: TarFileWriter(const DataIORef & dio) : _writerIO(dio), _currentHeaderOffset(-1)
{
   // empty
}

TarFileWriter :: ~TarFileWriter()
{
   (void) Close();
}

status_t TarFileWriter :: Close()
{
   status_t ret = _writerIO() ? FinishCurrentFileDataBlock() : B_NO_ERROR;
   _writerIO.Reset();  // do this no matter what
   return ret;
}

void TarFileWriter :: SetFile(const DataIORef & dio)
{
   (void) Close();
   _writerIO = dio;
}

status_t TarFileWriter :: SetFile(const char * outputFileName, bool append)
{
   (void) Close();
   _writerIO.Reset();

   if (outputFileName)
   {
      if (append == false) (void) DeleteFile(outputFileName);

      FILE * fpOut = muscleFopen(outputFileName, append?"ab":"wb");
      if (fpOut) 
      {
         _writerIO.SetRef(newnothrow FileDataIO(fpOut));
         if (_writerIO()) return B_NO_ERROR;
                     else {fclose(fpOut); WARN_OUT_OF_MEMORY;}
      }
      return B_ERROR;
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
   int numChars = muscleMin((int)fieldSize, ((int)(strlen(tmp)+1)));  // include the NUL byte if possible
   uint8 * dStart = (b+fieldSize)-numChars;
   memcpy(dStart, tmp, numChars);
   memset(b, '0', dStart-b);  // initial zeros
}

status_t TarFileWriter :: FinishCurrentFileDataBlock()
{
   if (_writerIO() == NULL) return B_ERROR;

   if (_currentHeaderOffset >= 0)
   {
      int64 currentPos = GetCurrentSeekPosition();
      uint64 currentFileLength = currentPos-(_currentHeaderOffset+TAR_BLOCK_SIZE);
      int64 extraBytes = (currentPos%TAR_BLOCK_SIZE);
      if (extraBytes != 0)
      {
         int64 numPadBytes = (TAR_BLOCK_SIZE-extraBytes);
         uint8 zeros[TAR_BLOCK_SIZE]; memset(zeros, 0, numPadBytes);
         if (_writerIO()->WriteFully(zeros, numPadBytes) != numPadBytes) return B_ERROR;
      }

      WriteOctalASCII(&_currentHeaderBytes[124], currentFileLength, 12);

      uint32 checksum = 0;
      for (uint32 i=0; i<TAR_BLOCK_SIZE; i++) checksum += _currentHeaderBytes[i];
      WriteOctalASCII(&_currentHeaderBytes[148], checksum, 8);

      if ((_writerIO()->Seek(_currentHeaderOffset, DataIO::IO_SEEK_SET) != B_NO_ERROR)||(_writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)) != sizeof(_currentHeaderBytes))||(_writerIO()->Seek(0, DataIO::IO_SEEK_END) != B_NO_ERROR)) return B_ERROR;

      _currentHeaderOffset = -1;
   }
   return B_NO_ERROR;
}

int64 TarFileWriter :: GetCurrentSeekPosition() const
{
   return _writerIO() ? _writerIO()->GetPosition() : -1;
}

status_t TarFileWriter :: WriteFileHeader(const char * fileName, uint32 fileMode, uint32 ownerID, uint32 groupID, uint64 modificationTime, int linkIndicator, const char * linkedFileName)
{
   if ((strlen(fileName) > 100)||((linkedFileName)&&(strlen(linkedFileName)>100))) return B_ERROR;  // string fields are only 100 chars long!
   if (FinishCurrentFileDataBlock() != B_NO_ERROR) return B_ERROR;  // should pad out position out to a multiple of 512, if necessary

   int64 curSeekPos = GetCurrentSeekPosition();
   if ((curSeekPos < 0)||((curSeekPos%TAR_BLOCK_SIZE) != 0)) return B_ERROR;

   _currentHeaderOffset = curSeekPos;
   memset(_currentHeaderBytes, 0, sizeof(_currentHeaderBytes));
   muscleStrncpy((char *)(&_currentHeaderBytes[0]), fileName, sizeof(_currentHeaderBytes));

   WriteOctalASCII(&_currentHeaderBytes[100], fileMode, 8);
   WriteOctalASCII(&_currentHeaderBytes[108], ownerID, 8);
   WriteOctalASCII(&_currentHeaderBytes[116], groupID, 8);

   uint64 secondsSince1970 = MicrosToSeconds(modificationTime);
   if (secondsSince1970 != 0) WriteOctalASCII(&_currentHeaderBytes[136], secondsSince1970, 12);

   memset(&_currentHeaderBytes[148], ' ', 8);  // these spaces are used later on, when calculating the header checksum
   _currentHeaderBytes[156] = linkIndicator+'0';
   if (linkedFileName) muscleStrncpy((char *)(&_currentHeaderBytes[157]), linkedFileName, sizeof(_currentHeaderBytes)-157);

   // We write out the header as it is now, in order to keep the file offsets correct... but we'll rewrite it again later
   // when we know the actual file size.
   return (_writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)) == sizeof(_currentHeaderBytes)) ? B_NO_ERROR : B_ERROR;
}

status_t TarFileWriter :: WriteFileData(const uint8 * fileData, uint32 numBytes)
{
   if ((_writerIO() == NULL)||(_currentHeaderOffset < 0)) return B_ERROR;
   return (_writerIO()->WriteFully(fileData, numBytes) == numBytes) ? B_NO_ERROR : B_ERROR;
}

}; // end namespace muscle
