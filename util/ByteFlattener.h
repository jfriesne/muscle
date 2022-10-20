/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteFlattener_h
#define MuscleByteFlattener_h

#include "support/EndianEncoder.h"
#include "support/NotCopyable.h"
#include "support/PseudoFlattenable.h"
#include "util/ByteBuffer.h"
#include "util/String.h"

namespace muscle {

/** This is a lightweight helper class designed to safely and efficiently flatten POD data-values to a raw byte-buffer. */
template<class EndianEncoder> class ByteFlattenerHelper : public NotCopyable
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before using */
   ByteFlattenerHelper() {Reset();}

   /** Constructs a ByteFlattener that will write up to the specified number of bytes into (writeTo)
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param maxBytes The maximum number of bytes that we are allowed to write.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     */
   ByteFlattenerHelper(uint8 * writeTo, uint32 maxBytes) {SetBuffer(writeTo, maxBytes);}

   /** Same as above, except instead of taking a raw pointer as a target, we take a reference to a ByteBuffer object.
     * This will allow us to grow the size of the ByteBuffer when necessary, up to (maxBytes) long, thereby freeing
     * the calling code from having to pre-calculate the amount of buffer space it will need.
     * @param writeTo Reference to a ByteBuffer that we should append data to the end of.
     *                A pointer to this ByteBuffer will be retained for use in future Write*() method-calls.
     * @param maxBytes The new maximum size that we should allow the ByteBuffer to grow to.
     *                 Defaults to MUSCLE_NO_LIMIT, meaning no maximum size will be enforced.
     * @note data written via a ByteFlattener constructed with this constructor will be appended after any existing
     *       bytes in the ByteBuffer; it won't overwrite them.
     */
   ByteFlattenerHelper(ByteBuffer & writeTo, uint32 maxBytes = MUSCLE_NO_LIMIT) {SetBuffer(writeTo, maxBytes);}

   /** Destructor. */
   ~ByteFlattenerHelper() {/* empty */}

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero byte-count */
   void Reset() {SetBuffer(NULL, 0);}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing ByteFlattenerHelper object)
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
     * @note data written via a ByteFlattener initialized with this method will be appended after any existing
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
   ByteBufferRef GetByteBufferFromPool() const {return muscle::GetByteBufferFromPool(GetNumBytesWritten(), GetBuffer());}

   /** Writes the specified byte to our buffer.
     * @param theByte The byte to write
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY if no space is available.
     */
   status_t WriteByte(uint8 theByte) {return WriteBytes(&theByte, 1);}

   /** Writes the specified array of raw bytes into our buffer.
     * @param optBytes Pointer to an array of bytes to write into our buffer (or NULL to just add undefined bytes for later use)
     * @param numBytes the number of bytes that (bytes) points to
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     */
   status_t WriteBytes(const uint8 * optBytes, uint32 numBytes)
   {
      MRETURN_ON_ERROR(SizeCheck(numBytes, false));
      MRETURN_ON_ERROR(WriteBytesAux(optBytes, numBytes));
      return Advance(numBytes);
   }

///@{
   /** Convenience method for writing one POD-typed data-item into our buffer.
     * @param val the value to write to the end of the buffer
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     * @note if the call fails, our error-flag will be set true as a side-effect; call
     *       WasWriteErrorDetected() to check the error-flag.
     */
   status_t WriteInt8(  int8           val) {return WriteInt8s(  &val, 1);}
   status_t WriteInt16( int16          val) {return WriteInt16s( &val, 1);}
   status_t WriteInt32( int32          val) {return WriteInt32s( &val, 1);}
   status_t WriteInt64( int64          val) {return WriteInt64s( &val, 1);}
   status_t WriteFloat( float          val) {return WriteFloats( &val, 1);}
   status_t WriteDouble(double         val) {return WriteDoubles(&val, 1);}
   status_t WriteString(const String & val) {return WriteStrings(&val, 1);}
///@}

   /** Writes the given string (including its NUL-terminator) into our buffer
     * @param str pointer to a NUL-terminated C-string
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     */
   status_t WriteCString(const char * str)
   {
      const uint32 numBytes = strlen(str)+1;  // +1 for the NUL terminator
      MRETURN_ON_ERROR(SizeCheck(numBytes, false));
      return WriteBytes(reinterpret_cast<const uint8 *>(str), numBytes);
   }

   /** Writes the given Flattenable or PseudoFlattenable object into our buffer
     * @param val the Flattenable or PseudoFlattenable object to write
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     * @note if val.IsFixedSize() returns false, we'll write a 4-byte length-prefix before
     *       each flattened-object we write.  Otherwise we'll just write the flattened-object
     *       data with no length-prefix, since the object's flattened-size is considered well-known.
     */
   template<typename T> status_t WriteFlat(const T & val) {return WriteFlats<T>(&val, 1);}

///@{
   /** Convenience methods for writing an array of POD-typed data-items to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     */
   status_t WriteInt8s(const int8 * vals, uint32 numVals) {return WriteBytes(reinterpret_cast<const uint8 *>(vals), numVals);}

   status_t WriteInt16s(const uint16 * vals, uint32 numVals) {return WriteInt16s(reinterpret_cast<const int32 *>(vals), numVals);}
   status_t WriteInt16s(const int16 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt16(vals[i], _writeTo);
         _writeTo += sizeof(vals[0]);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }

   status_t WriteInt32s(const uint32 * vals, uint32 numVals) {return WriteInt32s(reinterpret_cast<const int32 *>(vals), numVals);}
   status_t WriteInt32s(const int32 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt32(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }

   status_t WriteInt64s(const uint64 * vals, uint32 numVals) {return WriteInt64s(reinterpret_cast<const int64 *>(vals), numVals);}
   status_t WriteInt64s(const int64 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt64(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }

   status_t WriteFloats(const float * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportFloat(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }

   status_t WriteDoubles(const double * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes, true));

      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportDouble(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      ReduceBytesLeftBy(numBytes);
      return B_NO_ERROR;
   }

   // Gotta implement this separately since WriteFlats() would write out a 4-byte header, which we don't want
   status_t WriteStrings(const String * vals, uint32 numVals)
   {
      uint32 numBytes = 0;
      for (uint32 i=0; i<numVals; i++) numBytes += vals[i].FlattenedSize();
      MRETURN_ON_ERROR(SizeCheck(numBytes, false));

      for (uint32 i=0; i<numVals; i++)
      {
         const String & s = vals[i];
         (void) WriteBytes(reinterpret_cast<const uint8 *>(s()), s.FlattenedSize());  // guaranteed not to fail!
      }
      return B_NO_ERROR;
   }

   template<typename T> status_t WriteFlats(const T * vals, uint32 numVals)
   {
      if (numVals == 0) return B_NO_ERROR; // avoid reading from invalid vals[0] below if the array is zero-length

      if (vals[0].IsFixedSize())
      {
         const uint32 flatSize = vals[0].FlattenedSize();
         const uint32 numBytes = flatSize*numVals;
         MRETURN_ON_ERROR(SizeCheck(numBytes, true));

         for (uint32 i=0; i<numVals; i++)
         {
            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         ReduceBytesLeftBy(numBytes);
         return B_NO_ERROR;
      }
      else
      {
         uint32 numBytes = (numVals*sizeof(uint32));  // for the flat-size-prefixes
         for (uint32 i=0; i<numVals; i++) numBytes += vals[i].FlattenedSize();
         MRETURN_ON_ERROR(SizeCheck(numBytes, true));

         for (uint32 i=0; i<numVals; i++)
         {
            const uint32 flatSize = vals[i].FlattenedSize();
            _encoder.ExportInt32(flatSize, _writeTo);
            _writeTo += sizeof(flatSize);

            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         ReduceBytesLeftBy(numBytes);
         return B_NO_ERROR;
      }
   }
///@}

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
   status_t SeekRelative(int32 numBytes) {return SeekTo(GetNumBytesWritten()+numBytes);}

private:
   const EndianEncoder _encoder;

   void ReduceBytesLeftBy(uint32 numBytes) {if (_bytesLeft != MUSCLE_NO_LIMIT) _bytesLeft -= numBytes;}

   status_t SizeCheck(uint32 numBytes, bool okayToExpandByteBuffer)
   {
      if (numBytes > _bytesLeft)
      {
         // Log about this, because attempting to write past the end of a non-dynamic buffer is almost certainly a program-bug
         LogTime(MUSCLE_LOG_CRITICALERROR, "ByteFlattener::SizeCheck() failed: wanted to write " UINT32_FORMAT_SPEC " bytes to a fixed-size buffer, but only " UINT32_FORMAT_SPEC " bytes are available to write to!\n", numBytes, _bytesLeft);
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
         const uint32 oldOffset = oldPtr ? (_writeTo-oldPtr) : 0;

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

typedef ByteFlattenerHelper<LittleEndianEncoder> LittleEndianByteFlattener;  /**< this flattener-type flattens to little-endian-format data */
typedef ByteFlattenerHelper<BigEndianEncoder>    BigEndianByteFlattener;     /**< this flattener-type flattens to big-endian-format data */
typedef ByteFlattenerHelper<NativeEndianEncoder> NativeEndianByteFlattener;  /**< this flattener-type flattens to native-endian-format data */
typedef LittleEndianByteFlattener                ByteFlattener;              /**< ByteFlattener is a pseudonym for LittleEndianByteFlattener, for convenience (since MUSCLE standardizes on little-endian encoding) */

} // end namespace muscle

#endif
