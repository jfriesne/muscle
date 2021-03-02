/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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

int32 UDPSocketDataIO :: ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retSource) 
{
   IPAddress tmpAddr = invalidIP;
   uint16 tmpPort = 0;
   const int32 ret = ReceiveDataUDP(_sock, buffer, size, _blocking, &tmpAddr, &tmpPort);
   retSource.Set(tmpAddr, tmpPort);
   SetSourceOfLastReadPacket(retSource);  // in case this is a direct call e.g. from the gateway code
   return ret;
}

int32 UDPSocketDataIO :: Write(const void * buffer, uint32 size) 
{
   // This method is overridden to support multiple destinations too
   bool sawErrors  = false;
   bool sawSuccess = false;
   int32 ret = 0;
   for (uint32 i=0; i<_sendTo.GetNumItems(); i++) 
   {
      const int32 numBytesSent = WriteTo(buffer, size, _sendTo[i]);
      if (numBytesSent >= 0) sawSuccess = true;
                        else sawErrors  = true;
      ret = muscleMax(numBytesSent, ret);
   }
   return ((sawErrors)&&(sawSuccess == false)) ? -1 : ret;
}

int32 UDPSocketDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest) 
{
   return SendDataUDP(_sock, buffer, size, _blocking, packetDest.GetIPAddress(), packetDest.GetPort());
}

status_t UDPSocketDataIO :: SetBlockingIOEnabled(bool blocking)
{
   const status_t ret = SetSocketBlockingEnabled(_sock, blocking);
   if (ret.IsOK()) _blocking = blocking;
   return ret;
}

} // end namespace muscle
