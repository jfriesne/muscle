#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "util/SocketMultiplexer.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using DataIO objects to multiplex TCP connections with stdin input.\n");
   printf("\n");
   printf("The program will listen for incoming TCP connections on port 9999, and print any data they\n");
   printf("send to us stdout.  It will also allow you to enter input on stdin, and anything you type will\n");
   printf("be sent out to all connected TCP clients.\n");
   printf("\n");
   printf("Note that this program uses SocketMultiplexer (i.e. select()) to multiplex stdin with the\n");
   printf("TCP socket I/O, which is supposed to be impossible under Windows.  StdinDataIO makes it\n");
   printf("work under Windows anyway, via clever magic.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const int tcpPort = 9999;
   ConstSocketRef acceptSock = CreateAcceptingSocket(tcpPort);
   if (acceptSock() == NULL)
   {
      printf("Unable to bind to port %i, aborting!\n", tcpPort);
      return 10;
   }

   StdinDataIO stdinIO(false);  // false == non-blocking I/O for stdin

   Hashtable<DataIORef, Void> tcpClients;
   SocketMultiplexer sm;

   printf("\n");
   printf("Listening for incoming TCP connections on port %i.\n", tcpPort);
   printf("telnet to that port in one or more other Terminal windows to connect.\n");
   printf("Also you can enter input into stdin here to send it to all connected TCP clients.\n");
   printf("\n");
 
   while(true)
   {
      // Tell the SocketMultiplexer what sockets to listen to
      sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      sm.RegisterSocketForReadReady(acceptSock.GetFileDescriptor());
      for (HashtableIterator<DataIORef, Void> iter(tcpClients); iter.HasData(); iter++)
      {
         sm.RegisterSocketForReadReady(iter.GetKey()()->GetReadSelectSocket().GetFileDescriptor());
      }

      // Wait here until something happens
      (void) sm.WaitForEvents();

      // Time to accept an incoming TCP connection?
      if (sm.IsSocketReadyForRead(acceptSock.GetFileDescriptor()))
      {
         IPAddress clientIP;
         ConstSocketRef tcpSock = Accept(acceptSock, &clientIP);
         if (tcpSock())
         {
            DataIORef newDataIORef(new TCPSocketDataIO(tcpSock, false));
            printf("Accepted new TCP connection %p from [%s]\n", newDataIORef(), clientIP.ToString()()); 
            (void) tcpClients.PutWithDefault(newDataIORef);
         }
         else printf("Accept failed!?\n");
      }

      // Time to read from stdin?
      if (sm.IsSocketReadyForRead(stdinIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         char inputBuf[1024];
         const int numBytesRead = stdinIO.Read(inputBuf, sizeof(inputBuf));
         if (numBytesRead >= 0)
         {
            printf("Read %i bytes from stdin, forwarding them to " UINT32_FORMAT_SPEC " TCP clients.\n", numBytesRead, tcpClients.GetNumItems());
            for (HashtableIterator<DataIORef, Void> iter(tcpClients); iter.HasData(); iter++)
            {
               const int numBytesWritten = iter.GetKey()()->Write(inputBuf, numBytesRead);
               if (numBytesWritten != numBytesRead)
               {
                  printf("Error writing to TCP client %p\n", iter.GetKey()()); 
               }
            }
         }
         else break;  // EOF on stdin; time to go away
      }

      // Time to read from a TCP client?
      for (HashtableIterator<DataIORef, Void> iter(tcpClients); iter.HasData(); iter++)
      {
         DataIO * clientIO = iter.GetKey()();
         if (sm.IsSocketReadyForRead(clientIO->GetReadSelectSocket().GetFileDescriptor()))
         {
            char inputBuf[1024];
            const int numBytesRead = clientIO->Read(inputBuf, sizeof(inputBuf)-1);
            if (numBytesRead >= 0)
            {
               inputBuf[numBytesRead] = '\0';  // ensure NUL termination 
               printf("TCP client %p:  sent this to me: [%s]\n", clientIO, String(inputBuf).Trim()());
            }
            else 
            {
               printf("TCP client %p closed his connection to the server.\n", clientIO);
               (void) tcpClients.Remove(iter.GetKey());  // buh-bye
            }
         }
      }
   }

   printf("Program exiting.\n");
   return 0;
}
