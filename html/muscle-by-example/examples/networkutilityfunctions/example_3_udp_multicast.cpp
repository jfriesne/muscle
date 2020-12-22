#ifndef WIN32
# include <unistd.h>  // for getpid()
#endif

#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/NetworkInterfaceInfo.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/Hashtable.h"
#include "util/Queue.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates some basic UDP multicast usage of the NetworkUtilityFunctions API.\n");
   printf("Each instance of the program will send out one multicast packet every 5 seconds, and print all of the\n");
   printf("multicast packets it receives.\n");
   printf("\n");
   printf("Running a few instances of this program (either all on the same machine, or on different machines on\n");
   printf("the same LAN) makes for a fun party.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();
   
   const IPAddressAndPort multicastGroup("ff12::1234", 7777, false);  // arbitrary

   status_t ret;

   Queue<NetworkInterfaceInfo> niis;
   if (GetNetworkInterfaceInfos(niis, GNIIFlags(GNII_FLAG_INCLUDE_IPV6_INTERFACES,GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_ENABLED_INTERFACES)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to retrieve the list of NetworkInterfaceInfos! [%s]\n", ret());
      return 10;
   }

   // Let's build up the set of IPv6 scope indices we want to send/receive on
   Hashtable<uint32, Void> scopeIDs; 
   for (uint32 i=0; i<niis.GetNumItems(); i++) 
   {
      const IPAddress & ip = niis[i].GetLocalAddress();
      if (ip.IsInterfaceIndexValid()) (void) scopeIDs.PutWithDefault(ip.GetInterfaceIndex());
   }

   // Now let's set up one multicast UDP socket per scope index (IPv6 multicast
   // works more reliably that way, than if we tried to get a single socket to 
   // handle traffic on all the interfaces at once -- sadly)
   Hashtable<ConstSocketRef, int> udpSocks;   // socket -> scopeID
   for (HashtableIterator<uint32, Void> iter(scopeIDs); iter.HasData(); iter++)
   {
      const uint32 scopeID = iter.GetKey();

      ConstSocketRef udpSock = CreateUDPSocket();
      if (udpSock() == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to create a UDP socket!\n");
         return 10;
      }   

      if (BindUDPSocket(udpSock, multicastGroup.GetPort(), NULL, invalidIP, true).IsOK(ret))  // true == share the port with the other UDP sockets
      {
         if (AddSocketToMulticastGroup(udpSock, multicastGroup.GetIPAddress().WithInterfaceIndex(scopeID)).IsOK(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "Added multicast UDP socket for scope " UINT32_FORMAT_SPEC "\n", scopeID);
            (void) udpSocks.Put(udpSock, scopeID);
         }
         else LogTime(MUSCLE_LOG_ERROR, "Unable to add the UDP socket for scope " UINT32_FORMAT_SPEC " to multicast group %s! [%s]\n", scopeID, multicastGroup.GetIPAddress().ToString()(), ret());
      }
      else LogTime(MUSCLE_LOG_ERROR, "Unable to bind the UDP socket for scope " UINT32_FORMAT_SPEC "! [%s]\n", scopeID, ret());
   }

#ifdef WIN32
   const int pid = (int)GetCurrentProcessId();
#else
   const int pid = (int) getpid();
#endif
   LogTime(MUSCLE_LOG_INFO, "Multicast event loop begins -- this is process #%i.\n", pid);

   uint64 nextPingTime = GetRunTime64();
   SocketMultiplexer sm;
   while(true)
   {
      for (HashtableIterator<ConstSocketRef, int> iter(udpSocks); iter.HasData(); iter++) sm.RegisterSocketForReadReady(iter.GetKey().GetFileDescriptor());
      (void) sm.WaitForEvents(nextPingTime);

      const uint64 now = GetRunTime64();
      if (now >= nextPingTime)
      {
         const String pingData = String("Hi guys, from process #%1 at time %2").Arg(pid).Arg(now);
         for (HashtableIterator<ConstSocketRef, int> iter(udpSocks); iter.HasData(); iter++)
         {
            const IPAddress destAddr =  multicastGroup.GetIPAddress().WithInterfaceIndex(iter.GetValue());
            const int numBytesSent = SendDataUDP(iter.GetKey(), pingData(), pingData.FlattenedSize(), true, destAddr, multicastGroup.GetPort());
            if (numBytesSent == pingData.FlattenedSize()) 
            {
               LogTime(MUSCLE_LOG_INFO, "Sent %i-byte multicast packet to [%s] on socket #%i: [%s]\n", numBytesSent, destAddr.ToString()(), iter.GetKey().GetFileDescriptor(), pingData());
            }
            else LogTime(MUSCLE_LOG_ERROR, "Error sending multicast ping to socket %i\n", iter.GetKey().GetFileDescriptor());
         }

         nextPingTime += SecondsToMicros(5);
      }

      for (HashtableIterator<ConstSocketRef, int> iter(udpSocks); iter.HasData(); iter++)
      {
         if (sm.IsSocketReadyForRead(iter.GetKey().GetFileDescriptor()))
         {
            char recvBuf[1024];
            int numBytesReceived;
            IPAddress sourceIP;
            uint16 sourcePort;
            if ((numBytesReceived = ReceiveDataUDP(iter.GetKey(), recvBuf, sizeof(recvBuf)-1, true, &sourceIP, &sourcePort)) >= 0)
            {
               const IPAddressAndPort fromIAP(sourceIP, sourcePort);  // just for convenience

               recvBuf[numBytesReceived] = '\0';  // paranoia -- ensure NUL termination
               LogTime(MUSCLE_LOG_INFO, "Received %i bytes of data from [%s]:  [%s]\n", numBytesReceived, fromIAP.ToString()(), recvBuf);
            }
         }
      }
   }

   return 0;
}
