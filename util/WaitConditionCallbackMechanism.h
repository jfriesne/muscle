/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef WaitConditionCallbackMechanism_h
#define WaitConditionCallbackMechanism_h

#include "system/WaitCondition.h"
#include "util/ICallbackMechanism.h"

namespace muscle {

/** This class implements the ICallbackMechanism interface using a WaitCondition as its signalling mechanism.
  * The waiting thread can block by calling our Wait() method, which passes through to WaitCondition::Wait().
  */
class WaitConditionCallbackMechanism : public ICallbackMechanism
{
public:
   /** Default Constructor */
   WaitConditionCallbackMechanism() : _wcRef(_defaultWaitCondition) {/* empty */}

   /** Explicit constructor
     * @param wc WaitCondition that we should use for waiting and signalling, instead of our internal one
     * @note that (wc) must remain valid for the lifetime of this object.
     */
   WaitConditionCallbackMechanism(WaitCondition & wc) : _wcRef(wc) {/* empty */}

   /** Destructor */
   virtual ~WaitConditionCallbackMechanism() {/* empty */}

   /** Blocks until the next time someone calls SignalDispatchThread() on this object, or until (wakeupTime)
     * is reached, whichever comes first.
     * @param wakeupTime the timestamp (eg as returned by GetRunTime64()) at which to give up and return
     *                   B_TIMED_OUT if SignalDispatchThread() hasn't been called by then.  Defaults to MUSCLE_TIME_NEVER.
     * @param optRetNotificationsCount if set non-NULL, then when this method returns successfully, the uint32
     *                   pointed to by this pointer will contain the number of notifications that
     *                   occurred (via calls to SignalDispatchThread()) since the previous time Wait() was called.
     * @returns B_NO_ERROR if this method returned because SignalDispatchThread() was called, or B_TIMED_OUT if the
     *                     timeout time was reached, or some other value if an error occurred.
     * @note if SignalDispatchThread() had already been called before Wait() was called, then Wait() will return immediately.
     *       That way the Wait()-ing thread doesn't have to worry about missing notifications if
     *       it was busy doing something else at the instant SignalDispatchThread() was called.
     */
   status_t Wait(uint64 wakeupTime = MUSCLE_TIME_NEVER, uint32 * optRetNotificationsCount = NULL) const {return _wcRef.Wait(wakeupTime, optRetNotificationsCount);}

protected:
   /** Overridden to call Notify() on our WaitCondition */
   virtual void SignalDispatchThread() {(void) _wcRef.Notify();}

private:
   WaitCondition _defaultWaitCondition;
   WaitCondition & _wcRef;
};

};  // end muscle namespace

#endif
