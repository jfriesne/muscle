/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "zlib/TarFileWriter.h"

namespace muscle {

TarFileWriter :: TarFileWriter()
   : _currentHeaderOffset(-1)
{
   // empty
}

TarFileWriter :: TarFileWriter(const char * outputFileName, bool append)
   : _currentHeaderOffset(-1)
{
   (void) SetFile(outputFileName, append);
}

TarFileWriter :: TarFileWriter(const SeekableDataIORef & dio)
   : _writerIO(dio)
   , _currentHeaderOffset(-1)
{
   // empty
}

TarFileWriter :: ~TarFileWriter()
{
   (void) Close();
}

status_t TarFileWriter :: Close()
{
   const status_t ret = _writerIO() ? FinishCurrentFileDataBlock() : B_NO_ERROR;
   _writerIO.Reset();  // do this no matter what
   return ret;
}

void TarFileWriter :: SetFile(const SeekableDataIORef & dio)
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

status_t TarFileWriter :: FinishCurrentFileDataBlock()
{
   if (_writerIO() == NULL) return B_BAD_OBJECT;

   status_t ret;
   if (_currentHeaderOffset >= 0)
   {
      const int64 currentPos = GetCurrentSeekPosition();
      const uint64 currentFileLength = currentPos-(_currentHeaderOffset+TAR_BLOCK_SIZE);
      const int64 extraBytes = (currentPos%TAR_BLOCK_SIZE);
      if (extraBytes != 0)
      {
         const int64 numPadBytes = (TAR_BLOCK_SIZE-extraBytes);
         uint8 zeros[TAR_BLOCK_SIZE]; memset(zeros, 0, (size_t) numPadBytes);
         if (_writerIO()->WriteFully(zeros, (uint32) numPadBytes) != (uint32) numPadBytes) return B_IO_ERROR;
      }

      WriteOctalASCII(&_currentHeaderBytes[124], currentFileLength, 12);

      uint32 checksum = 0;
      for (uint32 i=0; i<TAR_BLOCK_SIZE; i++) checksum += _currentHeaderBytes[i];
      WriteOctalASCII(&_currentHeaderBytes[148], checksum, 8);

      if (_writerIO()->Seek(_currentHeaderOffset, SeekableDataIO::IO_SEEK_SET).IsError(ret)) return ret;
      if (_writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)) != sizeof(_currentHeaderBytes)) return B_IO_ERROR;
      if (_writerIO()->Seek(0, SeekableDataIO::IO_SEEK_END).IsError(ret)) return ret;

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
   if ((strlen(fileName) > 100)||((linkedFileName)&&(strlen(linkedFileName)>100))) return B_BAD_ARGUMENT;  // string fields are only 100 chars long!

   status_t ret;
   if (FinishCurrentFileDataBlock().IsError(ret)) return ret;  // should pad out position out to a multiple of 512, if necessary

   const int64 curSeekPos = GetCurrentSeekPosition();
   if ((curSeekPos < 0)||((curSeekPos%TAR_BLOCK_SIZE) != 0)) return B_BAD_OBJECT;

   _currentHeaderOffset = curSeekPos;
   memset(_currentHeaderBytes, 0, sizeof(_currentHeaderBytes));
   muscleStrncpy((char *)(&_currentHeaderBytes[0]), fileName, sizeof(_currentHeaderBytes));

   WriteOctalASCII(&_currentHeaderBytes[100], fileMode, 8);
   WriteOctalASCII(&_currentHeaderBytes[108], ownerID, 8);
   WriteOctalASCII(&_currentHeaderBytes[116], groupID, 8);

   const uint64 secondsSince1970 = MicrosToSeconds(modificationTime);
   if (secondsSince1970 != 0) WriteOctalASCII(&_currentHeaderBytes[136], secondsSince1970, 12);

   memset(&_currentHeaderBytes[148], ' ', 8);  // these spaces are used later on, when calculating the header checksum
   _currentHeaderBytes[156] = (uint8) (linkIndicator+'0');
   if (linkedFileName) muscleStrncpy((char *)(&_currentHeaderBytes[157]), linkedFileName, sizeof(_currentHeaderBytes)-157);

   // We write out the header as it is now, in order to keep the file offsets correct... but we'll rewrite it again later
   // when we know the actual file size.
   return (_writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)) == sizeof(_currentHeaderBytes)) ? B_NO_ERROR : B_IO_ERROR;
}

status_t TarFileWriter :: WriteFileData(const uint8 * fileData, uint32 numBytes)
{
   if ((_writerIO() == NULL)||(_currentHeaderOffset < 0)) return B_BAD_OBJECT;
   return (_writerIO()->WriteFully(fileData, numBytes) == numBytes) ? B_NO_ERROR : B_IO_ERROR;
}

} // end namespace muscle
