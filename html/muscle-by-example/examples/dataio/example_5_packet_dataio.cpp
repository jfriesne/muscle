#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/UDPSocketDataIO.h"
#include "util/MiscUtilityFunctions.h"    // for PrintHexBytes()
#include "util/NetworkUtilityFunctions.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates blocking UDP I/O using the UDPSocketDataIO class.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   ConstSocketRef udpSock = CreateUDPSocket();
   if (udpSock() == NULL)
   {
      printf("Error, couldn't create UDP socket!\n");
      return 10;
   }

   status_t ret;
   uint16 udpPort;
   if (BindUDPSocket(udpSock, 0, &udpPort).IsError(ret))
   {
      printf("Unable to bind UDP socket to a port! [%s]\n", ret());
      return 10;
   }

   printf("UDP socket is listening on port %u and will echo back any packets sent to it.\n", udpPort);
   printf("Note:  The examples/networkutilityfunctions/example_2_udp_pingpong example can be used to send a UDP packet to our port, if you need a way to do that.\n");

   UDPSocketDataIO udpIO(udpSock, true);

   char inputBuf[2048];
   int numBytesRead;
   while((numBytesRead = udpIO.Read(inputBuf, sizeof(inputBuf))) >= 0)
   {
      const IPAddressAndPort source = udpIO.GetSourceOfLastReadPacket();

      printf("Received a %i-byte UDP packet from %s:\n", numBytesRead, source.ToString()());
      PrintHexBytes(inputBuf, numBytesRead);

      udpIO.SetPacketSendDestination(source);
      const int numBytesSent = udpIO.Write(inputBuf, numBytesRead);
      printf("Echoed %i/%i bytes back to %s\n", numBytesSent, numBytesRead, source.ToString()());
   }
   return 0;
}
