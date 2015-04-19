/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/TCPSocketDataIO.h"
#include "dataio/PacketizedDataIO.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "system/SetupSystem.h"

using namespace muscle;

// This is a simple test of the PacketizedDataIO class.  Once instance connects and sends
// data over a TCP stream in varying packet sizes, the other instance accepts the connection
// and makes sure that the data is being (de)packetized correctly.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args;
   (void) ParseArgs(argc, argv, args);

   const char * temp;
   ip_address connectTo = invalidIP;
   if (args.FindString("host", &temp) == B_NO_ERROR) connectTo = GetHostByName(temp);

   uint16 port = 0;
   if (args.FindString("port", &temp) == B_NO_ERROR) port = atol(temp);
   if (port == 0) port = 8888;

   uint32 mtu = 0;
   if (args.FindString("mtu", &temp) == B_NO_ERROR) mtu = atol(temp);
   if (mtu == 0) mtu = 64*1024;

   ConstSocketRef s;
   if (connectTo != invalidIP)
   {
      s = Connect(connectTo, port, NULL, "testpacketio", false);
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
   if (buf.SetNumBytes(mtu, false) != B_NO_ERROR)
   {
      WARN_OUT_OF_MEMORY;
      return 10;
   }

   TCPSocketDataIO tcp(s, true);
   PacketizedDataIO pack(DataIORef(&tcp, false), mtu);

   if (connectTo == invalidIP)
   {
      LogTime(MUSCLE_LOG_INFO, "Receiving packetized data...\n");
      while(1)
      {
         int32 numBytesRead = pack.Read(buf.GetBuffer(), buf.GetNumBytes());
         if (numBytesRead < 0)
         {
            LogTime(MUSCLE_LOG_ERROR, "Connection closed!\n");
            break;
         }

         LogTime(MUSCLE_LOG_INFO, "Read a packet that was " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " bytes long...\n", numBytesRead, mtu);
         uint8 c = numBytesRead % 256;
         const uint8 * p = buf.GetBuffer();
         for (int32 i=0; i<numBytesRead; i++)
         {
            if (p[i] != c)
            {
               LogTime(MUSCLE_LOG_ERROR, "Position " INT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ":  expected %u, got %u!\n", i, numBytesRead, c, p[i]);
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
         uint32 sendLen = rand() % mtu;
         uint8 c = (sendLen % 256);
         uint8 * b = buf.GetBuffer();
         for (uint32 i=0; i<sendLen; i++) b[i] = c;
         int32 numBytesSent = pack.Write(b, sendLen);
         LogTime(MUSCLE_LOG_INFO, "Write(" UINT32_FORMAT_SPEC ") returned " INT32_FORMAT_SPEC "\n", sendLen, numBytesSent);
         if (numBytesSent < 0) break;
      }
   }
   LogTime(MUSCLE_LOG_INFO, "Exiting, bye!\n");
   return 0;
}
