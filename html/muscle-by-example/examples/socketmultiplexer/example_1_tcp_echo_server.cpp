#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Socket.h"
#include "util/SocketMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateAcceptingSocket(), etc
#include "util/TimeUtilityFunctions.h"     // for GetHumanReadableTimeIntervalString()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates the use of a SocketMultiplexer object to allow a single\n");
   printf("thread to handle multiple Sockets simultaneously.  This program will listen for\n");
   printf("incoming TCP connections on port 9999, and will echo any data received on a given\n");
   printf("TCP connection back to its own connecting client program.\n");
   printf("\n");
   printf("Test this program by running it in one Terminal window, and doing a 'telnet localhost 9999'\n");
   printf("in one or more other Terminal windows.  Each telnet session should see its own data echoed\n");
   printf("back to it.\n");
   printf("\n");
   printf("Note that for simplicity's sake, this program is programmed to use blocking I/O.\n");
   printf("A production-grade server would likely use non-blocking I/O instead, so that one\n");
   printf("slow or malfunctioning client wouldn't be able to block the server's event-loop\n");
   printf("and therefore deny service to all the other clients.  (Handling non-blocking I/O\n");
   printf("correctly is beyond the scope of this example, however)\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int TCP_PORT = 9999;

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

   SocketMultiplexer socketMux;
   Hashtable<ConstSocketRef, Void> connectedClients;  // our set of TCP sockets representing currently-connected clients
   while(true)
   {
      // Register our acceptSock so we'll know if a new TCP connection comes in
      (void) socketMux.RegisterSocketForReadReady(acceptSock.GetFileDescriptor());

      // Register our client-sockets so we'll know if any of them send us data
      for (HashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
      {
         (void) socketMux.RegisterSocketForReadReady(iter.GetKey().GetFileDescriptor());
      }

      // Block here until there is something to do
      printf("Blocking in WaitForEvents() until there is something to do... (" UINT32_FORMAT_SPEC " clients currently connected)\n", connectedClients.GetNumItems());
      const uint64 nowBeforeWaitMicros = GetRunTime64();
      (void) socketMux.WaitForEvents();
      const uint64 nowAfterWaitMicros = GetRunTime64();
      printf("WaitForEvents() returned after %s\n", GetHumanReadableTimeIntervalString(nowAfterWaitMicros-nowBeforeWaitMicros)());
 
      // See if any new TCP connection requests have come in
      if (socketMux.IsSocketReadyForRead(acceptSock.GetFileDescriptor()))
      {
         printf("SocketMultiplexer thinks that the acceptSock is ready-for-read now!\n");

         ConstSocketRef newClientSock = Accept(acceptSock);
         if (newClientSock())
         {
            printf("Accepted new incoming TCP connection, socket is %p, file descriptor %i\n", newClientSock(), newClientSock.GetFileDescriptor());
            (void) connectedClients.PutWithDefault(newClientSock);
         }
         else printf("Error, Accept() failed!\n");
      }

      // See if any of our existing TCP sockets have any data for us
      for (HashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
      {
         const ConstSocketRef & clientSock = iter.GetKey();
         if (socketMux.IsSocketReadyForRead(clientSock.GetFileDescriptor()))
         {
            printf("Socket %p (file descriptor %i) reports ready-for-read...\n", clientSock(), clientSock.GetFileDescriptor());

            uint8 tempBuf[1024];
            const int numBytesRead = ReceiveData(clientSock, tempBuf, sizeof(tempBuf), true);  // true because we're using blocking I/O
            if (numBytesRead >= 0)   // Note that unlike recv(), ReceiveData() returning 0 doesn't mean connection-closed
            {
               printf("Read %i bytes from socket %i, echoing them back...\n", numBytesRead, clientSock.GetFileDescriptor());
               const int numBytesWritten = SendData(clientSock, tempBuf, numBytesRead, true);   // true because we're using blocking I/O
               printf("Wrote %i/%i bytes back to socket %i\n", numBytesWritten, numBytesRead, clientSock.GetFileDescriptor());
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
