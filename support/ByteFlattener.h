/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteFlattener_h
#define MuscleByteFlattener_h

#include "support/NotCopyable.h"
#include "support/PseudoFlattenable.h"
#include "util/String.h"

namespace muscle {

/** This is a super-lightweight helper class designed to safely and efficiently flatten POD data-values to a raw byte-buffer. */
template<int EndianType> class ByteFlattenerHelper : public NotCopyable
{
public:
   /** Constructs a ByteFlattener that will write up to the specified number of bytes into (writeTo)
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param maxBytes The maximum number of bytes that we are allowed to write.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     */
   ByteFlattenerHelper(uint8 * writeTo, uint32 maxBytes) {SetBuffer(writeTo, maxBytes);}

   /** Destructor. */
   ~ByteFlattenerHelper() {/* empty */}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing ByteFlattenerHelper object)
     * @param writeTo the new buffer to point to and write to in future method-calls.
     * @param maxBytes The new maximum number of bytes that we are allowed to write.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   void SetBuffer(uint8 * writeTo, uint32 maxBytes) {_writeTo = _origWriteTo = writeTo; _maxBytes = _origMaxBytes = maxBytes; _status = B_NO_ERROR;}

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   uint32 GetBuffer() const {return _origWriteTo;}

   /** Returns the number of bytes we have written into our buffer so far */
   uint32 GetNumBytesWritten() const {return (uint32)(_writeTo-_origWriteTo);}

   /** Returns the maximum number of bytes we are allowed to write, as passed in to our constructor (or to SetBuffer()) */
   uint32 GetMaxNumBytes() const {return _origMaxBytes;}

   /** Returns an error code if we've detected any errors while writing data (so far), or B_NO_ERROR if we haven't seen any. */
   status_t GetStatus() const {return _status;}

   /** Writes the specified byte to our buffer.
     * @param theByte The byte to write
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY if no space is available.
     */
   status_t WriteByte(uint8 theByte) {if (_maxBytes < 1) return FlagError(B_OUT_OF_MEMORY); *_writeTo++ = theByte; _maxBytes--; return B_NO_ERROR;}

   /** Writes the specified array of raw bytes into our buffer.
     * @param bytes Pointer to an array of bytes to write into our buffer
     * @param numBytes the number of bytes that (bytes) points to
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure (not enough room available)
     */
   status_t WriteBytes(const uint8 * bytes, uint32 numBytes)
   {
      MRETURN_ON_ERROR(SizeCheck(numBytes));
      memcpy(_writeTo, bytes, numBytes);
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
      MRETURN_ON_ERROR(SizeCheck(numBytes));
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

   status_t WriteInt16s(const int16 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes));

      for (uint32 i=0; i<numVals; i++)
      {
         switch(EndianType)
         {
            case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_INT16(vals[i])); break;
            case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_INT16(vals[i])); break;
            default:                 muscleCopyOut(_writeTo, vals[i]);                          break;
         }
         _writeTo += sizeof(vals[0]);
      }
      _maxBytes -= numBytes;
      return B_NO_ERROR;
   }

   status_t WriteInt32s(const int32 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes));

      for (uint32 i=0; i<numVals; i++)
      {
         switch(EndianType)
         {
            case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_INT32(vals[i])); break;
            case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_INT32(vals[i])); break;
            default:                 muscleCopyOut(_writeTo, vals[i]);                          break;
         }
         _writeTo += sizeof(vals[i]);
      }
      _maxBytes -= numBytes;
      return B_NO_ERROR;
   }

   status_t WriteInt64s(const int64 * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes));

      for (uint32 i=0; i<numVals; i++)
      {
         switch(EndianType)
         {
            case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_INT64(vals[i])); break;
            case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_INT64(vals[i])); break;
            default:                 muscleCopyOut(_writeTo, vals[i]);                          break;
         }
         _writeTo += sizeof(vals[i]);
      }
      _maxBytes -= numBytes;
      return B_NO_ERROR;
   }

   status_t WriteFloats(const float * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes));

      for (uint32 i=0; i<numVals; i++)
      {
         switch(EndianType)
         {
            case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_IFLOAT(vals[i])); break;
            case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_IFLOAT(vals[i])); break;
            default:                 muscleCopyOut(_writeTo, vals[i]);                           break;
         }
         _writeTo += sizeof(vals[i]);
      }
      _maxBytes -= numBytes;
      return B_NO_ERROR;
   }

   status_t WriteDoubles(const double * vals, uint32 numVals)
   {
      const uint32 numBytes = numVals*sizeof(vals[0]);
      MRETURN_ON_ERROR(SizeCheck(numBytes));

      for (uint32 i=0; i<numVals; i++)
      {
         switch(EndianType)
         {
            case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_IDOUBLE(vals[i])); break;
            case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_IDOUBLE(vals[i])); break;
            default:                 muscleCopyOut(_writeTo, vals[i]);                            break;
         }
         _writeTo += sizeof(vals[i]);
      }
      _maxBytes -= numBytes;
      return B_NO_ERROR;
   }

   // Gotta implement this separately since WriteFlats() would write out a 4-byte header, which we don't want
   status_t WriteStrings(const String * vals, uint32 numVals)
   {
      uint32 numBytes = 0;
      for (uint32 i=0; i<numVals; i++) numBytes += vals[i].FlattenedSize();
      MRETURN_ON_ERROR(SizeCheck(numBytes));

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
         MRETURN_ON_ERROR(SizeCheck(numBytes));

         for (uint32 i=0; i<numVals; i++)
         {
            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         _maxBytes -= numBytes;
         return B_NO_ERROR;
      }
      else
      {
         uint32 numBytes = (numVals*sizeof(uint32));  // for the flat-size-prefixes
         for (uint32 i=0; i<numVals; i++) numBytes += vals[i].FlattenedSize();
         MRETURN_ON_ERROR(SizeCheck(numBytes));

         for (uint32 i=0; i<numVals; i++)
         {
            const uint32 flatSize = vals[i].FlattenedSize();
            switch(EndianType)
            {
               case ENDIAN_TYPE_LITTLE: muscleCopyOut(_writeTo, B_HOST_TO_LENDIAN_INT32(flatSize)); break;
               case ENDIAN_TYPE_BIG:    muscleCopyOut(_writeTo, B_HOST_TO_BENDIAN_INT32(flatSize)); break;
               default:                 muscleCopyOut(_writeTo, flatSize);                          break;
            }
            _writeTo += sizeof(flatSize);

            vals[i].Flatten(_writeTo);
            _writeTo += flatSize;
         }
         _maxBytes -= numBytes;
         return B_NO_ERROR;
      }
   }
///@}

private:
   status_t SizeCheck(uint32 numBytes) {return (numBytes <= _maxBytes) ? B_NO_ERROR : FlagError(B_OUT_OF_MEMORY);}
   status_t Advance(  uint32 numBytes) {_writeTo += numBytes; _maxBytes -= numBytes; return B_NO_ERROR;}
   status_t FlagError(status_t ret)    {_status |= ret; return ret;}

   uint8 * _writeTo;      // pointer to our output buffer
   uint8 * _origWriteTo;  // the pointer the user passed in
   uint32 _maxBytes;      // max number of bytes we are allowed to write into our output buffer
   uint32 _origMaxBytes;  // the byte-count the user passed in
   status_t _status;      // cache any errors found so far
};

typedef ByteFlattenerHelper<ENDIAN_TYPE_LITTLE> LittleEndianByteFlattener;  /**< this flattener-type flattens to little-endian-format data */
typedef ByteFlattenerHelper<ENDIAN_TYPE_BIG>    BigEndianByteFlattener;     /**< this flattener-type flattens to big-endian-format data */
typedef ByteFlattenerHelper<ENDIAN_TYPE_NATIVE> NativeEndianByteFlattener;  /**< this flattener-type flattens to native-endian-format data */
typedef LittleEndianByteFlattener               ByteFlattener;              /**< ByteFlattener is a pseudonym for LittleEndianByteFlattener, for convenience */

} // end namespace muscle

#endif
