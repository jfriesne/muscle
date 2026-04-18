/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ICallbackMechanism_h
#define ICallbackMechanism_h

#ifndef MUSCLE_AVOID_CPLUSPLUS11
# include <atomic>
#endif

#include "support/NotCopyable.h"
#include "system/Mutex.h"
#include "util/Hashtable.h"
#include "util/NestCount.h"

namespace muscle {

class ICallbackSubscriber;

/** This class defines the interface for an object that provides thread-safe callback
  * injection into a thread.  The use case is:  an ICallbackSubscriber in thread A
  * (eg a background networking thread) wants some functions to be called by thread B
  * (eg the GUI thread).
  *
  * To make that happen, the ICallbackSubscriber calls its RequestCallbackInDispatchThread()
  * method, which in turn calls the SignalDispatchThread() method of the ICallbackMechanism
  * it is registered to.  It is then the ICallbackMechanism's job to make sure that the
  * DispatchCallbacks() method gets called ASAP by thread B.
  *
  * ICallbackMechanism::DispatchCallbacks() will then call the DispatchCallbacks() method
  * on each registered ICallbackSubscriber.
  */
class ICallbackMechanism : public NotCopyable
{
public:
   /** Default Constructor */
   ICallbackMechanism()
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      : _signalPending(false)
#endif
   {
      // empty
   }

   /** Destructor */
   virtual ~ICallbackMechanism();

   /** The ICallbackMechanism implementation should call this method from the event loop of
     * the dispatch-thread (e.g. the main/GUI thread) ASAP after receiving the dispatch-trigger-signal
     * that was sent to it earlier by the subclass's SignalDispatchThreadImplementation() method.
     * This method will call DispatchCallbacksImplementation(), which will in turn call
     * DispatchCallbacks() on any registered ICallbackSubscriber objects.
     */
   void DispatchCallbacks()
   {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      _signalPending.store(false);        // clear the dirty-bit (must be done first to avoid a race condition)
#endif
      DispatchCallbacksImplementation();  // defined in our subclass
   }

protected:
   /** This method may be called from any thread; its only job is to asynchronously send
     * a signal to the dispatch-thread, in order to trigger the dispatch-thread to call
     * DispatchCallbacks() ASAP.
     */
   void SignalDispatchThread()
   {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      // logic to avoid queueing up redundant events if the main thread is slow to react
      bool expected = false;
      if (_signalPending.compare_exchange_strong(expected, true) == false) return;
#endif
      SignalDispatchThreadImplementation();  // defined in our subclass
   }

   /** Called by DispatchCallbacks() (ie typically the main/GUI thread) ASAP after receiving
     * the dispatch-trigger-signal that was sent earlier via SignalDispatchThread().
     * @note this method should only ever be called directly by our DispatchCallbacks() method,
     *       or by subclass implementations of DispatchCallbacksImplementation().
     */
   virtual void DispatchCallbacksImplementation();

private:
   friend class ICallbackSubscriber;

   /** This method will be called from SignalDispatchThread(), and may be called from any thread; the subclass
     * must implement it to asynchronously send some sort of signal/event to the dispatch-thread, in order to
     * trigger the dispatch-thread to call DispatchCallbacks() ASAP.
     * @note this method should not be called directly by anyone other than SignalDispatchThread().
     */
   virtual void SignalDispatchThreadImplementation() = 0;

   void DispatchCallbacksAux(Hashtable<ICallbackSubscriber *, uint32> & scratchSubsTable);

   // these methods are here to be called by the ICallbackSubscriber class only
   void RegisterCallbackSubscriber(  ICallbackSubscriber * sub) {DECLARE_MUTEXGUARD(_registeredSubscribersMutex); (void) _registeredSubscribers.PutWithDefault(sub);}
   void UnregisterCallbackSubscriber(ICallbackSubscriber * sub) {DECLARE_MUTEXGUARD(_registeredSubscribersMutex); (void) _registeredSubscribers.Remove(sub);}
   void RequestCallbackInDispatchThread(ICallbackSubscriber * sub, uint32 eventTypeBits, uint32 clearBits);

   // These tables should be accessed from the main/dispatch thread only
   Mutex _registeredSubscribersMutex;                              // only necessary for cases where subscribers register from non-dispatch threads!
   Hashtable<ICallbackSubscriber *, Void> _registeredSubscribers;  // everyone who has registered
   Hashtable<ICallbackSubscriber *, uint32> _scratchSubscribers;   // only here to minimize heap reallocations

   // This table may be accessed from any thread, hence the Mutex
   Mutex _dirtySubscribersMutex;                               // serialize access to the _dirtySubscribers table
   Hashtable<ICallbackSubscriber *, uint32> _dirtySubscribers; // who has requested a DispatchCallbacks() call
   NestCount _dispatchCallbacksNestCount;                      // so we can handle re-entrant calls to DispatchCallbacks() gracefully

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   std::atomic<bool> _signalPending;
#endif
};

}  // end muscle namespace

#endif
