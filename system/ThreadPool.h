/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef MuscleThreadPool_h
#define MuscleThreadPool_h

#include "system/Thread.h"
#include "system/Mutex.h"
#include "util/Queue.h"
#include "util/RefCount.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/ObjectPool.h"  // for AbstractObjectRecycler

namespace muscle {

class ThreadPool;

/** This is an interface class that should be implemented by objects that want to make use of a ThreadPool. */
class IThreadPoolClient
{
public:
   /** Constructor.  
     * @param threadPool Pointer to the ThreadPool object this client should register with, or NULL if you wish this client
     *                   to start out unregistered.  (If the latter, be sure to call SetThreadPool() later on).
     */
   IThreadPoolClient(ThreadPool * threadPool) : _threadPool(NULL) {SetThreadPool(threadPool);}

   /** Destructor.  Note that if this object is still registered with a ThreadPool when this destructor is called,
     * an assertion failure will be triggered -- registered IThreadPoolClient objects MUST call SetThreadPool(NULL) 
     * BEFORE any part of them is destroyed, in order to avoid race conditions between their constructors and
     * their MessageReceivedFromThreadPool() method callbacks.
     */
   virtual ~IThreadPoolClient();

   /** Sends the specified MessageRef object to the ThreadPool for later handling.  
     * @param msg A Message for the ThreadPool to handle.  Our MessageReceivedFromThreadPoolClient() method
     *            will be called in the near future, from within a ThreadPool thread.
     * @returns B_NO_ERROR if the Message was scheduled for execution by a thread in the ThreadPool, or B_ERROR if it wasn't.
     * Messages are guaranteed to be processed in the order that they were passed to this method, but there is no
     * guarantee that they will all be processed in the same thread as each other.
     */
   status_t SendMessageToThreadPool(const MessageRef & msg);

   /** Changes this IThreadPoolClient to use a different ThreadPool.  This will unregister this client
     * from the current ThreadPool if necessary, and register with the new one if necessary.
     *
     * @param tp The new ThreadPool to register with, or NULL if you only want to unregister from the old one.
     *
     * Note that it is REQUIRED for registered IThreadPoolClient objects to call SetThreadPool(NULL) to
     * un-register themselves from their ThreadPool BEFORE their destructors start to tear down their state;
     * otherwise race conditions will result if the ThreadPool object happens to call MessageReceivedFromThreadPoolClient()
     * on the partially-deconstructed IThreadPoolClient object.
     *
     * Note that if Messages callbacks for this IThreadPoolClient are still pending in the ThreadPool object
     * when this method un-registers the IThreadPoolClient object, then this method will block until after all those
     * callbacks have completed.
     */
   void SetThreadPool(ThreadPool * tp);
   
   /** Returns a pointer to the ThreadPool this client is currently registered with, or NULL if it isn't registered anywhere. */
   ThreadPool * GetThreadPool() const {return _threadPool;}

protected:
   /** Called from inside one of the ThreadPool's threads, some time after SendMessageToThreadPool(msg) was
     * called by the IThreadPoolClient.
     * @param msg The MessageRef that was passed to SendMessageToThreadPool().
     * @param numLeft The number of additional Messages that will arrive in this batch after this one.
     * Note that since this method will be called in a different thread from the thread that called
     * SendMessageToThreadPool(), implementations of this method need to be careful when accessing member
     * variables to avoid race conditions.
     */
   virtual void MessageReceivedFromThreadPool(const MessageRef & msg, uint32 numLeft) = 0;

private:
   friend class ThreadPool;

   ThreadPool * _threadPool;
};

/** This class allows you to multiplex the handling of a large number of parallel Message streams
  * into a finite number of actual Threads.  By doing so you can (roughly) simulate the behavior
  * of a much larger number of Threads than are actually created.
  *
  * This class is Thread-safe, in that you can have IThreadPoolClients using it from different
  * threads simultaneously, and it will still work as expected.
  */
class ThreadPool : private AbstractObjectRecycler, private CountedObject<ThreadPool>
{
public:
   /** Constructor. 
     * @param maxThreadCount The maximum number of Threads this ThreadPool object will be allowed
     *                       to create at once.  Defaults to 16.
     */
   ThreadPool(uint32 maxThreadCount = 16);

   /** Destructor.  */
   virtual ~ThreadPool();

   /** Returns the maximum number of threads this ThreadPool is allowed to keep around at once time (as specified in the constructor) */
   uint32 GetMaxThreadCount() const {return _maxThreadCount;}

   /** Used for debugging only */
   virtual void PrintToStream() const 
   {
      printf("ThreadPool %p:  _maxThreadCount=" UINT32_FORMAT_SPEC ", _shuttingDown=%i, _threadIDCounter=" UINT32_FORMAT_SPEC ", _availableThreads=" UINT32_FORMAT_SPEC ", _activeThreads=" UINT32_FORMAT_SPEC ", _registeredClients=" UINT32_FORMAT_SPEC ", _pendingMessages=" UINT32_FORMAT_SPEC ", _deferredMessages=" UINT32_FORMAT_SPEC ", _waitingForCompletion=" UINT32_FORMAT_SPEC "\n", this, _maxThreadCount, _shuttingDown, _threadIDCounter, _availableThreads.GetNumItems(), _activeThreads.GetNumItems(), _registeredClients.GetNumItems(), _pendingMessages.GetNumItems(), _deferredMessages.GetNumItems(), _waitingForCompletion.GetNumItems());
   }

protected:
   /** Starts the specified Thread object's internal thread running.
     * Broken out into a virtual method so that the Thread's attributes (stack size, etc) can be customized if desired.   
     * Default implementation just calls StartInternalThread() on the thread object.
     * @param thread The Thread object to start.
     * @returns B_NO_ERROR if the Thread was successfully started, or B_ERROR otherwise.
     */
   virtual status_t StartInternalThread(Thread & thread);

private:
   virtual void RecycleObject(void * /*obj*/) {/* empty */}
   virtual uint32 FlushCachedObjects() {return Shutdown();}  // called by SetupSystem destructor, to avoid crashes on exit
   uint32 Shutdown();

   class ThreadPoolThread : public Thread, public RefCountable
   {
   public:
      ThreadPoolThread(ThreadPool * tp, uint32 threadID) : _threadID(threadID), _threadPool(tp), _currentClient(NULL) {/* empty */}

      uint32 GetThreadID() const {return _threadID;}
      status_t SendMessagesToInternalThread(IThreadPoolClient * client, Queue<MessageRef> & mq);

   protected:
      virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft);

   private:
      const uint32 _threadID;
      ThreadPool * _threadPool;
      IThreadPoolClient * _currentClient;
      Queue<MessageRef> _internalQueue;
   };
   DECLARE_REFTYPES(ThreadPoolThread);

   friend class IThreadPoolClient;
   friend class ThreadPoolThread;

   void RegisterClient(IThreadPoolClient * client);
   void UnregisterClient(IThreadPoolClient * client);
   status_t SendMessageToThreadPool(IThreadPoolClient * client, const MessageRef & msg);
   void DispatchPendingMessagesUnsafe();  // _poolLock must be locked when this is called!
   void ThreadFinishedProcessingClientMessages(uint32 threadID, IThreadPoolClient * client);
   bool DoesClientHaveMessagesOutstandingUnsafe(IThreadPoolClient * client) const;
   void MessageReceivedFromThreadPoolAux(IThreadPoolClient * client, const MessageRef & msg, uint32 numLeft) {client->MessageReceivedFromThreadPool(msg, numLeft);}  // just to skirt protected-member issues 

   const uint32 _maxThreadCount;

   Mutex _poolLock;
   bool _shuttingDown;

   uint32 _threadIDCounter;
   Hashtable<uint32, ThreadPoolThreadRef> _availableThreads;
   Hashtable<uint32, ThreadPoolThreadRef> _activeThreads;
   Hashtable<IThreadPoolClient *, bool> _registeredClients;  // registered clients -> (true iff a Thread is currently handling them)
   Hashtable<IThreadPoolClient *, Queue<MessageRef> > _pendingMessages;  // Messages ready to be sent to a pool Thread
   Hashtable<IThreadPoolClient *, Queue<MessageRef> > _deferredMessages; // Messages to be be sent to a pool Thread when the current Thread's Messages are done
   Hashtable<IThreadPoolClient *, ConstSocketRef> _waitingForCompletion; // Clients who are blocked in UnregisterClient() waiting for Messages to complete processing
};

}; // end namespace muscle

#endif
