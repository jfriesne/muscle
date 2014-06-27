/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleAcceptSocketsThread_h
#define MuscleAcceptSocketsThread_h

#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

/** Event message codes returned by this thread */
enum {
   AST_EVENT_NEW_SOCKET_ACCEPTED = 1634956336, // 'ast0' - sent when we accept and send back a new socket
   AST_LAST_EVENT
};

#define AST_NAME_SOCKET "socket"  // field name where we store our ConstSocketRef in our reply Messages.

/** A thread that waits for TCP connections on a given port, and when it gets one, 
  * it sends the socket to its owner via a ConstSocketRef.
  */
class AcceptSocketsThread : public Thread, private CountedObject<AcceptSocketsThread>
{
public:
   /** Default constructor.  You'll need to call SetPort() before calling StartInternalThread(). */
   AcceptSocketsThread();

   /** Constructor.
     * @param port Port to listen on, or 0 if we should select our own port.
     *             If the latter, you can call GetPort() to find out which port was selected. 
     * @param optFrom If specified, the IP address to accept connections from.  If left as zero,
     *                then connections will be accepted from any IP address.
     */
   AcceptSocketsThread(uint16 port, const ip_address & optFrom = invalidIP);

   /** Destructor.  Closes the accept socket and frees the port */
   virtual ~AcceptSocketsThread();

   /** Overridden to grab the notify socket */
   virtual status_t StartInternalThread();

   /** Returns the port we are (or will be) listening on, or zero if we aren't listening at all. */
   uint16 GetPort() const {return _port;}

   /** Tries to allocate a socket to listen on the given port.  Will close any previously existing
     * socket first.  Does not work if the internal thread is already running. 
     * @param port Which port to allocate a socket to listen on, or zero if you wish for the system to choose.
     * @param optInterfaceIP if specified, this should be the IP address of a local network interface
     *                       to listen for incoming connections on.  If left unspecified (or set to invalidIP)
     *                       then we will accept connections on all network interfaces.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (port couldn't be allocated, or internal
     *          thread was already running)
     */
   status_t SetPort(uint16 port, const ip_address & optInterfaceIP = invalidIP);

protected:
   virtual void InternalThreadEntry();

private:
   uint16 _port;
   ConstSocketRef _notifySocket;
   ConstSocketRef _acceptSocket;
};

};

#endif
