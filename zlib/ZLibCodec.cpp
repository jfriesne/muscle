/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "dataio/DataIO.h"
#include "zlib/ZLibCodec.h"
#include "system/GlobalMemoryAllocator.h"

namespace muscle {

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
static void * muscleZLibAlloc(void *, uInt items, uInt size) {using namespace muscle; return muscleAlloc(items*size);}
static void muscleZLibFree(void *, void * address) {using namespace muscle; muscleFree(address);}
# define MUSCLE_ZLIB_ALLOC muscleZLibAlloc
# define MUSCLE_ZLIB_FREE  muscleZLibFree
#else
# define MUSCLE_ZLIB_ALLOC Z_NULL
# define MUSCLE_ZLIB_FREE  Z_NULL
#endif

ZLibCodec :: ZLibCodec(int compressionLevel)
   : _compressionLevel(muscleClamp(compressionLevel, 0, 9))
{
   InitStream(_inflater);
   _inflateOkay = (inflateInit(&_inflater) == Z_OK);

   InitStream(_deflater);
   _deflateOkay = (deflateInit(&_deflater, _compressionLevel) == Z_OK);
}

ZLibCodec :: ~ZLibCodec()
{
   if (_inflateOkay)
   {
      inflateEnd(&_inflater);
      _inflateOkay = false;
   }
   if (_deflateOkay)
   {
      deflateEnd(&_deflater);
      _deflateOkay = false;
   }
}

void ZLibCodec :: InitStream(z_stream & stream)
{
   stream.next_in   = Z_NULL;
   stream.avail_in  = 0;
   stream.total_in  = 0;

   stream.next_out  = Z_NULL;
   stream.avail_out = 0;
   stream.total_out = 0;

   stream.zalloc    = MUSCLE_ZLIB_ALLOC;
   stream.zfree     = MUSCLE_ZLIB_FREE;
   stream.opaque    = Z_NULL;
}

static const uint32 ZLIB_CODEC_HEADER_DEPENDENT   = 2053925218;               // 'zlib'
static const uint32 ZLIB_CODEC_HEADER_INDEPENDENT = 2053925219;               // 'zlic'
static const uint32 ZLIB_CODEC_HEADER_SIZE  = sizeof(uint32)+sizeof(uint32);  // 4 bytes of magic, 4 bytes of raw-size

// (headerBuf) must point to at least ZLIB_CODEC_HEADER_SIZE bytes of writable memory!
static void WriteZLibCodecHeader(uint8 * headerBuf, bool independent, uint32 totalBytesToRead)
{
   muscleCopyOut(&headerBuf[0*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(independent ? ZLIB_CODEC_HEADER_INDEPENDENT : ZLIB_CODEC_HEADER_DEPENDENT));
   muscleCopyOut(&headerBuf[1*sizeof(uint32)], B_HOST_TO_LENDIAN_INT32(totalBytesToRead));
}

ByteBufferRef ZLibCodec :: Deflate(const uint8 * rawBytes, uint32 numRaw, bool independent, uint32 addHeaderBytes, uint32 addFooterBytes)
{
   ByteBufferRef ret; 
   if ((rawBytes)&&(_deflateOkay))
   {
      if ((independent)&&(deflateReset(&_deflater) != Z_OK))
      {
         _deflateOkay = false;
         return ByteBufferRef();
      }

      const uint32 compAvailSize = (uint32) (ZLIB_CODEC_HEADER_SIZE+deflateBound(&_deflater, numRaw)+13);
      ret = GetByteBufferFromPool(addHeaderBytes+compAvailSize+addFooterBytes);
      if (ret())
      {
         _deflater.next_in   = (Bytef *)rawBytes;
         _deflater.total_in  = 0;
         _deflater.avail_in  = numRaw;

         _deflater.next_out  = ret()->GetBuffer()+addHeaderBytes+ZLIB_CODEC_HEADER_SIZE;
         _deflater.total_out = 0;
         _deflater.avail_out = compAvailSize;  // doesn't include the users add-header or add-footer bytes!
         
         if ((deflate(&_deflater, Z_SYNC_FLUSH) == Z_OK)&&(ret()->SetNumBytes((uint32)(addHeaderBytes+ZLIB_CODEC_HEADER_SIZE+_deflater.total_out+addFooterBytes), true).IsOK()))
         {
            (void) ret()->FreeExtraBytes();  // no sense keeping all that extra space around, is there?
            WriteZLibCodecHeader(ret()->GetBuffer()+addHeaderBytes, independent, numRaw);
         }
         else ret.Reset();  // oops, something went wrong!
      }
   }
   return ret;
}

status_t ZLibCodec :: Deflate(const uint8 * rawBytes, uint32 numRaw, bool independent, ByteBuffer & outBuf, uint32 addHeaderBytes, uint32 addFooterBytes)
{
   if (rawBytes     == NULL)  return B_BAD_ARGUMENT;
   if (_deflateOkay == false) return B_BAD_OBJECT;
   
   if ((independent)&&(deflateReset(&_deflater) != Z_OK))
   {
      _deflateOkay = false;
      return B_ZLIB_ERROR;
   }

   status_t ret;
   const uint32 compAvailSize = (uint32)(ZLIB_CODEC_HEADER_SIZE+deflateBound(&_deflater, numRaw)+13);
   if (outBuf.SetNumBytes(addHeaderBytes+compAvailSize+addFooterBytes, false).IsOK(ret))
   {
      _deflater.next_in   = (Bytef *)rawBytes;
      _deflater.total_in  = 0;
      _deflater.avail_in  = numRaw;

      _deflater.next_out  = outBuf.GetBuffer()+addHeaderBytes+ZLIB_CODEC_HEADER_SIZE;
      _deflater.total_out = 0;
      _deflater.avail_out = compAvailSize;  // doesn't include the users add-header or add-footer bytes!
      
      if (deflate(&_deflater, Z_SYNC_FLUSH) != Z_OK) return B_ZLIB_ERROR;
      if (outBuf.SetNumBytes((uint32)(addHeaderBytes+ZLIB_CODEC_HEADER_SIZE+_deflater.total_out+addFooterBytes), true).IsOK(ret))
      {
         WriteZLibCodecHeader(outBuf.GetBuffer()+addHeaderBytes, independent, numRaw);
         return B_NO_ERROR;
      }
      else return ret;
   }
   else return ret;
}

int32 ZLibCodec :: GetInflatedSize(const uint8 * compBytes, uint32 numComp, bool * optRetIsIndependent) const
{
   if ((compBytes)&&(numComp >= ZLIB_CODEC_HEADER_SIZE))
   {
      const uint32 magic = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(compBytes));
      if ((magic == ZLIB_CODEC_HEADER_INDEPENDENT)||(magic == ZLIB_CODEC_HEADER_DEPENDENT))
      {
         if (optRetIsIndependent) *optRetIsIndependent = (magic == ZLIB_CODEC_HEADER_INDEPENDENT);
         return B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(compBytes+sizeof(magic)));
      }
   }
   return -1;
}

ByteBufferRef ZLibCodec :: Inflate(const uint8 * compBytes, uint32 numComp)
{
   ByteBufferRef ret;

   bool independent;
   const int32 rawLen = GetInflatedSize(compBytes, numComp, &independent);
   if ((rawLen >= 0)&&(_inflateOkay))
   {
      if ((independent)&&(inflateReset(&_inflater) != Z_OK))
      {
         _inflateOkay = false;
         return ByteBufferRef();
      }

      ret = GetByteBufferFromPool(rawLen);
      if (ret())
      {
         _inflater.next_in   = (Bytef *) (compBytes+ZLIB_CODEC_HEADER_SIZE);
         _inflater.total_in  = 0;
         _inflater.avail_in  = numComp-ZLIB_CODEC_HEADER_SIZE;

         _inflater.next_out  = ret()->GetBuffer();
         _inflater.total_out = 0;
         _inflater.avail_out = ret()->GetNumBytes();

         const int zRet = inflate(&_inflater, Z_SYNC_FLUSH);
         if (((zRet != Z_OK)&&(zRet != Z_STREAM_END))||((int32)_inflater.total_out != rawLen)) ret.Reset();  // oopsie!
      }
   }
   return ret;
}

status_t ZLibCodec :: Inflate(const uint8 * compBytes, uint32 numComp, ByteBuffer & outBuf)
{
   bool independent;
   const int32 rawLen = GetInflatedSize(compBytes, numComp, &independent);
   if (rawLen < 0)            return B_BAD_ARGUMENT;
   if (_inflateOkay == false) return B_BAD_OBJECT;

   if ((independent)&&(inflateReset(&_inflater) != Z_OK))
   {
      _inflateOkay = false;
      return B_ZLIB_ERROR;
   }

   status_t ret;
   if (outBuf.SetNumBytes(rawLen, false).IsOK(ret))
   {
      _inflater.next_in   = (Bytef *) (compBytes+ZLIB_CODEC_HEADER_SIZE);
      _inflater.total_in  = 0;
      _inflater.avail_in  = numComp-ZLIB_CODEC_HEADER_SIZE;

      _inflater.next_out  = outBuf.GetBuffer();
      _inflater.total_out = 0;
      _inflater.avail_out = outBuf.GetNumBytes();

      const int zRet = inflate(&_inflater, Z_SYNC_FLUSH);
      return (((zRet != Z_OK)&&(zRet != Z_STREAM_END))||((int32)_inflater.total_out != rawLen)) ? B_ZLIB_ERROR : B_NO_ERROR;
   }
   else return ret;
}

status_t ZLibCodec :: ReadAndDeflateAndWrite(DataIO & sourceRawIO, DataIO & destDeflatedIO, bool independent, uint32 totalBytesToRead)
{
   if (_deflateOkay == false) return B_BAD_OBJECT;

   ByteBuffer scratchInBuf(32*1024);
   if (scratchInBuf.GetNumBytes() == 0) MRETURN_OUT_OF_MEMORY;

   ByteBuffer scratchOutBuf(scratchInBuf.GetNumBytes()*2);  // yes, bigger than scratchInBuf!  Because I'm paranoid
   if (scratchOutBuf.GetNumBytes() == 0) MRETURN_OUT_OF_MEMORY;

   if ((independent)&&(deflateReset(&_deflater) != Z_OK))
   {
      _deflateOkay = false;
      return B_ZLIB_ERROR;
   }

   uint8 headerBuf[ZLIB_CODEC_HEADER_SIZE];
   WriteZLibCodecHeader(headerBuf, independent, totalBytesToRead);
   if (destDeflatedIO.WriteFully(headerBuf, sizeof(headerBuf)) != sizeof(headerBuf)) return B_IO_ERROR;

   _deflater.next_in   = scratchInBuf.GetBuffer();
   _deflater.avail_in  = 0;
   _deflater.total_in  = 0;
   _deflater.total_out = 0;

   uint32 numRawBytesLeftToRead = totalBytesToRead;
   while((numRawBytesLeftToRead > 0)||(_deflater.avail_in > 0))
   {
      // We'll always deflate to the same destination, since we Write() all deflated bytes out immediately each time
      _deflater.next_out  = scratchOutBuf.GetBuffer();
      _deflater.avail_out = scratchOutBuf.GetNumBytes();

      // Pull in some more input data, if we don't have any
      if ((_deflater.avail_in == 0)&&(numRawBytesLeftToRead > 0))
      {
         const int32 numBytesRead = sourceRawIO.Read(scratchInBuf.GetBuffer(), muscleMin(numRawBytesLeftToRead, scratchInBuf.GetNumBytes()));
         if (numBytesRead <= 0) return B_IO_ERROR;

         numRawBytesLeftToRead -= numBytesRead;
         _deflater.next_in  = scratchInBuf.GetBuffer();
         _deflater.avail_in = numBytesRead;
      }

      const int zRet = deflate(&_deflater, Z_SYNC_FLUSH);
      if ((zRet != Z_OK)&&(zRet != Z_STREAM_END)) return B_ZLIB_ERROR;

      // If deflate() generated some deflated bytes, write them out to the destDeflatedIO
      const int32 numBytesProduced = (int32)(_deflater.next_out-scratchOutBuf.GetBuffer());
      if (numBytesProduced > 0)
      {
         const int32 numBytesWritten = destDeflatedIO.WriteFully(scratchOutBuf.GetBuffer(), numBytesProduced);
         if (numBytesWritten != numBytesProduced) return B_IO_ERROR;
      }
   }

   return B_NO_ERROR;
}

status_t ZLibCodec :: ReadAndInflateAndWrite(DataIO & sourceDeflatedIO, DataIO & destInflatedIO)
{
   if (_inflateOkay == false) return B_BAD_OBJECT;

   ByteBuffer scratchInBuf(32*1024);
   if (scratchInBuf.GetNumBytes() == 0) MRETURN_OUT_OF_MEMORY;

   ByteBuffer scratchOutBuf(scratchInBuf.GetNumBytes()*8);
   if (scratchOutBuf.GetNumBytes() == 0) MRETURN_OUT_OF_MEMORY;

   uint8 headerBuf[ZLIB_CODEC_HEADER_SIZE];
   const uint32 headerBytesRead = sourceDeflatedIO.ReadFully(headerBuf, sizeof(headerBuf));
   if (headerBytesRead != sizeof(headerBuf)) return B_IO_ERROR;

   bool independent;
   const int32 numBytesToBeWritten = GetInflatedSize(headerBuf, sizeof(headerBuf), &independent);
   if (numBytesToBeWritten < 0) return B_BAD_DATA;

   if ((independent)&&(inflateReset(&_inflater) != Z_OK))
   {
      _inflateOkay = false;
      return B_ZLIB_ERROR;
   }

   _inflater.next_in   = scratchInBuf.GetBuffer();
   _inflater.avail_in  = 0;
   _inflater.total_in  = 0;
   _inflater.total_out = 0;

   while(_inflater.total_out < (uint32)numBytesToBeWritten)
   {
      // We'll always inflate to the same destination, since we Write() all inflated bytes out immediately each time
      _inflater.next_out  = scratchOutBuf.GetBuffer();
      _inflater.avail_out = scratchOutBuf.GetNumBytes();

      // Pull in some more input data, if we don't have any
      if (_inflater.avail_in == 0)
      {
         const int32 numBytesRead = sourceDeflatedIO.Read(scratchInBuf.GetBuffer(), scratchInBuf.GetNumBytes());
         if (numBytesRead <= 0) return B_IO_ERROR;

         _inflater.next_in  = scratchInBuf.GetBuffer();
         _inflater.avail_in = numBytesRead;
      }

      const int zRet = inflate(&_inflater, Z_SYNC_FLUSH);
      if ((zRet != Z_OK)&&(zRet != Z_STREAM_END)) return B_ZLIB_ERROR;

      // If inflate() generated some inflated bytes, write them out to the destInflatedIO
      const int32 numBytesProduced = (int32)(_inflater.next_out-scratchOutBuf.GetBuffer());
      if (numBytesProduced > 0)
      {
         const int32 numBytesWritten = destInflatedIO.WriteFully(scratchOutBuf.GetBuffer(), numBytesProduced);
         if (numBytesWritten != numBytesProduced) return B_IO_ERROR;
      }
   }
   return B_NO_ERROR;
}

} // end namespace muscle

#endif
