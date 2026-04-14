/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ByteBufferRefAndIPAddressAndPort_h
#define ByteBufferRefAndIPAddressAndPort_h

#include "util/ByteBuffer.h"
#include "util/IPAddress.h"

namespace muscle {

/** Just a dumb class to bundle a ConstByteBufferRef and an IPAddressAndPort together */
class ConstByteBufferRefAndIPAddressAndPort
{
public:
   /** Default constructor */
   ConstByteBufferRefAndIPAddressAndPort() {/* empty */}

   /** Explicit constructor
    *  @param buf the ByteBufferRef to store (data to send or space for data to be received into)
    *  @param iap the IPAddressAndPort associated with this byte buffer
    */
   ConstByteBufferRefAndIPAddressAndPort(const ConstByteBufferRef & buf, const IPAddressAndPort & iap) : _buf(buf), _iap(iap) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   ConstByteBufferRefAndIPAddressAndPort(const ConstByteBufferRefAndIPAddressAndPort & rhs) : _buf(rhs._buf), _iap(rhs._iap) {/* empty */}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator ==(const ConstByteBufferRefAndIPAddressAndPort & rhs) const {return ((_buf == rhs._buf)&&(_iap == rhs._iap));}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator !=(const ConstByteBufferRefAndIPAddressAndPort & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   ConstByteBufferRefAndIPAddressAndPort & operator = (const ConstByteBufferRefAndIPAddressAndPort & rhs) {_buf = rhs._buf; _iap = rhs._iap; return *this;}

   /** Returns the ConstByteBuffer argument passed into the constructor */
   const ConstByteBufferRef & GetConstByteBufferRef() const {return _buf;}

   /** Returns the IPAddressAndPort argument passed into the constructor */
   const IPAddressAndPort & GetIPAddressAndPort() const {return _iap;}

private:
   ConstByteBufferRef _buf;
   IPAddressAndPort _iap;
};

/** Just a dumb class to bundle a ByteBufferRef and an IPAddressAndPort together.  Same as ConstByteBufferRefAndIPAddressAndPort except the ByteBufferRef is not read-only  */
class ByteBufferRefAndIPAddressAndPort : public ConstByteBufferRefAndIPAddressAndPort
{
public:
   /** Default constructor */
   ByteBufferRefAndIPAddressAndPort() {/* empty */}

   /** Explicit constructor
    *  @param buf the ByteBufferRef to store (data to send or space for data to be received into)
    *  @param iap the IPAddressAndPort associated with this byte buffer
    */
   ByteBufferRefAndIPAddressAndPort(const ByteBufferRef & buf, const IPAddressAndPort & iap) : ConstByteBufferRefAndIPAddressAndPort(buf, iap) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   ByteBufferRefAndIPAddressAndPort(const ByteBufferRefAndIPAddressAndPort & rhs) : ConstByteBufferRefAndIPAddressAndPort(rhs) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   ByteBufferRefAndIPAddressAndPort & operator = (const ByteBufferRefAndIPAddressAndPort & rhs)
   {
      _scratchBufRef.Reset();
      return static_cast<ByteBufferRefAndIPAddressAndPort &>(ConstByteBufferRefAndIPAddressAndPort::operator=(rhs));
   }

   /** Returns the ByteBuffer argument passed into the constructor */
   const ByteBufferRef & GetByteBufferRef() const
   {
      _scratchBufRef = CastAwayConstFromRef(GetConstByteBufferRef());  // always do this, in case base-class-operator=() changed the buffer on us
      return _scratchBufRef;
   }

private:
   mutable ByteBufferRef _scratchBufRef;  // demand-allocated
};

} // end namespace muscle

#endif
