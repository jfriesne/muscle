/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <QCoreApplication>
#include "qtsupport/QMessageTransceiverThread.h"

namespace muscle {

static const uint32 QMTT_SIGNAL_EVENT = QEvent::User+14837;  // why yes, this is a completely arbitrary number

#if QT_VERSION >= 0x040000
QMessageTransceiverThread :: QMessageTransceiverThread(QObject * parent, const char * name) : QObject(parent), _firstSeenHandler(NULL), _lastSeenHandler(NULL)
{
   if (name) setObjectName(name);
}
#else
QMessageTransceiverThread :: QMessageTransceiverThread(QObject * parent, const char * name) : QObject(parent, name), _firstSeenHandler(NULL), _lastSeenHandler(NULL)
{
   // empty
}
#endif

QMessageTransceiverThread :: ~QMessageTransceiverThread()
{
   ShutdownInternalThread();  // just in case (note this assumes the user isn't going to subclass this class!)

   // Make sure our handlers don't try to reference us anymore
   for (HashtableIterator<uint32, QMessageTransceiverHandler *> iter(_handlers); iter.HasData(); iter++) iter.GetValue()->ClearRegistrationFields();
}

status_t QMessageTransceiverThread :: SendMessageToSessions(const MessageRef & msgRef, const char * optDistPath)       
{
   // This method is reimplemented here so it can be a Qt "slot" method
   return MessageTransceiverThread :: SendMessageToSessions(msgRef, optDistPath);
}

void QMessageTransceiverThread :: SignalOwner()
{
#if QT_VERSION >= 0x040000
   QEvent * evt = newnothrow QEvent((QEvent::Type)QMTT_SIGNAL_EVENT);
#else
   QCustomEvent * evt = newnothrow QCustomEvent(QMTT_SIGNAL_EVENT);
#endif
   if (evt) QCoreApplication::postEvent(this, evt);
       else WARN_OUT_OF_MEMORY;
}

bool QMessageTransceiverThread :: event(QEvent * event)
{
   if (event->type() == QMTT_SIGNAL_EVENT)
   {
      HandleQueuedIncomingEvents();
      return true;
   }
   else return QObject::event(event);
}

void QMessageTransceiverThread :: HandleQueuedIncomingEvents()
{
   uint32 code;
   MessageRef next;
   String sessionID;
   uint32 factoryID;
   bool seenIncomingMessage = false;
   IPAddressAndPort iap;

   // Check for any new messages from our internal thread
   while(GetNextEventFromInternalThread(code, &next, &sessionID, &factoryID, &iap) >= 0)
   {
      switch(code)
      {
         case MTT_EVENT_INCOMING_MESSAGE: default:
            if (seenIncomingMessage == false)
            {
               seenIncomingMessage = true;
               emit BeginMessageBatch();
            }
            emit MessageReceived(next, sessionID); 
         break;

         case MTT_EVENT_SESSION_ACCEPTED:      emit SessionAccepted(sessionID, factoryID, iap); break;
         case MTT_EVENT_SESSION_ATTACHED:      emit SessionAttached(sessionID);                 break;
         case MTT_EVENT_SESSION_CONNECTED:     emit SessionConnected(sessionID, iap);           break;
         case MTT_EVENT_SESSION_DISCONNECTED:  emit SessionDisconnected(sessionID);             break;
         case MTT_EVENT_SESSION_DETACHED:      emit SessionDetached(sessionID);                 break;
         case MTT_EVENT_FACTORY_ATTACHED:      emit FactoryAttached(factoryID);                 break;
         case MTT_EVENT_FACTORY_DETACHED:      emit FactoryDetached(factoryID);                 break;
         case MTT_EVENT_OUTPUT_QUEUES_DRAINED: emit OutputQueuesDrained(next);                  break;
         case MTT_EVENT_SERVER_EXITED:         emit ServerExited();                             break;
      }
      emit InternalThreadEvent(code, next, sessionID, factoryID);  // these get emitted for any event

      const char * id = _handlers.HasItems() ? strchr(sessionID()+1, '/') : NULL;
      if (id)
      {
         QMessageTransceiverHandler * handler;
         if (_handlers.Get(atoi(id+1), handler) == B_NO_ERROR)
         {
            // If it's not already in the list, prepend it to the list and tell it to emit its BeginMessageBatch() signal
            if ((code == MTT_EVENT_INCOMING_MESSAGE)&&(handler != _lastSeenHandler)&&(handler->_nextSeen == NULL))
            {
               if (_firstSeenHandler == NULL) _firstSeenHandler = _lastSeenHandler = handler;
               else
               {
                  _firstSeenHandler->_prevSeen = handler;
                  handler->_nextSeen = _firstSeenHandler;
                  _firstSeenHandler = handler;
               }
               handler->EmitBeginMessageBatch();
            }
            handler->HandleIncomingEvent(code, next, iap);
         }
      }
   }

   FlushSeenHandlers(true);

   if (seenIncomingMessage) emit EndMessageBatch();
}

void QMessageTransceiverThread :: RemoveFromSeenList(QMessageTransceiverHandler * h, bool doEmit)
{
   if ((h == _lastSeenHandler)||(h->_nextSeen))  // make sure (h) is actually in the list
   {
      if (h->_prevSeen) h->_prevSeen->_nextSeen = h->_nextSeen;
                   else _firstSeenHandler       = h->_nextSeen;
      if (h->_nextSeen) h->_nextSeen->_prevSeen = h->_prevSeen;
                   else _lastSeenHandler        = h->_prevSeen;
      h->_prevSeen = h->_nextSeen = NULL;
      if (doEmit) h->EmitEndMessageBatch();  // careful:  lots of interesting things could happen inside this call!
   }
}

void QMessageTransceiverThread :: Reset()
{
   FlushSeenHandlers(true);
   for (HashtableIterator<uint32, QMessageTransceiverHandler *> iter(_handlers); iter.HasData(); iter++) iter.GetValue()->ClearRegistrationFields();
   _handlers.Clear();

   MessageTransceiverThread::Reset(); 
}

status_t QMessageTransceiverThread :: RegisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sessionRef)
{
   if (this != &thread) return thread.RegisterHandler(thread, handler, sessionRef);  // paranoia
   else
   {
      int32 id = sessionRef() ? (int32)sessionRef()->GetSessionID() : -1;
      if ((id >= 0)&&(_handlers.Put(id, handler) == B_NO_ERROR)) 
      {
         handler->_master    = this;
         handler->_mtt       = this;
         handler->_sessionID = id;
         handler->_sessionTargetString = String("/*/") + sessionRef()->GetSessionIDString();
         return B_NO_ERROR;
      }
      else handler->ClearRegistrationFields();  // paranoia

      return B_ERROR;
   }
}

void QMessageTransceiverThread :: UnregisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, bool emitEndMessageBatchIfNecessary)
{
   if (this != &thread) thread.UnregisterHandler(thread, handler, emitEndMessageBatchIfNecessary);  // paranoia
   else
   {
      if (_handlers.Remove(handler->_sessionID) == B_NO_ERROR)
      {
         // paranoia:  in case we are doing this in the middle of our last-seen traversal, we need
         // to safely remove (handler) from the traversal so that the traversal doesn't break
         if ((emitEndMessageBatchIfNecessary)&&((handler->_nextSeen)||(handler == _lastSeenHandler))) RemoveFromSeenList(handler, emitEndMessageBatchIfNecessary);

         (void) RemoveSessions(handler->_sessionTargetString());
      }

      handler->ClearRegistrationFields();
   }
}

QMessageTransceiverThreadPool :: QMessageTransceiverThreadPool(uint32 maxSessionsPerThread) : _maxSessionsPerThread(maxSessionsPerThread)
{
   // empty
}

QMessageTransceiverThreadPool :: ~QMessageTransceiverThreadPool()
{
   ShutdownAllThreads();
}

void QMessageTransceiverThreadPool :: ShutdownAllThreads()
{
   for (HashtableIterator<QMessageTransceiverThread *, Void> iter(_threads); iter.HasData(); iter++) 
   {
      QMessageTransceiverThread * mtt = iter.GetKey();
      mtt->ShutdownInternalThread();
      delete mtt;
   }
   _threads.Clear();
}
 
QMessageTransceiverThread * QMessageTransceiverThreadPool :: CreateThread()
{
   QMessageTransceiverThread * newThread = newnothrow QMessageTransceiverThread;
   if (newThread == NULL) WARN_OUT_OF_MEMORY;
   return newThread; 
}

QMessageTransceiverThread * QMessageTransceiverThreadPool :: ObtainThread()
{
   // If any room is available, it will be in the last item in the table
   QMessageTransceiverThread * const * lastThread = _threads.GetLastKey();
   if ((lastThread)&&((*lastThread)->GetHandlers().GetNumItems() < _maxSessionsPerThread)) return *lastThread;

   // If we got here, we need to create a new thread
   QMessageTransceiverThread * newThread = CreateThread();
   if ((newThread == NULL)||(newThread->StartInternalThread() != B_NO_ERROR)||(_threads.PutWithDefault(newThread) != B_NO_ERROR))
   {
      WARN_OUT_OF_MEMORY;
      newThread->ShutdownInternalThread();  // in case Put() failed
      delete newThread;
      return NULL;
   }
   return newThread;
}

status_t QMessageTransceiverThreadPool :: RegisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sessionRef)
{
   if (thread.RegisterHandler(thread, handler, sessionRef) == B_NO_ERROR)
   { 
      handler->_master = this;  // necessary since QMessageTransceiverThread::RegisterHandler will have overwritten it
      if (thread.GetHandlers().GetNumItems() >= _maxSessionsPerThread) (void) _threads.MoveToFront(&thread);
      return B_NO_ERROR;
   }
   return B_ERROR;
}

void QMessageTransceiverThreadPool :: UnregisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, bool emitEndMessageBatchIfNecessary)
{
   thread.UnregisterHandler(thread, handler, emitEndMessageBatchIfNecessary);
   if (thread.GetHandlers().GetNumItems() < _maxSessionsPerThread) (void) _threads.MoveToBack(&thread);
}

#if QT_VERSION >= 0x040000
QMessageTransceiverHandler :: QMessageTransceiverHandler(QObject * parent, const char * name) : QObject(parent), _master(NULL), _mtt(NULL), _prevSeen(NULL), _nextSeen(NULL)
{
   if (name) setObjectName(name);
}
#else
QMessageTransceiverHandler :: QMessageTransceiverHandler(QObject * parent, const char * name) : QObject(parent, name), _master(NULL), _mtt(NULL), _prevSeen(NULL), _nextSeen(NULL)
{
   // empty
}
#endif

QMessageTransceiverHandler :: ~QMessageTransceiverHandler()
{
   (void) Reset(false);  // yes, it's virtual... but that's okay, it will just call our implementation
}

status_t QMessageTransceiverHandler :: SetupAsNewSession(IMessageTransceiverMaster & master, const ConstSocketRef & sock, const ThreadWorkerSessionRef & optSessionRef)
{
   Reset();
   QMessageTransceiverThread * thread = master.ObtainThread();
   if (thread)
   {
      ThreadWorkerSessionRef sRef = optSessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession(*thread);  // gotta do this now so we can know its ID
      if ((sRef())&&(master.RegisterHandler(*thread, this, sRef) == B_NO_ERROR))
      {
         if (thread->AddNewSession(sock, sRef) == B_NO_ERROR) return B_NO_ERROR;
         master.UnregisterHandler(*thread, this, true);
      }
   }
   return B_ERROR;
}

status_t QMessageTransceiverHandler :: SetupAsNewConnectSession(IMessageTransceiverMaster & master, const ip_address & targetIPAddress, uint16 port, const ThreadWorkerSessionRef & optSessionRef, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   Reset();
   QMessageTransceiverThread * thread = master.ObtainThread();
   if (thread)
   {
      ThreadWorkerSessionRef sRef = optSessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession(*thread);  // gotta do this now so we can know its ID
      if ((sRef())&&(master.RegisterHandler(*thread, this, sRef) == B_NO_ERROR)) 
      {
         if (thread->AddNewConnectSession(targetIPAddress, port, sRef, autoReconnectDelay, maxAsyncConnectPeriod) == B_NO_ERROR) return B_NO_ERROR;
         master.UnregisterHandler(*thread, this, true);
      }
   }
   return B_ERROR;
}

status_t QMessageTransceiverHandler :: SetupAsNewConnectSession(IMessageTransceiverMaster & master, const String & targetHostName, uint16 port, const ThreadWorkerSessionRef & optSessionRef, bool expandLocalhost, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   Reset();
   QMessageTransceiverThread * thread = master.ObtainThread();
   if (thread)
   {
      ThreadWorkerSessionRef sRef = optSessionRef;
      if (sRef() == NULL) sRef = CreateDefaultWorkerSession(*thread);  // gotta do this now so we can know its ID
      if ((sRef())&&(master.RegisterHandler(*thread, this, sRef) == B_NO_ERROR))
      {
         if (thread->AddNewConnectSession(targetHostName, port, sRef, expandLocalhost, autoReconnectDelay, maxAsyncConnectPeriod) == B_NO_ERROR) return B_NO_ERROR;
         master.UnregisterHandler(*thread, this, true);
      }
   }
   return B_ERROR;
}

status_t QMessageTransceiverHandler :: RequestOutputQueueDrainedNotification(const MessageRef & notificationMsg, DrainTag * optDrainTag)
{
   return _mtt ? _mtt->RequestOutputQueuesDrainedNotification(notificationMsg, _sessionTargetString(), optDrainTag) : B_ERROR;
}

status_t QMessageTransceiverHandler :: SetNewInputPolicy(const AbstractSessionIOPolicyRef & pref)
{
   return _mtt ? _mtt->SetNewInputPolicy(pref, _sessionTargetString()) : B_ERROR;
}

status_t QMessageTransceiverHandler :: SetNewOutputPolicy(const AbstractSessionIOPolicyRef & pref)
{
   return _mtt ? _mtt->SetNewOutputPolicy(pref, _sessionTargetString()) : B_ERROR;
}

status_t QMessageTransceiverHandler :: SetOutgoingMessageEncoding(int32 encoding)
{
   return _mtt ? _mtt->SetOutgoingMessageEncoding(encoding, _sessionTargetString()) : B_ERROR;
}

status_t QMessageTransceiverHandler :: SendMessageToSession(const MessageRef & msgRef)
{
   return _mtt ? _mtt->SendMessageToSessions(msgRef, _sessionTargetString()) : B_ERROR;
}

void QMessageTransceiverHandler :: Reset(bool emitEndBatchIfNecessary)
{
   if (_mtt)
   {
      (void) _mtt->RemoveSessions(_sessionTargetString());
      _master->UnregisterHandler(*_mtt, this, emitEndBatchIfNecessary);
   }
}

void QMessageTransceiverHandler :: HandleIncomingEvent(uint32 code, const MessageRef & next, const IPAddressAndPort & iap)
{
   switch(code)
   {
      case MTT_EVENT_INCOMING_MESSAGE:      emit MessageReceived(next);    break;
      case MTT_EVENT_SESSION_ATTACHED:      emit SessionAttached();        break;
      case MTT_EVENT_SESSION_CONNECTED:     emit SessionConnected(iap);    break;
      case MTT_EVENT_SESSION_DISCONNECTED:  emit SessionDisconnected();    break;
      case MTT_EVENT_SESSION_DETACHED:      emit SessionDetached();        break;
      case MTT_EVENT_OUTPUT_QUEUES_DRAINED: emit OutputQueueDrained(next); break;
   }
   emit InternalHandlerEvent(code, next);  // these get emitted for any event
}

void QMessageTransceiverHandler :: ClearRegistrationFields()
{
   _master = NULL;
   _mtt = NULL;
   _sessionID = -1;
   _sessionTargetString.Clear();
   _prevSeen = _nextSeen = NULL;
}

ThreadWorkerSessionRef QMessageTransceiverHandler :: CreateDefaultWorkerSession(QMessageTransceiverThread & thread)
{
   return thread.CreateDefaultWorkerSession();
}

};  // end namespace muscle;
