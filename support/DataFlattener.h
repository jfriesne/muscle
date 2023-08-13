/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataFlattener_h
#define MuscleDataFlattener_h

#include "support/EndianConverter.h"
#include "support/PseudoFlattenable.h"
#include "util/Queue.h"
#include "util/RefCount.h"

namespace muscle {

class ByteBuffer;

/** This is a lightweight helper class, designed to safely and efficiently flatten POD data-values
  * and/or Flattenable/PseudoFlattenable objects to a fixed-size byte-buffer.
  * @tparam EndianConverter the type of EndianConverter object to use when flattening data-types into raw bytes.
  */
template<class EndianConverter> class MUSCLE_NODISCARD DataFlattenerHelper MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before trying to write data with object! */
   MUSCLE_CONSTEXPR DataFlattenerHelper() : _endianConverter() {Init();}

   /** Constructs a DataFlattenerHelper to write (maxBytes) bytes into the byte-array (writeTo) points to.
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer will still
     *                be valid when the methods on this class are called.
     * @param maxBytes How many bytes of data this DataFlattenerHelper is expected to write.
     * @note failure to write exactly (maxBytes) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   DataFlattenerHelper(uint8 * writeTo, uint32 maxBytes) : _endianConverter() {Init(); SetBuffer(writeTo, maxBytes);}

   /** Constructs a DataFlattenerHelper to write (parent.GetNumBytsAvailable()) bytes into (parentFlat.GetCurrentWritePointer()).
     * @param parentFlat reference to a parent DataFlattenerHelper object.  Our destructor will call
     *                   parentFlat.SeekRelative(GetNumBytesWritten()).
     * @note failure to write exactly (parentFlat.GetNumBytesAvailable()) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   DataFlattenerHelper(const DataFlattenerHelper & parentFlat) : _endianConverter() {Init(); SetBuffer(parentFlat, parentFlat.GetNumBytesAvailable());}

   /** Constructs a DataFlattenerHelper to write (maxBytes) bytes into (parentFlat.GetCurrentWritePointer())
     * @param parentFlat reference to a parent DataFlattenerHelper object.  Our destructor will call
     *                   parentFlat.SeekRelative(GetNumBytesWritten()).
     * @param maxBytes How many bytes of data this DataFlattenerHelper is expected to write.
     *                 This value must not be greater than (parentFlat.GetNumBytesAvailable()).
     * @note failure to write exactly (maxBytes) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   DataFlattenerHelper(const DataFlattenerHelper & parentFlat, uint32 maxBytes) : _endianConverter() {Init(); SetBuffer(parentFlat, maxBytes);}

   /** Constructs a DataFlattenerHelper to write (buf.GetNumBytes()) bytes into (buf).
     * @param buf a ByteBuffer object whose contents we will overwrite
     * @note failure to write exactly (buf.GetNumBytes()) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   DataFlattenerHelper(ByteBuffer & buf);

   /** Convenience constructor:  Sets us to write to the byte-array held by (buf)
     * @param buf a ByteBufferRef object whose contents we will overwrite
     * @note failure to write exactly (buf()->GetNumBytes()) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   DataFlattenerHelper(const Ref<ByteBuffer> & buf);

   /** This destructor will crash the program with a diagnostic log-print, if it detects that we wrote the wrong number of bytes into the buffer.
     * @note Unless -DMUSCLE_AVOID_ASSERTIONS is defined during compilation, this destructor will print an error
     *       message and abort the program unless the number of bytes written was the same as the (maxBytes) value
     *       passed in to our constructor or to SetBuffer(); this is done to make any underwrite/overwrite bugs in
     *       Flatten() methods immediately obvious.  If you deliberately didn't write to all of the bytes in the buffer,
     *       you can avoid a crash by calling MarkWritingComplete() on this object before it is destroyed.
     */
   ~DataFlattenerHelper() {Finalize();}

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero maximum-byte-count */
   void Reset() {SetBuffer(NULL, 0, false);}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing DataFlattenerHelper object)
     * @param writeTo the new buffer to point to and write to in future Write*() method-calls.
     * @param maxBytes How many bytes of writable buffer space (writeTo) is pointing to
     * @note failure to write exactly (buf()->GetNumBytes()) of data will trigger an assertion failure!
     *       if you don't want that, you can avoid it by calling MarkWritingComplete() on this object before destroying it.
     */
   void SetBuffer(uint8 * writeTo, uint32 maxBytes) {Finalize(); _writeTo = _origWriteTo = writeTo; _maxBytes = maxBytes; _parentFlat = NULL;}

   /** Sets us up to write into (parentFlat)'s data array, starting at (parentFlat.GetCurrentWritePointer()).
     * @param parentFlat reference to a parent DataFlattenerHelper object.  Our destructor will call
     *                   parentFlat.SeekRelative(GetNumBytesWritten()).
     * @param maxBytes How many bytes of writable buffer space (writeTo) is pointing to
     */
   void SetBuffer(const DataFlattenerHelper & parentFlat, uint32 maxBytes)
   {
      Finalize();
      _writeTo    = _origWriteTo = parentFlat.GetCurrentWritePointer();
      _maxBytes   = maxBytes;
      _parentFlat = &parentFlat;
      if (_maxBytes > parentFlat.GetNumBytesAvailable())
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "DataFlattenerHelper %p:  SetBuffer() specified more bytes (" UINT32_FORMAT_SPEC ") than the parent DataFlattenerHelper has available (" UINT32_FORMAT_SPEC ")!\n", this, maxBytes, parentFlat.GetNumBytesAvailable());
         MCRASH("DataFlattenerHelper::SetBuffer() detected imminent buffer-write overflow");
      }
   }

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   MUSCLE_NODISCARD uint8 * GetBuffer() const {return _origWriteTo;}

   /** Returns the number of bytes we have written into our buffer so far. */
   MUSCLE_NODISCARD uint32 GetNumBytesWritten() const {return (uint32)(_writeTo-_origWriteTo);}

   /** Returns the number of free bytes that are still remaining to write to */
   MUSCLE_NODISCARD uint32 GetNumBytesAvailable() const
   {
      if (_maxBytes == MUSCLE_NO_LIMIT) return MUSCLE_NO_LIMIT;
      const uint32 nbw = GetNumBytesWritten();
      return (nbw < _maxBytes) ? (_maxBytes-nbw) : 0;
   }

   /** Returns the maximum number of bytes we are allowed to write, as passed in to our constructor (or to SetBuffer()) */
   MUSCLE_NODISCARD uint32 GetMaxNumBytes() const {return _maxBytes;}

   /** Convenience method:  Allocates and returns a ByteBuffer containing a copy of all the bytes we have written so far */
   Ref<ByteBuffer> GetByteBufferFromPool() const;

   /** Writes the specified byte to our buffer.
     * @param theByte The byte to write
     */
   void WriteByte(uint8 theByte) {WriteBytes(&theByte, 1);}

   /** Writes the specified array of raw bytes into our buffer.
     * @param optBytes Pointer to an array of bytes to write into our buffer (or NULL to just add undefined bytes for later use)
     * @param numBytes the number of bytes that (bytes) points to
     */
   void WriteBytes(const uint8 * optBytes, uint32 numBytes)
   {
      if (optBytes) memcpy(_writeTo, optBytes, numBytes);
      Advance(numBytes);
   }

   /** Convenience method; writes out all of the bytes inside (buf)
     * @param buf a ByteBuffer whose bytes we should write into our own buffer.
     */
   void WriteBytes(const ByteBuffer & buf);

///@{
   /** Convenience methods for writing one POD-typed data-item into our buffer.
     * @param val the value to write to the end of the buffer
     */
   template<typename T> void WritePrimitive(const T & val) {WritePrimitives(&val, 1);}
   void WriteInt8(  int8   val) {WriteInt8s(  &val, 1);}
   void WriteInt16( int16  val) {WriteInt16s( &val, 1);}
   void WriteInt32( int32  val) {WriteInt32s( &val, 1);}
   void WriteInt64( int64  val) {WriteInt64s( &val, 1);}
   void WriteFloat( float  val) {WriteFloats( &val, 1);}
   void WriteDouble(double val) {WriteDoubles(&val, 1);}
///@}

   /** Writes the given string (including its NUL-terminator) into our buffer
     * @param str pointer to a NUL-terminated C-string
     */
   void WriteCString(const char * str) {WriteBytes(reinterpret_cast<const uint8 *>(str), (uint32)strlen(str)+1);}  // +1 for the NUL terminator byte

   /** Writes the given Flattenable or PseudoFlattenable object into our buffer
     * @param val the Flattenable or PseudoFlattenable object to write
     */
   template<typename T> void WriteFlat(const T & val) {WriteFlats<T>(&val, 1);}

   /** Convenience method:  writes a 32-bit integer field-size header, followed by the flattened bytes of (val)
     * @param val the Flattenable or PseudoFlattenable object to Flatten() into our byte-buffer.
     */
   template<typename T> void WriteFlatWithLengthPrefix(const T & val) {WriteFlatsWithLengthPrefixes(&val, 1);}

///@{
   /** Convenience methods for writing an array of POD-typed data-items to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     */
   void WriteInt8s(  const  uint8 * vals, uint32 numVals) {WriteBytes(vals, numVals);}
   void WriteInt8s(  const   int8 * vals, uint32 numVals) {WriteInt8s(reinterpret_cast<const uint8 *>(vals), numVals);}
   void WriteInt16s( const uint16 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteInt16s( const  int16 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteInt32s( const uint32 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteInt32s( const  int32 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteInt64s( const uint64 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteInt64s( const  int64 * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteFloats( const  float * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   void WriteDoubles(const double * vals, uint32 numVals) {WritePrimitives(vals, numVals);}
   template<typename T> void WriteFlats(                  const T * vals, uint32 numVals) {return WriteFlatsAux(vals, numVals, false);}
   template<typename T> void WriteFlatsWithLengthPrefixes(const T * vals, uint32 numVals) {return WriteFlatsAux(vals, numVals, true);}
///@}

   /** Generic method for writing an array of any of the standard POD-typed data-items
     * (int32, int64, float, double, etc) to our buffer.
     * @param vals Pointer to an array of values to write into our buffer
     * @param numVals the number of values in the value-array that (vals) points to
     */
   template<typename T> void WritePrimitives(const T * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _endianConverter.Export(vals[i], _writeTo);
         _writeTo += sizeof(T);
      }
   }

   /** Convenience method:  writes between 0 and (alignmentSize-1) 0-initialized bytes,
     * so that upon return our total-bytes-written-count (as returned by GetNumBytesWritten())
     * is an even multiple of (alignmentSize).
     * @param alignmentSize the alignment-size we want the data of the next Write*() call to be aligned to.
     * @note (alignmentSize) would typically be something like sizeof(uint32) or sizeof(uint64).
     */
   void WritePaddingBytesToAlignTo(uint32 alignmentSize)
   {
      const uint32 modBytes = (GetNumBytesWritten() % alignmentSize);
      if (modBytes > 0)
      {
         const uint32 padBytes = alignmentSize-modBytes;
         memset(_writeTo, 0, padBytes);
         WriteBytes(NULL, padBytes);
      }
   }

   /** Returns a pointer into our buffer at the location we will next write to */
   MUSCLE_NODISCARD uint8 * GetCurrentWritePointer() const {return _writeTo;}

   /** Seeks our "write position" to a new offset within our output buffer.
     * @param offset the new write-position within our output buffer
     */
   status_t SeekTo(uint32 offset)
   {
      if ((offset == MUSCLE_NO_LIMIT)||(offset > _maxBytes)) return B_BAD_ARGUMENT;
      _writeTo = _origWriteTo+offset;
      return B_NO_ERROR;
   }

   /** Moves the pointer into our buffer forwards or backwards by the specified number of bytes.
     * @param numBytes the number of bytes to move the pointer by
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the new write-location would be a negative value
     */
   status_t SeekRelative(int32 numBytes)
   {
      const uint32 nbw = GetNumBytesWritten();
      return ((numBytes > 0)||(((uint32)(-numBytes)) <= nbw)) ? SeekTo(GetNumBytesWritten()+numBytes) : B_BAD_ARGUMENT;
   }

   /** Convenience method:  Sets our maximum-number-of-bytes setting equal to the current number of bytes
     * written.  Call this when you've finished writing into an oversized buffer, and you don't want an
     * assertion-failure to be caused by the fact that you didn't write all the way to the end of the buffer.
     */
   void MarkWritingComplete() {const uint32 nbw = GetNumBytesWritten(); if (nbw <= _maxBytes) _maxBytes = nbw;}

private:
   DataFlattenerHelper & operator = (const DataFlattenerHelper &);  // deliberately private and unimplemented

   const EndianConverter _endianConverter;

   template<typename T> void WriteFlatsAux(const T * vals, uint32 numVals, bool includeLengthPrefix)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         const uint32 flatSize = vals[i].FlattenedSize();
         if (includeLengthPrefix) WriteInt32(flatSize);
         vals[i].Flatten(DataFlattenerHelper(*this, flatSize));
      }
   }

   void Init()
   {
      _writeTo    = _origWriteTo = NULL;
      _maxBytes   = 0;
      _parentFlat = NULL;
   }

   void Advance(uint32 numBytes) {_writeTo += numBytes;}

   void Finalize()
   {
      if (_origWriteTo)
      {
         const uint32 nbw = GetNumBytesWritten();
#ifndef MUSCLE_AVOID_ASSERTIONS
         // caller is required to either write all of the bytes he said he would!
         // If you only want to write some of the bytes, be sure to call MarkWritingComplete() before
         // the DataFlattener's destructor executes, to avoid these assertion-failures.
         if ((nbw != _maxBytes)&&(_maxBytes != MUSCLE_NO_LIMIT))
         {
            if (nbw > _maxBytes)
            {
               LogTime(MUSCLE_LOG_CRITICALERROR, "DataFlattenerHelper %p:  " UINT32_FORMAT_SPEC " bytes were written into a buffer that only had space for " UINT32_FORMAT_SPEC " bytes!\n", this, nbw, _maxBytes);
               MCRASH("~DataFlattenerHelper(): detected buffer-write overflow");
            }
            else
            {
               LogTime(MUSCLE_LOG_CRITICALERROR, "DataFlattenerHelper %p:  Only " UINT32_FORMAT_SPEC " bytes were written to a buffer that had space for " UINT32_FORMAT_SPEC " bytes, leaving " UINT32_FORMAT_SPEC " bytes uninitialized!\n", this, nbw, _maxBytes, GetNumBytesAvailable());
               MCRASH("~DataFlattenerHelper(): detected incomplete buffer-write");
            }
         }
#endif
         if (_parentFlat) _parentFlat->_writeTo += nbw;
      }
   }

   mutable uint8 * _writeTo; // pointer to our output buffer
   uint8 * _origWriteTo;     // the pointer the user passed in
   uint32 _maxBytes;         // used for post-hoc detection of underwrite/overwrite errors
   const DataFlattenerHelper * _parentFlat;  // if non-NULL, we will call SeekRelative() on this in our destructor
};

typedef DataFlattenerHelper<LittleEndianConverter>  LittleEndianDataFlattener;  /**< this flattener-type flattens to little-endian-format data */
typedef DataFlattenerHelper<BigEndianConverter>     BigEndianDataFlattener;     /**< this flattener-type flattens to big-endian-format data */
typedef DataFlattenerHelper<NativeEndianConverter>  NativeEndianDataFlattener;  /**< this flattener-type flattens to native-endian-format data */
typedef DataFlattenerHelper<DefaultEndianConverter> DataFlattener;              /**< this flattener-type flattens to MUSCLE's preferred endian-format (which is little-endian by default) */

} // end namespace muscle

#endif

