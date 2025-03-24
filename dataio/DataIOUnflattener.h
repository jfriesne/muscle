/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataIOUnflattener_h
#define MuscleDataIOUnflattener_h

#include "support/EndianConverter.h"
#include "support/PseudoFlattenable.h"
#include "dataio/SeekableDataIO.h"
#include "util/String.h"

namespace muscle {

class ByteBuffer;

/** This is a lightweight helper class designed to safely and efficiently flatten POD data-values to a raw byte-buffer.
  * @tparam EndianConverter the type of EndianConverter object use when converting raw serialized bytes to native data-types
  */
template<class EndianConverter> class MUSCLE_NODISCARD DataIOUnflattenerHelper MUSCLE_FINAL_CLASS
{
public:
   /** Constructs a DataIOUnflattenerHelper to read bytes from a specified DataIO object.
     * @param dataIORef Reference to the DataIO we should use to read in.  The referenced
     *                  DataIO must remain valid for the lifetime of this object.
     */
   DataIOUnflattenerHelper(DataIO & dataIORef) : _endianConverter(), _dataIO(dataIORef), _optSeekableIO(dynamic_cast<SeekableDataIO *>(&dataIORef))
   {
      // empty
   }

   /** Returns the DataIO reference that was passed in to our constructor. */
   MUSCLE_NODISCARD const DataIO & GetDataIO() const {return _dataIO;}

   /** Returns a pointer to our DataIO as a SeekableDataIO, or NULL if our DataIO isn't actually
     * a subclass of SeekableDataIO.
     */
   MUSCLE_NODISCARD const SeekableDataIO * GetSeekableDataIO() const {return _optSeekableIO;}

   /** Reads the specified byte from our DataIO and returns it.
     * @param retByte On success, the read byte is written here
     * @returns B_NO_ERROR on success, or another value if a read-error has occurred.
     */
   status_t ReadByte(uint8 & retByte) {return ReadBytes(&retByte, 1);}

   /** Reads the specified array of raw bytes into our buffer.
     * @param retBytes Pointer to an array of bytes to write results into
     * @param numBytes the number of bytes that (retBytes) has space for
     * @returns B_NO_ERROR on success, or an error value if there was an error while reading.
     */
   status_t ReadBytes(uint8 * retBytes, uint32 numBytes)
   {
      return _status.IsOK() ? FlagError(_dataIO.ReadFully(retBytes, numBytes)) : _status;
   }

///@{
   /** Convenience method for reading and returning the next POD-typed data-item from our buffer.
     * @returns the read value, or 0 on failure (no space available)
     * @note if the call fails, our error-flag will be set true as a side-effect; call
     *       WasParseErrorDetected() to check the error-flag.
     */
   MUSCLE_NODISCARD uint8  ReadByte()   {uint8 v = 0;    (void) ReadBytes(  &v, 1); return v;}
   MUSCLE_NODISCARD int8   ReadInt8()   {int8  v = 0;    (void) ReadInt8s(  &v, 1); return v;}
   MUSCLE_NODISCARD int16  ReadInt16()  {int16 v = 0;    (void) ReadInt16s( &v, 1); return v;}
   MUSCLE_NODISCARD int32  ReadInt32()  {int32 v = 0;    (void) ReadInt32s( &v, 1); return v;}
   MUSCLE_NODISCARD int64  ReadInt64()  {int64 v = 0;    (void) ReadInt64s( &v, 1); return v;}
   MUSCLE_NODISCARD float  ReadFloat()  {float v = 0.0f; (void) ReadFloats( &v, 1); return v;}
   MUSCLE_NODISCARD double ReadDouble() {double v = 0.0; (void) ReadDoubles(&v, 1); return v;}

   /** Reads and returns a primitive of the specified type.
     * @tparam T the type of primitive to return.
     */
   template<typename T> MUSCLE_NODISCARD T ReadPrimitive() {T v = T(); (void) ReadPrimitives(&v, 1); return v;}
///@}

   /** Reads a NUL-terminated char-array from the DataIO and returns it as a String. */
   MUSCLE_NODISCARD String ReadCString()
   {
      String ret;
      while(1)
      {
         uint8 nextChar = 0;
         if ((ReadByte(nextChar).IsError())||(nextChar == '\0')) break;
         ret += (char) nextChar;
      }
      return ret;
   }

   /** Unflattens and returns a Flattenable or PseudoFlattenable object from data in our buffer
     * @tparam T the type of object to return.
     * @param numBytesToRead how many bytes to pass to read in from the DataIO and pass to
     *                       (retVal)'s Unflatten() method.  Note that for some types (e.g.
     *                       String) it is not always obvious how to calculate this value in
     *                       advance!
     * @note errors in unflattening can be detected by calling GetStatus() after this call.
     */
   template<typename T> MUSCLE_NODISCARD T ReadFlat(uint32 numBytesToRead) {T ret; (void) ReadFlat(ret, numBytesToRead); return ret;}

   /** Unflattens and returns a Flattenable or PseudoFlattenable object from data in our buffer
     * without attempting to read any 4-byte length prefix.
     * @param retVal the Flattenable or PseudoFlattenable object to call Unflatten() on.
     * @param numBytesToRead The number of bytes to read into memory and pass to (retVal)'s
     *                 Unflatten() method.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note on success, we will have consumed (numBytesToRead) bytes from the DataIO, regardless
     *       of how many bytes (retVal) actually needed to consume.  This can be a problem for
     *       certain types (like String) whose exact FlattenedSize() isn't easily known in advance.
     */
   template<typename T> status_t ReadFlat(T & retVal, uint32 numBytesToRead)
   {
      uint8 smallBuf[256];
      uint8 * bigBuf = NULL;

      if (numBytesToRead > sizeof(smallBuf))
      {
         bigBuf = newnothrow_array(uint8, numBytesToRead);
         if (bigBuf == NULL) return FlagError(B_OUT_OF_MEMORY);
      }

      DataUnflattenerHelper<EndianConverter> unflat(bigBuf?bigBuf:smallBuf, numBytesToRead);
      const status_t ret = retVal.Unflatten(unflat);
      delete [] bigBuf;

      return FlagError(ret);
   }

   /** Reads a 4-byte length prefix from our buffer, and then passes the next (N) bytes from our
     * buffer to the Unflatten() method of an Flattenable/PseudoFlattenable object of the specified type.
     * @tparam T the type of object to return.
     * @returns the unflattened Flattenable/PseudoFlattenable object, by value.
     * @note errors in unflattening can be detected by calling GetStatus() after this call.
     * @note After this method returns, we will have consumed both the 4-byte length prefix
     *       and the number of bytes it indicates (whether the Unflatten() call succeeded or not)
     */
   template<typename T> MUSCLE_NODISCARD T ReadFlatWithLengthPrefix() {T ret; (void) ReadFlatWithLengthPrefix(ret); return ret;}

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
         for (uint32 i=0; i<numVals; i++) MRETURN_ON_ERROR(ReadFlat(retVals[i], flatSize));
         return B_NO_ERROR;
      }
      else return B_UNIMPLEMENTED;  // it's not obvious how to read the right number of bytes in, so I'm punting on this for now!
   }

   template<typename T> status_t ReadFlatsWithLengthPrefixes(T * retVals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         const uint32 payloadSize = ReadInt32();
         if (_status.IsError()) return _status;

         MRETURN_ON_ERROR(ReadFlat(retVals[i], payloadSize));
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
      uint8 tempBuf[sizeof(T)];
      for (uint32 i=0; i<numVals; i++)
      {
         MRETURN_ON_ERROR(ReadBytes(tempBuf, sizeof(tempBuf)));
         _endianConverter.Import(tempBuf, retVals[i]);
      }
      return B_NO_ERROR;
   }

   /** Convenience method:  seeks past between 0 and (alignmentSize-1) 0-initialized bytes,
     * so that upon successful return our total-bytes-read-count (as returned by our DataIO's
     * GetPosition( method)) will be an even multiple of (alignmentSize).
     * @param alignmentSize the alignment-size we want the data for the next Read*() call to be aligned to.
     * @note (alignmentSize) would typically be something like sizeof(uint32) or sizeof(uint64).
     * @returns B_NO_ERROR on success, or an error code on failure (bad seek position, or DataIO doesn't support seeking?)
     */
   status_t SeekPastPaddingBytesToAlignTo(uint32 alignmentSize)
   {
      if (_optSeekableIO)
      {
         const uint32 modBytes = _optSeekableIO->GetPosition() % alignmentSize;
         if (modBytes == 0) return B_NO_ERROR;  // we're already aligned

         return FlagError(_optSeekableIO->Seek(alignmentSize - modBytes, SeekableDataIO::IO_SEEK_CUR));
      }
      else return FlagError(B_BAD_OBJECT);  // can't seek if we aren't seekable
   }

   /** Returns true iff we have detected any problems reading in data so far */
   status_t GetStatus() const {return _status;}

   /** Resets our status-flag back to B_NO_ERROR.  To be called in case you want to continue after a read-error. */
   void ResetStatus() {_status = status_t();}

private:
   const EndianConverter _endianConverter;

   status_t FlagError(status_t ret) {_status |= ret; return ret;}

   DataIO & _dataIO;                // the reference the user passed in to our constructor
   SeekableDataIO * _optSeekableIO; // only non-NULL if our DataIO is actually a SeekableDataIO
   status_t _status;                // cache any error reported so far
};

typedef DataIOUnflattenerHelper<LittleEndianConverter>  LittleEndianDataIOUnflattener;  /**< this unflattener-type unflattens from little-endian-format data */
typedef DataIOUnflattenerHelper<BigEndianConverter>     BigEndianDataIOUnflattener;     /**< this unflattener-type unflattens from big-endian-format data */
typedef DataIOUnflattenerHelper<NativeEndianConverter>  NativeEndianDataIOUnflattener;  /**< this unflattener-type unflattens from native-endian-format data */
typedef DataIOUnflattenerHelper<DefaultEndianConverter> DataIOUnflattener;              /**< this unflattener-type unflattens from MUSCLE's preferred endian-format (which is little-endian by default) */

} // end namespace muscle

#endif
