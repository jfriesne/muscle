/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"

namespace muscle {

TCPSocketDataIO :: TCPSocketDataIO(const ConstSocketRef & sock, bool blocking) : _sock(sock), _blocking(true), _naglesEnabled(true), _stallLimit(MUSCLE_DEFAULT_TCP_STALL_TIMEOUT)
{
   if (SetBlockingIOEnabled(blocking).IsError())
   {
#ifndef WIN32  // GetSocketBlockingEnabled() doesn't work under Windows, alas
      _blocking = GetSocketBlockingEnabled(_sock);  // on error, we can at least make sure our state matches the socket's state
#endif
   }
}

TCPSocketDataIO :: ~TCPSocketDataIO()
{
   // empty
}

void TCPSocketDataIO :: FlushOutput()
{
   if ((_naglesEnabled)&&(_sock()))
   {
#if !defined(__APPLE__)  // Apple's implementation of TCP_NOPUSH doesn't flush pending data, and it occasionally causes 5-second delays :(
      MLOG_ON_ERROR("FlushOutput::Cork(false)", SetSocketCorkAlgorithmEnabled(_sock, false));
#endif

      // cork doesn't always transmit the data right away if I don't also toggle Nagle.  --jaf
      MLOG_ON_ERROR("FlushOutput::Nagle(false)", SetSocketNaglesAlgorithmEnabled(_sock, false));

#if !defined(__linux__)
      (void) SendData(_sock, NULL, 0, _blocking);  // Force immediate buffer flush (not necessary under Linux)
#endif

      MLOG_ON_ERROR("FlushOutput::Nagle(false)", SetSocketNaglesAlgorithmEnabled(_sock, true));

#if !defined(__APPLE__)  // Apple's implementation of TCP_NOPUSH doesn't flush pending data, and it occasionally causes 5-second delays :(
      MLOG_ON_ERROR("FlushOutput::Cork(true)", SetSocketCorkAlgorithmEnabled(_sock, true));
#endif
   }
}

status_t TCPSocketDataIO :: SetBlockingIOEnabled(bool blocking)
{
   MRETURN_ON_ERROR(SetSocketBlockingEnabled(_sock, blocking));
   _blocking = blocking;
   return B_NO_ERROR;
}

status_t TCPSocketDataIO :: SetNaglesAlgorithmEnabled(bool enabled)
{
   MRETURN_ON_ERROR(SetSocketNaglesAlgorithmEnabled(_sock, enabled));
   _naglesEnabled = enabled;
   return B_NO_ERROR;
}

} // end namespace muscle
