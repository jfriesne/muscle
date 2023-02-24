/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"
#include "dataio/PacketizedProxyDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "system/SetupSystem.h"

using namespace muscle;

// This is a simple test of the PacketizedProxyDataIO class.  Once instance connects and sends
// data over a TCP stream in varying packet sizes, the other instance accepts the connection
// and makes sure that the data is being (de)packetized correctly.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args;
   (void) ParseArgs(argc, argv, args);

   if (args.HasName("fromscript"))
   {
      printf("Called from script, skipping test\n");
      return 0;
   }

   const char * temp;
   IPAddress connectTo = invalidIP;
   if (args.FindString("host", &temp).IsOK()) connectTo = GetHostByName(temp);

   uint16 port = 0;
   if (args.FindString("port", &temp).IsOK()) port = (uint16) atoi(temp);
   if (port == 0) port = 8888;

   uint32 mtu = 0;
   if (args.FindString("mtu", &temp).IsOK()) mtu = (uint32) atoi(temp);
   if (mtu == 0) mtu = 64*1024;

   ConstSocketRef s;
   if (connectTo != invalidIP)
   {
      s = Connect(IPAddressAndPort(connectTo, port), NULL, "testpacketio", false);
      if (s() == NULL) return 10;
   }
   else
   {
      ConstSocketRef as = CreateAcceptingSocket(port);
      if (as() == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Could not create TCP socket on port %u!\n", port);
         return 10;
      }
      LogTime(MUSCLE_LOG_INFO, "Awaiting TCP connection on port %u...\n", port);

      s = Accept(as);
      if (s() == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Accept() failed!\n");
         return 10;
      }
   }

   ByteBuffer buf;
   if (buf.SetNumBytes(mtu, false).IsError())
   {
      MWARN_OUT_OF_MEMORY;
      return 10;
   }

   TCPSocketDataIO tcp(s, true);
   PacketizedProxyDataIO pack(DummyDataIORef(tcp), mtu);

   if (connectTo == invalidIP)
   {
      LogTime(MUSCLE_LOG_INFO, "Receiving packetized data...\n");
      while(1)
      {
         const io_status_t numBytesRead = pack.Read(buf.GetBuffer(), buf.GetNumBytes());
         if (numBytesRead.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "Connection closed! [%s]\n", numBytesRead.GetStatus()());
            break;
         }

         const uint32 nbr = numBytesRead.GetByteCount();
         LogTime(MUSCLE_LOG_INFO, "Read a packet that was " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes long...\n", nbr, mtu);
         const uint8 c = nbr % 256;
         const uint8 * p = buf.GetBuffer();
         for (uint32 i=0; i<nbr; i++)
         {
            if (p[i] != c)
            {
               LogTime(MUSCLE_LOG_ERROR, "Position " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":  expected %u, got %u!\n", i, nbr, c, p[i]);
               break;
            }
         }
      }
   }
   else
   {
      // We're sending...
      while(1)
      {
         const uint32 sendLen = rand() % mtu;
         const uint8 c = (sendLen % 256);
         uint8 * b = buf.GetBuffer();
         for (uint32 i=0; i<sendLen; i++) b[i] = c;
         const io_status_t numBytesSent = pack.Write(b, sendLen);
         LogTime(MUSCLE_LOG_INFO, "Write(" UINT32_FORMAT_SPEC ") returned " INT32_FORMAT_SPEC " [%s]\n", sendLen, numBytesSent.GetByteCount(), numBytesSent.GetStatus()());
         if (numBytesSent.IsError()) break;
      }
   }
   LogTime(MUSCLE_LOG_INFO, "Exiting, bye!\n");
   return 0;
}
