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
   SetEnobufsErrorMode(false);  // initialize _enobufsCount and _nextErrorModeSendTime to their default settings

   status_t ret;
   if (StartInternalThread().IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  Unable to start internal thread for group [%s] [%s]\n", this, multicastAddress.ToString()(), ret());
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

   if (msg()->FindFlat(SMDIO_NAME_RLOC, retPacketSource).IsError()) retPacketSource.Reset();
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
         LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  ReadFrom():  Unexpected whatCode " UINT32_FORMAT_SPEC "\n", this, msg()->what);
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
           (toInternalThreadMsg()->AddData(SMDIO_NAME_DATA, B_RAW_TYPE, buffer, size).IsOK())&&
           ((packetDest.IsValid() == false)||(toInternalThreadMsg()->AddFlat(SMDIO_NAME_RLOC, packetDest).IsOK()))&&
           (SendMessageToInternalThread(toInternalThreadMsg).IsOK())) ? size : -1;
}

UDPSocketDataIORef SimulatedMulticastDataIO :: CreateMulticastUDPDataIO(const IPAddressAndPort & iap) const
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL) return UDPSocketDataIORef();

   // This must be done before adding the socket to any multicast groups, otherwise Windows gets uncooperative
   status_t errRet;
   if (BindUDPSocket(udpSock, iap.GetPort(), NULL, invalidIP, true).IsError(errRet))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "SimulatedMulticastDataIO %p:  Unable to bind multicast socket to UDP port %u! [%s]\n", this, iap.GetPort(), errRet());
      return UDPSocketDataIORef();
   }

   // Send a test packet just to verify that packets can be sent on this socket (otherwise there's little use in continuing)
   const uint8 dummyBuf = 0;  // doesn't matter what this is, I just want to make sure I can actually send on this socket
   if (SendDataUDP(udpSock, &dummyBuf, 0, true, iap.GetIPAddress(), iap.GetPort()) != 0)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "SimulatedMulticastDataIO %p:  Unable to send test UDP packet to multicast destination [%s]\n", this, iap.ToString()());
      return UDPSocketDataIORef();
   }

   if (AddSocketToMulticastGroup(udpSock, iap.GetIPAddress()).IsError(errRet))
   {
      LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  Unable to add UDP socket to multicast address [%s] [%s]\n", this, Inet_NtoA(iap.GetIPAddress())(), errRet());
      return UDPSocketDataIORef();
   }

   UDPSocketDataIORef ret(newnothrow UDPSocketDataIO(udpSock, false));
   if (ret() == NULL) {MWARN_OUT_OF_MEMORY; return UDPSocketDataIORef();}
   (void) ret()->SetPacketSendDestination(iap);
   return ret;
}

static UDPSocketDataIORef CreateUnicastUDPDataIO(uint16 & retPort)
{
   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL) return UDPSocketDataIORef();

   if (BindUDPSocket(udpSock, 0, &retPort).IsError()) return UDPSocketDataIORef();

   UDPSocketDataIORef ret(newnothrow UDPSocketDataIO(udpSock, false));
   if (ret() == NULL) {MWARN_OUT_OF_MEMORY; return UDPSocketDataIORef();}
   return ret;
}

status_t SimulatedMulticastDataIO :: ReadPacket(DataIO & dio, ByteBufferRef & retBuf)
{
   if (_scratchBuf() == NULL) _scratchBuf = GetByteBufferFromPool(_maxPacketSize);
   if ((_scratchBuf() == NULL)||(_scratchBuf()->SetNumBytes(_maxPacketSize, false).IsError())) return B_OUT_OF_MEMORY;

   const int32 bytesRead = dio.Read(_scratchBuf()->GetBuffer(), _scratchBuf()->GetNumBytes());
   if (bytesRead > 0)
   {
      (void) _scratchBuf()->SetNumBytes(bytesRead, true);
      retBuf = _scratchBuf;
      _scratchBuf.Reset();
      return B_NO_ERROR;
   }
   else return B_IO_ERROR;
}

status_t SimulatedMulticastDataIO :: SendIncomingDataPacketToMainThread(const ByteBufferRef & data, const IPAddressAndPort & source)
{
   MessageRef toMainThreadMsg = GetMessageFromPool(SMDIO_COMMAND_DATA);
   if (toMainThreadMsg() == NULL) return B_OUT_OF_MEMORY;

   status_t ret;
   return ((toMainThreadMsg()->AddFlat(SMDIO_NAME_DATA, data).IsOK(ret))
         &&(toMainThreadMsg()->AddFlat(SMDIO_NAME_RLOC, source).IsOK(ret))) ? SendMessageToOwner(toMainThreadMsg) : ret;
}

void SimulatedMulticastDataIO :: NoteHeardFromMember(const IPAddressAndPort & heardFromPingSource, uint64 timeStampMicros)
{
   uint64 * lastHeardFromTime = _knownMembers.Get(heardFromPingSource);
        if (lastHeardFromTime) *lastHeardFromTime = muscleMax(*lastHeardFromTime, timeStampMicros);
   else if (_knownMembers.Put(heardFromPingSource, timeStampMicros).IsOK()) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "New member [%s] added to the simulated-multicast group [%s], now there are " UINT32_FORMAT_SPEC " members.\n", heardFromPingSource.ToString()(), _multicastAddress.ToString()(), _knownMembers.GetNumItems());
   }
}

static const uint32 ENOBUFS_COUNT_THRESHOLD  = 100;  // if SendDataUDP() errors out with ENOBUFS (this many) times in a row, then we'll assume the Wi-Fi is broken and nerf ourself out to avoid a spinloop
static const uint32 ENOBUFS_DURATION_SECONDS = 5;    // how long to disable attempts to write to the socket for, before trying again

void SimulatedMulticastDataIO :: SetEnobufsErrorMode(bool enable)
{
   _enobufsCount          = enable ? ENOBUFS_COUNT_THRESHOLD : 0;
   _nextErrorModeSendTime = enable ? (GetRunTime64()+SecondsToMicros(ENOBUFS_DURATION_SECONDS)) : MUSCLE_TIME_NEVER;
}

bool SimulatedMulticastDataIO :: IsInEnobufsErrorMode() const
{
   return (_enobufsCount >= ENOBUFS_COUNT_THRESHOLD);
}

void SimulatedMulticastDataIO :: UpdateUnicastSocketRegisteredForWrite(bool shouldBeRegisteredForWrite)
{
   if ((IsInEnobufsErrorMode())&&(shouldBeRegisteredForWrite))
   {
      if (GetRunTime64() < _nextErrorModeSendTime) shouldBeRegisteredForWrite = false; // don't bother trying to write, when in enobufs-error-mode
      else 
      {
         LogTime(MUSCLE_LOG_WARNING, "SimulatedMulticastDataIO %p:  Exiting fault-mode to see if the ENOBUFS fault has cleared yet.\n", this);
         SetEnobufsErrorMode(false);
      }
   }

   if (shouldBeRegisteredForWrite != _isUnicastSocketRegisteredForWrite)
   {
      const ConstSocketRef & udpSock = _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]()->GetWriteSelectSocket();
      if (shouldBeRegisteredForWrite)
      {
         if (RegisterInternalThreadSocket(udpSock, SOCKET_SET_WRITE).IsOK()) _isUnicastSocketRegisteredForWrite = true;
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
   enum {ipAddressAndPortFlattenedSize = (sizeof(uint64)+sizeof(uint64)+sizeof(uint32)+sizeof(uint16))};  // low-bits, high-bits, interface-index, port
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
   if (buf() == NULL) return B_OUT_OF_MEMORY;

   Queue<ConstByteBufferRef> * pq = _outgoingPacketsTable.GetOrPut(destIAP);
   return pq ? pq->AddTail(buf) : B_OUT_OF_MEMORY;
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
         if (SendDataUDP(udpSock, b()->GetBuffer(), b()->GetNumBytes(), false, dest.GetIPAddress(), dest.GetPort()) == 0)
         {
            // Work-around for occasional Apple bug where a disabled Wi-Fi interface will errneously show up and appear
            // to be usable and ready-for-write, but every call to SendDataUDP() on it results in immediate ENOBUFS,
            // causing the internal thread to go into a loop and spin the CPU.
            if ((PreviousOperationHadTransientFailure())&&(b()->GetNumBytes() > 0))
            {
               ++_enobufsCount;
               if (IsInEnobufsErrorMode())
               {
                  LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  ENOBUFS bug detected, disabling writes to socket for " UINT32_FORMAT_SPEC " seconds to avoid a spin-loop.\n", this, ENOBUFS_DURATION_SECONDS);
                  SetEnobufsErrorMode(true);  // this call is here to set _nextErrorModeSendTime
               }
            }
            return;
         }
         else SetEnobufsErrorMode(false);  // reset the _enobufsCounter to zero on any sign of success
 
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
   if (buf.GetNumBytes() < sizeof(uint64)+sizeof(uint32)) return B_BAD_DATA;

   const uint8 * b      = buf.GetBuffer();
   const uint64 magic   = B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(b)); b += sizeof(uint64);
   if (magic != SIMULATED_MULTICAST_CONTROL_MAGIC) return B_BAD_DATA;
   retWhatCode          = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(b)); b += sizeof(uint32);

   if (retWhatCode == SMDIO_COMMAND_PONG)
   {
      const uint32 numExtras = (uint32)(((buf.GetBuffer()+buf.GetNumBytes())-b)/IPAddressAndPort::FlattenedSize());
      for (uint32 i=0; i<numExtras; i++)
      {
         IPAddressAndPort next; 
         if (next.Unflatten(b, IPAddressAndPort::FlattenedSize()).IsOK())
         {
            const uint64 microsSinceHeardFrom = MillisToMicros(next.GetIPAddress().GetInterfaceIndex()); // yes, I'm abusing this field
            if (microsSinceHeardFrom < _timeoutPeriodMicros) 
            {
               next = next.WithInterfaceIndex(_localAddressAndPort.GetIPAddress().GetInterfaceIndex());  // since we don't actually use it anyway
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
      if (GetNetworkInterfaceInfos(niis).IsOK())
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
      if ((io() == NULL)||(RegisterInternalThreadSocket(io()->GetReadSelectSocket(), SOCKET_SET_READ).IsError()))
      {
         LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  Unable to set up %s UDP socket\n", this, GetUDPSocketTypeName(i));
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
      const int32 numLeft = WaitForNextMessageFromOwner(msgRef, nextMulticastPingTime);
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
                     if ((msgRef()->FindFlat(SMDIO_NAME_RLOC, destIAP).IsOK())&&(destIAP != _multicastAddress))
                     {
                        // Special case for WriteTo():  This packet can go out as a normal UDP packet
                        (void) _udpDataIOs[SMDIO_SOCKET_TYPE_UNICAST]()->WriteTo(data()->GetBuffer(), data()->GetNumBytes(), destIAP);
                     }
                     else if (IsInEnobufsErrorMode() == false) outgoingUserPacketsQueue.AddTail(data);  // Normal case:  the packet will go out via simulated-multicast
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  No data in SMDIO_COMMAND_DATA Message!\n", this);
               }
               break;

               default:
                  LogTime(MUSCLE_LOG_ERROR, "SimulatedMulticastDataIO %p:  Got unexpected whatCode " UINT32_FORMAT_SPEC " from main thread.\n", this, msgRef()->what);
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
            while(ReadPacket(udpIO, packetData).IsOK()) 
            {
               const IPAddressAndPort & fromIAP = udpIO.GetSourceOfLastReadPacket();
               NoteHeardFromMember(fromIAP, now);

               uint32 whatCode;
               if (ParseMulticastControlPacket(*packetData(), now, whatCode).IsOK())
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
                        LogTime(MUSCLE_LOG_WARNING, "SimulatedMulticastDataIO %p:  Got unexpected what-code " UINT32_FORMAT_SPEC " from %s\n", this, whatCode, fromIAP.ToString()());
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
               if (outgoingUserPacketsQueue.RemoveHead(nextPacket).IsOK())
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
   if (EnqueueOutgoingMulticastControlCommand(SMDIO_COMMAND_BYE, GetRunTime64(), _multicastAddress).IsOK()) DrainOutgoingPacketsTable();
}

void SimulatedMulticastDataIO :: ShutdownAux()
{
   ShutdownInternalThread();
}

} // end namespace muscle
