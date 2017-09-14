/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSDLMessageTransceiverThread_h
#define MuscleSDLMessageTransceiverThread_h

#include <SDL/SDL_events.h>
#include "system/MessageTransceiverThread.h"

namespace muscle {

static const uint32 SDL_MTT_EVENT = SDL_NUMEVENTS-1;

/** This class is useful for interfacing a MessageTransceiverThread to the SDL event loop.
 *  @author Shard
 */
class SDLMessageTransceiverThread : public MessageTransceiverThread
{
public:
   /** Constructor. */
   SDLMessageTransceiverThread();

protected:
   /** Overridden to send a SDLEvent */
   virtual void SignalOwner();
};
DECLARE_REFTYPES(SDLMessageTransceiverThread);

} // end namespace muscle

#endif
