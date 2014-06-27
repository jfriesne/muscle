/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

/******************************************************************************
/
/   File:      Flattenable.h
/
/   Description:    version of Be's BFlattenable base class.
/
******************************************************************************/

#ifndef MuscleFlattenable_h
#define MuscleFlattenable_h

#include "support/MuscleSupport.h"
#include "dataio/DataIO.h"
#include "util/RefCount.h"

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

   /** Should return true iff every object of this type has a size that is known at compile time. */
   virtual bool IsFixedSize() const = 0;

   /** Should return the type code identifying this type of object.  */
   virtual uint32 TypeCode() const = 0;

   /** Should return the number of bytes needed to store this object in its current state.  */
   virtual uint32 FlattenedSize() const = 0;

   /** 
    *  Should store this object's state into (buffer). 
    *  @param buffer The bytes to write this object's stat into.  Buffer must be at least FlattenedSize() bytes long.
    */
   virtual void Flatten(uint8 *buffer) const = 0;

   /** 
    *  Should return true iff a buffer with uint32 (code) can be used to reconstruct
    *  this object's state.  Defaults implementation returns true iff (code) equals TypeCode() or B_RAW_DATA.
    *  @param code A type code constant, e.g. B_RAW_TYPE or B_STRING_TYPE, or something custom.
    *  @return True iff this object can Unflatten from a buffer of the given type, false otherwise.
    */
   virtual bool AllowsTypeCode(uint32 code) const {return ((code == B_RAW_TYPE)||(code == TypeCode()));}

   /** 
    *  Should attempt to restore this object's state from the given buffer.
    *  @param buf The buffer of bytes to unflatten from.
    *  @param size Number of bytes in the buffer.
    *  @return B_NO_ERROR if the Unflattening was successful, else B_ERROR.
    */
   virtual status_t Unflatten(const uint8 *buf, uint32 size) = 0;

   /** 
    *  Causes (copyTo)'s state to set from this Flattenable, if possible. 
    *  Default implementation is not very efficient, since it has to flatten
    *  this object into a byte buffer, and then unflatten the bytes back into 
    *  (copyTo).  However, you can override CopyFromImplementation() to provide 
    *  a more efficient implementation when possible.
    *  @param copyTo Object to make into the equivalent of this object.  (copyTo)
    *                May be any subclass of Flattenable.
    *  @return B_NO_ERROR on success, or B_ERROR on failure (typecode mismatch, out-of-memory, etc)
    */
   status_t CopyTo(Flattenable & copyTo) const 
   {
      return (this == &copyTo) ? B_NO_ERROR : ((copyTo.AllowsTypeCode(TypeCode())) ? copyTo.CopyFromImplementation(*this) : B_ERROR);
   }

   /** 
    *  Causes our state to be set from (copyFrom)'s state, if possible. 
    *  Default implementation is not very efficient, since it has to flatten
    *  (copyFrom) into a byte buffer, and then unflatten the bytes back into 
    *  (this).  However, you can override CopyFromImplementation() to provide 
    *  a more efficient implementation when possible.
    *  @param copyFrom Object to read from to set the state of this object.  
    *                  (copyFrom) may be any subclass of Flattenable.
    *  @return B_NO_ERROR on success, or B_ERROR on failure (typecode mismatch, out-of-memory, etc)
    */
   status_t CopyFrom(const Flattenable & copyFrom)
   {
      return (this == &copyFrom) ? B_NO_ERROR : ((AllowsTypeCode(copyFrom.TypeCode())) ? CopyFromImplementation(copyFrom) : B_ERROR);
   }

   /** 
    * Convenience method for writing data into a byte buffer.
    * Writes data consecutively into a byte buffer.  The output buffer is
    * assumed to be large enough to hold the data.
    * @param outBuf Flat buffer to write to
    * @param writeOffset Offset into buffer to write to.  Incremented by (blockSize) on success.
    * @param copyFrom memory location to copy bytes from
    * @param blockSize number of bytes to copy
    */
   static void WriteData(uint8 * outBuf, uint32 * writeOffset, const void * copyFrom, uint32 blockSize)
   {
      memcpy(&outBuf[*writeOffset], copyFrom, blockSize);
      *writeOffset += blockSize;
   };
    
   /** 
    * Convenience method for safely reading bytes from a byte buffer.  (Checks to avoid buffer overrun problems)
    * @param inBuf Flat buffer to read bytes from
    * @param inputBufferBytes total size of the input buffer
    * @param readOffset Offset into buffer to read from.  Incremented by (blockSize) on success.
    * @param copyTo memory location to copy bytes to
    * @param blockSize number of bytes to copy
    * @return B_NO_ERROR if the data was successfully read, B_ERROR if the data couldn't be read (because the buffer wasn't large enough)
    */
   static status_t ReadData(const uint8 * inBuf, uint32 inputBufferBytes, uint32 * readOffset, void * copyTo, uint32 blockSize)
   {
      if ((*readOffset + blockSize) > inputBufferBytes) return B_ERROR;
      memcpy(copyTo, &inBuf[*readOffset], blockSize);
      *readOffset += blockSize;
      return B_NO_ERROR;
   };

   /** Convenience method.  Flattens this object into the supplied ByteBuffer object. 
     * @param outBuf the ByteBuffer to dump our flattened bytes into.  
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t FlattenToByteBuffer(ByteBuffer & outBuf) const;

   /** Convenience method.  Unflattens this object from the supplied ByteBuffer object.
     * @param buf The ByteBuffer to unflatten from.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t UnflattenFromByteBuffer(const ByteBuffer & buf);

   /** Convenience method.  Unflattens this object from the specified byte buffer reference.
     * @param bufRef The ByteBufferRef to unflatten from.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (or if bufRef is a NULL reference)
     */
   status_t UnflattenFromByteBuffer(const ConstRef<ByteBuffer> & bufRef);

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
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
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
     *                       will return B_ERROR instead of trying to read that many bytes.  This can be useful if you
     *                       want to prevent the possibility of huge buffers being allocated due to malicious or corrupted
     *                       size headers.  Defaults to MUSCLE_NO_LIMIT, meaning that no size limit is enforced.
     * @returns B_NO_ERROR if the data was correctly read, or B_ERROR otherwise.
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
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out-of-memory, etc)
    */
   virtual status_t CopyFromImplementation(const Flattenable & copyFrom);
};

/** This class is here to support lightweight subclasses that want to have a Flattenable-like 
  * API (Flatten(), Unflatten(), etc) without incurring the one-word-per-object memory 
  * required by the presence of virtual methods.  To use this class, subclass your
  * class from this one and declare Flatten(), Unflatten(), FlattenedSize(), etc methods
  * in your class, but don't make them virtual.  That will be enough to allow you to
  * use Message::AddFlat(), Message::FindFlat(), etc on your objects, with no extra
  * memory overhead.  See the MUSCLE Point and Rect classes for examples of this technique.
  */ 
class PseudoFlattenable 
{
public:
   /**
    * Dummy implemention of CopyFrom().  It's here only so that Message::FindFlat() will
    * compile when called with a PseudoFlattenable object as an argument.
    * @param copyFrom This parameter is ignored.   
    * @returns B_ERROR always, because given that this object is not a Flattenable object,
    *          it's assumed that it can't receive the state of a Flattenable object either.
    *          (but if that's not the case for your class, your subclass can implement its
    *           own CopyFrom() method to taste)
    */
   status_t CopyFrom(const Flattenable & copyFrom) {(void) copyFrom; return B_ERROR;}
};

/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

}; // end namespace muscle

#endif /* _MUSCLEFLATTENABLE_H */

