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
   printf("and other signals and reacts simply by printing to stdout.\n");
   printf("This demonstrates how to use a SignalHandlerSession to add customized\n");
   printf("signal-handling to a ReflectServer.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

class MySignalHandlerSession : public SignalHandlerSession
{
public:
   MySignalHandlerSession() {/* empty */}

protected:
   virtual void SignalReceived(int whichSignal)
   {
      // Note that this code runs within the main thread (not within the signal handler!)
      // so you can do anything you want to here without fear of trouble
      LogTime(MUSCLE_LOG_INFO, "MySignalHandlerSession::SignalReceived(%i) was called!\n", whichSignal);
   }
};

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

   MySignalHandlerSession signalHandlerSession;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&signalHandlerSession, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Unable to add SignalHandlerSession, aborting! [%s]\n", ret());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "example_2_custom_handling is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try pressing Control-C (or doing a \"kill -s SIGINT this_process_id\" in another Terminal) to see this process react to the signal.\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_2_custom_handling is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_2_custom_handling is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
