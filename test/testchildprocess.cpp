/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Test failed, line %i\n",__LINE__)

static void PrintUsageAndExit()
{
   LogTime(MUSCLE_LOG_INFO, "Usage: ./testchildprocess <count> <cmd> [args]\n");
   LogTime(MUSCLE_LOG_INFO, "Note:  count must be between 1 and 10000.\n");
   exit(10);
}

// This class is here only to verify that we do something reasonable
// when ChildProcessReadyToRun() returns an error
class AbortOnTakeoffChildProcessDataIO : public ChildProcessDataIO
{
public:
   explicit AbortOnTakeoffChildProcessDataIO(bool blocking) : ChildProcessDataIO(blocking) {/* empty */}

   virtual status_t ChildProcessReadyToRun()
   {
      (void) ChildProcessDataIO::ChildProcessReadyToRun();

      printf("AbortOnTakeoffChildProcessDataIO::ChildProcessReadyToRun() deliberately returning B_ERROR for testing purposes.\n");
      return B_ERROR("Deliberate Error");
   }

   static void UnitTest()
   {
      printf("Testing abort-on-takeoff logic, to verify that the child process is aborted cleanly.\n");

      Queue<String> args;
      (void) args.AddTail("foobar");

      // Scope for the child process object
      {
         AbortOnTakeoffChildProcessDataIO cpdio(false);
         if (cpdio.LaunchChildProcess(args).IsOK())
         {
            printf("ChildProcessDataIO::LaunchChildProcess() succeeded!\n");

            // See what we can read from the aborted child process.  Expected behavior
            // is an immediate error due to EOF.
            SocketMultiplexer sm;
            while(1)
            {
               if (sm.RegisterSocketForReadReady(cpdio.GetReadSelectSocket().GetFileDescriptor()).IsError()) printf("RegisterSocketForReadReady() failed!\n");

               const io_status_t r = sm.WaitForEvents();
               printf("WaitForEvents() returned [%s]\n", r());

               if (sm.IsSocketReadyForRead(cpdio.GetReadSelectSocket().GetFileDescriptor()))
               {
                  printf("File descriptor is ready-for-read\n");

                  char buf[1024];
                  const int32 numBytesRead = cpdio.Read(buf, sizeof(buf)).GetByteCount();
                  printf("numBytesRead=%i\n", numBytesRead);
                  if (numBytesRead >= 0) printf("Read: [%s]\n", buf);
                                    else break;
               }
            }

            printf("Sleeping for 10 seconds...\n");
            (void) Snooze64(SecondsToMicros(10));
         }
         else printf("ChildProcessDataIO::LaunchChildProcess() failed!\n");

         printf("Calling AbortOnTakeoffChildProcessDataIO dtor\n");
      }

      printf("AbortOnTakeoffChildProcessDataIO dtor returned, sleeping 10 more seconds\n");
      (void) Snooze64(SecondsToMicros(10));
   }
};

// This program is equivalent to the portableplaintext client, except
// that we communicate with a child process instead of a socket.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   char * dummyArgv[5];  // holds fake arguments to use when we are launched from runtests.sh

   if (argc >= 2)
   {
      if (strcmp(argv[1], "abortontakeoff") == 0)
      {
         AbortOnTakeoffChildProcessDataIO::UnitTest();
         return 0;
      }
      else if (strcmp(argv[1], "fromscript") == 0)
      {
         dummyArgv[0] = argv[0];
         dummyArgv[1] = (char *) "1";
         dummyArgv[2] = (char *) "ls";
         dummyArgv[3] = (char *) "-l";
         dummyArgv[4] = NULL;

         argc = 4;
         argv = dummyArgv;
      }
   }

   if (argc < 3) PrintUsageAndExit();

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   bool doPriv = false;
   for (int i=argc-1; i>=1; i--)
   {
      if (strcmp(argv[i], "--asroot") == 0)
      {
         doPriv = true;
         for (int j=i; j<argc; j++) argv[j] = argv[j+1];  // remove the --asroot argument and shift the other back one
      }
   }
#endif

   const uint32 numProcesses = atol(argv[1]);
   if ((numProcesses == 0)||(numProcesses > 10000)) PrintUsageAndExit();

   const char * cmd = argv[2];

   Hashtable<String,String> testEnvVars;
   (void) testEnvVars.Put("Peanut Butter", "Jelly");
   (void) testEnvVars.Put("Jelly", "Peanut Butter");
   (void) testEnvVars.Put("Oranges", "Grapes");

   Queue<DataIORef> refs;
   for (uint32 i=0; i<numProcesses; i++)
   {
      ChildProcessDataIORef dio(new ChildProcessDataIO(false));
      (void) refs.AddTail(dio);

      printf("About To Launch child process #" UINT32_FORMAT_SPEC ":  [%s]\n", i+1, cmd); fflush(stdout);

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
      if (doPriv) dio()->SetRequestRootAccessForChildProcessEnabled("testchildprocess needs your password to test privilege escalation of the child process");
#endif

      status_t ret;

      ConstSocketRef s = dio()->LaunchChildProcess(argc-2, ((const char **) argv)+2, ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), NULL, &testEnvVars).IsOK(ret) ? dio()->GetReadSelectSocket() : ConstSocketRef();
      printf("Finished Launching child process #" UINT32_FORMAT_SPEC ":  [%s]\n", i+1, cmd); fflush(stdout);
      if (s() == NULL)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error launching child process #" UINT32_FORMAT_SPEC " [%s] [%s]!\n", i+1, cmd, ret());
         return 10;
      }
   }

   StdinDataIO stdinIO(false);
   PlainTextMessageIOGateway stdinGateway;
   QueueGatewayMessageReceiver stdinInputQueue;
   stdinGateway.SetDataIO(DummyDataIORef(stdinIO));

   SocketMultiplexer multiplexer;

   for (uint32 i=0; i<refs.GetNumItems(); i++)
   {
      printf("------------ CHILD PROCESS #" UINT32_FORMAT_SPEC " ------------------\n", i+1);
      PlainTextMessageIOGateway ioGateway;
      ioGateway.SetDataIO(refs[i]);
      ConstSocketRef readSock = refs[i]()->GetReadSelectSocket();
      QueueGatewayMessageReceiver ioInputQueue;
      while(1)
      {
         const int readFD = readSock.GetFileDescriptor();
         (void) multiplexer.RegisterSocketForReadReady(readFD);

         const int writeFD = ioGateway.HasBytesToOutput() ? refs[i]()->GetWriteSelectSocket().GetFileDescriptor() : -1;
         if (writeFD >= 0) (void) multiplexer.RegisterSocketForWriteReady(writeFD);

         const int stdinFD = stdinIO.GetReadSelectSocket().GetFileDescriptor();
         (void) multiplexer.RegisterSocketForReadReady(stdinFD);

         status_t ret;
         if (multiplexer.WaitForEvents().IsError(ret)) printf("testchildprocess: WaitForEvents() failed! [%s]\n", ret());

         // First, deliver any lines of text from stdin to the child process
         if ((multiplexer.IsSocketReadyForRead(stdinFD))&&(stdinGateway.DoInput(ioGateway).IsError()))
         {
            printf("Error reading from stdin, aborting!\n");
            break;
         }

         const bool reading    = multiplexer.IsSocketReadyForRead(readFD);
         const bool writing    = ((writeFD >= 0)&&(multiplexer.IsSocketReadyForWrite(writeFD)));
         const bool writeError = ((writing)&&(ioGateway.DoOutput().IsError()));
         const bool readError  = ((reading)&&(ioGateway.DoInput(ioInputQueue).IsError()));
         if ((readError)||(writeError))
         {
            printf("Connection closed, exiting.\n");
            break;
         }

         MessageRef incoming;
         while(ioInputQueue.RemoveHead(incoming).IsOK())
         {
            printf("Received output from child process:--------------------------\n");
            const char * inStr;
            for (int j=0; (incoming()->FindString(PR_NAME_TEXT_LINE, j, &inStr).IsOK()); j++) printf("Line %i: [%s]\n", j, inStr);

            printf("-------------------------------------------------------------\n");
         }

         if ((reading == false)&&(writing == false)) break;

         (void) multiplexer.RegisterSocketForReadReady(readFD);
         if (ioGateway.HasBytesToOutput()) (void) multiplexer.RegisterSocketForWriteReady(writeFD);
      }

      if (ioGateway.HasBytesToOutput())
      {
         printf("Waiting for all pending messages to be sent...\n");
         while((ioGateway.HasBytesToOutput())&&(ioGateway.DoOutput().IsOK())) {printf ("."); fflush(stdout);}
      }
   }
   printf("\n\nBye!\n");

   return 0;
}
