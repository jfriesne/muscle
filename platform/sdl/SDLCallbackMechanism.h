/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef SDLCallbackMechanism_h
#define SDLCallbackMechanism_h

#include <SDL/SDL_events.h>

#include "util/ICallbackMechanism.h"

namespace muscle {

/** Event code for the SDL event used by the SDLCallbackMechanism class */
static const uint32 SDL_CALLBACK_MECHANISM_EVENT = SDL_NUMEVENTS-1;

/** This is a SDL-specific subclass of ICallbackMechanism.  It calls SDL_PeepEvents(SDL_ADDEVENT) to signal the main SDL thread to dispatch events. */
class SDLCallbackMechanism : public ICallbackMechanism
{
public:
   SDLCallbackMechanism() {/* empty */}

protected:
   /** May be called from any thread; SDL event loop in the main thread should be instrumented to call DispatchCallbacks() on this object when it receives this event. */
   virtual void SignalDispatchThread()
   {
      SDL_Event event;
      event.type       = SDL_CALLBACK_MECHANISM_EVENT;
      event.user.code  = SDL_CALLBACK_MECHANISM_EVENT;
      event.user.data1 = NULL;
      event.user.data2 = NULL;
      SDL_PeepEvents(&event, 1, SDL_ADDEVENT, 0);
   }
};

} // end namespace muscle

#endif
