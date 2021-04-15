#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "util/SocketMultiplexer.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates using a ChildProcessDataIO object to launch a child\n");
   printf("process and then communicate with it by writing data to its stdin and reading\n");
   printf("data from its stdout.\n");
   printf("\n");
   printf("This program will launch the \"example_2_tcp_server\" program as a child\n");
   printf("process, and then let you interact with it in the usual way.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

#ifdef WIN32
   const String childExeName = ".\\example_2_tcp_server.exe";
#else
   const String childExeName = "./example_2_tcp_server";
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   Queue<String> childArgv; (void) childArgv.AddTail(childExeName);  // support for pre-C++11 compilers
#else
   Queue<String> childArgv = {childExeName};
#endif
   status_t ret;
   ChildProcessDataIO cpIO(false);  // false == non-blocking
   if (cpIO.LaunchChildProcess(childArgv).IsError(ret))
   {
      printf("Unable to launch child process!  Perhaps the example2_tcp_server executable isn't in the current directory, or doesn't have execute permission set? [%s]\n", ret());
      return 10;
   }

   StdinDataIO stdinIO(false);  // false == non-blocking I/O for stdin
   SocketMultiplexer sm;
   while(true)
   {
      // Tell the SocketMultiplexer what sockets to listen to
      sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      sm.RegisterSocketForReadReady(cpIO.GetReadSelectSocket().GetFileDescriptor());

      // Wait here until something happens
      (void) sm.WaitForEvents();

      // Time to read from stdin?
      if (sm.IsSocketReadyForRead(stdinIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         char inputBuf[1024];
         const int numBytesRead = stdinIO.Read(inputBuf, sizeof(inputBuf));
         if (numBytesRead >= 0)
         {
            printf("Read %i bytes from stdin, forwarding them to the child process.\n", numBytesRead);
            const int numBytesWritten = cpIO.Write(inputBuf, numBytesRead);
            if (numBytesWritten != numBytesRead)
            {
               printf("Error writing to the child process!\n");
            }
         }
         else break;  // EOF on stdin; time to go away
      }

      // Time to read from the child process's stdout?
      if (sm.IsSocketReadyForRead(cpIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         char inputBuf[1024];
         const int numBytesRead = cpIO.Read(inputBuf, sizeof(inputBuf)-1);
         if (numBytesRead >= 0)
         {
            inputBuf[numBytesRead] = '\0';  // ensure NUL termination 
            printf("Child Process sent this to me: [%s]\n", String(inputBuf).Trim()());
         }
         else 
         {
            printf("Child process has exited!\n");
            break;
         }
      }
   }

   printf("Program exiting.\n");
   return 0;
}
