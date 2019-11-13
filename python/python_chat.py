"""
A very simple little BeShare-compatible chat client, written in Python.
"""

__author__    = "Jeremy Friesner (jaf@meyersound.com)"
__version__   = "$Revision: 1.5 $"
__date__      = "$Date: 2005/01/01 01:03:21 $"
__copyright__ = "Copyright (c) 2000-2013 Meyer Sound Laboratories Inc"
__license__   = "See enclosed LICENSE.TXT file"

import select
import sys

import message
import message_transceiver_thread
import storage_reflect_constants

VERSION_STRING = "1.0"

# some constants defined by BeShare
NET_CLIENT_NEW_CHAT_TEXT = 2
NET_CLIENT_PING          = 5
NET_CLIENT_PONG          = 6

# This method retrieves sessionID, e.g.
# Given "/199.42.1.106/1308/beshare/name", it returns "1308"
def GetSessionID(x):
   if x[0] == "/":
      x = x[1:]
      x = x[x.find("/")+1:] # remove leading slash, hostname and second slash
      x = x[:x.find("/")]       # remove everything after the session ID
      return x
   else:
      return None

# our program all runs here
if __name__ == "__main__":
   if len(sys.argv) < 2:
      print "Usage:   python python_chat.py <servername> [nick]"
      print "Example: python python_chat.py beshare.tycomsystems.com PythonBoy"
      sys.exit(5)

   # our settings
   servername = sys.argv[1]
   port = 2960
   nick = "PythonBoy"
   if len(sys.argv) >= 3: 
      nick = sys.argv[2]
 
   mtt = None
   try:
      print "Now attempting to connect to server", servername, "..."
      mtt = message_transceiver_thread.MessageTransceiverThread(servername, port)
      mtt.start()  # important!  Gotta remember to start the thread going
      notifySocket = mtt.GetNotificationSocket()
      users = {}
      while 1:
         inReady, outReady, exReady = select.select([sys.stdin, notifySocket], [], [])
         if notifySocket in inReady:
            notifySocket.recv(1024) # just to clear the notify bytes out
            while 1:
               nextEvent = mtt.GetNextIncomingEvent()
               if nextEvent == message_transceiver_thread.MTT_EVENT_CONNECTED:
                  print "Connection to server established!  Logging in as", nick

                  # Upload our user name so people know who we are
                  nameMsg = message.Message()
                  nameMsg.PutString("name", nick)
                  nameMsg.PutInt32("port", 0)  # BeShare requires this, although we don't use it
                  nameMsg.PutString("version_name", "PythonChat")
                  nameMsg.PutString("version_num", VERSION_STRING)
                  uploadMsg = message.Message(storage_reflect_constants.PR_COMMAND_SETDATA)
                  uploadMsg.PutMessage("beshare/name", nameMsg)
                  mtt.SendOutgoingMessage(uploadMsg)
                 
                  # and subscribe to get updates regarding who else is on the server
                  subscribeMsg = message.Message(storage_reflect_constants.PR_COMMAND_SETPARAMETERS)
                  subscribeMsg.PutBool("SUBSCRIBE:beshare/name", True)
                  mtt.SendOutgoingMessage(subscribeMsg)
               elif nextEvent == message_transceiver_thread.MTT_EVENT_DISCONNECTED:
                  print "Connection to server broken, goodbye!"
                  sys.exit(10)
               elif nextEvent is None:     
                  break  # no more events to process, go back to sleep now
               else:
                  if nextEvent.what == NET_CLIENT_NEW_CHAT_TEXT:
                     sessionID  = nextEvent.GetString("session")
                     chatText = nextEvent.GetString("text")
                     if nextEvent.HasFieldName("private"):
                        print "<PRIVATE> ",
                     if users.has_key(sessionID):
                        user = users[sessionID]
                     else:
                        user = "<Unknown>"
                     if chatText[0:3] == "/me":
                        print "<ACTION> " + user + chatText[3:]
                     else:
                        print "("+sessionID+") "+user+": "+chatText
                  elif nextEvent.what == NET_CLIENT_PING:
                     fromSession = nextEvent.GetString("session") 
                     if fromSession is not None:
                        nextEvent.what = NET_CLIENT_PONG
                        nextEvent.PutString(storage_reflect_constants.PR_NAME_KEYS, "/*/"+fromSession+"/beshare")
                        nextEvent.PutString("session", "blah")   # server will set this
                        nextEvent.PutString("version", "PythonChat v" + VERSION_STRING)
                        mtt.SendOutgoingMessage(nextEvent)
                  elif nextEvent.what == storage_reflect_constants.PR_RESULT_DATAITEMS:
                     # Username/session list updates!  Gotta scan it and see what it says

                     # First check for any node-removal notices
                     removedList = nextEvent.GetStrings(storage_reflect_constants.PR_NAME_REMOVED_DATAITEMS)
                     for nextStr in removedList:
                        sessionID = GetSessionID(nextStr)
                        if users.has_key(sessionID):
                           print "User ("+sessionID+") "+users[sessionID]+" has disconnected."
                           del users[sessionID]

                     # Now check for any node-update notices
                     keys = nextEvent.GetFieldNames()
                     for fieldName in keys:
                        sessionID = GetSessionID(fieldName)
                        if sessionID is not None:
                           datamsg = nextEvent.GetMessage(fieldName)
                           if datamsg is not None:
                              username = datamsg.GetString("name")
                              if username is not None:
                                 if users.has_key(sessionID):
                                    print "User ("+sessionID+") (a.k.a. "+users[sessionID]+") is now known as " + username
                                 else:
                                    print "User ("+sessionID+") (a.k.a. "+username+") has connected to the server."
                                 users[sessionID] = username
                        
         if sys.stdin in inReady:
            usertyped = sys.stdin.readline().strip()
            if (not usertyped) or (usertyped == "/quit"):
               print "Exiting!"
               mtt.Destroy()  # important, or we'll hang
               sys.exit(0)
            if usertyped:
               chatMsg = message.Message(NET_CLIENT_NEW_CHAT_TEXT)
               chatMsg.PutString(storage_reflect_constants.PR_NAME_KEYS, "/*/*/beshare")
               chatMsg.PutString("session", "blah")  # server will set this for us
               chatMsg.PutString("text", usertyped)
               mtt.SendOutgoingMessage(chatMsg)
   except:
      if mtt is not None:
         mtt.Destroy()
      raise
