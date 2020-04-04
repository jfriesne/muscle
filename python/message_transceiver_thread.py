"""
This class represents an asynchronous TCP connection
over which MUSCLE Messages may be sent and received
without blocking the main thread of execution.
"""

__author__    = "Jeremy Friesner (jaf@meyersound.com)"
__version__   = "$Revision: 1.12 $"
__date__      = "$Date: 2005/07/06 23:41:33 $"
__copyright__ = "Copyright (c) 2000-2013 Meyer Sound Laboratories Inc"
__license__   = "See enclosed LICENSE.TXT file"

import message
import threading
import Queue
import select
import socket
import cStringIO
import struct
import sys

# constants used in our notifications
MTT_EVENT_CONNECTED     = 0
MTT_EVENT_DISCONNECTED  = 1

# Muscle gateway protocol's magic cookie, for sanity checking
MUSCLE_MESSAGE_ENCODING_DEFAULT = 1164862256  # 'Enc0'

# stolen from asyncore.py, and I'm not sorry
import os
if os.name == 'nt':
    EWOULDBLOCK = 10035
    EINPROGRESS = 10036
    EALREADY    = 10037
    ECONNRESET  = 10054
    ENOTCONN    = 10057
    ESHUTDOWN   = 10058
else:
    from errno import EALREADY, EINPROGRESS, EWOULDBLOCK, ECONNRESET, ENOTCONN, ESHUTDOWN 

def CreateConnectedSocketPair(blockingMode, preferIPv6=True):
   """Returns a pair (2-tuple) of stream-oriented sockets that are connected to each other
      This can be useful for inter-thread signalling, etc.
      (blockingMode) Set to True if you want blocking I/O semantics, or False for non-blocking
      (preferIPv6) Set to True if you prefer IPv6 transport, or False for IPv4 (this is used
                   only when socket.sockepair() isn't available; it probably doesn't matter which you use)
   """
   if (hasattr(socket, 'socketpair')):
      socketA, socketB = socket.socketpair()
      socketB.setblocking(blockingMode)
      socketA.setblocking(blockingMode)
      return (socketA, socketB)
   else:
      if (preferIPv6):
         family = socket.AF_INET6
         tempAcceptSock = socket.socket(family, socket.SOCK_STREAM)
         tempAcceptSock.bind(("::1", 0))
      else:
         family = socket.AF_INET
         tempAcceptSock = socket.socket(family, socket.SOCK_STREAM)
         tempAcceptSock.bind(("127.0.0.1", 0))
      tempAcceptSock.listen(1)
      socketA = socket.socket(family, socket.SOCK_STREAM)
      socketA.connect(tempAcceptSock.getsockname())
      socketB = tempAcceptSock.accept()[0]
      tempAcceptSock.close()
      socketB.setblocking(blockingMode)
      socketA.setblocking(blockingMode)
      return (socketA, socketB)

class MessageTransceiverThread(threading.Thread):
   """Python implementation of the MUSCLE MessageTransceiverThread class.

   This class represents an asynchronous TCP connection over which MUSCLE Messages may be sent and received
   asynchronously, without ever blocking the main thread of execution.  When the TCP connection is established, 
   you will be notified with an MTT_EVENT_CONNECTED event.  When the TCP connection fails or is broken, you will
   be notified with an MTT_EVENT_DISCONNECTED event.  Whenever a Message is received over the TCP connection,
   you will get a Message object as your notification.  (See GetNextIncomingEvent() for details on notification).
   """

   def __init__(self, hostname, port, acceptFrom=None, preferIPv6=False):
      """This constructor creates a MessageTransceiverThread that does an outgoing TCP connection.

      (hostname) is the hostname (or IP address string) of the host to connect to (e.g. "www.meyersound.com"
                 or "132.239.50.8" or etc).  If (hostname) is set to None, then this MessageTransceiverThread will 
                 be configured to accept incoming TCP connections, instead of initiating an outgoing one.
      (port) is the port number to connect to or accept on (e.g. 2960 is the default MUSCLE server port).
             If you are accepting, and (port) is set to zero, the system will choose a port number for you.
             (use GetPort() to find out which one it chose).
      (acceptfrom) is only relevant if you are accepting incoming connections, i.e. if (hostname) was None.
                   If specified, it is the only IP address that is allowed to connect to you.  If left as
                   None, however, any IP address is allowed to connect to you.
      (preferIPv6) if set to True, we'll use IPv6 networking instead of IPv4

      Note that you will need to also call start() before the internal TCP connection thread will start executing."""

      threading.Thread.__init__(self)
      self.__outQ         = Queue.Queue(0)
      self.__inQ          = Queue.Queue(0)
      self.__hostname     = hostname
      self.__port         = port
      self.__acceptsocket = None
      self.__mainsocket   = None
      self.__threadsocket = None
      self.__preferIPv6   = preferIPv6

      # Used internally by the MessageTransceiverThread class, to clean up
      self._endSession = IOError()

      # Set up for accepting incoming TCP connections, if necessary
      if self.__hostname is None:
         if acceptFrom is None:
            acceptFrom = ""
         self.__acceptsocket = socket.socket(self.__getSocketFamily(), socket.SOCK_STREAM)
         self.__acceptsocket.bind((acceptFrom, port))
         self.__acceptsocket.listen(1)
         self.__acceptsocket.setblocking(False)
         self.__port = self.__acceptsocket.getsockname()[1]

      # Set up our internal socket-based thread notification/wakeup system
      (self.__mainsocket, self.__threadsocket) = CreateConnectedSocketPair(False, self.__preferIPv6)

   def SendOutgoingMessage(self, msg):
      """Call this to queue up the given Message object to be sent out across the network.

      This method may be called at any time, but queued Messages won't go out until after
      the internal thread is start()'d and the TCP connection is established.  All Messages will
      be sent out in the same order that they were enqueued (FIFO).  This method is thread safe,
      and is typically called by your main thread of execution."""

      self.__outQ.put(msg, 0)
      self.__mainsocket.send("j")  # wake up the internal thread to check the Q

   def Destroy(self):
      """Shuts down the internal thread, closes any TCP connection and releases all held resources.

         Call this method when you are done using this object forever and want it to go away.  
         This method is typically called by your main thread of execution."""
      if (self.__outQ is not None):
         self.SendOutgoingMessage(self._endSession)  # special 'please die' code
         self.join()                                 # wait for the internal thread to go away
         self.__outQ = None
         self.__inQ  = None
         if self.__mainsocket is not None:
            self.__mainsocket.close()
         if self.__threadsocket is not None:
            self.__threadsocket.close()
         if self.__acceptsocket is not None:
            self.__acceptsocket.close()

   def GetPort(self):
      """Returns the port this MessageTransceiverThread is connecting or accepting connections on."""
      return self.__port

   def GetNextIncomingEvent(self, block=False):
      """Returns the next incoming event object, or None if the incoming-event-queue is empty.  

         Incoming event objects will be one of the following three things:
            MTT_EVENT_CONNECTED    - TCP connection has been established
            MTT_EVENT_DISCONNECTED - TCP connection has been broken or failed
            (a Message object)     - A Message object that was received over the network.

         If (block) is set to true, this method will block until the next event
         is ready.  Otherwise (the default) this method will always return immediately.
         THIS METHOD IS THREAD SAFE, YOU MAY CALL IT AT ANY TIME, FROM ANY THREAD."""
      try:
         return self.__inQ.get(block)
      except Queue.Empty:
         return None

   def GetNotificationSocket(self):
      """Returns a socket that your main thread may select() or recv() on to wait for incoming network events.  

         Whenever a new event is ready, a byte will be sent to this socket.  Recv() it, then call 
         GetNextIncomingEvent() repeatedly to get all the new events, until it returns None.  
         Note:  please do not close() or write to this socket; it is still owned by (and used by) 
                this MessageTransceiverThread object."""
      return self.__mainsocket

   def NotifyMainThread(self):
      """This method is called by the MessageTransceiverThread's internal thread whenever 
         it wants to notify the main thread that there is a new event ready for the main
         thread to process.  This method's default behaviour is to send a byte to the 
         notification socket (see GetNotificationSocket(), above), which will wake up 
         the main thread if the main thread is blocking on that socket (via recv() or
         select() or whatnot).  However, if you wish to wake up the main thread using a
         different mechanism, feel free to override this method to do something more appropriate."""
      self.__threadsocket.send("t")

   def run(self):
      """Entry point for the internal thread.  Don't call this method; call start() instead."""
      toremote = None
      try:
         self._connectStillInProgress = False
         if self.__acceptsocket is None:
            try:
               toremote = self.__createConnectingSocket(False)  # FogBugz 10491:  if we can't connect with our preferred protocol (e.g. IPv6)...
            except socket.error:
               toremote = self.__createConnectingSocket(True)   # ... then we'll try again with the other protocol (e.g. IPv4)

         # setup the lists we will select() on in advance, to avoid having to do it every time
         inlist       = [toremote, self.__threadsocket]
         notifyonly   = [self.__threadsocket]
         notifyaccept = [self.__threadsocket, self.__acceptsocket]
         outlist      = [toremote]
         emptylist    = []

         inHeader, inBody, inBodySize = "", "", 0
         outHeader, outBody = self.__getNextMessageFromMain()
         while True:
            try:
               waitingForAccept = self.__acceptsocket is not None and toremote is None

               if (outHeader is not None or outBody is not None or self._connectStillInProgress) and waitingForAccept is False:
                  useoutlist = outlist
               else:
                  useoutlist = emptylist

               if self._connectStillInProgress:
                  useinlist = notifyonly
               elif waitingForAccept:
                  useinlist = notifyaccept
               else:
                  useinlist = inlist

               inready, outready, exlist = select.select(useinlist, useoutlist, emptylist)

               if waitingForAccept and self.__acceptsocket in inready:
                  toremote = self.__acceptsocket.accept()[0]
                  inlist  = [toremote, self.__threadsocket]
                  outlist = [toremote]
                  self.__sendReplyToMainThread(MTT_EVENT_CONNECTED)

               if toremote in inready:
                  if inHeader is not None:
                     newbytes = toremote.recv(8-len(inHeader))
                     if not newbytes:
                        raise socket.error
                     inHeader = inHeader + newbytes
                     if len(inHeader) == 8:   # full header size is 2*sizeof(int32)
                        # Okay, we have the entire inHeader!  Now decode it
                        inBodySize, magic = struct.unpack("<2L", inHeader)
                        if magic != MUSCLE_MESSAGE_ENCODING_DEFAULT:
                           raise socket.error
                        inHeader = None    # signal that we are now ready to read the inBody
                  else:
                     newbytes = toremote.recv(inBodySize-len(inBody))
                     if not newbytes:
                        raise socket.error
                     inBody = inBody + newbytes
                     if len(inBody) == inBodySize:
                        inmsg = message.Message()
                        inmsg.Unflatten(cStringIO.StringIO(inBody))
                        self.__sendReplyToMainThread(inmsg)
                        inHeader, inBody, inBodySize = "", "", 0 # reset to receive next inHeader

               # Note that we don't need to do anything with the data we read from
               # threadsocket; it's enough that it woke us up so we can check the out-queue.
               if self.__threadsocket in inready and not self.__threadsocket.recv(1024):
                  raise self._endSession

               if toremote in outready:
                  if self._connectStillInProgress:
                     toremote.send("") # finalize the connect          # everyone else can get by with just this
                     self.__sendReplyToMainThread(MTT_EVENT_CONNECTED)
                     self._connectStillInProgress = False 
                  else:
                     if outHeader is not None:
                        numSent = toremote.send(outHeader)
                        if numSent > 0:
                           outHeader = outHeader[numSent:]
                           if not outHeader:
                              outHeader = None
                     elif outBody is not None:
                        numSent = toremote.send(outBody)
                        if numSent > 0:
                           outBody = outBody[numSent:]
                           if not outBody:
                              outBody = None

               if outHeader is None and outBody is None:
                  outHeader, outBody = self.__getNextMessageFromMain()
            except socket.error:
               if self.__acceptsocket is not None:
                  self.__sendReplyToMainThread(MTT_EVENT_DISCONNECTED)
                  toremote = None # for accepting sessions, we can disconnect and then go back to waiting for an accept
               else:
                  raise # for connecting sessions, the first disconnection means game over, so rethrow
      except socket.error:
         self.__sendReplyToMainThread(MTT_EVENT_DISCONNECTED)
      except IOError, inst:
         if (inst == self._endSession):
            pass  # this exception just means to end the thread without comment
         else:
            raise

      if toremote is not None:
         toremote.close()

   def __sendReplyToMainThread(self, what):
      self.__inQ.put(what, 0)
      self.NotifyMainThread()

   def __getNextMessageFromMain(self):
      try:
         nextItem = self.__outQ.get(0)
         if nextItem == self._endSession:
            raise self._endSession
         sio = cStringIO.StringIO()
         nextItem.Flatten(sio)
         return (struct.pack("<2L", nextItem.FlattenedSize(), MUSCLE_MESSAGE_ENCODING_DEFAULT), sio.getvalue())
      except Queue.Empty:
         return None, None  # oops, nothing more to send for the moment

   def __getSocketFamily(self):
      if (self.__preferIPv6):
         return socket.AF_INET6
      else:
         return socket.AF_INET

   def __createConnectingSocket(self, useAlternateSocketFamily):
      family = self.__getSocketFamily()
      if (useAlternateSocketFamily):
         if (family == socket.AF_INET6):
            family = socket.AF_INET
         else:
            family = socket.AF_INET6

      remoteSocket = socket.socket(family, socket.SOCK_STREAM)
      remoteSocket.setblocking(False)
      try:
         r = socket.getaddrinfo(self.__hostname, self.__port, family, socket.SOCK_STREAM, socket.IPPROTO_IP, socket.AI_CANONNAME)
         if ((r is not None) and (r)):   # paranoia
            remoteSocket.connect(r[0][4])
         else:
            raise socket.error
      except socket.error, why:
         if why[0] in (EINPROGRESS, EALREADY, EWOULDBLOCK):
            self._connectStillInProgress = True
         else:
            raise

      if self._connectStillInProgress is False:
         self.__sendReplyToMainThread(MTT_EVENT_CONNECTED)

      return remoteSocket

# -----------------------------------------------------------------------------------------------
#
# Test stub below: exercises basic sending and receiving of Message objects.
# Easiest way to test this is to open two shell windows, and in the first type:
#   python message_transceiver_thread.py accept 9999
# and then in the other shell window, type:
#   python message_transceiver_thread.py localhost 9999
# Then the two programs should connect to each other, and typing anything into
# one window should result in output being printed to the other.  Fun!
#
# -----------------------------------------------------------------------------------------------

if __name__ == "__main__":

   # A custom subclass to make things simple for us.  A "real" program wouldn't do it this way.
   class TestMTT(MessageTransceiverThread):
      def __init__(self, hostname, port):
         MessageTransceiverThread.__init__(self, hostname, port)

      # Overridden to merely dequeue and print out any new incoming Messages or events.
      # Note that this isn't usually the best way to handle incoming events in a 'real' 
      # program, since this method is executed in the internal network thread and not in
      # the main thread.  In a more complex program, doing stuff here could easily lead
      # to race conditions.  Therefore, a better-written program would notify the main 
      # thread, and the main thread would respond by picking up the queued events and    
      # handle them locally, instead.  But this will do, for testing.
      # (see pythonchat.py for an example of a program that does things the right way)
      def NotifyMainThread(self):
         while True:
            nextEvent = self.GetNextIncomingEvent()
            if nextEvent == MTT_EVENT_CONNECTED:
               print ""
               print "TCP Connection has been successfully created!"
            elif nextEvent == MTT_EVENT_DISCONNECTED:
               print ""
               print "TCP Connection failed or was disconnected!"
            elif nextEvent is None:
               break  # If we got here, the Queue is empty, nothing more to show
            else:
               print ""
               print "Received the following Message from the remote peer:"
               nextEvent.PrintToStream()

   if len(sys.argv) < 2:
      print "Usage:  python message_transceiver_thread.py accept [port=0]"
      print "   or:  python message_transceiver_thread.py <hostname> [port=2960]"
      sys.exit(5)

   if sys.argv[1] == "accept":
      port = 0   # Default -- 0 means we get to pick the port
      if len(sys.argv) >= 3:
         port = sys.argv[2]
      mtt = TestMTT(None, int(port))
      print "Accepting connections on port", mtt.GetPort()
   else:
      hostname = sys.argv[1]
      port     = 2960
      if len(sys.argv) >= 3:
         port = sys.argv[2]
      mtt = TestMTT(hostname, int(port))
      print "Attempting to connect to", hostname, "port", port, "..."

   mtt.start()  # important!  Otherwise nothing will happen :^)
   while True:
      nextline = sys.stdin.readline().strip()
      if (not nextline) or (nextline == "q"):
         mtt.Destroy()
         print "Bye bye!"
         sys.exit()
      else: 
         m = message.Message(666)
         m.PutString("special message!", nextline)
         mtt.SendOutgoingMessage(m)
