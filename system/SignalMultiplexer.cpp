/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#if defined(__linux__) || defined(__APPLE__)
# define MUSCLE_USE_POSIX_SIGNALS 1
# include <signal.h>
#endif

#include "system/SignalMultiplexer.h"

namespace muscle {

status_t ISignalHandler :: GetNthSignalNumber(uint32 n, int & signalNumber) const
{
#if defined(WIN32)
   switch(n)
   {
      case 0:  signalNumber = CTRL_CLOSE_EVENT;    return B_NO_ERROR;
      case 1:  signalNumber = CTRL_LOGOFF_EVENT;   return B_NO_ERROR;
      case 2:  signalNumber = CTRL_SHUTDOWN_EVENT; return B_NO_ERROR;
   }
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
   switch(n)
   {
      case 0:  signalNumber = SIGINT;  return B_NO_ERROR;
      case 1:  signalNumber = SIGTERM; return B_NO_ERROR;
      case 2:  signalNumber = SIGHUP;  return B_NO_ERROR;
   }
#endif
   return B_ERROR;
}

#if defined(WIN32)
static BOOL Win32SignalHandlerCallbackFunc(DWORD sigNum) {SignalMultiplexer::GetSignalMultiplexer().CallSignalHandlers(sigNum); return true;}
#else
static void POSIXSignalHandlerCallbackFunc(int sigNum)   {SignalMultiplexer::GetSignalMultiplexer().CallSignalHandlers(sigNum);}
#endif

status_t SignalMultiplexer :: AddHandler(ISignalHandler * s) 
{
   MutexGuard m(_mutex);
   if (_handlers.AddTail(s) != B_NO_ERROR) return B_ERROR;
   if (UpdateSignalSets() == B_NO_ERROR) return B_NO_ERROR;
   else
   {
      _handlers.RemoveTail();
      return B_ERROR;
   }
}

void SignalMultiplexer :: RemoveHandler(ISignalHandler * s) 
{
   MutexGuard m(_mutex);
   if (_handlers.RemoveFirstInstanceOf(s) == B_NO_ERROR) (void) UpdateSignalSets();
}

void SignalMultiplexer :: CallSignalHandlers(int sigNum)
{
   _totalSignalCounts++;
   if (muscleInRange(sigNum, 0, (int)(ARRAYITEMS(_signalCounts)-1))) _signalCounts[sigNum]++;

   // Can't lock the Mutex here because we are being called within a signal context!
   // So we just have to hope that _handlers won't change while we do this
   for (uint32 i=0; i<_handlers.GetNumItems(); i++) _handlers[i]->SignalHandlerFunc(sigNum);
}

status_t SignalMultiplexer :: UpdateSignalSets()
{
   Queue<int> newSignalSet;
   for (uint32 i=0; i<_handlers.GetNumItems(); i++)
   {
      const ISignalHandler * s = _handlers[i];
      int sigNum;
      for (uint32 i=0; s->GetNthSignalNumber(i, sigNum) == B_NO_ERROR; i++) if ((newSignalSet.IndexOf(sigNum) < 0)&&(newSignalSet.AddTail(sigNum) != B_NO_ERROR)) return B_ERROR;
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
   return ((_currentSignalSet.IsEmpty())||(SetConsoleCtrlHandler((PHANDLER_ROUTINE) Win32SignalHandlerCallbackFunc, true))) ? B_NO_ERROR : B_ERROR;
#elif defined(MUSCLE_USE_POSIX_SIGNALS)
   struct sigaction newact;
   sigemptyset(&newact.sa_mask);
   newact.sa_flags   = 0;
   newact.sa_handler = POSIXSignalHandlerCallbackFunc;  /*set the new handler*/
   for (uint32 i=0; i<_currentSignalSet.GetNumItems(); i++)
   {
      int sigNum = _currentSignalSet[i];
      if (sigaction(sigNum, &newact, NULL) == -1) 
      {
         LogTime(MUSCLE_LOG_WARNING, "Could not install signal handler for signal #%i\n", sigNum);
         UnregisterSignals();
         return B_ERROR;
      }
   }
   return B_NO_ERROR;
#else
   return B_ERROR;
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
   for (uint32 i=0; i<_currentSignalSet.GetNumItems(); i++) (void) sigaction(_currentSignalSet[i], NULL, NULL);
#endif
}

SignalMultiplexer :: SignalMultiplexer() : _totalSignalCounts(0)
{
   for (uint32 i=0; i<ARRAYITEMS(_signalCounts); i++) _signalCounts[i] = 0;
}

SignalMultiplexer SignalMultiplexer::_signalMultiplexer;

}; // end namespace muscle
