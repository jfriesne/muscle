/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataFlattener_h
#define MuscleDataFlattener_h

#include "support/EndianEncoder.h"
#include "support/PseudoFlattenable.h"
#include "util/Queue.h"
#include "util/RefCount.h"

namespace muscle {

class ByteBuffer;

/** This is a lightweight helper class designed to safely and efficiently flatten POD data-values to a fixed-size byte-buffer. */
template<class EndianEncoder> class DataFlattenerHelper MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  Create an invalid object.  Call SetBuffer() before using */
   DataFlattenerHelper() {Reset();}

   /** Constructs a DataFlattener that will write up to the specified number of bytes into (writeTo)
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param maxBytes How many bytes of writable buffer space (writeTo) is pointing to
     */
   DataFlattenerHelper(uint8 * writeTo, uint32 maxBytes) {SetBuffer(writeTo, maxBytes);}

   /** Convenience constructor:  Sets us to write to the byte-array held by (buf)
     * @param buf a ByteBuffer object whose contents we will overwrite
     */
   DataFlattenerHelper(ByteBuffer & buf);

   /** Convenience constructor:  Sets us to write to the byte-array held by (buf)
     * @param buf a ByteBufferRef object whose contents we will overwrite
     */
   DataFlattenerHelper(const Ref<ByteBuffer> & buf);

   /** This destructor will crash the program with a diagnostic log-print, if it detects that we wrote past the end of our buffer.
     * @note Unless -DMUSCLE_AVOID_ASSERTIONS is defined during compilation, this destructor will print an error
     *       message and abort the program unless the number of bytes written was the same as the (maxBytes) value
     *       passed in to our constructor or to SetBuffer(); this is done to make underwrite/overwrite bugs in the Flatten()
     *       methods immediately obvious.  If you deliberately didn't write to all of the bytes in the buffer, you can
     *       avoid a crash by calling SeekToEnd() on this object before it is destroyed.
     */
   ~DataFlattenerHelper();

   /** Resets us to our just-default-constructed state, with a NULL array-pointer and a zero maximum-byte-count */
   void Reset() {SetBuffer(NULL, 0, false);}

   /** Set a new raw array to write to (same as what we do in the constructor, except this updates an existing DataFlattenerHelper object)
     * @param writeTo the new buffer to point to and write to in future Write*() method-calls.
     * @param maxBytes How many bytes of writable buffer space (writeTo) is pointing to
     */
   void SetBuffer(uint8 * writeTo, uint32 maxBytes) {_writeTo = _origWriteTo = writeTo; _maxBytes = maxBytes;;}

   /** Returns the pointer that was passed in to our constructor (or to SetBuffer()) */
   uint8 * GetBuffer() const {return _origWriteTo;}

   /** Returns the number of bytes we have written into our buffer so far. */
   uint32 GetNumBytesWritten() const {return (uint32)(_writeTo-_origWriteTo);}

   /** Returns the number of free bytes that are still remaining to write to */
   uint32 GetNumBytesAvailable() const
   {
      if (_maxBytes == MUSCLE_NO_LIMIT) return MUSCLE_NO_LIMIT;
      const uint32 nbw = GetNumBytesWritten();
      return (nbw < _maxBytes) ? (_maxBytes-nbw) : 0;
   }

   /** Returns the maximum number of bytes we are allowed to write, as passed in to our constructor (or to SetBuffer()) */
   uint32 GetMaxNumBytes() const {return _maxBytes;}

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
      WriteBytesAux(optBytes, numBytes);
      Advance(numBytes);
   }

   /** Convenience method; writes out all of the bytes inside (buf)
     * @param buf a ByteBuffer whose bytes we should write out
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
   void WriteCString(const char * str) {WriteBytes(reinterpret_cast<const uint8 *>(str), strlen(str)+1);}  // +1 for the NUL terminator byte

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
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   template<typename T> void WritePrimitives(const T * vals, uint32 numVals)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         _encoder.Export(vals[i], _writeTo);
         _writeTo += sizeof(T);
      }
   }

   /** Returns a pointer into our buffer at the location we will next write to */
   uint8 * GetCurrentWritePointer() const {return _writeTo;}

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

   /** Moves the pointer to the end of our buffer
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT on failure because we don't know how big our buffer is
     */
   status_t SeekToEnd() {return SeekTo(_maxBytes);}

private:
   const EndianEncoder _encoder;

   void WriteBytesAux(const uint8 * optBytes, uint32 numBytes)
   {
      if (optBytes) memcpy(_writeTo, optBytes, numBytes);
   }

   template<typename T> void WriteFlatsAux(const T * vals, uint32 numVals, bool includeLengthPrefix)
   {
      for (uint32 i=0; i<numVals; i++)
      {
         const uint32 flatSize = vals[i].FlattenedSize();
         if (includeLengthPrefix) WriteInt32(flatSize);
         vals[i].Flatten(DataFlattenerHelper(_writeTo, flatSize));
         Advance(flatSize);
      }
   }

   void Advance(uint32 numBytes) {_writeTo += numBytes;}

   uint8 * _writeTo;     // pointer to our output buffer
   uint8 * _origWriteTo; // the pointer the user passed in
   uint32 _maxBytes;     // used for post-hoc detection of underwrite/overwrite errors
};

template<class EndianEncoder> DataFlattenerHelper<EndianEncoder> :: ~DataFlattenerHelper()
{
#ifndef MUSCLE_AVOID_ASSERTIONS
   if (_maxBytes != MUSCLE_NO_LIMIT)
   {
      const uint32 nbw = GetNumBytesWritten();
      if ((nbw != 0)&&(nbw != _maxBytes))
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
   }
#endif
}

typedef DataFlattenerHelper<LittleEndianEncoder> LittleEndianDataFlattener;  /**< this flattener-type flattens to little-endian-format data */
typedef DataFlattenerHelper<BigEndianEncoder>    BigEndianDataFlattener;     /**< this flattener-type flattens to big-endian-format data */
typedef DataFlattenerHelper<NativeEndianEncoder> NativeEndianDataFlattener;  /**< this flattener-type flattens to native-endian-format data */
typedef LittleEndianDataFlattener                DataFlattener;              /**< DataFlattener is a pseudonym for LittleEndianDataFlattener, for convenience (since MUSCLE standardizes on little-endian encoding) */

} // end namespace muscle

#endif

