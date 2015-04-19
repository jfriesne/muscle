#include "dataio/DataIO.h"
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
      uint32 numReadableBytes = (uint32)((_buffer+_numValidBytes)-buffer);
      if (numBytes > numReadableBytes)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "ByteBuffer::SetBuffer();  Attempted to read " UINT32_FORMAT_SPEC " bytes off the end of our internal buffer!\n", numBytes-numReadableBytes);
         return B_ERROR;
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
      if (SetNumBytes(numBytes, false) != B_NO_ERROR) return B_ERROR;
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
         if (newBuf)
         {
            _buffer = newBuf;
            _numAllocatedBytes = _numValidBytes = newNumBytes;
         }
         else
         {
            WARN_OUT_OF_MEMORY;
            return B_ERROR;
         }
      }
      else
      {
         uint8 * newBuf = NULL;
         if (newNumBytes > 0)
         {
            newBuf = (uint8 *) (as ? as->Malloc(newNumBytes) : muscleAlloc(newNumBytes));
            if (newBuf == NULL)
            {
               WARN_OUT_OF_MEMORY;
               return B_ERROR;
            }
         }
         if (as) as->Free(_buffer, _numAllocatedBytes); else muscleFree(_buffer);
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
      uint8 * tmpBuf = newnothrow uint8[numBytes];
      if (tmpBuf) memcpy(tmpBuf, bytes, numBytes);
             else {WARN_OUT_OF_MEMORY; return B_ERROR;}
      status_t ret = AppendBytes(tmpBuf, numBytes, allocExtra);
      delete [] tmpBuf;
      return ret;
   }

   uint32 oldValidBytes = _numValidBytes;  // save this value since SetNumBytes() will change it
   if (SetNumBytesWithExtraSpace(_numValidBytes+numBytes, allocExtra) != B_NO_ERROR) return B_ERROR;
   if (bytes != NULL) memcpy(_buffer+oldValidBytes, bytes, numBytes);
   return B_NO_ERROR;
}

status_t ByteBuffer :: SetNumBytesWithExtraSpace(uint32 newNumValidBytes, bool allocExtra)
{
   if (SetNumBytes(((allocExtra)&&(newNumValidBytes > _numAllocatedBytes)) ? muscleMax(newNumValidBytes*4, (uint32)128) : newNumValidBytes, true) == B_NO_ERROR)
   {
      _numValidBytes = newNumValidBytes;
      return B_NO_ERROR;
   }
   else return B_ERROR;
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
      else return B_ERROR;
   }
   return B_NO_ERROR;
}

/** Overridden to set our buffer directly from (copyFrom)'s Flatten() method */
status_t ByteBuffer :: CopyFromImplementation(const Flattenable & copyFrom)
{
   uint32 numBytes = copyFrom.FlattenedSize();
   if (SetNumBytes(numBytes, false) != B_NO_ERROR) return B_ERROR;
   copyFrom.Flatten(_buffer);
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
   else SetNumBytes(0, false);
}

void ByteBuffer :: PrintToStream(uint32 maxBytesToPrint, uint32 numColumns, FILE * optFile) const
{
   PrintHexBytes(GetBuffer(), muscleMin(maxBytesToPrint, GetNumBytes()), "ByteBuffer", numColumns, optFile);
}

ByteBuffer operator+(const ByteBuffer & lhs, const ByteBuffer & rhs)
{
   ByteBuffer ret;
   if (ret.SetNumBytes(lhs.GetNumBytes()+rhs.GetNumBytes(), false) == B_NO_ERROR)
   {
      memcpy(ret.GetBuffer(), lhs.GetBuffer(), lhs.GetNumBytes());
      memcpy(ret.GetBuffer()+lhs.GetNumBytes(), rhs.GetBuffer(), rhs.GetNumBytes());
   }
   return ret;
}

static ByteBufferRef::ItemPool _bufferPool;
ByteBufferRef::ItemPool * GetByteBufferPool() {return &_bufferPool;}
const ByteBuffer & GetEmptyByteBuffer() {return _bufferPool.GetDefaultObject();}

static const ConstByteBufferRef _emptyBufRef(&_bufferPool.GetDefaultObject(), false);
ConstByteBufferRef GetEmptyByteBufferRef() {return _emptyBufRef;}

ByteBufferRef GetByteBufferFromPool(uint32 numBytes, const uint8 * optBuffer) {return GetByteBufferFromPool(_bufferPool, numBytes, optBuffer);}
ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, uint32 numBytes, const uint8 * optBuffer)
{
   ByteBufferRef ref(pool.ObtainObject());
   if ((ref())&&(ref()->SetBuffer(numBytes, optBuffer) != B_NO_ERROR)) ref.Reset();  // return NULL ref on out-of-memory
   return ref;
}

ByteBufferRef GetByteBufferFromPool(DataIO & dio) {return GetByteBufferFromPool(_bufferPool, dio);}

ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, DataIO & dio)
{
   int64 dioLen = dio.GetLength();
   if (dioLen < 0) return ByteBufferRef();  // we don't support reading in unknown lengths of data (for now)

   int64 pos = dio.GetPosition();
   if (pos < 0) pos = 0;

   int64 numBytesToRead = dioLen-pos;
   if (numBytesToRead < 0) return ByteBufferRef();  // wtf?

   int64 maxBBSize = (int64) ((uint32)-1);  // no point trying to read more data than a ByteBuffer will support anyway
   if (numBytesToRead > maxBBSize) return ByteBufferRef();

   ByteBufferRef ret = GetByteBufferFromPool(pool, (uint32)numBytesToRead);
   if (ret() == NULL) return ByteBufferRef();

   // This will truncate the ByteBuffer if we end up reading fewer bytes than we expected to
   ret()->SetNumBytes(dio.ReadFully(ret()->GetBuffer(), ret()->GetNumBytes()), true);
   return ret;
}

// These Flattenable methods are implemented here so that if you don't use them, you
// don't need to include ByteBuffer.o in your Makefile.  If you do use them, then you
// needed to include ByteBuffer.o in your Makefile anyway.

Ref<ByteBuffer> Flattenable :: FlattenToByteBuffer() const
{
   ByteBufferRef bufRef = GetByteBufferFromPool(FlattenedSize());
   if (bufRef()) Flatten(bufRef()->GetBuffer());
   return bufRef;
}

status_t Flattenable :: FlattenToByteBuffer(ByteBuffer & outBuf) const
{
   if (outBuf.SetNumBytes(FlattenedSize(), false) != B_NO_ERROR) return B_ERROR;
   Flatten(outBuf.GetBuffer());
   return B_NO_ERROR;
}

status_t Flattenable :: UnflattenFromByteBuffer(const ByteBuffer & buf)
{
   return Unflatten(buf.GetBuffer(), buf.GetNumBytes());
}

status_t Flattenable :: UnflattenFromByteBuffer(const ConstRef<ByteBuffer> & buf)
{
   return buf() ? Unflatten(buf()->GetBuffer(), buf()->GetNumBytes()) : B_ERROR;
}

uint32 ByteBuffer :: ReadInt8s(int8 * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, GetNumValidBytesAtOffset(readByteOffset));
   memcpy(vals, readAt, numValsToRead);
   readByteOffset += numValsToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadInt16s(int16 * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/sizeof(int16)));
   uint32 numBytesToRead = numValsToRead*sizeof(int16);
   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_SWAP_INT16(muscleCopyIn<int16>(&readAt[i*sizeof(int16)]));
   }
   else memcpy(vals, readAt, numBytesToRead);

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadInt32s(int32 * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/sizeof(int32)));
   uint32 numBytesToRead = numValsToRead*sizeof(int32);
   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_SWAP_INT32(muscleCopyIn<int32>(&readAt[i*sizeof(int32)]));
   }
   else memcpy(vals, readAt, numBytesToRead);

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadInt64s(int64 * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/sizeof(int64)));
   uint32 numBytesToRead = numValsToRead*sizeof(int64);
   if (IsEndianSwapEnabled())
   {
      for (uint64 i=0; i<numValsToRead; i++) vals[i] = B_SWAP_INT64(muscleCopyIn<int64>(&readAt[i*sizeof(int64)]));
   }
   else memcpy(vals, readAt, numBytesToRead);

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadFloats(float * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/sizeof(int32)));
   uint32 numBytesToRead = numValsToRead*sizeof(int32);
   if (IsEndianSwapEnabled())
   {
#if B_HOST_IS_BENDIAN
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&readAt[i*sizeof(int32)]));
#else
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&readAt[i*sizeof(int32)]));
#endif
   }
   else memcpy(vals, readAt, numBytesToRead);

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadDoubles(double * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/sizeof(int64)));
   uint32 numBytesToRead = numValsToRead*sizeof(int64);
   if (IsEndianSwapEnabled())
   {
#if B_HOST_IS_BENDIAN
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_LENDIAN_TO_HOST_IDOUBLE(muscleCopyIn<int64>(&readAt[i*sizeof(int64)]));
#else
      for (uint32 i=0; i<numValsToRead; i++) vals[i] = B_BENDIAN_TO_HOST_IDOUBLE(muscleCopyIn<int64>(&readAt[i*sizeof(int64)]));
#endif
   }
   else memcpy(vals, readAt, numBytesToRead);

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadPoints(Point * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint32 bytesPerPoint = sizeof(int32)*2;
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/bytesPerPoint));
   uint32 numBytesToRead = numValsToRead*bytesPerPoint;
   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numValsToRead; i++)
      {
         const uint8 * rBase = &readAt[i*bytesPerPoint];
#if B_HOST_IS_BENDIAN
         vals[i].Set(B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[0*sizeof(int32)])), B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[1*sizeof(int32)])));
#else
         vals[i].Set(B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[0*sizeof(int32)])), B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[1*sizeof(int32)])));
#endif
      }
   }
   else
   {
      for (uint32 i=0; i<numValsToRead; i++)
      {
         const uint8 * rBase = &readAt[i*bytesPerPoint];
         vals[i].Set(muscleCopyIn<float>(&rBase[0*sizeof(int32)]), muscleCopyIn<float>(&rBase[1*sizeof(int32)]));
      }
   }

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadRects(Rect * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   const uint32 bytesPerRect = sizeof(int32)*4;
   const uint8 * readAt = _buffer+readByteOffset;
   numValsToRead = muscleMin(numValsToRead, (uint32) (GetNumValidBytesAtOffset(readByteOffset)/bytesPerRect));
   uint32 numBytesToRead = numValsToRead*bytesPerRect;
   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numValsToRead; i++)
      {
         const uint8 * rBase = &readAt[i*bytesPerRect];
#if B_HOST_IS_BENDIAN
         vals[i].Set(B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[0*sizeof(int32)])),
                     B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[1*sizeof(int32)])),
                     B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[2*sizeof(int32)])),
                     B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[3*sizeof(int32)])));
#else
         vals[i].Set(B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[0*sizeof(int32)])),
                     B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[1*sizeof(int32)])),
                     B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[2*sizeof(int32)])),
                     B_BENDIAN_TO_HOST_IFLOAT(muscleCopyIn<int32>(&rBase[3*sizeof(int32)])));
#endif
      }
   }
   else
   {
      for (uint32 i=0; i<numValsToRead; i++)
      {
         const uint8 * rBase = &readAt[i*bytesPerRect];
         vals[i].Set(muscleCopyIn<float>(&rBase[0*sizeof(int32)]),
                     muscleCopyIn<float>(&rBase[1*sizeof(int32)]),
                     muscleCopyIn<float>(&rBase[2*sizeof(int32)]),
                     muscleCopyIn<float>(&rBase[3*sizeof(int32)]));
      }
   }

   readByteOffset += numBytesToRead;
   return numValsToRead;
}

uint32 ByteBuffer :: ReadStrings(String * vals, uint32 numValsToRead, uint32 & readByteOffset) const
{
   for (uint32 i=0; i<numValsToRead; i++)
   {
      uint32 numBytesAvailable = GetNumValidBytesAtOffset(readByteOffset);
      if ((numBytesAvailable == 0)||(vals[i].SetCstr((const char *)(_buffer+readByteOffset), numBytesAvailable) != B_NO_ERROR)) return i;
      readByteOffset = muscleMin(readByteOffset+vals[i].Length()+1, _numValidBytes);
   }
   return numValsToRead;
}

status_t ByteBuffer :: WriteInt8s(const int8 * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 newByteSize = muscleMax(_numValidBytes, writeByteOffset+numVals);
   if ((newByteSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newByteSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   memcpy(writeTo, vals, numVals);
   writeByteOffset += numVals;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteInt16s(const int16 * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes     = numVals*sizeof(int16);
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   if (IsEndianSwapEnabled())
   {
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int16)), B_SWAP_INT16(vals[i]));
   }
   else memcpy(writeTo, vals, numBytes);

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteInt32s(const int32 * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes     = numVals*sizeof(int32);
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   if (IsEndianSwapEnabled())
   {
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int32)), B_SWAP_INT32(vals[i]));
   }
   else memcpy(writeTo, vals, numBytes);

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteInt64s(const int64 * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes     = numVals*sizeof(int64);
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   if (IsEndianSwapEnabled())
   {
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int64)), B_SWAP_INT64(vals[i]));
   }
   else memcpy(writeTo, vals, numBytes);

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteFloats(const float * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes     = numVals*sizeof(int32);
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   if (IsEndianSwapEnabled())
   {
#if B_HOST_IS_BENDIAN
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int32)), B_HOST_TO_LENDIAN_IFLOAT(vals[i]));
#else
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int32)), B_HOST_TO_BENDIAN_IFLOAT(vals[i]));
#endif
   }
   else memcpy(writeTo, vals, numBytes);

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteDoubles(const double * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes     = numVals*sizeof(int64);
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;
   if (IsEndianSwapEnabled())
   {
#if B_HOST_IS_BENDIAN
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int64)), B_HOST_TO_LENDIAN_IDOUBLE(vals[i]));
#else
       for (uint32 i=0; i<numVals; i++) muscleCopyOut(writeTo+(i*sizeof(int64)), B_HOST_TO_BENDIAN_IDOUBLE(vals[i]));
#endif
   }
   else memcpy(writeTo, vals, numBytes);

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WritePoints(const Point * vals, uint32 numVals, uint32 & writeByteOffset)
{
   const uint32 bytesPerPoint = sizeof(int32)*2;
   uint32 numBytes     = numVals*bytesPerPoint;
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;

   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numVals; i++)
      {
         uint8 * wBase = &writeTo[i*bytesPerPoint];
#if B_HOST_IS_BENDIAN
         for (uint32 j=0; j<2; j++) muscleCopyOut(wBase+(j*sizeof(int32)), B_HOST_TO_LENDIAN_IFLOAT(vals[i][j]));
#else
         for (uint32 j=0; j<2; j++) muscleCopyOut(wBase+(j*sizeof(int32)), B_HOST_TO_BENDIAN_IFLOAT(vals[i][j]));
#endif
      }
   }
   else
   {
      for (uint32 i=0; i<numVals; i++)
      {
         uint8 * wBase = &writeTo[i*bytesPerPoint];
         for (uint32 j=0; j<2; j++) muscleCopyOut(wBase+(j*sizeof(int32)), vals[i][j]);
      }
   }

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteRects(const Rect * vals, uint32 numVals, uint32 & writeByteOffset)
{
   const uint32 bytesPerRect = sizeof(int32)*4;
   uint32 numBytes     = numVals*bytesPerRect;
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   uint8 * writeTo = _buffer+writeByteOffset;

   if (IsEndianSwapEnabled())
   {
      for (uint32 i=0; i<numVals; i++)
      {
         uint8 * wBase = &writeTo[i*bytesPerRect];
#if B_HOST_IS_BENDIAN
         for (uint32 j=0; j<4; j++) muscleCopyOut(wBase+(j*sizeof(int32)), B_HOST_TO_LENDIAN_IFLOAT(vals[i][j]));
#else
         for (uint32 j=0; j<4; j++) muscleCopyOut(wBase+(j*sizeof(int32)), B_HOST_TO_BENDIAN_IFLOAT(vals[i][j]));
#endif
      }
   }
   else
   {
      for (uint32 i=0; i<numVals; i++)
      {
         uint8 * wBase = &writeTo[i*bytesPerRect];
         for (uint32 j=0; j<4; j++) muscleCopyOut(wBase+(j*sizeof(int32)), vals[i][j]);
      }
   }

   writeByteOffset += numBytes;
   return B_NO_ERROR;
}

status_t ByteBuffer :: WriteStrings(const String * vals, uint32 numVals, uint32 & writeByteOffset)
{
   uint32 numBytes = 0; for (uint32 i=0; i<numVals; i++) numBytes += vals[i].FlattenedSize();
   uint32 newValidSize = muscleMax(_numValidBytes, writeByteOffset+numBytes);
   if ((newValidSize > _numValidBytes)&&(SetNumBytesWithExtraSpace(newValidSize, true) != B_NO_ERROR)) return B_ERROR;

   for (uint32 i=0; i<numVals; i++)
   {
      vals[i].Flatten(&_buffer[writeByteOffset]);
      writeByteOffset += vals[i].FlattenedSize();
   }
   return B_NO_ERROR;
}

}; // end namespace muscle

