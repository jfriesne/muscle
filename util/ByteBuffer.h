/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteBuffer_h
#define MuscleByteBuffer_h

#include "util/FlatCountable.h"
#include "util/OutputPrinter.h"
#include "util/String.h"

namespace muscle {

class SeekableDataIO;
class IMemoryAllocationStrategy;

/** This class is used to hold a dynamically-resizable buffer of raw bytes (aka uint8s), and is also Flattenable and RefCountable. */
class MUSCLE_NODISCARD ByteBuffer : public FlatCountable
{
public:
   /** Constructs a ByteBuffer that holds the specified bytes.
     * @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (ie, don't allocate a buffer)
     * @param optBuffer May be set to point to an array of (numBytes) bytes that we should copy into our internal buffer.
     *                  If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
     * @param optAllocationStrategy If non-NULL, this object will be used to allocate and free bytes.  If left as NULL (the default),
     *                              then muscleAlloc(), muscleRealloc(), and/or muscleFree() will be called as necessary.
     */
   ByteBuffer(uint32 numBytes = 0, const uint8 * optBuffer = NULL, IMemoryAllocationStrategy * optAllocationStrategy = NULL) : _buffer(NULL), _numValidBytes(0), _numAllocatedBytes(0), _allocStrategy(optAllocationStrategy)
   {
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
     *  @note We do NOT adopt (rhs)'s allocation strategy pointer!
     */
   ByteBuffer & operator=(const ByteBuffer & rhs) {if ((this != &rhs)&&(SetBuffer(rhs.GetNumBytes(), rhs.GetBuffer()).IsError())) Clear(); return *this;}

   /** Read/Write Accessor.  Returns a pointer to our held buffer, or NULL if we are not currently holding a buffer. */
   MUSCLE_NODISCARD uint8 * GetBuffer() {return _buffer;}

   /** Read-only Accessor.  Returns a pointer to our held buffer, or NULL if we are not currently holding a buffer. */
   MUSCLE_NODISCARD const uint8 * GetBuffer() const {return _buffer;}

   /** Convenience synonym for GetBuffer(). */
   MUSCLE_NODISCARD const uint8 * operator()() const {return _buffer;}

   /** Convenience synonym for GetBuffer(). */
   MUSCLE_NODISCARD uint8 * operator()() {return _buffer;}

   /** Returns the size of our held buffer, in bytes. */
   MUSCLE_NODISCARD uint32 GetNumBytes() const {return _numValidBytes;}

   /** Returns the number of bytes we have allocated internally.  Note that this
    *  number may be larger than the number of bytes we officially contain (as returned by GetNumBytes())
    */
   MUSCLE_NODISCARD uint32 GetNumAllocatedBytes() {return _numAllocatedBytes;}

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
     * @param p The OutputPrinter to use for printing.
     * @param maxBytesToPrint The maximum number of bytes we should actually print.  Defaults to MUSCLE_NO_LIMIT, meaning
     *                        that by default we will always print every byte held by this ByteBuffer.
     * @param numColumns The number of columns to format the bytes into.  Defaults to 16.  See the documentation for
     *                   the PrintHexBytes() function for further details.
     */
   void Print(const OutputPrinter & p, uint32 maxBytesToPrint = MUSCLE_NO_LIMIT, uint32 numColumns = 16) const;

   /** Returns the contents of this ByteBuffer as a human-readable hexadecimal string
     * @param maxBytesToInclude optional maximum number of byte-values to include in the string.  Defaults to MUSCLE_NO_LIMIT.
     * @param withSpaces if true (the default) then the hex-digit pairs in the returned String will be separated by spaces.
     *                   if passed as false, they will be packed together with no white space.
     */
   String ToHexString(uint32 maxBytesToInclude = MUSCLE_NO_LIMIT, bool withSpaces = true) const;

   /** Returns the contents of this ByteBuffer as a human-readable annotated hexadecimal/ASCII string
     * @param maxBytesToInclude optional maximum number of byte-values to include in the string.  Defaults to MUSCLE_NO_LIMIT.
     * @param numColumns if specified non-zero, then the string will be generated with this many bytes per row.  Defaults to 16.
     */
   String ToAnnotatedHexString(uint32 maxBytesToInclude = MUSCLE_NO_LIMIT, uint32 numColumns = 16) const;

   /** Sets our content using the given byte buffer.
     * @param numBytes Number of bytes to copy in (or just to allocate, if (optBuffer) is NULL).  Defaults to zero bytes (ie, don't allocate a buffer)
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

   /** Resets this ByteBuffer to its empty state, ie not holding any buffer.
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

   /** Convenience method:  If our current buffer size is greater than (newNumBytes), reduces our buffer size to (newNumBytes).
     * Otherwise, does nothing.  (In particular, this method will never grow our buffer larger; call SetNumBytes() to do that)
     * @param newNumBytes New desired length for our buffer
     */
   void TruncateToLength(uint32 newNumBytes) {if (newNumBytes < GetNumBytes()) (void) SetNumBytes(newNumBytes, true);}

   /** If we contain any extra bytes that are not being used to hold actual data (ie if GetNumAllocatedBytes()
    *  is returning a value greater than GetNumBytes(), this method can be called to free up the unused bytes.
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
   uint8 * ReleaseBuffer() {uint8 * ret = _buffer; _buffer = NULL; _numValidBytes = _numAllocatedBytes = 0; return ret;}

   /** Swaps our contents with those of the specified ByteBuffer.  This is an efficient O(1) operation.
     * @param swapWith ByteBuffer to swap contents with.
     * @note Allocation strategy pointer and data flags get swapped by this operation.
     */
   void SwapContents(ByteBuffer & swapWith) MUSCLE_NOEXCEPT
   {
      muscleSwap(_buffer,            swapWith._buffer);
      muscleSwap(_numValidBytes,     swapWith._numValidBytes);
      muscleSwap(_numAllocatedBytes, swapWith._numAllocatedBytes);
      muscleSwap(_allocStrategy,     swapWith._allocStrategy);
   }

   /** Returns a hash code for this ByteBuffer */
   MUSCLE_NODISCARD uint32 HashCode() const {return CalculateHashCode(GetBuffer(), GetNumBytes());}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ByteBuffer(ByteBuffer && rhs) MUSCLE_NOEXCEPT : _buffer(NULL), _numValidBytes(0), _numAllocatedBytes(0), _allocStrategy(NULL) {SwapContents(rhs);}

   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ByteBuffer & operator =(ByteBuffer && rhs) MUSCLE_NOEXCEPT {SwapContents(rhs); return *this;}
#endif

   // Flattenable interface
   MUSCLE_NODISCARD virtual bool IsFixedSize() const {return false;}
   MUSCLE_NODISCARD virtual uint32 TypeCode() const {return B_RAW_TYPE;}
   MUSCLE_NODISCARD virtual uint32 FlattenedSize() const {return _numValidBytes;}
   virtual void Flatten(DataFlattener flat) const {flat.WriteBytes(_buffer, _numValidBytes);}
   MUSCLE_NODISCARD virtual bool AllowsTypeCode(uint32 tc) const {(void) tc; return true;}
   virtual status_t Unflatten(DataUnflattener & unflat)
   {
      MRETURN_ON_ERROR(SetBuffer(unflat.GetNumBytesAvailable(), unflat.GetCurrentReadPointer()));
      MRETURN_ON_ERROR(unflat.SeekToEnd());
      return unflat.GetStatus();
   }

   /** Returns a 32-bit checksum corresponding to this ByteBuffer's contents.
     * Note that this method is O(N).  The checksum is calculated based solely on the valid held
     * bytes and does not depend on the the allocation-strategy setting or any reserve-bytes
     * that are currently allocated but not valid.
     */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const {return muscle::CalculateChecksum(_buffer, _numValidBytes);}

   /** Sets our allocation strategy pointer.  Note that you should be careful when you call this,
    *  as changing strategies can lead to allocation/deallocation method mismatches.
    *  @param imas Pointer to the new allocation strategy to use from now on.
    */
   void SetMemoryAllocationStrategy(IMemoryAllocationStrategy * imas) {_allocStrategy = imas;}

   /** Returns the current value of our allocation strategy pointer (may be NULL if the default strategy is in use) */
   MUSCLE_NODISCARD IMemoryAllocationStrategy * GetMemoryAllocationStrategy() const {return _allocStrategy;}

   /** Returns true iff (byte) is part of our held buffer of bytes.
     * @param byte a pointer to a byte of data.  This pointer will not be derefenced by this method.
     */
   MUSCLE_NODISCARD bool IsByteInLocalBuffer(const uint8 * byte) const {return ((_buffer)&&(byte >= _buffer)&&(byte < (_buffer+_numValidBytes)));}

protected:
   /** Overridden to set our buffer directly from (copyFrom)'s Flatten() method
     * @param copyFrom the object to copy data from
     */
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);

private:
   status_t SetNumBytesWithExtraSpace(uint32 newNumValidBytes, bool allocExtra);

   uint8 * _buffer;            // pointer to our byte array (or NULL if we haven't got one)
   uint32 _numValidBytes;      // number of bytes the user thinks we have
   uint32 _numAllocatedBytes;  // number of bytes we actually have
   IMemoryAllocationStrategy * _allocStrategy;
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
MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL ByteBufferRef::ItemPool * GetByteBufferPool();

/** Convenience method:  Gets a ByteBuffer from the ByteBuffer pool, makes sure it holds the specified number of bytes, and returns it.
 *  @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (ie retrieve an empty buffer)
 *  @param optBuffer If non-NULL, points to an array of (numBytes) bytes to copy in to our internal buffer.
 *                   If NULL, this ByteBuffer will contain (numBytes) uninitialized bytes.  Defaults to NULL.
 *  @return Reference to a ByteBuffer object that has been initialized as specified, or a NULL ref on failure (out of memory).
 */
ByteBufferRef GetByteBufferFromPool(uint32 numBytes = 0, const uint8 * optBuffer = NULL);

/** As above, except that the byte buffer is obtained from the specified pool instead of from the default ByteBuffer pool.
 *  @param pool the ObjectPool to allocate the ByteBuffer from.
 *  @param numBytes Number of bytes to copy in (or just allocate, if (optBuffer) is NULL).  Defaults to zero bytes (ie retrieve an empty buffer)
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
   if ((bufRef())&&(flattenMe.FlattenToByteBuffer(*bufRef()).IsError())) bufRef.Reset();
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
MUSCLE_NODISCARD const ByteBuffer & GetEmptyByteBuffer();

/** Convenience method:  returns a read-only reference to a ByteBuffer that contains no data. */
const ConstByteBufferRef & GetEmptyByteBufferRef();

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
   MUSCLE_NODISCARD virtual void * Malloc(size_t size) = 0;

   /** Called when a ByteBuffer needs to resize a memory buffer.  This method should be implemented to behave similarly to realloc().
    *  @param ptr Pointer to the buffer to resize, or NULL if there is no current buffer.
    *  @param newSize Desired new size of the buffer
    *  @param oldSize Current size of the buffer
    *  @param retainData If false, the returned buffer need not retain the contents of the old buffer.
    *  @returns A pointer to the new buffer on success, or NULL on failure (or if newSize == 0)
    */
   MUSCLE_NODISCARD virtual void * Realloc(void * ptr, size_t newSize, size_t oldSize, bool retainData) = 0;

   /** Called when a ByteBuffer needs to free a memory buffer.  This method should be implemented to behave similarly to free().
    *  @param ptr Pointer to the buffer to free.
    *  @param size Number of byes in the buffer.
    */
   virtual void Free(void * ptr, size_t size) = 0;
};

// The methods below have been implemented here (instead of inside DataFlattener.h or DataUnflattener.h)
// to avoid chicken-and-egg programs with include-ordering.  At this location we are guaranteed that the compiler
// knows everything it needs to know about both the DataFlattener/DataUnflattener classes and the ByteBuffer class.

template<class EndianConverter, class SizeChecker>
void DataUnflattenerHelper<EndianConverter, SizeChecker> :: SetBuffer(const ByteBuffer & readFrom, uint32 maxBytes, uint32 startOffset)
{
   startOffset = muscleMin(startOffset, readFrom.GetNumBytes());
   SetBuffer(readFrom.GetBuffer()+startOffset, muscleMin(maxBytes, readFrom.GetNumBytes()-startOffset));
}

template<class EndianConverter>
DataFlattenerHelper<EndianConverter> :: DataFlattenerHelper(ByteBuffer & buf) : _endianConverter()
{
   Init();
   SetBuffer(buf.GetBuffer(), buf.GetNumBytes());
}

template<class EndianConverter>
DataFlattenerHelper<EndianConverter> :: DataFlattenerHelper(const Ref<ByteBuffer> & buf) : _endianConverter()
{
   Init();
   if (buf()) SetBuffer(buf()->GetBuffer(), buf()->GetNumBytes());
}

template<class EndianConverter>
Ref<ByteBuffer> DataFlattenerHelper<EndianConverter> :: GetByteBufferFromPool() const
{
   return muscle::GetByteBufferFromPool(GetNumBytesWritten(), GetBuffer());
}

template<class EndianConverter>
void DataFlattenerHelper<EndianConverter> :: WriteBytes(const ByteBuffer & buf)
{
   WriteBytes(buf.GetBuffer(), buf.GetNumBytes());
}

template<class SubclassType>
status_t PseudoFlattenable<SubclassType> :: UnflattenFromByteBuffer(const ByteBuffer & buf)
{
   return static_cast<SubclassType *>(this)->UnflattenFromBytes(buf.GetBuffer(), buf.GetNumBytes());
}

template<class SubclassType>
status_t PseudoFlattenable<SubclassType> :: UnflattenFromByteBuffer(const ConstRef<ByteBuffer> & bufRef)
{
   return bufRef() ? static_cast<SubclassType *>(this)->UnflattenFromBytes(bufRef()->GetBuffer(), bufRef()->GetNumBytes()) : B_BAD_ARGUMENT;
}

template<class SubclassType>
status_t PseudoFlattenable<SubclassType> :: FlattenToByteBuffer(ByteBuffer & outBuf) const
{
   const SubclassType * sc = static_cast<const SubclassType *>(this);
   const uint32 flatSize = sc->FlattenedSize();
   MRETURN_ON_ERROR(outBuf.SetNumBytes(flatSize, false));
   sc->FlattenToBytes(outBuf.GetBuffer(), flatSize);
   return B_NO_ERROR;
}

template<class SubclassType>
Ref<ByteBuffer> PseudoFlattenable<SubclassType> :: FlattenToByteBuffer() const
{
   const SubclassType * sc = static_cast<const SubclassType *>(this);
   const uint32 flatSize = sc->FlattenedSize();
   ByteBufferRef bufRef = GetByteBufferFromPool(flatSize);
   if (bufRef()) sc->FlattenToBytes(bufRef()->GetBuffer(), flatSize);
   return bufRef;
}

template<class EndianConverter, class SizeChecker>
void DataUnflattenerHelper<EndianConverter, SizeChecker> :: SetBuffer(const ConstRef<ByteBuffer> & readFrom, uint32 maxBytes, uint32 startOffset)
{
   if (readFrom()) SetBuffer(*readFrom(), maxBytes, startOffset);
              else Reset();
}

} // end namespace muscle

#endif
