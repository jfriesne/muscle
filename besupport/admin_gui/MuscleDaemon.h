#ifndef MUSCLE_DAEMON_H
#define MUSCLE_DAEMON_H

#include <Application.h>
#include <Roster.h>
#include <MessageQueue.h>

#include "reflector/ReflectServer.h"
#include "reflector/DumbReflectSession.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/FilterSessionFactory.h"
#include "reflector/RateLimitSessionIOPolicy.h"
#include "reflector/SignalHandlerSession.h"
#include "system/GlobalMemoryAllocator.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"

#define STR_MUSCLE_DEAMON_NAME "application/x-vnd.Haiku-MuscleDaemon"

namespace muscle {
	
class App : public BApplication
{
public:
						App(void);
						~App(void);
			bool		QuitRequested(void);
			void		MessageReceived(BMessage *msg);
	virtual void		ArgvReceived(int32 argc, char** argv);
	
private:
   uint32 maxBytes;
   uint32 maxNodesPerSession;
   uint32 maxReceiveRate;
   uint32 maxSendRate;
   uint32 maxCombinedRate;
   uint32 maxMessageSize;
   uint32 maxSessions;
   uint32 maxSessionsPerHost;
   String* fprivateKeyFilePath;
		
   int retVal;
   ReflectServer server;

   bool okay;

   Hashtable<IPAddressAndPort, Void> listenPorts;
   Queue<String> bans;
   Queue<String> requires;
   Hashtable<IPAddress, String> tempRemaps;
   Message tempPrivs;
};
};

#endif
