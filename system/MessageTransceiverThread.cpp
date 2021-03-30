/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/MessageTransceiverThread.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/ReflectServer.h"

namespace muscle {

static status_t FindIPAddressInMessage(const Message & msg, const String & fieldName, IPAddress & ip)
{
   const String * s = NULL;
   if (msg.FindString(fieldName, &s).IsError()) return B_DATA_NOT_FOUND;

   ip = Inet_AtoN(s->Cstr());
   return B_NO_ERROR;
}

static status_t AddIPAddressToMessage(Message & msg, const String & fieldName, const IPAddress & ip)
{
   return msg.AddString(fieldName, Inet_NtoA(ip));
}

MessageTransceiverThread :: MessageTransceiverThread()
   : _forwardAllIncomingMessagesToSupervisor(true)
{
   // empty
}

MessageTransceiverThread :: ~MessageTransceiverThread()
{
   MASSERT(IsInternalThreadRunning() == false, "You must call ShutdownInternalThread() on a MessageTransceiverThread before deleting it!");
   if (_server()) _server()->Cleanup(); 
}

status_t MessageTransceiverThread :: EnsureServerAllocated()
{
   if (_server() == NULL)
   {
      ReflectServerRef server = CreateReflectServer();
      if (server())
      {
         const ConstSocketRef & sock = GetInternalThreadWakeupSocket();
         if (sock())
         {
            ThreadSupervisorSessionRef controlSession = CreateSupervisorSession();
            if (controlSession())
            {
               controlSession()->_mtt = this;
               controlSession()->SetDefaultDistributionPath(GetDefaultDistributionPath());
               if (server()->AddNewSession(controlSession, sock).IsOK())
               {
                  _server = server;
#ifdef MUSCLE_ENABLE_SSL
                  if (_privateKey()) server()->SetSSLPrivateKey(_privateKey);
                  if (_publicKey())  server()->SetSSLPublicKeyCertificate(_publicKey);
                  if (_pskUserName.HasChars()) server()->SetSSLPreSharedKeyLoginInfo(_pskUserName, _pskPassword);
#endif
                  return B_NO_ERROR;
               }
            }
            CloseSockets();  // close the other socket too
         }
         server()->Cleanup();
      }
      return B_ERROR("CreateReflectServer() failed");
   }
   return B_NO_ERROR;
}

ReflectServerRef MessageTransceiverThread :: CreateReflectServer()
{
   ReflectServer * rs = newnothrow ReflectServer;
   if (rs) rs->SetDoLogging(false);  // so that adding/removing client-side sessions won't show up in the log
      else MWARN_OUT_OF_MEMORY;
   return ReflectServerRef(rs);
}

status_t MessageTransceiverThread :: StartInternalThread()
{
   status_t ret;
   return EnsureServerAllocated().IsOK(ret) ? Thread::StartInternalThread() : ret;
}

status_t MessageTransceiverThread :: SendMessageToSessions(const MessageRef & userMsg, const char * optPath)
{
   MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SEND_USER_MESSAGE));
   MRETURN_OOM_ON_NULL(msgRef());

   status_t ret;
   return ((msgRef()->AddMessage(MTT_NAME_MESSAGE, userMsg).IsOK(ret))&&(msgRef()->CAddString(MTT_NAME_PATH, optPath).IsOK(ret))) ? SendMessageToInternalThread(msgRef) : ret;
}

void ThreadWorkerSession :: SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(bool defaultValue)
{
   if (_forwardAllIncomingMessagesToSupervisor.HasValueBeenSet() == false) SetForwardAllIncomingMessagesToSupervisor(defaultValue);
}

status_t MessageTransceiverThread :: AddNewSession(const ConstSocketRef & sock, const ThreadWorkerSessionRef & sessionRef)
{
   status_t ret;
   if (EnsureServerAllocated().IsOK(ret))
   {
      ThreadWorkerSessionRef sRef = sessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
      if (sRef()) sRef()->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
             else return B_ERROR("CreateDefaultWorkerSession() failed");
      return IsInternalThreadRunning() ? SendAddNewSessionMessage(sRef, sock, NULL, invalidIP, 0, false, MUSCLE_TIME_NEVER, MUSCLE_TIME_NEVER) : _server()->AddNewSession(sRef, sock);
   }
   else return ret;
}

status_t MessageTransceiverThread :: AddNewConnectSession(const IPAddress & targetIPAddress, uint16 port, const ThreadWorkerSessionRef & sessionRef, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   status_t ret;
   if (EnsureServerAllocated().IsOK(ret))
   {
      ThreadWorkerSessionRef sRef = sessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
      if (sRef()) sRef()->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
             else return B_ERROR("CreateDefaultWorkerSession() failed");
      return IsInternalThreadRunning() ? SendAddNewSessionMessage(sRef, ConstSocketRef(), NULL, targetIPAddress, port, false, autoReconnectDelay, maxAsyncConnectPeriod) : _server()->AddNewConnectSession(sRef, targetIPAddress, port, autoReconnectDelay, maxAsyncConnectPeriod);
   }
   else return ret;
}

status_t MessageTransceiverThread :: AddNewConnectSession(const String & targetHostName, uint16 port, const ThreadWorkerSessionRef & sessionRef, bool expandLocalhost, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   status_t ret;
   if (EnsureServerAllocated().IsOK(ret))
   {
      ThreadWorkerSessionRef sRef = sessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
      if (sRef())
      {
         sRef()->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
         if (IsInternalThreadRunning()) return SendAddNewSessionMessage(sRef, ConstSocketRef(), targetHostName(), 0, port, expandLocalhost, autoReconnectDelay, maxAsyncConnectPeriod);
         else
         {
            const IPAddress ip = GetHostByName(targetHostName(), expandLocalhost);
            return (ip != invalidIP) ? _server()->AddNewConnectSession(sRef, ip, port, autoReconnectDelay, maxAsyncConnectPeriod) : B_ERROR("GetHostByName() failed");
         }
      }
      else return B_ERROR("CreateDefaultWorkerSession() failed");
   }
   else return ret;
}

status_t MessageTransceiverThread :: SendAddNewSessionMessage(const ThreadWorkerSessionRef & sessionRef, const ConstSocketRef & sock, const char * hostName, const IPAddress & hostIP, uint16 port, bool expandLocalhost, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   if (sessionRef() == NULL) return B_BAD_ARGUMENT;

   MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_ADD_NEW_SESSION));
   MRETURN_OOM_ON_NULL(msgRef());

   status_t ret;
   return (((hostIP == invalidIP)||(AddIPAddressToMessage(*msgRef(), MTT_NAME_IP_ADDRESS, hostIP)  .IsOK(ret)))&&
       (msgRef()->AddTag(    MTT_NAME_SESSION,            sessionRef)                              .IsOK(ret)) &&
       (msgRef()->CAddString(MTT_NAME_HOSTNAME,           hostName)                                .IsOK(ret)) &&
       (msgRef()->CAddInt16( MTT_NAME_PORT,               port)                                    .IsOK(ret)) &&
       (msgRef()->CAddBool(  MTT_NAME_EXPANDLOCALHOST,    expandLocalhost)                         .IsOK(ret)) &&
       (msgRef()->CAddTag(   MTT_NAME_SOCKET,             CastAwayConstFromRef(sock))              .IsOK(ret)) &&
       (msgRef()->CAddInt64( MTT_NAME_AUTORECONNECTDELAY, autoReconnectDelay,    MUSCLE_TIME_NEVER).IsOK(ret)) &&
       (msgRef()->CAddInt64( MTT_NAME_MAXASYNCCONNPERIOD, maxAsyncConnectPeriod, MUSCLE_TIME_NEVER).IsOK(ret))) ? SendMessageToInternalThread(msgRef) : ret;
}

status_t MessageTransceiverThread :: PutAcceptFactory(uint16 port, const ThreadWorkerSessionFactoryRef & factoryRef, const IPAddress & optInterfaceIP, uint16 * optRetPort)
{
   status_t ret;
   if (EnsureServerAllocated().IsOK(ret))
   {
      ThreadWorkerSessionFactoryRef fRef = factoryRef;
      if (fRef() == NULL) fRef = CreateDefaultSessionFactory();
      if (fRef())
      {
         fRef()->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
         if (IsInternalThreadRunning())
         {
            MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_PUT_ACCEPT_FACTORY));
            MRETURN_OOM_ON_NULL(msgRef());
            if ((msgRef()->AddInt16(MTT_NAME_PORT, port)                              .IsOK(ret)) &&
                (msgRef()->AddTag(MTT_NAME_FACTORY, fRef)                             .IsOK(ret)) &&
                (AddIPAddressToMessage(*msgRef(), MTT_NAME_IP_ADDRESS, optInterfaceIP).IsOK(ret)) &&
                (SendMessageToInternalThread(msgRef)                                  .IsOK(ret))) return B_NO_ERROR;
         }
         else if (_server()->PutAcceptFactory(port, fRef, optInterfaceIP, optRetPort).IsOK(ret)) return B_NO_ERROR;
      }
      else return B_ERROR("CreateDefaultSessionFactory() failed");
   }
   return ret;
}

status_t MessageTransceiverThread :: RemoveAcceptFactory(uint16 port, const IPAddress & optInterfaceIP)
{
   if (_server())
   {
      if (IsInternalThreadRunning())
      {
         MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_REMOVE_ACCEPT_FACTORY));
         MRETURN_OOM_ON_NULL(msgRef());

         status_t ret;
         return ((msgRef()->AddInt16(MTT_NAME_PORT, port).IsOK(ret))&&
                 (AddIPAddressToMessage(*msgRef(), MTT_NAME_IP_ADDRESS, optInterfaceIP).IsOK(ret))) ? SendMessageToInternalThread(msgRef) : ret;
      }
      else return _server()->RemoveAcceptFactory(port, optInterfaceIP);
   }
   else return B_NO_ERROR;  // if there's no server, there's no port
}

#ifdef MUSCLE_ENABLE_SSL

status_t MessageTransceiverThread :: SetSSLPrivateKey(const ConstByteBufferRef & privateKey)
{
   _privateKey = privateKey;

   if (IsInternalThreadRunning())
   {
      MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SET_SSL_PRIVATE_KEY));
      MRETURN_OOM_ON_NULL(msgRef());

      status_t ret;
      return (((_privateKey())&&(msgRef()->AddFlat(MTT_NAME_DATA, CastAwayConstFromRef(privateKey)).IsError(ret)))||(SendMessageToInternalThread(msgRef).IsError(ret))) ? ret : B_NO_ERROR;
   }
   else return B_BAD_OBJECT;
}

status_t MessageTransceiverThread :: SetSSLPublicKeyCertificate(const ConstByteBufferRef & publicKey)
{
   _publicKey = publicKey;

   if (IsInternalThreadRunning())
   {
      MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SET_SSL_PUBLIC_KEY));
      MRETURN_OOM_ON_NULL(msgRef());

      status_t ret;
      return (((_publicKey())&&(msgRef()->AddFlat(MTT_NAME_DATA, CastAwayConstFromRef(_publicKey)).IsError(ret)))||(SendMessageToInternalThread(msgRef).IsError(ret))) ? ret : B_NO_ERROR;
   }
   else return B_BAD_OBJECT;
}

status_t MessageTransceiverThread :: SetSSLPreSharedKeyLoginInfo(const String & userName, const String & password)
{
   _pskUserName = userName;
   _pskPassword = password;

   if (IsInternalThreadRunning())
   {
      MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SET_SSL_PSK_INFO));
      MRETURN_OOM_ON_NULL(msgRef());

      status_t ret;
      return ((msgRef()->AddString(MTT_NAME_DATA, _pskUserName).IsError(ret))||(msgRef()->AddString(MTT_NAME_DATA, _pskPassword).IsError(ret))||(SendMessageToInternalThread(msgRef).IsError(ret))) ? ret : B_NO_ERROR;
   }
   else return B_BAD_OBJECT;
}

#endif

status_t MessageTransceiverThread :: SetDefaultDistributionPath(const String & path)
{
   if (IsInternalThreadRunning())
   {
      MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SET_DEFAULT_PATH));
      MRETURN_OOM_ON_NULL(msgRef());
      MRETURN_ON_ERROR(msgRef()->AddString(MTT_NAME_PATH, path));
      MRETURN_ON_ERROR(SendMessageToInternalThread(msgRef));
   }
   else _defaultDistributionPath = path;

   return B_NO_ERROR;
}

int32 MessageTransceiverThread :: GetNextEventFromInternalThread(uint32 & code, MessageRef * optRetRef, String * optFromSession, uint32 * optFromFactoryID, IPAddressAndPort * optLocation)
{
   // First, default values for everyone
   if (optRetRef)        optRetRef->Reset();
   if (optFromSession)   optFromSession->Clear();
   if (optFromFactoryID) *optFromFactoryID = 0;

   MessageRef msgRef;
   int32 ret = GetNextReplyFromInternalThread(msgRef);
   if (ret >= 0)
   {
      if (msgRef())
      {
         code = msgRef()->what;
         if ((optRetRef)&&(msgRef()->FindMessage(MTT_NAME_MESSAGE, *optRetRef).IsError())) *optRetRef = msgRef;
         if (optFromSession)   (void) msgRef()->FindString(MTT_NAME_FROMSESSION, *optFromSession);
         if (optFromFactoryID) (void) msgRef()->FindInt32(MTT_NAME_FACTORY_ID, optFromFactoryID);
         if (optLocation)
         {
            const String * s;
            if (msgRef()->FindString(MTT_NAME_LOCATION, &s).IsOK()) optLocation->SetFromString(*s, 0, false);
         }
      }
      else ret = -1;  // NULL event message should never happen, but just in case
   }
   return ret;
}

status_t MessageTransceiverThread :: RequestOutputQueuesDrainedNotification(const MessageRef & notifyRef, const char * optDistPath, DrainTag * optDrainTag)
{
   // Send a command to the supervisor letting him know we are waiting for him to assure
   // us that all the matching worker sessions have dequeued their messages.  Preallocate
   // as much as possible so we won't have to worry about out-of-memory later on, when
   // it's too late to handle it properly.
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_NOTIFY_ON_OUTPUT_DRAIN);
   MessageRef replyRef   = GetMessageFromPool(MTT_EVENT_OUTPUT_QUEUES_DRAINED);
   if ((commandRef() == NULL)||(replyRef() == NULL)) MRETURN_OUT_OF_MEMORY;

   status_t ret;
   if (replyRef()->CAddMessage(MTT_NAME_MESSAGE, notifyRef).IsOK(ret))
   {
      DrainTagRef drainTagRef(optDrainTag ? optDrainTag : newnothrow DrainTag);
      if (drainTagRef()) drainTagRef()->SetReplyMessage(replyRef);
      if ((drainTagRef())&&
          (commandRef()->CAddString(MTT_NAME_PATH, optDistPath) .IsOK(ret)) &&
          (commandRef()->AddTag(MTT_NAME_DRAIN_TAG, drainTagRef).IsOK(ret)) &&
          (SendMessageToInternalThread(commandRef)              .IsOK(ret))) return B_NO_ERROR;

      // User keeps ownership of his custom DrainTag on error, so we don't delete it.
      if ((drainTagRef())&&(drainTagRef() == optDrainTag)) 
      {
         drainTagRef()->SetReplyMessage(MessageRef());
         drainTagRef.Neutralize();
      }
   }
   return ret;
}

status_t MessageTransceiverThread :: SetNewInputPolicy(const AbstractSessionIOPolicyRef & pref, const char * optDistPath)
{
   return SetNewPolicyAux(MTT_COMMAND_SET_INPUT_POLICY, pref, optDistPath);
}

status_t MessageTransceiverThread :: SetNewOutputPolicy(const AbstractSessionIOPolicyRef & pref, const char * optDistPath)
{
   return SetNewPolicyAux(MTT_COMMAND_SET_OUTPUT_POLICY, pref, optDistPath);
}

status_t MessageTransceiverThread :: SetNewPolicyAux(uint32 what, const AbstractSessionIOPolicyRef & pref, const char * optDistPath)    
{
   MessageRef commandRef = GetMessageFromPool(what);
   MRETURN_OOM_ON_NULL(commandRef());

   status_t ret;
   return ((commandRef()->CAddString(MTT_NAME_PATH, optDistPath).IsOK(ret))&&(commandRef()->CAddTag(MTT_NAME_POLICY_TAG, pref).IsOK(ret))) ? SendMessageToInternalThread(commandRef) : ret;
}

status_t MessageTransceiverThread :: SetOutgoingMessageEncoding(int32 encoding, const char * optDistPath)
{
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_SET_OUTGOING_ENCODING);
   MRETURN_OOM_ON_NULL(commandRef());

   status_t ret;
   return ((commandRef()->CAddString(MTT_NAME_PATH, optDistPath).IsOK(ret))&&(commandRef()->AddInt32(MTT_NAME_ENCODING, encoding).IsOK(ret))) ? SendMessageToInternalThread(commandRef) : ret;
}

status_t MessageTransceiverThread :: RemoveSessions(const char * optDistPath)
{
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_REMOVE_SESSIONS);
   MRETURN_OOM_ON_NULL(commandRef());
 
   status_t ret;
   return (commandRef()->CAddString(MTT_NAME_PATH, optDistPath).IsOK(ret)) ? SendMessageToInternalThread(commandRef) : ret;
}

void MessageTransceiverThread :: Reset()
{
   ShutdownInternalThread();
   if (_server())
   {
      _server()->Cleanup();
      _server.Reset();
   }
   
   // Clear both message queues of any leftover messages.
   MessageRef junk;
   while(WaitForNextMessageFromOwner(junk, 0) >= 0) {/* empty */}
   while(GetNextReplyFromInternalThread(junk) >= 0) {/* empty */}
}

ThreadSupervisorSessionRef MessageTransceiverThread :: CreateSupervisorSession()
{
   ThreadSupervisorSession * ret = newnothrow ThreadSupervisorSession();
   if (ret == NULL) MWARN_OUT_OF_MEMORY;
   return ThreadSupervisorSessionRef(ret);
}

ThreadWorkerSessionRef MessageTransceiverThread :: CreateDefaultWorkerSession()
{
   ThreadWorkerSession * ret = newnothrow ThreadWorkerSession();
   if (ret == NULL) MWARN_OUT_OF_MEMORY;
   return ThreadWorkerSessionRef(ret);
}

ThreadWorkerSessionFactoryRef MessageTransceiverThread :: CreateDefaultSessionFactory()
{
   ThreadWorkerSessionFactory * ret = newnothrow ThreadWorkerSessionFactory();
   if (ret == NULL) MWARN_OUT_OF_MEMORY;
   return ThreadWorkerSessionFactoryRef(ret);
}

ThreadWorkerSessionFactory :: ThreadWorkerSessionFactory()
   : _forwardAllIncomingMessagesToSupervisor(true)
{
   // empty
}

status_t ThreadWorkerSessionFactory :: AttachedToServer()
{
   status_t ret;
   return (StorageReflectSessionFactory::AttachedToServer().IsOK(ret)) ? SendMessageToSupervisorSession(GetMessageFromPool(MTT_EVENT_FACTORY_ATTACHED)) : ret;
}

void ThreadWorkerSessionFactory :: AboutToDetachFromServer()
{
   (void) SendMessageToSupervisorSession(GetMessageFromPool(MTT_EVENT_FACTORY_DETACHED));
   StorageReflectSessionFactory::AboutToDetachFromServer();
}

status_t ThreadWorkerSessionFactory :: SendMessageToSupervisorSession(const MessageRef & msg, void * userData)
{
   // I'm not bothering to cache the supervisor pointer for this class, because this method is called 
   // so rarely and I can't be bothered to add anti-dangling-pointer logic for so little gain --jaf
   ThreadSupervisorSession * supervisorSession = FindFirstSessionOfType<ThreadSupervisorSession>();
   if (supervisorSession)
   {
      supervisorSession->MessageReceivedFromFactory(*this, msg, userData);
      return B_NO_ERROR;
   }
   else return B_BAD_OBJECT;
}

void ThreadWorkerSessionFactory :: SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(bool defaultValue)
{
   if (_forwardAllIncomingMessagesToSupervisor.HasValueBeenSet() == false) SetForwardAllIncomingMessagesToSupervisor(defaultValue);
}

ThreadWorkerSessionRef ThreadWorkerSessionFactory :: CreateThreadWorkerSession(const String &, const IPAddressAndPort &)
{
   ThreadWorkerSession * ret = newnothrow ThreadWorkerSession();
   if (ret == NULL) MWARN_OUT_OF_MEMORY;
   return ThreadWorkerSessionRef(ret);
}

AbstractReflectSessionRef ThreadWorkerSessionFactory :: CreateSession(const String & clientHostIP, const IPAddressAndPort & iap)
{
   ThreadWorkerSessionRef tws = CreateThreadWorkerSession(clientHostIP, iap);
   if ((tws())&&(SetMaxIncomingMessageSizeFor(tws()).IsOK()))
   {
      tws()->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
      tws()->_acceptedIAP = iap;  // gotta send the MTT_EVENT_SESSION_ACCEPTED Message from within AttachedToServer()
      return tws;
   }
   return AbstractReflectSessionRef();
}

void MessageTransceiverThread :: InternalThreadEntry()
{
   if (_server()) 
   {
      (void) _server()->ServerProcessLoop();
      _server()->Cleanup();
   }
   SendMessageToOwner(GetMessageFromPool(MTT_EVENT_SERVER_EXITED));
}

ThreadWorkerSession :: ThreadWorkerSession()
   : _forwardAllIncomingMessagesToSupervisor(true)
   , _supervisorSession(NULL)
{
   // empty
}

ThreadWorkerSession :: ~ThreadWorkerSession()
{
   // empty
}

void ThreadWorkerSession :: AsyncConnectCompleted()
{
   StorageReflectSession::AsyncConnectCompleted();

   MessageRef msg = GetMessageFromPool(MTT_EVENT_SESSION_CONNECTED);
   if ((msg())&&(msg()->AddString(MTT_NAME_LOCATION, IPAddressAndPort(GetAsyncConnectIP(), GetAsyncConnectPort()).ToString()).IsOK())) (void) SendMessageToSupervisorSession(msg);
}

status_t ThreadWorkerSession :: AttachedToServer()
{
   MRETURN_ON_ERROR(StorageReflectSession::AttachedToServer());

   if (_acceptedIAP.IsValid())
   {
      MessageRef msg = GetMessageFromPool(MTT_EVENT_SESSION_ACCEPTED);
      MRETURN_OOM_ON_NULL(msg());
      MRETURN_ON_ERROR(msg()->AddString(MTT_NAME_LOCATION, _acceptedIAP.ToString()));
      MRETURN_ON_ERROR(SendMessageToSupervisorSession(msg));
   }
   return SendMessageToSupervisorSession(GetMessageFromPool(MTT_EVENT_SESSION_ATTACHED));
}

status_t ThreadWorkerSession :: SendMessageToSupervisorSession(const MessageRef & msg, void * userData)
{
   if (_supervisorSession == NULL) _supervisorSession = FindFirstSessionOfType<ThreadSupervisorSession>();
   if (_supervisorSession)
   {
      _supervisorSession->MessageReceivedFromSession(*this, msg, userData);
      return B_NO_ERROR;
   }
   else return B_BAD_OBJECT;
}

bool ThreadWorkerSession :: ClientConnectionClosed()
{
   (void) SendMessageToSupervisorSession(GetMessageFromPool(MTT_EVENT_SESSION_DISCONNECTED));
   _drainedNotifiers.Clear();
   return StorageReflectSession::ClientConnectionClosed();
}

void ThreadWorkerSession :: AboutToDetachFromServer()
{
   (void) SendMessageToSupervisorSession(GetMessageFromPool(MTT_EVENT_SESSION_DETACHED));
   _drainedNotifiers.Clear();
   _supervisorSession = NULL;  // paranoia
   StorageReflectSession::AboutToDetachFromServer();
}

int32 ThreadWorkerSession :: DoOutput(uint32 maxBytes)
{
   const int32 ret = StorageReflectSession::DoOutput(maxBytes);
   if (_drainedNotifiers.HasItems())
   {
      AbstractMessageIOGateway * gw = GetGateway()();
      if ((gw == NULL)||(gw->HasBytesToOutput() == false)) _drainedNotifiers.Clear();
   }
   return ret;
}

void ThreadWorkerSession :: MessageReceivedFromGateway(const MessageRef & msg, void * userData)
{
   if (_forwardAllIncomingMessagesToSupervisor)
   {
      // Wrap it up so the supervisor knows its for him, and send it out
      MessageRef wrapper = GetMessageFromPool(MTT_EVENT_INCOMING_MESSAGE);
      if ((wrapper())&&(wrapper()->AddMessage(MTT_NAME_MESSAGE, msg).IsOK())) (void) SendMessageToSupervisorSession(wrapper, userData);
   }
   else StorageReflectSession::MessageReceivedFromGateway(msg, userData);
}

void ThreadWorkerSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData)
{
   const Message * msg = msgRef();
   if (msg)
   {
      if ((msg->what >= MTT_COMMAND_SEND_USER_MESSAGE)&&(msg->what <= MTT_LAST_COMMAND))
      {
         switch(msg->what)
         {
            case MTT_COMMAND_NOTIFY_ON_OUTPUT_DRAIN:
            {
               RefCountableRef genericRef;
               if (msg->FindTag(MTT_NAME_DRAIN_TAG, genericRef).IsOK())
               {
                  DrainTagRef drainTagRef(genericRef, true);
                  if (drainTagRef())
                  {
                     // Add our session ID so that the supervisor session will know we received the drain tag
                     Message * rmsg = drainTagRef()->GetReplyMessage()();
                     if (rmsg) rmsg->AddString(MTT_NAME_FROMSESSION, GetSessionRootPath());

                     // If we have any messages pending, we'll save this message reference until our
                     // outgoing message queue becomes empty.  That way the DrainTag item held by the 
                     // referenced message won't be deleted until the appropriate time, and hence
                     // the supervisor won't be notified until all the specified queues have drained.
                     AbstractMessageIOGateway * gw = GetGateway()();
                     if ((gw)&&(gw->HasBytesToOutput())) _drainedNotifiers.AddTail(drainTagRef);
                  }
               }
            }
            break;

            case MTT_COMMAND_SEND_USER_MESSAGE:
            {
               MessageRef userMsg;
               if (msg->FindMessage(MTT_NAME_MESSAGE, userMsg).IsOK()) AddOutgoingMessage(userMsg);
            }
            break;

            case MTT_COMMAND_SET_INPUT_POLICY:
            case MTT_COMMAND_SET_OUTPUT_POLICY:
            {
               RefCountableRef tagRef;
               (void) msg->FindTag(MTT_NAME_POLICY_TAG, tagRef);
               AbstractSessionIOPolicyRef pref(tagRef, true);
               if (msg->what == MTT_COMMAND_SET_INPUT_POLICY) SetInputPolicy(pref);
                                                         else SetOutputPolicy(pref);
            }
            break;

            case MTT_COMMAND_SET_OUTGOING_ENCODING:
            {
               int32 enc;
               if (msg->FindInt32(MTT_NAME_ENCODING, enc).IsOK())
               {
                  MessageIOGateway * gw = dynamic_cast<MessageIOGateway *>(GetGateway()());
                  if (gw) gw->SetOutgoingEncoding(enc);
               }
            }
            break;

            case MTT_COMMAND_REMOVE_SESSIONS:
               EndSession();
            break;
         }
      }
      else if ((msg->what >= MTT_EVENT_INCOMING_MESSAGE)&&(msg->what <= MTT_LAST_EVENT)) 
      {
         // ignore these; we don't care about silly MTT_EVENTS, those are for the supervisor and the user
      }
      else StorageReflectSession::MessageReceivedFromSession(from, msgRef, userData);
   }
}

ThreadSupervisorSession :: ThreadSupervisorSession()
   : _mtt(NULL)
{
   // empty
}

ThreadSupervisorSession :: ~ThreadSupervisorSession()
{
   // empty
}

void ThreadSupervisorSession :: AboutToDetachFromServer()
{
   // Neutralize all outstanding DrainTrags so that they won't try to call DrainTagIsBeingDeleted() on me after I'm gone.
   for (HashtableIterator<DrainTag *, Void> tagIter(_drainTags); tagIter.HasData(); tagIter++) tagIter.GetKey()->SetNotify(NULL);

   // Nerf any ThreadWorkerSessions' cached pointers to us so they won't dangle
   Queue<AbstractReflectSessionRef> workers;
   if (FindSessionsOfType<ThreadWorkerSession>(workers).IsOK())
   {
      for (uint32 i=0; i<workers.GetNumItems(); i++) 
      {
         ThreadWorkerSession * ws = static_cast<ThreadWorkerSession *>(workers[i]());
         if (ws->_supervisorSession == this) ws->_supervisorSession = NULL;
      }
   }
 
   StorageReflectSession :: AboutToDetachFromServer();
}

void ThreadSupervisorSession :: DrainTagIsBeingDeleted(DrainTag * tag)
{
   if (_drainTags.Remove(tag).IsOK()) _mtt->SendMessageToOwner(tag->GetReplyMessage());
}

AbstractMessageIOGatewayRef ThreadSupervisorSession :: CreateGateway()
{
   AbstractMessageIOGateway * gw = newnothrow SignalMessageIOGateway();
   if (gw == NULL) MWARN_OUT_OF_MEMORY;
   return AbstractMessageIOGatewayRef(gw);
}

void ThreadSupervisorSession :: MessageReceivedFromGateway(const MessageRef &, void *)
{
   // The message from the gateway is merely a signal that we should check
   // the message queue from the main thread again, to see if there are
   // new messages from our owner waiting.  So we'll do that here.
   MessageRef msgFromOwner;
   int32 numLeft;
   while((numLeft = _mtt->WaitForNextMessageFromOwner(msgFromOwner, 0)) >= 0)
   {
      if (msgFromOwner()) MessageReceivedFromOwner(msgFromOwner, (uint32)numLeft);
      else
      {
         EndServer();  // this will cause our thread to exit
         break;
      }
   }
}

void ThreadSupervisorSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void *)
{
   if (msgRef()) (void) msgRef()->AddString(MTT_NAME_FROMSESSION, from.GetSessionRootPath());
   _mtt->SendMessageToOwner(msgRef);
}

void ThreadSupervisorSession :: MessageReceivedFromFactory(ReflectSessionFactory & from, const MessageRef & msgRef, void *)
{
   if (msgRef()) msgRef()->AddInt32(MTT_NAME_FACTORY_ID, from.GetFactoryID());
   _mtt->SendMessageToOwner(msgRef);
}

bool ThreadSupervisorSession :: ClientConnectionClosed()
{
   EndServer();
   return StorageReflectSession::ClientConnectionClosed();
}

status_t ThreadSupervisorSession :: AddNewWorkerConnectSession(const ThreadWorkerSessionRef & sessionRef, const IPAddress & hostIP, uint16 port, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   const status_t ret = (hostIP != invalidIP) ? AddNewConnectSession(sessionRef, hostIP, port, autoReconnectDelay, maxAsyncConnectPeriod) : B_BAD_ARGUMENT;

   // For immediate failure: Since (sessionRef) never attached, we need to send the disconnect message ourself.
   if ((ret.IsError())&&(sessionRef()))
   {
      // We have to synthesize the MTT_NAME_FROMSESSION path ourselves, since the session was never added to the server and thus its path isn't set
      MessageRef errorMsg = GetMessageFromPool(MTT_EVENT_SESSION_DISCONNECTED);
      if ((errorMsg())&&(errorMsg()->AddString(MTT_NAME_FROMSESSION, String("/%1/%2").Arg(Inet_NtoA(hostIP)).Arg(sessionRef()->GetSessionID())).IsOK())) _mtt->SendMessageToOwner(errorMsg);
   }
   return ret;
}

void ThreadSupervisorSession :: SendMessageToWorkers(const MessageRef & distMsg)
{
   String distPath;
   SendMessageToMatchingSessions(distMsg, (distMsg()->FindString(MTT_NAME_PATH, distPath).IsOK()) ? distPath : _defaultDistributionPath, ConstQueryFilterRef(), false); 
}

status_t ThreadSupervisorSession :: MessageReceivedFromOwner(const MessageRef & msgRef, uint32)
{
   const Message * msg = msgRef();
   if (msg)
   {
      if (muscleInRange(msg->what, (uint32) MTT_COMMAND_SEND_USER_MESSAGE, (uint32) (MTT_LAST_COMMAND-1)))
      {
         switch(msg->what)
         {
            case MTT_COMMAND_ADD_NEW_SESSION:
            {
               RefCountableRef tagRef;
               if (msg->FindTag(MTT_NAME_SESSION, tagRef).IsOK())
               {
                  ThreadWorkerSessionRef sessionRef(tagRef, true);
                  if (sessionRef())
                  {
                     const char * hostName;
                     IPAddress hostIP;
                     const uint16 port                  = msg->GetInt16(MTT_NAME_PORT);
                     const uint64 autoReconnectDelay    = msg->GetInt64(MTT_NAME_AUTORECONNECTDELAY, MUSCLE_TIME_NEVER);
                     const uint64 maxAsyncConnectPeriod = msg->GetInt64(MTT_NAME_MAXASYNCCONNPERIOD, MUSCLE_TIME_NEVER);

                          if (FindIPAddressInMessage(*msg, MTT_NAME_IP_ADDRESS, hostIP).IsOK()) (void) AddNewWorkerConnectSession(sessionRef, hostIP, port, autoReconnectDelay, maxAsyncConnectPeriod);
                     else if (msg->FindString(MTT_NAME_HOSTNAME, &hostName)            .IsOK()) (void) AddNewWorkerConnectSession(sessionRef, GetHostByName(hostName, msg->GetBool(MTT_NAME_EXPANDLOCALHOST)), port, autoReconnectDelay, maxAsyncConnectPeriod);
                     else                                                                       (void) AddNewSession(sessionRef, ConstSocketRef(msg->GetTag(MTT_NAME_SOCKET), true));
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "MTT_COMMAND_PUT_ACCEPT_FACTORY:  Could not get Session!\n");
               }
            }
            break;

            case MTT_COMMAND_PUT_ACCEPT_FACTORY:
            {
               RefCountableRef tagRef;
               if (msg->FindTag(MTT_NAME_FACTORY, tagRef).IsOK())
               {
                  ThreadWorkerSessionFactoryRef factoryRef(tagRef, true);
                  if (factoryRef())
                  {
                     const uint16 port = msg->GetInt16(MTT_NAME_PORT);
                     IPAddress ip = invalidIP; (void) FindIPAddressInMessage(*msg, MTT_NAME_IP_ADDRESS, ip);
                     (void) PutAcceptFactory(port, factoryRef, ip);
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "MTT_COMMAND_PUT_ACCEPT_FACTORY:  Could not get ReflectSessionFactory!\n");
               }
            }
            break;

            case MTT_COMMAND_REMOVE_ACCEPT_FACTORY:
            {
               uint16 port;
               IPAddress ip;
               if ((msg->FindInt16(MTT_NAME_PORT, port).IsOK())&&(FindIPAddressInMessage(*msg, MTT_NAME_IP_ADDRESS, ip).IsOK())) (void) RemoveAcceptFactory(port, ip);
            }
            break;

            case MTT_COMMAND_SET_DEFAULT_PATH:
               _defaultDistributionPath = msg->GetString(MTT_NAME_PATH);
            break;

            case MTT_COMMAND_NOTIFY_ON_OUTPUT_DRAIN:
            {
               RefCountableRef genericRef;
               if (msg->FindTag(MTT_NAME_DRAIN_TAG, genericRef).IsOK())
               {
                  DrainTagRef drainTagRef(genericRef, true);
                  if ((drainTagRef())&&(_drainTags.PutWithDefault(drainTagRef()).IsOK()))
                  {
                     drainTagRef()->SetNotify(this);
                     SendMessageToWorkers(msgRef);

                     // Check the tag to see if anyone got it.  If not, we'll add the
                     // PR_NAME_KEY string to the reply field, to give the user thread
                     // a hint about which handler the reply should be directed back to.
                     Message * rmsg = drainTagRef()->GetReplyMessage()();
                     if ((rmsg)&&(rmsg->HasName(MTT_NAME_FROMSESSION) == false))
                     {
                        String t;
                        if (msg->FindString(MTT_NAME_PATH, t).IsOK()) (void) rmsg->AddString(MTT_NAME_FROMSESSION, t);
                     }
                  }
               }
            }
            break;

#ifdef MUSCLE_ENABLE_SSL
            case MTT_COMMAND_SET_SSL_PRIVATE_KEY:
               _mtt->_server()->SetSSLPrivateKey(msg->GetFlat(MTT_NAME_DATA));
            break;

            case MTT_COMMAND_SET_SSL_PUBLIC_KEY:
               _mtt->_server()->SetSSLPublicKeyCertificate(msg->GetFlat(MTT_NAME_DATA));
            break;

            case MTT_COMMAND_SET_SSL_PSK_INFO:
               _mtt->_server()->SetSSLPreSharedKeyLoginInfo(msg->GetString(MTT_NAME_DATA, GetEmptyString(), 0), msg->GetString(MTT_NAME_DATA, GetEmptyString(), 1));
            break;
#endif

            default:
               SendMessageToWorkers(msgRef);
            break;
         }
      }
      else StorageReflectSession::MessageReceivedFromGateway(msgRef, NULL);

      return B_NO_ERROR;
   }
   else return B_BAD_ARGUMENT;
}

DrainTag :: ~DrainTag() 
{
   if (_notify) _notify->DrainTagIsBeingDeleted(this);
}

} // end namespace muscle
