/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQMessageTransceiverThread_h
#define MuscleQMessageTransceiverThread_h

#include "support/MuscleSupport.h"  // placed first to avoid ordering problems with MUSCLE_FD_SETSIZE

#include <qobject.h>
#include <qthread.h>
#include "system/MessageTransceiverThread.h"

namespace muscle {

class QMessageTransceiverHandler;
class QMessageTransceiverThread;
class QMessageTransceiverThreadPool;

/** 
 * This is an interface that identifies an object that QMessageTransceiverHandlers
 * can attach themselves to.  It is implemented by both the QMessageTransceiverThread
 * and QMessageTransceiverThreadPool classes.
 */
class IMessageTransceiverMaster
{
public:
   /** Default constructor */
   IMessageTransceiverMaster() {/* empty */}

   /** Destructor */
   virtual ~IMessageTransceiverMaster() {/* empty */}

protected:
   /** Must be implemented to return an available QMessageTransceiverThread object,
     * or return NULL on failure.
     */
   virtual QMessageTransceiverThread * ObtainThread() = 0;

   /** Must be implemented to attach (handler) to the specified thread.
     * @param thread the QMessageTransceiverThread to attach the handler to
     * @param handler the QMessageTransceiverHandler to attach to the thread
     * @param sessionRef the AbstractReflectSession that will represent the handler inside the thread.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   virtual status_t RegisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sessionRef) = 0;

   /** Must be implemented to detach (handler) from (thread)
     * @param thread the QMessageTransceiverThread to detach the handler from
     * @param handler the QMessageTransceiverHandler to detach from the thread
     * @param emitEndMessageBatchIfNecessary If true, and (handler) is currently in the middle of
     *                                       a message-batch, then this method will cause (handler) to emit an
     *                                       EndMessageBatch() signal before dissasociating itself
     *                                       from this IMessageTransceiverMaster.  That way the
     *                                       un-registration won't break the rule that says that one 
     *                                       EndMessageBatch() signal must always be emitted for
     *                                       every one BeginMessageBatch() signal.
     */
   virtual void UnregisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, bool emitEndMessageBatchIfNecessary) = 0;

private:
   friend class QMessageTransceiverHandler;
};

/**
 *  This is a Qt-specific subclass of MessageTransceiverThread.
 *  It hooks all the standard MessageTransceiverThread events
 *  up to Qt signals, so you can just connect() your QMessageTransceiverThread
 *  to the various slots in your application, instead of having to worry
 *  about event loops and such.  In all other ways it works the
 *  same as any MessageTransceiverThread object.
 */
class QMessageTransceiverThread : public QObject, public MessageTransceiverThread, public IMessageTransceiverMaster, private CountedObject<QMessageTransceiverThread>
{
   Q_OBJECT

public:
   /** Constructor.
     * @param parent Passed on to the QObject constructor
     * @param name Passed on to the QObject constructor
     */
   QMessageTransceiverThread(QObject * parent = NULL, const char * name = NULL);

   /** 
    *  Destructor.  This constructor will call ShutdownInternalThread() itself,
    *  so you don't need to call ShutdownInternalThread() explicitly UNLESS you
    *  have subclassed this class and overridden virtual methods that can get
    *  called from the internal thread -- in that case you should call 
    *  ShutdownInternalThread() yourself to avoid potential race conditions between
    *  the internal thread and your own destructor method.
    */
   virtual ~QMessageTransceiverThread();

   /** Overridden to handle signal events from our internal thread */
   virtual bool event(QEvent * event);

   /** Returns a read-only reference to our table of registered QMessageTransceiverHandler objects. */
   const Hashtable<uint32, QMessageTransceiverHandler *> GetHandlers() const {return _handlers;}

signals:
   /** Emitted when MessageReceived() is about to be emitted one or more times. */
   void BeginMessageBatch();

   /** Emitted when a new Message has been received by one of the sessions being operated by our internal thread.
     * @param msg Reference to the Message that was received.
     * @param sessionID Session ID string of the session that received the message
     */
   void MessageReceived(const MessageRef & msg, const String & sessionID);

   /** Emitted when we are done emitting MessageReceived, for the time being. */
   void EndMessageBatch();

   /** Emitted when a new Session object is accepted by one of the factories being operated by our internal thread
     * @param sessionID Session ID string of the newly accepted Session object.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     * @param iap The location of the peer that we are accepting a connection from.
     */ 
   void SessionAccepted(const String & sessionID, uint32 factoryID, const IPAddressAndPort & iap);

   /** Emitted when a session object is attached to the internal thread's ReflectServer */
   void SessionAttached(const String & sessionID);

   /** Emitted when a session object connects to its remote peer (only used by sessions that were
     * created using AddNewConnectSession())
     * @param sessionID Session ID string of the newly connected Session object.
     * @param connectedTo the IP address and port that the session is connected to.
     */
   void SessionConnected(const String & sessionID, const IPAddressAndPort & connectedTo);

   /** Emitted when a session object is disconnected from its remote peer
     * @param sessionID Session ID string of the newly disconnected Session object.
     */
   void SessionDisconnected(const String & sessionID);

   /** Emitted when a session object is removed from the internal thread's ReflectServer 
     * @param sessionID Session ID string of the newly disconnected Session object.
     */
   void SessionDetached(const String & sessionID);

   /** Emitted when a factory object is attached to the internal thread's ReflectServer.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     */
   void FactoryAttached(uint32 factoryID);

   /** Emitted when a factory object is removed from the internal thread's ReflectServer.
     * @param factoryID Factory ID of the ReflectSessionFactory that accepted the new session.
     */
   void FactoryDetached(uint32 factoryID);

   /** Emitted when the thread's internal ReflectServer object exits. */
   void ServerExited();

   /** Emitted when the output-queues of the sessions specified in a previous call to 
     * RequestOutputQueuesDrainedNotification() have drained.  Note that this signal only 
     * gets emitted once per call to RequestOutputQueuesDrainedNotification();
     * it is not emitted spontaneously.
     * @param ref MessageRef that you previously specified in RequestOutputQueuesDrainedNotification().
     */
   void OutputQueuesDrained(const MessageRef & ref);

   /** This signal is called for all events send by the internal thread.  You can use this
     * to catch custom events that don't have their own signal defined above, or if you want to
     * receive all thread events via a single slot.
     * @param code the MTT_EVENT_* code of the new event.
     * @param optMsg If a Message is relevant, this will contain it; else it's a NULL reference.
     * @param optFromSession If a session ID is relevant, this is the session ID; else it will be "".
     * @param optFromFactory If a factory is relevant, this will be the factory's ID number; else it will be zero.
     */
   void InternalThreadEvent(uint32 code, const MessageRef & optMsg, const String & optFromSession, uint32 optFromFactory);

public slots:
   /**
    * This method is the same as the MessageTransceiverThread::SendMessageToSessions();
    * it's reimplemented here as a pass-through merely so it can be a slot.
    * Enqueues the given message for output by one or more of our attached sessions.
    * @param msgRef a reference to the Message to send out.
    * @param optDistPath if non-NULL, then only sessions that contain at least one node that matches this
    *                    path will receive the Message.  Otherwise all sessions will receive the Message.
    * @return B_NO_ERROR on success, B_ERROR if out of memory.
    */
   status_t SendMessageToSessions(const MessageRef & msgRef, const char * optDistPath = NULL);

   /** Parses through the incoming-events queue and emits signals as appropriate.
     * Typically this method is called when appropriate by the event() method,
     * so you don't need to call it yourself unless you are handling event notification
     * in some custom fashion.
     */
   virtual void HandleQueuedIncomingEvents();

   /** Overridden to also call Reset() on any QMessageTransceiverHandlers we have registered.  */
   virtual void Reset();

protected:
   /** Overridden to send a QEvent */
   virtual void SignalOwner();

   virtual QMessageTransceiverThread * ObtainThread() {return this;}
   virtual status_t RegisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sessionRef);
   virtual void UnregisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, bool emitEndMessageBatchIfNecessary);

private:
   friend class QMessageTransceiverHandler;
   friend class QMessageTransceiverThreadPool;

   void FlushSeenHandlers(bool doEmit) {while(_firstSeenHandler) RemoveFromSeenList(_firstSeenHandler, doEmit);}
   void RemoveFromSeenList(QMessageTransceiverHandler * h, bool doEmit);

   IMessageTransceiverMaster * RegisterHandler(QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sref);
   void UnregisterHandler(IMessageTransceiverMaster * handler);

   Hashtable<uint32, QMessageTransceiverHandler *> _handlers;  // registered handlers, by ID

   QMessageTransceiverHandler * _firstSeenHandler;
   QMessageTransceiverHandler * _lastSeenHandler;
};

/** This class represents a demand-allocated pool of QMessageTransceiverThread objects.
  * By instantiating a QMessageTransceiverThreadPool object and passing it into your
  * QMessageTransceiverHandler::SetupAs*() methods instead of passing in a QMessageTransceiverThread
  * directly, you will have a system where threads are demand-allocated as necessary.
  * You can specify how many QMessageTransceiverHandler objects may share a single
  * QMessageTransceiverThread at once.
  */
class QMessageTransceiverThreadPool : public IMessageTransceiverMaster, private CountedObject<QMessageTransceiverThreadPool>
{
public:
   /**
     * Constructor.  Creates a thread pool where each thread will be allowed to have
     * at most (maxSessionsPerThread) sessions associated with it.  If more than that
     * number of sessions are attached to the pool, another thread will be created to
     * handle the extra sessions (and so on).
     * @param maxSessionsPerThread The maximum number of sessions to add to each 
     *                             thread in the pool. Defaults to 32.
     */
   QMessageTransceiverThreadPool(uint32 maxSessionsPerThread = 32);

   /** Destructor.  Deletes all QMessageTransceiverThread objects in the pool. 
     * Any QMessageTransceiverHandlers still attached to those threads will be detached.
     */
   virtual ~QMessageTransceiverThreadPool();
 
   /** Call this to shut down and delete all QMessageTransceiverThread objects in this pool. 
     * Any QMessageTransceiverHandlers still attached to those threads will be detached.
     */
   void ShutdownAllThreads();

protected:
   virtual QMessageTransceiverThread * ObtainThread();
   virtual status_t RegisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, const ThreadWorkerSessionRef & sessionRef);
   virtual void UnregisterHandler(QMessageTransceiverThread & thread, QMessageTransceiverHandler * handler, bool emitEndMessageBatchIfNecessary);

   /** Should create and return a new QMessageTransceiverThread object.
     * Broken out into a virtual method so that subclasses may alter its behavior if necessary.
     * Default implementation creates and returns a new QMessageTransceiverThread object.
     */
   virtual QMessageTransceiverThread * CreateThread();

private:
   uint32 _maxSessionsPerThread;
   Hashtable<QMessageTransceiverThread *, Void> _threads;
};

/**
 * This class can be used in conjunction with the QMessageTransceiverThread class to make
 * it easier to manage multiple sessions inside a single I/O thread.  You can create multiple
 * QMessageTransceiverHandler objects that are associated with the same QMessageTransceiverThread,
 * and they will each act similarly to a QMessageTransceiverThread, except that their
 * I/O operations will all execute within the single, shared QMessageTransceiverThread context.
 * The QMessageTransceiverHandler class handles all the necessary multiplexing/demultiplexing
 * of commands so that your code doesn't have to worry about it.  This class is useful if you
 * are creating lots and lots of network connections and want to limit the number of
 * I/O threads you create... however it is still perfectly possible to use the QMessageTransceiverThread
 * class directly for everything and ignore this class, if you prefer -- this class is here just as
 * an optimization.
 */
class QMessageTransceiverHandler : public QObject, private CountedObject<QMessageTransceiverHandler>
{
   Q_OBJECT

public:
   /** Constructor.  After creating this object, you'll want to call one of the SetupAs*() methods
     *               on this object to associate it with a QMessageTransceiverThread object and assign it a role.
     * @param parent Passed on to the QObject constructor
     * @param name Passed on to the QObject constructor
     */
   QMessageTransceiverHandler(QObject * parent = NULL, const char * name = NULL);

   /** 
    *  Destructor.  This destructor will unregister us with our QMessageTransceiverThread object.
    */
   virtual ~QMessageTransceiverHandler();

   /**
     * Associates this handler with a specified IMessageTransceiverMaster, and tells it to use
     * the specified socket for its I/O.  If the handler was already associated with a IMessageTransceiverMaster,
     * the previous association will be removed first.
     * May be called at any time, but behaves slightly differently depending on whether the internal
     * thread is running or not.  If the internal thread is running, the session will be added asynchronously
     * to the server.  If not, the call is immediately passed on through to ReflectServer::AddNewSession(). 
     * @param master the IMessageTransceiverMaster to bind this handler to.
     * @param socket The TCP socket that the new session will be using, or -1, if the new session is to have no
     *               associated TCP connection.  This socket becomes property of this object on success.
     * @param optSessionRef Optional reference for a session to add.  If it's a NULL reference, a default ThreadWorkerSession
     *                      will be created and used.  If you do specify a session here, you will want to use either a 
     *                      ThreadWorkerSession, a subclass of ThreadWorkerSession, or at least something that acts 
     *                      like one, or else things won't work correctly.
     *                      The referenced session becomes sole property of the MessageTransceiverThread on success.
     * @return B_NO_ERROR on success, or B_ERROR on failure.  Note that if the internal thread is currently running,
     *         then success merely indicates that the add command was enqueued successfully, not that it was executed (yet).
     */  
   virtual status_t SetupAsNewSession(IMessageTransceiverMaster & master, const ConstSocketRef & socket, const ThreadWorkerSessionRef & optSessionRef);

   /** Convenience method -- calls the above method with a NULL session reference. */
   status_t SetupAsNewSession(IMessageTransceiverMaster & master, const ConstSocketRef & socket) {return SetupAsNewSession(master, socket, ThreadWorkerSessionRef());}

   /**
     * Associates this handler with a specified IMessageTransceiverMaster, and tells it to connect to
     * the specified host IP address and port.  If the handler was already associated with a IMessageTransceiverMaster,
     * the previous association will be removed first.
     * May be called at any time, but behaves slightly differently depending on whether the internal
     * thread is running or not.  If the internal thread is running, the session will be added asynchronously
     * to the server.  If not, the call is immediately passed on through to ReflectServer::AddNewConnectSession(). 
     * @param master the IMessageTransceiverMaster to bind this handler to.
     * @param targetIPAddress IP address to connect to
     * @param port Port to connect to at that IP address.
     * @param optSessionRef optional Reference for a session to add.  If it's a NULL reference, a default ThreadWorkerSession 
     *                      will be created and used.  If you do specify a session here, you will want to use either a 
     *                      ThreadWorkerSession, a subclass of ThreadWorkerSession, or at least something that acts 
     *                      like one, or things won't work correctly.
     *                      The referenced session becomes sole property of the MessageTransceiverThread on success.
     * @param autoReconnectDelay If specified, this is the number of microseconds after the
     *                           connection is broken that an automatic reconnect should be
     *                           attempted.  If not specified, an automatic reconnect will not
     *                           be attempted, and the session will be removed when the
     *                           connection breaks.  Specifying this is equivalent to calling
     *                           SetAutoReconnectDelay() on (optSessionRef).
     * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
     *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
     *                              and the TCP connection still has not been established, we will force the connection attempt to
     *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
     *                              is used; typically this means that it will be left up to the operating system how long to wait
     *                              before timing out the connection attempt.
     * @return B_NO_ERROR on success, or B_ERROR on failure.  Note that if the internal thread is currently running,
     *         then success merely indicates that the add command was enqueued successfully, not that it was executed (yet).
     */
   virtual status_t SetupAsNewConnectSession(IMessageTransceiverMaster & master, const ip_address & targetIPAddress, uint16 port, const ThreadWorkerSessionRef & optSessionRef, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS);

   /** Convenience method -- calls the above method with a NULL session reference. */
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster & master, const ip_address & targetIPAddress, uint16 port, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS) {return SetupAsNewConnectSession(master, targetIPAddress, port, ThreadWorkerSessionRef(), autoReconnectDelay, maxAsyncConnectPeriod);}

   /**
     * Associates this handler with a specified IMessageTransceiverMaster, and tells it to connect to
     * the specified host name and port.  If the handler was already associated with a IMessageTransceiverMaster,
     * the previous association will be removed first.
     * May be called at any time, but behaves slightly differently depending on whether the internal
     * thread is running or not.  If the internal thread is running, the session will be added asynchronously
     * to the server.  If not, the call is passed immediately on through to ReflectServer::AddNewConnectSession(). 
     * @param master the IMessageTransceiverMaster to bind this handler to.
     * @param targetHostName ASCII hostname or ASCII IP address to connect to.  (e.g. "blah.com" or "132.239.50.8")
     * @param port Port to connect to at that IP address.
     * @param optSessionRef optional Reference for a session to add.  If it's a NULL reference, a default ThreadWorkerSession 
     *                      will be created and used.  If you do specify session here, you will want to use either a 
     *                      ThreadWorkerSession, a subclass of ThreadWorkerSession, or at least something that acts
     *                      like one, or things won't work correctly.
     *                      The referenced session becomes sole property of the MessageTransceiverThread on success.
     * @param expandLocalhost Passed to GetHostByName().  See GetHostByName() documentation for details.  Defaults to false.
     * @param autoReconnectDelay If specified, this is the number of microseconds after the
     *                           connection is broken that an automatic reconnect should be
     *                           attempted.  If not specified, an automatic reconnect will not
     *                           be attempted, and the session will be removed when the
     *                           connection breaks.  Specifying this is equivalent to calling
     *                           SetAutoReconnectDelay() on (optSessionRef).
     * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
     *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
     *                              and the TCP connection still has not been established, we will force the connection attempt to
     *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
     *                              is used; typically this means that it will be left up to the operating system how long to wait
     *                              before timing out the connection attempt.
     * @return B_NO_ERROR on success, or B_ERROR on failure.  Note that if the internal thread is currently running,
     *         then success merely indicates that the add command was enqueued successfully, not that it was executed (yet).
     */
   virtual status_t SetupAsNewConnectSession(IMessageTransceiverMaster & master, const String & targetHostName, uint16 port, const ThreadWorkerSessionRef & optSessionRef, bool expandLocalhost = false, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS);

   /** Convenience method -- calls the above method with a NULL session reference. */
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster & master, const String & targetHostName, uint16 port, bool expandLocalhost = false, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS) {return SetupAsNewConnectSession(master, targetHostName, port, ThreadWorkerSessionRef(), expandLocalhost, autoReconnectDelay, maxAsyncConnectPeriod);}

   /**
     * This method is that Requests that the MessageTranceiverThread object send us a MTT_EVENT_OUTPUT_QUEUES_DRAINED event
     * when this handler's outgoing message queues has become empty.
     * @param notificationMsg MessageRef to return with the MTT_EVENT_OUTPUT_QUEUES_DRAINED event.  May be a NULL ref.
     * @param optDrainTag If non-NULL, this DrainTag will be used to track drainage, instead of a 
     *                    default one.  Don't supply a value for this argument unless you think you 
     *                    know what you are doing.  ;^)  On success, (optDrainTag) becomes property
     *                    of this MessageTransceiverThread.
     * @returns B_NO_ERROR on success (in which case an MTT_EVENT_OUTPUT_QUEUES_DRAINED event will be
     *          forthcoming) or B_ERROR on error (out of memory).
     */
   status_t RequestOutputQueueDrainedNotification(const MessageRef & notificationMsg, DrainTag * optDrainTag = NULL);

   /**
     * Tells this handler's worker session to install a new input IOPolicy.
     * @param pref Reference to the new IOPolicy object.  Since IOPolicies are generally
     *             not thread safe, the referenced IOPolicy should not be used after it has
     *             been successfully passed in via this call.  May be a NULL ref to remove
     *             the existing input policy.
     * @return B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t SetNewInputPolicy(const AbstractSessionIOPolicyRef & pref);

   /**
     * Tells this handler's worker session to install a new output IOPolicy.
     * @param pref Reference to the new IOPolicy object.  Since IOPolicies are generally
     *             not thread safe, the referenced IOPolicy should not be used after it has
     *             been successfully passed in via this call.  May be a NULL ref to remove
     *             the existing output policy.
     * @return B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t SetNewOutputPolicy(const AbstractSessionIOPolicyRef & pref);

   /**
     * Tells this handler's worker session to switch to a different message encoding 
     * for the Messages they are sending to the network.  Note that this only works if
     * the worker session is using the usual MessageIOGateways for their I/O.
     * Note that ZLIB encoding is only enabled if your program is compiled with the
     * -DMUSCLE_ENABLE_ZLIB_ENCODING compiler flag set.
     * @param encoding one of the MUSCLE_MESSAGE_ENCODING_* constant declared in MessageIOGateway.h
     * @return B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t SetOutgoingMessageEncoding(int32 encoding);

   /** Returns this handler's current session ID, or -1 if this handler is not currently associated with a thread. */
   int32 GetSessionID() const {return _sessionID;}

   /** Returns a pointer to this handler's associated QMessageTransceiverThread, or NULL if the handler
     * is not currently associated with any thread.
     */
   QMessageTransceiverThread * GetThread() {return _mtt;}

   /** Returns a pointer to this handler's associated QMessageTransceiverThread, or NULL if the handler
     * is not currently associated with any thread.
     */
   const QMessageTransceiverThread * GetThread() const {return _mtt;}

   /** Returns a pointer to this handler's associated IMessageTransceiverMaster, or NULL if the handler
     * is not currently associated with any master.
     */
   IMessageTransceiverMaster * GetMaster() {return _master;}

   /** Returns a pointer to this handler's associated IMessageTransceiverMaster, or NULL if the handler
     * is not currently associated with any master.
     */
   const IMessageTransceiverMaster * GetMaster() const {return _master;}

signals:
   /** Emitted when MessageReceived() is about to be emitted one or more times by this handler. */
   void BeginMessageBatch();

   /** Emitted when a new Message has been received by this handler.
     * @param msg Reference to the Message that was received.
     */
   void MessageReceived(const MessageRef & msg);

   /** Emitted when this handler is done emitting MessageReceived(), for the time being. */
   void EndMessageBatch();

   /** Emitted when this handler's session object has attached to the I/O thread's ReflectServer */
   void SessionAttached();

   /** Emitted when this handler's session object has connected to its remote peer 
     * (only used by sessions that were created in connect-mode)
     * The IP address and port that the session is connected to.
     */
   void SessionConnected(const IPAddressAndPort & connectedTo);

   /** Emitted when this handler's session object has disconnected from its remote peer */
   void SessionDisconnected();

   /** Emitted when this handler's session object has been removed from the I/O thread's ReflectServer */
   void SessionDetached();

   /** Emitted when this handler's output queue has been drained, after a call to RequestOutputQueueDrainedNotification() */
   void OutputQueueDrained(const MessageRef & drainMsg);

   /** This signal is called for all events associated with this handler.  You can use this
     * to catch custom events that don't have their own signal defined above, or if you want to
     * receive all thread events via a single slot.
     * @param code the MTT_EVENT_* code of the new event.
     * @param optMsg If a Message is relevant, this will contain it; else it's a NULL reference.
     */
   void InternalHandlerEvent(uint32 code, const MessageRef & optMsg);

public slots:
   /**
    * Sends the specified Message to our session inside the I/O thread.
    * @param msgRef a reference to the Message to send out.
    * @return B_NO_ERROR on success, B_ERROR if out of memory.
    */
   status_t SendMessageToSession(const MessageRef & msgRef);

   /** Reverts this handler to its default state (as if it had just been constructed).
     * If the handler was associated with any IMessageTransceiverMaster, this will cleanly
     * disassociate it from the thread and close any connection that it represented.
     * @param emitEndMessageBatchIfNecessary If true, and we are currently in the middle of
     *                                       a message-batch, then this method will emit an
     *                                       EndMessageBatch() signal before dissasociating itself
     *                                       from the IMessageTransceiverMaster.  That way the
     *                                       Reset() call won't break the rule that says that one 
     *                                       EndMessageBatch() signal must always be emitted for
     *                                       every one BeginMessageBatch() signal.
     */
   virtual void Reset(bool emitEndMessageBatchIfNecessary = true);

protected:
   /** Called when we need a worker session inside one of the SetupAs*() methods. (i.e. if the
     * user didn't provide a worker session manually).  Default implementation simply calls
     * thread.CreateDefaultWorkerSession(), but the call has been broken out into a separate
     * virtual method so that subclasses can override if they need to.
     * @param thread the QMessageTransceiverThread object we are going to be associated with.
     * @returns a new AbstractReflectSession object on success, or NULL on failure.
     */
   virtual ThreadWorkerSessionRef CreateDefaultWorkerSession(QMessageTransceiverThread & thread);

private:
   /** These method are here (and private, and unimplemented) just to make sure the developer doesn't forget to include the (expandLocalhost) argument. */
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster &, const String &, uint16, uint64);           // deliberately private and unimplemented!
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster &, const String &, uint16, uint64, uint64);   // deliberately private and unimplemented!
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster &, const String &, uint16, const ThreadWorkerSessionRef &, uint64);          // deliberately private and unimplemented!
   status_t SetupAsNewConnectSession(IMessageTransceiverMaster &, const String &, uint16, const ThreadWorkerSessionRef &, uint64, uint64);  // deliberately private and unimplemented!

   friend class QMessageTransceiverThread;
   friend class QMessageTransceiverThreadPool;

   // These methods will be called by our QMessageTransceiverThread only
   void HandleIncomingEvent(uint32 code, const MessageRef & msgRef, const IPAddressAndPort & iap);
   inline void EmitBeginMessageBatch() {emit BeginMessageBatch();}
   inline void EmitEndMessageBatch() {emit EndMessageBatch();}

   void ClearRegistrationFields();

   IMessageTransceiverMaster * _master;  
   QMessageTransceiverThread * _mtt;     // _mtt and _master may or may not point to the same object

   int32 _sessionID;  // will be set by our _mtt when we register with it
   String _sessionTargetString;  // cached for convenience... "/*/[_sessionID]"

   QMessageTransceiverHandler * _prevSeen;  // used by _mtt for a quickie linked list
   QMessageTransceiverHandler * _nextSeen;  // used by _mtt for a quickie linked list
};

};  // end namespace muscle;

#endif
