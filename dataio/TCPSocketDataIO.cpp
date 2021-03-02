/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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
      // Note:  I use both cork AND Nagle because cork is a no-op outside of Linux,
      // and inside of Linux cork doesn't always transmit the data right away if I don't also toggle Nagle.  --jaf
      (void) SetSocketCorkAlgorithmEnabled(_sock, false);
      if (_naglesEnabled)
      {
         SetSocketNaglesAlgorithmEnabled(_sock, false);
# if !defined(__linux__)
         (void) SendData(_sock, NULL, 0, _blocking);  // Force immediate buffer flush (not necessary under Linux)
# endif
         SetSocketNaglesAlgorithmEnabled(_sock, true);
      }
      (void) SetSocketCorkAlgorithmEnabled(_sock, true);
   }
}
   
status_t TCPSocketDataIO :: SetBlockingIOEnabled(bool blocking)
{
   const status_t ret = SetSocketBlockingEnabled(_sock, blocking);
   if (ret.IsOK()) _blocking = blocking;
   return ret;
}

status_t TCPSocketDataIO :: SetNaglesAlgorithmEnabled(bool enabled)
{
   const status_t ret = SetSocketNaglesAlgorithmEnabled(_sock, enabled);
   if (ret.IsOK()) _naglesEnabled = enabled;
   return ret;
}

} // end namespace muscle
