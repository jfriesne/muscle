/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef JUCECallbackMechanism_h
#define JUCECallbackMechanism_h

// Use this (for every JUCE module) instead of the specific headers because they don't have include guards.
#include <juce_events/juce_events.h>

#include "util/ICallbackMechanism.h"
#include "util/NestCount.h"

namespace muscle {

/** This is a JUCE-specific subclass of ICallbackMechanism. */
class JUCECallbackMechanism : public ICallbackMechanism, private juce::AsyncUpdater
{
public:
   /** Constructor */
   virtual JUCECallbackMechanism() {/* empty */}

protected:
   /** May be called from any thread; triggers an asynchronous call to DispatchCallbacks() within the main thread */
   virtual void SignalDispatchThread() {triggerAsyncUpdate();}

   /** Called by JUCE's AsyncUpdater, in the main thread, when it is time for muscle-callbacks to be called */
   virtual void handleAsyncUpdate()
   {
      NestCountGuard ncg(_handleAsyncNestCount);
      if (ncg.IsOutermost()) DispatchCallbacks();
                        else triggerAsyncUpdate();  // If JUCE called us re-entrantly rather than asyncronously, then we need to schedule the call again for later
   }

private:
   NestCount _handleAsyncNestCount;  // avoid problems with JUCE calling handleAsyncUpdate() re-entrantly
};

}  // end muscle namespace

#endif
