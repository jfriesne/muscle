/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAbstractReflectSession_h
#define MuscleAbstractReflectSession_h

#include "iogateway/AbstractMessageIOGateway.h"
#include "reflector/AbstractSessionIOPolicy.h"
#include "reflector/ServerComponent.h"
#include "support/TamperEvidentValue.h"
#include "util/Queue.h"
#include "util/RefCount.h"

namespace muscle {

/** This is an interface for an object that knows how to create new
 *  AbstractReflectSession objects when needed.  It is used by the
 *  ReflectServer classes to generate sessions when connections are received.
 */
class ReflectSessionFactory : public ServerComponent
{
public:
   /** Constructor.  Our globally unique factory ID is assigned here. */
   ReflectSessionFactory();

   /** Destructor */
   virtual ~ReflectSessionFactory() {/* empty */}

   /** Should be overriden to return a new ReflectSession object, or NULL on failure.
    *  @param clientAddress A string representing the connecting client's host (typically an IP address, eg "192.168.1.102")
    *  @param factoryInfo The IP address and port number of the local network interface on which this connection was received.
    *  @returns a reference to a freshly allocated AbstractReflectSession object on success, or a NULL reference on failure.
    */
   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo) = 0;

   /**
    * Should return true iff this factory is currently ready to accept connections.
    * This method is called each time through the event loop, and if this method
    * is overridden to return false, then this factory will not be included in the
    * select() set and any incoming TCP connections on this factory's port will
    * not be acted upon (ie they will be forced to wait).
    * Default implementation always returns true.
    */
   MUSCLE_NODISCARD virtual bool IsReadyToAcceptSessions() const {return true;}

   /**
    * Returns an auto-assigned ID value that represents this factory.
    * The returned value is guaranteed to be unique across all factories in the server.
    */
   MUSCLE_NODISCARD uint32 GetFactoryID() const {return _id;}

   /** Returns the GetRunTime64()-style timestamp of the time at which an incoming TCP connection was most recently
     * considered, or MUSCLE_TIME_NEVER if nobody has ever tried to connect to this factory's port.
     * @note this timestamp is updated even if the factory decides not to create a session object for the TCP session.
     */
   MUSCLE_NODISCARD uint64 GetMostRecentAcceptTimeStamp() const {return _mostRecentAcceptTimeStamp;}

   /** Returns the number of times a TCP connection has been accepted by this factory.
     * @note this count increments even if the factory decides not to create a session object for the TCP session.
     */
   MUSCLE_NODISCARD uint32 GetAcceptCount() const {return _acceptCount;}

protected:
   /**
    * Convenience method:  Calls MessageReceivedFromFactory() on all session
    * objects.  Saves you from having to do your own iteration every time you
    * want to broadcast something.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    */
   void BroadcastToAllSessions(const MessageRef & msgRef, void * userData = NULL);

   /**
    * Convenience method:  Calls MessageReceivedFromFactory() on all session
    * objects of the specified type.  Saves you from having to do your own iteration every time you
    * want to broadcast something.
    * @tparam SessionType the type of session object you want to send the specified Message to.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    */
   template <class SessionType> void BroadcastToAllSessionsOfType(const MessageRef & msgRef, void * userData = NULL)
   {
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         SessionType * session = dynamic_cast<SessionType *>(iter.GetValue()());
         if (session) session->MessageReceivedFromFactory(*this, msgRef, userData);
      }
   }

   /**
    * Convenience method:  Calls MessageReceivedFromFactory() on all session-factory
    * objects.  Saves you from having to do your own iteration every time you
    * want to broadcast something to the factories.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    * @param includeSelf Whether or not MessageReceivedFromFactory() should be called on 'this' factory.  Defaults to true.
    */
   void BroadcastToAllFactories(const MessageRef & msgRef, void * userData = NULL, bool includeSelf = true);

private:
   friend class ReflectServer;

   uint32 _id;
   uint64 _mostRecentAcceptTimeStamp;
   uint32 _acceptCount;

   DECLARE_COUNTED_OBJECT(ReflectSessionFactory);
};
DECLARE_REFTYPES(ReflectSessionFactory);

/** This is a partially specialized factory that knows how to act as a facade for a "slave" factory.
  * In particular, it contains implementations of AttachedToServer() and AboutToDetachFromServer()
  * that set up and tear down the slave factory appropriately.  This way that logic doesn't have
  * to be replicated in every ReflectSessionFactory subclass that wants to use the facade pattern.
  */
class ProxySessionFactory : public ReflectSessionFactory
{
public:
   /** Constructor.  Makes this factory a facade for (slaveRef)
     * @param slaveRef the child factor that we will pass our calls on to.
     */
   ProxySessionFactory(const ReflectSessionFactoryRef & slaveRef) : _slaveRef(slaveRef) {/* empty */}
   virtual ~ProxySessionFactory() {/* empty */}

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   MUSCLE_NODISCARD virtual bool IsReadyToAcceptSessions() const {return _slaveRef() ? _slaveRef()->IsReadyToAcceptSessions() : true;}

   /** Returns the reference to the "slave" factory that was passed in to our constructor. */
   MUSCLE_NODISCARD const ReflectSessionFactoryRef & GetSlave() const {return _slaveRef;}

private:
   ReflectSessionFactoryRef _slaveRef;
};
DECLARE_REFTYPES(ProxySessionFactory);

/** This is the abstract base class that defines the server side logic for a single
 *  client-server connection.  This class contains no message routing logic of its own,
 *  but defines the interface so that subclasses can do so.
 */
class AbstractReflectSession : public ServerComponent, public AbstractGatewayMessageReceiver
{
public:
   /** Default Constructor. */
   AbstractReflectSession();

   /** Destructor. */
   virtual ~AbstractReflectSession();

   /** Returns the hostname of this client that is associated with this session.
     * May only be called if this session is currently attached to a ReflectServer.
     */
   MUSCLE_NODISCARD const String & GetHostName() const;

   /** Returns the server-side port that this session was accepted on, or 0 if
     * we weren't accepted from a port (eg we were created locally)
     * May only be called if this session is currently attached to a ReflectServer.
     */
   MUSCLE_NODISCARD uint16 GetPort() const;

   /** Returns the server-side network interface IP that this session was accepted on,
     * or 0 if we weren't created via accepting a network connection  (eg we were created locally)
     * May only be called if this session is currently attached to a ReflectServer.
     */
   MUSCLE_NODISCARD const IPAddress & GetLocalInterfaceAddress() const;

   /** Returns a globally unique ID for this session. */
   MUSCLE_NODISCARD uint32 GetSessionID() const {return _sessionID;}

   /**
    * Returns an ID string to represent this session with.
    * (This string is the ASCII representation of GetSessionID())
    */
   MUSCLE_NODISCARD const String & GetSessionIDString() const {return _idString;}

   /** Marks this session for immediate termination and removal from the server. */
   virtual void EndSession();

   /** Forces the disconnection of this session's TCP connection to its client.
    *  Calling this will cause ClientConnectionClosed() to be called, as if the
    *  TCP connection had been severed externally.
    *  @returns the value that ClientConnectionClosed() returned.
    */
   bool DisconnectSession();

   /**
    * Causes this session to be terminated (similar to EndSession(),
    * and the session specified in (newSessionRef) to take its
    * place using the same socket connection & message IO gateway.
    * @param newSession the new session object that is to take the place of this one.
    * @return B_NO_ERROR on success, an error code if the new session refused to be attached.
    */
   status_t ReplaceSession(const AbstractReflectSessionRef & newSession);

   /**
    * Called when the TCP connection to our client is broken.
    * If this method returns true, then this session will be removed and
    * deleted.
    * @return If it returns false, then this session will continue, even
    *         though the client is no longer available.  Default implementation
    *         always returns true, unless the automatic-reconnect feature has
    *         been enabled (via SetAutoReconnectDelay()), in which case this
    *         method will return false and try to Reconnect() again, instead.
    */
   MUSCLE_NODISCARD virtual bool ClientConnectionClosed();

   /**
    * For sessions that were added to the server with AddNewConnectSession(),
    * or AddNewDormantConnectSession(), this method is called when the asynchronous
    * connect process completes successfully.  (if the asynchronous connect fails,
    * ClientConnectionClosed() is called instead).  Default implementation just sets
    * an internal flag that governs whether an error message should be printed when
    * the session is disconnected later on.
    */
   virtual void AsyncConnectCompleted();

   /**
    * Set a new input I/O policy for this session.
    * @param newPolicy Reference to the new policy to use to control the incoming byte stream
    *                  for this session.  May be a NULL reference if you just want to remove the existing policy.
    */
   void SetInputPolicy(const AbstractSessionIOPolicyRef & newPolicy);

   /** Returns a reference to the current input policy for this session.
     * May be a NULL reference, if there is no input policy installed (which is the default state)
     */
   const AbstractSessionIOPolicyRef & GetInputPolicy() const {return _inputPolicyRef;}

   /**
    * Set a new output I/O policy for this session.
    * @param newPolicy Reference to the new policy to use to control the outgoing byte stream
    *                 for this session.  May be a NULL reference if you just want to remove the existing policy.
    */
   void SetOutputPolicy(const AbstractSessionIOPolicyRef & newPolicy);

   /** Returns a reference to the current output policy for this session.  May be a NULL reference.
     * May be a NULL reference, if there is no output policy installed (which is the default state)
     */
   const AbstractSessionIOPolicyRef & GetOutputPolicy() const {return _outputPolicyRef;}

   /** Installs the given AbstractMessageIOGateway as the gateway we should use for I/O.
     * If this method isn't called, the ReflectServer will call our CreateGateway() method
     * to set our gateway for us when we are attached.
     * @param ref Reference to the I/O gateway to use, or a NULL reference to remove any gateway we have.
     */
   void SetGateway(const AbstractMessageIOGatewayRef & ref) {_gateway = ref; _outputStallLimit = _gateway()?_gateway()->GetOutputStallLimit():MUSCLE_TIME_NEVER;}

   /**
    * Returns a reference to our internally held message IO gateway object,
    * or NULL reference if there is none.  The returned gateway remains
    * the property of this session.
    */
   MUSCLE_NODISCARD const AbstractMessageIOGatewayRef & GetGateway() const {return _gateway;}

   /**
     * Convenience method:  returns a reference DataIO object attached to this session's
     * current gateway object, or NULL if there isn't one.
     */
   MUSCLE_NODISCARD const DataIORef & GetDataIO() const;

   /** Should return true iff we have data pending for output.
    *  Default implementation calls HasBytesToOutput() on our installed AbstractDataIOGateway object,
    *  if we have one, or returns false if we don't.
    */
   MUSCLE_NODISCARD virtual bool HasBytesToOutput() const;

   /**
     * Should return true iff we are willing to read more bytes from our
     * client connection at this time.  Default implementation calls
     * IsReadyForInput() on our install AbstractDataIOGateway object, if we
     * have one, or returns false if we don't.
     *
     */
   MUSCLE_NODISCARD virtual bool IsReadyForInput() const;

   /** Called by the ReflectServer when it wants us to read some more bytes from our client.
     * Default implementation simply calls DoInput() on our Gateway object (if any).
     * @param receiver Object to call CallMessageReceivedFromGateway() on when new Messages are ready to be looked at.
     * @param maxBytes Maximum number of bytes to read before returning.
     * @returns The total number of bytes read, or an error code if there was a fatal error.
     */
   virtual io_status_t DoInput(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes);

   /** Called by the ReflectServer when it wants us to push some more bytes out to our client.
     * Default implementation simply calls DoOutput() on our Gateway object (if any).
     * @param maxBytes Maximum number of bytes to write before returning.
     * @returns The total number of bytes written, or an error code if there was a fatal error.
     */
   virtual io_status_t DoOutput(uint32 maxBytes);

   /** Socket factory method.  This method is called by AddNewSession() when
    *  no valid Socket was supplied as an argument to the AddNewSession() call.
    *  This method should either create and supply a default Socket, or return
    *  a NULL ConstSocketRef.  In the latter case, the session will run without any
    *  connection to a client.
    *  Default implementation always returns a NULL ConstSocketRef.
    */
   virtual ConstSocketRef CreateDefaultSocket();

   /** DataIO factory method.  Should return a new non-blocking DataIO
    *  object for our gateway to use, or NULL on failure.  Called by
    *  ReflectServer when this session is added to the server.
    *  The default implementation returns a non-blocking TCPSocketDataIO
    *  object, which is the correct behaviour 99% of the time.
    *  @param socket The socket to provide the DataIO object for.
    *                On success, the DataIO object becomes owner of (socket).
    *  @return A newly allocated DataIO object, or NULL on failure.
    */
   virtual DataIORef CreateDataIO(const ConstSocketRef & socket);

   /**
    * Gateway factory method.  Should return a reference to a new
    * AbstractMessageIOGateway for this session to use for communicating
    * with its remote peer.
    * Called by ReflectServer when this session object is added to the
    * server, but doesn't already have a valid gateway installed.
    * The default implementation returns a MessageIOGateway object.
    * @return a new message IO gateway object, or a NULL reference on failure.
    */
   virtual AbstractMessageIOGatewayRef CreateGateway();

   /** Overridden to support auto-reconnect via SetAutoReconnectDelay() */
   MUSCLE_NODISCARD virtual uint64 GetPulseTime(const PulseArgs &);

   /** Overridden to support auto-reconnect via SetAutoReconnectDelay() */
   virtual void Pulse(const PulseArgs &);

   /** Convenience method -- returns a human-readable string describing our
    *  session-type, our session ID, and the client we are connected to (
    *  as returned by GetClientDescriptionString()).
    */
   String GetSessionDescriptionString() const;

   /** Returns a human-readable description of the client this session is associated with.
     * Default implementation returns the hostname/IP address and port number
     * of the client, but subclasses could override this to return something
     * else that is more descriptive.
     * @note this method is called by GetSessionDescriptionString() to help it form its return value.
     */
   virtual String GetClientDescriptionString() const;

   /** Returns the destination we connected to asynchronously. */
   MUSCLE_NODISCARD const IPAddressAndPort & GetAsyncConnectDestination() const {return _asyncConnectDest;}

   /** Sets the async-connect-destination (as returned by GetAsyncConnectDestination())
     * manually.  Typically you don't need to call this; so only call this method if you really know
     * what you are doing and why you need to.
     * @param iap The IP address and port that this session connected to asynchronously.
     * @param reconnectViaTCP If true, then any calls to Reconnect() will try to reconnect to this address via a
     *                        standard TCP connection.  If false, this address will be ignored by Reconnect().
     */
   void SetAsyncConnectDestination(const IPAddressAndPort & iap, bool reconnectViaTCP) {_asyncConnectDest = iap; _reconnectViaTCP = reconnectViaTCP;}

   /** Returns the node path of the node representing this session (eg "/192.168.1.105/17") */
   MUSCLE_NODISCARD virtual const String & GetSessionRootPath() const {return _sessionRootPath;}

   /** Sets the amount of time that should pass between when this session loses its connection
     * (that was previously set up using AddNewConnectSession() or AddNewDormantConnectSession())
     * and when it should automatically try to reconnect to that same destination (by calling Reconnect()).
     * Default setting is MUSCLE_TIME_NEVER, meaning that automatic reconnection is disabled.
     * @param delay The amount of time to delay before reconnecting, in microseconds.
     */
   void SetAutoReconnectDelay(uint64 delay) {_autoReconnectDelay = delay; InvalidatePulseTime();}

   /** Returns the current automatic-reconnect delay period, as was previously set by
     * SetAutoReconnectDelay().  Note that this setting is only relevant for sessions
     * that were attached using AddNewConnectSession() or AddNewDormantConnectSession().
     */
   MUSCLE_NODISCARD uint64 GetAutoReconnectDelay() const {return _autoReconnectDelay;}

   /** Sets the maximum time that we should allow an asynchronous-connect to continue before
     * forcibly aborting it.  Default behavior is determined by the MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS
     * compiler define, whose default value is MUSCLE_TIME_NEVER, aka let the asynchronous connect
     * go on for as long as the operating system cares to allow it to.
     * Note that this setting is only relevant for sessions that were attached using AddNewConnectSession()
     * or AddNewDormantConnectSession().
     * @param delay The new maximum connect time to allow, in microseconds, or MUSCLE_TIME_NEVER to not
     *              enforce any particular maximum connect time.
     */
   void SetMaxAsyncConnectPeriod(uint64 delay) {_maxAsyncConnectPeriod = delay; InvalidatePulseTime();}

   /** Returns the current maximum-asynchronous-connect setting, in microseconds.
     * Note that this setting is only relevant for sessions that were attached using AddNewConnectSession()
     * or AddNewDormantConnectSession().
     */
   MUSCLE_NODISCARD uint64 GetMaxAsyncConnectPeriod() const {return _maxAsyncConnectPeriod;}

   /** Returns true iff we are currently in the middle of an asynchronous TCP connection */
   MUSCLE_NODISCARD bool IsConnectingAsync() const {return _connectingAsync;}

   /** Returns true iff this session is currently connected to our remote counterpart. */
   MUSCLE_NODISCARD bool IsConnected() const {return _isConnected;}

   /** Returns true if this session was successfully connected to its remote counterpart before it became disconnected.
     * Returns false if the session never was successfully connected.
     */
   MUSCLE_NODISCARD bool WasConnected() const {return _wasConnected;}

   /** This method may be called by ReflectServer when the server process runs low on memory.
     * If it returns true this session may be disposed of in order to free up memory.  If it
     * returns false, this session will not be disposed of.
     *
     * Default return value is false, but the ReflectServer class will call SetExpendable(true)
     * on any sessions return by ReflectSessionFactory::CreateSession() method, or
     * passed to AddNewConnectSession(), unless SetExpendable() had already been explicitly
     * called on the session beforehand.
     */
   MUSCLE_NODISCARD bool IsExpendable() const {return _isExpendable;}

   /** Calls this to set or clear the isExpendable flag on a session (see IsExpendable() for details)
     * @param isExpendable If true, this session may be removed in a low-on-memory situation.  If false, it won't be.
     */
   void SetExpendable(bool isExpendable) {_isExpendable = isExpendable;}

   /**
    * Adds a MessageRef to our gateway's outgoing message queue.
    * (ref) will be sent back to our client when time permits.
    * @param ref Reference to a Message to send to our client.
    * @return B_NO_ERROR on success, B_OUT_OF_MEMORY if out-of-memory.
    */
   virtual status_t AddOutgoingMessage(const MessageRef & ref);

   /**
    * Convenience method:  Calls MessageReceivedFromSession() on all session
    * objects.  Saves you from having to do your own iteration every time you
    * want to broadcast something.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    * @param includeSelf Whether or not MessageReceivedFromSession() should be called on 'this' session.  Defaults to true.
    * @see GetSessions(), AddNewSession(), GetSession()
    */
   void BroadcastToAllSessions(const MessageRef & msgRef, void * userData = NULL, bool includeSelf = true);

   /**
    * Convenience method:  Calls MessageReceivedFromSession() on all session
    * objects of the specified type.  Saves you from having to do your own iteration every time you
    * want to broadcast something.
    * @tparam SessionType the type of session object you want to send the specified Message to.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    * @param includeSelf Whether or not MessageReceivedFromSession() should be called on 'this' session.  Defaults to true.
    */
   template <class SessionType> void BroadcastToAllSessionsOfType(const MessageRef & msgRef, void * userData = NULL, bool includeSelf=true)
   {
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         SessionType * session = dynamic_cast<SessionType *>(iter.GetValue()());
         if ((session)&&((includeSelf)||(session != this))) session->MessageReceivedFromSession(*this, msgRef, userData);
      }
   }

   /**
    * Convenience method:  Calls MessageReceivedFromSession() on all installed
    * session-factory objects.  Saves you from having to do your own iteration
    * every time you want to broadcast something to the factories.
    * @param msgRef a reference to the Message you wish to broadcast
    * @param userData any userData value you care to include.  Defaults to NULL.
    * @see GetFactories(), PutAcceptFactory(), GetFactory()
    */
   void BroadcastToAllFactories(const MessageRef & msgRef, void * userData = NULL);

   /**
    * Closes this session's current TCP connection (if any), and creates a new
    * TCP socket that will then try to asynchronously connect back to the previous
    * socket's host and port.  Note that this method is primarily useful for sessions
    * that were added with AddNewConnectSession() or AddNewDormantConnectSession();
    * for other types of session, it will just destroy this session's DataIO and IOGateway
    * and then create new ones by calling CreateDefaultSocket() and CreateDataIO().
    * @note This method will call CreateDataIO() to make a new DataIO object for the newly created socket.
    * @returns B_NO_ERROR on success, or an error code on failure.
    *          On success, the connection result will be reported back
    *          later, either via a call to AsyncConnectCompleted() (if the connection
    *          succeeds) or a call to ClientConnectionClosed() (if the connection fails)
    */
   status_t Reconnect();

   /** Convenience method:  Returns the "read" file descriptor associated with this session's
     * DataIO class, or a NULL reference if there is none.
     */
   MUSCLE_NODISCARD const ConstSocketRef & GetSessionReadSelectSocket() const;

   /** Convenience method:  Returns the "write" file descriptor associated with this session's
     * DataIO class, or a NULL reference if there is none.
     */
   MUSCLE_NODISCARD const ConstSocketRef & GetSessionWriteSelectSocket() const;

   /** Prints a report of what sessions are currently present on this server, and how
     * much memory each of them is currently using for various things.  Useful for understanding
     * what your RAM is being used for.
     * @param p the OutputPrinter to use for printing the text
     */
   void PrintSessionsInfo(const OutputPrinter & p) const;

   /** Prints a report of what ReflectSessionFactories are currently present on this server,
     * and what interfaces and ports they are listening on.
     * @param p the OutputPrinter to use for printing the text
     */
   void PrintFactoriesInfo(const OutputPrinter & p) const;

   /** Returns the GetRunTime64()-style timestamp of the when this session most recently received data,
     * or MUSCLE_TIME_NEVER if has never received any data.
     */
   MUSCLE_NODISCARD uint64 GetMostRecentInputTimeStamp() const {return _mostRecentInputTimeStamp;}

   /** Returns the GetRunTime64()-style timestamp of the when this session most recently sent data,
     * or MUSCLE_TIME_NEVER if has never sent any data.
     */
   MUSCLE_NODISCARD uint64 GetMostRecentOutputTimeStamp() const {return _mostRecentOutputTimeStamp;}

protected:
   /** Set by StorageReflectSession::AttachedToServer()
     * @param p the new session-root-path for us to use (eg "/127.0.0.1/12345")
     */
   void SetSessionRootPath(const String & p) {_sessionRootPath = p;}

   /** When a hostname is being chosen to represent this session (ie at the first
    *  level of the MUSCLE node-tree), this method is called to allow the subclass
    *  to choose a different name instead.
    *  @param ip The IP address associated with this session, or invalidIP if none is known.
    *  @param defaultHostName The hostname that the system suggests be used for this session.
    *  Default implementation just returns (defaultHostName), ie it goes with the suggested name.
    */
   virtual String GenerateHostName(const IPAddress & ip, const String & defaultHostName) const;

   /** Returns the timestamp (in microseconds, as reported by GetRunTime64()) of the most recent
     * time this session successfully sent some data to its client.
     */
   MUSCLE_NODISCARD uint64 GetLastByteOutputTimeStamp() const {return _lastByteOutputAt;}

private:
   virtual void TallySubscriberTablesInfo(uint32 & retNumCachedSubscriberTables, uint32 & tallyNumNodes, uint32 & tallyNumNodeBytes) const;  // yes, this virtual method is intentionally private!

   void SetPolicyAux(AbstractSessionIOPolicyRef & setRef, uint32 & setChunk, const AbstractSessionIOPolicyRef & newRef, bool isInput);
   void PlanForReconnect();
   void SetConnectingAsync(bool isConnectingAsync);
   MUSCLE_NODISCARD bool IsThisSessionScheduledForPostSleepReconnect() const;

   friend class ReflectServer;

   uint32 _sessionID;
   String _idString;

   IPAddressAndPort _ipAddressAndPort;

   bool _connectingAsync;
   bool _isConnected;
   uint64 _maxAsyncConnectPeriod;    // max number of microseconds to allow an async-connect to take before timing out (or MUSCLE_TIME_NEVER if no limit is enforced)
   uint64 _asyncConnectTimeoutTime;  // timestamp of when to force an async-connect-in-progress to timeout

   String _hostName;
   IPAddressAndPort _asyncConnectDest;
   bool _reconnectViaTCP;            // only valid when _asyncConnectDest is set
   AbstractMessageIOGatewayRef _gateway;
   uint64 _lastByteOutputAt;         // timestamp of last time we output a byt
   uint64 _pendingLastByteOutputAt;  // same as _lastByteOutputAt but 0 if we don't currently want to output anything
   uint32 _lastReportedQueueSize;    // used by ReflectServer.cpp to warn about growing/socket-free queues
   AbstractSessionIOPolicyRef _inputPolicyRef;
   AbstractSessionIOPolicyRef _outputPolicyRef;
   uint32 _maxInputChunk;   // as determined by our Policy object
   uint32 _maxOutputChunk;  // and stored here for convenience
   uint64 _outputStallLimit;
   bool _scratchReconnected; // scratch, watched by ReflectServer during ClientConnectionClosed() calls.
   String _sessionRootPath;

   // auto-reconnect support
   uint64 _autoReconnectDelay;
   uint64 _reconnectTime;
   bool _wasConnected;

   TamperEvidentValue<bool> _isExpendable;

   uint64 _mostRecentInputTimeStamp;
   uint64 _mostRecentOutputTimeStamp;

   DECLARE_COUNTED_OBJECT(AbstractReflectSession);
};

} // end namespace muscle

#endif
