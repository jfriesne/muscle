/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x) != B_NO_ERROR) printf("Test failed, line %i\n",__LINE__)

static void PrintUsageAndExit()
{
   LogTime(MUSCLE_LOG_INFO, "Usage:  ./testchildprocess <count> <cmd> [args]\n");
   exit(10);
}

// This program is equivalent to the portableplaintext client, except
// that we communicate with a child process instead of a socket.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc < 3) PrintUsageAndExit();

   uint32 numProcesses = atol(argv[1]);
   if (numProcesses == 0) PrintUsageAndExit();

   const char * cmd = argv[2];

   Queue<DataIORef> refs;
   for (uint32 i=0; i<numProcesses; i++)
   {
      ChildProcessDataIO * dio = new ChildProcessDataIO(false);
      refs.AddTail(DataIORef(dio));
      printf("About To Launch child process #" UINT32_FORMAT_SPEC":  [%s]\n", i+1, cmd); fflush(stdout);
      ConstSocketRef s = (dio->LaunchChildProcess(argc-2, ((const char **) argv)+2) == B_NO_ERROR) ? dio->GetReadSelectSocket() : ConstSocketRef();
      printf("Finished Launching child process #" UINT32_FORMAT_SPEC":  [%s]\n", i+1, cmd); fflush(stdout);
      if (s() == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error launching child process #" UINT32_FORMAT_SPEC" [%s]!\n", i+1, cmd);
         return 10;
      }
   }

   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway;
   QueueGatewayMessageReceiver stdinInputQueue;
   stdinGateway.SetDataIO(DataIORef(&stdinIO, false));

   SocketMultiplexer multiplexer;

   for (uint32 i=0; i<refs.GetNumItems(); i++)
   {
      printf("------------ CHILD PROCESS #" UINT32_FORMAT_SPEC" ------------------\n", i+1);
      PlainTextMessageIOGateway ioGateway;
      ioGateway.SetDataIO(refs[i]);
      ConstSocketRef readSock = refs[i]()->GetReadSelectSocket();
      QueueGatewayMessageReceiver ioInputQueue;
      while(1)
      {
         int readFD = readSock.GetFileDescriptor();
         multiplexer.RegisterSocketForReadReady(readFD);

         int writeFD = ioGateway.HasBytesToOutput() ? refs[i]()->GetWriteSelectSocket().GetFileDescriptor() : -1;
         if (writeFD >= 0) multiplexer.RegisterSocketForWriteReady(writeFD);
         int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();
         multiplexer.RegisterSocketForReadReady(stdinFD);
         if (multiplexer.WaitForEvents() < 0) printf("testchildprocess: WaitForEvents() failed!\n");

         // First, deliver any lines of text from stdin to the child process
         if ((multiplexer.IsSocketReadyForRead(stdinFD))&&(stdinGateway.DoInput(ioGateway) < 0))
         {
            printf("Error reading from stdin, aborting!\n");
            break;
         }

         bool reading    = multiplexer.IsSocketReadyForRead(readFD);
         bool writing    = ((writeFD >= 0)&&(multiplexer.IsSocketReadyForWrite(writeFD)));
         bool writeError = ((writing)&&(ioGateway.DoOutput() < 0));
         bool readError  = ((reading)&&(ioGateway.DoInput(ioInputQueue) < 0));
         if ((readError)||(writeError))
         {
            printf("Connection closed, exiting.\n");
            break;
         }

         MessageRef incoming;
         while(ioInputQueue.RemoveHead(incoming) == B_NO_ERROR)
         {
            printf("Heard message from server:-----------------------------------\n");
            const char * inStr;
            for (int i=0; (incoming()->FindString(PR_NAME_TEXT_LINE, i, &inStr) == B_NO_ERROR); i++) printf("Line %i: [%s]\n", i, inStr);

            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         multiplexer.RegisterSocketForReadReady(readFD);
         if (ioGateway.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(writeFD);
      }

      if (ioGateway.HasBytesToOutput())
      {
         printf("Waiting for all pending messages to be sent...\n");
         while((ioGateway.HasBytesToOutput())&&(ioGateway.DoOutput() >= 0)) {printf ("."); fflush(stdout);}
      }
   }
   printf("\n\nBye!\n");
   return 0;
}
