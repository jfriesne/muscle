#ifndef ThreadedInternalSession_h
#define ThreadedInternalSession_h

#include "AdvancedQMessageTransceiverThread.h"

using namespace muscle;

/** This class is an example of a MUSCLE session object that spawns its own internal thread to do
  * asynchronous processing.  Note that the internal thread is NOT allowed to touch any of the MUSCLE
  * database or ReflectServer nodes directly, or it will suffer from race conditions.  The internal thread
  * can only do its own separate work, and comunicate with the MUSCLE thread by sending or receiving 
  * Message objects to/from the MUSCLE thread.
  */
class ThreadedInternalSession : public AdvancedThreadWorkerSession, private Thread
{
public:
   /** Constructor
     * @param args This can contain whatever information the thread will find useful when it starts up.
     */
   explicit ThreadedInternalSession(const MessageRef & args);

   /** Called during setup.  Overridden to start the internal thread running. */
   virtual status_t AttachedToServer();

   /** Our SignalMessageIOGateway gateway sends us an empty dummy Message whenever it wants us to check our 
    * internal thread's reply-messages-queue.  We respond by grabbing all of the Messages from the internal thread's queue,
    * and handing them over to the superclass's MessageReceivedFromGateway() method, as if they came from a regular
    * old (TCP-connected) AdvancedThreadWorkerSession's client process.
    */
   virtual void MessageReceivedFromGateway(const MessageRef & /*dummyMsg*/, void * userData);

   /** Called (in the MUSCLE thread) whenever this session receives a Message from one of our neighboring sessions.
     * Typically it would be the AdvancedThreadSupervisorSession that is sending us Messages.
     */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData);

   /** Called when we are about to go away -- overridden to the internal slave thread first */
   virtual void AboutToDetachFromServer();

protected:
   /** Called by InternalThreadEntry(), in the internal/slave thread, whenever the main thread has a Message to give the slave Thread
     * @param msgRef the MessageRef that was handed to us from the MUSCLE thread.
     * @param numLeft the number of Messages left for us to process in the FIFO queue after this one.
     */
   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft);

   /** Overridden so that internal thread sessions will show up under the "hostname" of "InternalThreadSessions". */
   virtual String GenerateHostName(const IPAddress & /*ip*/, const String & /*defaultName*/) const {return "InternalThreadSessions";}

private:
   /** Entry point for the internal thread -- overridden to customize the event loop with a poll timeout.
     * So in addition to handling any Messages received from the MUSCLE thread, this event loop will also
     * send a PR_COMMAND_SETDATA to the MUSCLE thread once every 5 seconds, so that the Qt GUI and other
     * subscribed MUSCLE clients can track our thread's progress in counting up from zero.
     */
   virtual void InternalThreadEntry();

   status_t SetupNotifierGateway();
   void SendExampleMessageToMainThread();

#if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
   friend class TimerSignalReceiverObject;
#endif

   MessageRef _args;
   bool _gatewayOK;

   uint32 _count;
   uint64 _nextStatusPostTime;

   char _threadIDString[20];
};

#endif
