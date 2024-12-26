/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_ASYNC_CALLBACK_H
#define EMSCRIPTEN_ASYNC_CALLBACK_H

#include "support/NotCopyable.h"
#include "util/Queue.h"
#include "util/RefCount.h"
#include "util/TimeUtilityFunctions.h"  // for MUSCLE_TIME_NEVER

namespace muscle {

/** This class manages an asynchronous callback from Emscripten */
class EmscriptenAsyncCallback : private NotCopyable
{
public:
   /** Default constructor */
   EmscriptenAsyncCallback();

   /** Destructor */
   virtual ~EmscriptenAsyncCallback();

   /** Schedules a time for the callback to be called.
     * If a callback is already pending, if will be rescheduled to this new time
     * @param callbackTime the time at which we want the callback to be called,
     *                     or MUSCLE_TIME_NEVER to just cancel any pending callback.
     * @note specifying a (callbackTime) that is less than the current value returned
     *       by GetRunTime64() will result in the callback being called ASAP (i.e. on
     *       the next iteration of the event loop)
     * @returns B_NO_ERROR on success, or B_UNIMPLEMENTED if we haven't been compiled to use Emscripten.
     */
   status_t SetAsyncCallbackTime(uint64 callbackTime);

   /** Returns the timestamp of our next call to AsyncCallback(), or MUSCLE_TIME_NEVER if none is scheduled. */
   uint64 GetAsyncCallbackTime() const;

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
   class AsyncCallbackStub : public RefCountable, private NotCopyable
   {
   public:
      AsyncCallbackStub(EmscriptenAsyncCallback * master);
      ~AsyncCallbackStub();

      uint64 GetAsyncCallbackTime() const {return _callbackTime;}
      status_t SetAsyncCallbackTime(uint64 callbackTime);

      void ForgetMaster() {_master = NULL;}
      bool AsyncCallback();

   private:
      status_t ScheduleCallback(uint64 when);
      bool HasCallbackAtOrBefore(uint64 when) const;

      EmscriptenAsyncCallback * _master;
      uint64 _callbackTime;

      Queue<uint64> _scheduledTimes;
      uint32 _timingOkayCount;
   };
   DECLARE_REFTYPES(AsyncCallbackStub);
   friend class AsyncCallbackStub;

   AsyncCallbackStubRef _stub;
#endif
};

};  // end namespace muscle

#endif
