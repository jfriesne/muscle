#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/PulseNode.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program is the same as example_4_smart_server, except in this version\n");
   printf("our server will send a Message to each client once per second containing\n");
   printf("a counter.\n");
   printf("\n");
   printf("This is mainly just to demonstrate the use of the GetPulseTime() and Pulse()\n");
   printf("methods to have method-callbacks called at well-defined intervals.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

class TimerStorageReflectSession : public StorageReflectSession
{
public:
   TimerStorageReflectSession() : _nextTimerTime(MUSCLE_TIME_NEVER), _counter(0)
   {
      // empty
   }

   virtual status_t AttachedToServer()
   {
      MRETURN_ON_ERROR(StorageReflectSession::AttachedToServer());

      _nextTimerTime = GetRunTime64();  // now that we're attached, we'd like to get our first Pulse() callback ASAP please
      InvalidatePulseTime();  // make sure GetPulseTime() gets called again ASAP, since we've changed the value it will return

      return B_NO_ERROR;
   }

   virtual uint64 GetPulseTime(const PulseArgs & args)
   {
      // Return the time at which Pulse() should next be called.  Note that I call up to the 
      // superclass and minimize the result, just in case StorageReflectSession ever wants to
      // do its own Pulse() callbacks (currently it doesn't, but you never know what the future holds)
      return muscleMin(StorageReflectSession::GetPulseTime(args), _nextTimerTime);
   }

   virtual void Pulse(const PulseArgs & args)
   {
      StorageReflectSession::Pulse(args);

      if (args.GetCallbackTime() >= _nextTimerTime)
      {
         LogTime(MUSCLE_LOG_INFO, "TimerStorageSession %p: Pulse() called on session #" UINT32_FORMAT_SPEC ", sending a Message with counter = " UINT32_FORMAT_SPEC " to my client.\n", this, GetSessionID(), _counter);

         MessageRef countMsg = GetMessageFromPool(3333);  // arbitrary
         countMsg()->AddInt32("timer count", _counter);
         AddOutgoingMessage(countMsg);

         _counter++;
         _nextTimerTime += SecondsToMicros(3);

         // No need to call InvalidatePulseTime() here, event though _nextTimerTime has changed
         // That is because GetPulseTime() is guaranteed to always be called after a call to Pulse().
      }
   }

private:
   uint64 _nextTimerTime;
   uint32 _counter;
};

// A factory to create TimerStorageReflectSession objects whenever a TCP connection is received
class TimerStorageReflectSessionFactory : public StorageReflectSessionFactory
{
public:
   TimerStorageReflectSessionFactory() {/* empty */}

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo)
   {
      return AbstractReflectSessionRef(new TimerStorageReflectSession);
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
   TimerStorageReflectSessionFactory timerSmartSessionFactory;
   status_t ret;
   if (reflectServer.PutAcceptFactory(SMART_SERVER_TCP_PORT, ReflectSessionFactoryRef(&timerSmartSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?) [%s]\n", SMART_SERVER_TCP_PORT, ret());
      return 5;
   }

   LogTime(MUSCLE_LOG_INFO, "example_6_smart_server_with_pulsenode is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Note that this is the same server as example_4_smart_server, but with automatic \"counter\"\n");
   LogTime(MUSCLE_LOG_INFO, "Messages sent to each client at three-second intervals, just to demonstrate PulseNode usage.\n");
   printf("\n");
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of example_5_smart_client to connect/chat/subscribe!\n");
   printf("\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_6_smart_server_with_pulsenode is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_6_smart_server_with_pulsenode is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
