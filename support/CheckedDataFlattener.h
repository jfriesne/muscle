/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleCheckedDataFlattener_h
#define MuscleCheckedDataFlattener_h

#include "support/EndianConverter.h"
#include "support/NotCopyable.h"
#include "util/ByteBuffer.h"
#include "util/Queue.h"

namespace muscle {

/** This is an enhanced version of DataFlattenerHelper that does bounds-checking on every method-call to avoid
  * any chance of writing past the end of the output buffer.  It also supports writing to a ByteBuffer
  * and automatically resizing the ByteBuffer's internal byte-array larger as necessary to hold data,
  * so that the data-size doesn't need to be calculated in advance.
  */
template<class EndianConverter> class CheckedDataFlattenerHelper : public NotCopyable
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before using */
   CheckedDataFlattenerHelper() : _endianConverter() {Reset();}

   /** Constructs a CheckedDataFlattener that will write up to the specified number of bytes into (writeTo)
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param maxBytes The maximum number of bytes that we are allowed to write.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     */
   CheckedDataFlattenerHelper(uint8 * writeTo, uint32 maxBytes): _endianConverter()  {SetBuffer(writeTo, maxBytes);}

   /** Same as above, except instead of taking a raw pointer as a target, we take a reference to a ByteBuffer object.
     * This will allow us to grow the size of the ByteBuffer when necessary, up to (maxBytes) long, thereby freeing
     * the calling code from having to pre-calculate the amount of buffer space it will need.
     * @param writeTo Reference to a ByteBuffer that we should append data to the end of.
     *                A pointer to this ByteBuffer will be retained for use in future Write*() method-calls.
     * @param maxBytes The new maximum size that we should allow the ByteBuffer to grow to.
     *                 Defaults to MUSCLE_NO_LIMIT, meaning no maximum size will be enforced.
     * @note data written via a CheckedDataFlattener constructed with this constructor will be appended after any existing
     *       bytes in the ByteBuffer; it won't overwrite them.
     */
   CheckedDataFlattenerHelper(ByteBuffer & writeTo, uint32 maxBytes = MUSCLE_NO_LIMIT) : _endianConverter() {SetBuffer(writeTo, maxBytes);}

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero byte-count */
   void Reset() {SetBuffer(NULL, 0);}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing CheckedDataFlattenerHelper object)
     * @param writeTo the new buffer to point to and write to in future Write*() method-calls.
     * @param maxBytes The new maximum number of bytes that we are allowed to write.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   void SetBuffer(uint8 * writeTo, uint32 maxBytes)
   {
      _writeTo    = _origWriteTo = writeTo;
      _bytesLeft  = _maxBytes    = maxBytes;
      _byteBuffer = NULL;
      _status     = B_NO_ERROR;
   }

   /** Same as above, except instead of taking a raw pointer as a target, we take a reference to a ByteBuffer object.
     * This will allow us to grow the size of the ByteBuffer when necessary, up to (maxBytes) long, thereby freeing
     * the calling code from having to pre-calculate the amount of buffer space it will need.
     * @param writeTo Reference to a ByteBuffer that we should append data to the end of.
     *                A pointer to this ByteBuffer will be retained for use in future Write*() method-calls.
     * @param maxBytes The new maximum size that we should allow the ByteBuffer to grow to.
     *                 Defaults to MUSCLE_NO_LIMIT, meaning no maximum size will be enforced.
     * @note this method resets our status-flag back to B_NO_ERROR.
     * @note data written via a CheckedDataFlattener initialized with this method will be appended after any existing
     *       bytes in the ByteBuffer; it won't overwrite them.
     */
   void SetBuffer(ByteBuffer & writeTo, uint32 maxBytes = MUSCLE_NO_LIMIT)
   {
      const uint32 curBufSize = writeTo.GetNumBytes();

      _writeTo     = writeTo.GetBuffer();
      _origWriteTo = writeTo.GetBuffer();
      _bytesLeft   = (maxBytes>curBufSize) ? (maxBytes-curBufSize) : 0;
      _maxBytes    = maxBytes;
      _byteBuffer  = &writeTo;
      _status      = B_NO_ERROR;
   }

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   uint8 * GetBuffer() const {return _origWriteTo;}

   /** Returns the pointer to a ByteBuffer that was passed in to our constructor (or to SetBuffer())
     * Returns NULL if we aren't currently associated with any ByteBuffer object.
     */
   ByteBuffer * GetByteBuffer() const {return _byteBuffer;}

   /** Returns the number of bytes we have written into our buffer so far
     * @note in the case where a ByteBuffer reference was passed to SetBuffer(), this value
     *       includes any bytes that were already present in the ByteBuffer at the time.
     */
   uint32 GetNumBytesWritten() const {return (uint32)(_writeTo-_origWriteTo);}

   /** Returns the number of free bytes we still have remaining to write to
     * @note in the case where a ByteBuffer reference was passed to SetBuffer(), this value
     *       includes any bytes that we haven't allocated yet, but are permitted to allocate in the future.
     */
   uint32 GetNumBytesAvailable() const {return _bytesLeft;}

   /** Returns the maximum number of bytes we are allowed to write, as passed in to our constructor (or to SetBuffer()) */
   uint32 GetMaxNumBytes() const {return _maxBytes;}

   /** Returns an error code if we've detected any errors while writing data (so far), or B_NO_ERROR if we haven't seen any. */
   status_t GetStatus() const {return _status;}

   /** Convenience method:  Allocates and returns a ByteBuffer containing a copy of our contents */
   Ref<ByteBuffer> GetByteBufferFromPool() const {return muscle::GetByteBufferFromPool(GetNumBytesWritten(), GetBuffer());}

   /** Writes the specified byte to our buffer.
     * @param theByte The byte to write
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteByte(uint8 theByte) {return WriteBytes(&theByte, 1);}

   /** Writes the specified array of raw bytes into our buffer.
     * @param optBytes Pointer to an array of bytes to write into our buffer (or NULL to just add undefined bytes for later use)
     * @param numBytes the number of bytes that (bytes) points to
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteBytes(const uint8 * optBytes, uint32 numBytes)
   {
      MRETURN_ON_ERROR(SizeCheck(numBytes, false));
      MRETURN_ON_ERROR(WriteBytesAux(optBytes, numBytes));
      return Advance(numBytes);
   }

   /** Convenience method; writes out all of the bytes inside (buf)
     * @param buf a ByteBuffer whose bytes we should write out
     * @returns B_NO_ERROR on success, or an erro code on failure.
     */
   status_t WriteBytes(const ByteBuffer & buf) {return WriteBytes(buf.GetBuffer(), buf.GetNumBytes());}

///@{
   /** Convenience method for writing one POD-typed data-item into our buffer.
     * @param val the value to write to the end of the buffer
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note if the call fails, our error-flag will be set true as a side-effect; call
     *       WasWriteErrorDetected() to check the error-flag.
     */
   template<typename T> status_t WritePrimitive(const T & val) {return WritePrimitives(&val, 1);}
   status_t WriteInt8(  int8           val) {return WriteInt8s(  &val, 1);}
   status_t WriteInt16( int16          val) {return WriteInt16s( &val, 1);}
   status_t WriteInt32( int32          val) {return WriteInt32s( &val, 1);}
   status_t WriteInt64( int64          val) {return WriteInt64s( &val, 1);}
   status_t WriteFloat( float          val) {return WriteFloats( &val, 1);}
   status_t WriteDouble(double         val) {return WriteDoubles(&val, 1);}
///@}

   /** Writes the given string (including its NUL-terminator) into our buffer
     * @param str pointer to a NUL-terminated C-string
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteCString(const char * str)
   {
      const uint32 numBytes = (uint32) (strlen(str)+1);  // +1 for the NUL terminator
      MRETURN_ON_ERROR(SizeCheck(numBytes, false));
      return WriteBytes(reinterpret_cast<const uint8 *>(str), numBytes);
   }

   /** Writes the given Flattenable or PseudoFlattenable object into our buffer
     * @param val the Flattenable or PseudoFlattenable object to write
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   template<typename T> status_t WriteFlat(const T & val) {return WriteFlats<T>(&val, 1);}

   /** Convenience method:  writes a 32-bit integer field-size header, followed by the flattened bytes of (val)
     * @param val the Flattenable or PseudoFlattenable object to Flatten() into our byte-buffer.
     */
   template<typename T> status_t WriteFlatWithLengthPrefix(const T & val) {return WriteFlatsWithLengthPrefixes<T>(&val, 1);}

///@{
   /** Convenience methods for writing an array of POD-typed data-items to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteInt8s(  const  uint8 * vals, uint32 numVals) {return WriteBytes(vals, numVals);}
   status_t WriteInt8s(  const   int8 * vals, uint32 numVals) {return WriteInt8s(reinterpret_cast< const uint8 *>(vals), numVals);}
   status_t WriteInt16s( const uint16 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteInt16s( const  int16 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteInt32s( const uint32 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteInt32s( const  int32 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteInt64s( const uint64 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteInt64s( const  int64 * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteFloats( const  float * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   status_t WriteDoubles(const double * vals, uint32 numVals) {return WritePrimitives(vals, numVals);}
   template<typename T> status_t WriteFlats(                  const T * vals, uint32 numVals) {return WriteFlatsAux(vals, numVals, false);}
   template<typename T> status_t WriteFlatsWithLengthPrefixes(const T * vals, uint32 numVals) {return WriteFlatsAux(vals, numVals, true);}

   template<typename T> status_t WriteFlatsAux(const T * vals, uint32 numVals, bool includeLengthPrefix)
   {
      if (numVals == 0) return B_NO_ERROR;  // nothing to do

      uint32 numBytes = includeLengthPrefix ? (numVals*sizeof(uint32)) : 0;
      const uint32 fixedSizeBytes = vals[0].IsFixedSize() ? vals[0].FlattenedSize() : MUSCLE_NO_LIMIT;
      Queue<uint32> flatSizes;  // to store the results of all our FlattenedSize() calls so we can reuse them below
      if (fixedSizeBytes == MUSCLE_NO_LIMIT)
      {
         MRETURN_ON_ERROR(flatSizes.EnsureSize(numVals, true));
         for (uint32 i=0; i<numVals; i++) numBytes += (flatSizes[i] = vals[i].FlattenedSize());
      }
      else numBytes += (numVals*fixedSizeBytes);  // if all vals are guaranteed to be the same size then it's easy to compute

      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         const uint32 flatSize = (fixedSizeBytes == MUSCLE_NO_LIMIT) ? flatSizes[i] : fixedSizeBytes;
         if (includeLengthPrefix)
         {
            _endianConverter.Export(flatSize, _writeTo);
            _writeTo += sizeof(flatSize);
         }
         vals[i].FlattenToBytes(_writeTo, flatSize);
         _writeTo += flatSize;
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }
///@}

   /** Generic method for writing an array of any of the standard POD-typed data-items
     * (int32, int64, float, double, etc) to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   template<typename T> status_t WritePrimitives(const T * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(T);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _endianConverter.Export(vals[i], _writeTo);
         _writeTo += sizeof(T);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }


   /** Returns a pointer into our buffer at the location we will next write to */
   uint8 * GetCurrentWritePointer() const {return _writeTo;}

   /** Seeks our "write position" to a new offset within our output buffer.
     * @param offset the new write-position within our output buffer
     * @returns B_NO_ERROR on success, or an error code on failure (e.g. B_BAD_ARGUMENT if (offset) is greater than our maximum-bytes value)
     * @note if we are currently associated with a ByteBuffer object, this method will call SetNumBytes()
     *       on it, invalidating any bytes in the ByteBuffer that are located at or after (offset)
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   status_t SeekTo(uint32 offset)
   {
      if (offset > _maxBytes) return B_BAD_ARGUMENT;
      if (_byteBuffer) MRETURN_ON_ERROR(_byteBuffer->SetNumBytes(offset, true));

      _writeTo   = (_byteBuffer ? _byteBuffer->GetBuffer() : _origWriteTo)+offset;
      _bytesLeft = (_maxBytes == MUSCLE_NO_LIMIT) ? MUSCLE_NO_LIMIT : (_maxBytes-offset);
      _status    = B_NO_ERROR;

      return B_NO_ERROR;
   }

   /** Moves the pointer into our buffer forwards or backwards by the specified number of bytes.
     * @param numBytes the number of bytes to move the pointer by
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the new write-location would be outside
     *          the bounds of our buffer (note moving the write-location to one-past-the-last-byte is ok)
     * @note if we are currently associated with a ByteBuffer object, this method will call SetNumBytes()
     *       on it, invalidating any bytes in the ByteBuffer that are located at or after (offset)
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   status_t SeekRelative(int32 numBytes)
   {
      const uint32 nbw = GetNumBytesWritten();
      return ((numBytes > 0)||(((uint32)(-numBytes)) <= nbw)) ? SeekTo(GetNumBytesWritten()+numBytes) : B_BAD_ARGUMENT;
   }

   /** Moves the pointer to the end of our buffer
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT on failure because we don't know how big our buffer is
     */
   status_t SeekToEnd() {return SeekTo(_maxBytes);}

private:
   const EndianConverter _endianConverter;

   void ReduceBytesLeftBy(uint32 numBytes) {if (_bytesLeft != MUSCLE_NO_LIMIT) _bytesLeft -= numBytes;}

   status_t SizeCheck(uint32 numBytes, bool okayToExpandByteBuffer)
   {
      if (numBytes > _bytesLeft)
      {
         // Log about this, because attempting to write past the end of a non-dynamic buffer is almost certainly a program-bug
         LogTime(MUSCLE_LOG_CRITICALERROR, "CheckedDataFlattener::SizeCheck() failed: wanted to write " UINT32_FORMAT_SPEC " bytes to a fixed-size buffer, but only " UINT32_FORMAT_SPEC " bytes are available to write to!\n", numBytes, _bytesLeft);
         return FlagError(B_LOGIC_ERROR);
      }
      if ((okayToExpandByteBuffer)&&(_byteBuffer))
      {
         const uint32 numBytesWritten   = GetNumBytesWritten();
         const uint32 bufLen            = _byteBuffer->GetNumBytes();
         const uint32 numBytesAvailable = (bufLen > numBytesWritten) ? (bufLen - numBytesWritten) : 0;
         if (numBytesAvailable < numBytes) return FlagError(WriteBytesAux(NULL, numBytes));
      }
      return B_NO_ERROR;
   }

   status_t WriteBytesAux(const uint8 * optBytes, uint32 numBytes)
   {
      if (_byteBuffer)
      {
         const uint8 * oldPtr   = _byteBuffer->GetBuffer();
         const uint32 oldOffset = oldPtr ? (uint32)(_writeTo-oldPtr) : 0;

         MRETURN_ON_ERROR(_byteBuffer->AppendBytes(optBytes, numBytes));

         // Recalculate our pointers, in case ByteBuffer::AppendBytes() caused a buffer-reallocation
         _origWriteTo = _byteBuffer->GetBuffer();
         _writeTo     = _origWriteTo+oldOffset;
      }
      else if (optBytes) memcpy(_writeTo, optBytes, numBytes);

      return B_NO_ERROR;
   }

   status_t Advance(  uint32 numBytes) {_writeTo += numBytes; ReduceBytesLeftBy(numBytes); return B_NO_ERROR;}
   status_t FlagError(status_t ret)    {_status |= ret; return ret;}

   ByteBuffer * _byteBuffer;  // if non-NULL, pointer to a ByteBuffer to use instead of (_writeTo) and (_origWriteTo)

   uint8 * _writeTo;     // pointer to our output buffer
   uint8 * _origWriteTo; // the pointer the user passed in
   uint32 _bytesLeft;    // max number of bytes we are allowed to write into our output buffer
   uint32 _maxBytes;     // the byte-count the user passed in
   status_t _status;     // cache any errors found so far
};

typedef CheckedDataFlattenerHelper<LittleEndianConverter>  CheckedLittleEndianDataFlattener;  /**< this checked-flattener-type flattens to little-endian-format data */
typedef CheckedDataFlattenerHelper<BigEndianConverter>     CheckedBigEndianDataFlattener;     /**< this checked-flattener-type flattens to big-endian-format data */
typedef CheckedDataFlattenerHelper<NativeEndianConverter>  CheckedNativeEndianDataFlattener;  /**< this checked-flattener-type flattens to native-endian-format data */
typedef CheckedDataFlattenerHelper<DefaultEndianConverter> CheckedDataFlattener;              /**< this checked-flattener-type flattens to MUSCLE's preferred endian-format (which is little-endian by default) */

} // end namespace muscle

#endif

