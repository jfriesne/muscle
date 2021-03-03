/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifdef MUSCLE_ENABLE_SSL
# include "dataio/FileDataIO.h"
#endif

#include "reflector/ReflectServer.h"
#include "reflector/DumbReflectSession.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/FilterSessionFactory.h"
#include "reflector/RateLimitSessionIOPolicy.h"
#include "reflector/SignalHandlerSession.h"
#include "system/GlobalMemoryAllocator.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

using namespace muscle;

#define DEFAULT_MUSCLED_PORT 2960

static status_t LoadCryptoKey(bool isPublic, const String * optKeyFilePath, ReflectServer & server)
{
   if (optKeyFilePath == NULL) return B_NO_ERROR;  // no request == no SSL == no problem

   const char * desc = isPublic?"public":"private";

#ifdef MUSCLE_ENABLE_SSL
   FileDataIO fdio(muscleFopen(optKeyFilePath->Cstr(), "rb"));
   ByteBufferRef fileData = GetByteBufferFromPool((uint32)fdio.GetLength());
   if ((fdio.GetFile())&&(fileData())&&(fdio.ReadFully(fileData()->GetBuffer(), fileData()->GetNumBytes()) == fileData()->GetNumBytes()))
   {
      if (isPublic) server.SetSSLPublicKeyCertificate(fileData);
               else server.SetSSLPrivateKey(fileData);

      LogTime(MUSCLE_LOG_INFO, "Using %s key file [%s] to authenticate with connecting clients\n", desc, optKeyFilePath->Cstr());
      return B_NO_ERROR;
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't load %s key file [%s] (file not found?)\n", desc, optKeyFilePath->Cstr());
#else
   (void) server;
   LogTime(MUSCLE_LOG_CRITICALERROR, "Can't load %s key file [%s], SSL support is not compiled in!\n", desc, optKeyFilePath->Cstr());
#endif
   return B_IO_ERROR;
}

// Aux method; main() without the global stuff.  This is a good method to
// call if you already have the global stuff set up the way you like it.
// The third argument can be passed in as NULL, or point to a UsageLimitProxyMemoryAllocator object 
static int muscledmainAux(int argc, char ** argv, void * cookie)
{
   TCHECKPOINT;

   UsageLimitProxyMemoryAllocator * usageLimitAllocator = static_cast<UsageLimitProxyMemoryAllocator*>(cookie);

   uint32 maxBytes           = MUSCLE_NO_LIMIT;
   uint32 maxNodesPerSession = MUSCLE_NO_LIMIT;
   uint32 maxReceiveRate     = MUSCLE_NO_LIMIT;
   uint32 maxSendRate        = MUSCLE_NO_LIMIT;
   uint32 maxCombinedRate    = MUSCLE_NO_LIMIT;
   uint32 maxMessageSize     = MUSCLE_NO_LIMIT;
   uint32 maxSessions        = MUSCLE_NO_LIMIT;
   uint32 maxSessionsPerHost = MUSCLE_NO_LIMIT;

   Hashtable<IPAddressAndPort, Void> listenPorts;
   Queue<String> bans;
   Queue<String> requires;
   Message tempPrivs;
   Hashtable<IPAddress, String> tempRemaps;

   Message args; (void) ParseArgs(argc, argv, args);
   HandleStandardDaemonArgs(args);

   const char * value;
   if (args.HasName("help"))
   {
      Log(MUSCLE_LOG_INFO, "Usage:  muscled [port=%u] [listen=ip:port] [displaylevel=lvl] [filelevel=lvl] [logfile=filename]\n", DEFAULT_MUSCLED_PORT); 
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
      Log(MUSCLE_LOG_INFO, "                [maxmem=megs]\n");
#endif
      Log(MUSCLE_LOG_INFO, "                [maxnodespersession=num] [remap=oldip=newip]\n");
      Log(MUSCLE_LOG_INFO, "                [ban=ippattern] [require=ippattern]\n");
      Log(MUSCLE_LOG_INFO, "                [privban=ippattern] [privunban=ippattern]\n");
      Log(MUSCLE_LOG_INFO, "                [privkick=ippattern] [privall=ippattern]\n");
      Log(MUSCLE_LOG_INFO, "                [maxsendrate=kBps] [maxreceiverate=kBps]\n");
      Log(MUSCLE_LOG_INFO, "                [maxcombinedrate=kBps] [maxmessagesize=k]\n");
      Log(MUSCLE_LOG_INFO, "                [maxsessions=num] [maxsessionsperhost=num]\n");
      Log(MUSCLE_LOG_INFO, "                [localhost=ipaddress] [daemon]\n");
      Log(MUSCLE_LOG_INFO, " - port may be any number between 1 and 65536\n");
      Log(MUSCLE_LOG_INFO, " - listen is like port, except it includes a local interface IP as well.\n");
      Log(MUSCLE_LOG_INFO, " - lvl is: none, critical, errors, warnings, info, debug, or trace.\n");
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
      Log(MUSCLE_LOG_INFO, " - maxmem is the max megabytes of memory the server may use (default=unlimited)\n");
#endif
      Log(MUSCLE_LOG_INFO, " - You may also put one or more ban=<pattern> arguments in.\n");
      Log(MUSCLE_LOG_INFO, "   Each pattern specifies one or more IP addresses to\n");
      Log(MUSCLE_LOG_INFO, "   disallow connections from, e.g. ban=192.168.*.*\n");
      Log(MUSCLE_LOG_INFO, " - You may put one or more require=<pattern> arguments in.\n");
      Log(MUSCLE_LOG_INFO, "   If any of these are present, then only IP addresses that match\n");
      Log(MUSCLE_LOG_INFO, "   at least one of them will be allowed to connect.\n");
      Log(MUSCLE_LOG_INFO, " - To assign privileges, specify one of the following:\n");
      Log(MUSCLE_LOG_INFO, "   privban=<pattern>, privunban=<pattern>,\n");
      Log(MUSCLE_LOG_INFO, "   privkick=<pattern> or privall=<pattern>.\n");
      Log(MUSCLE_LOG_INFO, "   privall assigns all privileges to the matching IP addresses.\n");
      Log(MUSCLE_LOG_INFO, " - remap tells muscled to treat connections from a given IP address\n");
      Log(MUSCLE_LOG_INFO, "   as if they are coming from another (for stupid NAT tricks, etc)\n");
      Log(MUSCLE_LOG_INFO, " - If daemon is specified, muscled will run as a background process.\n");
      return(5);
   }

   {
      for (int32 i=0; (args.FindString("port", i, &value).IsOK()); i++)
      {
         const int16 port = (int16) atoi(value);
         if (port >= 0) listenPorts.PutWithDefault(IPAddressAndPort(invalidIP, port));
      }

      for (int32 i=0; (args.FindString("listen", i, &value).IsOK()); i++)
      {
         const IPAddressAndPort iap(value, DEFAULT_MUSCLED_PORT, false);
         if (iap.GetPort() > 0) listenPorts.PutWithDefault(iap);
                           else LogTime(MUSCLE_LOG_ERROR, "Unable to parse IP/port string [%s]\n", value);
      }
   }

   {
      for (int32 i=0; (args.FindString("remap", i, &value).IsOK()); i++)
      {
         StringTokenizer tok(value, ",=");
         const char * from = tok();
         const char * to = tok();
         const IPAddress fromIP = from ? Inet_AtoN(from) : 0;
         if ((fromIP != invalidIP)&&(to))
         {
            char ipbuf[64]; Inet_NtoA(fromIP, ipbuf);
            LogTime(MUSCLE_LOG_INFO, "Will treat connections coming from [%s] as if they were from [%s].\n", ipbuf, to);
            tempRemaps.Put(fromIP, to);
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error parsing remap argument (it should look something like remap=192.168.0.1,132.239.50.8).\n");
      }
   }

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   if (args.FindString("maxmem", &value).IsOK())
   {
      const int megs = muscleMax(1, atoi(value));
      LogTime(MUSCLE_LOG_INFO, "Limiting memory usage to %i megabyte%s.\n", megs, (megs==1)?"":"s");
      maxBytes = megs*1024L*1024L;
   }
#endif

   if (args.FindString("maxmessagesize", &value).IsOK())
   {
      const int k = muscleMax(1, atoi(value));
      LogTime(MUSCLE_LOG_INFO, "Limiting message sizes to %i kilobyte%s.\n", k, (k==1)?"":"s");
      maxMessageSize = k*1024L;
   }

   if (args.FindString("maxsendrate", &value).IsOK())
   {
      const float k = (float) atof(value);
      maxSendRate = muscleMax((uint32)0, (uint32)(k*1024.0f));
   }

   if (args.FindString("maxreceiverate", &value).IsOK())
   {
      const float k = (float) atof(value);
      maxReceiveRate = muscleMax((uint32)0, (uint32)(k*1024.0f));
   }

   if (args.FindString("maxcombinedrate", &value).IsOK())
   {
      const float k = (float) atof(value);
      maxCombinedRate = muscleMax((uint32)0, (uint32)(k*1024.0f));
   }

   if (args.FindString("maxnodespersession", &value).IsOK())
   {
      maxNodesPerSession = atoi(value);
      LogTime(MUSCLE_LOG_INFO, "Limiting nodes-per-session to " UINT32_FORMAT_SPEC ".\n", maxNodesPerSession);
   }

   if (args.FindString("maxsessions", &value).IsOK())
   {
      maxSessions = atoi(value);
      LogTime(MUSCLE_LOG_INFO, "Limiting total session count to " UINT32_FORMAT_SPEC ".\n", maxSessions);
   }

   if (args.FindString("maxsessionsperhost", &value).IsOK()) 
   {
      maxSessionsPerHost = atoi(value);
      LogTime(MUSCLE_LOG_INFO, "Limiting session count for any given host to " UINT32_FORMAT_SPEC ".\n", maxSessionsPerHost);
   }

   {
      for (int32 i=0; (args.FindString("ban", i, &value).IsOK()); i++)
      {
         LogTime(MUSCLE_LOG_INFO, "Banning all clients whose IP addresses match [%s].\n", value);   
         bans.AddTail(value);
      }
   }

   {
      for (int32 i=0; (args.FindString("require", i, &value).IsOK()); i++)
      {
         LogTime(MUSCLE_LOG_INFO, "Allowing only clients whose IP addresses match [%s].\n", value);   
         requires.AddTail(value);
      }
   }

   {
      const char * privNames[] = {"privkick", "privban", "privunban", "privall"};
      for (int p=0; p<=PR_NUM_PRIVILEGES; p++)  // if (p == PR_NUM_PRIVILEGES), that means all privileges
      {
         for (int32 q=0; (args.FindString(privNames[p], q, &value).IsOK()); q++)
         {
            LogTime(MUSCLE_LOG_INFO, "Clients whose IP addresses match [%s] get %s privileges.\n", value, privNames[p]+4);
            char tt[32]; muscleSprintf(tt, "priv%i", p);
            (void) tempPrivs.AddString(tt, value);
         }
      }
   }

   if ((maxBytes != MUSCLE_NO_LIMIT)&&(usageLimitAllocator)) usageLimitAllocator->SetMaxNumBytes(maxBytes);

   int retVal = 0;
   ReflectServer server;

   status_t ret;
   server.GetAddressRemappingTable() = tempRemaps;

   if (maxNodesPerSession != MUSCLE_NO_LIMIT) server.GetCentralState().AddInt32(PR_NAME_MAX_NODES_PER_SESSION, maxNodesPerSession);
   for (MessageFieldNameIterator iter = tempPrivs.GetFieldNameIterator(); iter.HasData(); iter++) tempPrivs.CopyName(iter.GetFieldName(), server.GetCentralState());

   // If the user asked for bandwidth limiting, create Policy objects to handle that.
   AbstractSessionIOPolicyRef inputPolicyRef, outputPolicyRef;
   if (maxCombinedRate != MUSCLE_NO_LIMIT)
   {
      inputPolicyRef.SetRef(newnothrow RateLimitSessionIOPolicy(maxCombinedRate));
      outputPolicyRef = inputPolicyRef;
      if (inputPolicyRef()) LogTime(MUSCLE_LOG_INFO, "Limiting aggregate I/O bandwidth to %.02f kilobytes/second.\n", ((float)maxCombinedRate/1024.0f));
      else
      {
         MWARN_OUT_OF_MEMORY;
         ret = B_OUT_OF_MEMORY;
      }
   }
   else
   {
      if (maxReceiveRate != MUSCLE_NO_LIMIT)
      {
         inputPolicyRef.SetRef(newnothrow RateLimitSessionIOPolicy(maxReceiveRate));
         if (inputPolicyRef()) LogTime(MUSCLE_LOG_INFO, "Limiting aggregate receive bandwidth to %.02f kilobytes/second.\n", ((float)maxReceiveRate/1024.0f));
         else
         {
            MWARN_OUT_OF_MEMORY;
            ret = B_OUT_OF_MEMORY;
         }
      }
      if (maxSendRate != MUSCLE_NO_LIMIT)
      {
         outputPolicyRef.SetRef(newnothrow RateLimitSessionIOPolicy(maxSendRate));
         if (outputPolicyRef()) LogTime(MUSCLE_LOG_INFO, "Limiting aggregate send bandwidth to %.02f kilobytes/second.\n", ((float)maxSendRate/1024.0f)); 
         else
         {
            MWARN_OUT_OF_MEMORY;
            ret = B_OUT_OF_MEMORY;
         }
      }
   }

   // Set up the Session Factory.  This factory object creates the new StorageReflectSessions
   // as needed when people connect, and also has a filter to keep out the riff-raff.
   StorageReflectSessionFactory factory; factory.SetMaxIncomingMessageSize(maxMessageSize);
   FilterSessionFactory filter(ReflectSessionFactoryRef(&factory, false), maxSessionsPerHost, maxSessions);
   filter.SetInputPolicy(inputPolicyRef);
   filter.SetOutputPolicy(outputPolicyRef);

   for (int b=bans.GetNumItems()-1;     ((ret.IsOK())&&(b>=0)); b--) ret |= filter.PutBanPattern(bans[b]());
   for (int a=requires.GetNumItems()-1; ((ret.IsOK())&&(a>=0)); a--) ret |= filter.PutRequirePattern(requires[a]());

   ret |= LoadCryptoKey(false, args.GetStringPointer("privatekey"), server);
   ret |= LoadCryptoKey(true,  args.GetStringPointer("publickey"),  server);

   // Set up ports.  We allow multiple ports, mostly just to show how it can be done;
   // they all get the same set of ban/require patterns (since they all do the same thing anyway).
   if (listenPorts.IsEmpty()) listenPorts.PutWithDefault(IPAddressAndPort(invalidIP, DEFAULT_MUSCLED_PORT));
   for (HashtableIterator<IPAddressAndPort, Void> iter(listenPorts); iter.HasData(); iter++)
   {
      const IPAddressAndPort & iap = iter.GetKey();

      if (server.PutAcceptFactory(iap.GetPort(), ReflectSessionFactoryRef(&filter, false), iap.GetIPAddress()).IsError(ret))
      {
         if (iap.GetIPAddress() == invalidIP) LogTime(MUSCLE_LOG_CRITICALERROR, "Error adding port %u, aborting.  [%s]\n", iap.GetPort(), ret());
                                         else LogTime(MUSCLE_LOG_CRITICALERROR, "Error adding port %u to interface %s, aborting.  [%s]\n", iap.GetPort(), Inet_NtoA(iap.GetIPAddress())(), ret());
         break;
      }
   }

   if (ret.IsOK())
   {
      retVal = server.ServerProcessLoop().IsOK(ret) ? 0 : 10;
      if (retVal > 0) LogTime(MUSCLE_LOG_CRITICALERROR, "Server process aborted! [%s]\n", ret());
                 else LogTime(MUSCLE_LOG_INFO,          "Server process exiting.\n");
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Error occurred during setup, aborting!\n");

   server.Cleanup();
   return retVal;
}

#ifdef UNIFIED_DAEMON
int muscledmain(int argc, char ** argv) 
#else
int main(int argc, char ** argv) 
#endif
{
   CompleteSetupSystem css;

   TCHECKPOINT;

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   // Set up memory allocation policies for our server.  These policies will make sure
   // that the server can't allocate more than a specified amount of memory, and if it tries,
   // some emergency callbacks will be called to free up cached info.
   FunctionCallback fcb(AbstractObjectRecycler::GlobalFlushAllCachedObjects);
   MemoryAllocatorRef nullRef;
   AutoCleanupProxyMemoryAllocator cleanupAllocator(nullRef);
   cleanupAllocator.GetCallbacksQueue().AddTail(GenericCallbackRef(&fcb, false));

   UsageLimitProxyMemoryAllocator usageLimitAllocator(MemoryAllocatorRef(&cleanupAllocator, false));

   SetCPlusPlusGlobalMemoryAllocator(MemoryAllocatorRef(&usageLimitAllocator, false));
   const int ret = muscledmainAux(argc, argv, &usageLimitAllocator);
   SetCPlusPlusGlobalMemoryAllocator(MemoryAllocatorRef());  // unset, so that none of our allocator objects will be used after they are gone

   return ret;
#else
   // Without memory-allocation rules, things are much simpler :^)
   return muscledmainAux(argc, argv, NULL);
#endif
}
