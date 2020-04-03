#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "reflector/SignalHandlerSession.h"  // for SetMainReflectServerCatchSignals()
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"smart\" Message server that catches SIGINT\n");
   printf("and other signals and initiates a controlled shutdown.\n");
   printf("It's identical to the reflector/example_4_smart_server.cpp example,\n");
   printf("except this version calls SetMainReflectServerCatchSignals(true)\n");
   printf("before starting the ServerProcessLoop().\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the server is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   // This object contains our server's event loop.
   ReflectServer reflectServer;

   // This factory will create a StorageReflectSession object whenever
   // a TCP connection is received on SMART_SERVER_TCP_PORT, and
   // attach the StorageReflectSession to the ReflectServer for use.   
   StorageReflectSessionFactory smartSessionFactory;
   status_t ret;
   if (reflectServer.PutAcceptFactory(SMART_SERVER_TCP_PORT, ReflectSessionFactoryRef(&smartSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?) [%s]\n", SMART_SERVER_TCP_PORT, ret());
      return 5;
   }

   // This is the big one-line difference right here!
   SetMainReflectServerCatchSignals(true);

   LogTime(MUSCLE_LOG_INFO, "example_1_basic_usage is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try pressing Control-C (or doing a \"kill -s SIGINT this_process_id\" in another Terminal) to see this process do a controlled shutdown.\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_1_basic_usage is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_1_basic_usage is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
