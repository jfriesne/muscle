#include "system/DetectNetworkConfigChangesSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"

using namespace muscle;

class SomeOtherSession : public AbstractReflectSession, public INetworkConfigChangesTarget
{
public:
   SomeOtherSession () {/* empty */}

   virtual void MessageReceivedFromGateway(const MessageRef &, void *) {/* empty */}

   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & interfaceNames)
   {
      String s;
      if (interfaceNames.HasItems())
      {
         s = " on these interfaces: ";
         for (ConstHashtableIterator<String, Void> iter(interfaceNames); iter.HasData(); iter++) s += iter.GetKey().WithPrepend(' ');
      }
      LogTime(MUSCLE_LOG_INFO, "SomeOtherSession:  Network configuration change detected%s\n", s());
   }

   virtual void ComputerIsAboutToSleep()
   {
      LogTime(MUSCLE_LOG_INFO, "SomeOtherSession:  This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      LogTime(MUSCLE_LOG_INFO, "SomeOtherSession:  This computer just re-awoke from sleep!\n");
   }
};

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
         for (ConstHashtableIterator<String, Void> iter(interfaceNames); iter.HasData(); iter++) s += iter.GetKey().WithPrepend(' ');
      }
      LogTime(MUSCLE_LOG_INFO, "TestSession:  Network configuration change detected%s\n", s());
   }

   virtual void ComputerIsAboutToSleep()
   {
      LogTime(MUSCLE_LOG_INFO, "TestSession:  This computer is about to go to sleep!\n");
   }

   virtual void ComputerJustWokeUp()
   {
      LogTime(MUSCLE_LOG_INFO, "TestSession:  This computer just re-awoke from sleep!\n");
   }
};

int main(int argc, char ** argv)
{
   if ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0))
   {
      printf("Called from script, skipping this test.\n");
      return 0;
   }

   CompleteSetupSystem css;  // set up our environment

   ReflectServer server;
   TestSession testSession;        // detects config changes and computer sleeps/wakes
   SomeOtherSession otherSession;  // just to verify that the callbacks get called on other sessions also

   status_t ret;
   if ((server.AddNewSession(DummyAbstractReflectSessionRef(testSession)).IsOK(ret))&&(server.AddNewSession(DummyAbstractReflectSessionRef(otherSession)).IsOK(ret)))
   {
      LogTime(MUSCLE_LOG_INFO, "Beginning Network-Configuration-Change-Detector test... try changing your network config, or plugging/unplugging an Ethernet cable, or putting your computer to sleep.\n");
      if (server.ServerProcessLoop().IsOK(ret)) LogTime(MUSCLE_LOG_INFO, "testnetconfigdetect event loop exiting.\n");
                                           else LogTime(MUSCLE_LOG_CRITICALERROR, "testnetconfigdetect event loop exiting with an error condition [%s].\n", ret());
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "AddNewSession() failed!  [%s]\n", ret());

   server.Cleanup();

   return ret.IsOK() ? 0 : 10;
}
