/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <limits.h>  // for INT_MAX
# include <emscripten/websocket.h>
#endif

#include "platform/emscripten/EmscriptenAsyncCallback.h"

namespace muscle {

EmscriptenAsyncCallback :: ~EmscriptenAsyncCallback()
{
   if (_stub()) _stub()->ForgetMaster();  // in case the stub outlives us due to pending async callbacks
}

status_t EmscriptenAsyncCallback :: SetAsyncCallbackTime(uint64 callbackTime)
{
   if (_stub() == NULL) _stub.SetRef(new AsyncCallbackStub(this));
   return _stub()->SetAsyncCallbackTime(callbackTime);
}

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

#if defined(__EMSCRIPTEN__)
static void emscripten_async_callback(void * userData)
{
   EmscriptenAsyncCallback::AsyncCallbackStub * stub = reinterpret_cast<EmscriptenAsyncCallback::AsyncCallbackStub *>(userData);
   if (stub->AsyncCallback()) delete stub;
}
#endif

status_t EmscriptenAsyncCallback :: AsyncCallbackStub :: SetAsyncCallbackTime(uint64 callbackTime)
{
#if defined(__EMSCRIPTEN__)
   if (callbackTime != _callbackTime)
   {
      const bool needItSooner = (callbackTime < _callbackTime);
      _callbackTime = callbackTime;
      if (needItSooner) ScheduleCallback();
   }
   return B_NO_ERROR;
#else
   (void) callbackTime;
   return B_UNIMPLEMENTED;
#endif
}

void EmscriptenAsyncCallback :: AsyncCallbackStub :: ScheduleCallback()
{
#if defined(__EMSCRIPTEN__)
   const int millisUntil = (int) muscleClamp((int64)0, MicrosToMillis(_callbackTime-GetRunTime64()), (int64)(INT_MAX));
   emscripten_async_call(emscripten_async_callback, this, (int) millisUntil);
   IncrementRefCount();  // gotta keep ourselves alive until the async callback fires, at least!
#endif
}

};  // end namespace muscle
