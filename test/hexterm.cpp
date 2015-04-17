/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "dataio/RS232DataIO.h"
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "system/SetupSystem.h"
#include "system/SystemInfo.h"  // for GetFilePathSeparator()
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "util/MiscUtilityFunctions.h"

#ifdef BUILD_MUSCLE_IN_MEYER_CONTEXT
# include "version/dmitri_version.h"
#endif

using namespace muscle;

static bool _useHex             = true;
static bool _printChecksums     = false;
static bool _decorateOutput     = true;
static bool _printReceivedBytes = true;
static bool _quietSend          = false;
static uint32 _spamsPerSecond   = 0;
static uint32 _spamSize         = 1024;

static uint32 Calculate32BitChecksum(const uint8 * bytes, uint32 numBytes)
{
   // djb2 hash, as described at http://www.cse.yorku.ca/~oz/hash.html
   uint32 hash = 5381;
   for (uint32 i=0; i<numBytes; i++) hash = (hash*33) + bytes[i];
   return hash;
}

static String ChecksumHexString(uint32 checksum)
{
   uint8 bytes[5];
   for (uint32 i=0; i<5; i++) {bytes[i] = (checksum&0x7F); checksum >>= 7;}
   return HexBytesToString(bytes, 5);
}

static void LogChecksum(const uint8 * buf, uint32 numBytes)
{
   uint32 chk = Calculate32BitChecksum(buf, numBytes);
   LogTime(MUSCLE_LOG_INFO, "Computed checksum is " UINT32_FORMAT_SPEC " [%s]\n", chk, ChecksumHexString(chk)());
}

static void LogBytes(const uint8 * buf, uint32 numBytes, const char * optDesc)
{
   if (_useHex)
   {
      if (!_quietSend) LogHexBytes(MUSCLE_LOG_INFO, buf, numBytes, optDesc);
      if (_printChecksums) LogChecksum(buf, numBytes);
   }
   else
   {
      if (_decorateOutput)
      {
         LogTime(MUSCLE_LOG_INFO, "/-----------Begin " UINT32_FORMAT_SPEC " bytes of %s%sAscii Data-----------\\\n", numBytes, optDesc?optDesc:"", optDesc?" ":"");

         bool atFront = true;
         for (uint32 i=0; i<numBytes; i++)
         {
            if (atFront)
            {
               LogTime(MUSCLE_LOG_INFO, "| ");
               atFront = false;
            }
            Log(MUSCLE_LOG_INFO, "%c", buf[i]);
            if (buf[i] == '\n') atFront = true;
         }
         if (atFront) LogTime(MUSCLE_LOG_INFO, "| ");
         Log(MUSCLE_LOG_INFO, "\n");
      }
      else for (uint32 i=0; i<numBytes; i++) putchar(buf[i]);
      if (_printChecksums) LogChecksum(buf, numBytes);
      if (_decorateOutput) LogTime(MUSCLE_LOG_INFO, "\\-----------End " UINT32_FORMAT_SPEC " bytes of %s%sAscii Data-------------/\n", numBytes, optDesc?optDesc:"", optDesc?" ":"");
   }
}

static void DoSession(DataIO & io)
{
   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway; stdinGateway.SetDataIO(DataIORef(&stdinIO, false));
   QueueGatewayMessageReceiver receiver;
   ByteBufferRef spamBuf;  if (_spamsPerSecond > 0) spamBuf = GetByteBufferFromPool(_spamSize);

   const UDPSocketDataIO * optUDPIO = dynamic_cast<UDPSocketDataIO *>(&io);
   String scratchString;

   SocketMultiplexer multiplexer;

   uint64 spamTime = ((_spamsPerSecond > 0)&&(_spamsPerSecond != MUSCLE_NO_LIMIT)) ? GetRunTime64() : MUSCLE_TIME_NEVER;
   while(true)
   {
      int readFD  = io.GetReadSelectSocket().GetFileDescriptor();
      int writeFD = io.GetWriteSelectSocket().GetFileDescriptor();
      int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(readFD);
      if (_spamsPerSecond == MUSCLE_NO_LIMIT) multiplexer.RegisterSocketForWriteReady(writeFD);
      multiplexer.RegisterSocketForReadReady(stdinFD);
      if (multiplexer.WaitForEvents(spamTime) >= 0)
      {
         if (((_spamsPerSecond == MUSCLE_NO_LIMIT)&&(multiplexer.IsSocketReadyForWrite(writeFD)))||(GetRunTime64() >= spamTime))
         {
            int32 spamBytesSent = 0;
            uint8 * b = spamBuf() ? spamBuf()->GetBuffer() : NULL;
            if (b)
            {
               uint8 v = (uint8)(spamTime%256);
               for (uint32 i=0; i<_spamSize; i++) b[i] = v++;  // just some nice arbitrary data
               spamBytesSent = io.WriteFully(b, _spamSize);
            }
            if ((!_quietSend)&&(_decorateOutput)) LogTime(MUSCLE_LOG_ERROR, "Sent " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes of spam!\n", spamBytesSent, _spamSize);
            spamTime += (1000000/_spamsPerSecond);
         }

         if (multiplexer.IsSocketReadyForRead(readFD))
         {
            uint8 buf[2048];
            int32 ret = io.Read(buf, sizeof(buf));
            if (ret > 0)
            {
               if (_printReceivedBytes)
               {
                  const char * desc = "Received";
                  if (optUDPIO)
                  {
                     scratchString = String("Received from %1").Arg(optUDPIO->GetSourceOfLastReadPacket().ToString()());
                     desc = scratchString();
                  }
                  LogBytes(buf, ret, desc);
               }
               else LogTime(MUSCLE_LOG_DEBUG, "Received " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes of data.\n", ret, sizeof(buf));
            }
            else if (ret < 0)
            {
               LogTime(MUSCLE_LOG_ERROR, "Read() returned " INT32_FORMAT_SPEC ", aborting!\n", ret);
               break;
            }
         }
         if ((stdinFD >= 0)&&(multiplexer.IsSocketReadyForRead(stdinFD)))
         {
            while(1)
            {
               int32 bytesRead = stdinGateway.DoInput(receiver);
               if (bytesRead < 0)
               {
                  stdinFD = -1;  // indicate that stdin is no longer available
                  break;
               }
               if (bytesRead == 0) break;  // nothing more to read, for now
            }

            // Gather stdin bytes together into a single large buffer, so we can send them in as few groups as possible
            // (Main benefit is that this makes for prettier pretty-printed output on the receiving, if the receiver is another hexterm)
            ByteBufferRef outBuf = GetByteBufferFromPool();
            MessageRef nextMsg;
            while(receiver.GetMessages().RemoveHead(nextMsg) == B_NO_ERROR)
            {
               const char * b;
               for (int32 i=0; (nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &b) == B_NO_ERROR); i++)
               {
                  ByteBufferRef nextBuf;
                  if (_useHex) nextBuf = ParseHexBytes(b);
                  else
                  {
                     nextBuf = GetByteBufferFromPool(strlen(b)+1, (const uint8 *) b);
                     if (nextBuf()) nextBuf()->AppendByte('\n'); // add a newline byte
                  }

                  uint32 count = nextBuf() ? nextBuf()->GetNumBytes() : 0;
                  if ((count > 0)&&(outBuf()->AppendBytes(nextBuf()->GetBuffer(), nextBuf()->GetNumBytes()) != B_NO_ERROR))
                  {
                     WARN_OUT_OF_MEMORY;
                     break;
                  }
               }
            }

            if (outBuf())
            {
               uint32 wrote = io.WriteFully(outBuf()->GetBuffer(), outBuf()->GetNumBytes());
               if (wrote == outBuf()->GetNumBytes())
               {
                  if (_decorateOutput) LogBytes(outBuf()->GetBuffer(), outBuf()->GetNumBytes(), "Sent");
               }
               else
               {
                  LogTime(MUSCLE_LOG_ERROR, "Error, Write() only wrote " INT32_FORMAT_SPEC " of " UINT32_FORMAT_SPEC " bytes... aborting!\n", wrote, outBuf()->GetNumBytes());
                  break;
               }
            }
            if (stdinFD < 0) break;  // all done now!
         }
      }
      else break;
   }
}

static void DoUDPSession(const String & optHost, uint16 port)
{
   ConstSocketRef ss = CreateUDPSocket();
   if (ss() == NULL)
   {
      LogTime(MUSCLE_LOG_ERROR, "Error creating UDP socket!\n");
      return;
   }

   UDPSocketDataIO udpIO(ss, false);
   if (optHost.HasChars())
   {
      ip_address ip = GetHostByName(optHost(), false);
      if (ip != invalidIP)
      {
#ifndef MUSCLE_AVOID_MULTICAST_API
         // If it's a multicast address, we need to add ourselves to the multicast group
         // in order to get packets from the group.
         if (IsMulticastIPAddress(ip))
         {
            if (BindUDPSocket(ss, port, NULL, invalidIP, true) == B_NO_ERROR)
            {
               if (AddSocketToMulticastGroup(ss, ip) == B_NO_ERROR)
               {
                  LogTime(MUSCLE_LOG_INFO, "Added UDP socket to multicast group %s!\n", Inet_NtoA(ip)());
#ifdef DISALLOW_MULTICAST_TO_SELF
                  if (SetSocketMulticastToSelf(ss, false) != B_NO_ERROR) LogTime(MUSCLE_LOG_ERROR, "Error disabling multicast-to-self on socket\n");
#endif
               }
               else LogTime(MUSCLE_LOG_ERROR, "Error adding UDP socket to multicast group %s!\n", Inet_NtoA(ip)());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error binding multicast socket to port %u\n", port);
         }
#endif

#ifdef MUSCLE_AVOID_IPV6
         if ((ip & 0xFF) == 0xFF)
         {
            if (SetUDPSocketBroadcastEnabled(ss, true) == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO, "Broadcast UDP address detected:  UDP broadcast enabled on socket.\n");
                                                                 else LogTime(MUSCLE_LOG_ERROR, "Could not enable UDP broadcast on socket!\n");
         }
#endif
         IPAddressAndPort iap(ip, port);
         udpIO.SetSendDestination(iap);
         LogTime(MUSCLE_LOG_INFO, "Ready to send UDP packets to %s\n", iap.ToString()());
         DoSession(udpIO);
      }
      else LogTime(MUSCLE_LOG_ERROR, "Could not look up target hostname [%s]\n", optHost());
   }
   else
   {
      if (BindUDPSocket(ss, port) == B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_INFO, "Listening for incoming UDP packets on port %i\n", port);
         DoSession(udpIO);
      }
      else LogTime(MUSCLE_LOG_ERROR, "Could not bind UDP socket to port %i\n", port);
   }
}

static void LogUsage(const char * argv0)
{
   String progName = String(argv0).Substring(GetFilePathSeparator());

#ifdef BUILD_MUSCLE_IN_MEYER_CONTEXT
   char buf[256];
   Log(MUSCLE_LOG_INFO, "%s (%s)\n\n", progName(), GetLocalDmitriReleaseVersionTitle(progName(), false, buf));
#else
   Log(MUSCLE_LOG_INFO, "%s (compiled from MUSCLE v%s)\n\n", progName(), MUSCLE_VERSION_STRING);
#endif
   Log(MUSCLE_LOG_INFO, "Usage:  hexterm tcp=<port>              (listen for incoming TCP connections on the given port)\n");
   Log(MUSCLE_LOG_INFO, "   or:  hexterm tcp=<host>:<port>       (make an outgoing TCP connection to the given host/port)\n");
   Log(MUSCLE_LOG_INFO, "   or:  hexterm udp=<host>:<port>       (send outgoing UDP packets to the given host/port)\n");
   Log(MUSCLE_LOG_INFO, "   or:  hexterm udp=<port>              (listen for incoming UDP packets on the given port)\n");
   Log(MUSCLE_LOG_INFO, "   or:  hexterm serial=<devname>:<baud> (send/receive via a serial device, e.g. /dev/ttyS0)\n");
   Log(MUSCLE_LOG_INFO, "   or:  hexterm child=<prog_and_args>   (send/receive via a child process, e.g. 'ls -l')\n");
   Log(MUSCLE_LOG_INFO, "  Additional optional args include:\n");
   Log(MUSCLE_LOG_INFO, "                ascii                   (print and parse bytes as ASCII rather than hexadecimal)\n");
   Log(MUSCLE_LOG_INFO, "                plain                   (Suppress decorative elements in hexterm's output)\n");
   Log(MUSCLE_LOG_INFO, "                quietreceive            (Suppress the printing out of incoming data bytes)\n");
   Log(MUSCLE_LOG_INFO, "                spamrate=<Hz>           (Specify number of automatic-spam-transmissions to send per second)\n");
   Log(MUSCLE_LOG_INFO, "                spamsize=<bytes>        (Specify size of each automatic-spam-transmission; defaults to 1024)\n");
   Log(MUSCLE_LOG_INFO, "                printchecksums          (print checksums for incoming and sent data)\n");
   Log(MUSCLE_LOG_INFO, "                help                    (print this help text)\n");
}

// Secondary entry point, used when embedding hexterm in a unified daemon
int hextermmain(const char * argv0, const Message & args)
{
   _printChecksums = args.HasName("printchecksums");
   if (_printChecksums) printf("Checksum printing enabled.\n");

   if (args.HasName("help"))
   {
      LogUsage(argv0);
      return 0;
   }
   if (args.HasName("ascii"))
   {
      LogTime(MUSCLE_LOG_INFO, "ASCII mode activated!\n");
      _useHex = false;
   }
   if (args.HasName("plain"))
   {
      LogTime(MUSCLE_LOG_INFO, "Decorative output characters will be suppressed.\n");
      _decorateOutput = false;
   }

   _printReceivedBytes = (args.HasName("quietreceive") == false);
   _quietSend = args.HasName("quietsend");

   if (args.HasName("spamspersecond"))
   {
      const char * sizeStr = args.GetCstr("spamsize");
      if (sizeStr) _spamSize = atol(sizeStr);

      _spamsPerSecond = atoi(args.GetCstr("spamspersecond"));
      LogTime(MUSCLE_LOG_INFO, "Will generate and send " UINT32_FORMAT_SPEC " " UINT32_FORMAT_SPEC "-byte spam-transmissions per second.\n", _spamsPerSecond, _spamSize);
   }

   String host;
   uint16 port;

   String arg;
   if (args.FindString("child", arg) == B_NO_ERROR)
   {
      ChildProcessDataIO cpdio(false);
      int32 spaceIdx       = arg.IndexOf(' ');
      String childProgName = arg.Substring(0, spaceIdx).Trim();
      String childArgs     = arg.Substring(spaceIdx).Trim()();
      if (cpdio.LaunchChildProcess(arg()) == B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_INFO, "Communicating with child process (%s), childArgs=[%s]\n", childProgName(), childArgs());
         DoSession(cpdio);
         LogTime(MUSCLE_LOG_INFO, "Child process session aborted, exiting.\n");
      }
      else LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to open child process (%s) with childArgs (%s)\n", childProgName(), childArgs());
   }
   else if (args.FindString("serial", arg) == B_NO_ERROR)
   {
      const char * colon = strchr(arg(), ':');
      uint32 baudRate = colon ? atoi(colon+1) : 0; if (baudRate == 0) baudRate = 38400;
      String devName = arg.Substring(0, ":");
      Queue<String> devs;
      if (RS232DataIO::GetAvailableSerialPortNames(devs) == B_NO_ERROR)
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
            RS232DataIO io(devName(), baudRate, false);
            if (io.IsPortAvailable())
            {
               LogTime(MUSCLE_LOG_INFO, "Communicating with serial port %s (baud rate " UINT32_FORMAT_SPEC ")\n", serName(), baudRate);
               DoSession(io);
               LogTime(MUSCLE_LOG_INFO, "Serial session aborted, exiting.\n");
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
   else if (ParseConnectArg(args, "tcp", host, port, true) == B_NO_ERROR)
   {
      ConstSocketRef ss = Connect(host(), port, "hexterm", false);
      if (ss())
      {
         LogTime(MUSCLE_LOG_INFO, "Connected to [%s:%i]\n", host(), port);
         TCPSocketDataIO io(ss, false);
         DoSession(io);
         LogTime(MUSCLE_LOG_INFO, "Session socket disconnected, exiting.\n");
      }
      else LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to connect to %s\n", GetConnectString(host, port)());
   }
   else if (ParsePortArg(args, "tcp", port) == B_NO_ERROR)
   {
      ConstSocketRef as = CreateAcceptingSocket(port);
      if (as())
      {
         LogTime(MUSCLE_LOG_INFO, "Listening for incoming TCP connections on port %i\n", port);
         while(true)
         {
            ip_address acceptedFromIP;
            ConstSocketRef ss = Accept(as, &acceptedFromIP);
            if (ss())
            {
               char cbuf[64]; Inet_NtoA(GetPeerIPAddress(ss, true), cbuf);
               char hbuf[64]; Inet_NtoA(acceptedFromIP, hbuf);
               LogTime(MUSCLE_LOG_INFO, "Accepted TCP connection from %s on interface %s, awaiting data...\n", cbuf, hbuf);
               TCPSocketDataIO io(ss, false);
               DoSession(io);
               LogTime(MUSCLE_LOG_ERROR, "Session socket disconnected, awaiting next connection.\n");
            }
         }
      }
      else LogTime(MUSCLE_LOG_CRITICALERROR, "Could not bind to port %i\n", port);
   }
   else if (ParseConnectArg(args, "udp", host, port, true) == B_NO_ERROR) DoUDPSession(host, port);
   else if (ParsePortArg   (args, "udp", port)             == B_NO_ERROR) DoUDPSession("", port);
   else LogUsage(argv0);

   return 0;
}

// This program can send and receive raw hexadecimal bytes over TCP, UDP, or serial.
// Hex bytes are displayed and entered in ASCII format.
// Good for interactive debugging of low-level protocols like MIDI.
#ifndef UNIFIED_DAEMON
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   (void) HandleStandardDaemonArgs(args);
   return hextermmain(argv[0], args);
}
#endif

