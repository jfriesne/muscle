/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataIOFlattener_h
#define MuscleDataIOFlattener_h

#include "support/EndianConverter.h"
#include "support/PseudoFlattenable.h"
#include "dataio/SeekableDataIO.h"
#include "dataio/ErrorDataIO.h"
#include "util/RefCount.h"

namespace muscle {

/** This is a lightweight helper class, designed to safely and efficiently flatten POD data-values
  * and/or Flattenable/PseudoFlattenable objects via a DataIO object.
  * @tparam EndianConverter the type of EndianConverter object to use when flattening data-types into raw bytes.
  */
template<class EndianConverter> class MUSCLE_NODISCARD DataIOFlattenerHelper MUSCLE_FINAL_CLASS
{
public:
   /** Constructs a DataIOFlattenerHelper to write bytes using a specified DataIO object.
     * @param optDataIO if non-NULL, we'll write using this DataIO object.  Otherwise our
     *                  Write*() methods will all return B_BAD_OBJECT, unless you call SetDataIO() first.
     * @note ownership of the DataIO object is not transferred to this object.
     */
   DataIOFlattenerHelper(DataIO * optDataIO = NULL) : _endianConverter(), _dataIO(NULL), _safeDataIO(NULL), _seekableDataIO(NULL) {SetDataIO(optDataIO);}

   /** Destructor */
   ~DataIOFlattenerHelper() {/* empty */}

   /** Resets us to our just-default-constructed state, with a NULL DataIO pointer */
   void Reset() {SetDataIO(NULL); _status = status_t();}

   /** Sets us up to write into (parentFlat)'s data array, starting at (parentFlat.GetCurrentWritePointer()).
     * @param dataIO the DataIO object we should use for writing
     * @note ownership of the DataIO object is not transferred to this object.
     */
   void SetDataIO(DataIO * dataIO)
   {
      _dataIO         = dataIO;
      _safeDataIO     = dataIO ? dataIO                                  : &_errorIO;
      _seekableDataIO = dataIO ? dynamic_cast<SeekableDataIO *>(_dataIO) : NULL;
   }

   /** Returns the pointer that was passed in to our constructor (or to SetDataIO()). */
   MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL const DataIO * GetDataIO() const {return _dataIO;}

   /** Returns a pointer to our DataIO as a SeekableDataIO, if our DataIO actually is a SeekableDataIO.
     * Returns NULL otherwise.
     */
   MUSCLE_NODISCARD const SeekableDataIO * GetSeekableDataIO() const {return _seekableDataIO;}

   /** Writes the specified byte to our DataIO.
     * @param theByte The byte to write
     */
   status_t WriteByte(uint8 theByte) {return WriteBytes(&theByte, 1);}

   /** Writes the specified array of raw bytes via our DataIO
     * @param bytesToWrite Pointer to an array of bytes to write via our DataIO
     * @param numBytes the number of bytes that (bytes) points to
     * @returns the status_t returned by the WriteFully() method of our DataIO object,
     *          or our existing status-code (without calling WriteFully()) if we're already in an error-state.
     */
   status_t WriteBytes(const uint8 * bytesToWrite, uint32 numBytes)
   {
      if (_status.IsOK()) _status |= _safeDataIO->WriteFully(bytesToWrite, numBytes);
      return _status;
   }


///@{
   /** Convenience methods for writing one POD-typed data-item using our DataIO.
     * @param val the value to write to the end of the buffer
     */
   template<typename T> status_t WritePrimitive(const T & val) {return WritePrimitives(&val, 1);}
   status_t WriteInt8(  int8   val) {return WriteInt8s(  &val, 1);}
   status_t WriteInt16( int16  val) {return WriteInt16s( &val, 1);}
   status_t WriteInt32( int32  val) {return WriteInt32s( &val, 1);}
   status_t WriteInt64( int64  val) {return WriteInt64s( &val, 1);}
   status_t WriteFloat( float  val) {return WriteFloats( &val, 1);}
   status_t WriteDouble(double val) {return WriteDoubles(&val, 1);}
///@}

   /** Writes the given string (including its NUL-terminator) using our DataIO
     * @param str pointer to a NUL-terminated C-string
     */
   status_t WriteCString(const char * str) {return WriteBytes(reinterpret_cast<const uint8 *>(str), (uint32)strlen(str)+1);}  // +1 for the NUL terminator byte

   /** Writes the given Flattenable or PseudoFlattenable object using our DataIO
     * @param val the Flattenable or PseudoFlattenable object to write
     */
   template<typename T> status_t WriteFlat(const T & val) {return WriteFlats<T>(&val, 1);}

   /** Convenience method:  writes a 32-bit integer field-size header, followed by the flattened bytes of (val)
     * @param val the Flattenable or PseudoFlattenable object to Flatten() using our DataIO.
     */
   template<typename T> status_t WriteFlatWithLengthPrefix(const T & val) {return WriteFlatsWithLengthPrefixes(&val, 1);}

///@{
   /** Convenience methods for writing an array of POD-typed data-items to our DataIO.
     * @param vals Pointer to an array of values to write using our DataIO.
     * @param numVals the number of values in the value-array that (vals) points to
     */
   status_t WriteInt8s(  const  uint8 * vals, uint32 numVals) {return WriteBytes(vals, numVals);}
   status_t WriteInt8s(  const   int8 * vals, uint32 numVals) {return WriteInt8s(reinterpret_cast<const uint8 *>(vals), numVals);}
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
///@}

   /** Generic method for writing an array of any of the standard POD-typed data-items
     * (int32, int64, float, double, etc) to our DataIO.
     * @param vals Pointer to an array of values to write using our DataIO
     * @param numVals the number of values in the value-array that (vals) points to
     */
   template<typename T> status_t WritePrimitives(const T * vals, uint32 numVals)
   {
      uint8 tempBuf[sizeof(T)];
      for (uint32 i=0; i<numVals; i++)
      {
         _endianConverter.Export(vals[i], tempBuf);
         MRETURN_ON_ERROR(WriteBytes(tempBuf, sizeof(T)));
      }
      return B_NO_ERROR;
   }

   /** Convenience method:  writes between 0 and (alignmentSize-1) 0-initialized bytes,
     * so that upon return our total-bytes-written-count (as returned by our DataIO's GetLength() method)
     * is an even multiple of (alignmentSize)
     * @param alignmentSize the alignment-size we want the data of the next Write*() call to be aligned to.
     * @note (alignmentSize) would typically be something like sizeof(uint32) or sizeof(uint64).
     * @returns B_NO_ERROR on success, or another error on failure.  (If our DataIO isn't a SeekableDataIO,
     *          returns B_BAD_OBJECT since we can't calculate alignment without knowing our current seek-position)
     */
   status_t WritePaddingBytesToAlignTo(uint32 alignmentSize)
   {
      if (_seekableDataIO == NULL) return B_BAD_OBJECT;  // can't find out the current position of a non-seekable I/O!

      const uint32 modBytes = (uint32) (_seekableDataIO->GetPosition() % alignmentSize);
      if (modBytes > 0)
      {
         uint8 tempBuf[64]; memset(tempBuf, 0, sizeof(tempBuf));
         uint32 padBytes = alignmentSize-modBytes;
         while(padBytes > 0)
         {
            const uint32 numBytesToWrite = muscleMin(padBytes, (uint32) sizeof(tempBuf));
            MRETURN_ON_ERROR(WriteBytes(padBytes, numBytesToWrite));
            padBytes -= numBytesToWrite;
         }
      }
      return B_NO_ERROR;
   }

   /** If any of our previous method-calls returned an error code, this method returns that error code.
     * That way you only have to check for errors once, at the end, if you prefer.
     */
   status_t GetStatus() const {return _status;}

private:
   const EndianConverter _endianConverter;

   template<typename T> status_t WriteFlatsAux(const T * vals, uint32 numVals, bool includeLengthPrefix)
   {
      uint8 smallBuf[256];
      uint8 * bigBuf    = NULL;
      uint32 bigBufSize = 0;

      for (uint32 i=0; i<numVals; i++)
      {
         const uint32 flatSize = vals[i].FlattenedSize();
         uint8 * bufPtr = smallBuf;

         if (flatSize > sizeof(smallBuf))
         {
            if (flatSize > bigBufSize)
            {
               // demand-allocate more space if necessary
               if (flatSize > bigBufSize) delete [] bigBuf;

               bigBuf = newnothrow_array(uint8, flatSize);
               if (bigBuf == NULL) return B_OUT_OF_MEMORY;
            }

            bufPtr = bigBuf;
         }

         if (includeLengthPrefix)
         {
            const status_t ret = WriteInt32(flatSize);
            if (ret.IsError()) {delete [] bigBuf; return ret;}
         }

         vals[i].Flatten(DataFlattenerHelper<EndianConverter>(bufPtr, flatSize));

         const status_t ret = WriteBytes(bufPtr, flatSize);
         if (ret.IsError()) {delete [] bigBuf; return ret;}
      }

      delete [] bigBuf;
      return B_NO_ERROR;
   }

   ErrorDataIO _errorIO;              // used as a fallback

   DataIO * _dataIO;                  // what the user passed in
   DataIO * _safeDataIO;              // guaranteed never to be NULL
   SeekableDataIO * _seekableDataIO;  // only non-NULL if our DataIO is actually a SeekableDataIO
   status_t _status;                  // cache any error reported so far
};

typedef DataIOFlattenerHelper<LittleEndianConverter>  LittleEndianDataIOFlattener;  /**< this flattener-type flattens to little-endian-format data */
typedef DataIOFlattenerHelper<BigEndianConverter>     BigEndianDataIOFlattener;     /**< this flattener-type flattens to big-endian-format data */
typedef DataIOFlattenerHelper<NativeEndianConverter>  NativeEndianDataIOFlattener;  /**< this flattener-type flattens to native-endian-format data */
typedef DataIOFlattenerHelper<DefaultEndianConverter> DataIOFlattener;              /**< this flattener-type flattens to MUSCLE's preferred endian-format (which is little-endian by default) */

} // end namespace muscle

#endif

