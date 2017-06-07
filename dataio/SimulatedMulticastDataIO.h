/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSimulatedMulticastDataIO_h
#define MuscleSimulatedMulticastDataIO_h

#include "dataio/UDPSocketDataIO.h"
#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

/**
 *  This is a special-purpose type of DataIO meant to simulate multicast semantics while
 *  minimizing the actual transmission of multicast packets on the network by sending the data
 *  via unicast instead.  This is particularly useful on WiFi networks, which are extremely 
 *  inefficient at delivering actual multicast traffic.
 *
 *  On wired networks you are probably better off using actual multicast packets instead,
 *  since wired networks handle multicast traffic much more elegantly.
 */
class SimulatedMulticastDataIO : public DataIO, private Thread, private CountedObject<SimulatedMulticastDataIO>
{
public:
   /**
    *  Constructor.
    *  @param userMulticastAddress The multicast address we want to listen to and simulate (without actually transmitting on)
    */
   SimulatedMulticastDataIO(const IPAddressAndPort & userMulticastAddress);

   /** Destructor.  Shuts down the internal thread and cleans up. */
   virtual ~SimulatedMulticastDataIO() {ShutdownAux();}

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual const ConstSocketRef & GetReadSelectSocket()  const {return const_cast<SimulatedMulticastDataIO &>(*this).GetOwnerWakeupSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return const_cast<SimulatedMulticastDataIO &>(*this).GetOwnerWakeupSocket();}
   virtual const IPAddressAndPort & GetSourceOfLastReadPacket() const {return _lastPacketReceivedFrom;}
   virtual void Shutdown() {ShutdownAux();}

   /** This method implementation always returns B_ERROR, because you can't seek on a socket!  */
   virtual status_t Seek(int64 /*seekOffset*/, int /*whence*/) {return B_ERROR;}

   /** Always returns -1, since a socket has no position to speak of */
   virtual int64 GetPosition() const {return -1;}

   /** Implemented as a no-op:  UDP sockets are always flushed immediately anyway */
   virtual void FlushOutput() {/* empty */}
   
   /** Overridden to return the maximum packet size of a UDP packet.
     * Defaults to MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka 1388 bytes),
     * but the returned value can be changed via SetPacketMaximumSize().
     */
   virtual uint32 GetPacketMaximumSize() const {return _maxPacketSize;}

   /** This can be called to change the maximum packet size value returned
     * by GetPacketMaximumSize().  You might call this e.g. if you are on a network
     * that supports Jumbo UDP packets and want to take advantage of that.
     */
   void SetPacketMaximumSize(uint32 maxPacketSize) {_maxPacketSize = maxPacketSize;}

   /** Returns the multicast address that was passed in to our constructor. */
   const IPAddressAndPort & GetSendDestination() const {return _userMulticastAddress;}

protected:
   virtual void InternalThreadEntry();

private:
   void ShutdownAux();
   status_t ReadPacket(DataIO & dio, ByteBufferRef & retBuf);
   status_t SendIncomingDataPacketToMainThread(const ByteBufferRef & data, const IPAddressAndPort & source);
   void UpdateIsInternalSocketRegisteredForWrite(uint32 purpose, uint32 type, bool wantsWriteReadyNotification);
   void SendPingOrPong(uint32 whatCode, const IPAddressAndPort & destIAP);
   void NoteHeardFromMember(const IPAddressAndPort & heardFrom, uint64 timestampMicros, uint16 optUserUnicastPort);
   void EnsureUserUnicastMembershipSetUpToDate();

   const IPAddressAndPort _userMulticastAddress;
   IPAddressAndPort _lastPacketReceivedFrom;
   uint32 _maxPacketSize;

   // Values below this line may be accessed by the internal thread ONLY

   enum {
      SMDIO_SOCKET_PURPOSE_USER,  // i.e. this socket will be used to transmit user-data packets only
      SMDIO_SOCKET_PURPOSE_PING,  // i.e. this socket will be used to transmit keepalive pings only
      NUM_SMDIO_SOCKET_PURPOSES
   };

   enum {
      SMDIO_SOCKET_TYPE_MULTICAST = 0, // i.e. this socket will send and receive multicast packets
      SMDIO_SOCKET_TYPE_UNICAST,       // i.e. this socket will send and receive unicast packets
      NUM_SMDIO_SOCKET_TYPES
   };

   class KnownMemberInfo
   {
   public:
      KnownMemberInfo() : _userUnicastPort(0), _lastHeardFromTime(0) {/* empty */}

      String ToString() const {return String("UserUnicastPort=%1 LastHeardFromTime=%2").Arg(_userUnicastPort).Arg(_lastHeardFromTime);}

      uint16 _userUnicastPort;
      uint64 _lastHeardFromTime;
   };

   Hashtable<IPAddressAndPort, KnownMemberInfo> _knownMembers;  // member-unicast-location -> last-heard-from-time
   ByteBufferRef _scratchBuf;
   UDPSocketDataIORef _udpDataIOs[NUM_SMDIO_SOCKET_PURPOSES][NUM_SMDIO_SOCKET_TYPES];
   bool _isInternalSocketRegisteredForWrite[NUM_SMDIO_SOCKET_PURPOSES][NUM_SMDIO_SOCKET_PURPOSES];
   uint16 _localUnicastPorts[NUM_SMDIO_SOCKET_PURPOSES];
};
DECLARE_REFTYPES(SimulatedMulticastDataIO);

}; // end namespace muscle

#endif
