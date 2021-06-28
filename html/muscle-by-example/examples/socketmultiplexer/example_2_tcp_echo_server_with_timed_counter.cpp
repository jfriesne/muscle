#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Socket.h"
#include "util/SocketMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateAcceptingSocket(), etc
#include "util/TimeUtilityFunctions.h"     // for GetHumanReadableTimeIntervalString()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program is the same as the example_1_tcp_echo_server program, except\n");
   printf("that it also wakes up once every 2 seconds to increment a timer value.\n");
   printf("Its purpose is just to demonstrate how you can use the (timeoutAtTime)\n");
   printf("argument of the SocketMultiplexer::WaitForEvents() method to efficiently\n");
   printf("handle both I/O-driven events and time-driven events in a single thread.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int TCP_PORT                 = 9999;
   const uint64 TIMER_INTERVAL_MICROS = SecondsToMicros(2);

   ConstSocketRef acceptSock = CreateAcceptingSocket(TCP_PORT);
   if (acceptSock())
   {
      printf("Now accepting TCP connections on port %i.  Try running some 'telnet localhost %i' sessions in other Terminal windows\n", TCP_PORT, TCP_PORT);
   }
   else
   {
      printf("Error binding to port %i!  Perhaps another instance of this program is still running somewhere?\n", TCP_PORT);
      return 10;
   }

   uint32 counter = 0;
   uint64 nextCounterIncrementTime = GetRunTime64() + TIMER_INTERVAL_MICROS;

   SocketMultiplexer socketMux;
   Hashtable<ConstSocketRef, Void> connectedClients;  // our set of TCP sockets representing currently-connected clients
   while(true)
   {
      // Register our acceptSock so we'll know if a new TCP connection comes in
      (void) socketMux.RegisterSocketForReadReady(acceptSock.GetSocketDescriptor());

      // Register our client-sockets so we'll know if any of them send us data
      for (HashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
      {
         (void) socketMux.RegisterSocketForReadReady(iter.GetKey().GetSocketDescriptor());
      }

      // Block here until there is something to do
      printf("Blocking in WaitForEvents() until there is something to do... (" UINT32_FORMAT_SPEC " clients currently connected)\n", connectedClients.GetNumItems());
      (void) socketMux.WaitForEvents(nextCounterIncrementTime);  // wait until I/O, *or* until (nextCounterIncrementTime)
      const uint64 nowAfterWaitMicros = GetRunTime64();
 
      if (nowAfterWaitMicros >= nextCounterIncrementTime)
      {
         nextCounterIncrementTime += TIMER_INTERVAL_MICROS;
         counter++;

         printf("INCREMENTED COUNTER TO " UINT32_FORMAT_SPEC "\n", counter);
      }

      // See if any new TCP connection requests have come in
      if (socketMux.IsSocketReadyForRead(acceptSock.GetSocketDescriptor()))
      {
         printf("SocketMultiplexer thinks that the acceptSock is ready-for-read now!\n");

         ConstSocketRef newClientSock = Accept(acceptSock);
         if (newClientSock())
         {
            printf("Accepted new incoming TCP connection, socket is %p, socket descriptor " SOCKET_FORMAT_SPEC "\n", newClientSock(), newClientSock.GetSocketDescriptor());
            (void) connectedClients.PutWithDefault(newClientSock);
         }
         else printf("Error, Accept() failed!\n");
      }

      // See if any of our existing TCP sockets have any data for us
      for (HashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
      {
         const ConstSocketRef & clientSock = iter.GetKey();
         if (socketMux.IsSocketReadyForRead(clientSock.GetSocketDescriptor()))
         {
            printf("Socket %p (socket descriptor " SOCKET_FORMAT_SPEC ") reports ready-for-read...\n", clientSock(), clientSock.GetSocketDescriptor());

            uint8 tempBuf[1024];
            const int numBytesRead = ReceiveData(clientSock, tempBuf, sizeof(tempBuf), true);  // true because we're using blocking I/O
            if (numBytesRead >= 0)   // Note that unlike recv(), ReceiveData() returning 0 doesn't mean connection-closed
            {
               printf("Read %i bytes from socket " SOCKET_FORMAT_SPEC ", echoing them back...\n", numBytesRead, clientSock.GetSocketDescriptor());
               const int numBytesWritten = SendData(clientSock, tempBuf, numBytesRead, true);   // true because we're using blocking I/O
               printf("Wrote %i/%i bytes back to socket " SOCKET_FORMAT_SPEC "\n", numBytesWritten, numBytesRead, clientSock.GetSocketDescriptor());
            }
            else
            {
               printf("ReceiveData() returned %i, closing connection!\n", numBytesRead);
               (void) connectedClients.Remove(clientSock);  // yes, that's all!  close() will be called on the file descriptor automatically
            }
         }
      }
   }

   return 0;
}
