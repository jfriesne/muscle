/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef SDLCallbackMechanism_h
#define SDLCallbackMechanism_h

#ifndef MUSCLE_AVOID_CPLUSPLUS11
# include <atomic>
#endif

#include <SDL/SDL_events.h>

#include "util/ICallbackMechanism.h"

namespace muscle {

/** Event code for the SDL event used by the SDLCallbackMechanism class */
static const uint32 SDL_CALLBACK_MECHANISM_EVENT = SDL_NUMEVENTS-1;

/** This is a SDL-specific subclass of ICallbackMechanism.  It calls SDL_PeepEvents(SDL_ADDEVENT) to signal the main SDL thread to dispatch events. */
class SDLCallbackMechanism : public ICallbackMechanism
{
public:
   SDLCallbackMechanism()
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      : _signalPending(false)
#endif
   {
      // empty
   }

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   virtual void DispatchCallbacks()
   {
      _signalPending.store(false);
      ICallbackMechanism::DispatchCallbacks();
   }
#endif

protected:
   /** May be called from any thread; SDL event loop in the main thread should be instrumented to call DispatchCallbacks() on this object when it receives this event. */
   virtual void SignalDispatchThread()
   {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      bool expected = false;
      if (_signalPending.compare_exchange_strong(expected, true) == false) return;  // avoid piling up events
#endif

      SDL_Event event;
      event.type       = SDL_CALLBACK_MECHANISM_EVENT;
      event.user.code  = SDL_CALLBACK_MECHANISM_EVENT;
      event.user.data1 = NULL;
      event.user.data2 = NULL;
      if (SDL_PeepEvents(&event, 1, SDL_ADDEVENT, 0) == -1)
      {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
         _signalPending.store(false);  // to allow a future retry
#endif
         LogTime(MUSCLE_LOG_ERROR, "SDLCallbackMechanism::SignalDispatchThread():  SDL_PeepEvents() returned an error!\n");
      }
   }

private:
#ifndef MUSCLE_AVOID_CPLUSPLUS11
   std::atomic<bool> _signalPending;
#endif
};

} // end namespace muscle

#endif
