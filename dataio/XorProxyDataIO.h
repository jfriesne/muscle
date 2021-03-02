/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleXorProxyDataIO_h
#define MuscleXorProxyDataIO_h

#include "dataio/ProxyDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {
 
/** This DataIO is a "wrapper" DataIO that adds an XOR operation to any data
  * that it reads or writes before passing the call on to the DataIO that it
  * holds internally.  This can be useful if you want to obfuscate your data
  * a little bit before sending it out to disk or over the network.
  */
class XorProxyDataIO : public ProxyDataIO
{
public:
   /** Default Constructor.  Be sure to set a child dataIO with SetChildDataIO()
     * before using this DataIO, so that this object will do something useful!
     */
   XorProxyDataIO() {/* empty */}

   /** Constructor. 
     * @param childIO Reference to the DataIO to pass calls on through to
     *                after the data has been XOR'd.
     */
   XorProxyDataIO(const DataIORef & childIO) : ProxyDataIO(childIO) {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~XorProxyDataIO() {/* empty */}

   /** Implemented to XOR the child DataIO's read bytes before returning.
     * @copydoc ProxyDataIO::Read(void *, uint32)
     */
   virtual int32 Read(void * buffer, uint32 size)
   {
      const int32 ret = ProxyDataIO::Read(buffer, size);
      if (ret > 0) XorCopy(buffer, buffer, size);
      return ret;
   }

   /** Implemented to pass XOR's bytes to the child DataIO's Write() method.
     * @copydoc ProxyDataIO::Write(const void *, uint32)
     */
   virtual int32 Write(const void * buffer, uint32 size)
   {
      if ((GetChildDataIO()() == NULL)||(_tempBuf.SetNumBytes(size, buffer).IsError())) return -1;
      XorCopy(_tempBuf.GetBuffer(), buffer, size);
      return ProxyDataIO::Write(_tempBuf.GetBuffer(), size);
   }

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

   ByteBuffer _tempBuf;   // holds the XOR'd bytes temporarily for us

   DECLARE_COUNTED_OBJECT(XorProxyDataIO);
};
DECLARE_REFTYPES(XorProxyDataIO);

} // end namespace muscle

#endif
