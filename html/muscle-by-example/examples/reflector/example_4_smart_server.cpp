#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"smart\" Message server.  This server implements\n");
   printf("the standard MUSCLE StorageReflectSession features (it's quite similar to \n");
   printf("the standard muscled; the main difference is that it doesn't accept any\n");
   printf("command line options, for simplicity)\n");
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
   status_t ret;
   StorageReflectSessionFactory smartSessionFactory;
   if (reflectServer.PutAcceptFactory(SMART_SERVER_TCP_PORT, ReflectSessionFactoryRef(&smartSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?) [%s]\n", SMART_SERVER_TCP_PORT, ret());
      return 5;
   }

   LogTime(MUSCLE_LOG_INFO, "example_4_smart_server is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of example_5_smart_client to connect/chat/subscribe!\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_4_smart_server is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_4_smart_server is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
