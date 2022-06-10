/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"

namespace muscle {

TCPSocketDataIO :: TCPSocketDataIO(const ConstSocketRef & sock, bool blocking) : _sock(sock), _blocking(true), _naglesEnabled(true), _stallLimit(MUSCLE_DEFAULT_TCP_STALL_TIMEOUT)
{
   (void) SetBlockingIOEnabled(blocking);
}

TCPSocketDataIO :: ~TCPSocketDataIO() 
{
   Shutdown();
}

void TCPSocketDataIO :: FlushOutput()
{
   if (_sock())
   {
      // cork doesn't always transmit the data right away if I don't also toggle Nagle.  --jaf
#if !defined(__APPLE__)  // Apple's implementation of TCP_NOPUSH doesn't flush pending data, and it occasionally causes 5-second delays :(
      (void) SetSocketCorkAlgorithmEnabled(_sock, false);
#endif

      if (_naglesEnabled)
      {
         (void) SetSocketNaglesAlgorithmEnabled(_sock, false);
#if !defined(__linux__)
         (void) SendData(_sock, NULL, 0, _blocking);  // Force immediate buffer flush (not necessary under Linux)
#endif
         (void) SetSocketNaglesAlgorithmEnabled(_sock, true);
      }

#if !defined(__APPLE__)  // Apple's implementation of TCP_NOPUSH doesn't flush pending data, and it occasionally causes 5-second delays :(
      (void) SetSocketCorkAlgorithmEnabled(_sock, true);
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
