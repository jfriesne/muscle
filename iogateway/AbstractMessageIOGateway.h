/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAbstractMessageIOGateway_h
#define MuscleAbstractMessageIOGateway_h

#include "dataio/DataIO.h"
#include "message/Message.h"
#include "support/NotCopyable.h"
#include "util/Queue.h"
#include "util/RefCount.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/PulseNode.h"

namespace muscle {

class AbstractMessageIOGateway;

/** Interface for any object that wishes to be notified by AbstractMessageIOGateway::DoInput() about received Messages. */
class AbstractGatewayMessageReceiver
{
public:
   /** Default constructor */
   AbstractGatewayMessageReceiver() : _inBatch(false), _doInputCount(0) {/* empty */}

   /** Destructor */
   virtual ~AbstractGatewayMessageReceiver() {/* empty */}

   /** This method calls MessageReceivedFromGateway() and then AfterMessageReceivedFromGateway().
    *  AbstractMessageIOGateway::DoInput() should call this method whenever it has received a new
    *  Message from its DataIO object..
    *  @param msg MessageRef containing the new Message
    *  @param userData This is a miscellaneous value that may be used by some gateways for various purposes.
    *                  Or it may be ignored if the MessageRef is sufficient.
    */
   void CallMessageReceivedFromGateway(const MessageRef & msg, void * userData = NULL)
   {
      if ((_doInputCount > 0)&&(_inBatch == false))
      {
         _inBatch = true;
         BeginMessageReceivedFromGatewayBatch();
      }
      MessageReceivedFromGateway(msg, userData);
      AfterMessageReceivedFromGateway(msg, userData);
   }

protected:
   /** Called whenever a new incoming Message is available for us to look at.
    *  @param msg Reference to the new Message to process.
    *  @param userData This is a miscellaneous value that may be used by some gateways for various purposes.
    */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData) = 0;

   /** Called after each call to MessageReceivedFromGateway().  Useful when there
    *  is something that needs to be done after the subclass has finished its processing.
    *  Default implementation is a no-op.
    *  @param msg MessageRef containing the Message that was just passed to MessageReceivedFromGateway()
    *  @param userData userData value that was just passed to MessageReceivedFromGateway()
    */
   virtual void AfterMessageReceivedFromGateway(const MessageRef & msg, void * userData) {(void) msg; (void) userData;}

   /** This method will be called just before MessageReceivedFromGateway() and AfterMessageReceivedFromGateway()
    *  are called one or more times.  Default implementation is a no-op.
    */
   virtual void BeginMessageReceivedFromGatewayBatch() {/* empty */}

   /** This method will be called just after MessageReceivedFromGateway() and AfterMessageReceivedFromGateway()
    *  have been called one or more times.  Default implementation is a no-op.
    */
   virtual void EndMessageReceivedFromGatewayBatch() {/* empty */}

private:
   friend class AbstractMessageIOGateway;

   void DoInputBegins() {_doInputCount++;}
   void DoInputEnds()
   {
      if ((--_doInputCount == 0)&&(_inBatch))
      {
         EndMessageReceivedFromGatewayBatch();
         _inBatch = false;
      }
   }

   bool _inBatch;
   uint32 _doInputCount;
};

/** Handy utility class for programs that don't want to define their own custom subclass
 *  just to gather incoming Messages a gateway -- this receiver just adds the received
 *  Messages to the tail of the Queue, which your code can then pick up later on at its leisure.
 *  (For high-bandwidth stuff, this isn't as memory efficient, but for simple programs it's good enough)
 */
class QueueGatewayMessageReceiver : public AbstractGatewayMessageReceiver
{
public:
   /** Default constructor */
   QueueGatewayMessageReceiver() {/* empty */}

   /** Returns a read-only reference to our held Queue of received Messages. */
   const Queue<MessageRef> & GetMessages() const {return _messageQueue;}

   /** Returns a read-only reference to our held Queue of received Messages. */
   Queue<MessageRef> & GetMessages() {return _messageQueue;}

   /** Convenience method, provided for backwards compatibility with older code. */
   status_t RemoveHead(MessageRef & msg) {return _messageQueue.RemoveHead(msg);}

   /** Convenience method, provided for backwards compatibility with older code. */
   bool HasItems() const {return _messageQueue.HasItems();}

protected:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData) {(void) userData; (void) _messageQueue.AddTail(msg);}

private:
   Queue<MessageRef> _messageQueue;
};

/**
 *  Abstract base class representing an object that can send/receive
 *  Messages via a DataIO byte-stream.
 */
class AbstractMessageIOGateway : public RefCountable, public AbstractGatewayMessageReceiver, public PulseNode, private CountedObject<AbstractMessageIOGateway>, private NotCopyable
{
public:
   /** Default Constructor. */
   AbstractMessageIOGateway();

   /** Destructor. */
   virtual ~AbstractMessageIOGateway();

   /**
    * Appends the given message reference to the end of our list of outgoing messages to send.  Never blocks.
    * @param messageRef A reference to the Message to send out through the gateway.
    * @return B_NO_ERROR on success, B_ERROR iff for some reason the message can't be queued (out of memory?)
    */
   virtual status_t AddOutgoingMessage(const MessageRef & messageRef) {return _hosed ? B_ERROR : _outgoingMessages.AddTail(messageRef);}

   /**
    * Writes some of our outgoing message bytes to the wire.
    * Not guaranteed to write all outgoing messages (it will try not to block)
    * @note Do not override this method!  Override DoOutputImplementation() instead!
    * @param maxBytes optional limit on the number of bytes that should be sent out.
    *                 Defaults to MUSCLE_NO_LIMIT (which is a very large number)
    * @return The number of bytes written, or a negative value if the connection has been broken
    *         or some other catastrophic condition has occurred.
    */
   int32 DoOutput(uint32 maxBytes = MUSCLE_NO_LIMIT) {return DoOutputImplementation(maxBytes);}

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
      int32 ret = DoInputImplementation(receiver, maxBytes);
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
    * and waiting to be sent across the wire.  Should return false if
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

   /**
    * By default, the AbstractMessageIOGateway calls Flush() on its DataIO's
    * output stream whenever the last outgoing message in the outgoing message queue
    * is sent.  Call SetFlushOnEmpty(false) to inhibit this behavior (e.g. for bandwidth
    * efficiency when low message latency is not a requirement).
    * @param flush If true, auto-flushing will be enabled.  If false, it will be disabled.
    */
   void SetFlushOnEmpty(bool flush);

   /** Accessor for the current state of the FlushOnEmpty flag */
   bool GetFlushOnEmpty() const {return _flushOnEmpty;}

   /** Returns A reference to our outgoing messages queue.  */
   Queue<MessageRef> & GetOutgoingMessageQueue() {return _outgoingMessages;}

   /** Returns A const reference to our outgoing messages queue.  */
   const Queue<MessageRef> & GetOutgoingMessageQueue() const {return _outgoingMessages;}

   /** Installs (ref) as the DataIO object we will use for our I/O.
     * Typically called by the ReflectServer object.
     */
   virtual void SetDataIO(const DataIORef & ref) {_ioRef = ref;}

   /** As above, but returns a reference instead of the raw pointer. */
   const DataIORef & GetDataIO() const {return _ioRef;}

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
     *                then this method will return B_ERROR after (timeoutPeriod) microseconds
     *                if the operation hasn't already completed by then.
     * @returns B_NO_ERROR on success (all pending outgoing Messages were sent) or B_ERROR
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

protected:
   /** Called by ExecuteSynchronousMessaging() to see if we are still awaiting our reply Messages.  Default implementation calls HasBytesToOutput() and returns that value. */
   virtual bool IsStillAwaitingSynchronousMessagingReply() const {return HasBytesToOutput();}

   /** Called by ExecuteSynchronousMessaging() when a Message is received.  Default implementation just passes the call on to the like-named method in (r) */
   virtual void SynchronousMessageReceivedFromGateway(const MessageRef & msg, void * userData, AbstractGatewayMessageReceiver & r) {r.MessageReceivedFromGateway(msg, userData);}

   /** Called by ExecuteSynchronousMessaging() after a Message is received.  Default implementation just passes the call on to the like-named method in (r) */
   virtual void SynchronousAfterMessageReceivedFromGateway(const MessageRef & msg, void * userData, AbstractGatewayMessageReceiver & r) {r.AfterMessageReceivedFromGateway(msg, userData);}

   /** Called by ExecuteSynchronousMessaging() when a batch of Messages is about to be received.  Default implementation just passes the call on to the like-named method in (r) */
   virtual void SynchronousBeginMessageReceivedFromGatewayBatch(AbstractGatewayMessageReceiver & r) {r.BeginMessageReceivedFromGatewayBatch();}

   /** Called by ExecuteSynchronousMessaging() when all Messages in a batch have been received.  Default implementation just passes the call on to the like-named method in (r) */
   virtual void SynchronousEndMessageReceivedFromGatewayBatch(AbstractGatewayMessageReceiver & r) {r.EndMessageReceivedFromGatewayBatch();}

   /** Implementation of the AbstracteMessageIOGatewayReceiver interface:  calls AddOutgoingMessage(msg).
     * This way you can have the input of one gateway go directly to the output of another, without any intermediate step.
     */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * /*userData*/) {(void) AddOutgoingMessage(msg);}

private:
   friend class ScratchProxyReceiver;
   Queue<MessageRef> _outgoingMessages;

   DataIORef _ioRef;

   bool _hosed;  // set true on error
   bool _flushOnEmpty;

   friend class ReflectServer;
};
DECLARE_REFTYPES(AbstractMessageIOGateway);

}; // end namespace muscle

#endif
