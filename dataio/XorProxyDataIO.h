/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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

   /** Implemented to use the child DataIO object to read in some bytes, and then
     * un-XOR the read bytes before returning them to the caller.
     * @copydoc ProxyDataIO::Read(void *, uint32)
     */
   virtual io_status_t Read(void * buffer, uint32 size)
   {
      const io_status_t ret = ProxyDataIO::Read(buffer, size);
      if (ret.GetByteCount() > 0) XorMemCpy(buffer, buffer, ret.GetByteCount());
      return ret;
   }

   /** Implemented to make an XOR'd copy of the passed-in bytes, and then use
     * the child DataIO object to transit the XOR'd bytes.
     * @copydoc ProxyDataIO::Write(const void *, uint32)
     */
   virtual io_status_t Write(const void * buffer, uint32 size)
   {
      if (GetChildDataIO()() == NULL) return B_BAD_OBJECT;
      MRETURN_ON_ERROR(_tempBuf.SetNumBytes(size, false));

      XorMemCpy(_tempBuf.GetBuffer(), buffer, size);
      return ProxyDataIO::Write(_tempBuf.GetBuffer(), size);
   }

private:
   void XorMemCpy(const void * to, const void * from, uint32 numBytes) const
   {
      uint8 * cto = (uint8 *) to;
      const uint8 * cfrom = (const uint8 *) from;
      for (uint32 i=0; i<numBytes; i++) cto[i] = ~cfrom[i];
   }

   ByteBuffer _tempBuf;   // holds the XOR'd bytes temporarily for us

   DECLARE_COUNTED_OBJECT(XorProxyDataIO);
};
DECLARE_REFTYPES(XorProxyDataIO);

} // end namespace muscle

#endif
