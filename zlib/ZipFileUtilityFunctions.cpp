/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "dataio/FileDataIO.h"
#include "util/ByteBuffer.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"
#include "zlib/ZipFileUtilityFunctions.h"
#include "zlib/zlib/contrib/minizip/zip.h"
#include "zlib/zlib/contrib/minizip/unzip.h"

namespace muscle {

static voidpf ZCALLBACK fopen_dataio_func (voidpf opaque, const char * /*filename*/, int /*mode*/)
{
   return opaque;
}

static uLong ZCALLBACK fread_dataio_func (voidpf /*opaque*/, voidpf stream, void *buf, uLong size)
{
   DataIO * dio = (DataIO *)stream;
   return (uLong) (dio ? dio->ReadFully(buf, size) : 0);
}

static uLong ZCALLBACK fwrite_dataio_func (voidpf /*opaque*/, voidpf stream, const void * buf, uLong size)
{
   DataIO * dio = (DataIO *)stream;
   return (uLong) (dio ? dio->WriteFully(buf, size) : 0);
}

static long ZCALLBACK ftell_dataio_func (voidpf /*opaque*/, voidpf stream)
{
   DataIO * dio = (DataIO *)stream;
   return (long) (dio ? dio->GetPosition() : -1);
}

static long ZCALLBACK fseek_dataio_func (voidpf /*opaque*/, voidpf stream, uLong offset, int origin)
{
   int muscleSeekOrigin;
   switch(origin)
   {
      case ZLIB_FILEFUNC_SEEK_CUR: muscleSeekOrigin = DataIO::IO_SEEK_CUR; break;
      case ZLIB_FILEFUNC_SEEK_END: muscleSeekOrigin = DataIO::IO_SEEK_END; break;
      case ZLIB_FILEFUNC_SEEK_SET: muscleSeekOrigin = DataIO::IO_SEEK_SET; break;
      default:                     return -1;
   }
   DataIO * dio = (DataIO *)stream;
   return (long) ((dio)&&(dio->Seek(offset, muscleSeekOrigin) == B_NO_ERROR)) ? 0 : -1;
}

static int ZCALLBACK fclose_dataio_func (voidpf /*opaque*/, voidpf /*stream*/)
{
   // empty
   return 0;
}

static int ZCALLBACK ferror_dataio_func (voidpf /*opaque*/, voidpf /*stream*/)
{
   // empty
   return -1;
}

static status_t WriteZipFileAux(zipFile zf, const String & baseName, const Message & msg, int compressionLevel, zip_fileinfo * fileInfo)
{
   for (MessageFieldNameIterator iter = msg.GetFieldNameIterator(); iter.HasData(); iter++)
   {
      const String & fn = iter.GetFieldName();

      uint32 fieldType;
      if (msg.GetInfo(fn, &fieldType) != B_NO_ERROR) return B_ERROR;
      switch(fieldType)
      {
         case B_MESSAGE_TYPE:
         {
            String newBaseName = baseName;
            if ((newBaseName.HasChars())&&(newBaseName.EndsWith('/') == false)) newBaseName += '/';
            newBaseName += fn;

            // Message fields we treat as sub-directories   
            MessageRef subMsg;
            for (int32 i=0; msg.FindMessage(fn, i, subMsg) == B_NO_ERROR; i++) if (WriteZipFileAux(zf, newBaseName, *subMsg(), compressionLevel, fileInfo) != B_NO_ERROR) return B_ERROR;
         }
         break;

         case B_RAW_TYPE:
         {
            String fileName = baseName;
            if ((fileName.HasChars())&&(fileName.EndsWith('/') == false)) fileName += '/';
            fileName += fn;

            const void * data;
            uint32 numBytes;
            for (int32 i=0; msg.FindData(fn, B_RAW_TYPE, i, &data, &numBytes) == B_NO_ERROR; i++)
            {
               if (zipOpenNewFileInZip2(zf,
                                        fileName(),  // file name
                                        fileInfo,    // file info
                                        NULL,        // const void* extrafield_local,
                                        0,           // uInt size_extrafield_local,
                                        NULL,        // const void* extrafield_global,
                                        0,           // uInt size_extrafield_global,
                                        NULL,        // const char* comment,
                                        (compressionLevel>0)?Z_DEFLATED:0,  // int method,
                                        compressionLevel,  // int compressionLevel
                                        0) != ZIP_OK) return B_ERROR;
               if (zipWriteInFileInZip(zf, data, numBytes) != ZIP_OK) return B_ERROR;
               if (zipCloseFileInZip(zf) != ZIP_OK) return B_ERROR;
            }
         }
         break;
      }
   }
   return B_NO_ERROR;
}

status_t WriteZipFile(const char * fileName, const Message & msg, int compressionLevel, uint64 fileCreationTime)
{
   FileDataIO fio(fopen(fileName, "wb"));
   return WriteZipFile(fio, msg, compressionLevel, fileCreationTime);
}

status_t WriteZipFile(DataIO & writeTo, const Message & msg, int compressionLevel, uint64 fileCreationTime)
{
   TCHECKPOINT;

   zlib_filefunc_def zdefs = {
      fopen_dataio_func,
      fread_dataio_func,
      fwrite_dataio_func,
      ftell_dataio_func,
      fseek_dataio_func,
      fclose_dataio_func,
      ferror_dataio_func,
      &writeTo
   };
   const char * comment = "";
   zipFile zf = zipOpen2(NULL, false, &comment, &zdefs);
   if (zf)
   {
      zip_fileinfo * fi = NULL;
      zip_fileinfo fileInfo;  
      {
         memset(&fileInfo, 0, sizeof(fileInfo));
         HumanReadableTimeValues v;
         if (GetHumanReadableTimeValues((fileCreationTime==MUSCLE_TIME_NEVER)?GetCurrentTime64(MUSCLE_TIMEZONE_LOCAL):fileCreationTime, v, MUSCLE_TIMEZONE_LOCAL) == B_NO_ERROR)
         {
            fi = &fileInfo;
            fileInfo.tmz_date.tm_sec  = v.GetSecond();
            fileInfo.tmz_date.tm_min  = v.GetMinute();
            fileInfo.tmz_date.tm_hour = v.GetHour();
            fileInfo.tmz_date.tm_mday = v.GetDayOfMonth()+1;
            fileInfo.tmz_date.tm_mon  = v.GetMonth();
            fileInfo.tmz_date.tm_year = v.GetYear();
         }
      }
      
      status_t ret = WriteZipFileAux(zf, "", msg, compressionLevel, fi);
      zipClose(zf, NULL);
      return ret; 
   }
   else return B_ERROR;
}

static status_t ReadZipFileAux(zipFile zf, Message & msg, char * nameBuf, uint32 nameBufLen, bool loadData)
{
   while(unzOpenCurrentFile(zf) == UNZ_OK)
   {
      unz_file_info fileInfo;
      if (unzGetCurrentFileInfo(zf, &fileInfo, nameBuf, nameBufLen, NULL, 0, NULL, 0) != UNZ_OK) return B_ERROR;

      // Add the new entry to the appropriate spot in the tree (demand-allocate sub-Messages as necessary)
      {
         const char * nulByte = strchr(nameBuf, '\0');
         bool isFolder = ((nulByte > nameBuf)&&(*(nulByte-1) == '/'));
         Message * m = &msg;
         StringTokenizer tok(true, nameBuf, "/");
         const char * nextTok;
         while((nextTok = tok()) != NULL)
         {
            String fn(nextTok);
            if ((isFolder)||(tok.GetRemainderOfString()))
            {
               // Demand-allocate a sub-message
               MessageRef subMsg;
               if (m->FindMessage(fn, subMsg) != B_NO_ERROR) 
               {
                  if ((m->AddMessage(fn, Message()) != B_NO_ERROR)||(m->FindMessage(fn, subMsg) != B_NO_ERROR)) return B_ERROR;
               }
               m = subMsg();
            }
            else
            {
               if (loadData)
               {
                  ByteBufferRef bufRef = GetByteBufferFromPool(fileInfo.uncompressed_size);
                  if ((bufRef() == NULL)||(unzReadCurrentFile(zf, bufRef()->GetBuffer(), bufRef()->GetNumBytes()) != (int32)bufRef()->GetNumBytes())||(m->AddFlat(fn, bufRef) != B_NO_ERROR)) return B_ERROR;
               }
               else if (m->AddInt64(fn, fileInfo.uncompressed_size) != B_NO_ERROR) return B_ERROR;
            }
         }
      }
      if (unzCloseCurrentFile(zf) != UNZ_OK) return B_ERROR;
      if (unzGoToNextFile(zf) != UNZ_OK) break;
   }
   return B_NO_ERROR;
}

MessageRef ReadZipFile(const char * fileName, bool loadData)
{
   FileDataIO fio(fopen(fileName, "rb"));
   return ReadZipFile(fio, loadData);
}

MessageRef ReadZipFile(DataIO & readFrom, bool loadData)
{
   TCHECKPOINT;

   static const int NAME_BUF_LEN = 8*1024;  // names longer than 8KB are ridiculous anyway!
   char * nameBuf = newnothrow_array(char, NAME_BUF_LEN);
   if (nameBuf)
   {
      MessageRef ret = GetMessageFromPool();
      if (ret())
      {
         zlib_filefunc_def zdefs = {
            fopen_dataio_func,
            fread_dataio_func,
            fwrite_dataio_func,
            ftell_dataio_func,
            fseek_dataio_func,
            fclose_dataio_func,
            ferror_dataio_func,
            &readFrom
         };
         zipFile zf = unzOpen2(NULL, &zdefs);
         if (zf != NULL)
         {
            if (ReadZipFileAux(zf, *ret(), nameBuf, NAME_BUF_LEN, loadData) != B_NO_ERROR) ret.Reset();
            unzClose(zf);
         }
         else ret.Reset();  // failure!
      }
      delete [] nameBuf;
      return ret;
   }
   else WARN_OUT_OF_MEMORY;

   return MessageRef();
}

}; // end namespace muscle

#endif
