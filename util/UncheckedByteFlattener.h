/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleUncheckedByteFlattener_h
#define MuscleUncheckedByteFlattener_h

#include "support/EndianEncoder.h"
#include "support/NotCopyable.h"
#include "support/PseudoFlattenable.h"
#include "util/ByteBuffer.h"
#include "util/String.h"

namespace muscle {

/** This is a version of ByteFlattenerHelper that doesn't do any bounds-checking on the array it writes to.
  * When you use this you should be very careful not to let it write past the end of its array, as doing so
  * will invoke undefined behavior.
  */
template<class EndianEncoder> class UncheckedByteFlattenerHelper : public NotCopyable
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before using */
   UncheckedByteFlattenerHelper() {Reset();}

   /** Constructs a UncheckedByteFlattener that will write up to the specified number of bytes into (writeTo)
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     */
   UncheckedByteFlattenerHelper(uint8 * writeTo) {SetBuffer(writeTo);}

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero byte-count */
   void Reset() {SetBuffer(NULL);}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing UncheckedByteFlattenerHelper object)
     * @param writeTo the new buffer to point to and write to in future Write*() method-calls.
     */
   void SetBuffer(uint8 * writeTo) {_writeTo = _origWriteTo = writeTo;}

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   uint8 * GetBuffer() const {return _origWriteTo;}

   /** Returns the number of bytes we have written into our buffer so far. */
   uint32 GetNumBytesWritten() const {return (uint32)(_writeTo-_origWriteTo);}

   /** Convenience method:  Allocates and returns a ByteBuffer containing a copy of our contents */
   ByteBufferRef GetByteBufferFromPool() const {return muscle::GetByteBufferFromPool(GetNumBytesWritten(), GetBuffer());}

   /** Writes the specified byte to our buffer.
     * @param theByte The byte to write
     * @returns B_NO_ERROR
     */
   status_t WriteByte(uint8 theByte) {return WriteBytes(&theByte, 1);}

   /** Writes the specified array of raw bytes into our buffer.
     * @param optBytes Pointer to an array of bytes to write into our buffer (or NULL to just add undefined bytes for later use)
     * @param numBytes the number of bytes that (bytes) points to
     * @returns B_NO_ERROR
     */
   status_t WriteBytes(const uint8 * optBytes, uint32 numBytes)
   {
      MRETURN_ON_ERROR(WriteBytesAux(optBytes, numBytes));
      return Advance(numBytes);
   }

///@{
   /** Convenience method for writing one POD-typed data-item into our buffer.
     * @param val the value to write to the end of the buffer
     * @returns B_NO_ERROR
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
     * @returns B_NO_ERROR
     */
   status_t WriteCString(const char * str)
   {
      return WriteBytes(reinterpret_cast<const uint8 *>(str), strlen(str)+1);  // +1 for the NUL terminator
   }

   /** Writes the given Flattenable or PseudoFlattenable object into our buffer
     * @param val the Flattenable or PseudoFlattenable object to write
     * @returns B_NO_ERROR
     * @note if val.IsFixedSize() returns false, we'll write a 4-byte length-prefix before
     *       each flattened-object we write.  Otherwise we'll just write the flattened-object
     *       data with no length-prefix, since the object's flattened-size is considered well-known.
     */
   template<typename T> status_t WriteFlat(const T & val) {return WriteFlats<T>(&val, 1);}

   /** Same as WriteFlat(), but this method will never write out a 4-byte length-prefix, even
     * if (val.IsFixedSize()) returns false.  It will be up to the future reader of the serialized
     * bytes to figure out how many bytes correspond to this object, by some other means.
     * @param val the Flattenable or PseudoFlattenable object to write
     * @returns B_NO_ERROR
     */
   template<typename T> status_t WriteFlatWithoutLengthPrefix(const T & val) {return WriteFlatsAux(&val, 1, false);}

///@{
   /** Convenience methods for writing an array of POD-typed data-items to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     * @returns B_NO_ERROR
     */
   status_t WriteInt8s(const int8 * vals, uint32 numVals) {return WriteBytes(reinterpret_cast<const uint8 *>(vals), numVals);}

   status_t WriteInt16s(const uint16 * vals, uint32 numVals) {return WriteInt16s(reinterpret_cast<const int32 *>(vals), numVals);}
   status_t WriteInt16s(const int16 * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt16(vals[i], _writeTo);
         _writeTo += sizeof(vals[0]);
      }
      return B_NO_ERROR;
   }

   status_t WriteInt32s(const uint32 * vals, uint32 numVals) {return WriteInt32s(reinterpret_cast<const int32 *>(vals), numVals);}
   status_t WriteInt32s(const int32 * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt32(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      return B_NO_ERROR;
   }

   status_t WriteInt64s(const uint64 * vals, uint32 numVals) {return WriteInt64s(reinterpret_cast<const int64 *>(vals), numVals);}
   status_t WriteInt64s(const int64 * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportInt64(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      return B_NO_ERROR;
   }

   status_t WriteFloats(const float * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportFloat(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      return B_NO_ERROR;
   }

   status_t WriteDoubles(const double * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.ExportDouble(vals[i], _writeTo);
         _writeTo += sizeof(vals[i]);
      }
      return B_NO_ERROR;
   }

   // Gotta implement this separately since WriteFlats() would write out a 4-byte header, which we don't want
   status_t WriteStrings(const String * vals, uint32 numVals)
   {
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
      return WriteFlatsAux(vals, numVals, (vals[0].IsFixedSize() == false));
   }
///@}

   /** Returns a pointer into our buffer at the location we will next write to */
   uint8 * GetCurrentWritePointer() const {return _writeTo;}

   /** Seeks our "write position" to a new offset within our output buffer.
     * @param offset the new write-position within our output buffer
     * @returns B_NO_ERROR
     */
   status_t SeekTo(uint32 offset) {_writeTo = _origWriteTo + offset; return B_NO_ERROR;}

   /** Moves the pointer into our buffer forwards or backwards by the specified number of bytes.
     * @param numBytes the number of bytes to move the pointer by
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the new write-location would be a negative value
     */
   status_t SeekRelative(int32 numBytes)
   {
      const uint32 bw = GetNumBytesWritten();
      return ((numBytes > 0)||(((uint32)(-numBytes)) <= bw)) ? SeekTo(GetNumBytesWritten()+numBytes) : B_BAD_ARGUMENT;
   }

private:
   const EndianEncoder _encoder;

   status_t WriteBytesAux(const uint8 * optBytes, uint32 numBytes)
   {
      if (optBytes) memcpy(_writeTo, optBytes, numBytes);
      return B_NO_ERROR;
   }

   template<typename T> status_t WriteFlatsAux(const T * vals, uint32 numVals, bool includeLengthPrefix)
   {
      if (includeLengthPrefix)
      {
         for (uint32 i=0; i<numVals; i++)
         {
            const uint32 flatSize = vals[i].FlattenedSize();
            _encoder.ExportInt32(flatSize, _writeTo);
            _writeTo += sizeof(flatSize);

            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         return B_NO_ERROR;
      }
      else
      {
         const uint32 flatSize = vals[0].FlattenedSize();
         for (uint32 i=0; i<numVals; i++)
         {
            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         return B_NO_ERROR;
      }
   }

   status_t Advance(uint32 numBytes) {_writeTo += numBytes; return B_NO_ERROR;}

   uint8 * _writeTo;     // pointer to our output buffer
   uint8 * _origWriteTo; // the pointer the user passed in
};

typedef UncheckedByteFlattenerHelper<LittleEndianEncoder> LittleEndianUncheckedByteFlattener;  /**< this unchecked flattener-type flattens to little-endian-format data */
typedef UncheckedByteFlattenerHelper<BigEndianEncoder>    BigEndianUncheckedByteFlattener;     /**< this unchecked flattener-type flattens to big-endian-format data */
typedef UncheckedByteFlattenerHelper<NativeEndianEncoder> NativeEndianUncheckedByteFlattener;  /**< this unchecked flattener-type flattens to native-endian-format data */
typedef LittleEndianUncheckedByteFlattener                UncheckedByteFlattener;              /**< UncheckedByteFlattener is a pseudonym for LittleEndianUncheckedByteFlattener, for convenience (since MUSCLE standardizes on little-endian encoding) */

} // end namespace muscle

#endif
