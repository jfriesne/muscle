#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates some basic blocking-I/O UDP unicast usage of the NetworkUtilityFunctions API.\n");
   p.printf("It takes one optional argument, which is a port number on localhost to send a UDP packet to on startup.\n");
   p.printf("\n");
   p.printf("Once it is running, it will bind to a UDP port and listen for incoming UDP packets on that port.\n");
   p.printf("Any UDP packet it receives, it will echo back to the sender.\n");
   p.printf("\n");
   p.printf("Run two or more instances of this program simultaneously and point them at each other's UDP ports,\n");
   p.printf("in order to enjoy a nice game of ping-pong.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to create a UDP socket!\n");
      return 10;
   }

   status_t ret;

   // Bind to a UDP port (let the OS choose an available port for us)
   uint16 udpPort;
   if (BindUDPSocket(udpSock, 0, &udpPort).IsOK(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Bound UDP socket to port %u\n", udpPort);
   }
   else
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to bind the UDP socket! [%s]\n", ret());
      return 10;
   }

   // Serve the ball to start the game (if the user specified a port number to serve to)
   const uint16 targetPort = (argc > 1) ? atoi(argv[1]) : 0;
   if (targetPort > 0)
   {
      const uint8 serveBuf[] = "Serve!";
      const io_status_t numBytesSent = SendDataUDP(udpSock, serveBuf, sizeof(serveBuf), true, localhostIP, targetPort);
      if (numBytesSent.IsOK()) LogTime(MUSCLE_LOG_INFO, "Serve:  Sent %i/%zu bytes of serve-packet to [%s]\n", numBytesSent.GetByteCount(), sizeof(serveBuf), IPAddressAndPort(localhostIP, targetPort).ToString()());
                          else LogTime(MUSCLE_LOG_ERROR, "Serve:  Error [%s] sending %zu bytes of serve-packet to [%s]\n", numBytesSent.GetStatus()(), sizeof(serveBuf), IPAddressAndPort(localhostIP, targetPort).ToString()());
   }
   else LogTime(MUSCLE_LOG_WARNING, "No target port argument specified.  To serve the ball, specify a target port number as an argument.\n");

   char recvBuf[1024];
   int numBytesReceived;
   IPAddress sourceIP;
   uint16 sourcePort;
   while((numBytesReceived = ReceiveDataUDP(udpSock, recvBuf, sizeof(recvBuf)-1, true, &sourceIP, &sourcePort).GetByteCount()) >= 0)
   {
      const IPAddressAndPort fromIAP(sourceIP, sourcePort);  // just for convenience

      recvBuf[numBytesReceived] = '\0';  // paranoia -- ensure NUL termination
      LogTime(MUSCLE_LOG_INFO, "Received %i bytes of data from [%s]:  [%s]\n", numBytesReceived, fromIAP.ToString()(), recvBuf);

      // Now let's fire back to the sender
      String s = recvBuf;
      if (s.StartsWith("Serve")) s = "Return #1";
      else
      {
         const int returnNum = (s.StartsWith("Return #") ? atoi(s()+8) : 0) + 1;
         s = String("Return #%1").Arg(returnNum);
      }

      const io_status_t numBytesSent = SendDataUDP(udpSock, s(), s.Length()+1, true, sourceIP, sourcePort);
      if (numBytesSent.IsOK()) LogTime(MUSCLE_LOG_INFO, "Sent %i/%u bytes of data to [%s]:  [%s]\n", numBytesSent.GetByteCount(), s.Length()+1, fromIAP.ToString()(), s());
                          else LogTime(MUSCLE_LOG_ERROR, "Error [%s] sending %u bytes of data to [%s]:  [%s]\n", numBytesSent.GetStatus()(), s.Length()+1, fromIAP.ToString()(), s());

      (void) Snooze64(MillisToMicros(100));  // otherwise the ping-ponging goes too fast for my taste
   }

   LogTime(MUSCLE_LOG_INFO, "\n");
   return 0;
}
