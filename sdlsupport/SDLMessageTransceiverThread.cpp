/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "sdlsupport/SDLMessageTransceiverThread.h"

namespace muscle {

SDLMessageTransceiverThread :: SDLMessageTransceiverThread()
   : MessageTransceiverThread()
{
   // empty
}

void SDLMessageTransceiverThread :: SignalOwner()
{
   SDL_Event event;
   event.type       = SDL_MTT_EVENT;
   event.user.code  = SDL_MTT_EVENT;
   event.user.data1 = NULL;
   event.user.data2 = NULL;
   SDL_PeepEvents(&event, 1, SDL_ADDEVENT, 0);
}

} // end namespace muscle
