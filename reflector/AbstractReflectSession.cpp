/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/AbstractReflectSession.h"
#include "reflector/AbstractSessionIOPolicy.h"
#include "reflector/ReflectServer.h"
#include "dataio/TCPSocketDataIO.h"
#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
# include "iogateway/TemplatingMessageIOGateway.h"
#else
# include "iogateway/MessageIOGateway.h"
#endif
#include "system/Mutex.h"
#include "system/SetupSystem.h"

#ifdef MUSCLE_ENABLE_SSL
# include "dataio/SSLSocketDataIO.h"
# include "iogateway/SSLSocketAdapterGateway.h"
#endif

namespace muscle {

static uint32 _sessionIDCounter = 0L;
static uint32 _factoryIDCounter = 0L;

static uint32 GetNextGlobalID(uint32 & counter)
{
   Mutex * ml = GetGlobalMuscleLock();
   MASSERT(ml, "Please instantiate a CompleteSetupSystem object on the stack before creating any session or session-factory objects (at beginning of main() is preferred)\n");

   DECLARE_MUTEXGUARD(*ml);
   return counter++;
}

ReflectSessionFactory :: ReflectSessionFactory()
{
   TCHECKPOINT;

   _id                        = GetNextGlobalID(_factoryIDCounter);
   _mostRecentAcceptTimeStamp = MUSCLE_TIME_NEVER;
   _acceptCount               = 0;
}

status_t ProxySessionFactory :: AttachedToServer()
{
   MRETURN_ON_ERROR(ReflectSessionFactory::AttachedToServer());

   status_t ret;
   if (_slaveRef())
   {
      _slaveRef()->SetOwner(GetOwner());
      if (_slaveRef()->AttachedToServer().IsOK(ret)) _slaveRef()->SetFullyAttachedToServer(true);
                                                else _slaveRef()->SetOwner(NULL);
   }
   return ret;
}

void ProxySessionFactory :: AboutToDetachFromServer()
{
   if (_slaveRef())
   {
      _slaveRef()->SetFullyAttachedToServer(false);
      _slaveRef()->AboutToDetachFromServer();
      _slaveRef()->SetOwner(NULL);
   }
   ReflectSessionFactory::AboutToDetachFromServer();
}

AbstractReflectSession ::
AbstractReflectSession()
   : _sessionID(GetNextGlobalID(_sessionIDCounter))
   , _connectingAsync(false)
   , _isConnected(false)
   , _maxAsyncConnectPeriod(MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
   , _asyncConnectTimeoutTime(MUSCLE_TIME_NEVER)
   , _reconnectViaTCP(true)
   , _lastByteOutputAt(0)
   , _pendingLastByteOutputAt(0)
   , _lastReportedQueueSize(0)
   , _maxInputChunk(MUSCLE_NO_LIMIT)
   , _maxOutputChunk(MUSCLE_NO_LIMIT)
   , _outputStallLimit(MUSCLE_TIME_NEVER)
   , _scratchReconnected(false)
   , _autoReconnectDelay(MUSCLE_TIME_NEVER)
   , _reconnectTime(MUSCLE_TIME_NEVER)
   , _wasConnected(false)
   , _isExpendable(false)
   , _mostRecentInputTimeStamp(MUSCLE_TIME_NEVER)
   , _mostRecentOutputTimeStamp(MUSCLE_TIME_NEVER)
{
   char buf[64]; muscleSprintf(buf, UINT32_FORMAT_SPEC, _sessionID);
   _idString = buf;
}

AbstractReflectSession ::
~AbstractReflectSession()
{
   TCHECKPOINT;
   SetInputPolicy(AbstractSessionIOPolicyRef());   // make sure the input policy knows we're going away
   SetOutputPolicy(AbstractSessionIOPolicyRef());  // make sure the output policy knows we're going away
}

const String &
AbstractReflectSession ::
GetHostName() const
{
   MASSERT(IsAttachedToServer(), "Can not call GetHostName() while not attached to the server");
   return _hostName;
}

uint16
AbstractReflectSession ::
GetPort() const
{
   MASSERT(IsAttachedToServer(), "Can not call GetPort() while not attached to the server");
   return _ipAddressAndPort.GetPort();
}

const IPAddress &
AbstractReflectSession ::
GetLocalInterfaceAddress() const
{
   MASSERT(IsAttachedToServer(), "Can not call LocalInterfaceAddress() while not attached to the server");
   return _ipAddressAndPort.GetIPAddress();
}

status_t
AbstractReflectSession ::
AddOutgoingMessage(const MessageRef & ref)
{
   MASSERT(IsAttachedToServer(), "Can not call AddOutgoingMessage() while not attached to the server");
   return _gateway() ? _gateway()->AddOutgoingMessage(ref) : B_BAD_OBJECT;
}

status_t
AbstractReflectSession ::
Reconnect()
{
   TCHECKPOINT;

#ifdef MUSCLE_ENABLE_SSL
   ConstByteBufferRef publicKey;  // If it's an SSL connection we'll grab its key into here so we can reuse it
#endif

   MASSERT(IsAttachedToServer(), "Can not call Reconnect() while not attached to the server");
   if (_gateway())
   {
#ifdef MUSCLE_ENABLE_SSL
      SSLSocketDataIO * sdio = dynamic_cast<SSLSocketDataIO *>(_gateway()->GetDataIO()());
      if (sdio) publicKey = sdio->GetPublicKeyCertificate();
#endif
      _gateway()->SetDataIO(DataIORef());  // get rid of any existing socket first
      _gateway()->Reset();                 // set gateway back to its virgin state
   }

   _isConnected = _wasConnected = false;
   SetConnectingAsync(false);

   bool doTCPConnect = ((_reconnectViaTCP)&&(_asyncConnectDest.GetIPAddress() != invalidIP));
   bool isReady = false;
   ConstSocketRef optSock = doTCPConnect ? ConnectAsync(_asyncConnectDest, isReady) : CreateDefaultSocket();

   // FogBugz #5256:  If ConnectAsync() fails, we want to act as if it succeeded, so that the calling
   //                 code still uses its normal asynchronous-connect-failure code path.  That way the
   //                 caller doesn't have to worry about synchronous failure as a separate case.
   if ((doTCPConnect)&&(optSock() == NULL))
   {
      ConstSocketRef tempSockRef;  // tempSockRef represents the closed remote end of the failed connection and is intentionally closed ASAP
      if (CreateConnectedSocketPair(optSock, tempSockRef).IsOK()) doTCPConnect = false;
      MRETURN_ON_ERROR(optSock);
   }

   DataIORef io = CreateDataIO(optSock);
   if (io() == NULL) return B_ERROR("Reconnect(): CreateDataIO() failed");

   if (_gateway() == NULL)
   {
      _gateway = CreateGateway();
      if (_gateway() == NULL) return B_ERROR("Reconnect(): CreateGateway() failed");
   }

#ifdef MUSCLE_ENABLE_SSL
   // auto-wrap the user's gateway and socket in the necessary SSL adapters!
   if ((publicKey())&&(optSock())&&(dynamic_cast<TCPSocketDataIO *>(io()) != NULL))
   {
      SSLSocketDataIO * ssio = new SSLSocketDataIO(optSock, false, false);
      io.SetRef(ssio);
      MRETURN_ON_ERROR(ssio->SetPublicKeyCertificate(publicKey));
      if (dynamic_cast<SSLSocketAdapterGateway *>(_gateway()) == NULL) _gateway.SetRef(new SSLSocketAdapterGateway(_gateway));
   }
#endif

   _gateway()->SetDataIO(io);
   if (isReady)
   {
      _isConnected = _wasConnected = true;
      AsyncConnectCompleted();
   }
   else
   {
      _isConnected = false;
      SetConnectingAsync(doTCPConnect);
   }
   _scratchReconnected = true;   // tells ReflectServer not to shut down our new IO!
   return B_NO_ERROR;
}

ConstSocketRef
AbstractReflectSession ::
CreateDefaultSocket()
{
   return ConstSocketRef();  // NULL Ref means run clientless by default
}

DataIORef
AbstractReflectSession ::
CreateDataIO(const ConstSocketRef & socket)
{
   return TCPSocketDataIORef(new TCPSocketDataIO(socket, false));
}

AbstractMessageIOGatewayRef
AbstractReflectSession ::
CreateGateway()
{
#ifdef MUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
   return MessageIOGatewayRef(new TemplatingMessageIOGateway());
#else
   return MessageIOGatewayRef(new MessageIOGateway());
#endif
}

bool
AbstractReflectSession ::
ClientConnectionClosed()
{
   if (_autoReconnectDelay == MUSCLE_TIME_NEVER) return true;  // true == okay to remove this session
   else
   {
      if (_wasConnected) LogTime(MUSCLE_LOG_DEBUG, "%s:  Connection severed, will auto-reconnect in [%s]\n", GetSessionDescriptionString()(), GetHumanReadableUnsignedTimeIntervalString(_autoReconnectDelay)());
      PlanForReconnect();
      return false;
   }
}

void
AbstractReflectSession ::
BroadcastToAllSessions(const MessageRef & msgRef, void * userData, bool toSelf)
{
   TCHECKPOINT;

   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      AbstractReflectSession * session = iter.GetValue()();
      if ((session)&&((toSelf)||(session != this))) session->MessageReceivedFromSession(*this, msgRef, userData);
   }
}

void
AbstractReflectSession ::
BroadcastToAllFactories(const MessageRef & msgRef, void * userData)
{
   TCHECKPOINT;

   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      ReflectSessionFactory * factory = iter.GetValue()();
      if (factory) factory->MessageReceivedFromSession(*this, msgRef, userData);
   }
}

void AbstractReflectSession :: AsyncConnectCompleted()
{
   // These sets are mostly redundant, since typically ReflectServer sets these variables
   // directly before calling AsyncConnectCompleted()... but if third party code is calling
   // AsyncConnectCompleted(), then we might as well make sure they are set from that context
   // also.
   _isConnected = _wasConnected = true;

#if defined(__EMSCRIPTEN__)
   // A cheap hack to keep Emscripten responsive, because otherwise
   // there's no easy way to trigger the ServerEventLoop to be executed
   // again later on to flush our outgoing-message-queue.
   while(DoOutput(MUSCLE_NO_LIMIT).GetByteCount() > 0) {/* empty */}
#endif
}

void AbstractReflectSession :: SetInputPolicy(const AbstractSessionIOPolicyRef & newRef) {SetPolicyAux(_inputPolicyRef, _maxInputChunk, newRef, true);}
void AbstractReflectSession :: SetOutputPolicy(const AbstractSessionIOPolicyRef & newRef) {SetPolicyAux(_outputPolicyRef, _maxOutputChunk, newRef, true);}
void AbstractReflectSession :: SetPolicyAux(AbstractSessionIOPolicyRef & myRef, uint32 & chunk, const AbstractSessionIOPolicyRef & newRef, bool isInput)
{
   TCHECKPOINT;

   if (newRef != myRef)
   {
      PolicyHolder ph(this, isInput);
      if (myRef()) myRef()->PolicyHolderRemoved(ph);
      myRef = newRef;
      chunk = myRef() ? 0 : MUSCLE_NO_LIMIT;  // sensible default to use until my policy gets its say about what we should do
      if (myRef()) myRef()->PolicyHolderAdded(ph);
   }
}

status_t
AbstractReflectSession ::
ReplaceSession(const AbstractReflectSessionRef & replaceMeWithThis)
{
   MASSERT(IsAttachedToServer(), "Can not call ReplaceSession() while not attached to the server");
   return GetOwner()->ReplaceSession(replaceMeWithThis, this);
}

void
AbstractReflectSession ::
EndSession()
{
   if (IsAttachedToServer()) GetOwner()->EndSession(this);
}

bool
AbstractReflectSession ::
DisconnectSession()
{
   MASSERT(IsAttachedToServer(), "Can not call DisconnectSession() while not attached to the server");
   return GetOwner()->DisconnectSession(this);
}

String
AbstractReflectSession ::
GetSessionDescriptionString() const
{
   String ret = GetTypeName();
   ret += ' ';
   ret += GetSessionIDString();
   ret += (_ipAddressAndPort.GetPort()>0)?" at ":" to ";
   ret += GetClientDescriptionString();
   return ret;
}

String
AbstractReflectSession ::
GetClientDescriptionString() const
{
   if (GetDataIO()())
   {
      const uint16 port = _ipAddressAndPort.GetPort();
      if (port > 0)
      {
         char buf[64]; muscleSprintf(buf, ":%u", port);
         return _hostName + buf;
      }
      else
      {
         const IPAddressAndPort iap = GetSocketBindAddress(GetSessionReadSelectSocket());
         return iap.IsValid() ? iap.ToString() : ((iap.GetPort() > 0) ? String("port %1").Arg(iap.GetPort()) : "???");
      }
   }
   else return "nowhere";
}

bool
AbstractReflectSession ::
HasBytesToOutput() const
{
   return _gateway() ? _gateway()->HasBytesToOutput() : false;
}

bool
AbstractReflectSession ::
IsReadyForInput() const
{
   return _gateway() ? _gateway()->IsReadyForInput() : false;
}

io_status_t
AbstractReflectSession ::
DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   return _gateway() ? _gateway()->DoInput(receiver, maxBytes) : io_status_t();
}

io_status_t
AbstractReflectSession ::
DoOutput(uint32 maxBytes)
{
   return _gateway() ? _gateway()->DoOutput(maxBytes) : io_status_t();
}

void
ReflectSessionFactory ::
BroadcastToAllSessions(const MessageRef & msgRef, void * userData)
{
   TCHECKPOINT;

   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      AbstractReflectSession * session = iter.GetValue()();
      if (session) session->MessageReceivedFromFactory(*this, msgRef, userData);
   }
}

void
ReflectSessionFactory ::
BroadcastToAllFactories(const MessageRef & msgRef, void * userData, bool toSelf)
{
   TCHECKPOINT;

   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      ReflectSessionFactory * factory = iter.GetValue()();
      if ((factory)&&((toSelf)||(factory != this))) factory->MessageReceivedFromFactory(*this, msgRef, userData);
   }
}

void
AbstractReflectSession ::
PlanForReconnect()
{
   _reconnectTime = (_autoReconnectDelay == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+_autoReconnectDelay);
   InvalidatePulseTime();
}

uint64
AbstractReflectSession ::
GetPulseTime(const PulseArgs &)
{
   return muscleMin(IsThisSessionScheduledForPostSleepReconnect()?MUSCLE_TIME_NEVER:_reconnectTime, _asyncConnectTimeoutTime);
}

bool
AbstractReflectSession ::
IsThisSessionScheduledForPostSleepReconnect() const
{
   return ((GetOwner())&&(GetOwner()->IsSessionScheduledForPostSleepReconnect(GetSessionIDString())));
}

void
AbstractReflectSession ::
Pulse(const PulseArgs & args)
{
   PulseNode::Pulse(args);
   if ((args.GetCallbackTime() >= _reconnectTime)&&(IsThisSessionScheduledForPostSleepReconnect() == false))
   {
      if (_autoReconnectDelay == MUSCLE_TIME_NEVER) _reconnectTime = MUSCLE_TIME_NEVER;
      else
      {
         // FogBugz #3810
         if (_wasConnected) LogTime(MUSCLE_LOG_DEBUG, "%s is attempting to auto-reconnect...\n", GetSessionDescriptionString()());
         _reconnectTime = MUSCLE_TIME_NEVER;

         status_t ret;
         if (Reconnect().IsError(ret))
         {
            LogTime(MUSCLE_LOG_DEBUG, "%s: Could not auto-reconnect [%s], will try again later...\n", ret(), GetSessionDescriptionString()());
            PlanForReconnect();  // okay, we'll try again later!
         }
      }
   }
   else if ((IsConnectingAsync())&&(args.GetCallbackTime() >= _asyncConnectTimeoutTime)) (void) DisconnectSession();  // force us to terminate our async-connect now
}

void AbstractReflectSession :: SetConnectingAsync(bool isConnectingAsync)
{
   _connectingAsync = isConnectingAsync;
   _asyncConnectTimeoutTime = ((_connectingAsync)&&(_maxAsyncConnectPeriod != MUSCLE_TIME_NEVER)) ? (GetRunTime64()+_maxAsyncConnectPeriod) : MUSCLE_TIME_NEVER;
   InvalidatePulseTime();
}

const DataIORef &
AbstractReflectSession ::
GetDataIO() const
{
   return _gateway() ? _gateway()->GetDataIO() : GetDefaultObjectForType<DataIORef>();
}

const ConstSocketRef &
AbstractReflectSession ::
GetSessionReadSelectSocket() const
{
   const DataIORef & dio = GetDataIO();
   return dio() ? dio()->GetReadSelectSocket() : GetNullSocket();
}

const ConstSocketRef &
AbstractReflectSession ::
GetSessionWriteSelectSocket() const
{
   const DataIORef & dio = GetDataIO();
   return dio() ? dio()->GetWriteSelectSocket() : GetNullSocket();
}

String
AbstractReflectSession ::
GenerateHostName(const IPAddress & /*ip*/, const String & defaultHostName) const
{
   return defaultHostName;
}

void
AbstractReflectSession ::
PrintFactoriesInfo(const OutputPrinter & p) const
{
   p.printf("There are " UINT32_FORMAT_SPEC " factories attached:\n", GetFactories().GetNumItems());
   const uint64 now = GetRunTime64();
   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      const ReflectSessionFactory & f = *iter.GetValue()();
      p.printf("   %s #" UINT32_FORMAT_SPEC " is listening at %s (%sAcceptCount=" UINT32_FORMAT_SPEC, f.GetTypeName(), f.GetFactoryID(), iter.GetKey().ToString()(), f.IsReadyToAcceptSessions()?"ReadyToAcceptSessions, ":"", f.GetAcceptCount());

      const uint64 ts = f.GetMostRecentAcceptTimeStamp();
      if (ts != MUSCLE_TIME_NEVER) p.printf(" LastAccept: %s ago)\n", GetHumanReadableUnsignedTimeIntervalString(now-ts, 1)());
                              else p.printf(")\n");
   }
}

void
AbstractReflectSession ::
TallySubscriberTablesInfo(uint32 & retNumCachedSubscriberTables, uint32 & tallyNumNodes, uint32 & tallyNumNodeBytes) const
{
   (void) retNumCachedSubscriberTables;
   (void) tallyNumNodes;
   (void) tallyNumNodeBytes;
}

void
AbstractReflectSession ::
PrintSessionsInfo(const OutputPrinter & p) const
{
   const Hashtable<const String *, AbstractReflectSessionRef> & t = GetSessions();

   p.printf("There are " UINT32_FORMAT_SPEC " sessions attached:\n", t.GetNumItems());

   const uint64 now = GetRunTime64();
   uint32 totalNumCachedSubscribersTables = 0, totalNumOutMessages = 0, totalNumOutBytes = 0, totalNumNodes = 0, totalNumNodeBytes = 0;
   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(t); iter.HasData(); iter++)
   {
      AbstractReflectSession * ars = iter.GetValue()();

      uint32 numCachedSubscribersTables = 0, numNodes = 0, numNodeBytes = 0;
      ars->TallySubscriberTablesInfo(numCachedSubscribersTables, numNodes, numNodeBytes);

      uint32 numOutMessages = 0, numOutBytes = 0;
      const AbstractMessageIOGateway * gw = ars->GetGateway()();
      if (gw)
      {
         const Queue<MessageRef> & q = gw->GetOutgoingMessageQueue();
         numOutMessages = q.GetNumItems();
         for (uint32 i=0; i<numOutMessages; i++) numOutBytes = q[i]()->FlattenedSize();
      }

      String stateStr;
      if (ars->IsConnectingAsync()) stateStr = stateStr.WithAppendedWord("ConnectingAsync",  ", ");
      if (ars->IsConnected())       stateStr = stateStr.WithAppendedWord("Connected",        ", ");
      if (ars->IsExpendable())      stateStr = stateStr.WithAppendedWord("Expendable",       ", ");
      if (ars->IsReadyForInput())   stateStr = stateStr.WithAppendedWord("IsReadyForInput",  ", ");
      if (ars->HasBytesToOutput())  stateStr = stateStr.WithAppendedWord("HasBytesToOutput", ", ");
      if (ars->WasConnected())      stateStr = stateStr.WithAppendedWord("WasConnected",     ", ");
      if (ars->GetMostRecentInputTimeStamp()  != MUSCLE_TIME_NEVER) stateStr = stateStr.WithAppendedWord(String("LastInput: %1 ago").Arg(GetHumanReadableSignedTimeIntervalString( now-ars->GetMostRecentInputTimeStamp(),  1)), ", ");
      if (ars->GetMostRecentOutputTimeStamp() != MUSCLE_TIME_NEVER) stateStr = stateStr.WithAppendedWord(String("LastOutput: %1 ago").Arg(GetHumanReadableSignedTimeIntervalString(now-ars->GetMostRecentOutputTimeStamp(), 1)), ", ");
      if (stateStr.HasChars()) stateStr = stateStr.WithPrepend(", ");

      p.printf("  Session [%s] (rfd=%i,wfd=%i) is [%s]:  (" UINT32_FORMAT_SPEC " outgoing Messages, " UINT32_FORMAT_SPEC " Message-bytes, " UINT32_FORMAT_SPEC " tables, " UINT32_FORMAT_SPEC " nodes, " UINT32_FORMAT_SPEC " node-bytes%s)\n", iter.GetKey()->Cstr(), ars->GetSessionReadSelectSocket().GetFileDescriptor(), ars->GetSessionWriteSelectSocket().GetFileDescriptor(), ars->GetSessionDescriptionString()(), numOutMessages, numOutBytes, numCachedSubscribersTables, numNodes, numNodeBytes, stateStr());
      totalNumCachedSubscribersTables += numCachedSubscribersTables;
      totalNumOutMessages             += numOutMessages;
      totalNumOutBytes                += numOutBytes;
      totalNumNodes                   += numNodes;
      totalNumNodeBytes               += numNodeBytes;
   }
   p.printf("------------------------------------------------------------\n");
   p.printf("Totals: " UINT32_FORMAT_SPEC " outgoing Messages, " UINT32_FORMAT_SPEC " Message-bytes, " UINT32_FORMAT_SPEC " tables, " UINT32_FORMAT_SPEC " nodes, " UINT32_FORMAT_SPEC " node-bytes.\n", totalNumOutMessages, totalNumOutBytes, totalNumCachedSubscribersTables, totalNumNodes, totalNumNodeBytes);
}

} // end namespace muscle
