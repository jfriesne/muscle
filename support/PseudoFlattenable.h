/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePseudoFlattenable_h
#define MusclePseudoFlattenable_h

/******************************************************************************
/
/   File:      PseudoFlattenable.h
/
/   Description:    no-op/empty/template-compatible version of the Flattenable interface
/
******************************************************************************/

#include "support/DataFlattener.h"
#include "support/DataUnflattener.h"
#include "util/RefCount.h"

namespace muscle {

class DataIO;
class ByteBuffer;
class Flattenable;

/** This class is here to support lightweight subclasses that want to have a Flattenable-like
  * API (Flatten(), Unflatten(), etc) without incurring the one-word-per-object memory
  * overhead caused by the presence of virtual methods.  To use this class, subclass your
  * class from this one and declare Flatten(), Unflatten(), FlattenedSize(), etc methods
  * in your class, but don't make them virtual.  That will be enough to allow you to
  * use Message::AddFlat(), Message::FindFlat(), etc on your objects, with no extra
  * memory overhead.  See the MUSCLE Point and Rect classes for examples of this technique.
  */
template <class SubclassType> class PseudoFlattenable
{
public:
   /**
    * Dummy implemention of CopyFrom().  It's here only so that Message::FindFlat() will
    * compile when called with a PseudoFlattenable object as an argument.
    * @param copyFrom This parameter is ignored.
    * @returns B_UNIMPLEMENTED always, because given that this object is not a Flattenable object,
    *          it's assumed that it can't receive the state of a Flattenable object either.
    *          (but if that's not the case for your class, your subclass can implement its
    *           own CopyFrom() method to taste)
    */
   status_t CopyFrom(const Flattenable & copyFrom)
   {
#if !defined(_MSC_VER) || (_MSC_VER >= 1910)  // avoids error C2027: use of undefined type 'muscle::Flattenable'
      (void) copyFrom;
#endif
      return B_UNIMPLEMENTED;
   }

   /** Default implementation returns true iff (tc) is either B_RAW_TYPE or equal to the type-code returned by our TypeCode() method
     * @param tc the type code to check if we are compatible with, or not
     */
   bool AllowsTypeCode(uint32 tc) const {return ((tc == B_RAW_TYPE)||(tc == static_cast<const SubclassType*>(this)->TypeCode()));}

   /** Convenience method:  Unflattens this object from the bytes in the supplied buffer.
     * @param buffer pointer to a buffer of bytes
     * @param numBytes how many bytes (buffer) points to
     * @returns B_NO_ERROR on success, or another value on failure.
     */
   status_t UnflattenFromBytes(const uint8 * buffer, uint32 numBytes)
   {
      DataUnflattener unflat(buffer, numBytes);
      return static_cast<SubclassType *>(this)->Unflatten(unflat);
   }

   /** Convenience method.  Unflattens this object from the supplied ByteBuffer object.
     * @param buf The ByteBuffer to unflatten from.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t UnflattenFromByteBuffer(const ByteBuffer & buf);

   /** Convenience method.  Unflattens this object from the specified byte buffer reference.
     * @param bufRef The ByteBufferRef to unflatten from.
     * @returns B_NO_ERROR on success, or an error code on failure (B_BAD_ARGUMENT if bufRef was a NULL reference)
     */
   status_t UnflattenFromByteBuffer(const ConstRef<ByteBuffer> & bufRef);

   /** Convenience method.  Flattens this object into the supplied ByteBuffer object.
     * @param outBuf the ByteBuffer to dump our flattened bytes into.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t FlattenToByteBuffer(ByteBuffer & outBuf) const;

   /** Convenience method.  Calls through to Flatten()
     * @param writeTo The buffer to write bytes into.  Caller must guarantee that this pointer remains valid when any methods on this class are called.
     * @param flatSize How many bytes the Flatten() call should write.  This should be equal to the value returned by our FlattenedSize() method.
     *                 Providing it as an argument here allows us to avoid an unecessary second call to FlattenedSize() if you already know its value.
     */
   void FlattenToBytes(uint8 * writeTo, uint32 flatSize) const {static_cast<const SubclassType *>(this)->Flatten(DataFlattener(writeTo, flatSize));}

   /** Convenience method.  Calls through to Flatten()
     * @param writeTo The buffer to write bytes into.  The buffer must be at least (FlattenedSize()) bytes long.
     */
   void FlattenToBytes(uint8 * writeTo) const
   {
      const SubclassType * sc = static_cast<const SubclassType *>(this);
      sc->Flatten(DataFlattener(writeTo, sc->FlattenedSize()));
   }

   /** Convenience method.  Allocates an appropriately sized ByteBuffer object via GetByteBufferFromPool(), Flatten()s
     * this object into the byte buffer, and returns the resulting ByteBufferRef.  Returns a NULL reference on failure (out of memory?)
     */
   Ref<ByteBuffer> FlattenToByteBuffer() const;

   /** Convenience method.  Flattens this object to the given DataIO object.
     * @param outputStream  The DataIO object to send our flattened data to.  This DataIO should be in blocking I/O mode; this method won't work reliably
     *                      if used with non-blocking I/O.
     * @param addSizeHeader If true, we will prefix our flattened data with a four-byte little-endian uint32 indicating the number of bytes
     *                      of flattened data that we are going to write.  If false, then the buffer size will need to be determined by
     *                      the reading code by some other means.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t FlattenToDataIO(DataIO & outputStream, bool addSizeHeader) const;

   /** Convenience method.  Flattens this object from the given DataIO object.
     * @param inputStream the DataIO object to read data from.  This DataIO should be in blocking I/O mode; this method won't work reliably
     *                    if used with non-blocking I/O.
     * @param optReadSize If non-negative, this many bytes will be read from (inputStream).  If set to -1, then the first four
     *                    bytes read from the stream will be used to determine how many more bytes should be read from the stream.
     *                    If the data was created with Flatten(io, true), then set this parameter to -1.
     * @param optMaxReadSize An optional value indicating the largest number of bytes that should be read by this call.
     *                       This value is only used if (optReadSize) is negative.  If (optReadSize) is negative and
     *                       the first four bytes read from (inputStream) are greater than this value, then this method
     *                       will return B_BAD_DATA instead of trying to read that many bytes.  This can be useful if you
     *                       want to prevent the possibility of huge buffers being allocated due to malicious or corrupted
     *                       size headers.  Defaults to MUSCLE_NO_LIMIT, meaning that no size limit is enforced.
     * @returns B_NO_ERROR if the data was correctly read, or an error code otherwise.
     */
   status_t UnflattenFromDataIO(DataIO & inputStream, int32 optReadSize, uint32 optMaxReadSize = MUSCLE_NO_LIMIT);
};

} // end namespace muscle

#endif /* MusclePseudoFlattenable_h */

