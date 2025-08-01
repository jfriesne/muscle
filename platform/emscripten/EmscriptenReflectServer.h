/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_REFLECT_SERVER_H
#define EMSCRIPTEN_REFLECT_SERVER_H

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "reflector/ReflectServer.h"

namespace muscle {

/** This ReflectServer also incorporates an EmscriptenAsyncCallback so that it
  * can drive its event-loop from Emscripten callbacks rather than requiring a separate thread.
  */
class EmscriptenReflectServer : public ReflectServer, public EmscriptenAsyncCallback
{
public:
   /** Default constructor */
   EmscriptenReflectServer() : _isRunning(false) {/* empty */}

   /** Destructor */
   virtual ~EmscriptenReflectServer() {/* empty */}

   /** Call this to start the ReflectServer "running" (via async callbacks)
     * @returns B_NO_ERROR on success, or B_UNIMPLEMENTED if we aren't in an Emscripten environment, or B_ALREADY_RUNNING if we were already started
     */
   status_t Start()
   {
#if defined(__EMSCRIPTEN__)
      if (_isRunning) return B_ALREADY_RUNNING;
      else
      {
         _isRunning = true;
         AsyncCallback(0);  // just to get the ball rolling
         return B_NO_ERROR;
      }
#else
      return B_UNIMPLEMENTED;
#endif
   }

   /** Call this to stop the ReflectServer's "running".  Has no effect if we aren't currently running. */
   void Stop() {_isRunning = false;}

   /** Returns true if we are currently in our running mode (i.e. Start() has been called but not Stop()) */
   bool IsRunning() const {return _isRunning;}

protected:
   virtual void AsyncCallback(uint64 /*scheduledTime*/)
   {
      if (_isRunning)
      {
         uint64 nextPulseTime = MUSCLE_TIME_NEVER;
         (void) ServerProcessLoop(0, &nextPulseTime); // just run the event-loop for one iteration and return ASAP
         (void) SetAsyncCallbackTime(nextPulseTime);  // schedule the next callback to handle any upcoming Pulse() calls
      }
   }

private:
   bool _isRunning;
};

};  // end namespace muscle

#endif
