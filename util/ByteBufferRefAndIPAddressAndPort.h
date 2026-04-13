/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ByteBufferRefAndIPAddressAndPort_h
#define ByteBufferRefAndIPAddressAndPort_h

#include "util/ByteBuffer.h"
#include "util/IPAddress.h"

namespace muscle {

/** Just a dumb class to bundle a ByteBufferRef and an IPAddressAndPort together */
class ByteBufferRefAndIPAddressAndPort
{
public:
   /** Default constructor */
   ByteBufferRefAndIPAddressAndPort() {/* empty */}

   /** Explicit constructor
    *  @param buf the ByteBufferRef to store (data to send or space for data to be received into)
    *  @param iap the IPAddressAndPort associated with this byte buffer
    */
   ByteBufferRefAndIPAddressAndPort(const ByteBufferRef & buf, const IPAddressAndPort & iap) : _buf(buf), _iap(iap) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   ByteBufferRefAndIPAddressAndPort(const ByteBufferRefAndIPAddressAndPort & rhs) : _buf(rhs._buf), _iap(rhs._iap) {/* empty */}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator ==(const ByteBufferRefAndIPAddressAndPort & rhs) const {return ((_buf == rhs._buf)&&(_iap == rhs._iap));}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator !=(const ByteBufferRefAndIPAddressAndPort & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   ByteBufferRefAndIPAddressAndPort & operator = (const ByteBufferRefAndIPAddressAndPort & rhs) {_buf = rhs._buf; _iap = rhs._iap; return *this;}

   /** Returns the ByteBuffer argument passed into the constructor */
   const ByteBufferRef & GetByteBufferRef() const {return _buf;}

   /** Returns the IPAddressAndPort argument passed into the constructor */
   const IPAddressAndPort & GetIPAddressAndPort() const {return _iap;}

private:
   ByteBufferRef _buf;
   IPAddressAndPort _iap;
};

} // end namespace muscle

#endif
