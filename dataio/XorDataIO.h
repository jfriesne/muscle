/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleXorDataIO_h
#define MuscleXorDataIO_h

#include "dataio/DataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/** This DataIO is a "wrapper" DataIO that adds an XOR operation to any data
  * that it reads or writes before passing the call on to the DataIO that it
  * holds internally.  This can be useful if you want to obfuscate your data
  * a little bit before sending it out to disk or over the network.
  */
class XorDataIO : public DataIO, public CountedObject<XorDataIO>
{
public:
   /** Default Constructor.  Be sure to set a child dataIO with SetChildDataIO()
     * before using this DataIO, so that this object will do something useful!
     */
   XorDataIO() {/* empty */}

   /** Constructor.
     * @param childIO Reference to the DataIO to pass calls on through to
     *                after the data has been XOR'd.
     */
   XorDataIO(const DataIORef & childIO) : _childIO(childIO) {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~XorDataIO() {/* empty */}

   /** Implemented to XOR the child DataIO's read bytes before returning.  */
   virtual int32 Read(void * buffer, uint32 size)
   {
      int32 ret = _childIO() ? _childIO()->Read(buffer, size) : -1;
      if (ret > 0) XorCopy(buffer, buffer, size);
      return ret;
   }

   /** Implemented to pass XOR's bytes to the child DataIO's Write() method. */
   virtual int32 Write(const void * buffer, uint32 size)
   {
      if ((_childIO() == NULL)||(_tempBuf.SetNumBytes(buffer, size) != B_NO_ERROR)) return B_ERROR;
      XorCopy(_tempBuf.GetBuffer(), buffer, size);
      return _childIO()->Write(_tempBuf.GetBuffer(), size);
   }

   virtual status_t Seek(int64 offset, int whence) {return _childIO() ? _childIO()->Seek(offset, whence) : B_ERROR;}
   virtual int64 GetPosition() const {return _childIO() ? _childIO()->GetPosition() : -1;}
   virtual uint64 GetOutputStallLimit() const {return _childIO() ? _childIO()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;}
   virtual void FlushOutput() {if (_childIO()) _childIO()->FlushOutput();}
   virtual void Shutdown() {if (_childIO()) _childIO()->Shutdown(); _childIO.Reset();}
   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _childIO() ? _childIO()->GetReadSelectSocket()  : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _childIO() ? _childIO()->GetWriteSelectSocket() : GetNullSocket();}
   virtual status_t GetReadByteTimeStamp(int32 whichByte, uint64 & retStamp) const {return _childIO() ? _childIO()->GetReadByteTimeStamp(whichByte, retStamp) : B_ERROR;}

   virtual bool HasBufferedOutput() const {return _childIO() ? _childIO()->HasBufferedOutput() : false;}
   virtual void WriteBufferedOutput() {if (_childIO()) _childIO()->WriteBufferedOutput();}
   virtual uint32 GetPacketMaximumSize() const {return (_childIO()) ? _childIO()->GetPacketMaximumSize() : 0:}

   /** Returns a reference to our held child DataIO (if any) */
   const DataIORef & GetChildDataIO() const {return _childIO;}

   /** Sets our current held child DataIO. */
   void SetChildDataIO(const DataIORef & childDataIO) {_childIO = childDataIO;}

private:
   void XorCopy(const void * to, const void * from, uint32 numBytes)
   {
      // Do the bulk of the copy a word at a time, for faster speed
      unsigned long * uto = (unsigned long *) to;
      const unsigned long * ufrom = (const unsigned long *) from;
      for (int32 i=(numBytes/sizeof(unsigned long))-1; i>=0; i--) *uto++ = ~(*ufrom++);

      // The last few bytes we have to do a byte a time though
      uint8 * cto = (uint8 *) uto;
      const uint8 * cfrom = (const uint8 *) ufrom;
      for (int32 i=(numBytes%sizeof(unsigned long))-1; i>=0; i--) *cto++ = ~(*cfrom++);
   }

   DataIORef _childIO;
   ByteBuffer _tempBuf;   // holds the XOR'd bytes temporarily for us
};

}; // end namespace muscle

#endif
