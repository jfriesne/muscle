/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/UDPSocketDataIO.h"

namespace muscle {

UDPSocketDataIO :: UDPSocketDataIO(const ConstSocketRef & sock, bool blocking) : _sock(sock), _maxPacketSize(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET)
{
   (void) SetBlockingIOEnabled(blocking);
   _sendTo.AddTail();  // so that by default, Write() will just call send() on our socket
}

UDPSocketDataIO :: ~UDPSocketDataIO()
{
   // empty
}

io_status_t UDPSocketDataIO :: ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retSource)
{
   IPAddress tmpAddr = invalidIP;
   uint16 tmpPort = 0;
   const io_status_t ret = ReceiveDataUDP(_sock, buffer, size, _blocking, &tmpAddr, &tmpPort);
   if (ret.IsOK())
   {
      retSource.Set(tmpAddr, tmpPort);
      SetSourceOfLastReadPacket(retSource);  // in case this is a direct call e.g. from the gateway code
   }
   return ret;
}

io_status_t UDPSocketDataIO :: Write(const void * buffer, uint32 size)
{
   if (_sendTo.IsEmpty()) return size;  // with no destinations, we'll just act as a data-sink, for consistency

   // This method is overridden to support multiple destinations too
   status_t seenError;
   bool sawSuccess = false;
   int32 maxSentBytes = 0;
   for (uint32 i=0; i<_sendTo.GetNumItems(); i++)
   {
      const io_status_t numBytesSent = WriteTo(buffer, size, _sendTo[i]);
      if (numBytesSent.IsOK())
      {
         sawSuccess = true;
         maxSentBytes = muscleMax(numBytesSent.GetByteCount(), maxSentBytes);
      }
      else seenError = numBytesSent.GetStatus();
   }
   return ((seenError.IsError())&&(sawSuccess == false)) ? io_status_t(seenError) : io_status_t(maxSentBytes);
}

io_status_t UDPSocketDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
{
   return SendDataUDP(_sock, buffer, size, _blocking, packetDest.GetIPAddress(), packetDest.GetPort());
}

status_t UDPSocketDataIO :: SetBlockingIOEnabled(bool blocking)
{
   MRETURN_ON_ERROR(SetSocketBlockingEnabled(_sock, blocking));
   _blocking = blocking;
   return B_NO_ERROR;
}

} // end namespace muscle
