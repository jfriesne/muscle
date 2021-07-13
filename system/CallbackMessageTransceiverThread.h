/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleCallbackMessageTransceiverThread_h
#define MuscleCallbackMessageTransceiverThread_h

#include "system/MessageTransceiverThread.h"

namespace muscle {

/** This subclass of MessageTransceiverThread uses the ICallbackSubscriber mechanism
  * to deliver events to the main/owning thread.  That makes it easier to integrate
  * with various environment-specific event-loops; you simply need to supply the correct
  * type of ICallbackMechanism to the constructor.
  */
class CallbackMessageTransceiverThread : public MessageTransceiverThread
{
public:
   /** Constructor
     * @param optCallbackMechanism Pointer to the ICallbackMechanism object we should register with
     */
   CallbackMessageTransceiverThread(ICallbackMechanism * optCallbackMechanism) : MessageTransceiverThread(optCallbackMechanism) {/* empty */}

protected:
   /** Implemented to call the appropriate virtual methods when a signal is received from the internal thread.
     * @param eventTypeBits this argument is ignored, for now
     */
   virtual void DispatchCallbacks(uint32 eventTypeBits);

   /** Called when MessageReceived() is about to be emitted one or more times.
     * Default implementation is a no-op.
     */
   virtual void BeginMessageBatch();

   /** Called when a new Message has been received by one of the sessions being operated by our internal thread.
     * @param msg Reference to the Message that was received.
     * @param sessionID Session ID string of the session that received the message
     * Default implementation is a no-op.
     */
   virtual void MessageReceived(const MessageRef & msg, const String & sessionID);

   /** Called when we are done emitting MessageReceived, for the time being.
     * Default implementation is a no-op.
     */
   virtual void EndMessageBatch();

   /** Called when a new Session object is accepted by one of the factories being operated by our internal thread
     * @param sessionID Session ID string of the newly accepted Session object.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     * @param iap The location of the peer that we are accepting a connection from.
     * Default implementation is a no-op.
     */
   virtual void SessionAccepted(const String & sessionID, uint32 factoryID, const IPAddressAndPort & iap);

   /** Called when a session object is attached to the internal thread's ReflectServer
     * @param sessionID the session ID string of the session that was attached to the ReflectServer
     * Default implementation is a no-op.
     */
   virtual void SessionAttached(const String & sessionID);

   /** Called when a session object connects to its remote peer (only used by sessions that were
     * created using AddNewConnectSession())
     * @param sessionID Session ID string of the newly connected Session object.
     * @param connectedTo the IP address and port that the session is connected to.
     * Default implementation is a no-op.
     */
   virtual void SessionConnected(const String & sessionID, const IPAddressAndPort & connectedTo);

   /** Called when a session object is disconnected from its remote peer
     * @param sessionID Session ID string of the newly disconnected Session object.
     * Default implementation is a no-op.
     */
   virtual void SessionDisconnected(const String & sessionID);

   /** Called when a session object is removed from the internal thread's ReflectServer
     * @param sessionID Session ID string of the newly disconnected Session object.
     * Default implementation is a no-op.
     */
   virtual void SessionDetached(const String & sessionID);

   /** Called when a factory object is attached to the internal thread's ReflectServer.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     * Default implementation is a no-op.
     */
   virtual void FactoryAttached(uint32 factoryID);

   /** Called when a factory object is removed from the internal thread's ReflectServer.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     * Default implementation is a no-op.
     */
   virtual void FactoryDetached(uint32 factoryID);

   /** Called when the thread's internal ReflectServer object exits.
     * Default implementation is a no-op.
     */
   virtual void ServerExited();

   /** Called when the output-queues of the sessions specified in a previous call to
     * RequestOutputQueuesDrainedNotification() have drained.  Note that this signal only
     * gets emitted once per call to RequestOutputQueuesDrainedNotification();
     * it is not emitted spontaneously.
     * @param ref MessageRef that you previously specified in RequestOutputQueuesDrainedNotification().
     * Default implementation is a no-op.
     */
   virtual void OutputQueuesDrained(const MessageRef & ref);

   /** This signal is called for all events sent by the internal thread.  You can use this
     * to catch custom events that don't have their own signal defined above, or if you want to
     * receive all thread events via a single slot.
     * @param code the MTT_EVENT_* code of the new event.
     * @param optMsg If a Message is relevant, this will contain it; else it's a NULL reference.
     * @param optFromSession If a session ID is relevant, this is the session ID; else it will be "".
     * @param optFromFactory If a factory is relevant, this will be the factory's ID number; else it will be zero.
     * Default implementation is a no-op.
     */
   virtual void InternalThreadEvent(uint32 code, const MessageRef & optMsg, const String & optFromSession, uint32 optFromFactory);
};

} // end namespace muscle

#endif

