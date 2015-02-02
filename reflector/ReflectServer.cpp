/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/ReflectServer.h"
# include "reflector/StorageReflectConstants.h"
#ifndef MUSCLE_AVOID_SIGNAL_HANDLING
#include "reflector/SignalHandlerSession.h"
#include "system/SetupSystem.h"  // for IsCurrentThreadMainThread()
#endif
#include "util/NetworkUtilityFunctions.h"
#include "util/MemoryAllocator.h"

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
# include "system/GlobalMemoryAllocator.h"
#endif

#ifdef MUSCLE_ENABLE_SSL
# include "dataio/SSLSocketDataIO.h"
# include "dataio/TCPSocketDataIO.h"
# include "iogateway/SSLSocketAdapterGateway.h"
#endif

namespace muscle {

extern bool _mainReflectServerCatchSignals;  // from SetupSystem.cpp

static const char * DEFAULT_SESSION_HOSTNAME = "_unknown_";

status_t
ReflectServer ::
AddNewSession(const AbstractReflectSessionRef & ref, const ConstSocketRef & ss)
{
   TCHECKPOINT;

   AbstractReflectSession * newSession = ref();
   if (newSession == NULL) return B_ERROR;

   newSession->SetOwner(this);  // in case CreateGateway() needs to use the owner

   ConstSocketRef s = ss;
   if (s() == NULL) s = ref()->CreateDefaultSocket();

   // Create the gateway object for this session, if it isn't already set up
   if (s())
   {
      AbstractMessageIOGatewayRef gatewayRef = newSession->GetGateway();
      if (gatewayRef() == NULL) gatewayRef = newSession->CreateGateway();
      if (gatewayRef())  // don't combine these ifs!
      {
         if (gatewayRef()->GetDataIO()() == NULL)
         {
            // create the new DataIO for the gateway; this must always be done on the fly
            // since it depends on the socket being used.
            DataIORef io = newSession->CreateDataIO(s);
            if (io()) 
            {
#ifdef MUSCLE_ENABLE_SSL
               if (_inDoAccept.IsInBatch())
               {
                  if (_privateKey()) 
                  {
                     if (dynamic_cast<TCPSocketDataIO *>(io()) != NULL)  // We only support SSL over TCP, for now
                     {
                        SSLSocketDataIO * sslIO = newnothrow SSLSocketDataIO(s, false, true);
                        DataIORef sslIORef(sslIO);
                        if (sslIO)
                        {
                           if ((sslIO->SetPublicKeyCertificate(_privateKey) == B_NO_ERROR)&&(sslIO->SetPrivateKey(_privateKey()->GetBuffer(), _privateKey()->GetNumBytes()) == B_NO_ERROR))
                           {
                              io = sslIORef; 
                              gatewayRef.SetRef(newnothrow SSLSocketAdapterGateway(gatewayRef));
                              if (gatewayRef() == NULL) {WARN_OUT_OF_MEMORY; newSession->SetOwner(NULL); return B_ERROR;}
                           }
                           else {LogTime(MUSCLE_LOG_ERROR, "AcceptSession:  Unable to use private key file, incoming connection refused!  (Bad .pem file format?)\n"); newSession->SetOwner(NULL); return B_ERROR;}
                        }
                        else {WARN_OUT_OF_MEMORY; newSession->SetOwner(NULL); return B_ERROR;}
                     }
                  }
               }
               else if (_inDoConnect.IsInBatch())
               {
                  if (_publicKey()) 
                  {
                     if (dynamic_cast<TCPSocketDataIO *>(io()) != NULL)  // We only support SSL over TCP, for now
                     {
                        SSLSocketDataIO * sslIO = newnothrow SSLSocketDataIO(s, false, false);
                        DataIORef sslIORef(sslIO);
                        if (sslIO)
                        {
                           if (sslIO->SetPublicKeyCertificate(_publicKey) == B_NO_ERROR)
                           {
                              io = sslIORef; 
                              gatewayRef.SetRef(newnothrow SSLSocketAdapterGateway(gatewayRef));
                              if (gatewayRef() == NULL) {WARN_OUT_OF_MEMORY; newSession->SetOwner(NULL); return B_ERROR;}
                           }
                           else {LogTime(MUSCLE_LOG_ERROR, "ConnectSession:  Unable to use public key file, outgoing connection aborted!  (Bad .pem file format?)\n"); newSession->SetOwner(NULL); return B_ERROR;}
                        }
                        else {WARN_OUT_OF_MEMORY; newSession->SetOwner(NULL); return B_ERROR;}
                     }
                  }
               }
#endif

               gatewayRef()->SetDataIO(io);
               newSession->SetGateway(gatewayRef);

            }
            else {newSession->SetOwner(NULL); return B_ERROR;}
         }
      }
      else {newSession->SetOwner(NULL); return B_ERROR;}
   }

   TCHECKPOINT;

   // Set our hostname (IP address) string if it isn't already set
   if (newSession->_hostName.IsEmpty())
   {
      const ConstSocketRef & sock = newSession->GetSessionReadSelectSocket();
      if (sock.GetFileDescriptor() >= 0)
      {
         ip_address ip = GetPeerIPAddress(sock, true);
         const String * remapString = _remapIPs.Get(ip);
         char ipbuf[64]; Inet_NtoA(ip, ipbuf);

         newSession->_hostName = newSession->GenerateHostName(ip, remapString ? *remapString : String((ip != invalidIP) ? ipbuf : DEFAULT_SESSION_HOSTNAME));
      }
      else newSession->_hostName = newSession->GenerateHostName(invalidIP, DEFAULT_SESSION_HOSTNAME);
   }

        if (AttachNewSession(ref) == B_NO_ERROR) return B_NO_ERROR;
   else if (newSession) newSession->SetOwner(NULL);

   TCHECKPOINT;

   return B_ERROR;
}

status_t
ReflectServer ::
AddNewConnectSession(const AbstractReflectSessionRef & ref, const ip_address & destIP, uint16 port, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   AbstractReflectSession * session = ref();
   if (session)
   {
      ConstSocketRef sock = ConnectAsync(destIP, port, session->_isConnected);

      // FogBugz #5256:  If ConnectAsync() fails, we want to act as if it succeeded, so that the calling
      //                 code still uses its normal asynchronous-connect-failure code path.  That way the
      //                 caller doesn't have to worry about synchronous failure as a separate case.
      bool usingFakeBrokenConnection = false;
      if (sock() == NULL)
      {
         ConstSocketRef tempSockRef;  // tempSockRef represents the closed remote end of the failed connection and is intentionally closed ASAP
         if (CreateConnectedSocketPair(sock, tempSockRef) == B_NO_ERROR) 
         {
            session->_isConnected = false;
            usingFakeBrokenConnection = true;
         }
      }

      if (sock())
      {
         NestCountGuard ncg(_inDoConnect);
         session->_asyncConnectDest = IPAddressAndPort(destIP, port);
         session->_reconnectViaTCP  = true;
         session->SetMaxAsyncConnectPeriod(maxAsyncConnectPeriod);  // must be done BEFORE SetConnectingAsync()!
         session->SetConnectingAsync((usingFakeBrokenConnection == false)&&(session->_isConnected == false));

         char ipbuf[64]; Inet_NtoA(destIP, ipbuf);
         session->_hostName = session->GenerateHostName(destIP, (destIP != invalidIP) ? ipbuf : DEFAULT_SESSION_HOSTNAME);

         if (AddNewSession(ref, sock) == B_NO_ERROR)
         {
            if (autoReconnectDelay != MUSCLE_TIME_NEVER) session->SetAutoReconnectDelay(autoReconnectDelay);
            if (session->_isConnected) 
            {
               session->_wasConnected = true;
               session->AsyncConnectCompleted();
            }
            if (session->_isExpendable.HasValueBeenSet() == false) session->SetExpendable(true);
            return B_NO_ERROR;
         }
         else
         {
            session->_asyncConnectDest.Reset();
            session->_hostName.Clear();
            session->_isConnected = false;
            session->SetConnectingAsync(false);
         }
      }
   }
   return B_ERROR;
}

status_t
ReflectServer ::
AddNewDormantConnectSession(const AbstractReflectSessionRef & ref, const ip_address & destIP, uint16 port, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   AbstractReflectSession * session = ref();
   if (session)
   {
      NestCountGuard ncg(_inDoConnect);
      session->_asyncConnectDest = IPAddressAndPort(destIP, port);
      session->_reconnectViaTCP  = true;
      char ipbuf[64]; Inet_NtoA(destIP, ipbuf);
      session->_hostName = session->GenerateHostName(destIP, (destIP != invalidIP) ? ipbuf : DEFAULT_SESSION_HOSTNAME);
      status_t ret = AddNewSession(ref, ConstSocketRef());
      if (ret == B_NO_ERROR)
      {
         if (autoReconnectDelay != MUSCLE_TIME_NEVER) session->SetAutoReconnectDelay(autoReconnectDelay);
         session->SetMaxAsyncConnectPeriod(maxAsyncConnectPeriod);
         return B_NO_ERROR;
      }
      else
      {
         session->_asyncConnectDest.Reset();
         session->_hostName.Clear();
      }
   }
   return B_ERROR;
}

status_t
ReflectServer ::
AttachNewSession(const AbstractReflectSessionRef & ref)
{
   AbstractReflectSession * newSession = ref();
   if ((newSession)&&(_sessions.Put(&newSession->GetSessionIDString(), ref) == B_NO_ERROR))
   {
      newSession->SetOwner(this);
      if (newSession->AttachedToServer() == B_NO_ERROR)
      {
         newSession->SetFullyAttachedToServer(true);
         if (_doLogging) LogTime(MUSCLE_LOG_DEBUG, "New %s (" UINT32_FORMAT_SPEC " total)\n", newSession->GetSessionDescriptionString()(), _sessions.GetNumItems());
         return B_NO_ERROR;
      }
      else 
      {
         newSession->AboutToDetachFromServer();  // well, it *was* attached, if only for a moment
         newSession->DoOutput(MUSCLE_NO_LIMIT);  // one last chance for him to send any leftover data!
         if (_doLogging) LogTime(MUSCLE_LOG_DEBUG, "%s aborted startup (" UINT32_FORMAT_SPEC " left)\n", newSession->GetSessionDescriptionString()(), _sessions.GetNumItems()-1);
      }
      newSession->SetOwner(NULL);
      (void) _sessions.Remove(&newSession->GetSessionIDString());
   }
   return B_ERROR;
}


ReflectServer :: ReflectServer() : _keepServerGoing(true), _serverStartedAt(0), _doLogging(true), _serverSessionID(GetCurrentTime64()+GetRunTime64()+rand())
{
   if (_serverSessionID == 0) _serverSessionID++;  // paranoia:  make sure 0 can be used as a guard value

   // make sure _lameDuckSessions has plenty of memory available in advance (we need might need it in a tight spot later!)
   _lameDuckSessions.EnsureSize(256);
}

ReflectServer :: ~ReflectServer()
{
   // empty
}

void
ReflectServer :: Cleanup()
{
   // Detach all sessions
   {
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         AbstractReflectSessionRef nextValue = iter.GetValue();
         if (nextValue()) 
         {
            AbstractReflectSession & ars = *nextValue();
            ars.SetFullyAttachedToServer(false);
            ars.AboutToDetachFromServer();
            ars.DoOutput(MUSCLE_NO_LIMIT);  // one last chance for him to send any leftover data!
            ars.SetOwner(NULL);
            _lameDuckSessions.AddTail(nextValue);  // we'll delete it below
            _sessions.Remove(iter.GetKey());  // but prevent other sessions from accessing it now that it's detached
         }
      }
   }
  
   // Detach all factories
   RemoveAcceptFactory(0);

   // This will dereference everything so they can be safely deleted here
   _lameDuckSessions.Clear();
   _lameDuckFactories.Clear();
}

status_t
ReflectServer ::
ReadyToRun()
{
   return B_NO_ERROR;
}

const char *
ReflectServer ::
GetServerName() const
{
   return "MUSCLE";
}

/** Makes sure the given policy has its BeginIO() called, if necessary, and returns it */
uint32
ReflectServer :: 
CheckPolicy(Hashtable<AbstractSessionIOPolicyRef, Void> & policies, const AbstractSessionIOPolicyRef & policyRef, const PolicyHolder & ph, uint64 now) const
{
   AbstractSessionIOPolicy * p = policyRef();
   if (p)
   {
      // Any policy that is found attached to a session goes into our temporary policy set
       policies.PutWithDefault(policyRef);

      // If the session is ready, and BeginIO() hasn't been called on this policy already, do so now
      if ((ph.GetSession())&&(p->_hasBegun == false))
      {
         CallSetCycleStartTime(*p, now);
         p->BeginIO(now); 
         p->_hasBegun = true;
      }
   }
   return ((ph.GetSession())&&((p == NULL)||(p->OkayToTransfer(ph)))) ? MUSCLE_NO_LIMIT : 0;
}

void ReflectServer :: CheckForOutOfMemory(const AbstractReflectSessionRef & optSessionRef)
{
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();
   if ((ma)&&(ma->HasAllocationFailed()))
   {
      ma->SetAllocationHasFailed(false);  // clear the memory-failed flag
      if ((optSessionRef())&&(optSessionRef()->IsExpendable()))
      {
         if (_doLogging) LogTime(MUSCLE_LOG_CRITICALERROR, "Low Memory!  Aborting %s to get some back!\n", optSessionRef()->GetSessionDescriptionString()());
         AddLameDuckSession(optSessionRef);
      }

      uint32 dumpCount = DumpBoggedSessions();       // see what other cleanup we can do
      if (_doLogging) LogTime(MUSCLE_LOG_CRITICALERROR, "Low Memory condition detected in session [%s].  Dumped " UINT32_FORMAT_SPEC " bogged sessions!\n", optSessionRef()?optSessionRef()->GetSessionDescriptionString()():NULL, dumpCount);
   }
#else
   (void) optSessionRef;
#endif
}

status_t 
ReflectServer ::
ServerProcessLoop()
{
   TCHECKPOINT;

   _serverStartedAt = GetRunTime64();

   if (_doLogging)
   {
      LogTime(MUSCLE_LOG_DEBUG, "This %s server was compiled on " __DATE__ " " __TIME__ "\n", GetServerName());
#ifdef MUSCLE_AVOID_IPV6
      const char * ipState = "disabled";
#else
      const char * ipState = "enabled";
#endif
      LogTime(MUSCLE_LOG_DEBUG, "The server was compiled with MUSCLE version %s.  IPv6 support is %s.\n", MUSCLE_VERSION_STRING, ipState);
      LogTime(MUSCLE_LOG_DEBUG, "This server's session ID is " UINT64_FORMAT_SPEC ".\n", GetServerSessionID());
   }

#ifndef MUSCLE_AVOID_SIGNAL_HANDLING
   if ((_mainReflectServerCatchSignals)&&(IsCurrentThreadMainThread()))
   {
      SignalHandlerSession * shs = newnothrow SignalHandlerSession;
      if (shs == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
      if (AddNewSession(AbstractReflectSessionRef(shs)) != B_NO_ERROR)
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "ReflectServer::ReadyToRun:  Could not install SignalHandlerSession!\n");
         return B_ERROR;
      }
   }
#endif

   if (ReadyToRun() != B_NO_ERROR) 
   {
      if (_doLogging) LogTime(MUSCLE_LOG_CRITICALERROR, "Server:  ReadyToRun() failed, aborting.\n");
      return B_ERROR;
   }

   TCHECKPOINT;

   // Print an informative startup message
   if ((_doLogging)&&(GetMaxLogLevel() >= MUSCLE_LOG_DEBUG))
   {
      if (_factories.HasItems())
      {
         bool listeningOnAll = false;
         for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(_factories); iter.HasData(); iter++)
         {
            const IPAddressAndPort & iap = iter.GetKey();
            LogTime(MUSCLE_LOG_DEBUG, "%s is listening on port %u ", GetServerName(), iap.GetPort());
            if (iap.GetIPAddress() == invalidIP) 
            {
               Log(MUSCLE_LOG_DEBUG, "on all network interfaces.\n");
               listeningOnAll = true;
            }
            else Log(MUSCLE_LOG_DEBUG, "on network interface %s\n", Inet_NtoA(iap.GetIPAddress())()); 
         }

         if (listeningOnAll)
         {
            Queue<NetworkInterfaceInfo> ifs;
            if ((GetNetworkInterfaceInfos(ifs) == B_NO_ERROR)&&(ifs.HasItems()))
            {
               LogTime(MUSCLE_LOG_DEBUG, "This host's network interface addresses are as follows:\n");
               for (uint32 i=0; i<ifs.GetNumItems(); i++) 
               {
                  LogTime(MUSCLE_LOG_DEBUG, "- %s (%s)\n", Inet_NtoA(ifs[i].GetLocalAddress())(), ifs[i].GetName()());
               }
            }
            else LogTime(MUSCLE_LOG_ERROR, "Could not retrieve this server's network interface addresses list.\n"); 
         }
      }
      else LogTime(MUSCLE_LOG_DEBUG, "Server is not listening on any ports.\n");
   }

   // The primary event loop for any MUSCLE-based server!
   // These variables are used as scratch space, but are declared outside the loop to avoid having to reinitialize them all the time.
   Hashtable<AbstractSessionIOPolicyRef, Void> policies;

   while(ClearLameDucks() == B_NO_ERROR)
   {
      TCHECKPOINT;
      EventLoopCycleBegins();

      uint64 nextPulseAt = MUSCLE_TIME_NEVER; // running minimum of everything that wants to be Pulse()'d

      // Set up socket multiplexer registrations and Pulse() timing info for all our different components
      {
         const uint64 now = GetRunTime64(); // nothing in this scope is supposed to take a significant amount of time to execute, so just calculate this once

         TCHECKPOINT;

         // Set up the session factories so we can be notified when a new connection is received
         if (_factories.HasItems())
         {
            for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(_factories); iter.HasData(); iter++)
            {
               ConstSocketRef * nextAcceptSocket = iter.GetValue()()->IsReadyToAcceptSessions() ? _factorySockets.Get(iter.GetKey()) : NULL;
               int nfd = nextAcceptSocket ? nextAcceptSocket->GetFileDescriptor() : -1;
               if (nfd >= 0) (void) _multiplexer.RegisterSocketForReadReady(nfd);
               CallGetPulseTimeAux(*iter.GetValue()(), now, nextPulseAt);
            }
         }

         TCHECKPOINT;

         // Set up the sessions, their associated IO-gateways, and their IOPolicies
         if (_sessions.HasItems())
         {
            for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
            {
               AbstractReflectSession * session = iter.GetValue()();
               if (session)
               {
                  session->_maxInputChunk = session->_maxOutputChunk = 0;
                  AbstractMessageIOGateway * g = session->GetGateway()();
                  if (g)
                  {
                     int sessionReadFD = session->GetSessionReadSelectSocket().GetFileDescriptor();
                     if ((sessionReadFD >= 0)&&(session->IsConnectingAsync() == false))
                     {
                        session->_maxInputChunk = CheckPolicy(policies, session->GetInputPolicy(), PolicyHolder(session->IsReadyForInput() ? session : NULL, true), now);
                        if (session->_maxInputChunk > 0) (void) _multiplexer.RegisterSocketForReadReady(sessionReadFD);
                     }

                     int sessionWriteFD = session->GetSessionWriteSelectSocket().GetFileDescriptor();
                     if (sessionWriteFD >= 0)
                     {
                        bool out;
                        if (session->IsConnectingAsync()) 
                        {
                           out = true;  // so we can watch for the async-connect event
#if defined(WIN32)
                           // Under Windows, failed asynchronous TCP connect()'s are communicated via the a raised exception-flag
                           (void) _multiplexer.RegisterSocketForExceptionRaised(sessionWriteFD);
#endif
                        }
                        else
                        {
                           session->_maxOutputChunk = CheckPolicy(policies, session->GetOutputPolicy(), PolicyHolder(session->HasBytesToOutput() ? session : NULL, false), now);
                           out = ((session->_maxOutputChunk > 0)||((g->GetDataIO()())&&(g->GetDataIO()()->HasBufferedOutput())));
                        }

                        if (out) 
                        {
                           (void) _multiplexer.RegisterSocketForWriteReady(sessionWriteFD);
                           if (session->_lastByteOutputAt == 0) session->_lastByteOutputAt = now;  // the bogged-session-clock starts ticking when we first want to write...
                           if (session->_outputStallLimit != MUSCLE_TIME_NEVER) nextPulseAt = muscleMin(nextPulseAt, session->_lastByteOutputAt+session->_outputStallLimit);
                        }
                        else session->_lastByteOutputAt = 0;  // If we no longer want to write, then the bogged-session-clock-timeout is cancelled
                     }

                     TCHECKPOINT;
                     CallGetPulseTimeAux(*g, now, nextPulseAt);
                     TCHECKPOINT;
                  }
                  TCHECKPOINT;
                  CallGetPulseTimeAux(*session, now, nextPulseAt);
                  TCHECKPOINT;
               }
            }
         }

         TCHECKPOINT;
         CallGetPulseTimeAux(*this, now, nextPulseAt);
         TCHECKPOINT;

         // Set up the Session IO Policies
         if (policies.HasItems())
         {
            // Now that the policies know *who* amongst their policyholders will be reading/writing,
            // let's ask each activated policy *how much* each policyholder should be allowed to read/write.
            for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
            {
               AbstractReflectSession * session = iter.GetValue()();
               if (session)
               {
                  AbstractSessionIOPolicy * inPolicy  = session->GetInputPolicy()();
                  AbstractSessionIOPolicy * outPolicy = session->GetOutputPolicy()();
                  if ((inPolicy)&&( session->_maxInputChunk  > 0)) session->_maxInputChunk  = inPolicy->GetMaxTransferChunkSize(PolicyHolder(session, true));
                  if ((outPolicy)&&(session->_maxOutputChunk > 0)) session->_maxOutputChunk = outPolicy->GetMaxTransferChunkSize(PolicyHolder(session, false));
               }
            }

            // Now that all is prepared, calculate all the policies' wakeup times
            TCHECKPOINT;
            for (HashtableIterator<AbstractSessionIOPolicyRef, Void> iter(policies); iter.HasData(); iter++) CallGetPulseTimeAux(*iter.GetKey()(), now, nextPulseAt);
            TCHECKPOINT;
         }
      }

      TCHECKPOINT;

      // This block is the center of the MUSCLE server's universe -- where we sit and wait for the next event
      if (_multiplexer.WaitForEvents(nextPulseAt) < 0)
      {
         if (_doLogging) LogTime(MUSCLE_LOG_CRITICALERROR, "WaitForEvents() failed, aborting!\n");
         ClearLameDucks();
         return B_ERROR;
      }

      // Each event-loop cycle officially "starts" as soon as WaitForEvents() returns
      CallSetCycleStartTime(*this, GetRunTime64());

      TCHECKPOINT;

      // Before we do any session I/O, make sure there hasn't been a generalized memory failure
      CheckForOutOfMemory(AbstractReflectSessionRef());

      TCHECKPOINT;

      // Do I/O for each of our attached sessions
      {
         for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
         {
            TCHECKPOINT;

            AbstractReflectSessionRef & sessionRef = iter.GetValue();
            AbstractReflectSession * session = sessionRef();
            if (session)
            {
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
               MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();
               if (ma) (void) ma->SetAllocationHasFailed(false);  // (session)'s responsibility for starts here!  If we run out of mem on his watch, he's history
#endif

               TCHECKPOINT;

               CallSetCycleStartTime(*session, GetRunTime64());
               TCHECKPOINT;
               CallPulseAux(*session, session->GetCycleStartTime());
               {
                  AbstractMessageIOGateway * gateway = session->GetGateway()();
                  if (gateway) 
                  {
                     TCHECKPOINT;

                     CallSetCycleStartTime(*gateway, GetRunTime64());
                     CallPulseAux(*gateway, gateway->GetCycleStartTime());
                  }
               }

               TCHECKPOINT;

               int readSock = session->GetSessionReadSelectSocket().GetFileDescriptor();
               if (readSock >= 0)
               {
                  int32 readBytes = 0;
                  if (_multiplexer.IsSocketReadyForRead(readSock))
                  {
                     readBytes = session->DoInput(*session, session->_maxInputChunk);  // session->MessageReceivedFromGateway() gets called here

                     AbstractSessionIOPolicy * p = session->GetInputPolicy()();
                     if ((p)&&(readBytes >= 0)) p->BytesTransferred(PolicyHolder(session, true), (uint32)readBytes);
                  }

                  TCHECKPOINT;

                  if (readBytes < 0)
                  {
                     bool wasConnecting = session->IsConnectingAsync();
                     if ((DisconnectSession(session) == false)&&(_doLogging)) LogTime(MUSCLE_LOG_DEBUG, "Connection for %s %s (read error).\n", session->GetSessionDescriptionString()(), wasConnecting?"failed":"was severed");
                  }
               }

               int writeSock = session->GetSessionWriteSelectSocket().GetFileDescriptor();
               if (writeSock >= 0)
               {
                  int32 wroteBytes = 0;

                  TCHECKPOINT;

                  if (_multiplexer.IsSocketReadyForWrite(writeSock))
                  {
                     if (session->IsConnectingAsync()) wroteBytes = (FinalizeAsyncConnect(sessionRef) == B_NO_ERROR) ? 0 : -1;
                     else
                     {
                        // if the session's DataIO object is still has bytes buffered for output, try to send them now
                        AbstractMessageIOGateway * g = session->GetGateway()();
                        if (g)
                        {
                           DataIO * io = g->GetDataIO()();
                           if (io) io->WriteBufferedOutput();
                        }

                        wroteBytes = session->DoOutput(session->_maxOutputChunk);

                        AbstractSessionIOPolicy * p = session->GetOutputPolicy()();
                        if ((p)&&(wroteBytes >= 0)) p->BytesTransferred(PolicyHolder(session, false), (uint32)wroteBytes);
                     }
                  }
#if defined(WIN32)
                  if (_multiplexer.IsSocketExceptionRaised(writeSock)) wroteBytes = -1;  // async connect() failed!
#endif

                  TCHECKPOINT;

                  if (wroteBytes < 0)
                  {
                     bool wasConnecting = session->IsConnectingAsync();
                     if ((DisconnectSession(session) == false)&&(_doLogging)) LogTime(MUSCLE_LOG_DEBUG, "Connection for %s %s (write error).\n", session->GetSessionDescriptionString()(), wasConnecting?"failed":"was severed");
                  }
                  else if (session->_lastByteOutputAt > 0)
                  {
                     // Check for output stalls
                     const uint64 now = GetRunTime64();
                          if ((wroteBytes > 0)||(session->_maxOutputChunk == 0)) session->_lastByteOutputAt = now;  // reset the moribundness-timer
                     else if (now-session->_lastByteOutputAt > session->_outputStallLimit)
                     {
                        if (_doLogging) LogTime(MUSCLE_LOG_WARNING, "Connection for %s timed out (output stall, no data movement for " UINT64_FORMAT_SPEC " seconds).\n", session->GetSessionDescriptionString()(), MicrosToSeconds(session->_outputStallLimit));
                        (void) DisconnectSession(session);
                     }
                  }
               }
            }
            TCHECKPOINT;
            CheckForOutOfMemory(sessionRef);  // if the session caused a memory error, give him the boot
         }
      }

      TCHECKPOINT;

      // Pulse() our other PulseNode objects, as necessary
      {
         // Tell the session policies we're done doing I/O (for now)
         if (policies.HasItems())
         {
            for (HashtableIterator<AbstractSessionIOPolicyRef, Void> iter(policies); iter.HasData(); iter++)
            {
               AbstractSessionIOPolicy * p = iter.GetKey()();
               if (p->_hasBegun)
               {
                  p->EndIO(GetRunTime64());
                  p->_hasBegun = false;
               }
            }
         }

         // Pulse the Policies
         if (policies.HasItems()) for (HashtableIterator<AbstractSessionIOPolicyRef, Void> iter(policies); iter.HasData(); iter++) CallPulseAux(*iter.GetKey()(), GetRunTime64());

         // Pulse the Server
         CallPulseAux(*this, GetRunTime64());
      }
      policies.Clear();

      TCHECKPOINT;

      // Lastly, check our accepting ports to see if anyone is trying to connect...
      if (_factories.HasItems())
      {
         for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(_factories); iter.HasData(); iter++)
         {
            ReflectSessionFactory * factory = iter.GetValue()();
            CallSetCycleStartTime(*factory, GetRunTime64());
            CallPulseAux(*factory, factory->GetCycleStartTime());

            if (factory->IsReadyToAcceptSessions())
            {
               ConstSocketRef * as = _factorySockets.Get(iter.GetKey());
               int fd = as ? as->GetFileDescriptor() : -1;
               if ((fd >= 0)&&(_multiplexer.IsSocketReadyForRead(fd))) (void) DoAccept(iter.GetKey(), *as, factory);
            }
         }
      }

      TCHECKPOINT;
      EventLoopCycleEnds();
   }

   TCHECKPOINT;
   (void) ClearLameDucks();  // get rid of any leftover ducks
   TCHECKPOINT;

   return B_NO_ERROR;
}

void ReflectServer :: ShutdownIOFor(AbstractReflectSession * session)
{
   AbstractMessageIOGateway * gw = session->GetGateway()();
   if (gw) 
   {
      DataIO * io = gw->GetDataIO()();
      if (io) io->Shutdown();  // so we won't try to do I/O on this one anymore
   }
}

status_t ReflectServer :: ClearLameDucks()
{
   // Delete any factories that were previously marked for deletion
   _lameDuckFactories.Clear();

   // Remove any sessions that were previously marked for removal
   AbstractReflectSessionRef duckRef;
   while(_lameDuckSessions.RemoveHead(duckRef) == B_NO_ERROR)
   {
      AbstractReflectSession * duck = duckRef();
      if (duck)
      {
         const String & id = duck->GetSessionIDString();
         if (_sessions.ContainsKey(&id))
         {
            duck->SetFullyAttachedToServer(false);
            duck->AboutToDetachFromServer();
            duck->DoOutput(MUSCLE_NO_LIMIT);  // one last chance for him to send any leftover data!
            if (_doLogging) LogTime(MUSCLE_LOG_DEBUG, "Closed %s (" UINT32_FORMAT_SPEC " left)\n", duck->GetSessionDescriptionString()(), _sessions.GetNumItems()-1);
            duck->SetOwner(NULL);
            (void) _sessions.Remove(&id);
         }
      }
   }

   return _keepServerGoing ? B_NO_ERROR : B_ERROR;
}

void ReflectServer :: LogAcceptFailed(int lvl, const char * desc, const char * ipbuf, const IPAddressAndPort & iap)
{
   if (_doLogging)
   {
      if (iap.GetIPAddress() == invalidIP) LogTime(lvl, "%s for [%s] on port %u.\n", desc, ipbuf?ipbuf:"???", iap.GetPort());
                                      else LogTime(lvl, "%s for [%s] on port %u on interface [%s].\n", desc, ipbuf?ipbuf:"???", iap.GetPort(), Inet_NtoA(iap.GetIPAddress())());
   }
}

status_t ReflectServer :: DoAccept(const IPAddressAndPort & iap, const ConstSocketRef & acceptSocket, ReflectSessionFactory * optFactory)
{
   // Accept a new connection and try to start up a session for it
   ip_address acceptedFromIP;
   ConstSocketRef newSocket = Accept(acceptSocket, &acceptedFromIP);
   if (newSocket())
   {
      NestCountGuard ncg(_inDoAccept);
      IPAddressAndPort nip(acceptedFromIP, iap.GetPort());
      ip_address remoteIP = GetPeerIPAddress(newSocket, true);
      if (remoteIP == invalidIP) LogAcceptFailed(MUSCLE_LOG_DEBUG, "GetPeerIPAddress() failed", NULL, nip);
      else
      {
         char ipbuf[64]; Inet_NtoA(remoteIP, ipbuf);

         AbstractReflectSessionRef newSessionRef;
         if (optFactory) newSessionRef = optFactory->CreateSession(ipbuf, nip);

         if (newSessionRef()) 
         {
            if (newSessionRef()->_isExpendable.HasValueBeenSet() == false) newSessionRef()->SetExpendable(true);
            newSessionRef()->_ipAddressAndPort = iap;
            newSessionRef()->_isConnected      = true;
            if (AddNewSession(newSessionRef, newSocket) == B_NO_ERROR) 
            {
               newSessionRef()->_wasConnected = true;   
               return B_NO_ERROR;  // success!
            }
            else
            {
               newSessionRef()->_isConnected = false;   
               newSessionRef()->_ipAddressAndPort.Reset();
            }
         }
         else if (optFactory) LogAcceptFailed(MUSCLE_LOG_DEBUG, "Session creation denied", ipbuf, nip);
      }
   }
   else LogAcceptFailed(MUSCLE_LOG_DEBUG, "Accept() failed", NULL, iap);

   return B_ERROR;
}

uint32 ReflectServer :: DumpBoggedSessions()
{
   TCHECKPOINT;

   uint32 ret = 0;

   // New for v1.82:  also find anyone whose outgoing message queue is getting too large.
   //                 (where "too large", for now, is more than 5 megabytes)
   // This could happen if someone has a really slow Internet connection, or has decided to
   // stop reading from their end indefinitely for some reason.
   const int MAX_MEGABYTES = 5;
   for (HashtableIterator<const String *, AbstractReflectSessionRef> xiter(GetSessions()); xiter.HasData(); xiter++)
   {
      AbstractReflectSession * asr = xiter.GetValue()();
      if ((asr)&&(asr->IsExpendable()))
      {
         AbstractMessageIOGateway * gw = asr->GetGateway()();
         if (gw)
         {
            uint32 qSize = 0;
            const Queue<MessageRef> & q = gw->GetOutgoingMessageQueue();
            for (int k=q.GetNumItems()-1; k>=0; k--)
            {
               const Message * qmsg = q[k]();
               if (qmsg) qSize += qmsg->FlattenedSize();
            }
            if (qSize > MAX_MEGABYTES*1024*1024)
            {
               if (_doLogging) LogTime(MUSCLE_LOG_CRITICALERROR, "Low Memory!  Aborting %s to get some back!\n", asr->GetSessionDescriptionString()());
               AddLameDuckSession(xiter.GetValue());
               ret++;
            }
         }
      }
   }
   return ret;
}

AbstractReflectSessionRef
ReflectServer ::
GetSession(const String & name) const
{
   AbstractReflectSessionRef ref;
   (void) _sessions.Get(&name, ref); 
   return ref;
}

AbstractReflectSessionRef
ReflectServer ::
GetSession(uint32 id) const
{
   char buf[64]; sprintf(buf, UINT32_FORMAT_SPEC, id);
   return GetSession(buf);
}

ReflectSessionFactoryRef
ReflectServer ::
GetFactory(uint16 port, const ip_address & optInterfaceIP) const
{
   ReflectSessionFactoryRef ref;
   (void) _factories.Get(IPAddressAndPort(optInterfaceIP, port), ref); 
   return ref;
}

status_t
ReflectServer ::
ReplaceSession(const AbstractReflectSessionRef & newSessionRef, AbstractReflectSession * oldSession)
{
   TCHECKPOINT;

   // move the gateway from the old session to the new one...
   AbstractReflectSession * newSession = newSessionRef();
   if (newSession == NULL) return B_ERROR;

   newSession->SetGateway(oldSession->GetGateway());
   newSession->_hostName = oldSession->_hostName;
   newSession->_ipAddressAndPort = oldSession->_ipAddressAndPort;

   if (AttachNewSession(newSessionRef) == B_NO_ERROR)
   {
      oldSession->SetGateway(AbstractMessageIOGatewayRef());   /* gateway now belongs to newSession */
      EndSession(oldSession);
      return B_NO_ERROR;
   }
   else
   {
       // Oops, rollback changes and error out
       newSession->SetGateway(AbstractMessageIOGatewayRef());
       newSession->_hostName.Clear();
       newSession->_ipAddressAndPort.Reset();
       return B_ERROR;
   }
}

bool ReflectServer :: DisconnectSession(AbstractReflectSession * session)
{
   session->SetConnectingAsync(false);
   session->_isConnected = false;
   session->_scratchReconnected = false;  // if the session calls Reconnect() this will be set to true below

   AbstractMessageIOGateway * oldGW = session->GetGateway()();
   DataIO * oldIO = oldGW ? oldGW->GetDataIO()() : NULL;

   bool ret = session->ClientConnectionClosed(); 

   AbstractMessageIOGateway * newGW = session->GetGateway()();
   DataIO * newIO = newGW ? newGW->GetDataIO()() : NULL;

   if (ret) 
   {
      ShutdownIOFor(session);
      AddLameDuckSession(session);
   }
   else if ((session->_scratchReconnected == false)&&(newGW == oldGW)&&(newIO == oldIO)) ShutdownIOFor(session);

   return ret;
}

void
ReflectServer ::
EndSession(AbstractReflectSession * who)
{
   AddLameDuckSession(who);
}

void
ReflectServer ::
EndServer()
{
   _keepServerGoing = false;
}

status_t
ReflectServer ::
PutAcceptFactory(uint16 port, const ReflectSessionFactoryRef & factoryRef, const ip_address & optInterfaceIP, uint16 * optRetPort)
{
   if (port > 0) (void) RemoveAcceptFactory(port, optInterfaceIP); // Get rid of any previous acceptor on this port...

   ReflectSessionFactory * f = factoryRef();
   if (f)
   {
      ConstSocketRef acceptSocket = CreateAcceptingSocket(port, 20, &port, optInterfaceIP);
      if (acceptSocket())
      {
         IPAddressAndPort iap(optInterfaceIP, port);
         if ((SetSocketBlockingEnabled(acceptSocket, false) == B_NO_ERROR)&&(_factories.Put(iap, factoryRef) == B_NO_ERROR))
         {
            if (_factorySockets.Put(iap, acceptSocket) == B_NO_ERROR)
            {
               f->SetOwner(this);
               if (optRetPort) *optRetPort = port;
               if (f->AttachedToServer() == B_NO_ERROR) 
               {
                  f->SetFullyAttachedToServer(true);
                  return B_NO_ERROR;
               }
               else
               {
                  (void) RemoveAcceptFactory(port, optInterfaceIP);
                  return B_ERROR;
               }
            }
            else _factories.Remove(iap); // roll back!
         }
      }
   }
   return B_ERROR;
}

status_t
ReflectServer ::
RemoveAcceptFactoryAux(const IPAddressAndPort & iap)
{
   ReflectSessionFactoryRef ref;
   if (_factories.Get(iap, ref) == B_NO_ERROR)  // don't remove it yet, in case AboutToDetachFromServer() calls a method on us
   {
      ref()->SetFullyAttachedToServer(false);
      ref()->AboutToDetachFromServer();
      _lameDuckFactories.AddTail(ref);  // we'll actually have (factory) deleted later on, since at the moment 
                                        // we could be in the middle of one of (ref())'s own method calls!
      (void) _factories.Remove(iap);  // must call this AFTER the AboutToDetachFromServer() call!

      // See if there are any other instances of this factory still present.
      // if there aren't, we'll clear the factory's owner-pointer so he can't access us anymore
      if (_factories.IndexOfValue(ref) < 0) ref()->SetOwner(NULL);

      (void) _factorySockets.Remove(iap);

      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t
ReflectServer ::
RemoveAcceptFactory(uint16 port, const ip_address & optInterfaceIP)
{
   if (port > 0) return RemoveAcceptFactoryAux(IPAddressAndPort(optInterfaceIP, port));
   else
   {
      for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(_factories); iter.HasData(); iter++) (void) RemoveAcceptFactoryAux(iter.GetKey());
      return B_NO_ERROR;
   }
}

status_t 
ReflectServer :: 
FinalizeAsyncConnect(const AbstractReflectSessionRef & ref)
{
   AbstractReflectSession * session = ref();
   if ((session)&&(muscle::FinalizeAsyncConnect(session->GetSessionReadSelectSocket()) == B_NO_ERROR))
   {
      session->SetConnectingAsync(false);
      session->_isConnected = session->_wasConnected = true;
      session->AsyncConnectCompleted();
      return B_NO_ERROR;
   }
   return B_ERROR;
}

uint64 
ReflectServer ::
GetNumAvailableBytes() const
{
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   const MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();
   return (ma) ? ma->GetNumAvailableBytes((size_t)GetNumUsedBytes()) : ((uint64)-1);
#else
   return ((uint64)-1);
#endif
}
 
uint64 
ReflectServer ::
GetMaxNumBytes() const
{
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   const MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();
   return ma ? ma->GetMaxNumBytes() : ((uint64)-1);
#else
   return ((uint64)-1);
#endif
}
 
uint64 
ReflectServer ::
GetNumUsedBytes() const
{
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING
   return GetNumAllocatedBytes();
#else
   return 0;  // if we're not tracking, there is no way to know!
#endif
}

void
ReflectServer :: AddLameDuckSession(const AbstractReflectSessionRef & ref)
{
   if ((_lameDuckSessions.IndexOf(ref) < 0)&&(_lameDuckSessions.AddTail(ref) != B_NO_ERROR)&&(_doLogging)) LogTime(MUSCLE_LOG_CRITICALERROR, "Server:  AddLameDuckSession() failed, I'm REALLY in trouble!  Aggh!\n");
}

void
ReflectServer ::
AddLameDuckSession(AbstractReflectSession * who)
{
   TCHECKPOINT;

   for (HashtableIterator<const String *, AbstractReflectSessionRef> xiter(GetSessions()); xiter.HasData(); xiter++)
   {
      if (xiter.GetValue()() == who)
      {
         AddLameDuckSession(xiter.GetValue());
         break;
      }
   }
}

}; // end namespace muscle
