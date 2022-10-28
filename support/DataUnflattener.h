/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataUnflattener_h
#define MuscleDataUnflattener_h

#include "support/EndianConverter.h"

namespace muscle {

class ByteBuffer;

/** This is a lightweight helper class designed to safely and efficiently flatten POD data-values to a raw byte-buffer. */
template<class EndianConverter, class SizeChecker=RealSizeChecker> class DataUnflattenerHelper MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before using */
   DataUnflattenerHelper() : _endianConverter(), _sizeChecker() {Reset();}

   /** Constructs a DataUnflattener that will read up to the specified number of bytes
     * @param readFrom The buffer to read bytes from.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param maxBytes The maximum number of bytes that we are allowed to read.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     */
   DataUnflattenerHelper(const uint8 * readFrom, uint32 maxBytes) : _endianConverter(), _sizeChecker() {SetBuffer(readFrom, maxBytes);}

   /** Same as above, except instead of taking a raw pointer as a target, we take a reference to a ByteBuffer object.
     * @param readFrom Reference to a ByteBuffer that we should read data out of.  A pointer to this ByteBuffer's data will be retained for use in future Read*() method-calls.
     * @param maxBytes The maximum number of bytes that we should allow ourselves to read out of (readFrom).  If this value is greater
     *                 than (readFrom.GetNumBytes()) it will treated as equal to (readFrom.GetNumBytes()).  Defaults to MUSCLE_NO_LIMIT.
     * @param startOffset byte-offset indicating where in (readFrom)'s buffer to start reading at.  Defaults to 0.
     */
   DataUnflattenerHelper(const ByteBuffer & readFrom, uint32 maxBytes = MUSCLE_NO_LIMIT, uint32 startOffset = 0) : _endianConverter(), _sizeChecker() {SetBuffer(readFrom, maxBytes, startOffset);}

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero byte-count */
   void Reset() {SetBuffer(NULL, 0);}

   /** Set a new raw array to read from (same as what we do in the constructor, except this updates an existing DataUnflattenerHelper object)
     * @param readFrom the new buffer to point to and read from in future Read*() method-calls.
     * @param maxBytes The new maximum number of bytes that we are allowed to read.  Pass in MUSCLE_NO_LIMIT if you don't want to enforce any maximum.
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   void SetBuffer(const uint8 * readFrom, uint32 maxBytes) {_readFrom = _origReadFrom = readFrom; _maxBytes = maxBytes; _status = B_NO_ERROR;}

   /** Same as above, except instead of taking a raw pointer as a target, we take a reference to a ByteBuffer object.
     * @param readFrom Reference to a ByteBuffer that we should read data out of.  A pointer to this ByteBuffer's data will be retained for use in future Read*() method-calls.
     * @param maxBytes The maximum number of bytes that we should allow ourselves to read out of (readFrom).  If this value is greater
     *                 than (readFrom.GetNumBytes()) it will treated as equal to (readFrom.GetNumBytes()).  Defaults to MUSCLE_NO_LIMIT.
     * @param startOffset byte-offset indicating where in (readFrom)'s buffer to start reading at.  Defaults to 0.
     * @note this method resets our status-flag back to B_NO_ERROR.
     */
   void SetBuffer(const ByteBuffer & readFrom, uint32 maxBytes = MUSCLE_NO_LIMIT, uint32 startOffset = 0);

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   const uint8 * GetBuffer() const {return _origReadFrom;}

   /** Returns the number of bytes we have read from our buffer so far */
   uint32 GetNumBytesRead() const {return (uint32)(_readFrom-_origReadFrom);}

   /** Returns the number of bytes we have remaining to read */
   uint32 GetNumBytesAvailable() const
   {
      if (_maxBytes == MUSCLE_NO_LIMIT) return MUSCLE_NO_LIMIT;
      const uint32 nbr = GetNumBytesRead();
      return (nbr < _maxBytes) ? (_maxBytes-nbr) : 0;
   }

   /** Returns the maximum number of bytes we are allowed to read, as passed in to our constructor (or to SetBuffer()) */
   uint32 GetMaxNumBytes() const {return _maxBytes;}

   /** Returns true iff we have detected any problems reading in data so far */
   status_t GetStatus() const {return _status;}

   /** Reads the specified byte to this DataUnflattenerHelper's contents.
     * @param retByte On success, the read byte is written here
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if no data is available.
     */
   status_t ReadByte(uint8 & retByte) {MRETURN_ON_ERROR(SizeCheck(sizeof(retByte))); retByte = *_readFrom; return Advance(sizeof(retByte));}

   /** Reads the specified array of raw bytes into our buffer.
     * @param retBytes Pointer to an array of bytes to write results into
     * @param numBytes the number of bytes that (retBytes) has space for
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (not enough data available)
     */
   status_t ReadBytes(uint8 * retBytes, uint32 numBytes)
   {
      MRETURN_ON_ERROR(SizeCheck(numBytes));
      memcpy(retBytes, _readFrom, numBytes);
      return Advance(numBytes);
   }

///@{
   /** Convenience method for reading and returning the next POD-typed data-item from our buffer.
     * @returns the read value, or 0 on failure (no space available)
     * @note if the call fails, our error-flag will be set true as a side-effect; call
     *       WasParseErrorDetected() to check the error-flag.
     */
   uint8  ReadByte()   {uint8 v = 0;    (void) ReadBytes(  &v, 1); return v;}
   int8   ReadInt8()   {int8  v = 0;    (void) ReadInt8s(  &v, 1); return v;}
   int16  ReadInt16()  {int16 v = 0;    (void) ReadInt16s( &v, 1); return v;}
   int32  ReadInt32()  {int32 v = 0;    (void) ReadInt32s( &v, 1); return v;}
   int64  ReadInt64()  {int64 v = 0;    (void) ReadInt64s( &v, 1); return v;}
   float  ReadFloat()  {float v = 0.0f; (void) ReadFloats( &v, 1); return v;}
   double ReadDouble() {double v = 0.0; (void) ReadDoubles(&v, 1); return v;}
   template<typename T> T ReadPrimitive() {T v = T(); (void) ReadPrimitives(&v, 1); return v;}
///@}

   /** Returns a pointer to the next NUL-terminated ASCII string inside our buffer, or NULL on failure
     * @note as a side effect, this method advances our internal read-pointer past the returned string
     */
   const char * ReadCString()
   {
      const uint32 nba = GetNumBytesAvailable();
      if (nba == 0) {_status = B_DATA_NOT_FOUND; return NULL;}

      uint32 flatSize;
      if (_maxBytes == MUSCLE_NO_LIMIT) flatSize = (uint32) (strlen(reinterpret_cast<const char *>(_readFrom))+1);
      else
      {
         // Gotta check for unterminated strings, or we won't be safe
         const uint8 * temp = _readFrom;
         const uint8 * firstInvalidByte = _readFrom+nba;
         while((temp < firstInvalidByte)&&(*temp != '\0')) temp++;
         if (temp == firstInvalidByte) {_status |= B_BAD_DATA; return NULL;}  // string wasn't terminated, so we can't return it
         flatSize = (1+((uint32)(temp-_readFrom)));  // +1 to include the NUL byte
      }

      const char * ret = reinterpret_cast<const char *>(_readFrom);
      Advance(flatSize);
      return ret;
   }

   /** Unflattens and returns a Flattenable or PseudoFlattenable object from data in our buffer
     * @param maxNumBytes how many bytes to pass to retVal's Unflatten() method.
     *                 If this value is greater than GetNumBytesAvailable(), it will be treated
     *                 as equal to GetNumBytesAvailable().  Defaults to MUSCLE_NO_LIMIT.
     * @returns the unflattened object, by value.
     * @note errors in unflattening can be detected by calling GetStatus() after this call.
     */
   template<typename T> T ReadFlat(uint32 maxNumBytes = MUSCLE_NO_LIMIT) {T ret; (void) ReadFlat(ret, maxNumBytes); return ret;}

   /** Unflattens and returns a Flattenable or PseudoFlattenable object from data in our buffer
     * without attempting to read any 4-byte length prefix.
     * @param retVal the Flattenable or PseudoFlattenable object to call Unflatten() on.
     * @param maxNumBytes how many bytes to pass to retVal's Unflatten() method.
     *                 If this value is greater than GetNumBytesAvailable(), it will be treated
     *                 as equal to GetNumBytesAvailable().  Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note on success, we will have consumed the number of bytes returned by (retVal.FlattenedSize())
     */
   template<typename T> status_t ReadFlat(T & retVal, uint32 maxNumBytes = MUSCLE_NO_LIMIT)
   {
      DataUnflattenerHelper unflat(_readFrom, muscleMin(maxNumBytes, GetNumBytesAvailable()));
      const status_t ret = retVal.Unflatten(unflat);
      if (ret.IsError()) return FlagError(ret);
      (void) Advance(unflat.GetNumBytesRead());
      return B_NO_ERROR;
   }

   /** Reads a 4-byte length prefix from our buffer, and then passes the next (N) bytes from our
     * buffer to the Unflatten() method of an Flattenable/PseudoFlattenable object of the specified type.
     * @returns the unflattened Flattenable/PseudoFlattenable object, by value.
     * @note errors in unflattening can be detected by calling GetStatus() after this call.
     * @note After this method returns, we will have consumed both the 4-byte length prefix
     *       and the number of bytes it indicates (whether the Unflatten() call succeeded or not)
     */
   template<typename T> T ReadFlatWithLengthPrefix() {T ret; (void) ReadFlatWithLengthPrefix(ret); return ret;}

   /** Reads a 4-byte length-prefix from our buffer, and then passes (that many) bytes from our
     * buffer to the Unflatten() method of the specified Flattenable/PseudoFlattenable object.
     * @param retVal the Flattenable or PseudoFlattenable object to call Unflatten() on.
     * @returns B_NO_ERROR on success, or an error code on failure
     * @note After this method returns, we will have consumed both the 4-byte length prefix
     *       and the number of bytes it indicates (whether the Unflatten() call succeeded or not)
     */
   template<typename T> status_t ReadFlatWithLengthPrefix(T & retVal) {return ReadFlatsWithLengthPrefixes(&retVal, 1);}

///@{
   /** Convenience methods for reading an array of POD-typed data items from our internal buffer.
     * @param retVals Pointer to an array of values to write results to
     * @param numVals the number of values in the value-array that (retVals) points to
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (not enough data available)
     */
   status_t ReadInt8s(   uint8 * retVals, uint32 numVals) {return ReadBytes(retVals, numVals);}
   status_t ReadInt8s(    int8 * retVals, uint32 numVals) {return ReadBytes(reinterpret_cast<uint8 *>(retVals), numVals);}
   status_t ReadInt16s( uint16 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadInt16s(  int16 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadInt32s( uint32 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadInt32s(  int32 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadInt64s( uint64 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadInt64s(  int64 * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadFloats(  float * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}
   status_t ReadDoubles(double * retVals, uint32 numVals) {return ReadPrimitives(retVals, numVals);}

   template<typename T> status_t ReadFlats(T * retVals, uint32 numVals)
   {
      if (numVals == 0) return B_NO_ERROR; // avoid reading from invalid retVals[0] below if the array is zero-length

      if (retVals[0].IsFixedSize())
      {
         const uint32 flatSize = retVals[0].FlattenedSize();
         MRETURN_ON_ERROR(SizeCheck(flatSize*numVals));
         for (uint32 i=0; i<numVals; i++)
         {
            DataUnflattenerHelper unflat(_readFrom, flatSize);
            const status_t ret = retVals[i].Unflatten(unflat);
            if (ret.IsError()) return FlagError(ret);
            if (unflat.GetNumBytesRead() != flatSize) LogTime(MUSCLE_LOG_WARNING, "Unflatten() didn't read the expected number of bytes!  flatSize was " UINT32_FORMAT_SPEC " but Unflatten() read " UINT32_FORMAT_SPEC " bytes\n", flatSize, unflat.GetNumBytesRead());
            (void) Advance(flatSize);
         }
      }
      else
      {
         for (uint32 i=0; i<numVals; i++)
         {
            DataUnflattenerHelper unflat(_readFrom, GetNumBytesAvailable());
            const status_t ret = retVals[i].Unflatten(unflat);
            if (ret.IsError()) return FlagError(ret);
            (void) Advance(unflat.GetNumBytesRead());
         }
      }
      return B_NO_ERROR;
   }

   template<typename T> status_t ReadFlatsWithLengthPrefixes(T * retVals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         MRETURN_ON_ERROR(SizeCheck(sizeof(uint32)));
         uint32 payloadSize; _endianConverter.Import(_readFrom, payloadSize);
         MRETURN_ON_ERROR(SizeCheck(payloadSize));
         Advance(sizeof(payloadSize));

         DataUnflattenerHelper unflat(_readFrom, payloadSize);
         const status_t ret = retVals[i].Unflatten(unflat);
         Advance(payloadSize);  // note that we always advance by the stated payload size, not by (retVal.FlattenedSize())
         if (ret.IsError()) return FlagError(ret);
      }
      return B_NO_ERROR;
   }

///@}

   /** Generic method for reading an array of any of the standard POD-typed data-items
     * (int32, int64, float, double, etc) from our buffer.
     * @param retVals Pointer to an array of values to restore from our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   template <typename T> status_t ReadPrimitives(T * retVals, uint32 numVals)
   {
      MRETURN_ON_ERROR(SizeCheck(numVals*sizeof(T)));
      for (uint32 i=0; i<numVals; i++)
      {
         _endianConverter.Import(_readFrom, retVals[i]);
         _readFrom += sizeof(T);
      }
      return B_NO_ERROR;
   }

   /** Returns a pointer into our buffer at the location we will next read from */
   const uint8 * GetCurrentReadPointer() const {return _readFrom;}

   /** Moves the pointer into our buffer to the specified absolute offset from the beginning of the buffer.
     * @param offset the byte-offset from the top of the buffer to move the read-position to.
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the requested position is out-of-bounds.
     *          (note moving the read-location to one-past-the-last-byte is ok)
     */
   status_t SeekTo(uint32 offset)
   {
      if (offset > _maxBytes) return B_BAD_ARGUMENT;
      _readFrom = _origReadFrom+offset;
      return B_NO_ERROR;
   }

   /** Moves the pointer into our buffer forwards or backwards by the specified number of bytes.
     * @param numBytes the number of bytes to move the pointer by
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the new read-location would be outside
     *          the bounds of our buffer (note moving the read-location to one-past-the-last-byte is ok)
     */
   status_t SeekRelative(int32 numBytes)
   {
      const uint32 nbw = GetNumBytesRead();
      return ((numBytes > 0)||(((uint32)(-numBytes)) <= nbw)) ? SeekTo(GetNumBytesRead()+numBytes) : B_BAD_ARGUMENT;
   }

   /** Moves the read-pointer to the end of our buffer
     * @returns B_NO_ERROR
     */
   status_t SeekToEnd() {return SeekTo(_maxBytes);}

   /** Convenience method:  seeks past between 0 and (alignmentSize-1) 0-initialized bytes,
     * so that upon return our total-bytes-read-count (as returned by GetNumBytesRead())
     * is an even multiple of (alignmentSize).
     * @param alignmentSize the alignment-size we want the data for the next Read*() call to be aligned to.
     * @note (alignmentSize) would typically be something like sizeof(uint32) or sizeof(uint64).
     * @returns B_NO_ERROR on success, or an error code on failure (bad seek position?)
     */
   status_t SeekPastPaddingBytesToAlignTo(uint32 alignmentSize)
   {
      const uint32 modBytes = GetNumBytesRead() % alignmentSize;
      return (modBytes > 0) ? SeekRelative(alignmentSize - modBytes) : B_NO_ERROR;
   }

   /** Sets our maximum-bytes-allowed-to-be-read value to a different value.
     * @param max the new maximum value
     */
   void SetMaxNumBytes(uint32 max) {_maxBytes = max;}

private:
   const EndianConverter _endianConverter;
   const SizeChecker _sizeChecker;

   status_t SizeCheck(uint32 numBytes) {return _sizeChecker.IsSizeOkay(numBytes, GetNumBytesAvailable()) ? B_NO_ERROR : FlagError(B_BAD_DATA);}
   status_t Advance(  uint32 numBytes) {_readFrom += numBytes; return B_NO_ERROR;}
   status_t FlagError(status_t ret)    {_status |= ret; return ret;}

   const uint8 * _readFrom;     // pointer to our input buffer
   const uint8 * _origReadFrom; // the pointer the user passed in
   uint32 _maxBytes;            // the byte-count the user passed in (may be MUSCLE_NO_LIMIT if no limit was defined)
   status_t _status;            // cache any errors found so far
};

typedef DataUnflattenerHelper<LittleEndianConverter>  LittleEndianDataUnflattener;  /**< this unflattener-type unflattens from little-endian-format data */
typedef DataUnflattenerHelper<BigEndianConverter>     BigEndianDataUnflattener;     /**< this unflattener-type unflattens from big-endian-format data */
typedef DataUnflattenerHelper<NativeEndianConverter>  NativeEndianDataUnflattener;  /**< this unflattener-type unflattens from native-endian-format data */
typedef DataUnflattenerHelper<DefaultEndianConverter> DataUnflattener;              /**< this unflattener-type unflattens from MUSCLE's preferred endian-format (which is little-endian by default) */

typedef DataUnflattenerHelper<LittleEndianConverter,  DummySizeChecker> LittleEndianUncheckedDataUnflattener;  /**< this unchecked unflattener-type unflattens from little-endian-format data */
typedef DataUnflattenerHelper<BigEndianConverter,     DummySizeChecker> BigEndianUncheckedDataUnflattener;     /**< this unchecked unflattener-type unflattens from big-endian-format data */
typedef DataUnflattenerHelper<NativeEndianConverter,  DummySizeChecker> NativeEndianUncheckedDataUnflattener;  /**< this unchecked unflattener-type unflattens from native-endian-format data */
typedef DataUnflattenerHelper<DefaultEndianConverter, DummySizeChecker> UncheckedDataUnflattener;              /**< this unchecked unflattener-type unflattens from MUSCLE's preferred endian-format (which is little-endian by default) */

/** This is an RAII-type class for temporary limiting the number of bytes
  * available on an existing DataUnflattener object.
  */
template<class DataUnflattenerType> class DataUnflattenerReadLimiter MUSCLE_FINAL_CLASS
{
public:
   DataUnflattenerReadLimiter(DataUnflattenerType & unflat, uint32 bytesLimit)
      : _unflat(unflat)
      , _oldMaxBytes(unflat.GetMaxNumBytes())
   {
      _unflat.SetMaxNumBytes(_unflat.GetNumBytesRead()+muscleMin(bytesLimit, _unflat.GetNumBytesAvailable()));
   }

   ~DataUnflattenerReadLimiter() {_unflat.SetMaxNumBytes(_oldMaxBytes);}  // pop the stack

private:
   DataUnflattenerType & _unflat;
   const uint32 _oldMaxBytes;
};

} // end namespace muscle

#endif
