#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/Socket.h"
#include "util/SocketMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"  // for CreateAcceptingSocket(), etc
#include "util/TimeUtilityFunctions.h"     // for GetHumanReadableUnsignedTimeIntervalString()

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program is the same as the example_1_tcp_echo_server program, except\n");
   p.printf("that it also wakes up once every 2 seconds to increment a timer value.\n");
   p.printf("Its purpose is just to demonstrate how you can use the (timeoutAtTime)\n");
   p.printf("argument of the SocketMultiplexer::WaitForEvents() method to efficiently\n");
   p.printf("handle both I/O-driven events and time-driven events in a single thread.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

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
      (void) socketMux.RegisterSocketForReadReady(acceptSock.GetFileDescriptor());

      // Register our client-sockets so we'll know if any of them send us data
      for (ConstHashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
      {
         (void) socketMux.RegisterSocketForReadReady(iter.GetKey().GetFileDescriptor());
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
      for (ConstHashtableIterator<ConstSocketRef, Void> iter(connectedClients); iter.HasData(); iter++)
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
                                      else printf("Error [%s] while writing %i bytes back to socket %i\n", numBytesWritten.GetStatus()(), numBytesRead.GetByteCount(), clientSock.GetFileDescriptor());
            }
            else
            {
               printf("ReceiveData() returned %i, closing connection!\n", numBytesRead.GetByteCount());
               (void) connectedClients.Remove(clientSock);  // yes, that's all!  close() will be called on the file descriptor automatically
            }
         }
      }
   }

   return 0;
}
