/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSignalMessageIOGateway_h
#define MuscleSignalMessageIOGateway_h

#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

/**
 * This is a special-purpose gateway used to facilitate cross-thread signalling.
 * All it does is read bytes from its DataIO, and whenever it has read some bytes,
 * it throws them away and adds a user-specified MessageRef to its incoming-Messages
 * Queue, so this its session's MessageReceivedFromGateway() method will be called ASAP.
 */
class SignalMessageIOGateway : public AbstractMessageIOGateway
{
public:
   /** Constructor.  Creates a SignalMessageIOGateway with a NULL signal message reference.
     */
   SignalMessageIOGateway() {/* empty */}

   /** Constructor
     * @param signalMessage The message to send out when we have read some incoming data.
     */
   SignalMessageIOGateway(const MessageRef & signalMessage) : _signalMessage(signalMessage) {/* empty */}

   /** Destructor */
   virtual ~SignalMessageIOGateway() {/* empty */}

   /** Always returns false. */
   MUSCLE_NODISCARD virtual bool HasBytesToOutput() const {return false;}

   /** Returns a reference to our current signal message */
   const MessageRef & GetSignalMessage() const {return _signalMessage;}

   /** Sets our current signal message reference.
     * @param r the new Message to send to the AbstractGatewayMessageReceiver whenever we receive some bytes from the socket
     */
   void SetSignalMessage(const MessageRef & r) {_signalMessage = r;}

protected:
   /** DoOutput is a no-op for this gateway... all messages are simply eaten and dropped.
     * @copydoc AbstractMessageIOGateway::DoOutputImplementation(uint32)
     */
   virtual io_status_t DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT)
   {
      (void) maxBytes;  // avoid compiler warning

      // Just eat and drop ... we don't really support outgoing messages
      while(GetOutgoingMessageQueue().RemoveHead().IsOK()) {/* keep doing it */}
      return io_status_t();
   }

   /** Overridden to enqeue a (signalMessage) whenever data is read.
     * @copydoc AbstractMessageIOGateway::DoInputImplementation(AbstractGatewayMessageReceiver &, uint32)
     */
   virtual io_status_t DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT)
   {
      char buf[256];
      const io_status_t bytesRead = GetDataIO()() ? GetDataIO()()->Read(buf, muscleMin(maxBytes, (uint32)sizeof(buf))) : io_status_t(B_BAD_OBJECT);
      if (bytesRead.GetByteCount() > 0) receiver.CallMessageReceivedFromGateway(_signalMessage);
      return bytesRead;
   }

private:
   MessageRef _signalMessage;

   DECLARE_COUNTED_OBJECT(SignalMessageIOGateway);
};
DECLARE_REFTYPES(SignalMessageIOGateway);

} // end namespace muscle

#endif
