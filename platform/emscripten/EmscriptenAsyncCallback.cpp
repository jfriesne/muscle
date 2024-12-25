/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <limits.h>  // for INT_MAX
# include <emscripten/emscripten.h>
# include "platform/emscripten/EmscriptenAsyncCallback.h"

namespace muscle {

EmscriptenAsyncCallback :: EmscriptenAsyncCallback()
{
   // empty
}

EmscriptenAsyncCallback :: ~EmscriptenAsyncCallback()
{
   if (_stub()) _stub()->ForgetMaster();
}

status_t EmscriptenAsyncCallback :: SetAsyncCallbackTime(uint64 callbackTime)
{
#if defined(__EMSCRIPTEN__)
   if (_stub() == NULL) _stub.SetRef(new AsyncCallbackStub(this));
   return _stub()->SetAsyncCallbackTime(callbackTime);
#else
   (void) callbackTime;
   return B_UNIMPLEMENTED;
#endif
}

uint64 EmscriptenAsyncCallback :: GetAsyncCallbackTime() const
{
#if defined(__EMSCRIPTEN__)
   return _stub() ? _stub()->GetAsyncCallbackTime() : MUSCLE_TIME_NEVER;
#else
   return MUSCLE_TIME_NEVER;
#endif
}

EmscriptenAsyncCallback :: AsyncCallbackStub :: AsyncCallbackStub(EmscriptenAsyncCallback * master)
   : _master(master)
   , _callbackTime(MUSCLE_TIME_NEVER)
{
   // empty
}

EmscriptenAsyncCallback :: AsyncCallbackStub :: ~AsyncCallbackStub()
{
   // empty
}

bool EmscriptenAsyncCallback :: AsyncCallbackStub :: AsyncCallback()
{
   const bool justEmptiedQueue = ((_scheduledTimes.RemoveHead().IsOK())&&(_scheduledTimes.IsEmpty()));

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

   return justEmptiedQueue ? DecrementRefCount() : false;
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
   const bool wasEmpty = _scheduledTimes.IsEmpty();
   if (_scheduledTimes.InsertItemAtSortedPosition(callbackTime) < 0) return B_OUT_OF_MEMORY;

   const int millisUntil = (int) muscleClamp((int64)0, MicrosToMillisRoundUp(callbackTime-GetRunTime64()), (int64)(INT_MAX));
   emscripten_async_call(emscripten_async_callback, this, millisUntil);

   if (wasEmpty) IncrementRefCount();  // gotta keep ourselves alive until the async callback fires, at least!
   return B_NO_ERROR;
}

};  // end namespace muscle

#endif
