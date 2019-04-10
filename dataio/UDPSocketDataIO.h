/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleUDPSocketDataIO_h
#define MuscleUDPSocketDataIO_h

#include "dataio/PacketDataIO.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

/**
 *  Data I/O to and from a UDP socket! 
 */
class UDPSocketDataIO : public PacketDataIO
{
public:
   /**
    *  Constructor.
    *  @param sock The socket to use.
    *  @param blocking specifies whether to use blocking or non-blocking socket I/O.
    *  If you will be using this object with a AbstractMessageIOGateway,
    *  and/or select(), then it's usually better to set blocking to false.
    */
   UDPSocketDataIO(const ConstSocketRef & sock, bool blocking) : _sock(sock), _maxPacketSize(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET)
   {
      (void) SetBlockingIOEnabled(blocking);
      _sendTo.AddTail();  // so that by default, Write() will just call send() on our socket
   }

   /** Destructor.
    *  Closes the socket descriptor, if necessary.
    */
   virtual ~UDPSocketDataIO() 
   {
      // empty
   }

   virtual int32 ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retSource) 
   {
      IPAddress tmpAddr = invalidIP;
      uint16 tmpPort = 0;
      const int32 ret = ReceiveDataUDP(_sock, buffer, size, _blocking, &tmpAddr, &tmpPort);
      retSource.Set(tmpAddr, tmpPort);
      SetSourceOfLastReadPacket(retSource);  // in case this is a direct call e.g. from the gateway code
      return ret;
   }

   virtual int32 Write(const void * buffer, uint32 size) 
   {
      // This method is overridden to support multiple destinations too
      bool sawErrors  = false;
      bool sawSuccess = false;
      int32 ret = 0;
      for (uint32 i=0; i<_sendTo.GetNumItems(); i++) 
      {
         const int32 numBytesSent = WriteTo(buffer, size, _sendTo[i]);
         if (numBytesSent >= 0) sawSuccess = true;
                           else sawErrors  = true;
         ret = muscleMax(numBytesSent, ret);
      }
      return ((sawErrors)&&(sawSuccess == false)) ? -1 : ret;
   }

   virtual int32 WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest) 
   {
      return SendDataUDP(_sock, buffer, size, _blocking, packetDest.GetIPAddress(), packetDest.GetPort());
   }

   /** Implemented as a no-op:  UDP sockets are always flushed immediately anyway */
   virtual void FlushOutput() {/* empty */}
   
   /** Overridden to return the maximum packet size of a UDP packet.
     * Defaults to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes),
     * but the returned value can be changed via SetPacketMaximumSize().
     */
   virtual uint32 GetMaximumPacketSize() const {return _maxPacketSize;}

   /** This can be called to change the maximum packet size value returned
     * by GetMaximumPacketSize().  You might call this e.g. if you are on a network
     * that supports Jumbo UDP packets and want to take advantage of that.
     * @param maxPacketSize the new maximum packet size, in bytes
     */
   void SetPacketMaximumSize(uint32 maxPacketSize) {_maxPacketSize = maxPacketSize;}

   /**
    * Closes our socket connection
    */
   virtual void Shutdown() {_sock.Reset();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _sock;}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _sock;}

   /** Call this to make our Write() method use sendto() with the specified
     * destination address and port.  Calling this with (invalidIP, 0) will
     * revert us to our default behavior of just calling() send on our UDP socket.
     * @param dest the new destination address (and port) that Write() should send UDP packets to
     */
   virtual status_t SetPacketSendDestination(const IPAddressAndPort & dest) {(void) _sendTo.EnsureSize(1, true); _sendTo.Head() = dest; return B_NO_ERROR;}

   /** Returns the IP address and port that Write() will send to, e.g. as was
     * previously specified in SetPacketSendDestination().
     */
   virtual const IPAddressAndPort & GetPacketSendDestination() const {return _sendTo.HasItems() ? _sendTo.Head() : _sendTo.GetDefaultItem();}

   /** Call this to make our Write() method use sendto() with the specified destination addresss and ports.
     * @param dests a list of IPAddressAndPort destinations; each Write() call will send a UDP packet to each destination in the list.
     */
   void SetPacketSendDestinations(const Queue<IPAddressAndPort> & dests) {_sendTo = dests;}

   /** Returns read/write access to our list of send-destinations. */
   Queue<IPAddressAndPort> & GetPacketSendDestinations() {return _sendTo;}

   /** Returns read-only access to our list of send-destinations. */
   const Queue<IPAddressAndPort> & GetPacketSendDestinations() const {return _sendTo;}

   /**
    * Enables or diables blocking I/O on this socket.
    * If this object is to be used by an AbstractMessageIOGateway,
    * then non-blocking I/O is usually better to use.
    * @param blocking If true, socket is set to blocking I/O mode.  Otherwise, non-blocking I/O.
    * @return B_NO_ERROR on success, B_ERROR on error.
    */
   status_t SetBlockingIOEnabled(bool blocking)
   {
      const status_t ret = SetSocketBlockingEnabled(_sock, blocking);
      if (ret == B_NO_ERROR) _blocking = blocking;
      return ret;
   }

   /** Returns true iff our socket is set to use blocking I/O (as specified in
    *  the constructor or in our SetBlockingIOEnabled() method)
    */
   bool IsBlockingIOEnabled() const {return _blocking;}

private:
   ConstSocketRef _sock;
   bool _blocking;

   Queue<IPAddressAndPort> _sendTo;
   uint32 _maxPacketSize;

   DECLARE_COUNTED_OBJECT(UDPSocketDataIO);
};
DECLARE_REFTYPES(UDPSocketDataIO);

} // end namespace muscle

#endif
