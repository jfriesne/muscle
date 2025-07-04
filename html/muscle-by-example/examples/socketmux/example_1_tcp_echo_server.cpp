#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Socket.h"
#include "util/SocketMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateAcceptingSocket(), etc
#include "util/TimeUtilityFunctions.h"     // for GetHumanReadableUnsignedTimeIntervalString()

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates the use of a SocketMultiplexer object to allow a single\n");
   p.printf("thread to handle multiple Sockets simultaneously.  This program will listen for\n");
   p.printf("incoming TCP connections on port 9999, and will echo any data received on a given\n");
   p.printf("TCP connection back to its own connecting client program.\n");
   p.printf("\n");
   p.printf("Test this program by running it in one Terminal window, and doing a 'telnet localhost 9999'\n");
   p.printf("in one or more other Terminal windows.  Each telnet session should see its own data echoed\n");
   p.printf("back to it.\n");
   p.printf("\n");
   p.printf("Note that for simplicity's sake, this program is programmed to use blocking I/O.\n");
   p.printf("A production-grade server would likely use non-blocking I/O instead, so that one\n");
   p.printf("slow or malfunctioning client wouldn't be able to block the server's event-loop\n");
   p.printf("and therefore deny service to all the other clients.  (Handling non-blocking I/O\n");
   p.printf("correctly is beyond the scope of this example, however)\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

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
      printf("WaitForEvents() returned after %s\n", GetHumanReadableUnsignedTimeIntervalString(nowAfterWaitMicros-nowBeforeWaitMicros)());

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
            const io_status_t numBytesRead = ReceiveData(clientSock, tempBuf, sizeof(tempBuf), true);  // true because we're using blocking I/O
            if (numBytesRead.IsOK())
            {
               printf("Read %i bytes from socket %i, echoing them back...\n", numBytesRead.GetByteCount(), clientSock.GetFileDescriptor());
               const io_status_t numBytesWritten = SendData(clientSock, tempBuf, numBytesRead.GetByteCount(), true);   // true because we're using blocking I/O
               if (numBytesWritten.IsOK()) printf("Wrote %i/%i bytes back to socket %i\n", numBytesWritten.GetByteCount(), numBytesRead.GetByteCount(), clientSock.GetFileDescriptor());
                                      else printf("Error [%s] writing %i bytes back to socket %i\n", numBytesWritten.GetStatus()(), numBytesRead.GetByteCount(), clientSock.GetFileDescriptor());
            }
            else
            {
               printf("ReceiveData() returned [%s], closing connection!\n", numBytesRead.GetStatus()());
               (void) connectedClients.Remove(clientSock);  // yes, that's all!  close() will be called on the file descriptor automatically
            }
         }
      }
   }

   return 0;
}
