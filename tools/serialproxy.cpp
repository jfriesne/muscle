/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/TCPSocketDataIO.h"
#include "dataio/RS232DataIO.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/MiscUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static const int DEFAULT_PORT = 5274;  // What CueStation 2.5 connects to by default

static status_t ReadIncomingData(bool wantToExitOnFailure, const char * desc, DataIO & readIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ)
{
   if (multiplexer.IsSocketReadyForRead(readIO.GetReadSelectSocket().GetFileDescriptor()))
   {
      uint8 buf[4096];
      const io_status_t ret = readIO.Read(buf, sizeof(buf));
      if (ret.GetByteCount() > 0)
      {
         LogTime(MUSCLE_LOG_TRACE, "Read " INT32_FORMAT_SPEC " bytes from %s:\n", ret.GetByteCount(), desc);
         PrintHexBytes(MUSCLE_LOG_TRACE, buf, ret.GetByteCount());

         ByteBufferRef toNetworkBuf = GetByteBufferFromPool(ret.GetByteCount(), buf);
         MRETURN_ON_ERROR(toNetworkBuf);
         MRETURN_ON_ERROR(outQ.AddTail(toNetworkBuf));
      }
      else if (ret.IsError())
      {
         LogTime(MUSCLE_LOG_ERROR, "Error, readIO.Read() returned [%s]\n", ret.GetStatus()());
         return wantToExitOnFailure ? B_SHUTTING_DOWN : ret.GetStatus();
      }
   }
   return B_NO_ERROR;
}

static status_t WriteOutgoingData(bool wantExitOnFailure, const char * desc, DataIO & writeIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ, uint32 & writeIdx)
{
   if (multiplexer.IsSocketReadyForWrite(writeIO.GetWriteSelectSocket().GetFileDescriptor()))
   {
      while(outQ.HasItems())
      {
         ByteBufferRef & firstBuf = outQ.Head();
         const uint32 bufSize = firstBuf()->GetNumBytes();
         if (writeIdx >= bufSize)
         {
            (void) outQ.RemoveHead();
            writeIdx = 0;
         }
         else
         {
            const io_status_t ret = writeIO.Write(firstBuf()->GetBuffer()+writeIdx, firstBuf()->GetNumBytes()-writeIdx);
            if (ret.GetByteCount() > 0)
            {
               writeIO.FlushOutput();
               LogTime(MUSCLE_LOG_TRACE, "Wrote " INT32_FORMAT_SPEC " bytes to %s:\n", ret.GetByteCount(), desc);
               PrintHexBytes(MUSCLE_LOG_TRACE, firstBuf()->GetBuffer()+writeIdx, ret.GetByteCount());
               writeIdx += ret.GetByteCount();
            }
            else if (ret.GetByteCount() == 0)
            {
               LogTime(MUSCLE_LOG_WARNING, "Write() returned 0, will retry send after 100mS\n");
               (void) Snooze64(MillisToMicros(100));  // a cheap hack but the alternative is to set up an outgoing-data-queue to drain and that seems like overkill for this program
            }
            else if (ret.IsError())
            {
               LogTime(MUSCLE_LOG_ERROR, "Error, writeIO.Write() returned [%s]\n", ret.GetStatus()());
               return wantExitOnFailure ? B_SHUTTING_DOWN : ret.GetStatus();
            }
         }
      }
   }
   return B_NO_ERROR;
}

static status_t DoSession(DataIO & networkIO, DataIO & serialIO)
{
   Queue<ByteBufferRef> outgoingSerialData;
   Queue<ByteBufferRef> outgoingNetworkData;
   uint32 serialIndex = 0, networkIndex = 0;
   SocketMultiplexer multiplexer;
   while(true)
   {
      const int networkReadFD  = networkIO.GetReadSelectSocket().GetFileDescriptor();
      const int serialReadFD   = serialIO.GetReadSelectSocket().GetFileDescriptor();
      const int networkWriteFD = networkIO.GetWriteSelectSocket().GetFileDescriptor();
      const int serialWriteFD  = serialIO.GetWriteSelectSocket().GetFileDescriptor();

      (void) multiplexer.RegisterSocketForReadReady(networkReadFD);
      (void) multiplexer.RegisterSocketForReadReady(serialReadFD);

      if (outgoingNetworkData.HasItems()) (void) multiplexer.RegisterSocketForWriteReady(networkWriteFD);
      if (outgoingSerialData.HasItems())  (void) multiplexer.RegisterSocketForWriteReady(serialWriteFD);

      status_t ret;
      if (multiplexer.WaitForEvents().IsOK(ret))
      {
         MRETURN_ON_ERROR(ReadIncomingData(false,  "network", networkIO, multiplexer, outgoingSerialData));                 // tells main() to wait for the next TCP connection
         MRETURN_ON_ERROR(ReadIncomingData(true,   "serial",  serialIO,  multiplexer, outgoingNetworkData));                // tells main() to exit on failure
         MRETURN_ON_ERROR(WriteOutgoingData(false, "network", networkIO, multiplexer, outgoingNetworkData, networkIndex));  // tells main() to wait for the next TCP connection
         MRETURN_ON_ERROR(WriteOutgoingData(true,  "serial",  serialIO,  multiplexer, outgoingSerialData,  serialIndex));   // tells main() to exit on failure
      }
      else
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error, WaitForEvents() failed! [%s]\n", ret());
         return ret;
      }
   }
}

static void LogUsage()
{
   LogPlain(MUSCLE_LOG_INFO, "Usage:  serialproxy serial=<devname>:<baud> [port=5274] (send/receive via a serial device, e.g. /dev/ttyS0)\n");

   Queue<String> devNames;
   const status_t r = RS232DataIO::GetAvailableSerialPortNames(devNames);
   if (r.IsOK())
   {
      LogTime(MUSCLE_LOG_INFO, UINT32_FORMAT_SPEC " available serial devices detected:\n", devNames.GetNumItems());
      for (uint32 i=0; i<devNames.GetNumItems(); i++) LogTime(MUSCLE_LOG_INFO, "  %s\n", devNames[i]());
   }
   else LogTime(MUSCLE_LOG_ERROR, "GetAvailableSerialPortNames() returned [%s]\n", r());
}

// This program acts as a proxy to forward serial data to a TCP stream (and back)
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);

   if (args.HasName("help"))
   {
      LogUsage();
      return 0;
   }

   uint16 port = 0;
   {
      const String * ps = args.GetStringPointer("port");
      if (ps) port = (uint16) atoi(ps->Cstr());
   }
   if (port == 0) port = DEFAULT_PORT;

   String arg;
   if (args.FindString("serial", arg).IsOK())
   {
      const char * colon = strchr(arg(), ':');
      uint32 baudRate = colon ? (uint32)Atoull(colon+1) : 0; if (baudRate == 0) baudRate = 38400;
      String devName = arg.Substring(0, ":");
      Queue<String> devs;
      if (RS232DataIO::GetAvailableSerialPortNames(devs).IsOK())
      {
         String serName;
         for (int32 i=devs.GetLastValidIndex(); i>=0; i--)
         {
            if (devs[i] == devName)
            {
               serName = devs[i];
               break;
            }
         }
         if (serName.HasChars())
         {
            RS232DataIO serialIO(devName(), baudRate, false);
            if (serialIO.IsPortAvailable())
            {
               LogTime(MUSCLE_LOG_INFO, "Using serial port %s (baud rate " UINT32_FORMAT_SPEC ")\n", serName(), baudRate);

               ConstSocketRef serverSock = CreateAcceptingSocket(port, 1);
               if (serverSock())
               {
                  // Now we just wait here until a TCP connection comes along on our port...
                  while(1)
                  {
                     LogTime(MUSCLE_LOG_INFO, "Awaiting incoming TCP connection on port %u...\n", port);
                     ConstSocketRef tcpSock = Accept(serverSock);
                     if (tcpSock())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Beginning serial proxy session!\n");
                        TCPSocketDataIO networkIO(tcpSock, false);

                        const status_t r = DoSession(networkIO, serialIO);
                        const bool timeToExit = (r == B_SHUTTING_DOWN);
                        LogTime(MUSCLE_LOG_INFO, "Serial proxy session ended [%s], %s.\n", r(), timeToExit ? "aborting" : "awaiting next TCP connection");
                        if (timeToExit) break;
                     }
                  }
               }
               else LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to listen on TCP port %u\n", port);
            }
            else LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to open serial device %s (baud rate " UINT32_FORMAT_SPEC ").\n", serName(), baudRate);
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Serial device %s not found.\n", devName());
            LogTime(MUSCLE_LOG_CRITICALERROR, "Available serial devices are:\n");
            for (uint32 i=0; i<devs.GetNumItems(); i++) LogTime(MUSCLE_LOG_CRITICALERROR, "   %s\n", devs[i]());
         }
      }
      else LogTime(MUSCLE_LOG_CRITICALERROR, "Could not get list of serial device names!\n");
   }
   else LogUsage();

   LogTime(MUSCLE_LOG_INFO, "serialproxy exiting!\n");
   return 0;
}
