#include "dataio/SeekableDataIO.h"
#include "util/ByteBuffer.h"
#include "util/MiscUtilityFunctions.h"
#include "system/GlobalMemoryAllocator.h"

namespace muscle {

void ByteBuffer :: AdoptBuffer(uint32 numBytes, uint8 * optBuffer)
{
   Clear(true);  // free any previously held array
   _buffer = optBuffer;
   _numValidBytes = _numAllocatedBytes = numBytes;
}

status_t ByteBuffer :: SetBuffer(uint32 numBytes, const uint8 * buffer)
{
   if (IsByteInLocalBuffer(buffer))
   {
      // Special logic for handling it when the caller wants our bytes-array to become a subset of its former self.
      const uint32 numReadableBytes = (uint32)((_buffer+_numValidBytes)-buffer);
      if (numBytes > numReadableBytes)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "ByteBuffer::SetBuffer();  Attempted to read " UINT32_FORMAT_SPEC " bytes off the end of our internal buffer!\n", numBytes-numReadableBytes);
         return B_BAD_ARGUMENT;
      }
      else
      {
         if (buffer > _buffer) memmove(_buffer, buffer, numBytes);
         return SetNumBytes(numBytes, true);
      }
   }
   else
   {
      Clear(numBytes<(_numAllocatedBytes/2));  // FogBugz #6933: if the new buffer takes up less than half of our current space, toss it

      MRETURN_ON_ERROR(SetNumBytes(numBytes, false));
      if ((buffer)&&(_buffer)) memcpy(_buffer, buffer, numBytes);
      return B_NO_ERROR;
   }
}

status_t ByteBuffer :: SetNumBytes(uint32 newNumBytes, bool retainData)
{
   TCHECKPOINT;

   if (newNumBytes > _numAllocatedBytes)
   {
      IMemoryAllocationStrategy * as = GetMemoryAllocationStrategy();
      if (retainData)
      {
         uint8 * newBuf = (uint8 *) (as ? as->Realloc(_buffer, newNumBytes, _numAllocatedBytes, true) : muscleRealloc(_buffer, newNumBytes));
         MRETURN_OOM_ON_NULL(newBuf);

         _buffer = newBuf;
         _numAllocatedBytes = _numValidBytes = newNumBytes;
      }
      else
      {
         uint8 * newBuf = (uint8 *) (as ? as->Malloc(newNumBytes) : muscleAlloc(newNumBytes));
         MRETURN_OOM_ON_NULL(newBuf);

         if (as) as->Free(_buffer, _numAllocatedBytes);
            else muscleFree(_buffer);

         _buffer = newBuf;
         _numAllocatedBytes = _numValidBytes = newNumBytes;
      }
   }
   else _numValidBytes = newNumBytes;  // truncating our array is easy!

   return B_NO_ERROR;
}

status_t ByteBuffer :: AppendBytes(const uint8 * bytes, uint32 numBytes, bool allocExtra)
{
   if (numBytes == 0) return B_NO_ERROR;

   if ((bytes)&&(IsByteInLocalBuffer(bytes))&&((_numValidBytes+numBytes)>_numAllocatedBytes))
   {
      // Oh dear, caller wants us to add a copy of some of our own bytes to ourself, AND we'll need to perform a reallocation to do it!
      // So to avoid freeing (bytes) before we read from them, we're going to copy them over to a temporary buffer first.
      uint8 * tmpBuf = newnothrow_array(uint8,numBytes);
      MRETURN_OOM_ON_NULL(tmpBuf);
      memcpy(tmpBuf, bytes, numBytes);

      const status_t ret = AppendBytes(tmpBuf, numBytes, allocExtra);
      delete [] tmpBuf;
      return ret;
   }

   const uint32 oldValidBytes = _numValidBytes;  // save this value since SetNumBytes() will change it
   MRETURN_ON_ERROR(SetNumBytesWithExtraSpace(_numValidBytes+numBytes, allocExtra));
   if (bytes != NULL) memcpy(_buffer+oldValidBytes, bytes, numBytes);
   return B_NO_ERROR;
}

status_t ByteBuffer :: SetNumBytesWithExtraSpace(uint32 newNumValidBytes, bool allocExtra)
{
   MRETURN_ON_ERROR(SetNumBytes(((allocExtra)&&(newNumValidBytes > _numAllocatedBytes)) ? muscleMax(newNumValidBytes*4, (uint32)128) : newNumValidBytes, true));
   _numValidBytes = newNumValidBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: FreeExtraBytes()
{
   TCHECKPOINT;

   if (_numValidBytes < _numAllocatedBytes)
   {
      IMemoryAllocationStrategy * as = GetMemoryAllocationStrategy();
      uint8 * newBuf = (uint8 *) (as ? as->Realloc(_buffer, _numValidBytes, _numAllocatedBytes, true) : muscleRealloc(_buffer, _numValidBytes));
      if ((_numValidBytes == 0)||(newBuf))
      {
         _buffer            = newBuf;
         _numAllocatedBytes = _numValidBytes;
      }
      else MRETURN_OUT_OF_MEMORY;
   }
   return B_NO_ERROR;
}

/** Overridden to set our buffer directly from (copyFrom)'s Flatten() method */
status_t ByteBuffer :: CopyFromImplementation(const Flattenable & copyFrom)
{
   const uint32 flatSize = copyFrom.FlattenedSize();
   MRETURN_ON_ERROR(SetNumBytes(flatSize, false));
   copyFrom.FlattenToBytes(_buffer, flatSize);
   return B_NO_ERROR;
}

void ByteBuffer :: Clear(bool releaseBuffers)
{
   if (releaseBuffers)
   {
      IMemoryAllocationStrategy * as = GetMemoryAllocationStrategy();
      if (as) as->Free(_buffer, _numAllocatedBytes); else muscleFree(_buffer);
      _buffer = NULL;
      _numValidBytes = _numAllocatedBytes = 0;
   }
   else (void) SetNumBytes(0, false);
}

void ByteBuffer :: Print(const OutputPrinter & p, uint32 maxBytesToPrint, uint32 numColumns) const
{
   PrintHexBytes(p, GetBuffer(), muscleMin(maxBytesToPrint, GetNumBytes()), "ByteBuffer", numColumns);
}

String ByteBuffer :: ToHexString(uint32 maxBytesToInclude, bool withSpaces) const
{
   return HexBytesToString(GetBuffer(), muscleMin(maxBytesToInclude, GetNumBytes()), withSpaces);
}

String ByteBuffer :: ToAnnotatedHexString(uint32 maxBytesToInclude, uint32 numColumns) const
{
   return HexBytesToAnnotatedString(GetBuffer(), muscleMin(maxBytesToInclude, GetNumBytes()), "ByteBuffer", numColumns);
}

ByteBuffer operator+(const ByteBuffer & lhs, const ByteBuffer & rhs)
{
   ByteBuffer ret;
   if (ret.SetNumBytes(lhs.GetNumBytes()+rhs.GetNumBytes(), false).IsOK())
   {
      memcpy(ret.GetBuffer(), lhs.GetBuffer(), lhs.GetNumBytes());
      memcpy(ret.GetBuffer()+lhs.GetNumBytes(), rhs.GetBuffer(), rhs.GetNumBytes());
   }
   return ret;
}

class ByteBufferPool : public ObjectPool<ByteBuffer>
{
public:
   ByteBufferPool() {/* empty */}

   virtual void RecycleObject(void * obj)
   {
      ByteBuffer * bb = (ByteBuffer *)obj;
      if (bb)
      {
         bb->Clear(true);
         bb->SetMemoryAllocationStrategy(NULL);  // we didn't set a strategy, so if a strategy was set by the calling code, we assume it was meant to be temporary
      }
      ObjectPool<ByteBuffer>::RecycleObject(obj);
   }
};

static ByteBufferPool _bufferPool;
ByteBufferRef::ItemPool * GetByteBufferPool() {return &_bufferPool;}
const ByteBuffer & GetEmptyByteBuffer() {return _bufferPool.GetDefaultObject();}

static const DummyConstByteBufferRef _emptyBufRef(_bufferPool.GetDefaultObject());
const ConstByteBufferRef & GetEmptyByteBufferRef() {return _emptyBufRef;}

ByteBufferRef GetByteBufferFromPool(uint32 numBytes, const uint8 * optBuffer) {return GetByteBufferFromPool(_bufferPool, numBytes, optBuffer);}
ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, uint32 numBytes, const uint8 * optBuffer)
{
   ByteBufferRef ref(pool.ObtainObject());
   if ((ref())&&(ref()->SetBuffer(numBytes, optBuffer).IsError())) ref.Reset();  // return NULL ref on out-of-memory
   return ref;
}

ByteBufferRef GetByteBufferFromPool(SeekableDataIO & dio) {return GetByteBufferFromPool(_bufferPool, dio);}

ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, SeekableDataIO & dio)
{
   const int64 dioLen = dio.GetLength();
   if (dioLen < 0) return ByteBufferRef();  // we don't support reading in unknown lengths of data (for now)

   const int64 pos = muscleMax(dio.GetPosition(), (int64) 0);

   const int64 numBytesToRead = dioLen-pos;
   if (numBytesToRead < 0) return ByteBufferRef();  // wtf?

   const int64 maxBBSize = (int64) ((uint32)-1);  // no point trying to read more data than a ByteBuffer will support anyway
   if (numBytesToRead > maxBBSize) return ByteBufferRef();

   ByteBufferRef ret = GetByteBufferFromPool(pool, (uint32)numBytesToRead);
   if (ret() == NULL) return ByteBufferRef();

   const io_status_t rfRet = dio.ReadFullyUpTo(ret()->GetBuffer(), ret()->GetNumBytes());
   if (rfRet.IsError()) return ByteBufferRef();  // I/O error?

   (void) ret()->SetNumBytes(rfRet.GetByteCount(), true);  // truncate to the size we actually read.  Guaranteed not to fail.
   return ret;
}

// These Flattenable methods are implemented here so that if you don't use them, you
// don't need to include ByteBuffer.o in your Makefile.  If you do use them, then you
// needed to include ByteBuffer.o in your Makefile anyway.

Ref<ByteBuffer> Flattenable :: FlattenToByteBuffer() const
{
   const uint32 flatSize = FlattenedSize();
   ByteBufferRef bufRef = GetByteBufferFromPool(flatSize);
   if (bufRef()) FlattenToBytes(bufRef()->GetBuffer(), flatSize);
   return bufRef;
}

status_t Flattenable :: FlattenToByteBuffer(ByteBuffer & outBuf) const
{
   const uint32 flatSize = FlattenedSize();
   MRETURN_ON_ERROR(outBuf.SetNumBytes(flatSize, false));
   FlattenToBytes(outBuf.GetBuffer(), flatSize);
   return B_NO_ERROR;
}

status_t Flattenable :: UnflattenFromByteBuffer(const ByteBuffer & buf)
{
   return UnflattenFromBytes(buf.GetBuffer(), buf.GetNumBytes());
}

status_t Flattenable :: UnflattenFromByteBuffer(const ConstRef<ByteBuffer> & bufRef)
{
   return bufRef() ? UnflattenFromBytes(bufRef()->GetBuffer(), bufRef()->GetNumBytes()) : B_BAD_ARGUMENT;
}

} // end namespace muscle

