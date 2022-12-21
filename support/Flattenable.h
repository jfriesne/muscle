/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFlattenable_h
#define MuscleFlattenable_h

#include "support/PseudoFlattenable.h"

namespace muscle {

class ByteBuffer;  // forward reference to avoid chicken-and-egg problems

/** This class is an interface representing an object that knows how
 *  to save itself into an array of bytes, and recover its state from
 *  an array of bytes.
 */
class Flattenable
{
public:
   /** Constructor */
   Flattenable() {/* empty */}

   /** Destructor */
   virtual ~Flattenable() {/* empty */}

   /** @copydoc DoxyTemplate::IsFixedSize() const */
   virtual bool IsFixedSize() const = 0;

   /** @copydoc DoxyTemplate::TypeCode() const */
   virtual uint32 TypeCode() const = 0;

   /** @copydoc DoxyTemplate::FlattenedSize() const */
   virtual uint32 FlattenedSize() const = 0;

   /** @copydoc DoxyTemplate::Flatten(DataFlattener) const */
   virtual void Flatten(DataFlattener flat) const = 0;

   /** @copydoc DoxyTemplate::AllowsTypeCode(uint32) const
     * @note base class's default implementation returns true iff (tc) equals either B_RAW_TYPE, or the value returned by TypeCode().
     */
   virtual bool AllowsTypeCode(uint32 tc) const {return ((tc == B_RAW_TYPE)||(tc == TypeCode()));}

   /** @copydoc DoxyTemplate::Unflatten(DataUnflattener & unflat) */
   virtual status_t Unflatten(DataUnflattener & unflat) = 0;

   /**
    *  Causes (copyTo)'s state to set from this Flattenable, if possible.
    *  Default implementation is not very efficient, since it has to flatten
    *  this object into a byte buffer, and then unflatten the bytes back into
    *  (copyTo).  However, you can override CopyFromImplementation() to provide
    *  a more efficient implementation when possible.
    *  @param copyTo Object to make into the equivalent of this object.  (copyTo)
    *                May be any subclass of Flattenable.
    *  @return B_NO_ERROR on success, or an error code (such as B_TYPE_MISMATCH) on failure.
    */
   status_t CopyTo(Flattenable & copyTo) const
   {
      return (this == &copyTo) ? B_NO_ERROR : ((copyTo.AllowsTypeCode(TypeCode())) ? copyTo.CopyFromImplementation(*this) : B_TYPE_MISMATCH);
   }

   /**
    *  Causes our state to be set from (copyFrom)'s state, if possible.
    *  Default implementation is not very efficient, since it has to flatten
    *  (copyFrom) into a byte buffer, and then unflatten the bytes back into
    *  (this).  However, you can override CopyFromImplementation() to provide
    *  a more efficient implementation when possible.
    *  @param copyFrom Object to read from to set the state of this object.
    *                  (copyFrom) may be any subclass of Flattenable.
    *  @return B_NO_ERROR on success, or an error code (such as B_TYPE_MISMATCH) on failure.
    */
   status_t CopyFrom(const Flattenable & copyFrom)
   {
      return (this == &copyFrom) ? B_NO_ERROR : ((AllowsTypeCode(copyFrom.TypeCode())) ? CopyFromImplementation(copyFrom) : B_TYPE_MISMATCH);
   }

   /** Convenience method:  Unflattens this object from the bytes in the supplied buffer.
     * @param buffer pointer to a buffer of bytes
     * @param numBytes how many bytes (buffer) points to
     * @returns B_NO_ERROR on success, or another value on failure.
     */
   status_t UnflattenFromBytes(const uint8 * buffer, uint32 numBytes)
   {
      DataUnflattener unflat(buffer, numBytes);
      return Unflatten(unflat);
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
   void FlattenToBytes(uint8 * writeTo, uint32 flatSize) const {Flatten(DataFlattener(writeTo, flatSize));}

   /** Convenience method.  Calls through to Flatten()
     * @param writeTo The buffer to write bytes into.  The buffer must be at least (FlattenedSize()) bytes long.
     */
   void FlattenToBytes(uint8 * writeTo) const {Flatten(DataFlattener(writeTo, FlattenedSize()));}

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

protected:
   /**
    *  Called by CopyFrom() and CopyTo().  Sets our state from (copyFrom) if
    *  possible.  Default implementation is not very efficient, since it has
    *  to flatten (copyFrom) into a byte buffer, and then unflatten the bytes
    *  back into (this).  However, you can override CopyFromImplementation()
    *  to provide a more efficient implementation when possible.
    *  @param copyFrom Object to set this object's state from.
    *                  May be any subclass of Flattenable, but it has been
    *                  pre-screened by CopyFrom() (or CopyTo()) to make sure
    *                  it's not (*this), and that we allow its type code.
    *  @return B_NO_ERROR on success, or an error code on failure.
    */
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);
};

} // end namespace muscle

#endif /* MuscleFlattenable_h */

