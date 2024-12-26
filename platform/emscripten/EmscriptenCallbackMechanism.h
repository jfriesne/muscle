/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EmscriptenCallbackMechanism_h
#define EmscriptenCallbackMechanism_h

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "util/ICallbackMechanism.h"

namespace muscle {

/** This class implements the ICallbackMechanism interface using an EmscriptenAsyncCallback as its signalling mechanism.  */
class EmscriptenCallbackMechanism : public ICallbackMechanism, private EmscriptenAsyncCallback
{
public:
   /** Default Constructor */
   EmscriptenCallbackMechanism() {/* empty */}

   /** Destructor */
   virtual ~EmscriptenCallbackMechanism() {/* empty */}

protected:
   /** Overridden to request an asynchronous callback from the Esmcripten callback engine */
   virtual void SignalDispatchThread() {(void) SetAsyncCallbackTime(0);}

private:
   virtual void AsyncCallback(uint64 scheduledTime) {(void) scheduledTime; DispatchCallbacks();}
};

};  // end muscle namespace

#endif
