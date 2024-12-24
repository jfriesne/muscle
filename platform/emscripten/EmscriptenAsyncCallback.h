/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_ASYNC_CALLBACK_H
#define EMSCRIPTEN_ASYNC_CALLBACK_H

#include "util/RefCount.h"
#include "util/TimeUtilityFunctions.h"  // for MUSCLE_TIME_NEVER

namespace muscle {

/** This class manages an asynchronous callback from Emscripten */
class EmscriptenAsyncCallback
{
public:
   /** Default constructor */
   EmscriptenAsyncCallback() {/* empty */}

   /** Destructor */
   virtual ~EmscriptenAsyncCallback()
   {
#if defined(__EMSCRIPTEN__)
      if (_stub()) _stub()->ForgetMaster();
#endif
   }

   /** Schedules a time for the callback to be called.
     * If a callback is already pending, if will be rescheduled to this new time
     * @param callbackTime the time at which we want the callback to be called,
     *                     or MUSCLE_TIME_NEVER to just cancel any pending callback.
     * @note specifying a (callbackTime) that is less than the current value returned
     *       by GetRunTime64() will result in the callback being called ASAP (i.e. on
     *       the next iteration of the event loop)
     * @returns B_NO_ERROR on success, or B_UNIMPLEMENTED if we haven't been compiled to use Emscripten.
     */
   status_t SetAsyncCallbackTime(uint64 callbackTime)
   {
#if defined(__EMSCRIPTEN__)
      if (_stub() == NULL) _stub.SetRef(new AsyncCallbackStub(this));
      return _stub()->SetAsyncCallbackTime(callbackTime);
#else
      (void) callbackTime;
      return B_UNIMPLEMENTED;
#endif
   }

   /** Returns the timestamp of our next call to AsyncCallback(), or MUSCLE_TIME_NEVER if none is scheduled. */
   uint64 GetAsyncCallbackTime() const
   {
#if defined(__EMSCRIPTEN__)
      return _stub() ? _stub()->GetAsyncCallbackTime() : MUSCLE_TIME_NEVER;
#else
      return MUSCLE_TIME_NEVER;
#endif
   }

protected:
   /** The asynchronous callback.  We aim to have this method called as close as
     * possible to the time that was previously passed to SetCallbackTime().
     * @param scheduledTime the time this callback was scheduled to occur at (actual call time may vary somewhat)
     */
   virtual void AsyncCallback(uint64 scheduledTime) = 0;

public:
#if defined(__EMSCRIPTEN__)
   // This stub is just here so we can safely destroy the EmscriptenAsyncCallback object
   // and not have to worry about any lingering async-callbacks trying to access it afterwards
   class AsyncCallbackStub : public RefCountable
   {
   public:
      AsyncCallbackStub(EmscriptenAsyncCallback * master);

      uint64 GetAsyncCallbackTime() const {return _callbackTime;}
      status_t SetAsyncCallbackTime(uint64 callbackTime);

      void ForgetMaster() {_master = NULL;}
      bool AsyncCallback();

   private:
      void ScheduleCallback();

      EmscriptenAsyncCallback * _master;
      uint64 _callbackTime;
   };
   DECLARE_REFTYPES(AsyncCallbackStub);
   friend class AsyncCallbackStub;

   AsyncCallbackStubRef _stub;
#endif
};

};  // end namespace muscle

#endif
