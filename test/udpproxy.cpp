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
      int32 ret = readIO.Read(buf, sizeof(buf));
      if (ret > 0)
      {
         LogTime(MUSCLE_LOG_TRACE, "Read " INT32_FORMAT_SPEC" bytes from %s:\n", ret, desc());
         LogHexBytes(MUSCLE_LOG_TRACE, buf, ret);

         ByteBufferRef toNetworkBuf = GetByteBufferFromPool(ret, buf);
         if (toNetworkBuf()) (void) outQ.AddTail(toNetworkBuf);
      }
      else if (ret < 0) {LogTime(MUSCLE_LOG_ERROR, "Error, readIO.Read() returned %i\n", ret); return B_ERROR;}
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
         uint32 bufSize = firstBuf()->GetNumBytes();
         if (writeIdx >= bufSize)
         {
            outQ.RemoveHead();
            writeIdx = 0;
         }
         else
         {
            int32 ret = writeIO.Write(firstBuf()->GetBuffer()+writeIdx, firstBuf()->GetNumBytes()-writeIdx);
            if (ret > 0)
            {
               writeIO.FlushOutput();
               LogTime(MUSCLE_LOG_TRACE, "Wrote " INT32_FORMAT_SPEC" bytes to %s:\n", ret, desc());
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
      int aReadFD  = aIO.GetReadSelectSocket().GetFileDescriptor();
      int bReadFD  = bIO.GetReadSelectSocket().GetFileDescriptor();
      int aWriteFD = aIO.GetWriteSelectSocket().GetFileDescriptor();
      int bWriteFD = bIO.GetWriteSelectSocket().GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(aReadFD);
      multiplexer.RegisterSocketForReadReady(bReadFD);
      if (outgoingAData.HasItems()) multiplexer.RegisterSocketForWriteReady(aWriteFD);
      if (outgoingBData.HasItems()) multiplexer.RegisterSocketForWriteReady(bWriteFD);

      if (multiplexer.WaitForEvents() >= 0)
      {
         if (ReadIncomingData( aDesc, aIO, multiplexer, outgoingBData)         != B_NO_ERROR) return B_ERROR;
         if (ReadIncomingData( bDesc, bIO, multiplexer, outgoingAData)         != B_NO_ERROR) return B_ERROR;
         if (WriteOutgoingData(aDesc, aIO, multiplexer, outgoingAData, aIndex) != B_NO_ERROR) return B_ERROR;
         if (WriteOutgoingData(bDesc, bIO, multiplexer, outgoingBData, bIndex) != B_NO_ERROR) return B_ERROR;
      }
      else
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error, WaitForEvents() failed!\n");
         return B_ERROR;
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

   uint16 listenPorts[2] = {DEFAULT_PORT, DEFAULT_PORT+1};
   IPAddressAndPort targets[2];
   {
      uint16 targetPorts[2] = {DEFAULT_PORT, DEFAULT_PORT+1};
      String hostNames[2];
      for (uint32 i=0; i<2; i++)
      {
         if (ParseConnectArg(args, "target", hostNames[i], targetPorts[i], false, i) != B_NO_ERROR)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Error, couldn't parse target argument #%i\n", i+1);
            LogUsage();
            return 10;
         }
         (void) ParsePortArg(args, "listen", listenPorts[i], i);

         targets[i] = IPAddressAndPort(GetHostByName(hostNames[i]()), targetPorts[i]);
         if (IsValidAddress(targets[i].GetIPAddress())) LogTime(MUSCLE_LOG_INFO, "Sending to target %s, listening on port %u\n", targets[i].ToString()(), listenPorts[i]);
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
      if (BindUDPSocket(udpSock, listenPorts[i], &listenPorts[i]) != B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_ERROR, "Failed to bind UDP socket to port %u!\n", listenPorts[i]);
         return 10;
      }
      UDPSocketDataIO * dio = newnothrow UDPSocketDataIO(udpSock, false);
      if (dio == NULL)
      {
         WARN_OUT_OF_MEMORY;
         return 10;
      }
      dio->SetSendDestination(targets[i]);
      udpIOs[i].SetRef(dio);
   }

   status_t ret = DoSession(targets[0].ToString(), *udpIOs[0](), targets[1].ToString(), *udpIOs[1]());
   LogTime(MUSCLE_LOG_INFO, "udpproxy exiting%s!\n", (ret==B_NO_ERROR)?"":" with an error");
   return 0;
}
