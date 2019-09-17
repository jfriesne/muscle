#include "reflector/DumbReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"dumb\" Message server.  All this server will\n");
   printf("do is take any Messages sent to it from any client and forward them to\n");
   printf("all of the other clients.  This program is designed to be run in conjunction\n");
   printf("with multiple instances of the example_2_dumb_client example program.\n");
   printf("\n");
}

static const uint16 DUMB_SERVER_TCP_PORT = 8765;  // arbitrary port number for the "dumb" server

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the server is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   // This object contains our server's event loop.
   ReflectServer reflectServer;

   // This factory will create a DumbReflectSession object whenever
   // a TCP connection is received on DUMB_SERVER_TCP_PORT, and
   // attach the DumbReflectSession to the ReflectServer for use.   
   status_t ret;
   DumbReflectSessionFactory dumbSessionFactory;
   if (reflectServer.PutAcceptFactory(DUMB_SERVER_TCP_PORT, ReflectSessionFactoryRef(&dumbSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?)  [%s]\n", DUMB_SERVER_TCP_PORT, ret());
      return 5;
   }

   LogTime(MUSCLE_LOG_INFO, "example_1_dumb_server is listening for incoming TCP connections on port %u\n", DUMB_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of example_2_dumb_client to connect and chat!\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_1_dumb_server is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_1_dumb_server is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
