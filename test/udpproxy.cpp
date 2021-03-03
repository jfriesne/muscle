/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/UDPSocketDataIO.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/MiscUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static const int DEFAULT_PORT = 8000;  // LX-300's default port for OSC

static status_t ReadIncomingData(const String & desc, DataIO & readIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ)
{
   if (multiplexer.IsSocketReadyForRead(readIO.GetReadSelectSocket().GetFileDescriptor()))
   {
      uint8 buf[4096];
      const int32 ret = readIO.Read(buf, sizeof(buf));
      if (ret > 0) 
      {
         LogTime(MUSCLE_LOG_TRACE, "Read " INT32_FORMAT_SPEC " bytes from %s:\n", ret, desc());
         LogHexBytes(MUSCLE_LOG_TRACE, buf, ret);
     
         ByteBufferRef toNetworkBuf = GetByteBufferFromPool(ret, buf);
         if (toNetworkBuf()) (void) outQ.AddTail(toNetworkBuf); 
      }
      else if (ret < 0) {LogTime(MUSCLE_LOG_ERROR, "Error, readIO.Read() returned %i\n", ret); return B_IO_ERROR;}
   }
   return B_NO_ERROR;
}

static status_t WriteOutgoingData(const String & desc, DataIO & writeIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ, uint32 & writeIdx)
{
   if (multiplexer.IsSocketReadyForWrite(writeIO.GetWriteSelectSocket().GetFileDescriptor()))
   {
      while(outQ.HasItems())
      {
         ByteBufferRef & firstBuf = outQ.Head();
         const uint32 bufSize = firstBuf()->GetNumBytes();
         if (writeIdx >= bufSize) 
         {
            outQ.RemoveHead();
            writeIdx = 0;
         }
         else
         {
            const int32 ret = writeIO.Write(firstBuf()->GetBuffer()+writeIdx, firstBuf()->GetNumBytes()-writeIdx);
            if (ret > 0)
            {
               writeIO.FlushOutput();
               LogTime(MUSCLE_LOG_TRACE, "Wrote " INT32_FORMAT_SPEC " bytes to %s:\n", ret, desc());
               LogHexBytes(MUSCLE_LOG_TRACE, firstBuf()->GetBuffer()+writeIdx, ret);
               writeIdx += ret;
            }
            else if (ret < 0) LogTime(MUSCLE_LOG_ERROR, "Error, writeIO.Write() returned %i\n", ret);
         }
      }
   }
   return B_NO_ERROR;
}

static status_t DoSession(const String aDesc, DataIO & aIO, const String & bDesc, DataIO & bIO)
{
   Queue<ByteBufferRef> outgoingBData;
   Queue<ByteBufferRef> outgoingAData;
   uint32 bIndex = 0, aIndex = 0;
   SocketMultiplexer multiplexer;

   while(true)
   {
      const int aReadFD  = aIO.GetReadSelectSocket().GetFileDescriptor();
      const int bReadFD  = bIO.GetReadSelectSocket().GetFileDescriptor();
      const int aWriteFD = aIO.GetWriteSelectSocket().GetFileDescriptor();
      const int bWriteFD = bIO.GetWriteSelectSocket().GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(aReadFD);
      multiplexer.RegisterSocketForReadReady(bReadFD);
      if (outgoingAData.HasItems()) multiplexer.RegisterSocketForWriteReady(aWriteFD);
      if (outgoingBData.HasItems()) multiplexer.RegisterSocketForWriteReady(bWriteFD);

      if (multiplexer.WaitForEvents() >= 0)
      {
         status_t ret;
         if (ReadIncomingData( aDesc, aIO, multiplexer, outgoingBData)        .IsError(ret)) return ret;
         if (ReadIncomingData( bDesc, bIO, multiplexer, outgoingAData)        .IsError(ret)) return ret;
         if (WriteOutgoingData(aDesc, aIO, multiplexer, outgoingAData, aIndex).IsError(ret)) return ret;
         if (WriteOutgoingData(bDesc, bIO, multiplexer, outgoingBData, bIndex).IsError(ret)) return ret;
      }
      else 
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error, WaitForEvents() failed! [%s]\n", B_ERRNO());
         return B_ERROR("WaitForEvents() failed");
      }
   }
}

static void LogUsage()
{
   Log(MUSCLE_LOG_INFO, "Usage:  udpproxy target=192.168.1.101:8000 [listen=9000] target=192.168.1.2:8000 [listen=9001]\n");
}

// This program acts as a proxy to redirect UDP packets to a further source (and back)
int main(int argc, char ** argv) 
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   if (args.HasName("help"))
   {
      LogUsage();
      return 10;
   }

   status_t ret;
  
   uint16 listenPorts[2] = {DEFAULT_PORT, DEFAULT_PORT+1};
   IPAddressAndPort targets[2];
   {
      uint16 targetPorts[2] = {DEFAULT_PORT, DEFAULT_PORT+1};
      String hostNames[2];
      for (uint32 i=0; i<2; i++)
      {
         if (ParseConnectArg(args, "target", hostNames[i], targetPorts[i], false, i).IsError(ret))
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Error, couldn't parse target argument #%i [%s]\n", i+1, ret());
            LogUsage();
            return 10;
         }
         (void) ParsePortArg(args, "listen", listenPorts[i], i);

         targets[i] = IPAddressAndPort(GetHostByName(hostNames[i]()), targetPorts[i]);
         if (targets[i].GetIPAddress().IsValid()) LogTime(MUSCLE_LOG_INFO, "Sending to target %s, listening on port %u\n", targets[i].ToString()(), listenPorts[i]);
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't resolve hostname [%s]\n", hostNames[i]());
            return 10;
         }
      }
   }

   DataIORef udpIOs[2];
   for (uint32 i=0; i<2; i++)
   {
      ConstSocketRef udpSock = CreateUDPSocket();
      if (udpSock() == NULL)
      {
         LogTime(MUSCLE_LOG_ERROR, "Creating UDP socket failed!\n");
         return 10;
      }
      if (BindUDPSocket(udpSock, listenPorts[i], &listenPorts[i], invalidIP, true).IsError(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "Failed to bind UDP socket to port %u! [%s]\n", listenPorts[i], ret());
         return 10;
      }

#ifndef MUSCLE_AVOID_MULTICAST_API
      const IPAddress & ip = targets[i].GetIPAddress();

      // If it's a multicast address, we need to add ourselves to the multicast group
      // in order to get packets from the group.
      if (ip.IsMulticast())
      {
         if (AddSocketToMulticastGroup(udpSock, ip).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_INFO, "Added UDP socket to multicast group %s!\n", Inet_NtoA(ip)());
#ifdef DISALLOW_MULTICAST_TO_SELF
            if (SetSocketMulticastToSelf(udpSock, false).IsError()) LogTime(MUSCLE_LOG_ERROR, "Error disabling multicast-to-self on socket\n");
#endif
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error adding UDP socket to multicast group %s! [%s]\n", Inet_NtoA(ip)(), ret());
      }
#endif

      UDPSocketDataIO * dio = newnothrow UDPSocketDataIO(udpSock, false);
      if (dio == NULL)
      {
         MWARN_OUT_OF_MEMORY;
         return 10;
      }
      (void) dio->SetPacketSendDestination(targets[i]);
      udpIOs[i].SetRef(dio);
   }

   ret = DoSession(targets[0].ToString(), *udpIOs[0](), targets[1].ToString(), *udpIOs[1]());
   LogTime(MUSCLE_LOG_INFO, "udpproxy exiting:  %s!\n", ret());
   return 0;
}
