/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/ByteBufferDataIO.h"
#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

#define DEFAULT_TUNNEL_IOGATEWAY_MAGIC 1114989680 // 'Budp'

/** This I/O gateway class is a "wrapper" class that you can use in conjunction with any other
  * AbstractMessageIOGateway class.  It will take the output of that class and packetize it in
  * such a way that the resulting data can be sent efficiently and correctly over an I/O channel that 
  * would otherwise not accept datagrams larger than a certain size.  You can also use it by itself,
  * (without a slave I/O gateway), in which case it will use the standard Message::Flatten() encoding.
  *
  * In particular, this class will combine several small messages into a single packet, for efficiency,
  * and also fragment overly-large data into multiple sub-packets, if necessary, in order to keep 
  * packet size smaller than the physical layer's MTU (Max Transfer Unit) size.  Note that this
  * class does not do any automated retransmission of lost data, so if you do use it over UDP
  * (or some other lossy I/O channel), you will need to handle lost Messages at a higher level.
  * If a message fragment is lost over the I/O channel, this class will simply drop the entire message
  * and continue.
  */
class PacketTunnelIOGateway : public AbstractMessageIOGateway, private CountedObject<PacketTunnelIOGateway>
{
public:
   /** @param slaveGateway This is the gateway we will call to generate data to send, etc.
     *                     If you leave this argument unset (or pass in a NULL reference),
     *                     a general-purpose default algorithm will be used.
     * @param maxTransferUnit The largest packet size this I/O gateway will be allowed to send.
     *                        Default value is MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (aka
     *                        1404 if MUSCLE_AVOID_IPV6 is defined, 1388 otherwise).  If the number
     *                        passed in here is less than (FRAGMENT_HEADER_SIZE+1), it will be
     *                        intepreted as (FRAGMENT_HEADER_SIZE+1).  (aka 21 bytes)
     * @param magic The "magic number" that is expected to be at the beginning of each packet
     *              sent and received.  You can usually leave this as the default, unless you
     *              are doing several separate instances of this class with different protocols,
     *              and you want to make sure they don't interfere with each other.
     */
   PacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway = AbstractMessageIOGatewayRef(), uint32 maxTransferUnit = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET, uint32 magic = DEFAULT_TUNNEL_IOGATEWAY_MAGIC);

   virtual bool HasBytesToOutput() const {return ((_currentOutputBuffer())||(GetOutgoingMessageQueue().HasItems()));}

   /** Sets our slave gateway.  Only necessary if you didn't specify a slave gateway in the constructor. */
   void SetSlaveGateway(const AbstractMessageIOGatewayRef & slaveGateway) {_slaveGateway = slaveGateway;}

   /** Returns our current slave gateway, or a NULL reference if we don't have one. */
   const AbstractMessageIOGatewayRef & GetSlaveGateway() const {return _slaveGateway;}

   /** Sets the maximum size message we will allow ourself to receive.  Defaults to MUSCLE_NO_LIMIT. */
   void SetMaxIncomingMessageSize(uint32 messageSize) {_maxIncomingMessageSize = messageSize;}

   /** Returns the current setting of the maximum-message-size value.  Default to MUSCLE_NO_LIMIT. */
   uint32 GetMaxIncomingMessageSize() const {return _maxIncomingMessageSize;}

   /** If set to true, any incoming UDP packets that aren't in our packetizer-format will be
     * be interpreted as separate, independent incoming messages.  If false (the default state),
     * then any incoming UDP packets that aren't in the packetizer-format will simply be discarded.
     */
   void SetAllowMiscIncomingData(bool allowMisc) {_allowMiscData = allowMisc;}

   /** Returns true iff we are accepting non-packetized incoming UDP messages. */
   bool GetAllowMiscIncomingData() const {return _allowMiscData;}

   /** Sets the source exclusion ID number for this gateway.  The source exclusion ID is
     * useful when you are broadcasting in such a way that your broadcast packets will come
     * back to your own PacketTunnelIOGateway and you don't want to receive them.  When this
     * value is set to non-zero, any packets we send out will be tagged with this value, and
     * any packets that come in tagged with this value will be ignored.
     * @param sexID The source-exclusion ID to use.  Set to non-zero to enable source-exclusion
     *              filtering, or to zero to disable it again.  Default state is zero.
     */
   void SetSourceExclusionID(uint32 sexID) {_sexID = sexID;}

   /** Returns the current source-exclusion ID.  See above for details. */
   uint32 GetSourceExclusionID() const {return _sexID;}

protected:
   /** Implemented to receive packets from various sources and re-assemble them together into
     * the appropriate Message objects.  Note that when MessageReceived() is called on the
     * AbstractGatewayMessageReceiver object, the void-pointer argument will point to an
     * IPAddressAndPort object that the callee can use to find out where the incoming Message
     * came from.
     */
   virtual int32 DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes);

   /** Implemented to send outgoing Messages in a packet-friendly way... i.e. by chopping up
     * too-large Messages, and batching together too-small Messages.
     */
   virtual int32 DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT);

private:
   void HandleIncomingMessage(AbstractGatewayMessageReceiver & receiver, const ByteBufferRef & buf, const IPAddressAndPort & fromIAP);

   const uint32 _magic;                 // our magic number, used to sanity check packets
   const uint32 _maxTransferUnit;       // max number of bytes to try to fit in a packet

   bool _allowMiscData;  // If true, we'll pass on non-magic UDP packets also, as if they were fragments
   uint32 _sexID;

   AbstractMessageIOGatewayRef _slaveGateway;

   ByteBuffer _inputPacketBuffer;
   ByteBuffer _outputPacketBuffer;
   uint32 _outputPacketSize;

   uint32 _sendMessageIDCounter;
   ByteBufferRef _currentOutputBuffer;
   uint32 _currentOutputBufferOffset;

   ByteBufferDataIO _fakeSendIO;
   ByteBuffer _fakeSendBuffer;

   ByteBufferDataIO _fakeReceiveIO;
   uint32 _maxIncomingMessageSize;

   class ReceiveState 
   {
   public:
      ReceiveState() : _messageID(0), _offset(0) {/* empty */}
      ReceiveState(uint32 messageID) : _messageID(messageID), _offset(0) {/* empty */}

      uint32 _messageID;
      uint32 _offset;
      ByteBufferRef _buf;
   };
   Hashtable<IPAddressAndPort, ReceiveState> _receiveStates;

   // Pass the call back through to our own caller, but with the appropriate argument.
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *) {_scratchReceiver->CallMessageReceivedFromGateway(msg, _scratchReceiverArg);}
   AbstractGatewayMessageReceiver * _scratchReceiver;
   void * _scratchReceiverArg;
};

}; // end namespace muscle
