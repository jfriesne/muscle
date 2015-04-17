#include "system/DetectNetworkConfigChangesSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"

using namespace muscle;

class TestSession : public DetectNetworkConfigChangesSession
{
public:
   TestSession() {/* empty */}

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & interfaceNames)
   {
      String s;
      if (interfaceNames.HasItems())
      {
         s = " on these interfaces: ";
         for (HashtableIterator<String, Void> iter(interfaceNames); iter.HasData(); iter++) s += iter.GetKey().Prepend(' ');
      }
      LogTime(MUSCLE_LOG_INFO, "Network configuration change detected%s\n", s());
   }

   virtual void ComputerIsAboutToSleep()
   {
      LogTime(MUSCLE_LOG_INFO, "This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      LogTime(MUSCLE_LOG_INFO, "This computer just re-awoke from sleep!\n");
   }
};

int main(int /*argc*/, char ** /*argv*/)
{
   CompleteSetupSystem css;  // set up our environment

   ReflectServer server;
   TestSession session;

   if (server.AddNewSession(AbstractReflectSessionRef(&session, false)) == B_NO_ERROR)
   {
      LogTime(MUSCLE_LOG_INFO, "Beginning Network-Configuration-Change-Detector test... try changing your network config, or plugging/unplugging an Ethernet cable.\n");
      if (server.ServerProcessLoop() == B_NO_ERROR) LogTime(MUSCLE_LOG_INFO, "testnetconfigdetect event loop exiting.\n");
                                               else LogTime(MUSCLE_LOG_CRITICALERROR, "testnetconfigdetect event loop exiting with an error condition.\n");
   }
   server.Cleanup();

   return 0;
}
