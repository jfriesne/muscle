/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePacketDataIO_h
#define MusclePacketDataIO_h

#include "dataio/DataIO.h"
#include "util/IPAddress.h"

namespace muscle {
 
/** Abstract base class for DataIO objects that represent packet-based I/O objects
  * (i.e. for UDP sockets, or objects that can act like UDP sockets)
  */
class PacketDataIO : public virtual DataIO
{
public:
   PacketDataIO() {/* empty */}
   virtual ~PacketDataIO() {/* empty */}

   /**
    * Should be implemented to return the maximum number of bytes that
    * can fit into a single packet.  Used by the I/O gateways e.g. to
    * determine how much memory to allocate before Read()-ing a packet of data in.
    */
   virtual uint32 GetMaximumPacketSize() const = 0;

   /** For packet-oriented subclasses, this method may be overridden
     * to return the IPAddressAndPort that the most recently Read()
     * packet came from.
     * The default implementation returns the IPAddressAndPort most recently passed 
     * to our SetSourceOfLastReadPacket() method, or an invalid IPAddressAndPort if
     * SetSourceOfLastReadPacket() has never been called.
     */
   virtual const IPAddressAndPort & GetSourceOfLastReadPacket() const {return _lastPacketReceivedFrom;}

   /** For packet-oriented subclasses, this method may be overridden
     * to return the IPAddressAndPort that outgoing packets will be
     * sent to (by default).
     * The default implementation returns a default/invalid IPAddressAndPort.
     */
   virtual const IPAddressAndPort & GetPacketSendDestination() const = 0;

   /** For packet-oriented subclasses, this method may be overridden
     * to set/change the IPAddressAndPort that outgoing packets will
     * be sent to (by default).
     * @param iap The new default address-and-port to send outgoing packets to.
     * @returns B_NO_ERROR if the operation was successful, or an error code if it failed.
     */
   virtual status_t SetPacketSendDestination(const IPAddressAndPort & iap) = 0;

   /** Default implementation of PacketDataIO::Read() 
    *  which just calls ReadFrom(buffer, size, GetSourceOfLastReadPacket()).
    *  @param buffer Buffer to write the bytes into
    *  @return Number of bytes read, or -1 on error.   
    *  @param size Number of bytes in the buffer.
    */ 
   virtual int32 Read(void * buffer, uint32 size)
   {
      return ReadFrom(buffer, size, GetWritableSourceOfLastReadPacket());
   }

   /** Default implementation of PacketDataIO::Write() 
    *  which just calls WriteTo(buffer, size, GetPacketSendDestination()).
    *  @param buffer Buffer of bytes to be sent.
    *  @param size Number of bytes to send. 
    *  @return Number of bytes sent, or -1 on error.        
    */
   virtual int32 Write(const void * buffer, uint32 size)
   {
      return WriteTo(buffer, size, GetPacketSendDestination());
   }

   /** Tries to the data from an incoming packet into (buffer).  Returns the
    *  actual number of bytes placed, or a negative value if there was an error.
    *  Default implementation calls Read(), and then calls GetSourceOfLastReadPacket()
    *  to fill out the (retPacketSource argument).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @param retPacketSource on success, the incoming packet's source is placed here.
    *  @return Number of bytes read, or -1 on error.   
    */
   virtual int32 ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource) = 0;

   /** Tries to send a packet of data to the specified location.
    *  Returns the actual number of sent, or a negative value if 
    *  there was an error.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @param packetDest The destination that the packet should be sent to.
    *  @return Number of bytes written, or -1 on error.        
    */
   virtual int32 WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest) = 0;

protected:
   /** Set the value that should be returned by our GetSourceOfLastReadPacket() method.
     * This method should typically be called from the subclasses Read() or ReadFrom() methods.
     * @param packetSource the IPAddressAndPort value indicating where the most recently received packet came from.
     */
   void SetSourceOfLastReadPacket(const IPAddressAndPort & packetSource) {_lastPacketReceivedFrom = packetSource;}

   /** Returns a read/write reference to our source-of-last-read-packet field. */
   IPAddressAndPort & GetWritableSourceOfLastReadPacket() {return _lastPacketReceivedFrom;}

private:
   IPAddressAndPort _lastPacketReceivedFrom;
};
DECLARE_REFTYPES(PacketDataIO);

} // end namespace muscle

#endif
