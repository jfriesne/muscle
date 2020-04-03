#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/DetectNetworkConfigChangesSession.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"smart\" Message server that uses\n");
   printf("a DetectNetworkConfigChangesSession to detect when the network\n");
   printf("configuration has changed, or when the computer is about to go\n");
   printf("to sleep (or has just woken up).\n");
   printf("\n");
   printf("It's otherwise identical to the reflector/example_4_smart_server.cpp example.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

class MyDetectNetworkConfigChangesSession : public DetectNetworkConfigChangesSession
{
public:
   MyDetectNetworkConfigChangesSession() {/* empty */}

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & interfaceNames)
   {
      String s;
      if (interfaceNames.HasItems())
      {
         s = " on these interfaces: ";
         for (HashtableIterator<String, Void> iter(interfaceNames); iter.HasData(); iter++) s += iter.GetKey().Prepend(' ');
      }
      LogTime(MUSCLE_LOG_INFO, "MyDetectNetworkConfigChangesSession:  Network configuration change detected%s\n", s());
   }

   virtual void ComputerIsAboutToSleep()
   {
      LogTime(MUSCLE_LOG_INFO, "MyDetectNetworkConfigChangesSession:  This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      LogTime(MUSCLE_LOG_INFO, "MyDetectNetworkConfigChangesSession:  This computer just re-awoke from sleep!\n");
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

   MyDetectNetworkConfigChangesSession myDetectSession;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&myDetectSession, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't add MyDetectNetworkConfigChangesSession, aborting! [%s]\n", ret());
      return 10;
   }

   LogTime(MUSCLE_LOG_INFO, "example_1_basic_usage is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try modifying your computer's Network Settings, or putting your computer to sleep!\n");
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
