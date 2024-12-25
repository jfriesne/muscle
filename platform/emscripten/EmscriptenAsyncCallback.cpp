/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <limits.h>  // for INT_MAX
# include <emscripten/emscripten.h>
# include "platform/emscripten/EmscriptenAsyncCallback.h"

namespace muscle {

EmscriptenAsyncCallback :: AsyncCallbackStub :: AsyncCallbackStub(EmscriptenAsyncCallback * master)
   : _master(master)
   , _callbackTime(MUSCLE_TIME_NEVER)
{
   // empty
}

bool EmscriptenAsyncCallback :: AsyncCallbackStub :: AsyncCallback()
{
   (void) _scheduledTimes.RemoveHead();

   if (_master)
   {
      const uint64 now = GetRunTime64();
      if (now >= _callbackTime)
      {
         const uint64 schedTime = _callbackTime;
         _callbackTime = MUSCLE_TIME_NEVER;
         _master->AsyncCallback(schedTime);
      }
      else if (HasCallbackAtOrBefore(_callbackTime) == false) (void) ScheduleCallback(_callbackTime);
   }

   return DecrementRefCount();  // always do this, since this callback means one less data structure pointing to us
}

static void emscripten_async_callback(void * userData)
{
   EmscriptenAsyncCallback::AsyncCallbackStub * stub = reinterpret_cast<EmscriptenAsyncCallback::AsyncCallbackStub *>(userData);
   if (stub->AsyncCallback()) delete stub;
}

bool EmscriptenAsyncCallback :: AsyncCallbackStub :: HasCallbackAtOrBefore(uint64 when) const
{
   for (uint32 i=0; i<_scheduledTimes.GetNumItems(); i++) if (_scheduledTimes[i] <= when) return true;
   return false;
}

status_t EmscriptenAsyncCallback :: AsyncCallbackStub :: SetAsyncCallbackTime(uint64 callbackTime)
{
   if (callbackTime != _callbackTime)
   {
      // If we have no existing callbacks scheduled at or before (callbackTime), we need to schedule one.
      if (HasCallbackAtOrBefore(callbackTime) == false) MRETURN_ON_ERROR(ScheduleCallback(callbackTime));
      _callbackTime = callbackTime;
   }
   return B_NO_ERROR;
}

status_t EmscriptenAsyncCallback :: AsyncCallbackStub :: ScheduleCallback(uint64 callbackTime)
{
   if (_scheduledTimes.InsertItemAtSortedPosition(callbackTime) < 0) return B_OUT_OF_MEMORY;

   const int millisUntil = (int) muscleClamp((int64)0, MicrosToMillisRoundUp(callbackTime-GetRunTime64()), (int64)(INT_MAX));
   emscripten_async_call(emscripten_async_callback, this, (int) millisUntil);
   IncrementRefCount();  // gotta keep ourselves alive until the async callback fires, at least!
   return B_NO_ERROR;
}

};  // end namespace muscle

#endif
