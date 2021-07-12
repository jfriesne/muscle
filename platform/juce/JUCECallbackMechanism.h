/* This file is Copyright 2002 Level Control Systems.  See the included LICENSE.txt file for details. */

#ifndef JUCECallbackMechanism_h
#define JUCECallbackMechanism_h

// Use this (for every JUCE module) instead of the specific headers because they don't have include guards.
#include <juce_events/juce_events.h>

#include "support/MuscleSupport.h"
#include "util/ICallbackMechanism.h"

namespace muscle {

/** This is a JUCE-specific subclass of ICallbackMechanism. */
class JUCECallbackMechanism : public ICallbackMechanism, private juce::AsyncUpdater
{
public:
   /** Destructor */
   virtual ~JUCECallbackMechanism() {}

protected:
   /** May be called from any thread; triggers an asynchronous call to DispatchCallbacks() within the main thread */
   virtual void SignalDispatchThread() {triggerAsyncUpdate();}

   /** Called by JUCE's AsyncUpdater, in the main thread, when it is time for muscle-callbacks to be called */
   virtual void handleAsyncUpdate() {DispatchCallbacks();}
};

}  // end muscle namespace

#endif
