#ifndef ICallbackMechanism_h
#define ICallbackMechanism_h

#include "system/Mutex.h"
#include "util/Hashtable.h"

namespace muscle {

class ICallbackSubscriber;

/** This class defines the interface for an object that provides thread-safe callback
  * injection into a thread.  The use case is:  an ICallbackSubscriber in thread A
  * (e.g. a background networking thread) wants some functions to be called by thread B
  * (e.g. the GUI thread).
  *
  * To make that happen, the ICallbackSubscriber calls its RequestCallbackInDispatchThread()
  * method, which in turn calls the SignalDispatchThread() method of the ICallbackMechanism
  * it is registered to.  It is then the ICallbackMechanism's job to make sure that the
  * DispatchCallbacks() method gets called ASAP by thread B.
  *
  * ICallbackMechanism::DispatchCallbacks() will then call the DispatchCallbacks() method
  * on each registered ICallbackSubscriber.
  */
class ICallbackMechanism
{
public:
   /** Default Constructor */
   ICallbackMechanism() {/* empty */}

   /** Destructor */
   virtual ~ICallbackMechanism();

   /** The ICallbackMechanism implementation should call this method from the dispatch-thread
     * (i.e. typically the main/GUI thread) in response to receiving the dispatch-signal that
     * was sent earlier via SignalDispatchThread().
     * This method will call DispatchCallbacks() on any registered ICallbackSubscriber objects.
     */
   virtual void DispatchCallbacks();

protected:
   /** This method may be called from any thread; its only job is to asynchronously send
     * a signal to the dispatch-thread, in order to trigger the dispatch-thread to call
     * DispatchCallbacks() ASAP.
     */
   virtual void SignalDispatchThread() = 0;

private:
   friend class ICallbackSubscriber;

   // these methods are here to be called by the ICallbackSubscriber class only
   void RegisterNetworkThread(  ICallbackSubscriber * sub) {(void) _registeredSubscribers.PutWithDefault(sub);}
   void UnregisterNetworkThread(ICallbackSubscriber * sub) {(void) _registeredSubscribers.Remove(sub);}
   void RequestCallbackInDispatchThread(ICallbackSubscriber * sub, uint32 eventTypeBits, uint32 clearBits);

   // These tables should be accessed from the main/dispatch thread only
   Hashtable<ICallbackSubscriber *, Void> _registeredSubscribers;  // everyone who has registered
   Hashtable<ICallbackSubscriber *, uint32> _scratchSubscribers;   // only here to minimize heap reallocations

   // This table may be accessed from any thread, hence the Mutex
   Mutex _dirtySubscribersMutex;                               // serialize access to the _dirtySubscribers table
   Hashtable<ICallbackSubscriber *, uint32> _dirtySubscribers; // who has requested a DispatchCallback() call
};

};  // end muscle namespace

#endif
