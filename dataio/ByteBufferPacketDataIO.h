/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleByteBufferPacketDataIO_h
#define MuscleByteBufferPacketDataIO_h

#include "dataio/PacketDataIO.h"
#include "util/ByteBuffer.h"
#include "util/NetworkUtilityFunctions.h"  // for MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET

namespace muscle {

/**
 *  Data I/O class to allow reading from/writing to a Queue of ByteBuffer objects (as if it was a packet I/O device)
 *  The ByteBuffer will behave much like a packet-device (e.g. UDP socket) would, except that
 *  the each "packet" is actually a ByteBuffer rather than a network-buffer.
 */
class ByteBufferPacketDataIO : public PacketDataIO
{
public:
   /** Constructor.  Creates a ByteBufferPacketDataIO with no packets ready to read.
    *  @param maxPacketSize the maximum support packet-size, in bytes.  Should be greater than zero.  Defaults
    *                       to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes)
    */
   ByteBufferPacketDataIO(uint32 maxPacketSize = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);

   /** Constructor.
    *  @param buf A ByteBuffer to that will be read by the next call to ReadFrom().
    *  @param fromIAP the IP address and port the "packet" is purporting to be from.  Defaults to an invalid IPAddressAndPort.
    *  @param maxPacketSize the maximum support packet-size, in bytes.  Should be greater than zero.  Defaults
    *                       to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes)
    */
   ByteBufferPacketDataIO(const ByteBufferRef & buf, const IPAddressAndPort & fromIAP = IPAddressAndPort(), uint32 maxPacketSize = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);

   /** Constructor.
    *  @param bufs A list of ByteBuffers to be read by subsequent calls to ReadFrom().
    *  @param fromIAP the IP address and port the "packets" are purporting to be from.  Defaults to an invalid IPAddressAndPort.
    *  @param maxPacketSize the maximum support packet-size, in bytes.  Should be greater than zero.  Defaults
    *                       to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes)
    */
   ByteBufferPacketDataIO(const Queue<ByteBufferRef> & bufs, const IPAddressAndPort & fromIAP = IPAddressAndPort(), uint32 maxPacketSize = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);

   /** Constructor.
    *  @param bufs A sequence of ByteBuffers to be read by subsequent calls to ReadFrom(), and their associated IPAddressAndPort values.
    *  @param maxPacketSize the maximum support packet-size, in bytes.  Should be greater than zero.  Defaults
    *                       to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes)
    */
   ByteBufferPacketDataIO(const Hashtable<ByteBufferRef, IPAddressAndPort> & bufs, uint32 maxPacketSize = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET);

   /** Virtual Destructor, to keep C++ honest */
   virtual ~ByteBufferPacketDataIO();

   /** Clears our list of byte-buffers that we are planning to hand to future callers of our Read() methods. */
   void ClearBuffersToRead() {_bufsToRead.Clear();}

   /** Convenience method.  Sets our buffers-list to a list containing only the single ByteBufferRef.
     * @param buf a ByteBufferRef to hold, to be handed to the next future caller of one of our Read() methods, or a NULL reference to not hold anything.
     * @param fromIAP the IP address and port the "packet" is purporting to be from.  Defaults to an invalid IPAddressAndPort.
     */
   void SetBuffersToRead(const ByteBufferRef & buf, const IPAddressAndPort & fromIAP = IPAddressAndPort()) {_bufsToRead.Clear(); (void) _bufsToRead.Put(buf, fromIAP);}

   /** Convenience method:  Sets our set of buffers to a list, with implicit invalid IPAddressAndPort values.
     * @param bufs a new list of buffers for this object to hold.  They will be handed to future callers of our Read() methods, in order.
     * @param fromIAP the IP address and port the "packets" are purporting to be from.  Defaults to an invalid IPAddressAndPort.
     */
   void SetBuffersToRead(const Queue<ByteBufferRef> & bufs, const IPAddressAndPort & fromIAP = IPAddressAndPort());

   /** Sets the current sequence of byte-buffers we are holding.  Keys are the ByteBuffers themselves; values are the
     * IPAddressAndPort associated with each byte-buffer.
     * @param bufs a new set of buffers for this object to hold.  They will be handed to future callers of our Read() methods, in order.
     */
   void SetBuffersToRead(const Hashtable<ByteBufferRef, IPAddressAndPort> & bufs) {_bufsToRead = bufs;}

   /** Returns the current sequence of byte-buffers we are holding, to be handed to future callers of our Read() methods, in order.
     * @note Keys are the ByteBuffers themselves; values are the IPAddressAndPort associated with each byte-buffer.
     */
   const Hashtable<ByteBufferRef, IPAddressAndPort> & GetBuffersToRead() const {return _bufsToRead;}

   /** Non-const version of GetBuffersToRead(). */
   Hashtable<ByteBufferRef, IPAddressAndPort> & GetBuffersToRead() {return _bufsToRead;}

   /** Clears our list of "sent" buffers */
   void ClearWrittenBuffers() {_writtenBufs.Clear();}

   /** Returns the current sequence of "sent" byte-buffers we are holding.  Keys are the ByteBuffers themselves;
     * values are the IPAddressAndPort passed in to WriteTo() for each byte-buffer.
     */
   const Hashtable<ByteBufferRef, IPAddressAndPort> & GetWrittenBuffers() const {return _writtenBufs;}

   /** Non-const version of GetWrittenBuffers(). */
   Hashtable<ByteBufferRef, IPAddressAndPort> & GetWrittenBuffers() {return _writtenBufs;}

   /** Disable us! */
   virtual void Shutdown() {_bufsToRead.Clear(); _writtenBufs.Clear();}

   /** Can't select on this one, sorry -- returns GetNullSocket() */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Can't select on this one, sorry -- returns GetNullSocket() */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

   /** Returns the maxPacketSize value that was passed in to our constructor */
   virtual uint32 GetMaximumPacketSize() const {return _maxPacketSize;}

   virtual int32 ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource);
   virtual int32 WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest);

   /** implemented as a no-op. */
   virtual void FlushOutput() {/* empty */}

   virtual const IPAddressAndPort & GetPacketSendDestination() const {return _packetSendDestination;}
   virtual status_t SetPacketSendDestination(const IPAddressAndPort & iap) {_packetSendDestination = iap; return B_NO_ERROR;}

private:
   Hashtable<ByteBufferRef, IPAddressAndPort> _bufsToRead;
   Hashtable<ByteBufferRef, IPAddressAndPort> _writtenBufs;
   const uint32 _maxPacketSize;

   IPAddressAndPort _packetSendDestination;

   DECLARE_COUNTED_OBJECT(ByteBufferPacketDataIO);
};
DECLARE_REFTYPES(ByteBufferPacketDataIO);

} // end namespace muscle

#endif
