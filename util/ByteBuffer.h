/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteBuffer_h
#define MuscleByteBuffer_h

#include "util/FlatCountable.h"
#include "support/BitChord.h"
#include "support/Point.h"
#include "support/Rect.h"
#include "util/String.h"

namespace muscle {

class SeekableDataIO;
class IMemoryAllocationStrategy;

/** These flags are used as hints regarding what endian-ness a ByteBuffer should store its multibyte data in */
enum {
   ENDIAN_FLAG_FORCE_LITTLE, /**< specifies that the data to be read (or written) is little-endian */
   ENDIAN_FLAG_FORCE_BIG,    /**< specifies that the data to be read (or written) is big-endian */
   NUM_ENDIAN_FLAGS          /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(EndianFlags, NUM_ENDIAN_FLAGS);

#ifndef DEFAULT_BYTEBUFFER_ENDIAN_FLAGS
/** Specified which ENDIAN_FLAG_* value should be used by default by a ByteBuffer object, when doing in-buffer serialization/deserialization operations.  Default value is ENDIAN_FLAG_FORCE_LITTLE (i.e. Intel-native data endian-ness), but this can be overridden on the compiler line via -DDEFAULT_BYTEBUFFER_ENDIAN_FLAGS=ENDIAN_FLAG_FORCE_BIG or -DDEFAULT_BYTEBUFFER_ENDIAN_FLAGS (if you want the default to be native-endian). */
# define DEFAULT_BYTEBUFFER_ENDIAN_FLAGS ENDIAN_FLAG_FORCE_LITTLE
#endif

/** This class is used to hold a dynamically-resizable buffer of raw bytes (aka uint8s), and is also Flattenable and RefCountable. */
class ByteBuffer : public FlatCountable
{
public:
   /** Constructs a ByteBuffer that holds the specified bytes.
     * @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (i.e., don't allocate a buffer)
     * @param optBuffer May be set to point to an array of (numBytes) bytes that we should copy into our internal buffer.
     *                  If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
     * @param optAllocationStrategy If non-NULL, this object will be used to allocate and free bytes.  If left as NULL (the default),
     *                              then muscleAlloc(), muscleRealloc(), and/or muscleFree() will be called as necessary.
     */
   ByteBuffer(uint32 numBytes = 0, const uint8 * optBuffer = NULL, IMemoryAllocationStrategy * optAllocationStrategy = NULL) : _buffer(NULL), _numValidBytes(0), _numAllocatedBytes(0), _allocStrategy(optAllocationStrategy, false)
   {
      SetEndianFlags(EndianFlags(DEFAULT_BYTEBUFFER_ENDIAN_FLAGS));
      (void) SetBuffer(numBytes, optBuffer);
   }
  
   /** Copy Constructor. 
     * @param rhs The ByteBuffer to become a copy of.  We will also use (rhs)'s allocation strategy pointer and data flags.
     */
   ByteBuffer(const ByteBuffer & rhs) : FlatCountable(), _buffer(NULL), _numValidBytes(0), _numAllocatedBytes(0), _allocStrategy(rhs._allocStrategy) 
   {
      *this = rhs;
   }
  
   /** Destructor.  Deletes our held byte buffer. */
   virtual ~ByteBuffer() {Clear(true);}

   /** Assigment operator.  Copies the byte buffer from (rhs).  If there is an error copying (out of memory), we become an empty ByteBuffer.
     *  @param rhs the ByteBuffer to become a duplicate of
     *  @note We do NOT adopt (rhs)'s allocation strategy pointer or data format setting!
     */
   ByteBuffer & operator=(const ByteBuffer & rhs) {if ((this != &rhs)&&(SetBuffer(rhs.GetNumBytes(), rhs.GetBuffer()).IsError())) Clear(); return *this;}

   /** Read/Write Accessor.  Returns a pointer to our held buffer, or NULL if we are not currently holding a buffer. */
   uint8 * GetBuffer() {return _buffer;}

   /** Read-only Accessor.  Returns a pointer to our held buffer, or NULL if we are not currently holding a buffer. */
   const uint8 * GetBuffer() const {return _buffer;}

   /** Convenience synonym for GetBuffer(). */
   const uint8 * operator()() const {return _buffer;}

   /** Convenience synonym for GetBuffer(). */
   uint8 * operator()() {return _buffer;}

   /** Returns the size of our held buffer, in bytes. */
   uint32 GetNumBytes() const {return _numValidBytes;}

   /** Returns the number of bytes we have allocated internally.  Note that this
    *  number may be larger than the number of bytes we officially contain (as returned by GetNumBytes())
    */
   uint32 GetNumAllocatedBytes() {return _numAllocatedBytes;}

   /** Returns true iff (rhs) is holding data that is byte-for-byte the same as our own data
     * @param rhs the ByteBuffer to compare against
     */
   bool operator ==(const ByteBuffer &rhs) const {return (this == &rhs) ? true : ((GetNumBytes() == rhs.GetNumBytes()) ? (memcmp(GetBuffer(), rhs.GetBuffer(), GetNumBytes()) == 0) : false);}

   /** Returns true iff the data (rhs) is holding is different from our own (byte-for-byte). 
     * @param rhs the ByteBuffer to compare against
     */
   bool operator !=(const ByteBuffer &rhs) const {return !(*this == rhs);}

   /** Appends the bytes contained in (rhs) to this ByteBuffer. 
     * @param rhs the bytes to append to the end of this buffer
     */
   ByteBuffer & operator += (const ByteBuffer & rhs) {(void) AppendBytes(rhs); return *this;}

   /** Appends the specified byte to this ByteBuffer.
     * @param byte the byte to append to the end of this buffer
     */
   ByteBuffer & operator += (uint8 byte) {(void) AppendByte(byte); return *this;}

   /** Appends the specified byte to this ByteBuffer's contents.
     * @param theByte The byte to append
     * @param allocExtra If true, and we need to resize the buffer larger, we will use an exponential resize
     *                   so that the number of reallocations is small.  This is useful if you are going to be
     *                   doing a number of small appends.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t AppendByte(uint8 theByte, bool allocExtra = true) {return AppendBytes(&theByte, 1, allocExtra);}

   /** Appends the specified bytes to the byte array held by this buffer.
     * @param bytes Pointer to the bytes to append.  If NULL, the added bytes will be undefined.
     * @param numBytes Number of bytes to append.
     * @param allocExtra If true, and we need to resize the buffer larger, we will use an exponential resize
     *                   so that the number of reallocations is small.  This is useful if you are going to be
     *                   doing a number of small appends.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t AppendBytes(const uint8 * bytes, uint32 numBytes, bool allocExtra = true);

   /** Convenience method, works the same as above
     * @param bb A ByteBuffer whose contents we should append to our own.
     * @param allocExtra See above for details
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t AppendBytes(const ByteBuffer & bb, bool allocExtra = true) {return AppendBytes(bb.GetBuffer(), bb.GetNumBytes(), allocExtra);}

   /** Prints the contents of this ByteBuffer to stdout, or to the specified file.  Useful for quick debugging.
     * @param maxBytesToPrint The maximum number of bytes we should actually print.  Defaults to MUSCLE_NO_LIMIT, meaning
     *                        that by default we will always print every byte held by this ByteBuffer.
     * @param numColumns The number of columns to format the bytes into.  Defaults to 16.  See the documentation for
     *                   the PrintHexBytes() function for further details.
     * @param optFile If specified, the bytes will be printed to this file.  Defaults to NULL, meaning that the bytes
     *                will be printed to stdout.
     */
   void PrintToStream(uint32 maxBytesToPrint = MUSCLE_NO_LIMIT, uint32 numColumns = 16, FILE * optFile = NULL) const;

   /** Returns the contents of this ByteBuffer as a human-readable hexadecimal string
     * @param maxBytesToInclude optional maximum number of byte-values to include in the string.  Defaults to MUSCLE_NO_LIMIT.
     */
   String ToHexString(uint32 maxBytesToInclude = MUSCLE_NO_LIMIT) const;

   /** Returns the contents of this ByteBuffer as a human-readable annotated hexadecimal/ASCII string
     * @param maxBytesToInclude optional maximum number of byte-values to include in the string.  Defaults to MUSCLE_NO_LIMIT.
     * @param numColumns if specified non-zero, then the string will be generated with this many bytes per row.  Defaults to 16.
     */
   String ToAnnotatedHexString(uint32 maxBytesToInclude = MUSCLE_NO_LIMIT, uint32 numColumns = 16) const;

   /** Sets our content using the given byte buffer.
     * @param numBytes Number of bytes to copy in (or just to allocate, if (optBuffer) is NULL).  Defaults to zero bytes (i.e., don't allocate a buffer)
     * @param optBuffer May be set to point to an array of bytes to copy into our internal buffer.
     *                  If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on out of memory--there are no side effects if that occurs.
     */ 
   status_t SetBuffer(uint32 numBytes = 0, const uint8 * optBuffer = NULL);

   /** This method is similar to SetBuffer(), except that instead of copying the bytes out of (optBuffer),
     * we simply assume ownership of (optBuffer) for ourself.  This means that this ByteBuffer object will
     * free the passed-in array-pointer later on, so you must be very careful to make sure that that is
     * the right thing to do!  If you aren't sure, call SetBuffer() instead.
     * @param numBytes Number of bytes that optBuffer points to.
     * @param optBuffer Pointer to an array to adopt.  Note that we take ownership of this array!
     */
   void AdoptBuffer(uint32 numBytes, uint8 * optBuffer);

   /** Resets this ByteBuffer to its empty state, i.e. not holding any buffer.
     * @param releaseBuffer If true, we will immediately muscleFree() any buffer we are holding; otherwise we will keep the buffer around for potential later re-use.
     */
   void Clear(bool releaseBuffer = false);

   /** Causes us to allocate/reallocate our buffer as necessary to be the given size.
     * @param newNumBytes New desired length for our buffer
     * @param retainData If true, we will take steps to ensure our current data is retained (as much as possible).
     *                   Otherwise, the contents of the resized buffer will be undefined.
     * @return B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t SetNumBytes(uint32 newNumBytes, bool retainData);

   /** If we contain any extra bytes that are not being used to hold actual data (i.e. if GetNumAllocatedBytes()
    *  is returning a valud greater than GetNumBytes(), this method can be called to free up the unused bytes.
    *  This method calls muscleRealloc(), so it should be quite efficient.  After this method returns successfully,
    *  the number of allocated bytes will be equal to the number of used bytes.
    *  @returns B_NO_ERROR on success or B_OUT_OF_MEMORY (although I can't imagine why muscleRealloc() would ever fail)
    */
   status_t FreeExtraBytes();

   /** Causes us to forget the byte buffer we were holding, without freeing it.  Once this method 
     * is called, the calling code becomes responsible for calling muscleFree() on our (previously held) buffer.
     * @returns a pointer to our data bytes.  It becomes the responsibility of the caller to muscleFree() this buffer
     *          when he is done with it!
     */
   const uint8 * ReleaseBuffer() {const uint8 * ret = _buffer; _buffer = NULL; _numValidBytes = _numAllocatedBytes = 0; return ret;}

   /** Swaps our contents with those of the specified ByteBuffer.  This is an efficient O(1) operation.
     * @param swapWith ByteBuffer to swap contents with.
     * @note Allocation strategy pointer and data flags get swapped by this operation.
     */
   void SwapContents(ByteBuffer & swapWith)
   {
      muscleSwap(_buffer,            swapWith._buffer);
      muscleSwap(_numValidBytes,     swapWith._numValidBytes);
      muscleSwap(_numAllocatedBytes, swapWith._numAllocatedBytes);
      muscleSwap(_allocStrategy,     swapWith._allocStrategy);
   }

   /** Returns a hash code for this ByteBuffer */
   uint32 HashCode() const {return CalculateHashCode(GetBuffer(), GetNumBytes());}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ByteBuffer(ByteBuffer && rhs) : _buffer(NULL), _numValidBytes(0), _numAllocatedBytes(0) {SwapContents(rhs);}

   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ByteBuffer & operator =(ByteBuffer && rhs) {SwapContents(rhs); return *this;}
#endif

   // Flattenable interface
   virtual bool IsFixedSize() const {return false;}
   virtual uint32 TypeCode() const {return B_RAW_TYPE;}
   virtual uint32 FlattenedSize() const {return _numValidBytes;}
   virtual void Flatten(uint8 *buffer) const {memcpy(buffer, _buffer, _numValidBytes);}
   virtual bool AllowsTypeCode(uint32 tc) const {(void) tc; return true;}
   virtual status_t Unflatten(const uint8 *buffer, uint32 size) {return SetBuffer(size, buffer);}

   /** Returns a 32-bit checksum corresponding to this ByteBuffer's contents.
     * Note that this method is O(N).  The checksum is calculated based solely on the valid held 
     * bytes, and does not depend on the data-format flags, the allocation-strategy setting, or 
     * any reserve bytes that are currently allocated.
     */
   uint32 CalculateChecksum() const {return muscle::CalculateChecksum(_buffer, _numValidBytes);}

   /** Sets our allocation strategy pointer.  Note that you should be careful when you call this,
    *  as changing strategies can lead to allocation/deallocation method mismatches.
    *  @param imas Pointer to the new allocation strategy to use from now on.
    */
   void SetMemoryAllocationStrategy(IMemoryAllocationStrategy * imas) {_allocStrategy.SetPointer(imas);}

   /** Returns the current value of our allocation strategy pointer (may be NULL if the default strategy is in use) */
   IMemoryAllocationStrategy * GetMemoryAllocationStrategy() const {return _allocStrategy.GetPointer();}

   /** Sets our data-format flags.  These flags are used to determine what sort of
    *  data-endian-ness rules should be used by any multi-byte Append*(), Read*(), and Write*()
    *  methods called on this object in the future.  Default state is determined by the
    *  DEFAULT_BYTEBUFFER_ENDIAN_FLAGS define, which expands to ENDIAN_FLAG_FORCE_LITTLE by default.
    *  @param flags a bit-chord of ENDIAN_FLAG_* values.
    */
   void SetEndianFlags(EndianFlags flags) 
   {
#if B_HOST_IS_BENDIAN
      _allocStrategy.SetBool(flags.IsBitSet(ENDIAN_FLAG_FORCE_LITTLE));
#else
      _allocStrategy.SetBool(flags.IsBitSet(ENDIAN_FLAG_FORCE_BIG));
#endif
   }

   /** Returns true iff Append*(), Read*(), and Write*() methods will swap the endian-ness of
     * multi-byte data items as they go.
     * @see SetEndianFlags()
     */
   bool IsEndianSwapEnabled() const {return _allocStrategy.GetBool();}

   /** Returns true iff (byte) is part of our held buffer of bytes.
     * @param byte a pointer to a byte of data.  This pointer will not be derefenced by this method.
     */
   bool IsByteInLocalBuffer(const uint8 * byte) const {return ((_buffer)&&(byte >= _buffer)&&(byte < (_buffer+_numValidBytes)));}

///@{
   /** Convenience method for appending one data-item to the end of this buffer.  The buffer will be resized larger if necessary to hold
     * the written data.
     * @param val the value to append to the end of the buffer
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure
     */
   status_t AppendInt8(  int8           val) {return AppendInt8s(  &val, 1);}
   status_t AppendInt16( int16          val) {return AppendInt16s( &val, 1);}
   status_t AppendInt32( int32          val) {return AppendInt32s( &val, 1);}
   status_t AppendInt64( int64          val) {return AppendInt64s( &val, 1);}
   status_t AppendFloat( float          val) {return AppendFloats( &val, 1);}
   status_t AppendDouble(double         val) {return AppendDoubles(&val, 1);}
   status_t AppendPoint( const Point  & val) {return AppendPoints( &val, 1);}
   status_t AppendRect(  const Rect   & val) {return AppendRects(  &val, 1);}
   status_t AppendString(const String & val) {return AppendStrings(&val, 1);}
   status_t AppendFlat(  const Flattenable & val) {uint32 w = _numValidBytes; return WriteFlat(val, w);}
///@}

///@{
   /** Convenience method for appending an array of data-items to the end of this buffer.  The buffer will be resized larger if necessary 
     * to hold the written data.
     * @param vals Pointer to an array of values to append
     * @param numVals the number of value that (vals) points to
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure
     */
   status_t AppendInt8s(  const int8   * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteInt8s(  vals, numVals, w);}
   status_t AppendInt16s( const int16  * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteInt16s( vals, numVals, w);}
   status_t AppendInt32s( const int32  * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteInt32s( vals, numVals, w);}
   status_t AppendInt64s( const int64  * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteInt64s( vals, numVals, w);}
   status_t AppendFloats( const float  * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteFloats( vals, numVals, w);}
   status_t AppendDoubles(const double * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteDoubles(vals, numVals, w);}
   status_t AppendPoints( const Point  * vals, uint32 numVals) {uint32 w = _numValidBytes; return WritePoints( vals, numVals, w);}
   status_t AppendRects(  const Rect   * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteRects(  vals, numVals, w);}
   status_t AppendStrings(const String * vals, uint32 numVals) {uint32 w = _numValidBytes; return WriteStrings(vals, numVals, w);}
///@}

   /** Convenience method for reading one data item from this buffer.  (readByteOffset) will be advanced to the byte-offset after the read item.
     * If the item cannot be read (not enough bytes in the buffer), zero (or a default-constructed item) will be returned instead.
     * @param readByteOffset The offset from the top of our buffer at which to start reading.  Will be increased as a side-effect of this method.
     * @returns the read value.
     */
///@{
   int8   ReadInt8(  uint32 & readByteOffset) const {int8 ret;   return (ReadInt8s(  &ret, 1, readByteOffset) == 1) ? ret : 0;}
   int16  ReadInt16( uint32 & readByteOffset) const {int16 ret;  return (ReadInt16s( &ret, 1, readByteOffset) == 1) ? ret : 0;}
   int32  ReadInt32( uint32 & readByteOffset) const {int32 ret;  return (ReadInt32s( &ret, 1, readByteOffset) == 1) ? ret : 0;}
   int64  ReadInt64( uint32 & readByteOffset) const {int64 ret;  return (ReadInt64s( &ret, 1, readByteOffset) == 1) ? ret : 0;}
   float  ReadFloat( uint32 & readByteOffset) const {float ret;  return (ReadFloats( &ret, 1, readByteOffset) == 1) ? ret : 0.0f;}
   double ReadDouble(uint32 & readByteOffset) const {double ret; return (ReadDoubles(&ret, 1, readByteOffset) == 1) ? ret : 0.0;}
   Point  ReadPoint( uint32 & readByteOffset) const {Point ret;  return (ReadPoints( &ret, 1, readByteOffset) == 1) ? ret : Point();}
   Rect   ReadRect(  uint32 & readByteOffset) const {Rect ret;   return (ReadRects(  &ret, 1, readByteOffset) == 1) ? ret : Rect();}
   String ReadString(uint32 & readByteOffset) const {String ret; return (ReadStrings(&ret, 1, readByteOffset) == 1) ? ret : String();}
///@}

   /** Sets the state of the specified Flattenable item from this buffer.
     * @param flat the Flattenable item whose state should be read in from this buffer.
     * @param readByteOffset The offset from the top of our buffer at which to start reading.  On success, this value will
     *                       be increased by flat.FlattenedSize()
     * @param optMaxReadSize optional maximum number of bytes to pass to flat.Unflatten().  Default is MUSCLE_NO_LIMIT,
     *                       meaning that the flat.Unflatten() will be given access to the all bytes in this ByteBuffer
     *                       starting at (readByteOffset).
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t ReadFlat(Flattenable & flat, uint32 & readByteOffset, uint32 optMaxReadSize = MUSCLE_NO_LIMIT) const;

///@{
   /** Convenience method for reading an array of data items from this buffer.  (readByteOffset) will be advanced to the byte-offset after
     * the last read item.  The number of items actually read will be returned.
     * @param vals Pointer to an array of values to copy data into (must point to at least maxValsToRead writeable items)
     * @param maxValsToRead the maximum number of values to copy in to (vals)
     * @param readByteOffset The offset from the top of our buffer at which to start reading.  Will be increased as a side-effect of this method.
     * @returns the actual number of values copied over (which may be smaller than (maxValsToRead) if there weren't enough bytes left in the 
     *          buffer to read (maxValsToRead) items)
     */
   uint32 ReadInt8s(  int8   * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadInt16s( int16  * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadInt32s( int32  * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadInt64s( int64  * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadFloats( float  * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadDoubles(double * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadPoints( Point  * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadRects(  Rect   * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
   uint32 ReadStrings(String * vals, uint32 maxValsToRead, uint32 & readByteOffset) const;
///@}

///@{
   /** Convenience method for writing one data-item to a specified offset in this buffer.  (writeByteOffset) will be advanced to the 
     * byte-offset after the written item.  The buffer will be resized larger if necessary to hold the written data.
     * @param val the value to copy in to the buffer
     * @param writeByteOffset the offset-from-the-top-of-the-buffer, in bytes, to write to.  This value will be increased as a side-effect of this call.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY.
     */
   status_t WriteInt8(  int8           val, uint32 & writeByteOffset) {return WriteInt8s(  &val, 1, writeByteOffset);}
   status_t WriteInt16( int16          val, uint32 & writeByteOffset) {return WriteInt16s( &val, 1, writeByteOffset);}
   status_t WriteInt32( int32          val, uint32 & writeByteOffset) {return WriteInt32s( &val, 1, writeByteOffset);}
   status_t WriteInt64( int64          val, uint32 & writeByteOffset) {return WriteInt64s( &val, 1, writeByteOffset);}
   status_t WriteFloat( float          val, uint32 & writeByteOffset) {return WriteFloats( &val, 1, writeByteOffset);}
   status_t WriteDouble(double         val, uint32 & writeByteOffset) {return WriteDoubles(&val, 1, writeByteOffset);}
   status_t WritePoint( const Point  & val, uint32 & writeByteOffset) {return WritePoints( &val, 1, writeByteOffset);}
   status_t WriteRect(  const Rect   & val, uint32 & writeByteOffset) {return WriteRects(  &val, 1, writeByteOffset);}
   status_t WriteString(const String & val, uint32 & writeByteOffset) {return WriteStrings(&val, 1, writeByteOffset);}
   status_t WriteFlat(const Flattenable & val, uint32 & writeByteOffset);
///@}

///@{
   /** Convenience method for writing an array data-items to a specified offset in this buffer.  (writeByteOffset) will be advanced to 
     * the byte-offset after the last written item.  The buffer will be resized larger if necessary to hold the written data.
     * @param vals Pointer to an array of values to copy in to the buffer.  Must point to at least (numVals) valid items.
     * @param numVals How many items to copy out of the (vals) array.
     * @param writeByteOffset the offset-from-the-top-of-the-buffer, in bytes, to write to.  This value will be increased as a side-effect of this call.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t WriteInt8s(  const int8   * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteInt16s( const int16  * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteInt32s( const int32  * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteInt64s( const int64  * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteFloats( const float  * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteDoubles(const double * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WritePoints( const Point  * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteRects(  const Rect   * vals, uint32 numVals, uint32 & writeByteOffset);
   status_t WriteStrings(const String * vals, uint32 numVals, uint32 & writeByteOffset);
///@}

protected:
   /** Overridden to set our buffer directly from (copyFrom)'s Flatten() method 
     * @param copyFrom the object to copy data from
     */
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);

private:
   uint32 GetNumValidBytesAtOffset(uint32 offset) const {return (offset<_numValidBytes) ? (_numValidBytes-offset) : 0;}
   status_t SetNumBytesWithExtraSpace(uint32 newNumValidBytes, bool allocExtra);

   uint8 * _buffer;            // pointer to our byte array (or NULL if we haven't got one)
   uint32 _numValidBytes;      // number of bytes the user thinks we have
   uint32 _numAllocatedBytes;  // number of bytes we actually have
   PointerAndBool<IMemoryAllocationStrategy> _allocStrategy;
};
DECLARE_REFTYPES(ByteBuffer);

/** ByteBuffer-concatenation operator.
  * @param lhs the first ByteBuffer to concatenate
  * @param rhs the second ByteBuffer to concatenate
  * @returns a ByteBuffer that holds a copy of the contents of (lhs) followed by a copy of the contents of (rhs)
  */
ByteBuffer operator+(const ByteBuffer & lhs, const ByteBuffer & rhs);

/** This function returns a pointer to a singleton ObjectPool that can be used to minimize the number of 
 *  ByteBuffer allocations and frees by recycling the ByteBuffer objects.
 */
ByteBufferRef::ItemPool * GetByteBufferPool();

/** Convenience method:  Gets a ByteBuffer from the ByteBuffer pool, makes sure it holds the specified number of bytes, and returns it.
 *  @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (i.e. retrieve an empty buffer)
 *  @param optBuffer If non-NULL, points to an array of (numBytes) bytes to copy in to our internal buffer. 
 *                   If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
 *  @return Reference to a ByteBuffer object that has been initialized as specified, or a NULL ref on failure (out of memory).
 */
ByteBufferRef GetByteBufferFromPool(uint32 numBytes = 0, const uint8 * optBuffer = NULL);

/** As above, except that the byte buffer is obtained from the specified pool instead of from the default ByteBuffer pool.
 *  @param pool the ObjectPool to allocate the ByteBuffer from.
 *  @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (i.e. retrieve an empty buffer)
 *  @param optBuffer If non-NULL, points to an array of (numBytes) bytes to copy in to our internal buffer. 
 *                   If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
 *  @return Reference to a ByteBuffer object that has been initialized as specified, or a NULL ref on failure (out of memory).
 */
ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, uint32 numBytes = 0, const uint8 * optBuffer = NULL);

/** Convenience method:  Gets a ByteBuffer from the default ByteBuffer pool, flattens (flattenMe) into the byte buffer, and 
 *  returns a reference to the new ByteBuffer.
 *  @param flattenMe A Flattenable object to flatten.
 *  @return Reference to a ByteBuffer object as specified, or a NULL ref on failure (out of memory).
 */
template <class T> inline ByteBufferRef GetFlattenedByteBufferFromPool(const T & flattenMe) {return GetFlattenedByteBufferFromPool(*GetByteBufferPool(), flattenMe);}

/** Convenience method:  Gets a ByteBuffer from the specified ByteBuffer pool, flattens (flattenMe) into the byte buffer, and 
 *  returns a reference to the new ByteBuffer.
 *  @param pool The ObjectPool to retrieve the new ByteBuffer object from.
 *  @param flattenMe A Flattenable object (or at least an object with Flatten() and FlattenedSize() methods) to flatten.
 *  @return Reference to a ByteBuffer object as specified, or a NULL ref on failure (out of memory).
 */
template <class T> inline ByteBufferRef GetFlattenedByteBufferFromPool(ObjectPool<ByteBuffer> & pool, const T & flattenMe)
{
   ByteBufferRef bufRef = GetByteBufferFromPool(pool, flattenMe.FlattenedSize());
   if (bufRef()) flattenMe.Flatten(bufRef()->GetBuffer());
   return bufRef;
}

/** Convenience method:   Returns a ByteBufferRef containing all the remaining data read in from (dio).
 *  @param dio The SeekableDataIO object to read the remaining data out of.
 *  @return Reference to a ByteBuffer object that has been initialized as specified, or a NULL ref on failure (out of memory).
  */
ByteBufferRef GetByteBufferFromPool(SeekableDataIO & dio);

/** As above, except that the byte buffer is obtained from the specified pool instead of from the default ByteBuffer pool.
 *  @param pool the ObjectPool to allocate the ByteBuffer from.
 *  @param dio The DataIO object to read the remaining data out of.
 *  @return Reference to a ByteBuffer object that has been initialized as specified, or a NULL ref on failure (out of memory).
 */
ByteBufferRef GetByteBufferFromPool(ObjectPool<ByteBuffer> & pool, SeekableDataIO & dio);

/** Convenience method:  returns a read-only reference to an empty ByteBuffer */
const ByteBuffer & GetEmptyByteBuffer();

/** Convenience method:  returns a read-only reference to a ByteBuffer that contains no data. */
ConstByteBufferRef GetEmptyByteBufferRef();

/** This interface is used to represent any object that knows how to allocate, reallocate, and free memory in a special way. */
class IMemoryAllocationStrategy 
{
public:
   /** Default constructor */
   IMemoryAllocationStrategy() {/* empty */}

   /** Destructor */
   virtual ~IMemoryAllocationStrategy() {/* empty */}

   /** Called when a ByteBuffer needs to allocate a memory buffer.  This method should be implemented to behave similarly to malloc().
    *  @param size Number of bytes to allocate
    *  @returns A pointer to the allocated bytes on success, or NULL on failure.
    */
   virtual void * Malloc(size_t size) = 0;

   /** Called when a ByteBuffer needs to resize a memory buffer.  This method should be implemented to behave similarly to realloc().
    *  @param ptr Pointer to the buffer to resize, or NULL if there is no current buffer.
    *  @param newSize Desired new size of the buffer
    *  @param oldSize Current size of the buffer
    *  @param retainData If false, the returned buffer need not retain the contents of the old buffer.
    *  @returns A pointer to the new buffer on success, or NULL on failure (or if newSize == 0)
    */
   virtual void * Realloc(void * ptr, size_t newSize, size_t oldSize, bool retainData) = 0;

   /** Called when a ByteBuffer needs to free a memory buffer.  This method should be implemented to behave similarly to free().
    *  @param ptr Pointer to the buffer to free.
    *  @param size Number of byes in the buffer.
    */
   virtual void Free(void * ptr, size_t size) = 0;
};

} // end namespace muscle

#endif
