#include "dataio/SimulatedMulticastDataIO.h"
#include "util/NetworkInterfaceInfo.h"

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

static const uint64 SIMULATED_MULTICAST_CONTROL_MAGIC = ((uint64) 0x72F967C8345A065BLL);  // arbitrary magic header number

SimulatedMulticastDataIO :: SimulatedMulticastDataIO(const IPAddressAndPort & multicastAddress)
   : _multicastAddress(multicastAddress)
   , _maxPacketSize(MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET)
   , _isUnicastSocketRegisteredForWrite(false)
{
   if (StartInternalThread() != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO:  Unable to start internal thread for group [%s]\n", multicastAddress.ToString()());
}

enum {
   SMDIO_COMMAND_DATA = 1936548964,  // 'smdd' 
   SMDIO_COMMAND_PING,
   SMDIO_COMMAND_PONG,
   SMDIO_COMMAND_BYE,
};

static const String SMDIO_NAME_DATA = "dat";  // B_RAW_TYPE: packet's data buffer
static const String SMDIO_NAME_RLOC = "rlc";  // IPAddressAndPort: packet's remote location (source or dest)

int32 SimulatedMulticastDataIO :: ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource)
{
   if (IsInternalThreadRunning() == false) return -1;

   MessageRef msg;
   if (GetNextReplyFromInternalThread(msg) < 0) return 0;  // nothing available to read, right now!

   ConstByteBufferRef incomingData = msg()->GetFlat(SMDIO_NAME_DATA);
   if (incomingData() == NULL) return 0;  // nothing for now!

   if (msg()->FindFlat(SMDIO_NAME_RLOC, retPacketSource) != B_NO_ERROR) retPacketSource.Reset();
   SetSourceOfLastReadPacket(retPacketSource); // in case this was a direct ReadFrom() call and anyone calls GetSourceOfLastReadPacket() later

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
         LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO::ReadFrom():  Unexpected whatCode " UINT32_FORMAT_SPEC "\n", msg()->what);
      break;
   }
   return 0;
}

int32 SimulatedMulticastDataIO :: Write(const void * buffer, uint32 size)
{
   return WriteTo(buffer, size, GetDefaultObjectForType<IPAddressAndPort>());
}

int32 SimulatedMulticastDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
{
   if (IsInternalThreadRunning() == false) return -1;

   MessageRef toInternalThreadMsg = GetMessageFromPool(SMDIO_COMMAND_DATA);
   return ((toInternalThreadMsg())&&
           (toInternalThreadMsg()->AddData(SMDIO_NAME_DATA, B_RAW_TYPE, buffer, size) == B_NO_ERROR)&&
           ((packetDest.IsValid() == false)||(toInternalThreadMsg()->AddFlat(SMDIO_NAME_RLOC, packetDest) == B_NO_ERROR))&&
           (SendMessageToInternalThread(toInternalThreadMsg) == B_NO_ERROR)) ? size : -1;
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
   (void) ret()->SetPacketSendDestination(iap);
   return ret;
}

static UDPSocketDataIORef CreateUnicastUDPDataIO(uint16 & retPort)
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL) return UDPSocketDataIORef();

   if (BindUDPSocket(udpSock, 0, &retPort) != B_NO_ERROR) return UDPSocketDataIORef();

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
   return ((toMainThreadMsg())&&(toMainThreadMsg()->AddFlat(SMDIO_NAME_DATA, data) == B_NO_ERROR)&&(toMainThreadMsg()->AddFlat(SMDIO_NAME_RLOC, source) == B_NO_ERROR)) ? SendMessageToOwner(toMainThreadMsg) : B_ERROR;
}

void SimulatedMulticastDataIO :: NoteHeardFromMember(const IPAddressAndPort & heardFromPingSource, uint64 timeStampMicros)
{
   uint64 * lastHeardFromTime = _knownMembers.Get(heardFromPingSource);
        if (lastHeardFromTime) *lastHeardFromTime = muscleMax(*lastHeardFromTime, timeStampMicros);
   else if (_knownMembers.Put(heardFromPingSource, timeStampMicros) == B_NO_ERROR) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "New member [%s] added to the simulated-multicast group [%s], now there are " UINT32_FORMAT_SPEC " members.\n", heardFromPingSource.ToString()(), _multicastAddress.ToString()(), _knownMembers.GetNumItems());
   }
}

void SimulatedMulticastDataIO :: UpdateUnicastSocketRegisteredForWrite(bool shouldBeRegisteredForWrite)
{
   if (shouldBeRegisteredForWrite != _isUnicastSocketRegisteredForWrite)
   {
      const ConstSocketRef & udpSock = _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]()->GetWriteSelectSocket();
      if (shouldBeRegisteredForWrite)
      {
         if (RegisterInternalThreadSocket(udpSock, SOCKET_SET_WRITE) == B_NO_ERROR) _isUnicastSocketRegisteredForWrite = true;
      }
      else
      {
         (void) UnregisterInternalThreadSocket(udpSock, SOCKET_SET_WRITE);
         _isUnicastSocketRegisteredForWrite = false;
      }
   }
}

static const uint32 NUM_EXTRA_ADDRESSES = 10;  // adds up to 220 bytes to the PONGs, but I think that's an okay amount of overhead for unicast

status_t SimulatedMulticastDataIO :: EnqueueOutgoingMulticastControlCommand(uint32 whatCode, uint64 now, const IPAddressAndPort & destIAP)
{
#ifdef MUSCLE_CONSTEXPR_IS_SUPPORTED
   uint8 pingBuf[sizeof(uint64)+sizeof(uint32)+(NUM_EXTRA_ADDRESSES*IPAddressAndPort::FlattenedSize())]; // ok because FlattenedSize() is declared constexpr
#else
   // Ugly hack work-around, for e.g. MSVC2013 or pre-C++11 compilers that don't support constexpr
   enum {ipAddressAndPortFlattenedSize = (sizeof(uint64)+sizeof(uint64)+sizeof(32)+sizeof(uint16))};  // low-bits, high-bits, interface-index, port
   uint8 pingBuf[sizeof(uint64)+sizeof(uint32)+(NUM_EXTRA_ADDRESSES*ipAddressAndPortFlattenedSize)];
#endif

   uint8 * b = pingBuf;
   muscleCopyOut(b, B_HOST_TO_LENDIAN_INT64(SIMULATED_MULTICAST_CONTROL_MAGIC)); b += sizeof(uint64);
   muscleCopyOut(b, B_HOST_TO_LENDIAN_INT32(whatCode));                          b += sizeof(uint32);
   if ((whatCode == SMDIO_COMMAND_PONG)&&(destIAP != _localAddressAndPort))  // no point telling myself about what I know
   {
      // Include the next (n) member-IAPs (not including our own) to the PONG's data so that the 
      // receiver can add them all, even if he doesn't get all of the PONGs directly from everyone
      const bool tableContainsSelf = ((_localAddressAndPort.IsValid())&&(_knownMembers.ContainsKey(_localAddressAndPort)));
      HashtableIterator<IPAddressAndPort, uint64> iter;
      if (tableContainsSelf) iter = _knownMembers.GetIteratorAt(_localAddressAndPort);
                        else iter = _knownMembers.GetIterator();
      const uint32 maxToAdd = muscleMin(NUM_EXTRA_ADDRESSES, _knownMembers.GetNumItems());
      for (uint32 count = 0; count < maxToAdd; count++)
      {
         // Go to next entry, wrapping around if necessary;
         iter++; if (iter.HasData() == false) iter = _knownMembers.GetIterator();

         // We don't want to send out our own IPAddressAndPort, that will be seen via recvfrom()'s args 
         IPAddressAndPort nextIAP = iter.GetKey();
         if ((tableContainsSelf)&&(nextIAP == _localAddressAndPort)) break;

         // We'll assume it doesn't need to know about itself
         if (nextIAP != destIAP)
         {
            const uint64 millisSinceHeardFrom = muscleMin((uint64) MicrosToMillis(now-iter.GetValue()), (uint64) (MUSCLE_NO_LIMIT-1));  // paranoia: cap at 2^32-1
            nextIAP = nextIAP.WithInterfaceIndex((uint32) millisSinceHeardFrom);  // yes, I'm abusing this (otherwise-unused-in-this-context) field
            nextIAP.Flatten(b); b += IPAddressAndPort::FlattenedSize();
         }
      }
   }

   ConstByteBufferRef buf = GetByteBufferFromPool((uint32)(b-pingBuf), pingBuf);
   if (buf() == NULL) return B_ERROR;

   Queue<ConstByteBufferRef> * pq = _outgoingPacketsTable.GetOrPut(destIAP);
   return pq ? pq->AddTail(buf) : B_ERROR;
}

void SimulatedMulticastDataIO :: DrainOutgoingPacketsTable()
{
   const ConstSocketRef & udpSock = _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]()->GetWriteSelectSocket();
   while(_outgoingPacketsTable.HasItems())
   {
      Queue<ConstByteBufferRef> & pq = *_outgoingPacketsTable.GetFirstValue();
      if (pq.HasItems())
      { 
         const IPAddressAndPort & dest = *_outgoingPacketsTable.GetFirstKey();
         const ConstByteBufferRef & b  = pq.Head();
         if (SendDataUDP(udpSock, b()->GetBuffer(), b()->GetNumBytes(), false, dest.GetIPAddress(), dest.GetPort()) == 0) return;
         (void) pq.RemoveHead();
      }
      if (pq.IsEmpty()) (void) _outgoingPacketsTable.RemoveFirst();
   }
}

static const uint64 _multicastPingIntervalMicros       = SecondsToMicros(10);
static const uint64 _multicastTimeoutPingIntervalCount = 5;    // if we haven't heard from someone in (this many) multicast-ping intervals, then they're off our list
static const uint64 _timeoutPeriodMicros               = (_multicastTimeoutPingIntervalCount*_multicastPingIntervalMicros);
static const uint64 _halfTimeoutPeriodMicros           = _timeoutPeriodMicros/2;

status_t SimulatedMulticastDataIO :: ParseMulticastControlPacket(const ByteBuffer & buf, uint64 now, uint32 & retWhatCode)
{
   if (buf.GetNumBytes() < sizeof(uint64)+sizeof(uint32)) return B_ERROR;

   const uint8 * b      = buf.GetBuffer();
   const uint64 magic   = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(b)); b += sizeof(uint64);
   if (magic != SIMULATED_MULTICAST_CONTROL_MAGIC) return B_ERROR;
   retWhatCode          = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(b)); b += sizeof(uint32);

   if (retWhatCode == SMDIO_COMMAND_PONG)
   {
      const uint32 numExtras = (const uint32)(((buf.GetBuffer()+buf.GetNumBytes())-b)/IPAddressAndPort::FlattenedSize());
      for (uint32 i=0; i<numExtras; i++)
      {
         IPAddressAndPort next; 
         if (next.Unflatten(b, IPAddressAndPort::FlattenedSize()) == B_NO_ERROR)
         {
            const uint64 microsSinceHeardFrom = MillisToMicros(next.GetIPAddress().GetInterfaceIndex()); // yes, I'm abusing this field
            if (microsSinceHeardFrom < _timeoutPeriodMicros) 
            {
               next = next.WithInterfaceIndex(_localAddressAndPort.GetIPAddress().GetInterfaceIndex());  // since we don't actually it anyway
               NoteHeardFromMember(next, (now>microsSinceHeardFrom)?(now-microsSinceHeardFrom):0);       // semi-paranoia
            }
            b += IPAddressAndPort::FlattenedSize();
         }
         else break;  // wtf?
      }
   }

   return B_NO_ERROR; 
}

const char * SimulatedMulticastDataIO :: GetUDPSocketTypeName(uint32 which) const
{
   switch(which)
   {
      case SMDIO_SOCKET_TYPE_MULTICAST: return "Multicast";
      case SMDIO_SOCKET_TYPE_UNICAST:   return "Unicast";
      default:                          return "???";
   }
}

void SimulatedMulticastDataIO :: InternalThreadEntry()
{
   _udpDataIOs[SMDIO_SOCKET_TYPE_MULTICAST] = CreateMulticastUDPDataIO(_multicastAddress);
   if (_udpDataIOs[SMDIO_SOCKET_TYPE_MULTICAST]() == NULL) LogTime(MUSCLE_LOG_ERROR, "Unable to create multicast socket for [%s]\n", _multicastAddress.ToString()());

   uint16 localUnicastPort = 0;
   _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST] = CreateUnicastUDPDataIO(localUnicastPort);
   if (_udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]() == NULL) LogTime(MUSCLE_LOG_ERROR, "Unable to create unicast socket!\n");

   // Figure out what our local IPAddressAndPort will be
   {
      Queue<NetworkInterfaceInfo> niis;
      if (GetNetworkInterfaceInfos(niis) == B_NO_ERROR)
      {
         for (uint32 i=0; i<niis.GetNumItems(); i++)
         {
            const NetworkInterfaceInfo & nii = niis[i];
            const IPAddress & ipAddr = nii.GetLocalAddress();
            if ((ipAddr.IsIPv4() == false)&&(ipAddr.GetInterfaceIndex() == _multicastAddress.GetIPAddress().GetInterfaceIndex()))
            {
               _localAddressAndPort = IPAddressAndPort(ipAddr, localUnicastPort);
               break;
            }
         }
      }
      if (_localAddressAndPort.IsValid()) LogTime(MUSCLE_LOG_DEBUG, "SimulatedMulticastDataIO %p:  For multicastAddress [%s], localAddressAndPort is [%s]\n", this, _multicastAddress.ToString()(), _localAddressAndPort.ToString()());
                                     else LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  Unable to find localAddressAndPort for multicastAddress [%s]!\n", this, _multicastAddress.ToString()());
   }

   for (uint32 i=0; i<NUM_SMDIO_SOCKET_TYPES; i++)
   {
      UDPSocketDataIORef & io = _udpDataIOs[i];
      if ((io() == NULL)||(RegisterInternalThreadSocket(io()->GetReadSelectSocket(), SOCKET_SET_READ) != B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO:  Unable to set up %s UDP socket\n", GetUDPSocketTypeName(i));
         Thread::InternalThreadEntry();  // just wait for death, then
         return;
      }
   }

   Queue<ConstByteBufferRef> outgoingUserPacketsQueue;
   Hashtable<IPAddress, Void> userUnicastDests;
   uint64 nextMulticastPingTime = 0;  // ASAP please!
   while(true)
   {
      UpdateUnicastSocketRegisteredForWrite((_outgoingPacketsTable.HasItems())||(outgoingUserPacketsQueue.HasItems()));

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
                  if (data()) 
                  {
                     IPAddressAndPort destIAP;
                     if ((msgRef()->FindFlat(SMDIO_NAME_RLOC, destIAP) == B_NO_ERROR)&&(destIAP != _multicastAddress))
                     {
                        // Special case for WriteTo():  This packet can go out as a normal UDP packet
                        (void) _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]()->WriteTo(data()->GetBuffer(), data()->GetNumBytes(), destIAP);
                     }
                     else (void) outgoingUserPacketsQueue.AddTail(data);  // Normal case:  the packet will go out via simulated-multicast
                  }
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
         UDPSocketDataIO & udpIO = *_udpDataIOs[i]();
         if (IsInternalThreadSocketReady(udpIO.GetReadSelectSocket(), SOCKET_SET_READ))
         {
            ByteBufferRef packetData;
            while(ReadPacket(udpIO, packetData) == B_NO_ERROR) 
            {
               const IPAddressAndPort & fromIAP = udpIO.GetSourceOfLastReadPacket();
               NoteHeardFromMember(fromIAP, now);

               uint32 whatCode;
               if (ParseMulticastControlPacket(*packetData(), now, whatCode) == B_NO_ERROR)
               {
                  switch(whatCode)
                  {
                     case SMDIO_COMMAND_PING:
                        (void) EnqueueOutgoingMulticastControlCommand(SMDIO_COMMAND_PONG, now, fromIAP);
                     // deliberate fall-through!
                     case SMDIO_COMMAND_PONG:
                        // empty -- NoteHeardFromMember() was already called, above
                     break;

                     case SMDIO_COMMAND_BYE:
                        if (_knownMembers.ContainsKey(fromIAP))
                        {
                           LogTime(MUSCLE_LOG_DEBUG, "Simulated-multicast member [%s] has left group [%s] (" UINT32_FORMAT_SPEC " members remain)\n", fromIAP.ToString()(), _multicastAddress.ToString()(), _knownMembers.GetNumItems()-1);
                           (void) _knownMembers.Remove(fromIAP);  // do this last!
                        }
                     break;

                     default:
                        LogTime(MUSCLE_LOG_WARNING, "SimulatedMulticastDataIO:  Got unexpected what-code " UINT32_FORMAT_SPEC " from %s\n", whatCode, fromIAP.ToString()());
                     break;
                  }
               }
               else (void) SendIncomingDataPacketToMainThread(packetData, fromIAP);
            }
         }
      }

      // If we have outgoing user-data to send, and the unicast-socket is ready-for-write, then enqueue some unicast-packets
      {
         UDPSocketDataIO & udpSock = *_udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]();
         if (IsInternalThreadSocketReady(udpSock.GetWriteSelectSocket(), SOCKET_SET_WRITE))
         {
            if ((_outgoingPacketsTable.IsEmpty())&&(outgoingUserPacketsQueue.HasItems()))
            {
               // Now populate the _outgoingPacketsTable with the next outgoing user packet for each destination
               ConstByteBufferRef nextPacket;
               if (outgoingUserPacketsQueue.RemoveHead(nextPacket) == B_NO_ERROR)
               {
                  Queue<ConstByteBufferRef> pq; (void) pq.AddTail(nextPacket);

                  (void) _outgoingPacketsTable.EnsureSize(_knownMembers.GetNumItems());
                  for (HashtableIterator<IPAddressAndPort, uint64> iter(_knownMembers); iter.HasData(); iter++) (void) _outgoingPacketsTable.Put(iter.GetKey(), pq);
               }
            }

            DrainOutgoingPacketsTable();
         }
      }

      if (now >= nextMulticastPingTime)
      {
         // We'll send a ping to multicast-land, in case there is someone out there we don't know about
         (void) EnqueueOutgoingMulticastControlCommand(SMDIO_COMMAND_PING, now, _multicastAddress);
         nextMulticastPingTime = now + _multicastPingIntervalMicros;

         // And we'll also send unicast pings to anyone who we haven't heard from in a while, just to double-check
         // If we haven't heard from someone for a very long time, we'll drop them
         for (HashtableIterator<IPAddressAndPort, uint64> iter(_knownMembers); iter.HasData(); iter++)
         {
            const IPAddressAndPort & destIAP = iter.GetKey();
            const uint64 timeSinceMicros = (now-iter.GetValue());
            if (timeSinceMicros >= _timeoutPeriodMicros)
            {
               LogTime(MUSCLE_LOG_DEBUG, "Dropping moribund SimulatedMulticast member at [%s], " UINT32_FORMAT_SPEC " members remain in group [%s]\n", destIAP.ToString()(), _knownMembers.GetNumItems()-1, _multicastAddress.ToString()());
               (void) _knownMembers.Remove(destIAP);
            }
            else if (timeSinceMicros >= _halfTimeoutPeriodMicros) (void) EnqueueOutgoingMulticastControlCommand(SMDIO_COMMAND_PING, now, destIAP);
         }
      }
   }

   // Finally, send out a BYE so that other members can delete us from their list if they get it, rather than having to wait to time out
   _outgoingPacketsTable.Clear();  // no point waiting around to send user data now
   if (EnqueueOutgoingMulticastControlCommand(SMDIO_COMMAND_BYE, GetRunTime64(), _multicastAddress) == B_NO_ERROR) DrainOutgoingPacketsTable();
}

void SimulatedMulticastDataIO :: ShutdownAux()
{
   ShutdownInternalThread();
}

} // end namespace muscle
