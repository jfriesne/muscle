#include "dataio/SimulatedMulticastDataIO.h"

namespace muscle {

/**
 *
 * Method for simulating multicast via directed unicast, for WiFi
 *
 * 1. Once at startup, and then periodically (but not too often), each member sends out a very small multicast ping
 * 2. All members that receive this ping add its source address to their list of members-of-the-multicast-group
 * 3. When sending "multicast" data, each member actually sends it separately as unicast to each member in the list
 * 4. Any member that has not been heard from in a long time will eventually be timed out and dropped from the list
 * 5. A member that is shutting down gracefully may send a "bye" packet, to let the others know he is going away.
 */

// Given the "actual" multicast address that we're going to simulate, return an associated address that we can use for 
// management-traffic without having that traffic show up on a multicast group that the user knows about.
static IPAddressAndPort GetManagementMulticastAddressFor(const IPAddressAndPort & userMulticastAddress)
{
   IPAddress ip = userMulticastAddress.GetIPAddress();
   ip.SetLowBits(ip.GetLowBits()^0x6666);

   uint16 newPort = userMulticastAddress.GetPort()+4444;
   if (newPort < 1024) newPort += 1024;  // let's try to stay out of the privileged range at least
   return IPAddressAndPort(ip, newPort);
}

SimulatedMulticastDataIO :: SimulatedMulticastDataIO(const IPAddressAndPort & userMulticastAddress) : _userMulticastAddress(userMulticastAddress), _maxPacketSize(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET)
{
   if (StartInternalThread() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO:  Unable to start internal thread for [%s]\n", userMulticastAddress.ToString()());
}

enum {
   SMDIO_COMMAND_DATA = 1936548964,  // 'smdd' 
   SMDIO_COMMAND_PING,
   SMDIO_COMMAND_PONG,
   SMDIO_COMMAND_BYE,
};

static const String SMDIO_NAME_DATA   = "dat";
static const String SMDIO_NAME_SOURCE = "src";

int32 SimulatedMulticastDataIO :: Read(void * buffer, uint32 size)
{
   if (IsInternalThreadRunning() == false) return -1;

   MessageRef msg;
   if (GetNextReplyFromInternalThread(msg) < 0) return 0;  // nothing available to read, right now!

   ConstByteBufferRef incomingData = msg()->GetFlat(SMDIO_NAME_DATA);
   if (incomingData() == NULL) return 0;  // nothing for now!

   if (msg()->FindFlat(SMDIO_NAME_SOURCE, _lastPacketReceivedFrom) != B_NO_ERROR) _lastPacketReceivedFrom.Reset();

   switch(msg()->what)
   {
      case SMDIO_COMMAND_DATA:
      {
         const uint32 bytesToReturn = muscleMin(incomingData()->GetNumBytes(), size);
         memcpy(buffer, incomingData()->GetBuffer(), bytesToReturn);
         return bytesToReturn; 
      }
      break;

      default:
         LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO::Read():  Unexpected whatCode " UINT32_FORMAT_SPEC "\n", msg()->what);
      break;
   }
   return 0;
}

int32 SimulatedMulticastDataIO :: Write(const void * buffer, uint32 size)
{
   if (IsInternalThreadRunning() == false) return -1;

   MessageRef toInternalThreadMsg = GetMessageFromPool(SMDIO_COMMAND_DATA);
   return ((toInternalThreadMsg())&&(toInternalThreadMsg()->AddData(SMDIO_NAME_DATA, B_RAW_TYPE, buffer, size) == B_NO_ERROR)&&(SendMessageToInternalThread(toInternalThreadMsg) == B_NO_ERROR)) ? size : -1;
}

static UDPSocketDataIORef CreateMulticastUDPDataIO(const IPAddressAndPort & iap)
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL) return UDPSocketDataIORef();

   // This must be done before adding the socket to any multicast groups, otherwise Windows gets uncooperative
   if (BindUDPSocket(udpSock, iap.GetPort(), NULL, invalidIP, true) != B_NO_ERROR)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to bind multicast socket to UDP port %u!\n", iap.GetPort());
      return UDPSocketDataIORef();
   }

   if (AddSocketToMulticastGroup(udpSock, iap.GetIPAddress()) != B_NO_ERROR)
   {
      LogTime(MUSCLE_LOG_ERROR, "CreateMulticastUDPDataIO:  Unable to add UDP socket to multicast address [%s]\n", Inet_NtoA(iap.GetIPAddress())());
      return UDPSocketDataIORef();
   }

   UDPSocketDataIORef ret(newnothrow UDPSocketDataIO(udpSock, false));
   if (ret() == NULL) {WARN_OUT_OF_MEMORY; return UDPSocketDataIORef();}
   ret()->SetSendDestination(iap);
   return ret;
}

static UDPSocketDataIORef CreateUnicastUDPDataIO(uint16 & retPort)
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL) return UDPSocketDataIORef();

   if (BindUDPSocket(udpSock, 0, &retPort) != B_NO_ERROR)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to bind unicast socket\n");
      return UDPSocketDataIORef();
   }

   UDPSocketDataIORef ret(newnothrow UDPSocketDataIO(udpSock, false));
   if (ret() == NULL) {WARN_OUT_OF_MEMORY; return UDPSocketDataIORef();}
   return ret;
}

status_t SimulatedMulticastDataIO :: ReadPacket(DataIO & dio, ByteBufferRef & retBuf)
{
   if (_scratchBuf() == NULL) _scratchBuf = GetByteBufferFromPool(_maxPacketSize);
   if ((_scratchBuf() == NULL)||(_scratchBuf()->SetNumBytes(_maxPacketSize, false) != B_NO_ERROR)) return B_ERROR;

   const int32 bytesRead = dio.Read(_scratchBuf()->GetBuffer(), _scratchBuf()->GetNumBytes());
   if (bytesRead > 0)
   {
      (void) _scratchBuf()->SetNumBytes(bytesRead, true);
      retBuf = _scratchBuf;
      _scratchBuf.Reset();
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t SimulatedMulticastDataIO :: SendIncomingDataPacketToMainThread(const ByteBufferRef & data, const IPAddressAndPort & source)
{
   MessageRef toMainThreadMsg = GetMessageFromPool(SMDIO_COMMAND_DATA);
   return ((toMainThreadMsg())&&(toMainThreadMsg()->AddFlat(SMDIO_NAME_DATA, data) == B_NO_ERROR)&&(toMainThreadMsg()->AddFlat(SMDIO_NAME_SOURCE, source) == B_NO_ERROR)) ? SendMessageToOwner(toMainThreadMsg) : B_ERROR;
}

void SimulatedMulticastDataIO :: NoteHeardFromMember(const IPAddressAndPort & heardFrom, uint64 timeStampMicros, uint16 optUserUnicastPort)
{
   KnownMemberInfo * kmi = _knownMembers.Get(heardFrom);
   if (kmi == NULL)
   {
      kmi = _knownMembers.PutAndGet(heardFrom);
      if (kmi) LogTime(MUSCLE_LOG_DEBUG, "New member [%s] added to the simulated-multicast group, userUnicastPort=%u, now there are " UINT32_FORMAT_SPEC " members.\n", heardFrom.ToString()(), optUserUnicastPort, _knownMembers.GetNumItems());
   }
   if (kmi) 
   {
      kmi->_lastHeardFromTime = timeStampMicros;
      if (optUserUnicastPort) kmi->_userUnicastPort = optUserUnicastPort;
   }
}

void SimulatedMulticastDataIO :: UpdateIsInternalSocketRegisteredForWrite(uint32 purpose, uint32 type, bool wantsWriteReadyNotification)
{
   const ConstSocketRef & udpSock = _udpDataIOs[purpose][type]()->GetWriteSelectSocket();
   bool & isRegistered = _isInternalSocketRegisteredForWrite[purpose][type];

   if (wantsWriteReadyNotification != isRegistered)
   {
      if (wantsWriteReadyNotification)
      {
         if (RegisterInternalThreadSocket(udpSock, SOCKET_SET_WRITE) == B_NO_ERROR) isRegistered = true;
      }
      else
      {
         (void) UnregisterInternalThreadSocket(udpSock, SOCKET_SET_WRITE);
         isRegistered = false;
      }
   }
}

void SimulatedMulticastDataIO :: SendPingOrPong(uint32 whatCode, const IPAddressAndPort & destIAP)
{
   // When we get a ping, we always send back a pong so he can know our IPAddressAndPort also
   UDPSocketDataIO & pingUnicastIO = *_udpDataIOs[SMDIO_SOCKET_PURPOSE_PING][SMDIO_SOCKET_TYPE_UNICAST]();
   
   // Calling SendDataUDP() directly so that I don't have to call pingUnicastIO.SetSendDestination()
   uint8 pingBuf[sizeof(uint32)+sizeof(uint16)];
   muscleCopyOut(pingBuf,                B_HOST_TO_LENDIAN_INT32(whatCode));
   muscleCopyOut(pingBuf+sizeof(uint32), B_HOST_TO_LENDIAN_INT16(_localUnicastPorts[SMDIO_SOCKET_PURPOSE_USER]));
   if (SendDataUDP(pingUnicastIO.GetWriteSelectSocket(), pingBuf, sizeof(pingBuf), false, destIAP.GetIPAddress(), destIAP.GetPort()) != sizeof(pingBuf)) LogTime(MUSCLE_LOG_ERROR, "Unable to send ping/pong to %s\n", destIAP.ToString()());
}

static const uint64 _multicastPingIntervalMicros       = SecondsToMicros(10);
static const uint64 _multicastTimeoutPingIntervalCount = 5;    // if we haven't heard from someone in (this many) multicast-ping intervals, then they're off our list

// userMulticastIO -> third-party-originated multicast data packets can be received on this
// userUnicastIO   -> our own unicast-data packets are received on this
// pingMulticastIO -> used for initial discovery
// pingUnicastIO   -> our unicast pings are sent on this
void SimulatedMulticastDataIO :: InternalThreadEntry()
{
   const IPAddressAndPort multicastAddresses[NUM_SMDIO_SOCKET_PURPOSES] = {_userMulticastAddress, GetManagementMulticastAddressFor(_userMulticastAddress)};
   for (uint32 i=0; i<NUM_SMDIO_SOCKET_PURPOSES; i++)
   {
      for (uint32 j=0; j<NUM_SMDIO_SOCKET_TYPES; j++)
      {
         _isInternalSocketRegisteredForWrite[i][j] = false;

         UDPSocketDataIORef & udpDataIO = _udpDataIOs[i][j];
         switch(j)
         {
            case SMDIO_SOCKET_TYPE_MULTICAST: udpDataIO = CreateMulticastUDPDataIO(multicastAddresses[i]); break;
            case SMDIO_SOCKET_TYPE_UNICAST:   udpDataIO = CreateUnicastUDPDataIO(_localUnicastPorts[i]);   break;
         }
         if ((udpDataIO() == NULL)||(RegisterInternalThreadSocket(udpDataIO()->GetReadSelectSocket(), SOCKET_SET_READ) != B_NO_ERROR))
         {
            LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO: Unable to create udpSock " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", i, j);
            Thread::InternalThreadEntry();  // just wait for death, then
            return;
         }
      }
   }

   Queue<ConstByteBufferRef> outgoingUserPacketsQueue;
   Hashtable<IPAddressAndPort, ConstByteBufferRef> outgoingUserPacketsTable;  // list of destinations to send the current outgoing-packet
   Hashtable<IPAddress, Void> userUnicastDests;
   uint64 nextMulticastPingTime = GetRunTime64();
   while(true)
   {
      // Yes, SMDIO_SOCKET_PURPOSE_PING is correct!  I'm sending all packets from that socket so that all packets have the same source IAP
      UpdateIsInternalSocketRegisteredForWrite(SMDIO_SOCKET_PURPOSE_PING, SMDIO_SOCKET_TYPE_UNICAST, (outgoingUserPacketsTable.HasItems())||(outgoingUserPacketsQueue.HasItems()));

      // Block until it is time to do something
      MessageRef msgRef;
      int32 numLeft = WaitForNextMessageFromOwner(msgRef, nextMulticastPingTime);
      if (numLeft >= 0)
      {
         if (msgRef())
         {
            switch(msgRef()->what)
            {
               case SMDIO_COMMAND_DATA:
               {
                  ConstByteBufferRef data = msgRef()->GetFlat(SMDIO_NAME_DATA);
                  if (data()) (void) outgoingUserPacketsQueue.AddTail(data);
                         else LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO:  No data in SMDIO_COMMAND_DATA Message!\n");
               }
               break;

               default:
                  LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO:  Got unexpected whatCode " UINT32_FORMAT_SPEC " from main thread.\n", msgRef()->what);
               break;
            }
         }
         else break;  // NULL MessageRef means the main thread wants us to exit
      }

      const uint64 now = GetRunTime64();

      for (uint32 i=0; i<NUM_SMDIO_SOCKET_TYPES; i++)
      {
         // Incoming User-data packets are received on the user-sockets
         UDPSocketDataIO & userIO = *_udpDataIOs[SMDIO_SOCKET_PURPOSE_USER][i]();
         if (IsInternalThreadSocketReady(userIO.GetReadSelectSocket(), SOCKET_SET_READ))
         {
            ByteBufferRef packetData;
            while(ReadPacket(userIO, packetData) == B_NO_ERROR) 
            {
               const IPAddressAndPort & fromIAP = userIO.GetSourceOfLastReadPacket();
               NoteHeardFromMember(fromIAP, now, 0);
               (void) SendIncomingDataPacketToMainThread(packetData, fromIAP);
            }
         }

         // Incoming ping-packets carry only a 32-bit what-code, to minimize their size.
         UDPSocketDataIO & pingIO = *_udpDataIOs[SMDIO_SOCKET_PURPOSE_PING][i]();
         if (IsInternalThreadSocketReady(pingIO.GetReadSelectSocket(), SOCKET_SET_READ))
         {
            uint8 pingBuf[sizeof(uint32)+sizeof(uint16)];
            while(pingIO.Read(pingBuf, sizeof(pingBuf)) == sizeof(pingBuf))
            {
               const IPAddressAndPort & fromIAP = pingIO.GetSourceOfLastReadPacket();
               const uint32 whatCode            = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(pingBuf));
               const uint32 userUnicastPort     = B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(pingBuf+sizeof(uint32)));
               switch(whatCode)
               {
                  case SMDIO_COMMAND_PING:
                     SendPingOrPong(SMDIO_COMMAND_PONG, fromIAP);
                  // deliberate fall-through!
                  case SMDIO_COMMAND_PONG:
                     NoteHeardFromMember(fromIAP, now, userUnicastPort);
                  break;

                  case SMDIO_COMMAND_BYE:
                     if (_knownMembers.ContainsKey(fromIAP))
                     {
                        LogTime(MUSCLE_LOG_DEBUG, "Simulated-multicast  member [%s] has left the group (" UINT32_FORMAT_SPEC " members remain)\n", fromIAP.ToString()(), _knownMembers.GetNumItems()-1);
                        (void) _knownMembers.Remove(fromIAP);  // do this last!
                     }
                  break;

                  default:
                     LogTime(MUSCLE_LOG_WARNING, "SimulatedMulticastDataIO:  Got unexpected what-code " UINT32_FORMAT_SPEC " from %s\n", whatCode, fromIAP.ToString()());
                  break;
               }
            }
         }
      }

      // If we have outgoing user-data to send, and the user-socket is ready-for-write, then send some unicast-packets
      {
         UDPSocketDataIO & udpSock = *_udpDataIOs[SMDIO_SOCKET_PURPOSE_PING][SMDIO_SOCKET_TYPE_UNICAST]();  // sending from the ping socket so that packets always have the same source IAP
         if (IsInternalThreadSocketReady(udpSock.GetWriteSelectSocket(), SOCKET_SET_WRITE))
         {
            if ((outgoingUserPacketsTable.IsEmpty())&&(outgoingUserPacketsQueue.HasItems()))
            {
               // Now populate the outgoingUserPacketsTable with the next outgoing user packet for each destination
               ConstByteBufferRef nextPacket;
               if (outgoingUserPacketsQueue.RemoveHead(nextPacket) == B_NO_ERROR)
               {
                  (void) outgoingUserPacketsTable.EnsureSize(_knownMembers.GetNumItems());
                  for (HashtableIterator<IPAddressAndPort, KnownMemberInfo> iter(_knownMembers); iter.HasData(); iter++) (void) outgoingUserPacketsTable.Put(IPAddressAndPort(iter.GetKey().GetIPAddress(), iter.GetValue()._userUnicastPort), nextPacket);
               }
            }

            while(outgoingUserPacketsTable.HasItems())
            {
               const IPAddressAndPort & destIAP = *outgoingUserPacketsTable.GetFirstKey();
               const ConstByteBufferRef & buf   = *outgoingUserPacketsTable.GetFirstValue();

               const int32 numBytesToSend = buf()->GetNumBytes();  // copy this out first!
               const int32 numBytesSent   = SendDataUDP(udpSock.GetWriteSelectSocket(), buf()->GetBuffer(), numBytesToSend, false, destIAP.GetIPAddress(), destIAP.GetPort());
               if (numBytesSent != 0) outgoingUserPacketsTable.RemoveFirst();  // only time to keep it is in the temporarily-out-of-buffer-space case, not on success and not on real-error
               if (numBytesSent != numBytesToSend) break;
            }
         }
      }

      if (now >= nextMulticastPingTime)
      {
         // We'll send a ping to multicast-land, in case there is someone out there we don't know about
         SendPingOrPong(SMDIO_COMMAND_PING, multicastAddresses[SMDIO_SOCKET_PURPOSE_PING]);
         nextMulticastPingTime = now + _multicastPingIntervalMicros;

         // And we'll also send unicast pings to anyone who we haven't heard from in a while, just to double-check
         const uint64 timeoutPeriodMicros = (_multicastTimeoutPingIntervalCount*_multicastPingIntervalMicros);
         const uint64 halfTimeoutPeriodMicros = timeoutPeriodMicros/2;

         // If we haven't heard from someone for a very long time, we'll drop them
         for (HashtableIterator<IPAddressAndPort, KnownMemberInfo> iter(_knownMembers); iter.HasData(); iter++)
         {
            const IPAddressAndPort & destIAP = iter.GetKey();
            const uint64 timeSinceMicros = (now-iter.GetValue()._lastHeardFromTime);
            if (timeSinceMicros >= timeoutPeriodMicros)
            {
               LogTime(MUSCLE_LOG_DEBUG, "Dropping moribund SimulatedMulticast member at [%s], " UINT32_FORMAT_SPEC " members remain\n", destIAP.ToString()(), _knownMembers.GetNumItems()-1);
               (void) _knownMembers.Remove(destIAP);
            }
            else if (timeSinceMicros >= halfTimeoutPeriodMicros) SendPingOrPong(SMDIO_COMMAND_PING, destIAP);
         }
      }
   }

   // Finally, send out a BYE so that other members can delete us from their list if they get it, rather than having to wait to time out
   SendPingOrPong(SMDIO_COMMAND_BYE, multicastAddresses[SMDIO_SOCKET_PURPOSE_PING]);
}

void SimulatedMulticastDataIO :: ShutdownAux()
{
   ShutdownInternalThread();
}

}; // end namespace muscle
