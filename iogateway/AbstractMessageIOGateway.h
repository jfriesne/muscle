/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAbstractMessageIOGateway_h
#define MuscleAbstractMessageIOGateway_h

#include "iogateway/AbstractGatewayMessageReceiver.h"
#include "dataio/PacketDataIO.h"
#include "message/Message.h"
#include "support/NotCopyable.h"
#include "util/PulseNode.h"
#include "util/RefCount.h"

namespace muscle {

/**
 *  Abstract base class representing an object that can convert Messages
 *  to bytes and send them to a DataIO byte-stream for transmission, and 
 *  can convert bytes from the DataIO back into Messages and pass them up to an 
 *  AbstractGatewayMessageReceiver object for processing by the high-level code.
 */
class AbstractMessageIOGateway : public RefCountable, public AbstractGatewayMessageReceiver, public PulseNode, private NotCopyable
{
public:
   /** Default Constructor. */
   AbstractMessageIOGateway();

   /** Destructor. */
   virtual ~AbstractMessageIOGateway();

   /**
    * Appends the given message reference to the end of our list of outgoing messages to send.  Never blocks.  
    * @param messageRef A reference to the Message to send out through the gateway.
    * @return B_NO_ERROR on success, or B_BAD_OBJECT if this gateway is out of commision, or B_OUT_OF_MEMORY.
    */
   virtual status_t AddOutgoingMessage(const MessageRef & messageRef) {return _hosed ? B_BAD_OBJECT : _outgoingMessages.AddTail(messageRef);}

   /**
    * Writes some of our outgoing message bytes to the wire.
    * Not guaranteed to write all outgoing messages (it will try not to block)
    * If it does output all the queued data, and flush-on-empty mode is active (which it
    * is by default) then this method will also call FlushOutput() to make sure the 
    * just-generated bytes get sent out to the wire immediately.
    * @note Do not override this method!  Override DoOutputImplementation() instead!
    * @param maxBytes optional limit on the number of bytes that should be sent out.
    *                 Defaults to MUSCLE_NO_LIMIT (which is a very large number)
    * @return The number of bytes written, or a negative value if the connection has been broken
    *         or some other catastrophic condition has occurred.
    */
   int32 DoOutput(uint32 maxBytes = MUSCLE_NO_LIMIT);

   /**
    * Reads some more incoming message bytes from the wire.  
    * Any time a new Message is received, MessageReceivedFromGateway() should be
    * called on the provided AbstractGatewayMessageReceiver to notify him about it.
    * @note Do not override this method!  Override DoInputImplementation() instead!
    * @param receiver An object to call MessageReceivedFromGateway() on whenever a new 
    *                 incoming Message is available.
    * @param maxBytes optional limit on the number of bytes that should be read in.
    *                 Defaults to MUSCLE_NO_LIMIT (which is a very large number)
    * Tries not to block, but may (depending on implementation)
    * @return The number of bytes read, or a negative value if the connection has been broken
    *         or some other catastrophic condition has occurred.
    */
   int32 DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT) 
   {
      receiver.DoInputBegins();
      const int32 ret = DoInputImplementation(receiver, maxBytes); 
      receiver.DoInputEnds();
      return ret;
   }

   /**
    * Should return true if this gateway is willing to receive from bytes
    * from the wire.  Should return false if (for some reason) the gateway
    * doesn't want to read any bytes right now.  The default implementation
    * of this method always returns true.
    */
   virtual bool IsReadyForInput() const;

   /**
    * Should return true if this gateway has bytes that are queued up 
    * and waiting to be sent to the DataIO object.  Should return false if
    * there are no bytes ready to send, or if the connection has been 
    * closed or hosed.
    */
   virtual bool HasBytesToOutput() const = 0;

   /** Returns the number of microseconds that output to this gateway's
    *  client should be allowed to stall for.  If the output stalls for
    *  longer than this amount of time, the connection will be closed.
    *  Return MUSCLE_TIME_NEVER to disable stall limit checking.  
    *  Default behaviour is to forward this call to the held DataIO object.
    */
   virtual uint64 GetOutputStallLimit() const;

   /** Shuts down the gateway.  Default implementation calls Shutdown() on
     * the held DataIO object.
     */
   virtual void Shutdown();

   /** This method must resets the gateway's encoding and decoding state to its default state.
    *  Any partially completed sends and receives should be cleared, so that the gateway
    *  is ready to send and receive fresh data streams.
    *  Default implementation clears the "hosed" flag and clears the outgoing-Messages queue. 
    *  Subclasses should override this to reset their parse-state variables appropriately too.
    */
   virtual void Reset();

   /** Flushes our output-stream, to get the bytes out the door ASAP.
     * Default implementation just calls FlushOutput() on our DataIO object, if we have one.
     */
   virtual void FlushOutput();

   /**
    * By default, AbstractMessageIOGateway::DoOutput() calls FlushOutput() whenever all of the 
    * data in the outgoing-data-queue has been sent.  Call SetFlushOnEmpty(false) to inhibit 
    * this behavior (e.g. for possible better bandwidth-efficiency when low message latency 
    * is not a requirement).
    * @param flush If true, auto-flushing will be enabled.  If false, it will be disabled.
    */
   void SetFlushOnEmpty(bool flush);

   /** Accessor for the current state of the FlushOnEmpty flag.  Default value is true. */
   bool GetFlushOnEmpty() const {return _flushOnEmpty;}

   /** Returns A reference to our outgoing messages queue.  */
   Queue<MessageRef> & GetOutgoingMessageQueue() {return _outgoingMessages;}

   /** Returns A const reference to our outgoing messages queue.  */
   const Queue<MessageRef> & GetOutgoingMessageQueue() const {return _outgoingMessages;}

   /** Installs (ref) as the DataIO object we will use for our I/O.
     * This method also calls GetMaximumPacketSize() on (ref()), if possible,
     * and stores the result (or 0) to be returned by our GetMaximumPacketSize() method.
     * Typically called by the ReflectServer object.
     * @param ref The new DataIORef that this gateway should use.
     */
   virtual void SetDataIO(const DataIORef & ref);

   /** As above, but returns a reference instead of the raw pointer. */
   const DataIORef & GetDataIO() const {return _ioRef;}

   /** Returns our DataIORef's maximum packet size in bytes, or zero
     * if we have no DataIORef or our DataIORef isn't a PacketDataIO
     */
   uint32 GetMaximumPacketSize() const {return _mtuSize;}

   /** Set whether or not PR_NAME_PACKET_REMOTE_LOCATION IPAddressAndPort fields should be added
     * to incoming Messages that were received via UDP.  Default state is true.
     * @param enable true to enable tagging; false to disable it.
     */
   void SetPacketRemoteLocationTaggingEnabled(bool enable) {_packetRemoteLocationTaggingEnabled = enable;}

   /** Returns whether or not PR_NAME_PACKET_REMOTE_LOCATION IPAddressAndPort fields should be added
     * to incoming Messages that were received via UDP.  Default state is true.
     */
   bool GetPacketRemoteLocationTaggingEnabled() const {return _packetRemoteLocationTaggingEnabled;}

   /** Returns true iff we are hosed--that is, we've experienced an unrecoverable error. */
   bool IsHosed() const {return _hosed;}

   /** This is a convenience method for when you want to do simple synchronous 
     * (RPC-style) communications.  This method will run its own little event loop and not 
     * return until all of this I/O gateway's outgoing Messages have been sent out.  
     * Subclasses of the AbstractMessageIOGateway class that support doing so may augment 
     * this method's logic so that this method does not return until the corresponding 
     * reply Messages have been received and passed to (optReceiver) as well, but since that 
     * functionality is dependent on the particulars of the gateway subclass's protocol, 
     * this base method does not do that.
     * @param optReceiver If non-NULL, then any Messages are received from the remote end 
     *                 of the during the time that ExecuteSynchronousMessaging() is
     *                 executing will be passed to (optReceiver)'s MessageReceivedFromGateway() 
     *                 method.  If NULL, then no data will be read from the socket.
     * @param timeoutPeriod A timeout period for this method.  If left as MUSCLE_TIME_NEVER
     *                (the default), then no timeout is applied.  If set to another value,
     *                then this method will return an error code after (timeoutPeriod) microseconds
     *                if the operation hasn't already completed by then.
     * @returns B_NO_ERROR on success (all pending outgoing Messages were sent) or an error code
     *                     on failure (usually because the connection was severed while
     *                     outgoing data was still pending)
     * @see SynchronousMessageReceivedFromGateway(), SynchronousAfterMessageReceivedFromGateway(),
     *      SynchronousBeginMessageReceivedFromGatewayBatch(), SynchronousEndMessageReceivedFromGatewayBatch(),
     *      and IsStillAwaitingSynchronousMessagingReply().
     * @note Even though this is a blocking call, you should still have the DataIO's socket
     *       set to non-blocking mode, otherwise you risk this call never returning due to a blocking recv().
     */
   virtual status_t ExecuteSynchronousMessaging(AbstractGatewayMessageReceiver * optReceiver, uint64 timeoutPeriod = MUSCLE_TIME_NEVER);

protected:
   /**
    * Writes some of our outgoing message bytes to the wire.
    * Not guaranteed to write all outgoing messages (it will try not to block)
    * @param maxBytes optional limit on the number of bytes that should be sent out.
    *                 Defaults to MUSCLE_NO_LIMIT (which is a very large number)
    * @return The number of bytes written, or a negative value if the connection has been broken
    *         or some other catastrophic condition has occurred.
    */
   virtual int32 DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT) = 0;

   /**
    * Reads some more incoming message bytes from the wire.  
    * Any time a new Message is received, MessageReceivedFromGateway() should be
    * called on the provided AbstractGatewayMessageReceiver to notify him about it.
    * @param receiver An object to call MessageReceivedFromGateway() on whenever a new 
    *                 incoming Message is available.
    * @param maxBytes optional limit on the number of bytes that should be read in.
    *                 Defaults to MUSCLE_NO_LIMIT (which is a very large number)
    * Tries not to block, but may (if the held DataIO object is in blocking mode)
    * @return The number of bytes read, or a negative value if the connection has been broken
    *         or some other catastrophic condition has occurred.
    */
   virtual int32 DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT) = 0;

   /** Call this method to flag this gateway as hosed--that is, to say that an unrecoverable error has occurred. */
   void SetHosed() {_hosed = true;}
  
   /** Called by ExecuteSynchronousMessaging() to see if we are still awaiting our reply Messages.  Default implementation calls HasBytesToOutput() and returns that value. */
   virtual bool IsStillAwaitingSynchronousMessagingReply() const {return HasBytesToOutput();}

   /** Called by ExecuteSynchronousMessaging() when a Message is received.  Default implementation just passes the call on to the like-named method in (r) 
     * @param msg the Message that was received
     * @param userData the user-data pointer, as was passed to ExecuteSynchronousMessaging()
     * @param r the receiver object that we will call MessageReceivedFromGateway() on
     */ 
   virtual void SynchronousMessageReceivedFromGateway(const MessageRef & msg, void * userData, AbstractGatewayMessageReceiver & r) {r.MessageReceivedFromGateway(msg, userData);}

   /** Called by ExecuteSynchronousMessaging() after a Message is received.  Default implementation just passes the call on to the like-named method in (r)
     * @param msg the Message that was received
     * @param userData the user-data pointer, as was passed to ExecuteSynchronousMessaging()
     * @param r the receiver object that we will call AfterMessageReceivedFromGateway() on
     */ 
   virtual void SynchronousAfterMessageReceivedFromGateway(const MessageRef & msg, void * userData, AbstractGatewayMessageReceiver & r) {r.AfterMessageReceivedFromGateway(msg, userData);}

   /** Called by ExecuteSynchronousMessaging() when a batch of Messages is about to be received.  Default implementation just passes the call on to the like-named method in (r)
     * @param r the receiver object that we will call BeginMessageReceivedFromGatewayBatch() on
     */ 
   virtual void SynchronousBeginMessageReceivedFromGatewayBatch(AbstractGatewayMessageReceiver & r) {r.BeginMessageReceivedFromGatewayBatch();}

   /** Called by ExecuteSynchronousMessaging() when all Messages in a batch have been received.  Default implementation just passes the call on to the like-named method in (r) 
     * @param r the receiver object that we will call EndMessageReceivedFromGatewayBatch() on
     */ 
   virtual void SynchronousEndMessageReceivedFromGatewayBatch(AbstractGatewayMessageReceiver & r) {r.EndMessageReceivedFromGatewayBatch();}

   /** Implementation of the AbstracteMessageIOGatewayReceiver interface:  calls AddOutgoingMessage(msg).
     * This way you can have the input of one gateway go directly to the output of another, without any intermediate step.
     * @param msg the Message that was received from the gateway
     */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * /*userData*/) {(void) AddOutgoingMessage(msg);}

   /** Convenience method -- if our held DataIO object is a subclass of PacketDataIO,
     * returns a pointer to it.  Otherwise, returns NULL.
     */
   PacketDataIO * GetPacketDataIO() const {return _packetDataIO;}

private:
   friend class ScratchProxyReceiver;
   Queue<MessageRef> _outgoingMessages;

   DataIORef _ioRef;
   PacketDataIO * _packetDataIO;  // non-NULL only if (_ioRef()) actually is a PacketDataIO
   uint32 _mtuSize;  // set whenever _ioRef changes

   bool _hosed;  // set true on error
   bool _flushOnEmpty;
   bool _packetRemoteLocationTaggingEnabled;

   friend class ReflectServer;

   DECLARE_COUNTED_OBJECT(AbstractMessageIOGateway);
};
DECLARE_REFTYPES(AbstractMessageIOGateway);

} // end namespace muscle

#endif
