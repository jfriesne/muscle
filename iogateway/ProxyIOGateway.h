/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/ByteBufferDataIO.h"
#include "dataio/ByteBufferPacketDataIO.h"
#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

/** This is a utility class that you can subclass from when you want to implement a "decorator"
  * gateway * that holds a child-gateway that it uses to create outgoing ByteBuffers and/or parse
  * incoming ByteBuffers before doing further processing on them.
  */
class ProxyIOGateway : public AbstractMessageIOGateway
{
public:
   /** @param slaveGateway This is the gateway we will call to generate data to send, etc.
     *                     If you specify NULL, we'll fall back to directly calling Message::Flatten() and Message::Unflatten() instead.
     */
   ProxyIOGateway(const AbstractMessageIOGatewayRef & slaveGateway);

   /** Sets our slave gateway.  Only necessary if you didn't specify a slave gateway in the constructor.
     * @param slaveGateway reference to the new slave gateway
     */
   void SetSlaveGateway(const AbstractMessageIOGatewayRef & slaveGateway) {_slaveGateway = slaveGateway;}

   /** Returns our current slave gateway, or a NULL reference if we don't have one. */
   const AbstractMessageIOGatewayRef & GetSlaveGateway() const {return _slaveGateway;}

protected:
   /** Handles the received bytes using the slave-gateway (if one is present) or by calling Message::Unflatten() if one isn't.
     * @param receiver the object to call CallMessageReceivedFromGateway() on when we have created a Message to receive
     * @param p Pointer to the received bytes
     * @param bytesRead the number of bytes that (p) points to
     * @param fromIAP the IP-address-and-port that the bytes came from (if known)
     */
   void HandleIncomingByteBuffer(AbstractGatewayMessageReceiver & receiver, const uint8 * p, uint32 bytesRead, const IPAddressAndPort & fromIAP);

   /** Same as above, except the bytes are specified via ByteBufferRef instead of via raw-pointer-and-length.
     * @param receiver the object to call CallMessageReceivedFromGateway() on when we have created a Message to receive
     * @param buf reference to a ByteBuffer containing the received bytes
     * @param fromIAP the IP-address-and-port that the bytes came from (if known)
     */
   void HandleIncomingByteBuffer(AbstractGatewayMessageReceiver & receiver, const ByteBufferRef & buf, const IPAddressAndPort & fromIAP);

   /** Pops the next MessageRef out of our outgoing-Messages-Queue and tries to convert it into one or more ByteBufferRefs
     * full of bytes to be sent out.  If we have a slave-gateway, it will do the conversion by calling DoOutput()
     * on the slave-gateway as necessary; otherwise it will just call Flatten() on the Message-object.
     */
   void GenerateOutgoingByteBuffers(Queue<ByteBufferRef> & outQ);

   /** Calls Clear() on our fakeStreamSendBuffer object to free up memory.
     * @param maxBytesToRetain if the fakeStreamSendBuffer's size is greater than this, we'll free the buffer; otherwise we'll just mark it
     *                         as zero-length in hope that we can re-use it later.
     */
   void ClearFakeSendBuffer(uint32 maxBytesToRetain) {_fakeStreamSendBuffer.Clear(_fakeStreamSendBuffer.GetNumBytes() > maxBytesToRetain);}

private:
   AbstractMessageIOGatewayRef _slaveGateway;

   ByteBufferPacketDataIO _fakePacketSendIO;
   ByteBufferDataIO       _fakeStreamSendIO;
   ByteBuffer             _fakeStreamSendBuffer;

   ByteBufferDataIO       _fakeStreamReceiveIO;
   ByteBufferPacketDataIO _fakePacketReceiveIO;

   // Pass the call back through to our own caller, but with the appropriate argument.
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *) {_scratchReceiver->CallMessageReceivedFromGateway(msg, _scratchReceiverArg);}
   AbstractGatewayMessageReceiver * _scratchReceiver;
   void * _scratchReceiverArg;
};
DECLARE_REFTYPES(ProxyIOGateway);

} // end namespace muscle
