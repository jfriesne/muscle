/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleThread_h
#define MuscleThread_h

#if defined(MUSCLE_USE_PTHREADS) && !defined(MUSCLE_AVOID_CPLUSPLUS11)
# include <atomic>
#endif

#include "support/NotCopyable.h"
#include "system/Mutex.h"
#include "system/WaitCondition.h"
#include "message/Message.h"
#include "util/DemandConstructedObject.h"
#include "util/Queue.h"
#include "util/ICallbackSubscriber.h"
#include "util/SocketMultiplexer.h"

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# error "You're not allowed use the Thread class if you have the MUSCLE_SINGLE_THREAD_ONLY compiler constant defined!"
#endif

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
# include <thread>
#elif defined(MUSCLE_USE_PTHREADS)
# include <pthread.h>
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
# // empty
#elif defined(MUSCLE_QT_HAS_THREADS)
# ifndef MUSCLE_USE_QT_THREADS
#  define MUSCLE_USE_QT_THREADS
# endif
# if QT_VERSION >= 0x040000
#  include <QThread>
#  define QT_HAS_THREAD_PRIORITIES
# else
#  include <qthread.h>
#  if QT_VERSION >= 0x030200
#   define QT_HAS_THREAD_PRIORITIES
#  endif
# endif
#else
# error "Thread:  threading support not implemented for this platform.  You'll need to add support code for your platform to the MUSCLE Lock and Thread classes for your OS before you can use the Thread class here (or define MUSCLE_USE_PTHREADS or QT_THREAD_SUPPORT to use those threading APIs, respectively)."
#endif

namespace muscle {

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
// Note that this class has to be declared before the Thread class, otherwise MSVC compiles fail with error C2908
/** This functor is specialized here so we can use std::thread::id objects as keys in a Hashtable. */
template <> class PODHashFunctor<std::thread::id>
{
public:
   MUSCLE_NODISCARD uint32 operator () (const std::thread::id & id) const {return (uint32) _hasher(id);}
   MUSCLE_NODISCARD bool AreKeysEqual(const std::thread::id & k1, const std::thread::id & k2) const {return (k1==k2);}

private:
   std::hash<std::thread::id> _hasher;
};
#endif

/** This class is an platform-independent class that creates an internally held thread and executes it.
  * You will want to subclass Thread in order to specify the behaviour of the internally held thread...
  * The default thread implementation doesn't do anything very useful.
  * It also includes support for sending Messages to the thread, receiving reply Messages from the thread,
  * and for waiting for the thread to exit.
  */
class Thread : public ICallbackSubscriber, private NotCopyable
{
public:
   /** Constructor.  Does very little (in particular, the internal thread is not started here...
     * that happens when you call StartInternalThread())
     * @param useMessagingSockets Specifies whether or not this thread should allocate a connected
     *                            pair of sockets to handle messaging between the internal thread
     *                            and the calling thread.  Defaults to true.  If you set it to false,
     *                            then a WaitCondition object will be used instead; that uses
     *                            fewer resources but it means you can't easily integrate the
     *                            Thread object with select()/SocketMultiplexer/etc.
     *                            If you're unsure about what you want, leave this set to true.
     * @param optCallbackMechanism if specified non-NULL, our SignalOwner() method will call
     *                             RequestCallbackInDispatchThread() in order to request that
     *                             the main/dispatch-thread call DispatchCallbacks() later on.
     */
   Thread(bool useMessagingSockets = true, ICallbackMechanism * optCallbackMechanism = NULL);

   /** Destructor.  You must have made sure that the internal thread is no longer running before
     * deleting the Thread object, or an assertion failure will occur.  (You should make sure the
     * internal thread is gone by calling WaitForInternalThreadToExit() before deleting this Thread object)
     */
   virtual ~Thread();

   /** Start the internal thread running
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT if our internal thread is already running, or etc.
     */
   virtual status_t StartInternalThread();

   /** Returns true iff the thread is considered to be running.
     * (Note that the thread is considered running from the time StartInternalThread() returns B_NO_ERROR
     * until the time WaitForInternalThreadToExit() is called and returns B_NO_ERROR.  Even if the thread
     * terminates itself before then, it is still considered to be 'running' as far as we're concerned)
     */
   MUSCLE_NODISCARD bool IsInternalThreadRunning() const {return _threadRunning;}

   /** Returns true iff the calling thread is the internal thread, or false if the caller is any other thread. */
   MUSCLE_NODISCARD bool IsCallerInternalThread() const;

   /** If the current thread is the internal thread of a Thread object, returns a pointer to that object.
     * Otherwise, returns NULL.
     */
   MUSCLE_NODISCARD static Thread * GetCurrentThread();

   /** Tells the internal thread to quit by sending it a NULL MessageRef, and then optionally
     * waits for it to go away by calling WaitForInternalThreadToExit().
     * If the internal thread isn't running, this method is a no-op.
     * You must call this before deleting the MessageTransceiverThread object!
     * @param waitForThread if true, this method won't return until the thread is gone.  Defaults to true.
     *                      (if you set this to false, you'll need to also call WaitForThreadToExit() before deleting this object)
     */
   virtual void ShutdownInternalThread(bool waitForThread = true);

   /** Blocks and won't return until after the internal thread exits.  If you have called
     * StartInternalThread(), you'll need to call this method (or ShutdownInternalThread())
     * before deleting this Thread object or calling StartInternalThread() again--even if
     * your thread has already terminated itself!  That way consistency is guaranteed and
     * race conditions are avoided.
     * @returns B_NO_ERROR on success, or B_BAD_OBJECT if the internal thread wasn't running.
     */
   status_t WaitForInternalThreadToExit();

   /** Puts the given message into a message queue for the internal thread to pick up,
     * and then calls SignalInternalThread() (if necessary) to signal the internal thread that a
     * new message is ready.  If the internal thread isn't currently running, then the
     * MessageRef will be queued up and available to the internal thread to process when it is started.
     * @param msg Reference to the message that is to be given to the internal thread.
     * @return B_NO_ERROR on success, or an error code on failure (out of memory?)
     */
   virtual status_t SendMessageToInternalThread(const MessageRef & msg);

   /** This method attempts to retrieve the next reply message that has been
     * sent back to the main thread by the internal thread (via SendMessageToOwner()).
     * @param ref On success, (ref) will be a reference to the newly received reply Message.
     * @param wakeupTime Time at which this method should stop blocking and return,
     *                   even if there is no new reply Message ready.  If this value is
     *                   0 (the default) or otherwise less than the current time
     *                   (as returned by GetRunTime64()), then this method does a
     *                   non-blocking poll of the reply queue.
     *                   If (wakeupTime) is set to MUSCLE_TIME_NEVER, then this method
     *                   will block indefinitely, until a new reply is ready.
     * @param optRetNumMessagesLeftInQueue if non-NULL, then on return the number of Messages still
     *                   remaining in the reply queue will be written into the uint32 this argument points to.
     * @returns B_NO_ERROR if (ref) was updated with a new reply-Message value
     *          B_TIMED_OUT if we reached the specified wakeup-time without any Messages becoming available
     *          B_IO_READY if we returned because one of the sockets in a user-suppplied socket-set is ready-for-I/O,
     *          or some other error code if something else goes wrong.
     * @see GetOwnerThreadSocketSet() for details about advanced control of this method's behaviour.
     */
   virtual status_t GetNextReplyFromInternalThread(MessageRef & ref, uint64 wakeupTime = 0, uint32 * optRetNumMessagesLeftInQueue = NULL);

   /** Locks the internal thread's message queue and returns a pointer to it.
     * Since the queue is locked, you may examine or modify the queue safely.
     * Once this method has returned successfully, you are responsible for unlocking the
     * message queue again by calling UnlockMessageQueue().  If you don't, the Thread will
     * remain locked and stuck!
     * @returns a pointer to our internal Message queue, on success, or NULL on failure (couldn't lock)
     */
   MUSCLE_NODISCARD Queue<MessageRef> * LockAndReturnMessageQueue();

   /** Unlocks our internal message queue, so that the internal thread can again pop messages off of it.
     * Should be called exactly once after each successful call to LockAndReturnMessageQueue().
     * After this call returns, it is no longer safe to use the pointer that was
     * previously returned by LockAndReturnMessageQueue().
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the unlock call failed (perhaps it wasn't locked?)
     */
   status_t UnlockMessageQueue();

   /** Locks this Thread's reply queue and returns a pointer to it.  Since the queue is
     * locked, you may examine or modify the queue safely.
     * Once this method has returned successfully, you are responsible for unlocking the
     * message queue again by calling UnlockReplyQueue().  If you don't, the Thread will
     * remain locked and stuck!
     * @returns a pointer to our internal reply queue on success, or NULL on failure (couldn't lock)
     */
   MUSCLE_NODISCARD Queue<MessageRef> * LockAndReturnReplyQueue();

   /** Unlocks the reply message queue, so that the internal thread can again append messages to it.
     * Should be called exactly once after each successful call to LockAndReturnReplyQueue().
     * After this call returns, it is no longer safe to use the pointer that was
     * previously returned by LockAndReturnReplyQueue().
     * @returns B_NO_ERROR on success, or B_LOCK_FAILED if the unlock call failed (perhaps it wasn't locked?)
     */
   status_t UnlockReplyQueue();

   /** Returns the socket that the main thread may select() for read on for wakeup-notification bytes.
     * This Thread object's thread-signalling sockets will be allocated by this method if they aren't already allocated.
     * @note this method will return a NULL reference if this Thread was created with constructor argument useMessagingSockets=false.
     */
   MUSCLE_NODISCARD const ConstSocketRef & GetOwnerWakeupSocket();

   /** Enumeration of the three socket-sets that are available for blocking on; used in GetOwnerThreadSocketSet(),
    *  GetInternalThreadSocketSet(), and related method calls.
    */
   enum {
      SOCKET_SET_READ = 0,  /**< set of sockets to watch for ready-to-read (ie incoming data available)          */
      SOCKET_SET_WRITE,     /**< set of sockets to watch for ready-to-write (ie outgoing buffer space available) */
      SOCKET_SET_EXCEPTION, /**< set of sockets to watch for exceptional conditions (implementation-defined)     */
      NUM_SOCKET_SETS       /**< A guard value */
   };

   /** This function returns a read-only reference to one of the three socket-sets
    *  that GetNextReplyFromInternalThread() will optionally use to determine whether
    *  to return early.  By default, all three of the socket-sets are empty, and
    *  GetNextReplyFromInternalThread() will return only when a new Message
    *  has arrived from the internal thread, or when the timeout period has elapsed.
    *
    *  However, in some cases it is useful to have GetNextReplyFromInternalThread()
    *  return under other conditions as well, such as when a specified socket becomes
    *  ready-to-read-from or ready-to-write-to.  You can specify that a socket should be
    *  watched in this manner, by registering that socket to the appropriate socket set(s).
    *  For example, to tell GetNextReplyFromInternalThread() to always return when
    *  mySocket is ready to be written to, you would add mySocket to the SOCKET_SET_WRITE
    *  set, like this:
    *
    *     _thread.RegisterOwnerThreadSocket(mySocket, SOCKET_SET_WRITE);
    *
    *  This only needs to be done once.  After GetNextReplyFromInternalThread()
    *  returns, you can determine whether your socket is ready-to-write-to like this:
    *
    *     bool canWrite = _thread.IsOwnerThreadSocketReady(mySocket, SOCKET_SET_WRITE);
    *     if (canWrite) printf("Socket is ready to be written to!\n");
    *
    *  @param socketSet SOCKET_SET_* indicating which socket-set to return a reference to.
    *  @note This method should only be called from the main thread!
    */
   MUSCLE_NODISCARD const Hashtable<ConstSocketRef, bool> & GetOwnerThreadSocketSet(uint32 socketSet) const {return _threadData[MESSAGE_THREAD_OWNER]._socketSets[socketSet];}

   /** Register the specified socket so that WaitForNextMessageFromOwner() will return
     * whenever this socket indicates that it is (ready-for-read, ready-for-write, or has-exception)
     * @param sock The socket to watch.
     * @param socketSet a SOCKET_SET_* value specifying which read-for-x state you are interested in
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if sock ref was NULL, or B_OUT_OF_MEMORY.
     * @note This method should only be called from the main thread!
     */
   virtual status_t RegisterOwnerThreadSocket(const ConstSocketRef & sock, uint32 socketSet) {return sock() ? GetOwnerThreadSocketSetRW(socketSet).Put(sock, false) : B_BAD_ARGUMENT;}

   /** Unregisters the specified socket so that WaitForNextMessageFromOwner() will no longer return
     * because this socket indicated that it is (ready-for-read, ready-for-write, or has-exception)
     * @param sock The socket to watch.
     * @param socketSet a SOCKET_SET_* value specifying which read-for-x state you are no longer interested in
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (sock wasn't registered?).
     * @note This method should only be called from the main thread!
     */
   virtual status_t UnregisterOwnerThreadSocket(const ConstSocketRef & sock, uint32 socketSet) {return GetOwnerThreadSocketSetRW(socketSet).Remove(sock);}

   /** Unregisters all sockets from the specified socket-set.
     * @param socketSet a SOCKET_SET_* value specifying the socket-set you want to clear.
     * @note This method should only be called from the main thread!
     */
   virtual void UnregisterAllOwnerThreadSocketsInSet(uint32 socketSet) {GetOwnerThreadSocketSetRW(socketSet).Clear();}

   /** Returns true iff the specified registered socket is currently flagged as ready-for-x
     * @param sock The socket whose state you are interested in.
     * @param socketSet the SOCKET_SET_* value corresponding to the ready-for-x state you are interested in.
     * @returns true iff the specified socket is ready-for-x (where x is specified by the SOCKET_SET_* value)
     * @note This method should only be called from the main thread!
     */
   MUSCLE_NODISCARD bool IsOwnerThreadSocketReady(const ConstSocketRef & sock, uint32 socketSet) const {return GetOwnerThreadSocketSet(socketSet).GetWithDefault(sock, false);}

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
   MUSCLE_NODISCARD uint32 GetSuggestedStackSize() const {return _suggestedStackSize;}

   /** If called from within the internal thread, returns (roughly) the amount of data (in bytes)
     * currently on the thread's stack.  Returns zero if the thread stack size is unknown
     * (eg because this method was called from a different thread)
     */
   MUSCLE_NODISCARD uint32 GetCurrentStackUsage() const;

   /** Sets this thread's priority to the specified priority value.
     * If the thread is currently running, the change will take place immediately; otherwise
     * the priority will be set when the thread is started.
     * @param newPriority a PRIORITY_* value indicating how much CPU-priority this thread should have over other threads.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SetThreadPriority(int newPriority);

   /** Returns a PRIORITY_* value indicating this thread's current priority (as specified
     * by a previous call to SetThreadPriority()), or PRIORITY_UNSPECIFIED if no thread priority
     * has been specified.
     */
   MUSCLE_NODISCARD int GetThreadPriority() const {return _threadPriority;}

   /** Values to pass in to Thread::SetThreadPriority() to specify a thread's execution-priority relative to other threads */
   enum {
      PRIORITY_UNSPECIFIED = -1,  /**< we'll just use whatever priority the OS gives us by default */
      PRIORITY_IDLE,              /**< super-low priority; we only want to run if there's nothing else to do */
      PRIORITY_LOWEST,            /**< lowest non-idle priority */
      PRIORITY_LOWER,             /**< lower priority      */
      PRIORITY_LOW,               /**< low priority        */
      PRIORITY_NORMAL,            /**< normal priority (neither low nor high) */
      PRIORITY_HIGH,              /**< high priority       */
      PRIORITY_HIGHER,            /**< higher priority     */
      PRIORITY_HIGHEST,           /**< highest priority    */
      PRIORITY_TIMECRITICAL,      /**< super-high priority */
      NUM_PRIORITIES              /**< guard value         */
   };

   /** Convenience method:  Given a PRIORITY_* value, returns a corresponding human-readable string (e.g. "High" or "Higher")
     * @param priority a PRIORITY_* value
     * @note Negative values will return "Unspecified", and values greater than or equal to NUM_PRIORITIES will return "???"
     */
   static const char * GetThreadPriorityName(int priority);

   /** Sets the scheduler this thread should be scheduled by.
     * If the thread is currently running, the change will take place immediately; otherwise
     * the scheduler will be set when the thread is started.
     * @param newScheduler a SCHEDULER_* value indicating the scheduler this thread should be scheduled by.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SetThreadScheduler(int newScheduler);

   /** Returns a SCHEDULER_* value indicating this thread's current scheduler (as specified
     * by a previous call to SetThreadScheduler()), or SCHDULER_UNSPECIFIED if no thread scheduler
     * has been specified.
     */
   MUSCLE_NODISCARD int GetThreadScheduler() const {return _threadScheduler;}

   /** Values to pass in to Thread::SetThreadScheduler() to specify the scheduler a thread should be scheduled by */
   enum {
      SCHEDULER_UNSPECIFIED = -1,  /**< we'll just use whatever scheduler the OS gives us by default */
      SCHEDULER_NONREALTIME,       /**< we want to be scheduled by the OS's non-real-time scheduler */
      SCHEDULER_REALTIME,          /**< we want to be scheduled by the OS's real-time scheduler */
      NUM_SCHEDULERS               /**< Guard value */
   };

   /** Convenience method:  Given a SCHEDULER_* value, returns a corresponding human-readable string (e.g. "Real Time" or "Non Real Time")
     * @param sched a SCHEDULER_* value
     * @note Negative values will return "Unspecified", and values greater than or equal to NUM_SCHEDULERS will return "???"
     */
   static const char * GetThreadSchedulerName(int sched);

#if defined(MUSCLE_USE_QT_THREADS)
   /** Returns a pointer to the QThread object being used to implement our internal thread.
     * Note that this method is only available when the MUSCLE_USE_QT_THREADS preprocessor macro is defined,
     * since otherwise there is no QThread object in use.
     */
   MUSCLE_NODISCARD QThread * GetQThread() {return &_thread;}

   /** Returns a read-only pointer to the QThread object being used to implement our internal thread.
     * Note that this method is only available when the MUSCLE_USE_QT_THREADS preprocessor macro is defined,
     * since otherwise there is no QThread object in use.
     */
   MUSCLE_NODISCARD const QThread * GetQThread() const {return &_thread;}
#endif

protected:
   /** Implemented to call MessageReceivedFromInternalThread() as necessary.
     * This method will be called in the main thread iff a non-NULL ICallbackMechanism
     * was passed to our constructor.
     * @param eventTypeBits this argument is not currently used for anything.
     */
   virtual void DispatchCallbacks(uint32 eventTypeBits);

   /** If you are using the default implementation of InternalThreadEntry(), then this
     * method will be called whenever a new MessageRef is received by the internal thread.
     * Default implementation does nothing, and returns B_NO_ERROR if (msgRef) is valid,
     * or B_ERROR if (msgRef) is a NULL reference.
     * @param msgRef Reference to the just-received Message object.
     * @param numLeft Number of Messages still left in the owner's message queue.
     * @return B_NO_ERROR if you wish to continue processing, or an error code if you wish to
     *                    terminate the internal thread and go away.  (B_SHUTTING_DOWN is a good
     *                    error code to return in the latter case, so that the calling code can
     *                    know there wasn't any "real" error that occurred)
     */
   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft);

   /** If you specified a non-NULL ICallbackMechanism in our constructor, this method will be called
     * in the main/dispatch thread by the ICallbackMechanism whenever the internal thread has sent
     * a MessageRef to the main thread.
     * @param msgRef a reference the next Message that was sent by the internal thread
     * @param numLeft the number of Messages still remaining in the queue (as returned by GetNextReplyFromInternalThread())
     */
   virtual void MessageReceivedFromInternalThread(const MessageRef & msgRef, uint32 numLeft);

   /** May be called by the internal thread to send a Message back to the owning thread.
     * Puts the given MessageRef into the replies queue, and then calls SignalOwner()
     * (if necessary) to notify the main thread that replies are pending.
     * @param replyRef MessageRef to send back to the owning thread.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SendMessageToOwner(const MessageRef & replyRef);

   /** You may override this method to be your Thread's execution entry point.
     * Default implementation runs in a loop calling WaitForNextMessageFromOwner() and
     * then MessageReceivedFromOwner().  In many cases, that is all you need, so you may
     * not need to override this method.
     * @note if the -DMUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION compiler flag is being used,
     *       and QThreads are being used as the threading implementation, then InternalThreadEntry()'s
     *       event loop will be implemented by calling QThread::exec() instead.  In that case,
     *       the owning thread's signalling of the internal thread (that a new MessageRef has been
     *       delivered) will be implemented via a QSocketNotifier, but otherwise the Thread
     *       class's behavior will remain the same.  This mode is useful for programs that
     *       wish to use Qt objects inside the internal thread, since some Qt objects require
     *       that the QThread::exec() event loop be running in order to function correctly.
     */
   virtual void InternalThreadEntry();

   /** This method is meant to be called by the internally held thread.
     * It will attempt retrieve the next message that has been sent to the
     * thread via SendMessageToInternalThread().
     * @param ref On success, (ref) will be set to be a reference to the retrieved Message.
     *            Note that (ref) might still be a NULL pointer if our owning thread wants us to exit.
     * @param wakeupTime Time at which this method should stop blocking and return,
     *                   even if there is no new message ready.  If this value is
     *                   0 or otherwise less than the current time (as returned by GetRunTime64()),
     *                   then this method does a non-blocking poll of the queue.
     *                   If (wakeuptime) is set to MUSCLE_TIME_NEVER (the default value),
     *                   then this method will block indefinitely, until a Message is ready.
     * @param optRetNumMessagesLeftInQueue if non-NULL, then on return the number of Messages still
     *                   remaining in the Message Queue will be written into the uint32 this argument points to.
     * @returns B_NO_ERROR if (ref) was updated with a new Message value from the owning-Thread.
     *          B_TIMED_OUT if we reached the specified wakeup-time without any Messages becoming available
     *          B_IO_READY if we returned because one of the sockets in a user-suppplied socket-set is ready-for-I/O,
     *          or some other error code if something else goes wrong.
     * @see GetInternalThreadSocketSet() for details about advanced control of this method's behaviour.
     */
   virtual status_t WaitForNextMessageFromOwner(MessageRef & ref, uint64 wakeupTime = MUSCLE_TIME_NEVER, uint32 * optRetNumMessagesLeftInQueue = NULL);

   /** Called by SendMessageToInternalThread() whenever there is a need to wake up the internal
     * thread so that it will look at its reply queue.
     * Default implementation sends a byte on a socket to implement this,
     * but you can override this method to do it a different way if you need to.
     */
   virtual void SignalInternalThread();

   /** Called by SendMessageToOwner() whenever there is a need to wake up the owning
     * thread so that it will look at its reply queue.  Default implementation calls
     * RequestCallbackInDispatchThread() and also sends a byte to the main-thread-listen socket
     * (if allocated), but you can override this method to do it different way if you need to.
     */
   virtual void SignalOwner();

   /** Returns the socket that the internal thread may select() for read on for wakeup-notification bytes.
     * This Thread object's thread-signalling sockets will be allocated by this method if they aren't already allocated.
     * @note this method will return a NULL reference if this Thread was created with constructor argument useMessagingSockets=false.
     * @returns The socket fd that the thread is to listen on, or a NULL reference on error.
     */
   MUSCLE_NODISCARD const ConstSocketRef & GetInternalThreadWakeupSocket();

   /** Closes all of our threading sockets, if they are open. */
   void CloseSockets();

   /** This function returns a read-only reference to one of the three socket-sets
    *  that WaitForNextMessageFromOwner() will optionally use to determine whether
    *  to return early.  By default, all three of the socket-sets are empty, and
    *  WaitForNextMessageFromOwner() will return only when a new Message
    *  has arrived from the owner thread, or when the timeout period has elapsed.
    *
    *  However, in some cases it is useful to have WaitForNextMessageFromOwner()
    *  return under other conditions as well, such as when a specified socket becomes
    *  ready-to-read-from or ready-to-write-to.  You can specify that a socket should be
    *  watched in this manner, by registering that socket to the appropriate socket set(s).
    *  For example, to tell WaitForNextMessageFromOwner() to always return when
    *  (mySocket) is ready to be written to, you would register (mySocket) with the
    *  SOCKET_SET_WRITE set, like this:
    *
    *     _thread.RegisterInternalThreadSocket(mySocket, SOCKET_SET_WRITE);
    *
    *  This only needs to be done once.  After WaitForNextMessageFromOwner()
    *  returns, you can determine whether your socket is ready-to-write-to by checking
    *  its associated value in the table, like this:
    *
    *     const bool canWrite = IsInternalThreadSocketReady(mySocket, SOCKET_SET_WRITE);
    *     if (canWrite) printf("Socket is ready to be written to!\n");
    *
    *  @param socketSet SOCKET_SET_* indicating which socket-set to return a reference to.
    *  @note This method should only be called from the internal thread!
    */
   MUSCLE_NODISCARD const Hashtable<ConstSocketRef, bool> & GetInternalThreadSocketSet(uint32 socketSet) const {return _threadData[MESSAGE_THREAD_INTERNAL]._socketSets[socketSet];}

   /** Register the specified socket so that WaitForNextMessageFromOwner() will return
     * whenever this socket indicates that it is (ready-for-read, ready-for-write, or has-exception)
     * @param sock The socket to watch.
     * @param socketSet a SOCKET_SET_* value specifying which read-for-x state you are interested in
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if (sock) was a NULL ref, or B_OUT_OF_MEMORY;
     * @note This method should only be called from the internal thread!
     */
   virtual status_t RegisterInternalThreadSocket(const ConstSocketRef & sock, uint32 socketSet) {return sock() ? GetInternalThreadSocketSetRW(socketSet).Put(sock, false) : B_BAD_ARGUMENT;}

   /** Unregisters the specified socket so that WaitForNextMessageFromOwner() will no longer return
     * because this socket indicated that it is (ready-for-read, ready-for-write, or has-exception)
     * @param sock The socket to watch.
     * @param socketSet a SOCKET_SET_* value specifying which read-for-x state you are no longer interested in
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (sock wasn't registered).
     * @note This method should only be called from the internal thread!
     */
   virtual status_t UnregisterInternalThreadSocket(const ConstSocketRef & sock, uint32 socketSet) {return GetInternalThreadSocketSetRW(socketSet).Remove(sock);}

   /** Unregisters all sockets from the specified socket-set.
     * @param socketSet a SOCKET_SET_* value specifying the socket-set you want to clear.
     * @note This method should only be called from the internal thread!
     */
   virtual void UnregisterAllInternalThreadSocketsInSet(uint32 socketSet) {GetInternalThreadSocketSetRW(socketSet).Clear();}

   /** Returns true iff the specified registered socket is currently flagged as ready-for-x
     * @param sock The socket whose state you are interested in.
     * @param socketSet the SOCKET_SET_* value corresponding to the ready-for-x state you are interested in.
     * @returns true iff the specified socket is ready-for-x (where x is specified by the SOCKET_SET_* value)
     * @note This method should only be called from the internal thread!
     */
   MUSCLE_NODISCARD bool IsInternalThreadSocketReady(const ConstSocketRef & sock, uint32 socketSet) const {return GetInternalThreadSocketSet(socketSet).GetWithDefault(sock, false);}

private:
   Thread(ICallbackMechanism * optCallbackMechanism);  // deliberately private and unimplemented to avoid implicit-cast-to-boolean errors if you forget the first argument to the Thread ctor

   // The read/write versions of these methods are intentionally private, since we want callers to use the Register()/Unregister() calls instead
   // of modifying the Hashtables directly.  That way the registration mechanism can be customized by subclasses, if desired.
   Hashtable<ConstSocketRef, bool> & GetInternalThreadSocketSetRW(uint32 socketSet) {return _threadData[MESSAGE_THREAD_INTERNAL]._socketSets[socketSet];}
   Hashtable<ConstSocketRef, bool> & GetOwnerThreadSocketSetRW(   uint32 socketSet) {return _threadData[MESSAGE_THREAD_OWNER   ]._socketSets[socketSet];}

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

      DemandConstructedObject<WaitCondition> _waitCondition;  // only used if (useMessagingSockets) was passed in as false
   };

   status_t StartInternalThreadAux();
   status_t StartInternalThreadAuxAux();
   const ConstSocketRef & GetThreadWakeupSocketAux(ThreadSpecificData & tsd);
   status_t WaitForNextMessageAux(ThreadSpecificData & tsd, MessageRef & ref, uint64 wakeupTime, uint32 * optRetNumThreadsLeftInQueue);
   status_t SendMessageAux(int whichQueue, const MessageRef & ref);
   void SignalAux(int whichSocket);
   void InternalThreadEntryAux();
   status_t SetThreadSchedulerAndPriorityAux(int newSched, int newPriority, bool calledFromInternalThread);

   enum {
      MESSAGE_THREAD_INTERNAL = 0,  // internal thread's (input queue, socket to block on)
      MESSAGE_THREAD_OWNER,         // main thread's (input queue, socket to block on)
      NUM_MESSAGE_THREADS
   };

   const bool _useMessagingSockets;
   bool _messageSocketsAllocated;

   ThreadSpecificData _threadData[NUM_MESSAGE_THREADS];

   bool _threadRunning;

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   std::thread _thread;
   static void InternalThreadEntryFunc(void * This) {((Thread *)This)->InternalThreadEntryAux();}
#elif defined(MUSCLE_USE_PTHREADS)
   pthread_t _thread;
   static void * InternalThreadEntryFunc(void * This) {((Thread *)This)->InternalThreadEntryAux(); return NULL;}
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   ::HANDLE _thread;
   DWORD _threadID;
   static DWORD WINAPI InternalThreadEntryFunc(LPVOID This) {((Thread*)This)->InternalThreadEntryAux(); return 0;}
#elif defined(MUSCLE_USE_QT_THREADS)
# if defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
   friend class MuscleQThreadSocketNotifier;
   void QtSocketReadReady(int sock);
   void QtSocketWriteReady(int /*sock*/) {/* unimplemented for now */}
# endif
   class MuscleQThread : public QThread
   {
   public:
      MuscleQThread() : _owner(NULL) {/* empty */}  // _owner not set here, for VC++6 compatibility
      void SetOwner(Thread * owner) {_owner = owner;}
      virtual void run() {_owner->InternalThreadEntryAux();}

      int CallExec() {return exec();}

   private:
      Thread * _owner;
   };
   MuscleQThread _thread;
   friend class MuscleQThread;
#endif

private:
#ifdef MUSCLE_USE_PTHREADS
   pthread_t GetPthreadID( bool calledFromInternalThread) const;
   pid_t     GetThreadPIDT(bool calledFromInternalThread) const;
#endif
#if defined(WIN32)
   HANDLE GetNativeThreadHandle(bool calledFromInternalThread) const;
#endif

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   typedef std::thread::id muscle_thread_key;
#elif defined(MUSCLE_USE_PTHREADS)
   typedef pthread_t muscle_thread_key;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   typedef DWORD muscle_thread_key;
#elif defined(MUSCLE_USE_QT_THREADS)
   typedef QThread * muscle_thread_key;
#endif

   static muscle_thread_key GetCurrentThreadKey();
   static Mutex _curThreadsMutex;
   static Hashtable<muscle_thread_key, Thread *> _curThreads;
   uint32 _suggestedStackSize;
   const uint32 * _threadStackBase;
   int _threadPriority;
   int _threadScheduler;

#if defined(MUSCLE_USE_PTHREADS)
# if defined(MUSCLE_AVOID_CPLUSPLUS11)
#  volatile pid_t _threadTid;
# else
   std::atomic<pid_t> _threadTid;  // there must be a better way to handle this
# endif
#endif

   DECLARE_COUNTED_OBJECT(Thread);
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
  * @param fileName the name of the source code file this method was called from (ie __FILE__)
  * @param line the line number that this method was called from (ie __LINE__)
  */
void CheckThreadStackUsage(const char * fileName, uint32 line);

} // end namespace muscle

#endif
