/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "zlib/ZLibDataIO.h"
#include "system/GlobalMemoryAllocator.h"

namespace muscle {

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
static void * muscleZLibAlloc(void *, uInt items, uInt size) {return muscleAlloc(items*size);}
static void muscleZLibFree(void *, void * address) {muscleFree(address);}
# define MUSCLE_ZLIB_ALLOC muscleZLibAlloc
# define MUSCLE_ZLIB_FREE  muscleZLibFree
#else
# define MUSCLE_ZLIB_ALLOC Z_NULL
# define MUSCLE_ZLIB_FREE  Z_NULL
#endif

ZLibDataIO :: ZLibDataIO(int compressionLevel)
   : _compressionLevel(compressionLevel)
{
   Init();
   SetChildDataIO(DataIORef());  // necessary to get the ZLib stuff initialized
}

ZLibDataIO :: ZLibDataIO(const DataIORef & childIO, int compressionLevel)
   : _compressionLevel(compressionLevel)  // deliberately NOT calling ProxyDataIO(childIO) ctor here!
{
   Init();
   SetChildDataIO(childIO);  // necessary to get the ZLib stuff initialized
}

ZLibDataIO :: ~ZLibDataIO()
{
   CleanupZLib();
}

void ZLibDataIO :: CleanupZLib()
{
   if (_inflateAllocated)
   {
      inflateEnd(&_readInflater);
      _inflateAllocated = _inflateOkay = false;
   }
   if (_deflateAllocated)
   {
      deflateEnd(&_writeDeflater);
      _deflateAllocated = false;
   }
}

void ZLibDataIO :: InitStream(z_stream & stream, uint8 * toStreamBuf, uint8 * fromStreamBuf, uint32 bufSize)
{
   stream.next_in   = toStreamBuf;
   stream.avail_in  = 0;
   stream.total_in  = 0;

   stream.next_out  = fromStreamBuf;
   stream.avail_out = bufSize;
   stream.total_out = 0;

   stream.zalloc    = MUSCLE_ZLIB_ALLOC;
   stream.zfree     = MUSCLE_ZLIB_FREE;
   stream.opaque    = Z_NULL;
}

// Note:  assumes nothing is allocated!
void ZLibDataIO :: Init()
{
   InitStream(_readInflater, _toInflateBuf, _inflatedBuf, sizeof(_inflatedBuf));
   _inflateAllocated = _inflateOkay = _inputStreamOkay = false;
   _sendToUser = _inflatedBuf;

   InitStream(_writeDeflater, _toDeflateBuf, _deflatedBuf, sizeof(_deflatedBuf));
   _deflateAllocated = false;
   _sendToChild = _deflatedBuf;
}

void ZLibDataIO :: SetChildDataIO(const DataIORef & dio)
{
   CleanupZLib();
   Init();

   ProxyDataIO::SetChildDataIO(dio);
   _inputStreamOkay = (GetChildDataIO()() != NULL);
   _inflateAllocated = _inflateOkay = ((_inputStreamOkay)&&(inflateInit(&_readInflater) == Z_OK));
   _deflateAllocated = (deflateInit(&_writeDeflater, _compressionLevel) == Z_OK);
}

#define ZLIB_READ_COPY_TO_USER                                                            \
   if (_readInflater.next_out > _sendToUser)                                              \
   {                                                                                      \
      const uint32 bytesToCopy = muscleMin(size, (uint32)(_readInflater.next_out-_sendToUser)); \
      memcpy(buffer, _sendToUser, bytesToCopy);                                           \
      uint8 * buf8 = (uint8 *) buffer;                                                    \
      buffer       = buf8+bytesToCopy;                                                    \
      bytesAdded  += bytesToCopy;                                                         \
      size        -= bytesToCopy;                                                         \
      _sendToUser += bytesToCopy;                                                         \
      if (_sendToUser == _readInflater.next_out)                                          \
      {                                                                                   \
         _sendToUser = _readInflater.next_out = _inflatedBuf;                             \
         _readInflater.avail_out = sizeof(_inflatedBuf);                                  \
      }                                                                                   \
   }

#define ZLIB_READ_INFLATE                                              \
   if (_inflateOkay)                                                   \
   {                                                                   \
      const int zRet = inflate(&_readInflater, Z_NO_FLUSH);            \
      if ((zRet != Z_OK)&&(zRet != Z_BUF_ERROR)) _inflateOkay = false; \
      ZLIB_READ_COPY_TO_USER;                                          \
   }
         
int32 ZLibDataIO :: Read(void * buffer, uint32 size)
{
   int32 bytesAdded = 0;
   if (GetChildDataIO()())
   {
      ZLIB_READ_COPY_TO_USER; // First, hand any pre-inflated bytes over to the user
      ZLIB_READ_INFLATE;      // Then try to inflate some more bytes

      // Lastly, try to read and inflate some more bytes from our stream
      if (_inputStreamOkay)
      {
         if (_readInflater.avail_in == 0) _readInflater.next_in = _toInflateBuf;
         const int32 bytesRead = ProxyDataIO::Read(_readInflater.next_in, (int32)((_toInflateBuf+sizeof(_toInflateBuf))-_readInflater.next_in));
         if (bytesRead >= 0)
         {
            _readInflater.avail_in += bytesRead;
            ZLIB_READ_INFLATE;
         }
         else _inputStreamOkay = false;
      }
   }
   return (bytesAdded > 0) ? bytesAdded : (((_inputStreamOkay)&&(_inflateOkay)) ? 0 : -1);
}

int32 ZLibDataIO :: Write(const void * buffer, uint32 size)
{
   return WriteAux(buffer, size, false);
}

#define ZLIB_WRITE_SEND_TO_SLAVE                                                                           \
   if (_writeDeflater.next_out > _sendToChild)                                                             \
   {                                                                                                       \
      const int32 bytesWritten = ProxyDataIO::Write(_sendToChild, (int32)(_writeDeflater.next_out-_sendToChild)); \
      if (bytesWritten >= 0)                                                                               \
      {                                                                                                    \
         _sendToChild += bytesWritten;                                                                     \
         if (_sendToChild == _writeDeflater.next_out)                                                      \
         {                                                                                                 \
            _sendToChild = _writeDeflater.next_out = _deflatedBuf;                                         \
            _writeDeflater.avail_out = sizeof(_deflatedBuf);                                               \
         }                                                                                                 \
      }                                                                                                    \
      else return -1;                                                                                      \
   }

int32 ZLibDataIO :: WriteAux(const void * buffer, uint32 size, bool flushAtEnd)
{
   if ((GetChildDataIO()())&&(_deflateAllocated))
   {
      int32 bytesCompressed = 0;

      ZLIB_WRITE_SEND_TO_SLAVE;
      if (_sendToChild == _deflatedBuf)
      {
         if (_writeDeflater.avail_in == 0) _writeDeflater.next_in = _toDeflateBuf;
         if (buffer)
         {
            const uint32 bytesToCopy = muscleMin((uint32)((_toDeflateBuf+sizeof(_toDeflateBuf))-_writeDeflater.next_in), size);
            memcpy(_writeDeflater.next_in, buffer, bytesToCopy);
            bytesCompressed += bytesToCopy;
#ifdef REMOVED_TO_SUPPRESS_CLANG_STATIC_ANALYZER_WARNING_BUT_REENABLE_THIS_IF_THERES_ANOTHER_STEP_ADDED_IN_THE_FUTURE
            uint8 * buf8 = (uint8 *) buffer;
            buffer = buf8+bytesToCopy;
            size -= bytesToCopy;
#endif
            _writeDeflater.avail_in += bytesToCopy;
         }

         const int zRet = deflate(&_writeDeflater, ((flushAtEnd)&&(_writeDeflater.avail_in == 0)) ? Z_SYNC_FLUSH : Z_NO_FLUSH);
         if ((zRet != Z_OK)&&(zRet != Z_BUF_ERROR)) return -1;
         ZLIB_WRITE_SEND_TO_SLAVE;
      }
      return bytesCompressed;
   }
   return -1;
}

void ZLibDataIO :: FlushOutput()
{
   if (GetChildDataIO()()) for (uint32 i=0; i<3; i++) WriteAux(NULL, 0, true);  // try to flush any/all buffered data out first...
   ProxyDataIO::FlushOutput();
}

void ZLibDataIO :: Shutdown()
{
   FlushOutput();
   ProxyDataIO::Shutdown();
   CleanupZLib();
   _inputStreamOkay = false;
}

bool ZLibDataIO :: HasBufferedOutput() const
{
   return ((_sendToChild < _writeDeflater.next_out)||(_writeDeflater.avail_in > 0));
}

void ZLibDataIO :: WriteBufferedOutput()
{
   (void) WriteAux(NULL, 0, false);
}

} // end namespace muscle

#endif
