/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

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

ZLibCodec :: ZLibCodec(int compressionLevel) : _compressionLevel(muscleClamp(compressionLevel, 0, 9))
{
   TCHECKPOINT;

   InitStream(_inflater);
   _inflateOkay = (inflateInit(&_inflater) == Z_OK);

   InitStream(_deflater);
   _deflateOkay = (deflateInit(&_deflater, _compressionLevel) == Z_OK);
}

ZLibCodec :: ~ZLibCodec()
{
   TCHECKPOINT;

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

ByteBufferRef ZLibCodec :: Deflate(const uint8 * rawBytes, uint32 numRaw, bool independent, uint32 addHeaderBytes, uint32 addFooterBytes)
{
   TCHECKPOINT;

   ByteBufferRef ret; 
   if ((rawBytes)&&(_deflateOkay))
   {
      if ((independent)&&(deflateReset(&_deflater) != Z_OK))
      {
         _deflateOkay = false;
         return ByteBufferRef();
      }

      uint32 compAvailSize = ZLIB_CODEC_HEADER_SIZE+deflateBound(&_deflater, numRaw)+13;
      ret = GetByteBufferFromPool(addHeaderBytes+compAvailSize+addFooterBytes);
      if (ret())
      {
         _deflater.next_in   = (Bytef *)rawBytes;
         _deflater.total_in  = 0;
         _deflater.avail_in  = numRaw;

         _deflater.next_out  = ret()->GetBuffer()+ZLIB_CODEC_HEADER_SIZE+addHeaderBytes;
         _deflater.total_out = 0;
         _deflater.avail_out = compAvailSize;  // doesn't include the users add-header or add-footer bytes!
         
         if ((deflate(&_deflater, Z_SYNC_FLUSH) == Z_OK)&&(ret()->SetNumBytes(addHeaderBytes+ZLIB_CODEC_HEADER_SIZE+_deflater.total_out+addFooterBytes, true) == B_NO_ERROR))
         {
            (void) ret()->FreeExtraBytes();  // no sense keeping all that extra space around, is there?

            uint8 * compBytes = ret()->GetBuffer()+addHeaderBytes;  // important -- it might have changed!

            const uint32 magic = B_HOST_TO_LENDIAN_INT32(independent ? ZLIB_CODEC_HEADER_INDEPENDENT : ZLIB_CODEC_HEADER_DEPENDENT);
            muscleCopyOut(compBytes, magic);

            const uint32 rawLen = B_HOST_TO_LENDIAN_INT32(numRaw);
            muscleCopyOut(&compBytes[sizeof(magic)], rawLen); 
//printf("Deflated " UINT32_FORMAT_SPEC" bytes to " UINT32_FORMAT_SPEC" bytes\n", numRaw, ret()->GetNumBytes());
         }
         else ret.Reset();  // oops, something went wrong!
      }
   }
   return ret;
}

int32 ZLibCodec :: GetInflatedSize(const uint8 * compBytes, uint32 numComp, bool * optRetIsIndependent) const
{
   TCHECKPOINT;

   if ((compBytes)&&(numComp >= ZLIB_CODEC_HEADER_SIZE))
   {
      uint32 magic; muscleCopyIn(magic, compBytes); magic = B_LENDIAN_TO_HOST_INT32(magic);
      if ((magic == ZLIB_CODEC_HEADER_INDEPENDENT)||(magic == ZLIB_CODEC_HEADER_DEPENDENT))
      {
         if (optRetIsIndependent) *optRetIsIndependent = (magic == ZLIB_CODEC_HEADER_INDEPENDENT);
         uint32 rawLen; muscleCopyIn(rawLen, compBytes+sizeof(magic));
         return B_LENDIAN_TO_HOST_INT32(rawLen);
      }
   }
   return -1;
}

ByteBufferRef ZLibCodec :: Inflate(const uint8 * compBytes, uint32 numComp)
{
   TCHECKPOINT;

   ByteBufferRef ret;

   bool independent;
   int32 rawLen = GetInflatedSize(compBytes, numComp, &independent);
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

         int zRet = inflate(&_inflater, Z_SYNC_FLUSH);
         if (((zRet != Z_OK)&&(zRet != Z_STREAM_END))||((int32)_inflater.total_out != rawLen)) ret.Reset();  // oopsie!
//printf("Inflated " UINT32_FORMAT_SPEC" bytes to " UINT32_FORMAT_SPEC" bytes\n", numComp, rawLen);
      }
   }
   return ret;
}

}; // end namespace muscle

#endif
