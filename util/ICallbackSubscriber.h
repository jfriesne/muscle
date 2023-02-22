/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ICallbackSubscriber_h
#define ICallbackSubscriber_h

#include "util/ICallbackMechanism.h"

namespace muscle {

class ICallbackMechanism;

/** Interface to any object that has operations running in a background thread, but also wants to
  * be able to have its callback-methods called in the main/dispatch thread.
  * To be used in conjunction with an ICallbackMechanism.
  */
class ICallbackSubscriber
{
public:
   /** Constructor
     * @param mechanism the ICallbackMechanism to register with.  May be NULL, if you plan
     *                 to call SetCallbackMechanism() later, instead.
     */
   ICallbackSubscriber(ICallbackMechanism * mechanism) : _mechanism(NULL) {SetCallbackMechanism(mechanism);}

   /** Destructor.  Calls SetCallbackMechanism(NULL). */
   virtual ~ICallbackSubscriber() {SetCallbackMechanism(NULL);}

   /** Sets our callback mechanism.  To be called from the main/dispatch thread only (typically from our constructor)
     * @param mechanism pointer to the new CallbackMechanism to register with, or NULL to deregister from any existing CallbackMechanism.
     */
   void SetCallbackMechanism(ICallbackMechanism * mechanism)
   {
      if (mechanism != _mechanism)
      {
         if (_mechanism) _mechanism->UnregisterCallbackSubscriber(this);
         _mechanism = mechanism;
         if (_mechanism) _mechanism->RegisterCallbackSubscriber(this);
      }
   }

   /** Returns a pointer to the CallbackMechanism we're currently registered with,
     * or NULL if we aren't currently registered.
     */
   ICallbackMechanism * GetCallbackMechanism() const {return _mechanism;}

protected:
   /** May be called from any thread.  Calling this method ensures that
     * DispatchCallbacks() will be called in the main/dispatch thread in the very near future.
     * @param eventTypeBits Optional bit-chord indicating which event-types you want to flag
     *                      as requiring attention.  The definition of these bits is left to
     *                      the subclass.  The default value of this argument is ~0, ie an
     *                      indication that every type of event should be serviced.
     * @param clearEventTypeBits Option bit-chord indicating any event-type bits you want
     *                           to remove from the pending-event-types flag.  Defaults to zero.
     */
   void RequestCallbackInDispatchThread(uint32 eventTypeBits = ~0, uint32 clearEventTypeBits = 0)
   {
      if (_mechanism) _mechanism->RequestCallbackInDispatchThread(this, eventTypeBits, clearEventTypeBits);
   }

   /** Called by our ICallbackMechanism, from the main/dispatch thread.
     * All user-facing callbacks should be done here, since data strutures
     * owned by the main/dispatch thread can be safely accessed within this context.
     * @param eventTypeBits the bit-chord of event-type bits that needs servicing.  This
     *                      will be the union of all values that were previously passed in
     *                      to RequestCallbackInDispatchThread() calls previous to this callback.
     */
   virtual void DispatchCallbacks(uint32 eventTypeBits) = 0;

private:
   friend class ICallbackMechanism;

   ICallbackMechanism * _mechanism;
};

};  // end muscle namespace

#endif
