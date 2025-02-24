/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__)
# define MUSCLE_USE_POSIX_SIGNALS 1
# include <signal.h>
#endif

#include "system/SignalMultiplexer.h"

namespace muscle {

void SignalEventInfo :: Flatten(DataFlattener flat) const
{
   flat.WriteInt32((int32) _sigNum);
   flat.WriteInt64((int64) _fromProcessID);
}

status_t SignalEventInfo :: Unflatten(DataUnflattener & unflat)
{
   _sigNum        = (int) unflat.ReadInt32();
   _fromProcessID = (muscle_pid_t) unflat.ReadInt64();
   return unflat.GetStatus();
}

status_t ISignalHandler :: GetNthSignalNumber(uint32 n, int & signalNumber) const
{
#if defined(WIN32)
   switch(n)
   {
      case 0:  signalNumber = CTRL_C_EVENT;        return B_NO_ERROR;
      case 1:  signalNumber = CTRL_BREAK_EVENT;    return B_NO_ERROR;
      case 2:  signalNumber = CTRL_CLOSE_EVENT;    return B_NO_ERROR;
      case 3:  signalNumber = CTRL_LOGOFF_EVENT;   return B_NO_ERROR;
      case 4:  signalNumber = CTRL_SHUTDOWN_EVENT; return B_NO_ERROR;
      default: /* empty */                         return B_BAD_ARGUMENT;
   }
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
   switch(n)
   {
      case 0:  signalNumber = SIGINT;  return B_NO_ERROR;
      case 1:  signalNumber = SIGTERM; return B_NO_ERROR;
      case 2:  signalNumber = SIGHUP;  return B_NO_ERROR;
      default: /* empty */             return B_BAD_ARGUMENT;
   }
#endif
   return B_UNIMPLEMENTED;
}

#if defined(WIN32)
static BOOL Win32SignalHandlerCallbackFunc(DWORD sigNum) {SignalMultiplexer::GetSignalMultiplexer().CallSignalHandlers(SignalEventInfo(sigNum, (muscle_pid_t)0)); return true;}
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
static void POSIXSignalHandlerCallbackFunc(int sigNum, siginfo_t * info, void *) {SignalMultiplexer::GetSignalMultiplexer().CallSignalHandlers(SignalEventInfo(sigNum, info->si_pid));}
#endif

status_t SignalMultiplexer :: AddHandler(ISignalHandler * s)
{
   DECLARE_MUTEXGUARD(_mutex);
   MRETURN_ON_ERROR(_handlers.AddTail(s));

   status_t ret;
   if (UpdateSignalSets().IsOK(ret)) return B_NO_ERROR;
   else
   {
      (void) _handlers.RemoveTail();  // roll back!
      return ret;
   }
}

void SignalMultiplexer :: RemoveHandler(ISignalHandler * s)
{
   DECLARE_MUTEXGUARD(_mutex);
   if (_handlers.RemoveFirstInstanceOf(s).IsOK()) (void) UpdateSignalSets();
}

void SignalMultiplexer :: CallSignalHandlers(const SignalEventInfo & sei)
{
   const int sigNum = sei.GetSignalNumber();

   (void) _totalSignalCounts.AtomicIncrement();
   if (muscleInRange(sigNum, 0, (int)(ARRAYITEMS(_signalCounts)-1))) (void) _signalCounts[sigNum].AtomicIncrement();

   // Can't lock the Mutex here because we are being called within a signal context!
   // So we just have to hope that _handlers won't change while we do this
   for (uint32 i=0; i<_handlers.GetNumItems(); i++) _handlers[i]->SignalHandlerFunc(sei);
}

status_t SignalMultiplexer :: UpdateSignalSets()
{
   Queue<int> newSignalSet;
   for (uint32 i=0; i<_handlers.GetNumItems(); i++)
   {
      const ISignalHandler * s = _handlers[i];
      int sigNum;
      for (uint32 j=0; s->GetNthSignalNumber(j, sigNum).IsOK(); j++) if (newSignalSet.IndexOf(sigNum) < 0) MRETURN_ON_ERROR(newSignalSet.AddTail(sigNum));
   }
   newSignalSet.Sort();

#ifdef WIN32
   // For Windows, we only need to register/unregister the callback function, not the individual signals
   if (newSignalSet.HasItems() == _currentSignalSet.HasItems())
   {
      _currentSignalSet.SwapContents(newSignalSet);
      return B_NO_ERROR;
   }
#else
   if (newSignalSet == _currentSignalSet) return B_NO_ERROR;  // already done!
#endif

   UnregisterSignals();
   _currentSignalSet.SwapContents(newSignalSet);
   return RegisterSignals();
}

status_t SignalMultiplexer :: RegisterSignals()
{
#if defined(WIN32)
   if (_currentSignalSet.IsEmpty()) return B_BAD_OBJECT;
   return SetConsoleCtrlHandler((PHANDLER_ROUTINE) Win32SignalHandlerCallbackFunc, true) ? B_NO_ERROR : B_ERRNO;
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
   struct sigaction newact; memset(&newact, 0, sizeof(newact));
   newact.sa_flags     = SA_SIGINFO;
   newact.sa_sigaction = POSIXSignalHandlerCallbackFunc;  /*set the new handler*/
   sigemptyset(&newact.sa_mask);

   for (uint32 i=0; i<_currentSignalSet.GetNumItems(); i++)
   {
      const int sigNum = _currentSignalSet[i];
      if (sigaction(sigNum, &newact, NULL) == -1)
      {
         const status_t ret = B_ERRNO;
         LogTime(MUSCLE_LOG_WARNING, "Could not install signal handler for signal #%i [%s]\n", sigNum, ret());
         UnregisterSignals();
         return ret;
      }
   }
   return B_NO_ERROR;
#else
   return B_UNIMPLEMENTED;
#endif
}

void SignalMultiplexer :: UnregisterSignals()
{
#if defined(WIN32)
   if (_currentSignalSet.HasItems()) (void) SetConsoleCtrlHandler((PHANDLER_ROUTINE)Win32SignalHandlerCallbackFunc, false);
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
   struct sigaction newact;
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0;
   newact.sa_handler = NULL;
   for (uint32 i=0; i<_currentSignalSet.GetNumItems(); i++) (void) sigaction(_currentSignalSet[i], &newact, NULL);
#endif
}

SignalMultiplexer SignalMultiplexer::_signalMultiplexer;

} // end namespace muscle
