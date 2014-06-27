/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef MuscleThread_h
#define MuscleThread_h

#include "system/Mutex.h"
#include "message/Message.h"
#include "util/Queue.h"
#include "util/SocketMultiplexer.h"

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# error "You're not allowed use the Thread class if you have the MUSCLE_SINGLE_THREAD_ONLY compiler constant defined!"
#endif

#if defined(MUSCLE_USE_PTHREADS)
# include <pthread.h>
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
# // empty
#elif defined(MUSCLE_QT_HAS_THREADS)
# define MUSCLE_USE_QT_THREADS
# if QT_VERSION >= 0x040000
#  include <QThread>
#  define QT_HAS_THREAD_PRIORITIES
# else
#  include <qthread.h>
#  if QT_VERSION >= 0x030200
#   define QT_HAS_THREAD_PRIORITIES
#  endif
# endif
#elif defined(__BEOS__) || defined(__HAIKU__)
# include <kernel/OS.h>
#elif defined(__ATHEOS__)
# include <atheos/threads.h>
#else
# error "Thread:  threading support not implemented for this platform.  You'll need to add support for your platform to the MUSCLE Lock and Thread classes for your OS before you can use the Thread class here (or define MUSCLE_USE_PTHREADS or QT_THREAD_SUPPORT to use those threading APIs, respectively)."
#endif

namespace muscle {

/** This class is an platform-independent class that creates an internally held thread and executes it.
  * You will want to subclass Thread in order to specify the behaviour of the internally held thread...
  * The default thread implementation doesn't do anything very useful.
  * It also includes support for sending Messages to the thread, receiving reply Messages from the thread,
  * and for waiting for the thread to exit.
  */
class Thread : private CountedObject<Thread>
{
public:
   /** Constructor.  Does very little (in particular, the internal thread is not started here...
     * that happens when you call StartInternalThread())
     * @param useMessagingSockets Whether or not this thread should allocate a connected pair of 
     *                            sockets to handle messaging between the internal thread and the
     *                            calling thread.  Defaults to true.  Don't set this to false unless
     *                            you really know what you are doing, as setting it to false will
     *                            break much of the Thread object's standard functionality.
     */
   Thread(bool useMessagingSockets = true);

   /** Destructor.  You must have made sure that the internal thread is no longer running before
     * deleting the Thread object, or an assertion failure will occur.  (You should make sure the
     * internal thread is gone by calling WaitForInternalThreadToExit() before deleting this Thread object)
     */
   virtual ~Thread();

   /** Start the internal thread running
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory, or thread is already running)
     */
   virtual status_t StartInternalThread();

   /** Returns true iff the thread is considered to be running.
     * (Note that the thread is considered running from the time StartInternalThread() returns B_NO_ERROR
     * until the time WaitForInternalThreadToExit() is called and returns B_NO_ERROR.  Even if the thread
     * terminates itself before then, it is still considered to be 'running' as far as we're concerned)
     */
   bool IsInternalThreadRunning() const {return _threadRunning;}

   /** Returns true iff the calling thread is the internal thread, or false if the caller is any other thread. */
   bool IsCallerInternalThread() const;

   /** If the current thread is the internal thread of a Thread object, returns a pointer to that object.
     * Otherwise, returns NULL.
     */
   static Thread * GetCurrentThread();

   /** Tells the internal thread to quit by sending it a NULL MessageRef, and then optionally 
     * waits for it to go away by calling WaitForInternalThreadToExit().  
     * If the internal thread isn't running, this method is a no-op.
     * You must call this before deleting the MessageTransceiverThread object!
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     * @param waitForThread if true, this method won't return until the thread is gone.  Defaults to true.
     *                      (if you set this to false, you'll need to also call WaitForThreadToExit() before deleting this object)
     */
   virtual void ShutdownInternalThread(bool waitForThread = true);

   /** Blocks and won't return until after the internal thread exits.  If you have called
     * StartInternalThread(), you'll need to call this method (or ShutdownInternalThread())
     * before deleting this Thread object or calling StartInternalThread() again--even if 
     * your thread has already terminated itself!  That way consistency is guaranteed and
     * race conditions are avoided.
     * @returns B_NO_ERROR on success, or B_ERROR if the internal thread wasn't running.
     */
   status_t WaitForInternalThreadToExit();

   /** Puts the given message into a message queue for the internal thread to pick up,
     * and then calls SignalInternalThread() (if necessary) to signal the internal thread that a
     * new message is ready.  If the internal thread isn't currently running, then the 
     * MessageRef will be queued up and available to the internal thread to process when it is started.
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     * @param msg Reference to the message that is to be given to the internal thread. 
     * @return B_NO_ERROR on success, or B_ERROR on failure (out of memory)
     */
   virtual status_t SendMessageToInternalThread(const MessageRef & msg);

   /** This method attempts to retrieve the next reply message that has been
     * sent back to the main thread by the internal thread (via SendMessageToOwner()).
     * @param ref On success, (ref) will be a reference to the new reply message.
     * @param wakeupTime Time at which this method should stop blocking and return,
     *                   even if there is no new reply message ready.  If this value is
     *                   0 (the default) or otherwise less than the current time 
     *                   (as returned by GetRunTime64()), then this method does a 
     *                   non-blocking poll of the reply queue.
     *                   If (wakeuptime) is set to MUSCLE_TIME_NEVER, then this method 
     *                   will block indefinitely, until a new reply is ready.
     * @see GetOwnerSocketSet() for advanced control of this method's behaviour
     * @returns The number of Messages left in the reply queue on success, or -1 on failure
     *          (The call timed out without any replies ever showing up)
     */
   virtual int32 GetNextReplyFromInternalThread(MessageRef & ref, uint64 wakeupTime = 0);

   /** Locks the internal thread's message queue and returns a pointer to it.  
     * Since the queue is locked, you may examine or modify the queue safely.
     * Once this method has returned successfully, you are responsible for unlocking the
     * message queue again by calling UnlockMessageQueue().  If you don't, the Thread will
     * remain locked and stuck!
     * @returns a pointer to our internal Message queue, on success, or NULL on failure (couldn't lock)
     */
   Queue<MessageRef> * LockAndReturnMessageQueue();

   /** Unlocks our internal message queue, so that the internal thread can again pop messages off of it.
     * Should be called exactly once after each successful call to LockAndReturnMessageQueue().
     * After this call returns, it is no longer safe to use the pointer that was
     * previously returned by LockAndReturnMessageQueue().
     * @returns B_NO_ERROR on success, or B_ERROR if the unlock call failed (perhaps it wasn't locked?)
     */
   status_t UnlockMessageQueue();

   /** Locks this Thread's reply queue and returns a pointer to it.  Since the queue is
     * locked, you may examine or modify the queue safely.
     * Once this method has returned successfully, you are responsible for unlocking the
     * message queue again by calling UnlockReplyQueue().  If you don't, the Thread will
     * remain locked and stuck!
     * @returns a pointer to our internal reply queue on success, or NULL on failure (couldn't lock)
     */
   Queue<MessageRef> * LockAndReturnReplyQueue();

   /** Unlocks the reply message queue, so that the internal thread can again append messages to it.
     * Should be called exactly once after each successful call to LockAndReturnReplyQueue().
     * After this call returns, it is no longer safe to use the pointer that was
     * previously returned by LockAndReturnReplyQueue().
     * @returns B_NO_ERROR on success, or B_ERROR if the unlock call failed (perhaps it wasn't locked?)
     */
   status_t UnlockReplyQueue();

   /** Returns the socket that the main thread may select() for read on for wakeup-notification bytes. 
     * This Thread object's thread-signalling sockets will be allocated by this method if they aren't already allocated.
     * @note this method will return a NULL reference if this Thread was created with constructor argument useMessagingSockets=false.
     */
   const ConstSocketRef & GetOwnerWakeupSocket();

   /** Enumeration of the socket sets that are available for blocking on; used in GetOwnerSocketSet()
    *  and GetInternalSocketSet() calls.
    */
   enum {
      SOCKET_SET_READ = 0,  /**< set of sockets to watch for ready-to-read (i.e. incoming data available)          */
      SOCKET_SET_WRITE,     /**< set of sockets to watch for ready-to-write (i.e. outgoing buffer space available) */
      SOCKET_SET_EXCEPTION, /**< set of sockets to watch for exceptional conditions (implementation defined)       */
      NUM_SOCKET_SETS       /**< A guard value */
   };

   /** This function returns a reference to one of the three socket-sets that
    *  GetNextReplyFromInternalThread() will optionally use to determine whether 
    *  to return early.  By default, all of the socket-sets are empty, and 
    *  GetNextReplyFromInternalThread() will return only when a new Message
    *  has arrived from the internal thread, or when the timeout period has elapsed.
    *
    *  However, in some cases it is useful to have GetNextReplyFromInternalThread()
    *  return under other conditions as well, such as when a specified socket becomes 
    *  ready-to-read-from or ready-to-write-to.  You can specify that a socket should be 
    *  watched in this manner, by adding that socket to the appropriate socket set(s).  
    *  For example, to tell GetNextReplyFromInternalThread() to always return when 
    *  mySocket is ready to be written to, you would add mySocket to the SOCKET_SET_WRITE 
    *  set, like this:
    *     
    *     _thread.GetOwnerSocketSet(SOCKET_SET_WRITE).Put(mySocket, false);
    *
    *  (This only needs to be done once)  After GetNextReplyFromInternalThread() 
    *  returns, you can determine whether your socket is ready-to-write-to by checking 
    *  its associated value in the table, like this:
    *
    *     bool canWrite = false;
    *     _thread.GetOwnerSocketSet(SOCKET_SET_WRITE).Get(mySocket, canWrite);
    *     if (canWrite) printf("Socket is ready to be written to!\n");
    *
    *  @param socketSet SOCKET_SET_* indicating which socket-set to return a reference to.
    *  @note This method should only be called from the main thread!
    */
   Hashtable<ConstSocketRef, bool> & GetOwnerSocketSet(uint32 socketSet) {return _threadData[MESSAGE_THREAD_OWNER]._socketSets[socketSet];}

   /** As above, but returns a read-only reference. */
   const Hashtable<ConstSocketRef, bool> & GetOwnerSocketSet(uint32 socketSet) const;

   /** Call this to set a suggested stack size for the new thread.  If set to non-zero, StartInternalThread()
    *  will try to set the stack size of the thread it creates to this value.  Note that calling this method
    *  has no effect on an internal thread that is already running.
    *  @param stackSizeBytes The number of bytes suggested for the internal thread to use, or 0 if
    *                        it's best to just use the stack size provided by default by the OS.
    */
   void SetSuggestedStackSize(uint32 stackSizeBytes) {_suggestedStackSize = stackSizeBytes;}

   /** Returns the current suggested stack size, as was previously set by SetSuggestedStackSize().
     * Returns 0 if there is no suggested stack size specified (which is the default behavior).
     */
   uint32 GetSuggestedStackSize() const {return _suggestedStackSize;}

   /** If called from within the internal thread, returns (roughly) the amount of data (in bytes)
     * currently on the thread's stack.  Returns zero if the thread stack size is unknown
     * (e.g. because this method was called from a different thread)
     */
   uint32 GetCurrentStackUsage() const;

protected:
   /** If you are using the default implementation of InternalThreadEntry(), then this
     * method will be called whenever a new MessageRef is received by the internal thread.
     * Default implementation does nothing, and returns B_NO_ERROR if (msgRef) is valid,
     * or B_ERROR if (msgRef) is a NULL reference.
     * @note this method will not be called if this Thread was created with constructor argument useMessagingSockets=false.
     * @param msgRef Reference to the just-received Message object.
     * @param numLeft Number of Messages still left in the owner's message queue.
     * @return B_NO_ERROR if you wish to continue processing, or B_ERROR if you wish to
     *                    terminate the internal thread and go away.
     */
   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft);

   /** May be called by the internal thread to send a Message back to the owning thread.
     * Puts the given MessageRef into the replies queue, and then calls SignalOwner()
     * (if necessary) to notify the main thread that replies are pending.
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     * @param replyRef MessageRef to send back to the owning thread.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t SendMessageToOwner(const MessageRef & replyRef);

   /** You may override this method to be your Thread's execution entry point.  
     * Default implementation runs in a loop calling WaitForNextMessageFromOwner() and
     * then MessageReceivedFromOwner().  In many cases, that is all you need, so you may
     * not need to override this method.
     */
   virtual void InternalThreadEntry();
 
   /** This method is meant to be called by the internally held thread.
     * It will attempt retrieve the next message that has been sent to the 
     * thread via SendMessageToThread().
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     * @param ref On success, (ref) will be set to be a reference to the retrieved Message.
     * @param wakeupTime Time at which this method should stop blocking and return,
     *                   even if there is no new message ready.  If this value is
     *                   0 or otherwise less than the current time (as returned by GetRunTime64()),
     *                   then this method does a non-blocking poll of the queue.
     *                   If (wakeuptime) is set to MUSCLE_TIME_NEVER (the default value),
     *                   then this method will block indefinitely, until a Message is ready.
     * @returns The number of Messages still remaining in the message queue on success, or 
     *          -1 on failure (i.e. the call was aborted before any Messages ever showing up,
     *          and (ref) was not written to)
     * @see GetInternalSocketSet() for advanced control of this method's behaviour
     */
   virtual int32 WaitForNextMessageFromOwner(MessageRef & ref, uint64 wakeupTime = MUSCLE_TIME_NEVER);

   /** Called by SendMessageToThread() whenever there is a need to wake up the internal
     * thread so that it will look at its reply queue.
     * Default implementation sends a byte on a socket to implement this,
     * but you can override this method to do it a different way if you need to.
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     */
   virtual void SignalInternalThread();

   /** Called by SendMessageToOwner() whenever there is a need to wake up the owning
     * thread so that it will look at its reply queue.  Default implementation sends
     * a byte to the main-thread-listen socket, but you can override this method to
     * do it different way if you need to.
     * @note this method will not work if this Thread was created with constructor argument useMessagingSockets=false.
     */
   virtual void SignalOwner();

   /** Returns the socket that the internal thread may select() for read on for wakeup-notification bytes.
     * This Thread object's thread-signalling sockets will be allocated by this method if they aren't already allocated.
     * @note this method will return a NULL reference if this Thread was created with constructor argument useMessagingSockets=false.
     * @returns The socket fd that the thread is to listen on, or a NULL reference on error.
     */
   const ConstSocketRef & GetInternalThreadWakeupSocket();

   /** Locks the lock we use to serialize calls to SignalInternalThread() and
     * SignalOwner().  Be sure to call UnlockSignallingLock() when you are done with the lock.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (couldn't lock)
     */
   status_t LockSignalling() {return _signalLock.Lock();}

   /** Unlocks the lock we use to serialize calls to SignalInternalThread() and SignalOwner().  
     * @returns B_NO_ERROR on success, or B_ERROR on failure (couldn't unlock)
     */
   status_t UnlockSignalling() {return _signalLock.Unlock();}

   /** Closes all of our threading sockets, if they are open. */
   void CloseSockets();

   /** This function returns a reference to one of the three socket-sets that
    *  WaitForNextMessageFromOwner() will optionally use to determine whether 
    *  to return early.  By default, all of the socket-sets are empty, and 
    *  WaitForNextMessageFromOwner() will return only when a new Message
    *  has arrived from the owner thread, or when the timeout period has elapsed.
    *
    *  However, in some cases it is useful to have WaitForNextMessageFromOwner()
    *  return under other conditions as well, such as when a specified socket becomes 
    *  ready-to-read-from or ready-to-write-to.  You can specify that a socket should be 
    *  watched in this manner, by adding that socket to the appropriate socket set(s).  
    *  For example, to tell WaitForNextMessageFromOwner() to always return when 
    *  mySocket is ready to be written to, you would add mySocket to the SOCKET_SET_WRITE 
    *  set, like this:
    *     
    *     _thread.GetInternalSocketSet(SOCKET_SET_WRITE).Put(mySocket, false);
    *
    *  (This only needs to be done once)  After WaitForNextMessageFromOwner() 
    *  returns, you can determine whether your socket is ready-to-write-to by checking 
    *  its associated value in the table, like this:
    *
    *     bool canWrite = false;
    *     _thread.GetInternalSocketSet(SOCKET_SET_WRITE).Get(mySocket, canWrite);
    *     if (canWrite) printf("Socket is ready to be written to!\n");
    *
    *  @param socketSet SOCKET_SET_* indicating which socket-set to return a reference to.
    *  @note This method should only be called from the internal thread!
    */
   Hashtable<ConstSocketRef, bool> & GetInternalSocketSet(uint32 socketSet) {return _threadData[MESSAGE_THREAD_INTERNAL]._socketSets[socketSet];}

   /** As above, but returns a read-only reference. */
   const Hashtable<ConstSocketRef, bool> & GetInternalSocketSet(uint32 socketSet) const;

private:
   Thread(const Thread & rhs);  // deliberately private and unimplemented
   Thread & operator = (const Thread & rhs);  // deliberately private and unimplemented

   /** This class encapsulates data that is used by one of our two threads (internal or owner).
    *  It's put in a class like this so that I can easily access two copies of everything.
    */
   class ThreadSpecificData 
   {
   public:
      ThreadSpecificData() {/* empty */}

      Mutex _queueLock;
      ConstSocketRef _messageSocket;
      Queue<MessageRef> _messages;
      Hashtable<ConstSocketRef, bool> _socketSets[NUM_SOCKET_SETS];  // (socket -> isFlagged)
      SocketMultiplexer _multiplexer;
   };

   status_t StartInternalThreadAux();
   const ConstSocketRef & GetThreadWakeupSocketAux(ThreadSpecificData & tsd);
   int32 WaitForNextMessageAux(ThreadSpecificData & tsd, MessageRef & ref, uint64 wakeupTime = MUSCLE_TIME_NEVER);
   status_t SendMessageAux(int whichQueue, const MessageRef & ref);
   void SignalAux(int whichSocket);
   void InternalThreadEntryAux();

   enum {
      MESSAGE_THREAD_INTERNAL = 0,  // internal thread's (input queue, socket to block on)
      MESSAGE_THREAD_OWNER,         // main thread's (input queue, socket to block on)
      NUM_MESSAGE_THREADS
   };

   const bool _useMessagingSockets;
   bool _messageSocketsAllocated;

   ThreadSpecificData _threadData[NUM_MESSAGE_THREADS];

   bool _threadRunning;
   Mutex _signalLock;

#if defined(MUSCLE_USE_PTHREADS)
   pthread_t _thread;
   static void * InternalThreadEntryFunc(void * This) {((Thread *)This)->InternalThreadEntryAux(); return NULL;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   ::HANDLE _thread;
   DWORD _threadID;
   static DWORD WINAPI InternalThreadEntryFunc(LPVOID This) {((Thread*)This)->InternalThreadEntryAux(); return 0;}
#elif defined(MUSCLE_USE_QT_THREADS)
   class MuscleQThread : public QThread
   {
   public:
      MuscleQThread() : _owner(NULL) {/* empty */}  // _owner not set here, for VC++6 compatibility
      void SetOwner(Thread * owner) {_owner = owner;}
      virtual void run() {_owner->InternalThreadEntryAux();}

   private:
      Thread * _owner;
   };
   MuscleQThread _thread;
   friend class MuscleQThread;

protected:
# ifdef QT_HAS_THREAD_PRIORITIES
   /** Returns the priority that the internal thread should be launched under.  Only available
    *  when using Qt 3.2 or higher, so you may want to wrap your references to this method in an 
    *  #ifdef QT_HAS_THREAD_PRIORITIES test, to make sure your code remains portable.
    *  @returns the QThread::Priority value, as described in the QThread documentation.  Default
    *           implementation always returns QThread::InheritPriority.
    */
   virtual QThread::Priority GetInternalQThreadPriority() const {return QThread::InheritPriority;}
# endif
#endif

private:
#if defined(MUSCLE_USE_PTHREADS)
   typedef pthread_t muscle_thread_key;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   typedef DWORD muscle_thread_key;
#elif defined(MUSCLE_USE_QT_THREADS)
   typedef QThread * muscle_thread_key;
#elif defined(__BEOS__) || defined(__HAIKU__)
   typedef thread_id muscle_thread_key;
   thread_id _thread;
   static int32 InternalThreadEntryFunc(void * This) {((Thread *)This)->InternalThreadEntryAux(); return 0;}
#elif defined(__ATHEOS__)
   typedef thread_id muscle_thread_key;
   thread_id _thread;
   static void InternalThreadEntryFunc(void * This) {((Thread *)This)->InternalThreadEntryAux();}
#endif

   static muscle_thread_key GetCurrentThreadKey();
   static Mutex _curThreadsMutex;
   static Hashtable<muscle_thread_key, Thread *> _curThreads;
   uint32 _suggestedStackSize;
   const uint32 * _threadStackBase;
};

#ifdef MUSCLE_AVOID_CHECK_THREAD_STACK_USAGE
# define CHECK_THREAD_STACK_USAGE
#else
/** Macro for checking the current Thread's stack usage, and aborting with an error message
  * if the Thread has overrun it's suggested stack-space.  Note that this macro only works
  * if called from a Thread object's internal thread, and only for Thread objects that 
  * had SetSuggestedStackSize() called on them before the internal thread was started.
  */
# define CHECK_THREAD_STACK_USAGE CheckThreadStackUsage(__FILE__, __LINE__)
#endif

/** Typically called by the CHECK_THREAD_STACK_USAGE macro.  Checks the current thread's stack usage,
  * and causes an assertion failure (and program exit) to occur if the thread is using more stack
  * space than was suggested via SetSuggestedStackSize().
  * @param fileName the name of the source code file this method was called from (i.e. __FILE__)
  * @param line the line number that this method was called from (i.e. __LINE__)
  */
void CheckThreadStackUsage(const char * fileName, uint32 line);

}; // end namespace muscle

#endif
