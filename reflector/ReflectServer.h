/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleReflectServer_h
#define MuscleReflectServer_h

#include "reflector/AbstractReflectSession.h"
#include "support/NotCopyable.h"
#include "system/AtomicCounter.h"
#include "util/NestCount.h"
#include "util/SocketMultiplexer.h"

namespace muscle {

/** This class represents a MUSCLE server:  It runs on a centrally located machine,
 *  and many clients may connect to it simultaneously.  This server can then redirect messages
 *  uploaded by any client to other clients in a somewhat efficient manner.
 *  This class can be used as-is, or subclassed if necessary.
 *  There is typically only one ReflectServer object present in a given MUSCLE server program.
 */
class ReflectServer : public RefCountable, public PulseNode, private PulseNodeManager, private NotCopyable
{
public: 
   /** Constructor. */
   ReflectServer();

   /** Destructor. */
   virtual ~ReflectServer();

   /** The main loop for the message reflection server.
    *  This method will not return until the server stops running (usually due to an error).
    *  @return B_NO_ERROR if the server has decided to exit peacefully, or an error code if there was a 
    *                     fatal error during setup or execution.
    */
   virtual status_t ServerProcessLoop();

   /** This function may be called zero or more times before ServerProcessLoop()
    *  Each call adds one port that the server should listen on, and the factory object
    *  to use to create sessions when a connection is received on that port.
    *  @param port The TCP port the server will listen on.  (muscled's traditional port is 2960)
    *              If this port is zero, then the server will choose an available port number to use.
    *              If this port is the same as one specified in a previous call to PutAcceptFactory(),
    *              the old factory associated with that port will be replaced with this one.
    *  @param sessionFactoryRef Reference to a factory object that can generate new sessions when needed.
    *  @param optInterfaceIP Optional local interface address to listen on.  If not specified, or if specified
    *                        as (invalidIP), then connections will be accepted from all local network interfaces.
    *  @param optRetPort If specified non-NULL, then on success the port that the factory was bound to will
    *                    be placed into this parameter.
    *  @return B_NO_ERROR on success, an error code on failure (couldn't bind to socket?)
    */
   virtual status_t PutAcceptFactory(uint16 port, const ReflectSessionFactoryRef & sessionFactoryRef, const IPAddress & optInterfaceIP = invalidIP, uint16 * optRetPort = NULL);
    
   /** Remove a listening port callback that was previously added by PutAcceptFactory().
    *  @param port whose callback should be removed.  If (port) is set to zero, all callbacks will be removed.
    *  @param optInterfaceIP Interface(s) that the specified callbacks were assigned to in their PutAcceptFactory() call.
    *                        This parameter is ignored when (port) is zero. 
    *  @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if a factory for the specified port was not found.
    */
   virtual status_t RemoveAcceptFactory(uint16 port, const IPAddress & optInterfaceIP = invalidIP);

   /**
    * Called after the server is set up, but just before accepting any connections.
    * Should return B_NO_ERROR if it's okay to continue, or an error code to abort and shut down the server.
    * @return Default implementation returns B_NO_ERROR.
    */
   virtual status_t ReadyToRun();

   /**
    * Adds a new session that uses the given socket for I/O.
    * @param ref New session to add to the server.
    * @param socket The TCP socket that the new session should use, or a NULL reference, if the new session is to have no client connection (or use the default socket, if CreateDefaultSocket() has been overridden by the session's subclass).  Note that if the session already has a gateway and DataIO installed, then the DataIO's existing socket will be used instead, and this socket will be ignored.
    * @return B_NO_ERROR if the new session was added successfully, or an error code if there was an error setting it up.
    */
   virtual status_t AddNewSession(const AbstractReflectSessionRef & ref, const ConstSocketRef & socket);

   /** Convenience method:  Calls AddNewSession() with a NULL Socket reference, so the session's default socket
     * (obtained by calling ref()->CreateDefaultSocket()) will be used.
     * @param ref New session to add to the server
     * @return B_NO_ERROR if the new session was added successfully, or an error code if there was an error setting it up.
     */
   status_t AddNewSession(const AbstractReflectSessionRef & ref) {return AddNewSession(ref, ConstSocketRef());}

   /**
    * Like AddNewSession(), only creates a session that connects asynchronously to
    * the given IP address.  AttachedToServer() will be called immediately on the
    * session, and then when the connection is complete, AsyncConnectCompleted() will
    * be called.  Other than that, however, (session) will behave like any other session,
    * except any I/O messages for the client won't be transferred until the connection
    * completes.
    * @param ref New session to add to the server.
    * @param targetIPAddress IP address to connect to
    * @param port Port to connect to at that IP address.
    * @param autoReconnectDelay If specified, this is the number of microseconds after the
    *                           connection is broken that an automatic reconnect should be
    *                           attempted.  If not specified, an automatic reconnect will not
    *                           be attempted, and the session will be removed when the
    *                           connection breaks.  Specifying this is equivalent to calling
    *                           SetAutoReconnectDelay() on (ref).
    * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
    *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
    *                              and the TCP connection still has not been established, we will force the connection attempt to
    *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
    *                              is used; typically this means that it will be left up to the operating system how long to wait
    *                              before timing out the connection attempt.
    * @return B_NO_ERROR if the session was successfully added, or an error code on error.
    */
   status_t AddNewConnectSession(const AbstractReflectSessionRef & ref, const IPAddress & targetIPAddress, uint16 port, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS); 

   /**
    * Like AddNewConnectSession(), except that the added session will not initiate
    * a TCP connection to the specified address immediately.  Instead, it will just
    * hang out and do nothing until you call Reconnect() on it.  Only then will it
    * create the TCP connection to the address specified here.
    * @param ref New session to add to the server.
    * @param targetIPAddress IP address to connect to
    * @param port Port to connect to at that IP address.
    * @param autoReconnectDelay If specified, this is the number of microseconds after the
    *                           connection is broken that an automatic reconnect should be
    *                           attempted.  If not specified, an automatic reconnect will not
    *                           be attempted, and the session will be removed when the
    *                           connection breaks.  Specifying this is equivalent to calling
    *                           SetAutoReconnectDelay() on (ref).
    * @param maxAsyncConnectPeriod If specified, this is the maximum time (in microseconds) that we will
    *                              wait for the asynchronous TCP connection to complete.  If this amount of time passes
    *                              and the TCP connection still has not been established, we will force the connection attempt to
    *                              abort.  If not specified, the default value (as specified by MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS)
    *                              is used; typically this means that it will be left up to the operating system how long to wait
    *                              before timing out the connection attempt.
    * @return B_NO_ERROR if the session was successfully added, or an error code on error.
    */
   status_t AddNewDormantConnectSession(const AbstractReflectSessionRef & ref, const IPAddress & targetIPAddress, uint16 port, uint64 autoReconnectDelay = MUSCLE_TIME_NEVER, uint64 maxAsyncConnectPeriod = MUSCLE_MAX_ASYNC_CONNECT_DELAY_MICROSECONDS);

   /**
    * Should be called just before the ReflectServer is to be destroyed.
    * this in a good place to put any cleanup code.  Be sure
    * to call Cleanup() of your parent class as well!
    * (We can't just do this in the destructor, as some cleanup
    * relies on the subclass still being functional, which it isn't
    * when our destructor gets called!)
    */
   virtual void Cleanup();

   /** Accessor to our central-state repository message */
   Message & GetCentralState() {return _centralState;}

   /** Set whether or not we should log informational messages when sessions are added and removed, etc.
     * Default state is true.
     * @param log true iff we want log messages, false to suppress them
     */
   void SetDoLogging(bool log) {_doLogging = log;}

   /** Returns whether or not we should be logging informational messages. */
   bool GetDoLogging() const {return _doLogging;}

   /**
    * Returns a human-readable string that describes the type of server that is running.  
    * @return Default implementation returns "MUSCLE".
    */
   virtual const char * GetServerName() const;

   /** Returns a read-only reference to our table of sessions currently attached to this server. */
   const Hashtable<const String *, AbstractReflectSessionRef> & GetSessions() const {return _sessions;}

   /** Convenience method:  Given a session ID string, returns a reference to the session, or a NULL reference if no such session exists.
     * @param sessionName the session ID string of the session we are trying to look up (same as its numeric session ID, but in string form)
     */
   AbstractReflectSessionRef GetSession(const String & sessionName) const;

   /** Convenience method:  Given a session ID number, returns a reference to the session, or a NULL reference if no such session exists. 
     * @param sessionID the numeric session ID of the session we are trying to look up
     */
   AbstractReflectSessionRef GetSession(uint32 sessionID) const;

   /** Convenience method:  Returns a pointer to the first session of the specified type.  Returns NULL if no session of the specified type is found.
     * @note this method iterates over the session list, so it's not as efficient as one might hope.
     */
   template <class SessionType> SessionType * FindFirstSessionOfType() const
   {
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         SessionType * ret = dynamic_cast<SessionType *>(iter.GetValue()());
         if (ret) return ret;
      }
      return NULL;
   };

   /** Convenience method:  Populates the specified table with sessions of the specified session type.
     * @param results The list of matching sessions is returned here.
     * @param maxSessionsToReturn No more than this many sessions will be placed into the table.  Defaults to MUSCLE_NO_LIMIT.
     * @returns B_NO_ERROR on success, or an B_OUT_OF_MEMORY on error.
     */
   template <class SessionType> status_t FindSessionsOfType(Queue<AbstractReflectSessionRef> & results, uint32 maxSessionsToReturn = MUSCLE_NO_LIMIT) const
   {
      if (maxSessionsToReturn > 0)
      {
         for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
         {
            SessionType * ret = dynamic_cast<SessionType *>(iter.GetValue()());
            if (ret)
            {
               if (results.AddTail(iter.GetValue()).IsError()) return B_OUT_OF_MEMORY;
               if (--maxSessionsToReturn == 0) break;
            }
         }
      }
      return B_NO_ERROR;
   }

   /** Returns an iterator that allows one to iterate over all the session factories currently attached to this server. */
   const Hashtable<IPAddressAndPort, ReflectSessionFactoryRef> & GetFactories() const {return _factories;}

   /** Convenience method: Given a port number, returns a reference to the factory of that port, or a 
     *                     NULL reference if no such factory exists.
     * @param port number to check
     * @param optInterfaceIP If the factory was created to listen on a specific local interface address
     *                       (when it was passed in to PutAcceptFactory()), then specify that address again here.
     *                       Defaults to (invalidIP), indicating a factory that listens on all local network interfaces.
     */
   ReflectSessionFactoryRef GetFactory(uint16 port, const IPAddress & optInterfaceIP = invalidIP) const;

   /** Call this and the server will quit ASAP */
   void EndServer();

   /** Returns the value GetRunTime64() was at, when the ServerProcessLoop() event loop started. */
   uint64 GetServerStartTime() const {return _serverStartedAt;}

   /** Returns the number of bytes that are currently available to be allocated, or ((uint64)-1)
     * if no memory watcher was specified in the constructor.
     */
   uint64 GetNumAvailableBytes() const;
 
   /** Returns the maximum number of bytes that may be allocated at any given time, or ((uint64)-1)
     * if no memory watcher was specified in the constructor. 
     */
   uint64 GetMaxNumBytes() const;
 
   /** Returns the number of bytes that are currently allocated, or ((uint64)-1)
     * if no memory watcher was specified in the constructor. 
     */
   uint64 GetNumUsedBytes() const;

   /** Returns a reference to a table mapping IP addresses to custom strings...
     * This table may be examined or altered.  When a new connection is accepted,
     * the ReflectServer will consult this table for the address-level MUSCLE node's
     * name.  If an entry is found, it will be used verbatim; otherwise, a name will
     * be created based on the peer's IP address.  Useful for e.g. NAT remapping...
     */
   Hashtable<IPAddress, String> & GetAddressRemappingTable() {return _remapIPs;}

   /** Read-only implementation of the above */
   const Hashtable<IPAddress, String> & GetAddressRemappingTable() const {return _remapIPs;}

   /** Returns a number that is (hopefully) unique to each ReflectSession instance. 
     * This number will be different each time the server is run, but will remain the same for the duration of the server's life.
     */
   uint64 GetServerSessionID() const {return _serverSessionID;}

   /** This method is called at the beginning of each iteration of the event loop.
     * Default implementation is a no-op.
     */
   virtual void EventLoopCycleBegins() {/* empty */}

   /** This method is called at the end of each iteration of the event loop.
     * Default implementation is a no-op.
     */
   virtual void EventLoopCycleEnds() {/* empty */}

#ifdef MUSCLE_ENABLE_SSL
   /** Sets the SSL private key data that should be used to authenticate and encrypt
     * accepted incoming TCP connections.  Default state is a NULL reference (i.e. no SSL
     * encryption will be used for incoming connecitons).
     * @param privateKey Reference to the contents of a .pem file containing both
     *        a PRIVATE KEY section and a CERTIFICATE section, or a NULL reference
     *        if you want to make SSL disabled again.
     * @note this method is only available if MUSCLE_ENABLE_OPENSSL is defined.
     */
   void SetSSLPrivateKey(const ConstByteBufferRef & privateKey) {_privateKey = privateKey;}

   /** Returns the current SSL private key data, or a NULL ref if there isn't any. */
   const ConstByteBufferRef & GetSSLPrivateKey() const {return _privateKey;}

   /** Sets the SSL public key data that should be used to authenticate and encrypt
     * outgoing TCP connections.  Default state is a NULL reference (i.e. no SSL
     * encryption will be used for outgoing connections).
     * @param publicKey Reference to the contents of a .pem file containing a CERTIFICATE 
     *        section, or a NULL reference if you want to make SSL disabled again.
     * @note this method is only available if MUSCLE_ENABLE_OPENSSL is defined.
     */
   void SetSSLPublicKeyCertificate(const ConstByteBufferRef & publicKey) {_publicKey = publicKey;}

   /** Returns the current SSL public key data, or a NULL ref if there isn't any. */
   const ConstByteBufferRef & GetSSLPublicKeyCertificate() const {return _publicKey;}

   /** Sets the SSL pre-shared-key data that should be used to authenticate and
     * encrypt either incoming or outgoing TCP connections.  Default state is empty strings.
     * @param userName the user name to transmit (or expect)
     * @param password the password to transmit (or expect)
     */
   void SetSSLPreSharedKeyLoginInfo(const String & userName, const String & password) {_pskUserName = userName; _pskPassword = password;}

   /** Returns the PSK user name, as previously set via SetSSLPreSharedKeyLoginInfo().  Default is an empty string */
   const String & GetSSLPreSharedKeyUserName() const {return _pskUserName;}

   /** Returns the PSK password, as previously set via SetPreSharedKeyLoginInfo().  Default is an empty string */
   const String & GetSSLPreSharedKeyPassword() const {return _pskPassword;}
#endif

   /** Returns true iff our event loop is currently blocked inside the SocketMultiplexer::WaitForEvents() call.
     * Note that calling this function from another thread is subject to race conditions, and that
     * if you call it from the thread that is running the ReflectServer's event loop it will always
     * return false, since if the thread was blocked inside WaitForEvents() you wouldn't have control
     * of the thread to call it.  This method is here to enable a separate watchdog thread to observe
     * the operations of the main thread; in general it is not necessary to call this method.
     */
   bool IsWaitingForEvents() const {return (_inWaitForEvents.GetCount() == 1);}

   /** Implemented to call DisconnectSession() on any attached TCP-based sessions, just before 
     * the computer goes to sleep, so that other computers won't have to deal with moribund 
     * TCP connections that this computer won't handle while it's asleep.
     *
     * @note this method will only be called if you have attached a
     *       DetectNetworkConfigChangesSession to this ReflectServer.
     *
     * This method also sets a computer-is-about-to-sleep flag so that any new TCP connections added
     * in the time period between now and (when the computer actually goes to sleep) will be added
     * only in dormant-mode, so that no TCP connection will actually happen until after the computer is
     * re-awoken.
     */
   virtual void ComputerIsAboutToSleep() {SetComputerIsAboutToSleep(true);}

   /** Implemented to call Reconnect() on any sessions that were previously disconnected
     * by our ComputerIsAboutToSleep() call.
     *
     * @note this method will only be called if you have attached a 
     *       DetectNetworkConfigChangesSession to this ReflectServer.
     *
     * This method also clears the computer-is-about-to-sleep flag that was set by
     * the previous call to ComputerIsAboutToSleep()
     */
   virtual void ComputerJustWokeUp() {SetComputerIsAboutToSleep(false);}

protected:
   /**
    * This version of AddNewSession (which is called by the previous 
    * version) assumes that the gateway, hostname, port, etc of the
    * new session have already been set up.
    * @param ref The new session to add.
    * @return B_NO_ERROR if the new session was added successfully, or an error code if there was an error setting it up.
    */
   virtual status_t AttachNewSession(const AbstractReflectSessionRef & ref);

   /** Called by a session to send a message to its factory.  
     * @param session the session that is sending the Message
     * @param msgRef the Message that is being sent
     * @param userData an application-specific user-data value
     * @see AbstractReflectSession::SendMessageToFactory() for details.
     */
   status_t SendMessageToFactory(AbstractReflectSession * session, const MessageRef & msgRef, void * userData);

   /**
    * Called by a session to get itself replaced (the
    * new session will continue using the same message io streams
    * as the old one)
    * @param newSession the new session we want to have replace the old one
    * @param replaceThisOne the old session that we want to go away
    * @returns B_NO_ERROR on success, an error code if the new session
    *          returns an error in its AttachedToServer() method.  If 
    *          an error code is returned, then this call is guaranteed not to
    *          have had any effect on the old session.
    */
   status_t ReplaceSession(const AbstractReflectSessionRef & newSession, AbstractReflectSession * replaceThisOne);

   /** Called by a session to get itself removed & destroyed
     * Causes the ReflectServer to place the session in the "lame duck sessions list", which will result in it being
     * safely detached and removed from the ReflectServer at the next iteration of the event loop.
     * @param which the session that wants to go away ASAP
     */
   void EndSession(AbstractReflectSession * which);

   /** Called by a session to force its TCP connection to be closed
     * @param which The session to force-disconnect.
     * @returns true iff the session has decided to terminate itself, or false if it decided to continue
     *               hanging around the server even though its client connection has been severed.
     * @see AbstractReflectSession::ClientConnectionClosed().
     */
   bool DisconnectSession(AbstractReflectSession * which);

private:
   friend class AbstractReflectSession;
   void AddLameDuckSession(const AbstractReflectSessionRef & whoRef);
   void AddLameDuckSession(AbstractReflectSession * who);  // convenience method ... less efficient
   void ShutdownIOFor(AbstractReflectSession * session);
   status_t ClearLameDucks();  // returns B_NO_ERROR if the server should keep going, or an error code otherwise
   uint32 DumpBoggedSessions();
   status_t RemoveAcceptFactoryAux(const IPAddressAndPort & iap);
   status_t FinalizeAsyncConnect(const AbstractReflectSessionRef & ref);
   status_t DoAccept(const IPAddressAndPort & iap, const ConstSocketRef & acceptSocket, ReflectSessionFactory * optFactory);
   void LogAcceptFailed(int lvl, const char * desc, const char * ipbuf, const IPAddressAndPort & iap);
   uint32 CheckPolicy(Hashtable<AbstractSessionIOPolicyRef, Void> & policies, const AbstractSessionIOPolicyRef & policyRef, const PolicyHolder & ph, uint64 now) const;
   void CheckForOutOfMemory(const AbstractReflectSessionRef & optSessionRef);
   void SetComputerIsAboutToSleep(bool isAboutToSleep);
   bool IsSessionScheduledForPostSleepReconnect(const String & sessionID) const {return _sessionsToReconnectOnWakeup.ContainsKey(sessionID);}

   Hashtable<IPAddressAndPort, ReflectSessionFactoryRef> _factories;
   Hashtable<IPAddressAndPort, ConstSocketRef> _factorySockets;

   Queue<ReflectSessionFactoryRef> _lameDuckFactories;  // for delayed-deletion of factories when they go away

   Message _centralState;
   Hashtable<const String *, AbstractReflectSessionRef> _sessions;
   Queue<AbstractReflectSessionRef> _lameDuckSessions;  // sessions that are due to be removed
   bool _keepServerGoing;
   uint64 _serverStartedAt;
   bool _doLogging;
   uint64 _serverSessionID;

   Hashtable<IPAddress, String> _remapIPs;  // for v2.20; custom strings for "special" IP addresses
   SocketMultiplexer _multiplexer;

#ifdef MUSCLE_ENABLE_SSL
   ConstByteBufferRef _publicKey;  // used for making outgoing TCP connections
   ConstByteBufferRef _privateKey; // used for receiving incoming TCP connections
   String _pskUserName;            // used for pre-shared-key connections
   String _pskPassword;            // used for pre-shared-key connections
#endif
   NestCount _inDoAccept;
   NestCount _inDoConnect;

   AtomicCounter _inWaitForEvents;
   bool _computerIsAboutToSleep;
   Hashtable<String, bool> _sessionsToReconnectOnWakeup;

   DECLARE_COUNTED_OBJECT(ReflectServer);
};
DECLARE_REFTYPES(ReflectServer);

} // end namespace muscle

#endif
