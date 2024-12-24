/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <limits.h>  // for INT_MAX
# include <emscripten/websocket.h>
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
   if (_master)
   {
      const uint64 schedTime = _callbackTime;
      const uint64 now       = GetRunTime64();
      if (now >= schedTime)
      {
         _callbackTime = MUSCLE_TIME_NEVER;
         _master->AsyncCallback(schedTime);
      }
      else ScheduleCallback();  // we were called too soon!  Reschedule the callback for later
   }

   return DecrementRefCount();  // always do this, since this callback means one less data structure pointing to us
}

static void emscripten_async_callback(void * userData)
{
   EmscriptenAsyncCallback::AsyncCallbackStub * stub = reinterpret_cast<EmscriptenAsyncCallback::AsyncCallbackStub *>(userData);
   if (stub->AsyncCallback()) delete stub;
}

status_t EmscriptenAsyncCallback :: AsyncCallbackStub :: SetAsyncCallbackTime(uint64 callbackTime)
{
   if (callbackTime != _callbackTime)
   {
      const bool needItSooner = (callbackTime < _callbackTime);
      _callbackTime = callbackTime;
      if (needItSooner) ScheduleCallback();
   }
   return B_NO_ERROR;
}

void EmscriptenAsyncCallback :: AsyncCallbackStub :: ScheduleCallback()
{
   const int millisUntil = (int) muscleClamp((int64)0, MicrosToMillisRoundUp(_callbackTime-GetRunTime64()), (int64)(INT_MAX));
   emscripten_async_call(emscripten_async_callback, this, (int) millisUntil);
   IncrementRefCount();  // gotta keep ourselves alive until the async callback fires, at least!
}

};  // end namespace muscle

#endif
