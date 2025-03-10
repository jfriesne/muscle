/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/ThreadPool.h"

namespace muscle {

static Message _dummyMsg;

IThreadPoolClient :: ~IThreadPoolClient()
{
   MASSERT(_threadPool == NULL, "You must not delete an IThreadPoolClient Thread object it is still registered with a ThreadPool!  (Call SetThreadPool(NULL) on it BEFORE deleting it!)");
}

status_t IThreadPoolClient :: SendMessageToThreadPool(const MessageRef & msg)
{
   if (_threadPool == NULL) return B_BAD_OBJECT;
   if (msg()       == NULL) return B_BAD_ARGUMENT;
   return _threadPool->SendMessageToThreadPool(this, msg);
}

void IThreadPoolClient :: SetThreadPool(ThreadPool * tp)
{
   if (tp != _threadPool)
   {
      if (_threadPool) _threadPool->UnregisterClient(this);
      _threadPool = tp;
      if (_threadPool) _threadPool->RegisterClient(this);
   }
}

ThreadPool :: ThreadPool(uint32 maxThreadCount)
   : _maxThreadCount(maxThreadCount)
   , _shuttingDown(false)
   , _threadIDCounter(0)
   , _availableThreads(PreallocatedItemSlotsCount(maxThreadCount))
   , _activeThreads(PreallocatedItemSlotsCount(maxThreadCount))
{
   // empty
}

ThreadPool :: ~ThreadPool()
{
   (void) Shutdown();
}

uint32 ThreadPool :: Shutdown()
{
   DECLARE_MUTEXGUARD(_poolLock);
   _shuttingDown = true;

   for (HashtableIterator<uint32, ThreadPoolThreadRef> iter(_availableThreads); iter.HasData(); iter++) iter.GetValue()()->ShutdownInternalThread();
   for (HashtableIterator<uint32, ThreadPoolThreadRef> iter(_activeThreads);    iter.HasData(); iter++) iter.GetValue()()->ShutdownInternalThread();
   for (HashtableIterator<IThreadPoolClient *, bool> iter(_registeredClients);  iter.HasData(); iter++) iter.GetKey()->_threadPool = NULL;  // so they won't try to unregister from us

   const uint32 ret = _availableThreads.GetNumItems()+_activeThreads.GetNumItems()+_registeredClients.GetNumItems()+_pendingMessages.GetNumItems()+_deferredMessages.GetNumItems()+_waitingForCompletion.GetNumItems();
   _availableThreads.Clear();
   _activeThreads.Clear();
   _registeredClients.Clear();
   _pendingMessages.Clear();
   _deferredMessages.Clear();
   _waitingForCompletion.Clear();
   return ret;
}

void ThreadPool :: RegisterClient(IThreadPoolClient * client)
{
   DECLARE_MUTEXGUARD(_poolLock);
   (void) _registeredClients.Put(client, false);
}

static bool AreThereMessagesInQueue(const Queue<MessageRef> * mq) {return ((mq)&&(mq->HasItems()));}

// Assumes _poolLock is locked!
bool ThreadPool :: DoesClientHaveMessagesOutstandingUnsafe(IThreadPoolClient * client) const
{
   return ((_registeredClients.GetWithDefault(client))||(AreThereMessagesInQueue(_pendingMessages.Get(client)))||(AreThereMessagesInQueue(_deferredMessages.Get(client))));
}

void ThreadPool :: UnregisterClient(IThreadPoolClient * client)
{
   ConstSocketRef waitSock;

   {
      // If this client has any Messages pending, we need to block until they are gone
      DECLARE_MUTEXGUARD(_poolLock);
      if (DoesClientHaveMessagesOutstandingUnsafe(client))
      {
         ConstSocketRef signalSock;
         if (CreateConnectedSocketPair(waitSock, signalSock, true).IsOK()) (void) _waitingForCompletion.Put(client, signalSock);
                                                                      else printf("ThreadPool::UnregisterClient:  Couldn't set up socket pair for shutdown notification!\n");
      }
   }

   char buf;
   if (waitSock()) (void) ReadData(waitSock, &buf, sizeof(buf), true);   // block here until ReadData() returns, indicating that we can continue

   // final cleanup
   DECLARE_MUTEXGUARD(_poolLock);
   (void) _registeredClients.Remove(client);
   (void) _pendingMessages.Remove(client);
   (void) _deferredMessages.Remove(client);
}

status_t ThreadPool :: ThreadPoolThread :: SendMessagesToInternalThread(IThreadPoolClient * client, Queue<MessageRef> & mq)
{
   MASSERT((_currentClient == NULL),   "ThreadPoolThread::SendMessagesToInternalThread:  _currentClient isn't NULL!");
   MASSERT((_internalQueue.IsEmpty()), "ThreadPoolThread::SendMessagesToInternalThread:  _internalQueue isn't empty!");

   _currentClient = client;
   _internalQueue.SwapContents(mq);
   if (SendMessageToInternalThread(DummyMessageRef(_dummyMsg)).IsOK()) return B_NO_ERROR;  // Send an empty Message, just to signal the internal thread
   else
   {
      // roll back!
      _currentClient = NULL;
      _internalQueue.SwapContents(mq);
      return B_NO_ERROR;
   }
}

status_t ThreadPool :: ThreadPoolThread :: MessageReceivedFromOwner(const MessageRef & msgRef, uint32 /*numLeft*/)
{
   if (msgRef() == NULL) return B_SHUTTING_DOWN;  // time to go away!

   MASSERT((_currentClient != NULL),    "ThreadPoolThread::MessageReceivedFromOwner:  _currentClient is NULL!");
   MASSERT((_internalQueue.HasItems()), "ThreadPoolThread::MessageReceivedFromOwner:  _internalQueue is empty!");

   while(_internalQueue.HasItems())
   {
      _threadPool->MessageReceivedFromThreadPoolAux(_currentClient, _internalQueue.Head(), _internalQueue.GetNumItems()-1);
      (void) _internalQueue.RemoveHead();
   }

   IThreadPoolClient * client = _currentClient;
   _currentClient = NULL; // we need _currentClient to be NULL when we call ThreadFinishedProcessingClientMessages()
   _threadPool->ThreadFinishedProcessingClientMessages(_threadID, client);
   return B_NO_ERROR;
}

status_t ThreadPool :: SendMessageToThreadPool(IThreadPoolClient * client, const MessageRef & msg)
{
   DECLARE_MUTEXGUARD(_poolLock);

   bool * isBeingHandled = _registeredClients.Get(client);
   if (isBeingHandled == NULL) return B_BAD_ARGUMENT;

   Queue<MessageRef> * mq = ((*isBeingHandled)?_deferredMessages:_pendingMessages).GetOrPut(client);
   MRETURN_OOM_ON_NULL(mq);
   MRETURN_ON_ERROR(mq->AddTail(msg));

   if ((*isBeingHandled == false)&&(mq->GetNumItems() == 1)) DispatchPendingMessagesUnsafe();
   return B_NO_ERROR;
}

// Note:  This method assumes the _poolLock is already unlocked!
void ThreadPool :: DispatchPendingMessagesUnsafe()
{
   if (_shuttingDown) return;  // no sense dispatching more messages if we're in the process of shutting down

   while(_pendingMessages.HasItems())
   {
      IThreadPoolClient * client = *_pendingMessages.GetFirstKey();
      Queue<MessageRef> * mq     = _pendingMessages.GetFirstValue();
      bool * isBeingHandled      = _registeredClients.Get(client);
      if ((isBeingHandled)&&(mq)&&(mq->HasItems()))  // we actually know mq will be non-NULL, but testing it makes clang++ happy
      {
         MASSERT(*isBeingHandled == false, "DispatchPendingMessagesUnsafe:  Client that is being handled is in the _pendingMessages table!");

         if ((_availableThreads.IsEmpty())&&(_activeThreads.GetNumItems() < _maxThreadCount))
         {
            // demand-allocate a new Thread for us to use
            ThreadPoolThreadRef tRef(new ThreadPoolThread(this, _threadIDCounter++));

            status_t ret;
            if (StartInternalThread(*tRef()).IsError(ret)) {LogTime(MUSCLE_LOG_ERROR, "ThreadPool:  Error launching thread! [%s]\n", ret()); break;}
            if (_availableThreads.Put(tRef()->GetThreadID(), tRef).IsError()) {tRef()->ShutdownInternalThread(); break;}  // should never happen, but just in case
         }

         if (_availableThreads.HasItems())
         {
            // Okay, there's an idle thread awaiting something to do, so we'll just pass the next client's requests off to him.
            ThreadPoolThreadRef tRef = *_availableThreads.GetLastValue();  // use the last thread because it's "hottest" in cache
            if (_availableThreads.MoveToTable(tRef()->GetThreadID(), _activeThreads).IsOK())
            {
               if (tRef()->SendMessagesToInternalThread(client, *mq).IsOK())
               {
                  *isBeingHandled = true;  // this is to note that this client now has a Thread that is processing its data
                  (void) _pendingMessages.RemoveFirst();
               }
               else
               {
                  (void) _activeThreads.MoveToTable(tRef()->GetThreadID(), _availableThreads);  // roll back!
                  break;
               }
            }
            else break;  // wtf!?
         }
         else break;  // nothing more we can do for now, all our pool threads are already busy
      }
      else (void) _pendingMessages.RemoveFirst();  // nothing to do for this client!?
   }
}

void ThreadPool :: ThreadFinishedProcessingClientMessages(uint32 threadID, IThreadPoolClient * client)
{
   DECLARE_MUTEXGUARD(_poolLock);
   if (_shuttingDown) return;

   bool * isClientBeingHandled = _registeredClients.Get(client);
   if (isClientBeingHandled)
   {
      MASSERT(*isClientBeingHandled, "ThreadFinishedProcessingClientMessages: *isBeingClientHandled if false!");
      *isClientBeingHandled = false;

      Queue<MessageRef> * deferredMessages = _deferredMessages.Get(client);
      if ((deferredMessages)&&(deferredMessages->HasItems()))
      {
         Queue<MessageRef> * pendingMessages = _pendingMessages.GetOrPut(client);
         if (pendingMessages)
         {
            MASSERT(pendingMessages->IsEmpty(), "ThreadPool::ThreadFinishedProcessingClientMessages():  pendingMessages isn't empty!");
            pendingMessages->SwapContents(*deferredMessages);
         }
      }
   }
   (void) _activeThreads.MoveToTable(threadID, _availableThreads);  // this thread is now available again for further tasks
   DispatchPendingMessagesUnsafe();

   if (DoesClientHaveMessagesOutstandingUnsafe(client) == false) (void) _waitingForCompletion.Remove(client);  // wake up user thread if he's waiting in UnregisterClient()
}

status_t ThreadPool :: StartInternalThread(Thread & thread)
{
   return thread.StartInternalThread();
}

} // end namespace muscle
