/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
   , _useGZip(false)
{
   Init();
   (void) SetChildDataIO(DataIORef());  // necessary to get the ZLib stuff initialized
}

ZLibDataIO :: ZLibDataIO(const DataIORef & childIO, int compressionLevel)
   : _compressionLevel(compressionLevel)
   , _useGZip(false)
{
   Init();
   (void) SetChildDataIO(childIO);  // necessary to get the ZLib stuff initialized
}

ZLibDataIO :: ZLibDataIO(const DataIORef & childIO, int compressionLevel, bool useGZip)
   : _compressionLevel(compressionLevel)
   , _useGZip(useGZip)
{
   Init();
   (void) SetChildDataIO(childIO);  // necessary to get the ZLib stuff initialized
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
      bool isFinished = false;
      while((isFinished == false)&&(WriteAux(NULL, 0, true, &isFinished) >= 0)) {/* empty */}
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

status_t ZLibDataIO :: SetChildDataIO(const DataIORef & dio)
{
   CleanupZLib();
   Init();

   _childDataIO     = dio;
   _inputStreamOkay = (GetChildDataIO()() != NULL);

   if (_useGZip)
   {
      // Code to enable GZ-compression mode borrowed from:  https://stackoverflow.com/a/2121190/131930
      const int enableGZWindowBits = MAX_WBITS+16; /* MAX_WBITS is default, plus 16 to enable gzip format */
      _inflateAllocated = _inflateOkay = ((_inputStreamOkay)&&(inflateInit2(&_readInflater, enableGZWindowBits)                                   == Z_OK));
      _deflateAllocated =                (deflateInit2(&_writeDeflater, _compressionLevel, Z_DEFLATED, enableGZWindowBits, 8, Z_DEFAULT_STRATEGY) == Z_OK);
   }
   else
   {
      _inflateAllocated = _inflateOkay = ((_inputStreamOkay)&&(inflateInit(&_readInflater) == Z_OK));
      _deflateAllocated =                 (deflateInit(&_writeDeflater, _compressionLevel) == Z_OK);
   }

   return ((_inflateAllocated)&&(_deflateAllocated)) ? B_NO_ERROR : B_ERROR("zlib init failure");
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
         const int32 bytesRead = GetChildDataIO()()->Read(_readInflater.next_in, (int32)((_toInflateBuf+sizeof(_toInflateBuf))-_readInflater.next_in));
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
   return WriteAux(buffer, size, false, NULL);
}

int32 ZLibDataIO :: WriteDeflatedOutputToChild()
{
   if (GetChildDataIO()() == NULL) return -1;

   int32 totalBytesWritten = 0;
   while(_writeDeflater.next_out > _sendToChild)
   {
      const uint32 bytesToWrite = (uint32)(_writeDeflater.next_out-_sendToChild);
      const  int32 bytesWritten = GetChildDataIO()()->Write(_sendToChild, bytesToWrite);
      if (bytesWritten < 0) return -1;

      totalBytesWritten += bytesWritten;
      _sendToChild      += bytesWritten;

      if (_sendToChild == _writeDeflater.next_out)
      {
         _sendToChild = _writeDeflater.next_out = _deflatedBuf;
         _writeDeflater.avail_out = sizeof(_deflatedBuf);
      }
   }
   return totalBytesWritten;
}

int32 ZLibDataIO :: WriteAux(const void * buffer, uint32 size, bool flushAtEnd, bool * optFinishingUp)
{
   if ((GetChildDataIO()())&&(_deflateAllocated))
   {
      const int32 preWrittenToChildBytes = WriteDeflatedOutputToChild();
      if (preWrittenToChildBytes < 0) return -1;

      int32 bytesAbsorbed = 0;

      if (_writeDeflater.avail_in == 0) _writeDeflater.next_in = _toDeflateBuf;
      if (buffer)
      {
         uint8 * writeTo = _writeDeflater.next_in + _writeDeflater.avail_in;
         const uint32 bytesToCopy = muscleMin((uint32)((_toDeflateBuf+sizeof(_toDeflateBuf))-writeTo), size);
         memcpy(writeTo, buffer, bytesToCopy);
         _writeDeflater.avail_in += bytesToCopy;
         bytesAbsorbed           += bytesToCopy;
      }

      const int zRet = deflate(&_writeDeflater, optFinishingUp ? Z_FINISH : (((flushAtEnd)&&(buffer==NULL)) ? Z_SYNC_FLUSH : Z_NO_FLUSH));

           if ((optFinishingUp)&&(zRet == Z_STREAM_END)) *optFinishingUp = true;
      else if ((zRet != Z_OK)&&(zRet != Z_BUF_ERROR))    return -1;

      const int32 postWrittenToChildBytes = WriteDeflatedOutputToChild();
      if (postWrittenToChildBytes < 0) return -1;

      // Try to avoid returning 0 just because zlib needed buffers to be flushed; blocking callers don't like it when WriteFully() returns a short write
      return bytesAbsorbed ? bytesAbsorbed : (((zRet == Z_STREAM_END)&&(preWrittenToChildBytes==0)&&(postWrittenToChildBytes==0)) ? 0 : WriteAux(buffer, size, flushAtEnd, optFinishingUp));
   }
   return -1;
}

void ZLibDataIO :: FlushOutput()
{
   if (GetChildDataIO()())
   {
      for (uint32 i=0; i<3; i++) WriteAux(NULL, 0, true, NULL);  // try to flush any/all buffered data out first...
      GetChildDataIO()()->FlushOutput();
   }
}

void ZLibDataIO :: Shutdown()
{
   FlushOutput();
   if (GetChildDataIO()()) GetChildDataIO()()->Shutdown();
   CleanupZLib();
   _inputStreamOkay = false;
}

const ConstSocketRef & ZLibDataIO ::  GetReadSelectSocket() const {return GetChildDataIO()() ? GetChildDataIO()()->GetReadSelectSocket()  : GetDefaultObjectForType<ConstSocketRef>();}
const ConstSocketRef & ZLibDataIO :: GetWriteSelectSocket() const {return GetChildDataIO()() ? GetChildDataIO()()->GetWriteSelectSocket() : GetDefaultObjectForType<ConstSocketRef>();}

bool ZLibDataIO :: HasBufferedOutput() const
{
   return ((_sendToChild < _writeDeflater.next_out)||(_writeDeflater.avail_in > 0));
}

void ZLibDataIO :: WriteBufferedOutput()
{
   (void) WriteAux(NULL, 0, false, NULL);
}

} // end namespace muscle

#endif
