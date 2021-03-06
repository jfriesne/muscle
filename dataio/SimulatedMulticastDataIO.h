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
class SimulatedMulticastDataIO : public PacketDataIO, private Thread
{
public:
   /**
    *  Constructor.
    *  @param multicastAddress The multicast address we want to listen to and simulate (without actually transmitting any user-data on)
    */
   SimulatedMulticastDataIO(const IPAddressAndPort & multicastAddress);

   /** Destructor.  Shuts down the internal thread and cleans up. */
   virtual ~SimulatedMulticastDataIO() {ShutdownAux();}

   virtual int32 ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retSourcePacket);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual int32 WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest);

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return const_cast<SimulatedMulticastDataIO &>(*this).GetOwnerWakeupSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return const_cast<SimulatedMulticastDataIO &>(*this).GetOwnerWakeupSocket();}
   virtual void Shutdown() {ShutdownAux();}

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

   /** Returns the multicast address that was passed in to our constructor. */
   virtual const IPAddressAndPort & GetPacketSendDestination() const {return _multicastAddress;}

protected:
   virtual void InternalThreadEntry();

private:
   // Deliberately made private because we don't want this method to be called while our internal thread is running
   // For this class, you have to choose your destination address in the constructor, and stick with it
   virtual status_t SetPacketSendDestination(const IPAddressAndPort & /*iap*/) {return B_UNIMPLEMENTED;}

   UDPSocketDataIORef CreateMulticastUDPDataIO(const IPAddressAndPort & iap) const;
   void ShutdownAux();
   status_t ReadPacket(DataIO & dio, ByteBufferRef & retBuf);
   status_t SendIncomingDataPacketToMainThread(const ByteBufferRef & data, const IPAddressAndPort & source);
   void UpdateUnicastSocketRegisteredForWrite(bool shouldBeRegisteredForWrite);
   void DrainOutgoingPacketsTable();
   void NoteHeardFromMember(const IPAddressAndPort & heardFromPingSource, uint64 timestampMicros);
   void EnsureUserUnicastMembershipSetUpToDate();
   status_t EnqueueOutgoingMulticastControlCommand(uint32 whatCode, uint64 now, const IPAddressAndPort & destIAP);
   status_t ParseMulticastControlPacket(const ByteBuffer & buf, uint64 now, uint32 & retWhatCode);
   const char * GetUDPSocketTypeName(uint32 which) const;
   bool IsInEnobufsErrorMode() const;
   void SetEnobufsErrorMode(bool enableErrorMode);

   IPAddressAndPort _multicastAddress;
   uint32 _maxPacketSize;

   // Values below this line may be accessed by the internal thread ONLY

   enum {
      SMDIO_SOCKET_TYPE_MULTICAST = 0, // i.e. this socket will send and receive multicast packets
      SMDIO_SOCKET_TYPE_UNICAST,       // i.e. this socket will send and receive unicast packets
      NUM_SMDIO_SOCKET_TYPES
   };

   IPAddressAndPort _localAddressAndPort;                         // address we are sending packets from
   OrderedKeysHashtable<IPAddressAndPort, uint64> _knownMembers;  // member-ping-socket-location -> last-heard-from-time
   ByteBufferRef _scratchBuf;
   UDPSocketDataIORef _udpDataIOs[NUM_SMDIO_SOCKET_TYPES];
   bool _isUnicastSocketRegisteredForWrite;
   Hashtable<IPAddressAndPort, Queue<ConstByteBufferRef> > _outgoingPacketsTable;

   uint32 _enobufsCount;
   uint64 _nextErrorModeSendTime;

   DECLARE_COUNTED_OBJECT(SimulatedMulticastDataIO);
};
DECLARE_REFTYPES(SimulatedMulticastDataIO);

} // end namespace muscle

#endif
