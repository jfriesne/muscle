/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAbstractGatewayReceiver_h
#define MuscleAbstractGatewayReceiver_h

#include "message/Message.h"
#include "util/NestCount.h"
#include "util/Queue.h"

namespace muscle {

class AbstractMessageIOGateway;

/** This is the name of the flattened-IPAddressAndPort indicating the
  * source (or destination) of a UDP packet transmitted via a gateway.
  * Used only when sending or receiving via a packet-based protocol (read: UDP).
  */
#define PR_NAME_PACKET_REMOTE_LOCATION "_rl"  

/** Interface for any object that wishes to be notified by AbstractMessageIOGateway::DoInput() about received Messages. */
class AbstractGatewayMessageReceiver 
{
public:
   /** Default constructor */
   AbstractGatewayMessageReceiver() : _inBatch(false) {/* empty */}

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
      if ((_doInputCount.IsInBatch())&&(_inBatch == false))
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

   void DoInputBegins() {(void) _doInputCount.Increment();}
   void DoInputEnds()
   {
      if ((_doInputCount.Decrement())&&(_inBatch))
      {
         EndMessageReceivedFromGatewayBatch();
         _inBatch = false;
      }
   }

   bool _inBatch;
   NestCount _doInputCount;
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

   /** Convenience method; Removes the next item from the head of the queue and returns it.
     * @param msg On success, the removed Message will be written into this MessageRef
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure.
     */
   status_t RemoveHead(MessageRef & msg) {return _messageQueue.RemoveHead(msg);}

   /** Convenience method, provided for backwards compatibility with older code. */
   bool HasItems() const {return _messageQueue.HasItems();}

protected:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData) {(void) userData; (void) _messageQueue.AddTail(msg);}

private:
   Queue<MessageRef> _messageQueue;
};

} // end namespace muscle

#endif
