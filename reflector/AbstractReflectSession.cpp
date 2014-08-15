/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/AbstractReflectSession.h"
#include "reflector/AbstractSessionIOPolicy.h"
#include "reflector/ReflectServer.h"
#include "dataio/TCPSocketDataIO.h"
#include "iogateway/MessageIOGateway.h"
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
   uint32 ret;

   Mutex * ml = GetGlobalMuscleLock();
   MASSERT(ml, "Please instantiate a CompleteSetupSystem object on the stack before creating any session or session-factory objects (at beginning of main() is preferred)\n");

   if (ml->Lock() == B_NO_ERROR) 
   {
      ret = counter++;
      ml->Unlock();
   }
   else
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Could not lock global muscle lock while assigning new ID!!?!\n");
      ret = counter++;  // do it anyway, I guess
   }
   return ret;
}

ReflectSessionFactory :: ReflectSessionFactory()
{
   TCHECKPOINT;
   _id = GetNextGlobalID(_factoryIDCounter);
}

status_t ProxySessionFactory :: AttachedToServer()
{
   if (ReflectSessionFactory::AttachedToServer() != B_NO_ERROR) return B_ERROR;

   status_t ret = B_NO_ERROR;
   if (_slaveRef())
   {
      _slaveRef()->SetOwner(GetOwner());
      ret = _slaveRef()->AttachedToServer();
      if (ret == B_NO_ERROR) _slaveRef()->SetFullyAttachedToServer(true);
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
AbstractReflectSession() : _sessionID(GetNextGlobalID(_sessionIDCounter)), _connectingAsync(false), _isConnected(false), _maxAsyncConnectPeriod(MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS), _asyncConnectTimeoutTime(MUSCLE_TIME_NEVER), _reconnectViaTCP(true), _lastByteOutputAt(0), _maxInputChunk(MUSCLE_NO_LIMIT), _maxOutputChunk(MUSCLE_NO_LIMIT), _outputStallLimit(MUSCLE_TIME_NEVER), _autoReconnectDelay(MUSCLE_TIME_NEVER), _reconnectTime(MUSCLE_TIME_NEVER), _wasConnected(false), _isExpendable(false)
{
   char buf[64]; sprintf(buf, UINT32_FORMAT_SPEC, _sessionID);
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

const ip_address &
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
   return (_gateway()) ? _gateway()->AddOutgoingMessage(ref) : B_ERROR;
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
   ConstSocketRef sock = doTCPConnect ? ConnectAsync(_asyncConnectDest.GetIPAddress(), _asyncConnectDest.GetPort(), isReady) : CreateDefaultSocket();

   // FogBugz #5256:  If ConnectAsync() fails, we want to act as if it succeeded, so that the calling
   //                 code still uses its normal asynchronous-connect-failure code path.  That way the
   //                 caller doesn't have to worry about synchronous failure as a separate case.
   if ((doTCPConnect)&&(sock() == NULL))
   {
      ConstSocketRef tempSockRef;  // tempSockRef represents the closed remote end of the failed connection and is intentionally closed ASAP
      if (CreateConnectedSocketPair(sock, tempSockRef) == B_NO_ERROR) doTCPConnect = false;
   }

   if (sock())
   {
      DataIORef io = CreateDataIO(sock);
      if (io())
      {
         if (_gateway() == NULL)
         {
            _gateway = CreateGateway();
            if (_gateway() == NULL) return B_ERROR;
         }

#ifdef MUSCLE_ENABLE_SSL
         // auto-wrap the user's gateway and socket in the necessary SSL adapters!
         if ((publicKey())&&(dynamic_cast<TCPSocketDataIO *>(io()) != NULL))
         {
            SSLSocketDataIO * ssio = newnothrow SSLSocketDataIO(sock, false, false);
            if (ssio == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
            io.SetRef(ssio);
            if (ssio->SetPublicKeyCertificate(publicKey) != B_NO_ERROR) return B_ERROR;

            if (dynamic_cast<SSLSocketAdapterGateway *>(_gateway()) == NULL) 
            {
               _gateway.SetRef(newnothrow SSLSocketAdapterGateway(_gateway));
               if (_gateway() == NULL) return B_ERROR;
            }
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
   }
   return B_ERROR;
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
   DataIORef dio(newnothrow TCPSocketDataIO(socket, false));
   if (dio() == NULL) WARN_OUT_OF_MEMORY;
   return dio;
}

AbstractMessageIOGatewayRef
AbstractReflectSession ::
CreateGateway()
{
   AbstractMessageIOGateway * gw = newnothrow MessageIOGateway();
   if (gw == NULL) WARN_OUT_OF_MEMORY;
   return AbstractMessageIOGatewayRef(gw);
}

bool
AbstractReflectSession ::
ClientConnectionClosed()
{
   if (_autoReconnectDelay == MUSCLE_TIME_NEVER) return true;  // true == okay to remove this session
   else
   {
      if (_wasConnected) LogTime(MUSCLE_LOG_DEBUG, "%s:  Connection severed, will auto-reconnect in " UINT64_FORMAT_SPEC "mS\n", GetSessionDescriptionString()(), MicrosToMillis(_autoReconnectDelay));
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
GetDefaultHostName() const
{
   return "_unknown_";  // This used to be "<unknown>" but that interfered with MUSCLE's pattern matching that looks for a number range between angle brackets!  --jaf
}

String
AbstractReflectSession ::
GetSessionDescriptionString() const
{
   uint16 port = _ipAddressAndPort.GetPort();

   String ret = GetTypeName();
   ret += " ";
   ret += GetSessionIDString();
   ret += (port>0)?" at [":" to [";
   ret += _hostName;
   char buf[64]; sprintf(buf, ":%u]", (port>0)?port:_asyncConnectDest.GetPort()); ret += buf;
   return ret;
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

int32
AbstractReflectSession ::
DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   return _gateway() ? _gateway()->DoInput(receiver, maxBytes) : 0;
}

int32 
AbstractReflectSession ::
DoOutput(uint32 maxBytes)
{
   TCHECKPOINT;

   return _gateway() ? _gateway()->DoOutput(maxBytes) : 0;
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
   return muscleMin(_reconnectTime, _asyncConnectTimeoutTime);
}

void
AbstractReflectSession :: 
Pulse(const PulseArgs & args)
{
   PulseNode::Pulse(args);
   if (args.GetCallbackTime() >= _reconnectTime)
   {
      if (_autoReconnectDelay == MUSCLE_TIME_NEVER) _reconnectTime = MUSCLE_TIME_NEVER;
      else
      {
         // FogBugz #3810
         if (_wasConnected) LogTime(MUSCLE_LOG_DEBUG, "%s is attempting to auto-reconnect...\n", GetSessionDescriptionString()());
         _reconnectTime = MUSCLE_TIME_NEVER;
         if (Reconnect() != B_NO_ERROR)
         {
            LogTime(MUSCLE_LOG_DEBUG, "%s: Could not auto-reconnect, will try again later...\n", GetSessionDescriptionString()());
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

}; // end namespace muscle
