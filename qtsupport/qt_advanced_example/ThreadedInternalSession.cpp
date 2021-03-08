#if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
# include <qtimer.h>
# include "qt_advanced_example.h"  // for TimerSignalReceiverObject
#endif

#include "dataio/TCPSocketDataIO.h"
#include "iogateway/SignalMessageIOGateway.h"

#include "ThreadedInternalSession.h"

using namespace muscle;

ThreadedInternalSession :: ThreadedInternalSession(const MessageRef & args)
   : _args(args)
   , _count(0)
   , _nextStatusPostTime(0)
{
   // Set up our communication mechanism with our internally held I/O thread
   // Must be done in the constructor so that the ReflectServer's event loop
   // will have access to our signalling socket.
   _gatewayOK = (SetupNotifierGateway().IsOK());
}

/** Called in the MUSCLE thread during setup.  Overridden to start the internal thread running also. */
status_t ThreadedInternalSession :: AttachedToServer()
{
   if (_gatewayOK == false) return B_BAD_OBJECT;

   status_t ret;
   return (AdvancedThreadWorkerSession::AttachedToServer().IsOK(ret)) ? StartInternalThread() : ret;
}

// Our SignalMessageIOGateway gateway sends us an empty dummy Message whenever it wants us to check our 
// internal thread's reply-messages-queue.  We respond here (in the MUSCLE thread) by grabbing all of the Messages 
// from the internal thread's queue, and handing them over to the superclass's MessageReceivedFromGateway() 
// method, as if they came from a regular old (TCP-connected) AdvancedThreadWorkerSession's client process.
void ThreadedInternalSession :: MessageReceivedFromGateway(const MessageRef & /*dummyMsg*/, void * userData)
{
   MessageRef ref;
   while(GetNextReplyFromInternalThread(ref) >= 0) AdvancedThreadWorkerSession::MessageReceivedFromGateway(ref, userData);
}

/** Called (in the MUSCLE thread) whenever this session receives a Message from one of our neighboring sessions.
  * Typically it would be the AdvancedThreadSupervisorSession that is sending us Messages.
  */
void ThreadedInternalSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData)
{
   if (muscleInRange(msgRef()->what, (uint32)FIRST_ADVANCED_COMMAND, (uint32)ADVANCED_COMMAND_ENDSESSION))
   {
      // We send ADVANCED_COMMAND_* Messages up to our superclass to process in the current (MUSCLE) thread
      AdvancedThreadWorkerSession::MessageReceivedFromSession(from, msgRef, userData);
   }
   else SendMessageToInternalThread(msgRef);  // all other commands get forwarded to our internal thread for it to process
}

/** Called in the MUSCLE thread when we are about to go away -- overridden to the internal slave thread first */
void ThreadedInternalSession :: AboutToDetachFromServer()
{
   (void) ShutdownInternalThread();  // important, to avoid race conditions!  This won't return until the internal thread is gone
   AdvancedThreadWorkerSession::AboutToDetachFromServer();
}

/** Called in the internal/slave thread by InternalThreadEntry(), whenever the main thread has sent a
  * Message to the internal/slave thread for it to process.
  * @param msgRef the MessageRef that was handed to us from the MUSCLE thread.
  * @param numLeft the number of Messages left for us to process in the FIFO queue after this one.
  */
status_t ThreadedInternalSession :: MessageReceivedFromOwner(const MessageRef & msgRef, uint32 /*numLeft*/)
{
   if (msgRef())
   {
      char buf[20];
      switch(msgRef()->what)
      {
         case INTERNAL_THREAD_COMMAND_HURRYUP:
            printf("internal-slave-thread-%s received the following SAYHELLO Message from the MUSCLE thread:\n", muscle_thread_id::GetCurrentThreadID().ToString(buf));
            _count += msgRef()->GetInt32("count");
            _nextStatusPostTime = 0;  // so we'll increment RIGHT AWAY
            InvalidatePulseTime();
         break;

         default:
            printf("internal-slave-thread-%s received the following unknown Message from the MUSCLE thread:\n", muscle_thread_id::GetCurrentThreadID().ToString(buf));
            msgRef()->PrintToStream();
         break;
      }
      return B_NO_ERROR;  // message processed, indicate this thread should keep running
   }
   else return B_ERROR;  // indicate that it's time for this thread to terminate now
}

#if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
void TimerSignalReceiverObject :: CallSendExampleMessageToMainThread() 
{
   printf("TimerSignalReceiveObject %p:  my slot was called by QTimer, calling ThreadInternalSession::SendExampleMessageToMainThread() on object %p\n", this, _master);
   _master->SendExampleMessageToMainThread();
}
#endif

/** Entry point for the internal/slave thread -- overridden to customize the event loop with a poll timeout.
  * So in addition to handling any Messages received from the MUSCLE thread, this event loop will also
  * send a PR_COMMAND_SETDATA to the MUSCLE thread once every 5 seconds, so that the Qt GUI and other
  * subscribed MUSCLE clients can track our thread's progress in counting up from zero.
  */
void ThreadedInternalSession :: InternalThreadEntry()
{
   printf("internal-slave-thread %s is now ALIVE!!!\n", muscle_thread_id::GetCurrentThreadID().ToString(_threadIDString));

   if (_args()) 
   {
      printf("Startup arguments for internal-slave-thread %s are:\n", _threadIDString);
      _args()->PrintToStream();  // a real program would probably use some data from here, not just print it out
   }

#if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
   // If Qt-event-loop integration is what we want, we can get the same behavior as below
   // using Qt's event loop and a QTimer object.
   QTimer timer(NULL);
   timer.setInterval(1000);  // the timer will fire once per second
   TimerSignalReceiverObject tsro(this);  // a little proxy object just so I don't have to make ThreadedInternalSession be a QObject
   QObject::connect(&timer, SIGNAL(timeout()), &tsro, SLOT(CallSendExampleMessageToMainThread()));
   timer.start();

   // since MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION is defined, this will call through to QThread::exec()
   Thread::InternalThreadEntry();  // won't return until it's time for the thread to go away
#else

   // Without MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION we have to provide our own custom event loop instead.
   while(true)
   {
      // Wait until the next Message from the MUSCLE thread, or until (_nextStatusPostTime), whichever comes first.
      // This demonstrates how to implement a polling loop -- if you don't need to poll, you can pass
      // in MUSCLE_TIME_NEVER as the second argument to WaitForNextMessageFromOwner() (or just use
      // the default implementation of Thread::InternalThreadEntry(), which does that same thing)
      //
      // Alternatively, if you never want to block in WaitForNextMessageFromOwner() you could pass 0 as 
      // the second argument, and that would cause WaitForNextMessageFromOwner() to always return immediately.
      MessageRef msgRef;
      int32 numLeftInQueue = WaitForNextMessageFromOwner(msgRef, _nextStatusPostTime);
      if ((numLeftInQueue >= 0)&&(MessageReceivedFromOwner(msgRef, numLeftInQueue).IsError())) 
      {
         // MessageReceivedFromOwner() returned an error, that means this thread needs to exit!
         break;
      }         

      const uint64 now = GetRunTime64();
      if (now >= _nextStatusPostTime) SendExampleMessageToMainThread();
   }
#endif

   printf("Internal-slave-thread %s is exiting!!!\n", _threadIDString);
}

void ThreadedInternalSession :: SendExampleMessageToMainThread()
{
   printf("Internal-slave-thread %s is sending a PR_COMMAND_SETDATA Message to the MUSCLE thread.\n", _threadIDString);

   // Send a message to the MUSCLE thread, telling him to update our session's database node with our new count value
   MessageRef dataMsg = GetMessageFromPool();
   dataMsg()->AddInt32("count", _count);

   MessageRef sendMsg = GetMessageFromPool(PR_COMMAND_SETDATA);
   sendMsg()->AddMessage("thread_status", dataMsg);
   SendMessageToOwner(sendMsg);

   _nextStatusPostTime = GetRunTime64() + SecondsToMicros(1);  // we'll update our status once every 1 second
   _count++;  // in real life this would be something more interesting, e.g. temperature or something
}

status_t ThreadedInternalSession :: SetupNotifierGateway()
{
   // Set up the sockets, DataIO, and SignalMessageIOGateway that we will use to signal
   // the MUSCLE thread that the internal thread has Messages ready for it to receive, and vice versa.
   const ConstSocketRef & socket = GetOwnerWakeupSocket();
   if (socket() == NULL) return B_BAD_OBJECT;

   // This is a socket connection between the internal thread and the MUSCLE thread, used for signalling only
   // We don't send the actual Messages over it, we only use it to wake up the other thread by sending
   // a single byte across the TCP connection.  This makes the messaging efficient even when the Messages
   // are large, because (once created) the Messages never need to be copied or flattened/unflattened.
   DataIORef dataIORef(newnothrow TCPSocketDataIO(socket, false));
   MRETURN_OOM_ON_NULL(dataIORef());

   AbstractMessageIOGatewayRef gw(newnothrow SignalMessageIOGateway());
   MRETURN_OOM_ON_NULL(gw());

   gw()->SetDataIO(dataIORef);
   SetGateway(gw);
   return B_NO_ERROR;
}
