/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMultiDataIO_h
#define MuscleMultiDataIO_h

#include "dataio/SeekableDataIO.h"
#include "util/Queue.h"

namespace muscle {
 
/** This DataIO holds a list of one or more other DataIO objects, and passes any
  * calls made to all the sub-DataIOs.  If an error occurs on any of the sub-objects,
  * the call will error out.  This class can be useful when implementing RAID-like behavior.
  */
class MultiDataIO : public SeekableDataIO
{
public:
   /** Default Constructor.  Be sure to add some child DataIOs to our Queue of
     * DataIOs (as returned by GetChildDataIOs()) so that this object will do something useful!
     */
   MultiDataIO() : _absorbPartialErrors(false) {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~MultiDataIO() {/* empty */}

   /** Implemented to read data from the first held DataIO only.  The other DataIOs will
     * have Seek() called on them instead, to simulate a read without actually having
     * to read their data (since we only have one memory-buffer to place it in anyway).
     * @param buffer A buffer to read data into
     * @param size The number of bytes that (buffer) points to
     * @returns the number of bytes read, or -1 on error.
     */
   virtual int32 Read(void * buffer, uint32 size);

   /** Calls Write() on all our held sub-DataIOs. 
     * @param buffer Pointer to a buffer of data to write
     * @param size Number of bytes pointed to by (buffer).
     * @returns The number of bytes written, or -1 on error.  If different sub-DataIOs write different
     *          amounts, the minimum amount written will be returned, and the sub-DataIOs will be Seek()'d
     *          to the position of the sub-DataIO that wrote the fewest bytes.
     */
   virtual int32 Write(const void * buffer, uint32 size);

   /** Calls Seek() on all our held sub-DataIOs.
     * @param offset Byte offset to seek to or by (depending on the next arg)
     * @param whence Set this to IO_SEEK_SET if you want the offset to
     *               be relative to the start of the stream; or to
     *               IO_SEEK_CUR if it should be relative to the current
     *               stream position, or IO_SEEK_END if it should be
     *               relative to the end of the stream.
     * @return B_NO_ERROR on success, or an error code on failure.
     */
   virtual status_t Seek(int64 offset, int whence) {return SeekAll(0, offset, whence);}

   virtual int64 GetPosition() const 
   {
      if (HasChildren() == false) return -1;
      const SeekableDataIO * sdio = dynamic_cast<SeekableDataIO *>(GetFirstChild());
      return sdio ? sdio->GetPosition() : -1;
   }

   virtual uint64 GetOutputStallLimit() const {return HasChildren() ? GetFirstChild()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;}

   virtual void FlushOutput();

   virtual void Shutdown() {_childIOs.Clear();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return HasChildren() ? GetFirstChild()->GetReadSelectSocket()  : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return HasChildren() ? GetFirstChild()->GetWriteSelectSocket() : GetNullSocket();}

   virtual status_t GetReadByteTimeStamp(int32 whichByte, uint64 & retStamp) const {return HasChildren() ? GetFirstChild()->GetReadByteTimeStamp(whichByte, retStamp) : B_BAD_OBJECT;}

   virtual bool HasBufferedOutput() const;
   virtual void WriteBufferedOutput();

   /** Returns a read-only reference to our list of child DataIO objects. */
   const Queue<DataIORef> & GetChildDataIOs() const {return _childIOs;}

   /** Returns a read/write reference to our list of child DataIO objects. */
   Queue<DataIORef> & GetChildDataIOs() {return _childIOs;}

   /** Sets whether an error condition in a child should be handled simply by removing the child,
     * or whether the error should be immediately propagated upwards.  Default value is false.
     * @param ape If true, any child DataIO object that reports an error will be silently removed
     *            from the list of child DataIOs.  No errors will be reported from this MultiDataIO
     *            object unless/until the child DataIO list is reduced to a single child DataIO, and 
     *            that child DataIO errors out.  If false (the default state), then any error from 
     *            any child DataIO will cause this MultiDataIO object to return an error immediately.
     */
   void SetAbsorbPartialErrors(bool ape) {_absorbPartialErrors = ape;}

   /** Returns true iff the absorb-partial-errors flag has been set. */
   bool IsAbsorbPartialErrors() const {return _absorbPartialErrors;}

private:
   bool HasChildren() const {return (_childIOs.HasItems());}
   DataIO * GetFirstChild() const {return (_childIOs.Head()());}
   status_t SeekAll(uint32 first, int64 offset, int whence);

   Queue<DataIORef> _childIOs;
   bool _absorbPartialErrors;

   DECLARE_COUNTED_OBJECT(MultiDataIO);
};
DECLARE_REFTYPES(MultiDataIO);

} // end namespace muscle

#endif
