/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef SocketCallbackMechanism_h
#define SocketCallbackMechanism_h

#include "util/Socket.h"
#include "util/ICallbackMechanism.h"

namespace muscle {

/** This class implements the ICallbackMechanism interface
  * using a socketpair-based signalling mechanism.  This
  * implementation should work under just about any OS.
  */
class SocketCallbackMechanism : public ICallbackMechanism
{
public:
   /** Default Constructor */
   SocketCallbackMechanism();

   /** Destructor */
   virtual ~SocketCallbackMechanism();

   /** Overridden to then read and discard any bytes received on the  socket, and then it will call
     * up to ICallbackMechanism::DispatchCallbacks(), which will call DispatchCallbacks() on the
     * appropriate registered ICallbackSubscribers.
     */
   virtual void DispatchCallbacks();

   /** Returns a reference to the main/dispatch-thread-notifier socket.
     * The only thing that you should ever do with this socket is have your event-loop
     * call select() (or some equivalent) on it.  When this socket selects-as-ready-for-read,
     * you should call DispatchCallbacks() on this object.
     */
   ConstSocketRef GetDispatchThreadNotifierSocket() {return _dispatchThreadSock;}

protected:
   /** Overridden to send a byte on the thread-side of the socketpair */
   virtual void SignalDispatchThread();

private:
   ConstSocketRef _dispatchThreadSock;  // this socket is read by the main/dispatch thread
   ConstSocketRef _otherThreadsSock;    // this socket is written to by other threads
};

};  // end muscle namespace

#endif
