/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFlipBitsProxyDataIO_h
#define MuscleFlipBitsProxyDataIO_h

#include "dataio/ProxyDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

/** This DataIO is a "wrapper" DataIO that adds a bitwise-NOT operation to any data
  * that it reads or writes before passing the call on to the DataIO that it
  * holds internally.  This can be useful if you want to obfuscate your data
  * a little bit before sending it out to disk or over the network.
  */
class FlipBitsProxyDataIO : public ProxyDataIO
{
public:
   /** Default Constructor.  Be sure to set a child dataIO with SetChildDataIO()
     * before using this DataIO, so that this object will do something useful!
     */
   FlipBitsProxyDataIO() {/* empty */}

   /** Constructor.
     * @param childIO Reference to the DataIO to pass calls on through to
     *                after the data has been NOT'd.
     */
   FlipBitsProxyDataIO(const DataIORef & childIO) : ProxyDataIO(childIO) {/* empty */}

   /** Virtual destructor, to keep C++ honest.  */
   virtual ~FlipBitsProxyDataIO() {/* empty */}

   /** Implemented to use the child DataIO object to read in some bytes, and then
     * un-NOT the read bytes before returning them to the caller.
     * @copydoc ProxyDataIO::Read(void *, uint32)
     */
   virtual io_status_t Read(void * buffer, uint32 size)
   {
      const io_status_t ret = ProxyDataIO::Read(buffer, size);
      if (ret.GetByteCount() > 0) BitwiseNotMemCpy(buffer, buffer, ret.GetByteCount());
      return ret;
   }

   /** Implemented to make an NOT'd copy of the passed-in bytes, and then use
     * the child DataIO object to transmit the NOT'd bytes.
     * @copydoc ProxyDataIO::Write(const void *, uint32)
     */
   virtual io_status_t Write(const void * buffer, uint32 size)
   {
      MRETURN_ON_ERROR(_tempBuf.SetNumBytes(size, false));
      BitwiseNotMemCpy(_tempBuf.GetBuffer(), buffer, size);
      return ProxyDataIO::Write(_tempBuf.GetBuffer(), size);
   }

   /** Implemented to use the child DataIO object to read in some bytes, and then
     * un-NOT the read bytes before returning them to the caller.
     * @copydoc ProxyDataIO::ReadFrom(void *, uint32, IPAddressAndPort &)
     */
   virtual io_status_t ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource)
   {
      const io_status_t ret = ProxyDataIO::ReadFrom(buffer, size, retPacketSource);
      if (ret.GetByteCount() > 0) BitwiseNotMemCpy(buffer, buffer, ret.GetByteCount());
      return ret;
   }

   /** Implemented to make an NOT'd copy of the passed-in bytes, and then use
     * the child DataIO object to transmit the NOT'd bytes.
     * @copydoc ProxyDataIO::WriteTo(const void *, uint32, const IPAddressAndPort &)
     */
   virtual io_status_t WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
   {
      MRETURN_ON_ERROR(_tempBuf.SetNumBytes(size, false));
      BitwiseNotMemCpy(_tempBuf.GetBuffer(), buffer, size);
      return ProxyDataIO::WriteTo(_tempBuf.GetBuffer(), size, packetDest);
   }

private:
   void BitwiseNotMemCpy(void * to, const void * from, uint32 numBytes) const
   {
      uint8 * cto = (uint8 *) to;
      const uint8 * cfrom = (const uint8 *) from;
      for (uint32 i=0; i<numBytes; i++) cto[i] = (uint8) (~cfrom[i]);
   }

   ByteBuffer _tempBuf;   // holds the NOT'd bytes temporarily for us

   DECLARE_COUNTED_OBJECT(FlipBitsProxyDataIO);
};
DECLARE_REFTYPES(FlipBitsProxyDataIO);

} // end namespace muscle

#endif
