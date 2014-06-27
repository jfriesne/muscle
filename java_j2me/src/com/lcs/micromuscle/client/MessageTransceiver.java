/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.client;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import javax.microedition.io.StreamConnection;
import javax.microedition.io.ServerSocketConnection;
import javax.microedition.io.Connector;

import com.meyer.micromuscle.iogateway.AbstractMessageIOGateway;
import com.meyer.micromuscle.iogateway.MessageIOGatewayFactory;
import com.meyer.micromuscle.message.Message;
import com.meyer.micromuscle.support.LEDataInputStream;
import com.meyer.micromuscle.support.LEDataOutputStream;
import com.meyer.micromuscle.thread.MessageListener;
import com.meyer.micromuscle.thread.MessageQueue;

/** This class is a handy utility class that acts as
 *  an interface to a TCP socket to send and receive
 *  Messages over.
 */
public class MessageTransceiver implements MessageListener
{
   /** Creates a MessageTransceiver that will send replies to the given queue,
    *  using the provided outgoing compression level.
    *  @param repliesTo where to send status & reply messages
    *  @param compressionLevel the outgoing compression level to use
    */
   public MessageTransceiver(MessageQueue repliesTo, int compressionLevel) 
   {
      _repliesTo = repliesTo;
      _gateway = MessageIOGatewayFactory.getMessageIOGateway(compressionLevel);
      _sendQueue = new MessageQueue(new MessageTransmitter());
   }

   /** Creates a MessageTransceiver that will send replies to the given queue.
    *  @param repliesTo where to send status & reply messages
    */
   public MessageTransceiver(MessageQueue repliesTo)
   {
      _repliesTo = repliesTo;
      _gateway   = MessageIOGatewayFactory.getMessageIOGateway(); // default protocol, default compression
      _sendQueue = new MessageQueue(new MessageTransmitter());
   }

   /** Start up a transceiver that automatically uses the specified socket.
    *  @param repliesTo where to send status & reply messages
    *  @param s pre-bound socket to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    *  @param compressionLevel the outgoing compression level to use.
    */
   public MessageTransceiver(MessageQueue repliesTo, StreamConnection s, Object successTag, Object failTag, int compressionLevel) 
   {
      _repliesTo = repliesTo;
      _gateway   = MessageIOGatewayFactory.getMessageIOGateway(compressionLevel);
      _sendQueue = new MessageQueue(new MessageTransmitter(s, successTag, failTag));
   }

   /** Start up a transceiver that automatically uses the specified socket.
    *  @param repliesTo where to send status & reply messages
    *  @param s pre-bound socket to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    *  @author Bryan Varner
    */
   public MessageTransceiver(MessageQueue repliesTo, StreamConnection s, Object successTag, Object failTag)
   {
      _repliesTo = repliesTo;
      _gateway   = MessageIOGatewayFactory.getMessageIOGateway();
      _sendQueue = new MessageQueue(new MessageTransmitter(s, successTag, failTag));
   }

   /** Creates a MessageTransceiver that will send replies to the given queue, and
    *  use the given IO gateway for I/O purposes.
    *  @param repliesTo where to send status & reply messages
    *  @param ioGateway the AbstractMessageIOGateway to use to flatten and unflatten Messages.
    */
   public MessageTransceiver(MessageQueue repliesTo, AbstractMessageIOGateway ioGateway)
   {
      _repliesTo = repliesTo;
      _gateway   = ioGateway;
      _sendQueue = new MessageQueue(new MessageTransmitter());
   }

   /** Start up a transceiver that automatically connects to the specified socket.
    *  @param repliesTo where to send status & reply messages
    *  @param ioGateway the AbstractMessageIOGateway to use to flatten and unflatten Messages.
    *  @param s pre-bound socket to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    *  @author Bryan Varner
    */
   public MessageTransceiver(MessageQueue repliesTo, AbstractMessageIOGateway ioGateway, StreamConnection s, Object successTag, Object failTag)
   {
      _repliesTo = repliesTo;
      _gateway   = ioGateway;
      _sendQueue = new MessageQueue(new MessageTransmitter(s, successTag, failTag));
   }

   /** Tells the internal thread to connect to the given host and port
    *  Connection is done asynchronously, and an OperationCompleteMessage
    *  will be sent to the repliesTo queue when it has been done.
    *  @param hostName IP address or host name of computer to connect to
    *  @param port Port number to connect to (2960 is the port for a MUSCLE server)
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    */
   public void connect(String hostName, int port, Object successTag, Object failTag)
   {
      _sendQueue.postMessage(new ConnectMessage(hostName, port, successTag, failTag));
   }

   /** Tells the internal thread to listen on the given port.
    *  Connection is done asynchronoulsy.
    *  With this you will receive multiple copies of successTag. The first signals that we have bound to the port.
    *  If continually listening, this is all you will receive. However, if you are doing a one-shot listen(), the
    *  second successTag will signify that the connection has been established.
    *  @param s A constructed ServerSocket that has already properly bound to it's port.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that a new connection has been established.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection has been broken. (must connect first, we're listening)
    *  @param continualyListen If true, the listen thread will continue to accept sockets until the MessageTranciever dies. If false, only a single connection is accepted.
    *  @author Bryan Varner
    */
   public void listen(ServerSocketConnection s, Object successTag, Object failTag, boolean continuallyListen)
   {
      _continuallyListen = continuallyListen;
      _sendQueue.postMessage(new ConnectMessage(s, successTag, failTag));
   }

   /** Forces the transceiver to stop listening immediately if it is in a mode to continually listen.
    *  Of course, this is asynchronous.
    *  @author Bryan Varner
    */
   public void stopListening()
   {
      _continuallyListen = true;
      if (_currentConnectThread instanceof ListenThread) _currentConnectThread.interrupt();
   }

   /** Tells the internal thread to disconnect.  This will be done asynchronously.
    *  When the disconnection is done, the failTag specified previously in the connect()
    *  call will be sent to your reply queue.
    */
   public void disconnect()
   {
      _sendQueue.postMessage(new ConnectMessage(null, 0, null, null));
   }

   /** Tells the internal thread to send a Message out over the TCP socket.
    *  This will be done asynchronously.
    */
   public void sendOutgoingMessage(Message msg)
   {
      _sendQueue.postMessage(msg);
   }
   
   /**
    * Set a tag that will be sent whenever our queue of outgoing Messages
    * becomes empty.  (Default is NULL, meaning that no notifications will be sent)
    * @param tag The notification tag to send, or NULL to disable.
    */
   public void setNotifyOutputQueueDrainedTag(Object tag) 
   {
      _outputQueueDrainedTag = tag;
   }
   
   /** Any received Messages will be sent out to the TCP socket */
   public void messageReceived(Object msg, int numLeft)
   {
      if (msg instanceof Message) sendOutgoingMessage((Message)msg);
   }

   /** Internal control class */
   private final class ConnectMessage
   {
      public ConnectMessage(String hostName, int port, Object successTag, Object failTag)
      {
         _hostName   = hostName;
         _port       = port;
         _successTag = successTag;
         _failTag    = failTag;
         _sock       = null;
      }
     
     public ConnectMessage(ServerSocketConnection s, Object successTag, Object failTag)
     {
        _hostName   = null;
        _port       = 0;
        _successTag = successTag;
        _failTag    = failTag;
        _sock       = s;
      }
     
      public String _hostName;
      public int    _port;
      public Object _successTag;
      public Object _failTag;
	 public ServerSocketConnection _sock;
   }

   /** Logic for the connecting-and-sending thread */
   private final class MessageTransmitter implements MessageListener
   {
      public MessageTransmitter()
      {
         // empty
      }

      public MessageTransmitter(StreamConnection s, Object successTag, Object failTag)
      {
         _failTag = failTag;
         _currentConnectThread = null;   // no longer in the connect period
         if (s != null)
         {
            try {
               _socket  = s;
               _out     = new LEDataOutputStream(_socket.openDataOutputStream());
               new MessageReceiver(new LEDataInputStream(_socket.openDataInputStream()), _connectionID);  // start the read thread
               sendReply(successTag);  // ok, done!
            }
            catch(IOException ex) {
			disconnect(_connectionID);
            }
         }
         else sendReply(_failTag);
      }

      public void messageReceived(Object message, int numLeft)
      {
         if (message instanceof Message)
         {
            if (_out != null)
            {
               try {
                  _gateway.flattenMessage(_out, (Message) message);
               }
               catch(IOException ex) {
			   disconnect(_connectionID);
               }
            }
         }
         else if (message instanceof ConnectMessage)
         {
            disconnect(_connectionID);  // make sure our previous connection is gone

            ConnectMessage cmsg = (ConnectMessage) message;
            if (cmsg._hostName != null)
            {
               // Do the actual connecting in Yet Another Thread, so that we can remain
               // responsive to the user's requests during the (possibly lengthy) connect period.
               _currentConnectThread = new ConnectThread(cmsg);  // connect thread is started in ctor
            }
            else if (cmsg._sock != null)
            {
               // If a socket was passed in, we know we need to listen!
               _currentConnectThread = new ListenThread(cmsg);
            }
         }
         else if (message instanceof ConnectThread)
         {
            // When our ConnectThread has finished, he sends himself back to us with the connect results.
            ConnectThread ct = (ConnectThread) message;
            StreamConnection s = ct._socket;
            ConnectMessage cmsg = ct._cmsg;
            if (ct == _currentConnectThread)
            {
               _currentConnectThread = null;   // no longer in the connect period
               if (s != null)
               {
                  try {
                     _socket  = s;
                     _failTag = cmsg._failTag;
                     _out     = new LEDataOutputStream(_socket.openDataOutputStream());
                     new MessageReceiver(new LEDataInputStream(_socket.openDataInputStream()), _connectionID);  // start the read thread
                     sendReply(cmsg._successTag);  // ok, done!
                  }
                  catch(IOException ex) {
				   disconnect(_connectionID);
                  }
               }
               else sendReply(cmsg._failTag);
            }
            else
            {
               // oops, no use for the socket, so just close it and forget about it
               if (s != null) try {s.close();} catch(Exception ex) {/* don't care */}
               sendReply(cmsg._failTag);
            }
         }
         else if (message instanceof MessageReceiver) disconnect(((MessageReceiver)message).connectionID());
         
         if (numLeft == 0) 
         {
            if (_out != null) 
            {
               try {
                  _out.flush();
               }
               catch(IOException ex) {
                  ex.printStackTrace();
               }
            }
            if (_outputQueueDrainedTag != null) sendReply(_outputQueueDrainedTag);
         }
      }
   
      /** Close the connection and notify the user (only if the connection ID is correct) */
      private void disconnect(int id)
      {
         if ((_socket != null)&&(_connectionID == id))
         {
            try {
			_out.close();
               _socket.close();
            }
            catch(IOException ex) {
               // ignore
            }
            sendReply(_failTag);
            _out = null;
            _socket = null;
            _failTag = null;
            _currentConnectThread = null;
            _connectionID++;
         }
      }
   
      /** Send a message back to the user */
      private void sendReply(Object replyObj)
      {
         if ((_repliesTo != null)&&(replyObj != null)) _repliesTo.postMessage(replyObj);
      }
   
      /** Logic for our receive-incoming-data thread */
      private class MessageReceiver implements MessageListener
      {
         public MessageReceiver(DataInput in, int cid)
         {
            _in = in;
            _connectionID = cid;
            (new MessageQueue(this)).postMessage(null);  // start the background reader thread
         }
         
         public void messageReceived(Object msg, int numLeft)
         {
            try {
               while(true) sendReply(_gateway.unflattenMessage(_in));
            }
            catch(IOException ex) {
               // oops, socket broke... notify out master that we're leaving (by posting ourself as a message, teehee)
               _sendQueue.postMessage(this);
            }
         }
         
         public int connectionID() {return _connectionID;}
         
         private int _connectionID;
         private DataInput _in;
      }
      
      private Object _failTag          = null;
      private StreamConnection _socket = null;
      private LEDataOutputStream _out  = null;
   }
   
   // A little class for doing TCP connects in a separate thread (so the MessageTransceiver won't block during the connect) */
   private class ConnectThread implements MessageListener
   {
      private MessageQueue myQueue = null;
      
      public ConnectThread(ConnectMessage cmsg)
      {
         _cmsg = cmsg;
         myQueue = new MessageQueue(this);
         myQueue.postMessage(null);  // just to kick off the thread
      }
      
      public void messageReceived(Object obj, int numLeft)
      {
         try {
		    _socket = (StreamConnection)Connector.open("socket://" + _cmsg._hostName + ":" + _cmsg._port);
         }
         catch(IOException ex) {
            // do nothing
         }
         _sendQueue.postMessage(this);
      }
      
      public Runnable getThread(){
         return myQueue;
      }
      
      public void interrupt(){
         myQueue.interrupt();
      }
      
      public StreamConnection _socket = null;
      public ConnectMessage _cmsg;
   }
   
   // A little class for listening for TCP connections in a separate thread. -- Author: Bryan Varner
   // We override the listener method of ConnectThread and block for any incoming connections to said port.
   private class ListenThread extends ConnectThread
   {
      public ListenThread(ConnectMessage cmsg)
      {
         super(cmsg);
      }
      
      public void messageReceived(Object obj, int numLeft)
      {
         ServerSocketConnection servSock = _cmsg._sock;
       
         try {
            while(true) {
               // At this point we know we want at least one connection, we'll test for continuance at the end.
               _socket = servSock.acceptAndOpen();
               
               if (_continuallyListen == false)
               {
                  // If we aren't supposed to keep listening, post this object, killing this thread.
                  _sendQueue.postMessage(this);
               }
               else
               {
                  // If we are supposed to keep goin, send the new socket back to the object that started the transceiver, and let them deal with it.
                  if (_repliesTo != null) _repliesTo.postMessage(_socket);
               }
            }
         } catch (IOException ex){
            // You're getting the preverbial screw. Hope you're enjoying it.
         }
      }
   }
      
   private AbstractMessageIOGateway _gateway;
   private MessageQueue _repliesTo;
   private MessageQueue _sendQueue;
   private int _connectionID = 0;
   private ConnectThread _currentConnectThread = null;  // non-null if we are connecting or listening (in a slave thread)
   private boolean _continuallyListen = false;
   private Object _outputQueueDrainedTag = null;
}
