/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/MessageTransceiverThread.h"
#include "iogateway/SignalMessageIOGateway.h"
#include "iogateway/MessageIOGateway.h"
#include "reflector/ReflectServer.h"

namespace muscle {

MessageTransceiverThread :: MessageTransceiverThread(ICallbackMechanism * optCallbackMechanism)
   : Thread(true, optCallbackMechanism)
   , _forwardAllIncomingMessagesToSupervisor(true)
{
   // empty
}

MessageTransceiverThread :: ~MessageTransceiverThread()
{
   MASSERT(IsInternalThreadRunning() == false, "You must call ShutdownInternalThread() on a MessageTransceiverThread before deleting it!");
   if (_server()) _server()->Cleanup();
}

#ifndef MUSCLE_ENABLE_SSL
static status_t ComplainAboutNoSSL(const char * funcName)
{
   LogTime(MUSCLE_LOG_CRITICALERROR, "MessageTransceiverThread::EnsureServerAllocated():  Can't call %s, because MUSCLE was compiled without -DMUSCLE_ENABLE_SSL\n", funcName);
   return B_UNIMPLEMENTED;
}
#endif

status_t MessageTransceiverThread :: EnsureServerAllocated()
{
   if (_server()) return B_NO_ERROR;  // already allocated, nothing for us to do

   status_t ret;
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
            if (server()->AddNewSession(controlSession, sock).IsOK(ret))
            {
#ifdef MUSCLE_ENABLE_SSL
               if (_privateKey())           server()->SetSSLPrivateKey(_privateKey);
               if (_publicKey())            server()->SetSSLPublicKeyCertificate(_publicKey);
               if (_pskUserName.HasChars()) server()->SetSSLPreSharedKeyLoginInfo(_pskUserName, _pskPassword);
#else
               if (_privateKey())           ret |= ComplainAboutNoSSL("SetSSLPrivateKey()");
               if (_publicKey())            ret |= ComplainAboutNoSSL("SetSSLPublicKeyCertificate()");
               if (_pskUserName.HasChars()) ret |= ComplainAboutNoSSL("SetSSLPreSharedKeyLoginInfo()");
#endif
               if (ret.IsOK())
               {
                  _server = server;
                  return ret;
               }
            }
         }
         CloseSockets();  // close the other socket too
      }
      server()->Cleanup();
   }

   return ret | B_ERROR("CreateReflectServer() failed");
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

status_t MessageTransceiverThread :: SendMessageToSessions(const MessageRef & userMsg, const String & optPath)
{
   MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_SEND_USER_MESSAGE));
   MRETURN_ON_ERROR(msgRef);

   status_t ret;
   return ((msgRef()->AddMessage(MTT_NAME_MESSAGE, userMsg).IsOK(ret))&&(msgRef()->CAddString(MTT_NAME_PATH, optPath).IsOK(ret))) ? SendMessageToInternalThread(msgRef) : ret;
}

void ThreadWorkerSession :: SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(bool defaultValue)
{
   if (_forwardAllIncomingMessagesToSupervisor.HasValueBeenSet() == false) SetForwardAllIncomingMessagesToSupervisor(defaultValue);
}

status_t MessageTransceiverThread :: AddNewSession(const ConstSocketRef & sock, const AbstractReflectSessionRef & sessionRef)
{
   MRETURN_ON_ERROR(EnsureServerAllocated());

   AbstractReflectSessionRef sRef = sessionRef;
   if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
   if (sRef())
   {
       ThreadWorkerSession * tws = dynamic_cast<ThreadWorkerSession *>(sRef());
       if (tws) tws->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
   }
   else return B_ERROR("CreateDefaultWorkerSession() failed");

   return IsInternalThreadRunning() ? SendAddNewSessionMessage(sRef, sock, NULL, IPAddressAndPort(), false, MUSCLE_TIME_NEVER, MUSCLE_TIME_NEVER) : _server()->AddNewSession(sRef, sock);
}

status_t MessageTransceiverThread :: AddNewConnectSession(const IPAddressAndPort & targetIPAddressAndPort, const AbstractReflectSessionRef & sessionRef, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   MRETURN_ON_ERROR(EnsureServerAllocated());

   AbstractReflectSessionRef sRef = sessionRef;
   if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
   if (sRef())
   {
      ThreadWorkerSession * tws = dynamic_cast<ThreadWorkerSession *>(sRef());
      if (tws) tws->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);
   }
   else return B_ERROR("CreateDefaultWorkerSession() failed");

   return IsInternalThreadRunning() ? SendAddNewSessionMessage(sRef, ConstSocketRef(), NULL, targetIPAddressAndPort, false, autoReconnectDelay, maxAsyncConnectPeriod) : _server()->AddNewConnectSession(sRef, targetIPAddressAndPort, autoReconnectDelay, maxAsyncConnectPeriod);
}

status_t MessageTransceiverThread :: AddNewConnectSession(const String & targetHostName, uint16 port, const AbstractReflectSessionRef & sessionRef, bool expandLocalhost, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   MRETURN_ON_ERROR(EnsureServerAllocated());

   AbstractReflectSessionRef sRef = sessionRef;
   if (sRef() == NULL) sRef = CreateDefaultWorkerSession();
   if (sRef())
   {
      ThreadWorkerSession * tws = dynamic_cast<ThreadWorkerSession *>(sRef());
      if (tws) tws->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);

      if (IsInternalThreadRunning()) return SendAddNewSessionMessage(sRef, ConstSocketRef(), targetHostName(), IPAddressAndPort(invalidIP, port), expandLocalhost, autoReconnectDelay, maxAsyncConnectPeriod);
      else
      {
         const IPAddress ip = GetHostByName(targetHostName(), expandLocalhost);
         return (ip != invalidIP) ? _server()->AddNewConnectSession(sRef, IPAddressAndPort(ip, port), autoReconnectDelay, maxAsyncConnectPeriod) : B_ERROR("GetHostByName() failed");
      }
   }
   else return B_ERROR("CreateDefaultWorkerSession() failed");
}

status_t MessageTransceiverThread :: SendAddNewSessionMessage(const AbstractReflectSessionRef & sessionRef, const ConstSocketRef & sock, const char * hostName, const IPAddressAndPort & hostIAP, bool expandLocalhost, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   if (sessionRef() == NULL) return B_BAD_ARGUMENT;

   MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_ADD_NEW_SESSION));
   MRETURN_ON_ERROR(msgRef);

   if (hostIAP.IsValid()) {MRETURN_ON_ERROR(msgRef()->CAddFlat( MTT_NAME_IPADDRESSANDPORT, hostIAP));}
                     else {MRETURN_ON_ERROR(msgRef()->CAddInt16(MTT_NAME_PORT,             hostIAP.GetPort()));}  // sometimes we need to send the port along with MTT_NAME_HOSTNAME instead

   MRETURN_ON_ERROR(msgRef()->AddTag(     MTT_NAME_SESSION,            sessionRef));
   MRETURN_ON_ERROR(msgRef()->CAddString( MTT_NAME_HOSTNAME,           hostName));
   MRETURN_ON_ERROR(msgRef()->CAddBool(   MTT_NAME_EXPANDLOCALHOST,    expandLocalhost));
   MRETURN_ON_ERROR(msgRef()->CAddTag(    MTT_NAME_SOCKET,             CastAwayConstFromRef(sock)));
   MRETURN_ON_ERROR(msgRef()->CAddInt64(  MTT_NAME_AUTORECONNECTDELAY, autoReconnectDelay,    MUSCLE_TIME_NEVER));
   MRETURN_ON_ERROR(msgRef()->CAddInt64(  MTT_NAME_MAXASYNCCONNPERIOD, maxAsyncConnectPeriod, MUSCLE_TIME_NEVER));
   return SendMessageToInternalThread(msgRef);
}

status_t MessageTransceiverThread :: PutAcceptFactory(uint16 port, const ReflectSessionFactoryRef & factoryRef, const IPAddress & optInterfaceIP, uint16 * optRetPort)
{
   MRETURN_ON_ERROR(EnsureServerAllocated());

   ReflectSessionFactoryRef fRef = factoryRef;
   if (fRef() == NULL) fRef = CreateDefaultSessionFactory();
   if (fRef() == NULL) return B_ERROR("CreateDefaultSessionFactory() failed");

   ThreadWorkerSessionFactory * twsf = dynamic_cast<ThreadWorkerSessionFactory *>(fRef());
   if (twsf) twsf->SetForwardAllIncomingMessagesToSupervisorIfNotAlreadySet(_forwardAllIncomingMessagesToSupervisor);

   if (IsInternalThreadRunning())
   {
      MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_PUT_ACCEPT_FACTORY));
      MRETURN_ON_ERROR(msgRef);
      MRETURN_ON_ERROR(msgRef()->CAddInt16(MTT_NAME_PORT, port));
      MRETURN_ON_ERROR(msgRef()->AddTag(MTT_NAME_FACTORY, fRef));
      MRETURN_ON_ERROR(msgRef()->CAddFlat(MTT_NAME_IPADDRESS, optInterfaceIP));
      return SendMessageToInternalThread(msgRef);
   }
   else return _server()->PutAcceptFactory(port, fRef, optInterfaceIP, optRetPort);
}

status_t MessageTransceiverThread :: RemoveAcceptFactory(uint16 port, const IPAddress & optInterfaceIP)
{
   if (_server())
   {
      if (IsInternalThreadRunning())
      {
         MessageRef msgRef(GetMessageFromPool(MTT_COMMAND_REMOVE_ACCEPT_FACTORY));
         MRETURN_ON_ERROR(msgRef);

         MRETURN_ON_ERROR(msgRef()->AddInt16(MTT_NAME_PORT,      port));
         MRETURN_ON_ERROR(msgRef()->CAddFlat(MTT_NAME_IPADDRESS, optInterfaceIP));
         return SendMessageToInternalThread(msgRef);
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
      MRETURN_ON_ERROR(msgRef);

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
      MRETURN_ON_ERROR(msgRef);

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
      MRETURN_ON_ERROR(msgRef);

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
      MRETURN_ON_ERROR(msgRef);
      MRETURN_ON_ERROR(msgRef()->AddString(MTT_NAME_PATH, path));
      MRETURN_ON_ERROR(SendMessageToInternalThread(msgRef));
   }
   else _defaultDistributionPath = path;

   return B_NO_ERROR;
}

status_t MessageTransceiverThread :: GetNextEventFromInternalThread(uint32 & code, MessageRef * optRetRef, String * optFromSession, uint32 * optFromFactoryID, IPAddressAndPort * optLocation)
{
   // First, default values for everyone
   if (optRetRef)        optRetRef->Reset();
   if (optFromSession)   optFromSession->Clear();
   if (optFromFactoryID) *optFromFactoryID = 0;

   MessageRef msgRef;
   MRETURN_ON_ERROR(GetNextReplyFromInternalThread(msgRef));
   if (msgRef() == NULL) return B_BAD_DATA;  // semi-paranoia

   code = msgRef()->what;
   if ((optRetRef)&&(msgRef()->FindMessage(MTT_NAME_MESSAGE, *optRetRef).IsError())) *optRetRef = msgRef;
   if (optFromSession)   *optFromSession   = msgRef()->GetString(MTT_NAME_FROMSESSION);
   if (optFromFactoryID) *optFromFactoryID = msgRef()->GetInt32(MTT_NAME_FACTORY_ID);
   if (optLocation)      *optLocation      = msgRef()->GetFlat<IPAddressAndPort>(MTT_NAME_IPADDRESSANDPORT);
   return B_NO_ERROR;
}

status_t MessageTransceiverThread :: RequestOutputQueuesDrainedNotification(const MessageRef & notifyRef, const String & optDistPath, DrainTag * optDrainTag)
{
   // Send a command to the supervisor letting him know we are waiting for him to assure
   // us that all the matching worker sessions have dequeued their messages.  Preallocate
   // as much as possible so we won't have to worry about out-of-memory later on, when
   // it's too late to handle it properly.
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_NOTIFY_ON_OUTPUT_DRAIN);
   MessageRef replyRef   = GetMessageFromPool(MTT_EVENT_OUTPUT_QUEUES_DRAINED);
   if ((commandRef() == NULL)||(replyRef() == NULL)) MRETURN_OUT_OF_MEMORY;

   MRETURN_ON_ERROR(replyRef()->CAddMessage(MTT_NAME_MESSAGE, notifyRef));

   DrainTagRef drainTagRef(optDrainTag ? optDrainTag : newnothrow DrainTag);
   if (drainTagRef()) drainTagRef()->SetReplyMessage(replyRef);
   if (drainTagRef())
   {
       MRETURN_ON_ERROR(commandRef()->CAddString(MTT_NAME_PATH, optDistPath));
       MRETURN_ON_ERROR(commandRef()->AddTag(MTT_NAME_DRAIN_TAG, drainTagRef));
       return SendMessageToInternalThread(commandRef);
   }

   // User keeps ownership of his custom DrainTag on error, so we don't delete it.
   if ((drainTagRef())&&(drainTagRef() == optDrainTag))
   {
      drainTagRef()->SetReplyMessage(MessageRef());
      drainTagRef.Neutralize();
   }

  return B_NO_ERROR;
}

status_t MessageTransceiverThread :: SetNewInputPolicy(const AbstractSessionIOPolicyRef & pref, const String & optDistPath)
{
   return SetNewPolicyAux(MTT_COMMAND_SET_INPUT_POLICY, pref, optDistPath);
}

status_t MessageTransceiverThread :: SetNewOutputPolicy(const AbstractSessionIOPolicyRef & pref, const String & optDistPath)
{
   return SetNewPolicyAux(MTT_COMMAND_SET_OUTPUT_POLICY, pref, optDistPath);
}

status_t MessageTransceiverThread :: SetNewPolicyAux(uint32 what, const AbstractSessionIOPolicyRef & pref, const String & optDistPath)
{
   MessageRef commandRef = GetMessageFromPool(what);
   MRETURN_ON_ERROR(commandRef);

   status_t ret;
   return ((commandRef()->CAddString(MTT_NAME_PATH, optDistPath).IsOK(ret))&&(commandRef()->CAddTag(MTT_NAME_POLICY_TAG, pref).IsOK(ret))) ? SendMessageToInternalThread(commandRef) : ret;
}

status_t MessageTransceiverThread :: SetOutgoingMessageEncoding(int32 encoding, const String & optDistPath)
{
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_SET_OUTGOING_ENCODING);
   MRETURN_ON_ERROR(commandRef);

   status_t ret;
   return ((commandRef()->CAddString(MTT_NAME_PATH, optDistPath).IsOK(ret))&&(commandRef()->AddInt32(MTT_NAME_ENCODING, encoding).IsOK(ret))) ? SendMessageToInternalThread(commandRef) : ret;
}

status_t MessageTransceiverThread :: RemoveSessions(const String & optDistPath)
{
   MessageRef commandRef = GetMessageFromPool(MTT_COMMAND_REMOVE_SESSIONS);
   MRETURN_ON_ERROR(commandRef);

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
   while(WaitForNextMessageFromOwner(junk, 0).IsOK()) {/* empty */}
   while(GetNextReplyFromInternalThread(junk).IsOK()) {/* empty */}
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
   if ((msg())&&(msg()->CAddFlat(MTT_NAME_IPADDRESSANDPORT, GetAsyncConnectDestination()).IsOK())) (void) SendMessageToSupervisorSession(msg);
}

status_t ThreadWorkerSession :: AttachedToServer()
{
   MRETURN_ON_ERROR(StorageReflectSession::AttachedToServer());

   if (_acceptedIAP.IsValid())
   {
      MessageRef msg = GetMessageFromPool(MTT_EVENT_SESSION_ACCEPTED);
      MRETURN_ON_ERROR(msg);
      MRETURN_ON_ERROR(msg()->AddFlat(MTT_NAME_IPADDRESSANDPORT, _acceptedIAP));
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

io_status_t ThreadWorkerSession :: DoOutput(uint32 maxBytes)
{
   const io_status_t ret = StorageReflectSession::DoOutput(maxBytes);
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
               DrainTagRef drainTagRef;
               if (msg->FindTag(MTT_NAME_DRAIN_TAG, drainTagRef).IsOK())
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
               AbstractSessionIOPolicyRef pref = msg->GetTag<AbstractSessionIOPolicyRef>(MTT_NAME_POLICY_TAG);
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
         ThreadWorkerSession * ws = static_cast<ThreadWorkerSession *>(workers[i]());  // static_cast is okay because FindSessionsOfType() guarantees it for us
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
   uint32 numLeft = 0;
   while(_mtt->WaitForNextMessageFromOwner(msgFromOwner, 0, &numLeft).IsOK())
   {
      if (msgFromOwner()) MessageReceivedFromOwner(msgFromOwner, numLeft);
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

status_t ThreadSupervisorSession :: AddNewWorkerConnectSession(const AbstractReflectSessionRef & sessionRef, const IPAddressAndPort & hostIAP, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   const status_t ret = hostIAP.IsValid() ? AddNewConnectSession(sessionRef, hostIAP, autoReconnectDelay, maxAsyncConnectPeriod) : B_BAD_ARGUMENT;

   // For immediate failure: Since (sessionRef) never attached, we need to send the disconnect message ourself.
   if ((ret.IsError())&&(sessionRef()))
   {
      // We have to synthesize the MTT_NAME_FROMSESSION path ourselves, since the session was never added to the server and thus its path isn't set
      MessageRef errorMsg = GetMessageFromPool(MTT_EVENT_SESSION_DISCONNECTED);
      if ((errorMsg())&&(errorMsg()->AddString(MTT_NAME_FROMSESSION, String("/%1/%2").Arg(Inet_NtoA(hostIAP.GetIPAddress())).Arg(sessionRef()->GetSessionID())).IsOK())) _mtt->SendMessageToOwner(errorMsg);
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
               AbstractReflectSessionRef sessionRef = msg->GetTag<AbstractReflectSessionRef>(MTT_NAME_SESSION);
               if (sessionRef())
               {
                  const char * hostName;
                  const uint64 autoReconnectDelay    = msg->GetInt64(MTT_NAME_AUTORECONNECTDELAY, MUSCLE_TIME_NEVER);
                  const uint64 maxAsyncConnectPeriod = msg->GetInt64(MTT_NAME_MAXASYNCCONNPERIOD, MUSCLE_TIME_NEVER);

                  IPAddressAndPort iap;
                       if (msg->FindFlat<IPAddressAndPort>(MTT_NAME_IPADDRESSANDPORT, iap).IsOK())  (void) AddNewWorkerConnectSession(sessionRef, iap, autoReconnectDelay, maxAsyncConnectPeriod);
                  else if (msg->FindString(MTT_NAME_HOSTNAME,                    &hostName).IsOK()) (void) AddNewWorkerConnectSession(sessionRef, IPAddressAndPort(GetHostByName(hostName, msg->GetBool(MTT_NAME_EXPANDLOCALHOST)), msg->GetInt16(MTT_NAME_PORT)), autoReconnectDelay, maxAsyncConnectPeriod);
                  else                                                                              (void) AddNewSession(sessionRef, msg->GetTag(MTT_NAME_SOCKET).DowncastTo<ConstSocketRef>());
               }
               else LogTime(MUSCLE_LOG_ERROR, "MTT_COMMAND_ADD_NEW_SESSION:  Could not get sessionRef!\n");
            }
            break;

            case MTT_COMMAND_PUT_ACCEPT_FACTORY:
            {
               ReflectSessionFactoryRef factoryRef = msg->GetTag<ReflectSessionFactoryRef>(MTT_NAME_FACTORY);
               if (factoryRef()) (void) PutAcceptFactory(msg->GetInt16(MTT_NAME_PORT), factoryRef, msg->GetFlat<IPAddress>(MTT_NAME_IPADDRESS));
                            else LogTime(MUSCLE_LOG_ERROR, "MTT_COMMAND_PUT_ACCEPT_FACTORY:  Could not get factoryRef!\n");
            }
            break;

            case MTT_COMMAND_REMOVE_ACCEPT_FACTORY:
               (void) RemoveAcceptFactory(msg->GetInt16(MTT_NAME_PORT), msg->GetFlat<IPAddress>(MTT_NAME_IPADDRESS));
            break;

            case MTT_COMMAND_SET_DEFAULT_PATH:
               _defaultDistributionPath = msg->GetString(MTT_NAME_PATH);
            break;

            case MTT_COMMAND_NOTIFY_ON_OUTPUT_DRAIN:
            {
               DrainTagRef drainTagRef;
               if ((msg->FindTag(MTT_NAME_DRAIN_TAG, drainTagRef).IsOK())&&(_drainTags.PutWithDefault(drainTagRef()).IsOK()))
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
            break;

#ifdef MUSCLE_ENABLE_SSL
            case MTT_COMMAND_SET_SSL_PRIVATE_KEY:
               _mtt->_server()->SetSSLPrivateKey(msg->GetFlat<ConstByteBufferRef>(MTT_NAME_DATA));
            break;

            case MTT_COMMAND_SET_SSL_PUBLIC_KEY:
               _mtt->_server()->SetSSLPublicKeyCertificate(msg->GetFlat<ConstByteBufferRef>(MTT_NAME_DATA));
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
