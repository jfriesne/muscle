/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/FileDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "zlib/TarFileWriter.h"

namespace muscle {

TarFileWriter :: TarFileWriter()
{
   (void) Close();  // this will update our int64 member-variables to their appropriate defaults
}

TarFileWriter :: ~TarFileWriter()
{
   (void) Close();  // this will ensure that any still-pending data is flushed to the file before we go away
}

TarFileWriter :: TarFileWriter(const char * outputFileName, bool append)
{
   (void) SetFile(outputFileName, append);  // SetFile() will call Close(), Close() will initialize our int64 member-variables to their appropriate defaults
}

TarFileWriter :: TarFileWriter(const DataIORef & dio)
{
   (void) SetFile(dio);  // SetFile() will call Close(), Close() will initialize our int64 member-variables to their appropriate defaults
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
   (void) _seekableWriterIO.SetFromRefCountableRef(dio);  // _seekableWriterIO may be a NULL Ref, and that's okay
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

   if (_currentHeaderOffset >= 0)
   {
      const uint64 currentFileLength = _currentSeekPosition-(_currentHeaderOffset+TAR_BLOCK_SIZE);
      const int64 extraBytes = (_currentSeekPosition%TAR_BLOCK_SIZE);
      if (extraBytes != 0)
      {
         const int64 numPadBytes = (TAR_BLOCK_SIZE-extraBytes);
         uint8 zeros[TAR_BLOCK_SIZE]; memset(zeros, 0, (size_t) numPadBytes);

         MRETURN_ON_ERROR(_writerIO()->WriteFully(zeros, (uint32) numPadBytes));
         _currentSeekPosition += numPadBytes;
      }

      if (_seekableWriterIO())
      {
         WriteOctalASCII(&_currentHeaderBytes[124], currentFileLength, 12);
         UpdateCurrentHeaderChecksum();

         MRETURN_ON_ERROR(_seekableWriterIO()->Seek(_currentHeaderOffset, SeekableDataIO::IO_SEEK_SET));
         _currentSeekPosition = _currentHeaderOffset;

         MRETURN_ON_ERROR(_seekableWriterIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)));
         _currentSeekPosition += _currentHeaderOffset;

         MRETURN_ON_ERROR(_seekableWriterIO()->Seek(0, SeekableDataIO::IO_SEEK_END));
         _currentSeekPosition = _seekableWriterIO()->GetLength();
      }
      else if (currentFileLength > _prestatedFileSize)
      {
         LogTime(MUSCLE_LOG_ERROR, "TarFileWriter::FinishCurrentFileDataBlock():  DataIO isn't seekable, and the file-length (" UINT64_FORMAT_SPEC ") of the current entry [%s] is larger than the prestated file-length (" UINT64_FORMAT_SPEC ")!  Can't update the tar entry header!\n", currentFileLength, _currentHeaderBytes, _prestatedFileSize);
         return B_LOGIC_ERROR;  // should never happen since WriteFileData() will truncate before we get here
      }
      else if (currentFileLength < _prestatedFileSize)
      {
         uint64 numBytesToPad = (_prestatedFileSize-currentFileLength);
         LogTime(MUSCLE_LOG_ERROR, "TarFileWriter::FinishCurrentFileDataBlock():  Writing " UINT64_FORMAT_SPEC " zero-pad-bytes to match non-seekable file-size-header (" UINT64_FORMAT_SPEC ") of [%s]\n", numBytesToPad, _prestatedFileSize, (const char *) _currentHeaderBytes);

         uint8 padBuf[1024]; memset(padBuf, 0, sizeof(padBuf));
         while(numBytesToPad > 0)
         {
            const uint32 numPadBytesToWrite = muscleMin(numBytesToPad, (uint64) sizeof(padBuf));
            MRETURN_ON_ERROR(_writerIO()->WriteFully(padBuf, numPadBytesToWrite));
            numBytesToPad -= numPadBytesToWrite;
         }
      }

      _currentHeaderOffset = -1;
      _prestatedFileSize   = 0;
   }
   return B_NO_ERROR;
}

static const size_t _maxPrefixLength = 155;

// Returns the offset to the slash character that should mark the end of the prefix field's content
static size_t ComputeCommonPathPrefixLength(const char * fileName, const char * optLinkedFileName)
{
   if (optLinkedFileName)
   {
      // Find the common prefix of the two strings
      const char * s1 = fileName;
      const char * s2 = optLinkedFileName;
      while((*s1)&&(*s2)&&(*s1 == *s2)&&(((size_t)(s1-fileName))<_maxPrefixLength)) {s1++; s2++;}

      // Then back it up to the most recent slash
      while((s1>fileName)&&(*s1 != '/')) s1--;
      return (size_t)(s1-fileName);
   }
   else
   {
      const char * slash = strchr(fileName, '/');
      if (slash == NULL) return 0;

      const char * nextSlash;
      while(((nextSlash = strchr(slash+1, '/')) != NULL)&&(((size_t)(nextSlash-fileName)) <= _maxPrefixLength)) slash = nextSlash;

      return (size_t)(slash-fileName);
   }
}

status_t TarFileWriter :: WriteFileHeader(const char * fileName, uint32 fileMode, uint32 ownerID, uint32 groupID, uint64 modificationTime, int linkIndicator, const char * linkedFileName, uint64 prestatedFileSize)
{
   const size_t basicFormatMaxLen = 100;  // the original tar format supports only file paths up to this length
   const size_t ustarFormatMaxLen = 256;  // the ustar extension allows file paths up to this length

   const size_t fileNameLen = strlen(fileName);
   if (fileNameLen > ustarFormatMaxLen) return B_ERROR("File Entry name too long for .tar format");

   const size_t linkedFileNameLen = linkedFileName ? strlen(linkedFileName) : 0;
   if (linkedFileNameLen > ustarFormatMaxLen) return B_ERROR("Linked File name too long for .tar format");

   size_t ustarPathPrefixLength = 0;
   if ((fileNameLen > basicFormatMaxLen)||(linkedFileNameLen > basicFormatMaxLen))
   {
      ustarPathPrefixLength = ComputeCommonPathPrefixLength(fileName, linkedFileName);
      if (ustarPathPrefixLength > _maxPrefixLength) return B_ERROR("Couldn't compute a valid common prefix!");
   }

   MRETURN_ON_ERROR(FinishCurrentFileDataBlock());  // should pad out position out to a multiple of 512, if necessary

   if ((_currentSeekPosition%TAR_BLOCK_SIZE) != 0) return B_BAD_OBJECT;

   _currentHeaderOffset = _currentSeekPosition;
   memset(_currentHeaderBytes, 0, sizeof(_currentHeaderBytes));

   if (ustarPathPrefixLength == 0) muscleStrncpy((char *)(&_currentHeaderBytes[0]), fileName, 100);
   else
   {
      memcpy(&_currentHeaderBytes[257], "ustar\0", 6);  /// enable magic ustar extension
      muscleStrncpy((char *)(&_currentHeaderBytes[345]), fileName, ustarPathPrefixLength+1);       // common path-prefix goes here
      if (ustarPathPrefixLength < _maxPrefixLength) _currentHeaderBytes[345+ustarPathPrefixLength] = '\0';  // NUL-terminate if there's room to do it
      muscleStrncpy((char *)(&_currentHeaderBytes[0]),   fileName+ustarPathPrefixLength+1, 100);   // remainder of filename goes here
   }

   WriteOctalASCII(&_currentHeaderBytes[100], fileMode, 8);
   WriteOctalASCII(&_currentHeaderBytes[108], ownerID, 8);
   WriteOctalASCII(&_currentHeaderBytes[116], groupID, 8);
   WriteOctalASCII(&_currentHeaderBytes[124], prestatedFileSize, 12);

   const uint64 secondsSince1970 = MicrosToSeconds(modificationTime);
   if (secondsSince1970 != 0) WriteOctalASCII(&_currentHeaderBytes[136], secondsSince1970, 12);

   _currentHeaderBytes[156] = (uint8) (linkIndicator+'0');
   if (linkedFileName)
   {
      if (ustarPathPrefixLength == 0) muscleStrncpy((char *)(&_currentHeaderBytes[157]), linkedFileName, 100);
      else
      {
         // prefix field will already have been filled out above
         muscleStrncpy((char *)(&_currentHeaderBytes[157]), linkedFileName+ustarPathPrefixLength+1, 100); // remainder of link_path goes here
      }
   }
   _currentHeaderBytes[sizeof(_currentHeaderBytes)-1] = '\0';  // just in case muscleStrncpy() didn't terminate the string

   UpdateCurrentHeaderChecksum();

   // We write out the header as it is now, in order to keep the file offsets correct... but we'll rewrite it again later
   // when we know the actual file size.
   MRETURN_ON_ERROR(_writerIO()->WriteFully(_currentHeaderBytes, sizeof(_currentHeaderBytes)));
   _currentSeekPosition += sizeof(_currentHeaderBytes);
   _prestatedFileSize = prestatedFileSize;
   return B_NO_ERROR;
}

status_t TarFileWriter :: WriteFileData(const uint8 * fileData, uint32 numBytes)
{
   if ((_writerIO() == NULL)||(_currentHeaderOffset < 0)) return B_BAD_OBJECT;

   uint32 effectiveNumBytes = numBytes;
   if (_seekableWriterIO() == NULL)
   {
      // Don't write more bytes than we promised in the header that we would write, since we can't seek back to modify the header now
      const uint64 currentFileLength = _currentSeekPosition-(_currentHeaderOffset+TAR_BLOCK_SIZE);
      const int64 spaceLeftWithoutModifyingHeader = _prestatedFileSize-currentFileLength;
      if (((int64)numBytes) > spaceLeftWithoutModifyingHeader)
      {
         effectiveNumBytes = muscleMin(numBytes, (uint32) spaceLeftWithoutModifyingHeader);
         LogTime(MUSCLE_LOG_WARNING, "TarFileWriter::WriteFileData:   Dropping " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " file-bytes from write to respect the fixed header-size value (" UINT64_FORMAT_SPEC ") for [%s]\n", (numBytes-effectiveNumBytes), numBytes, _prestatedFileSize, (const char *)_currentHeaderBytes);
      }
   }

   MRETURN_ON_ERROR(_writerIO()->WriteFully(fileData, effectiveNumBytes));
   _currentSeekPosition += effectiveNumBytes;
   return B_NO_ERROR;
}

} // end namespace muscle
