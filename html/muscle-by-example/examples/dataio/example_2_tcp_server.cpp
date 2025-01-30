#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/StdinDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "util/SocketMultiplexer.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates using DataIO objects to multiplex TCP connections with stdin input.\n");
   p.printf("\n");
   p.printf("The program will listen for incoming TCP connections on port 9999, and print any data they\n");
   p.printf("send to us stdout.  It will also allow you to enter input on stdin, and anything you type will\n");
   p.printf("be sent out to all connected TCP clients.\n");
   p.printf("\n");
   p.printf("Note that this program uses SocketMultiplexer (i.e. select()) to multiplex stdin with the\n");
   p.printf("TCP socket I/O, which is supposed to be impossible under Windows.  StdinDataIO makes it\n");
   p.printf("work under Windows anyway, via clever magic.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

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
   printf("Enter quit to quit, or press Ctrl-D.\n");
   printf("\n");

   while(true)
   {
      // Tell the SocketMultiplexer what sockets to listen to
      (void) sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      (void) sm.RegisterSocketForReadReady(acceptSock.GetFileDescriptor());
      for (HashtableIterator<DataIORef, Void> iter(tcpClients); iter.HasData(); iter++)
      {
         (void) sm.RegisterSocketForReadReady(iter.GetKey()()->GetReadSelectSocket().GetFileDescriptor());
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
         const io_status_t readRet = stdinIO.Read(inputBuf, sizeof(inputBuf));
         if (readRet.IsOK())
         {
            if ((readRet.GetByteCount() >= 5)&&(strncmp(inputBuf, "quit", 4) == 0)&&((inputBuf[4] == '\r')||(inputBuf[4] == '\n')))
            {
               printf("You entered quit, exiting!\n");
               break;
            }
            else
            {
               printf("Read %i bytes from stdin, forwarding them to " UINT32_FORMAT_SPEC " TCP clients.\n", readRet.GetByteCount(), tcpClients.GetNumItems());
               for (HashtableIterator<DataIORef, Void> iter(tcpClients); iter.HasData(); iter++)
               {
                  const io_status_t writeRet = iter.GetKey()()->Write(inputBuf, readRet.GetByteCount());
                  if (writeRet != readRet) printf("Error [%s] writing to TCP client %p\n", writeRet.GetStatus()(), iter.GetKey()());
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
            const io_status_t readRet = clientIO->Read(inputBuf, sizeof(inputBuf)-1);
            if (readRet.IsOK())
            {
               inputBuf[readRet.GetByteCount()] = '\0';  // ensure NUL termination
               printf("TCP client %p:  sent this to me: [%s]\n", clientIO, String(inputBuf).Trimmed()());
            }
            else
            {
               printf("TCP client %p closed his connection to the server. [%s]\n", clientIO, readRet.GetStatus()());
               (void) tcpClients.Remove(iter.GetKey());  // buh-bye
            }
         }
      }
   }

   printf("Program exiting.\n");
   return 6;  // returning 6 just for the benefit of example_6_child_process.cpp
}
