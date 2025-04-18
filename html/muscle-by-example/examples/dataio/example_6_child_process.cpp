#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "util/SocketMultiplexer.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription(const OutputPrinter & p)
{
   p.printf("\n");
   p.printf("This program demonstrates using a ChildProcessDataIO object to launch a child\n");
   p.printf("process and then communicate with it by writing data to its stdin and reading\n");
   p.printf("data from its stdout.\n");
   p.printf("\n");
   p.printf("This program will launch the \"dataio_example_2_tcp_server\" program as a child\n");
   p.printf("process, and then let you interact with it in the usual way.\n");
   p.printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription(stdout);

#ifdef WIN32
   const String childExeName = ".\\dataio_example_2_tcp_server.exe";
#else
   const String childExeName = "./dataio_example_2_tcp_server";
#endif

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   Queue<String> childArgv; (void) childArgv.AddTail(childExeName);  // support for pre-C++11 compilers
#else
   Queue<String> childArgv = {std_move_if_available(childExeName)};
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
      (void) sm.RegisterSocketForReadReady(stdinIO.GetReadSelectSocket().GetFileDescriptor());
      (void) sm.RegisterSocketForReadReady(cpIO.GetReadSelectSocket().GetFileDescriptor());

      // Wait here until something happens
      (void) sm.WaitForEvents();

      // Time to read from stdin?
      if (sm.IsSocketReadyForRead(stdinIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         char inputBuf[1024];
         const io_status_t numBytesRead = stdinIO.Read(inputBuf, sizeof(inputBuf));
         if (numBytesRead.IsOK())
         {
            printf("Read %i bytes from stdin, forwarding them to the child process.\n", numBytesRead.GetByteCount());
            const io_status_t numBytesWritten = cpIO.Write(inputBuf, numBytesRead.GetByteCount());
            if (numBytesWritten != numBytesRead)
            {
               printf("Error writing to the child process! [%s]\n", numBytesWritten.GetStatus()());
            }
         }
         else break;  // EOF on stdin; time to go away
      }

      // Time to read from the child process's stdout?
      if (sm.IsSocketReadyForRead(cpIO.GetReadSelectSocket().GetFileDescriptor()))
      {
         char inputBuf[1024];
         const io_status_t numBytesRead = cpIO.Read(inputBuf, sizeof(inputBuf)-1);
         if (numBytesRead.IsOK())
         {
            inputBuf[numBytesRead.GetByteCount()] = '\0';  // ensure NUL termination
            printf("Child Process sent this to me: [%s]\n", String(inputBuf).Trimmed()());
         }
         else
         {
            printf("Child process has exited! [%s]\n", numBytesRead.GetStatus()());
            break;
         }
      }
   }

   cpIO.Shutdown();  // only necessary so we can call GetChildProcessExitCode() and get a meaningful value

   printf("Program exiting (child process exit code was %s).\n", cpIO.GetChildProcessExitCode()());
   return 0;
}
