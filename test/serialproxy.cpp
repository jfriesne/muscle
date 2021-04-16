/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "dataio/TCPSocketDataIO.h"
#include "dataio/RS232DataIO.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/MiscUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static const int DEFAULT_PORT = 5274;  // What CueStation 2.5 connects to by default

static status_t ReadIncomingData(const char * desc, DataIO & readIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ)
{
   if (multiplexer.IsSocketReadyForRead(readIO.GetReadSelectSocket().GetFileDescriptor()))
   {
      uint8 buf[4096];
      const int32 ret = readIO.Read(buf, sizeof(buf));
      if (ret > 0) 
      {
         LogTime(MUSCLE_LOG_TRACE, "Read " INT32_FORMAT_SPEC " bytes from %s:\n", ret, desc);
         LogHexBytes(MUSCLE_LOG_TRACE, buf, ret);
     
         ByteBufferRef toNetworkBuf = GetByteBufferFromPool(ret, buf);
         if (toNetworkBuf()) (void) outQ.AddTail(toNetworkBuf); 
      }
      else if (ret < 0) {LogTime(MUSCLE_LOG_ERROR, "Error, readIO.Read() returned %i\n", ret); return B_IO_ERROR;}
   }
   return B_NO_ERROR;
}

static status_t WriteOutgoingData(const char * desc, DataIO & writeIO, const SocketMultiplexer & multiplexer, Queue<ByteBufferRef> & outQ, uint32 & writeIdx)
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
               LogTime(MUSCLE_LOG_TRACE, "Wrote " INT32_FORMAT_SPEC " bytes to %s:\n", ret, desc);
               LogHexBytes(MUSCLE_LOG_TRACE, firstBuf()->GetBuffer()+writeIdx, ret);
               writeIdx += ret;
            }
            else if (ret < 0) {LogTime(MUSCLE_LOG_ERROR, "Error, writeIO.Write() returned %i\n", ret); return B_IO_ERROR;}
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

      multiplexer.RegisterSocketForReadReady(networkReadFD);
      multiplexer.RegisterSocketForReadReady(serialReadFD);

      if (outgoingNetworkData.HasItems()) multiplexer.RegisterSocketForWriteReady(networkWriteFD);
      if (outgoingSerialData.HasItems())  multiplexer.RegisterSocketForWriteReady(serialWriteFD);

      if (multiplexer.WaitForEvents() >= 0)
      {
         status_t ret;
         if (ReadIncomingData("network",  networkIO, multiplexer, outgoingSerialData)               .IsError())    return B_NO_ERROR;  // tells main() to wait for the next TCP connection
         if (ReadIncomingData("serial",   serialIO,  multiplexer, outgoingNetworkData)              .IsError(ret)) return ret;         // tells main() to exit
         if (WriteOutgoingData("network", networkIO, multiplexer, outgoingNetworkData, networkIndex).IsError())    return B_NO_ERROR;  // tells main() to wait for the next TCP connection
         if (WriteOutgoingData("serial",  serialIO,  multiplexer, outgoingSerialData,  serialIndex) .IsError(ret)) return ret;         // tells main() to exit
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
   Log(MUSCLE_LOG_INFO, "Usage:  serialproxy serial=<devname>:<baud> [port=5274] (send/receive via a serial device, e.g. /dev/ttyS0)\n");
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
      if (ps) port = atoi(ps->Cstr());
   }
   if (port == 0) port = DEFAULT_PORT;
   
   String arg;
   if (args.FindString("serial", arg).IsOK())
   {
      const char * colon = strchr(arg(), ':');
      uint32 baudRate = colon ? atoi(colon+1) : 0; if (baudRate == 0) baudRate = 38400;
      String devName = arg.Substring(0, ":");
      Queue<String> devs;
      if (RS232DataIO::GetAvailableSerialPortNames(devs).IsOK())
      {
         String serName;
         for (int32 i=devs.GetNumItems()-1; i>=0; i--)
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
                  bool keepGoing = true;
                  while(keepGoing)
                  {
                     LogTime(MUSCLE_LOG_INFO, "Awaiting incoming TCP connection on port %u...\n", port);
                     ConstSocketRef tcpSock = Accept(serverSock);
                     if (tcpSock())
                     {
                        LogTime(MUSCLE_LOG_INFO, "Beginning serial proxy session!\n");
                        TCPSocketDataIO networkIO(tcpSock, false);
                        keepGoing = (DoSession(networkIO, serialIO).IsOK());
                        LogTime(MUSCLE_LOG_INFO, "Serial proxy session ended%s!\n", keepGoing?", awaiting new connection":", aborting!");
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
