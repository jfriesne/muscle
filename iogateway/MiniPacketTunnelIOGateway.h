/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/ProxyIOGateway.h"
#include "util/NetworkUtilityFunctions.h"  // for MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
# include "zlib/ZLibCodec.h"
#endif

namespace muscle {

#define DEFAULT_MINI_TUNNEL_IOGATEWAY_MAGIC 1836345197 /**< 'mtgm' - default magic value used in MiniPacketTunnelIOGateway packet headers.  See PacketTunnelIOGateway ctor for details. */

/** This class is similar to the PacketTunnelIOGateway class, but simplified so that it only handles
  * Messages smaller than (maxTransferUnit) bytes in size.  In particular, too-large Messages will
  * be dropped, rather than split across multiple packets.
  * That simplification allows the Message-header overhead to be significantly reduced (compared
  * to the logic used by PacketTunnelIOGateway), so that we can pack more Messages into each packet.
  */
class MiniPacketTunnelIOGateway : public ProxyIOGateway
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
   MiniPacketTunnelIOGateway(const AbstractMessageIOGatewayRef & slaveGateway = AbstractMessageIOGatewayRef(), uint32 maxTransferUnit = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET, uint32 magic = DEFAULT_MINI_TUNNEL_IOGATEWAY_MAGIC);

   virtual bool HasBytesToOutput() const {return ((_currentOutputBuffers.HasItems())||(GetOutgoingMessageQueue().HasItems()));}

   /** If set to true, any incoming UDP packets that aren't in our packetizer-format will be
     * be interpreted as separate, independent incoming messages.  If false (the default state),
     * then any incoming UDP packets that aren't in the packetizer-format will simply be discarded.
     * @param allowMisc true to allow miscellaneous incoming UDP packets, false to ignore them.
     */
   void SetAllowMiscIncomingData(bool allowMisc) {_allowMiscData = allowMisc;}

   /** Returns true iff we are accepting non-packetized incoming UDP messages. */
   bool GetAllowMiscIncomingData() const {return _allowMiscData;}

   /** Sets the source exclusion ID number for this gateway.  The source exclusion ID is
     * useful when you are broadcasting in such a way that your broadcast packets will come
     * back to your own MiniPacketTunnelIOGateway and you don't want to receive them.  When this
     * value is set to non-zero, any packets we send out will be tagged with this value, and
     * any packets that come in tagged with this value will be ignored.
     * @param sexID The source-exclusion ID to use.  Set to non-zero to enable source-exclusion
     *              filtering, or to zero to disable it again.  Default state is zero.
     */
   void SetSourceExclusionID(uint32 sexID) {_sexID = sexID;}

   /** Returns the current source-exclusion ID.  See above for details. */
   uint32 GetSourceExclusionID() const {return _sexID;}

   /** Set the level of zlib-compression to apply to outgoing packets just before sending them.
     * @param sendCompressionLevel 0 for no zlib-compression (this is the default) up to 9 for maximum compression.
     */
   void SetZLibCompressionLevel(uint8 sendCompressionLevel) {_sendCompressionLevel = sendCompressionLevel;}

   /** Returns the level of zlib-compression we will apply to outgoing packets just before sending them. */
   uint8 GetZLibCompressionLevel() const {return _sendCompressionLevel;}

protected:
   /** Implemented to receive packets from various sources and split them up into
     * the appropriate Message objects.  Note that when MessageReceived() is called on the
     * AbstractGatewayMessageReceiver object, the void-pointer argument will point to an
     * IPAddressAndPort object that the callee can use to find out where the incoming Message
     * came from.
     * @copydoc AbstractMessageIOGateway::DoInputImplementation(AbstractGatewayMessageReceiver &, uint32)
     */
   virtual int32 DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes);

   /** Implemented to send outgoing Messages in a packet-friendly way... i.e. by chopping up
     * too-large Messages, and batching together too-small Messages.
     * @copydoc AbstractMessageIOGateway::DoOutputImplementation(uint32)
     */
   virtual int32 DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT);

private:
   const uint32 _magic;                 // our magic number, used to sanity check packets
   const uint32 _maxTransferUnit;       // max number of bytes to try to fit in a packet
   uint8 _sendCompressionLevel;         // 0-9 (no zlib-deflate up to maximum-zlib-deflate)

   bool _allowMiscData;  // If true, we'll pass on non-magic UDP packets also, as if they were fragments
   uint32 _sexID;

   ByteBuffer _inputPacketBuffer;
   ByteBuffer _outputPacketBuffer;
   uint32 _outputPacketSize;

   uint32 _sendPacketIDCounter;
   Queue<ByteBufferRef> _currentOutputBuffers;

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
   ZLibCodecRef _codec;
#endif

   DECLARE_COUNTED_OBJECT(MiniPacketTunnelIOGateway);
};
DECLARE_REFTYPES(MiniPacketTunnelIOGateway);

} // end namespace muscle
