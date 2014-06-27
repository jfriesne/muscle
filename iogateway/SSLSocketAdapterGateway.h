/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSSLSocketAdapterGateway_h
#define MuscleSSLSocketAdapterGateway_h

#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

/**
 * This gateway "wraps" a caller-supplied AbstractMessageIOGateway
 * and modifies the gateway's behavior so that it can be used correctly.
 * with an SSLSocketDataIO.  This special logic is necessary because
 * non-blocking SSLSocketDataIOs have their own unique requirements for
 * when SSL_read() and SSL_write() are called that do not necessarily
 * coincide with what a normal gateway wants to do.
 *
 * So if you are using an SSLSocketDataIO object for non-blocking I/O,
 * you should wrap your gateway in one of these so that it can govern
 * data flow appropriately.
 */
class SSLSocketAdapterGateway : public AbstractMessageIOGateway, private CountedObject<SSLSocketAdapterGateway>
{
public:
   /** Constructor
     * @param slaveGateway Reference to the AbstractMessageIOGateway we want to proxy for.
     */
   SSLSocketAdapterGateway(const AbstractMessageIOGatewayRef & slaveGateway);

   /** Destructor */
   virtual ~SSLSocketAdapterGateway();

   virtual status_t AddOutgoingMessage(const MessageRef & messageRef);

   virtual bool IsReadyForInput() const;
   virtual bool HasBytesToOutput() const;
   virtual uint64 GetOutputStallLimit() const;
   virtual void Shutdown();
   virtual void Reset();
   virtual void SetDataIO(const DataIORef & ref);

   /** Sets our slave gateway to something else.
     * @param slaveGateway Reference to the new AbstractMessageIOGateway we want to proxy for.
     */
   void SetSlaveGateway(const AbstractMessageIOGatewayRef & slaveGateway);

   /** Returns a reference to our held slave gateway (or a NULL reference if we haven't got one) */
   const AbstractMessageIOGatewayRef & GetSlaveGateway() const {return _slaveGateway;}

protected:
   virtual int32 DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT);
   virtual int32 DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);

private:
   uint32 GetSSLState() const;
   void SetSSLForceReadReady(bool forceReadReady);

   AbstractMessageIOGatewayRef _slaveGateway;
   QueueGatewayMessageReceiver _sslMessages;   // messages that were generated during a DoOutput() call, oddly enough
};

}; // end namespace muscle

#endif
