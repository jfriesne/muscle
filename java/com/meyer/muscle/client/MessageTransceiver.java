/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.client;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

import com.meyer.muscle.iogateway.AbstractMessageIOGateway;
import com.meyer.muscle.iogateway.MessageIOGatewayFactory;
import com.meyer.muscle.message.Message;
import com.meyer.muscle.support.NotEnoughDataException;
import com.meyer.muscle.support.UnflattenFormatException;
import com.meyer.muscle.thread.MessageListener;
import com.meyer.muscle.thread.MessageQueue;

/** This class is a handy utility class that acts as
 *  an interface to a TCP socket to send and receive
 *  Messages over.
 *  @author Bryan Varner
 */
public class MessageTransceiver implements MessageListener
{
   /** Creates a MessageTransceiver that will send replies to the given queue,
    *  using the provided outgoing compression level.
    *  @param repliesTo where to send status & reply messages
    *  @param compressionLevel the outgoing compression level to use
    *  @param maximumIncomingMessageSize Maximum size of incoming messages, in bytes.  Set to Integer.MAX_VALUE to enforce no limit.
    */
   public MessageTransceiver(MessageQueue repliesTo, int compressionLevel, int maximumIncomingMessageSize) 
   {
      _repliesTo = repliesTo;
      _gateway = MessageIOGatewayFactory.getMessageIOGateway(compressionLevel, maximumIncomingMessageSize);
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
    *  @param s pre-bound channel to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    *  @param compressionLevel the outgoing compression level to use.
    *  @param maximumIncomingMessageSize Maximum size of incoming messages, in bytes.  Set to Integer.MAX_VALUE to enforce no limit.
    */
   public MessageTransceiver(MessageQueue repliesTo, ByteChannel s, Object successTag, Object failTag, int compressionLevel, int maximumIncomingMessageSize)
   {
      _repliesTo = repliesTo;
      _gateway   = MessageIOGatewayFactory.getMessageIOGateway(compressionLevel, maximumIncomingMessageSize);
      _sendQueue = new MessageQueue(new MessageTransmitter(s, successTag, failTag));
   }

   /** Start up a transceiver that automatically uses the specified socket.
    *  @param repliesTo where to send status & reply messages
    *  @param s pre-bound channel to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    */
   public MessageTransceiver(MessageQueue repliesTo, ByteChannel s, Object successTag, Object failTag)
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
    *  @param s pre-bound channel to run the MessageTransceiver on.
    *  @param successTag Any value you want--if non-null, this will be sent back to indicate that the connection succeeded.
    *  @param failTag Any value you want--if non-null, this will be sent back to indicate that the connection failed or was broken.
    */
   public MessageTransceiver(MessageQueue repliesTo, AbstractMessageIOGateway ioGateway, ByteChannel s, Object successTag, Object failTag)
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
    *  @param continuallyListen If true, the listen thread will continue to accept sockets until the MessageTranciever dies. If false, only a single connection is accepted.
    */
   public void listen(ServerSocketChannel s, Object successTag, Object failTag, boolean continuallyListen)
   {
      _continuallyListen = continuallyListen;
      _sendQueue.postMessage(new ConnectMessage(s, successTag, failTag));
   }

   /** Forces the transceiver to stop listening immediately if it is in a mode to continually listen.
    *  Of course, this is asynchronous.
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
     
     public ConnectMessage(ServerSocketChannel s, Object successTag, Object failTag)
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
      public ServerSocketChannel _sock;
   }

   /** Logic for the connecting-and-sending thread */
   private final class MessageTransmitter implements MessageListener
   {
      public MessageTransmitter()
      {
         // empty
      }

      public MessageTransmitter(ByteChannel s, Object successTag, Object failTag)
      {
         _failTag = failTag;
         _currentConnectThread = null;   // no longer in the connect period
         if (s != null)
         {
            _channel  = s;
            new MessageReceiver(_channel, _connectionID);
            sendReply(successTag);  // ok, done!
         }
         else sendReply(_failTag);
      }

      public void messageReceived(Object message, int numLeft)
      {
         if (message instanceof Message)
         {
            if (_channel != null)
            {
               try {
                  _gateway.flattenMessage(_channel, (Message) message);
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
            SocketChannel s = ct._socketChannel;
            ConnectMessage cmsg = ct._cmsg;
            if (ct == _currentConnectThread)
            {
               _currentConnectThread = null;   // no longer in the connect period
               if (s != null)
               {
                 _channel  = s;
                 _failTag = cmsg._failTag;
                 new MessageReceiver(_channel, _connectionID);
                 if (_channel.isOpen()) sendReply(cmsg._successTag);  // ok, done!
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
            if (_outputQueueDrainedTag != null) sendReply(_outputQueueDrainedTag);
         }
      }
   
      /** Close the connection and notify the user (only if the connection ID is correct) */
      private void disconnect(int id)
      {
         if ((_channel != null)&&(_connectionID == id))
         {
            try {
               _channel.close();
            }
            catch(IOException ex) {
               // ignore
            }
            sendReply(_failTag);
            _channel = null;
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
         public MessageReceiver(ByteChannel in, int cid)
         {
            _in = in;
            _connectionID = cid;
            (new MessageQueue(this)).postMessage(null);  // start the background reader thread
         }
         
         public void messageReceived(Object msg, int numLeft)
         {
            int maxIncomingMessageSize = _gateway.getMaximumIncomingMessageSize();
            
            ByteBuffer buffer = ByteBuffer.allocate(128*1024); // default receive-buffer-size is 128KB, per my discussion with Lior
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            int numBytesRead;
            try {
               while(true) {
                  numBytesRead = _in.read(buffer);
                  if (numBytesRead < 0) {
                     // The peer has disconnected, so go away.
                     _sendQueue.postMessage(this);
                     break;
                  }
                  buffer.limit(buffer.position());
                  buffer.rewind();
                  try {
                      while (buffer.remaining() > 0) {
                         buffer.mark();
                         sendReply(_gateway.unflattenMessage(buffer));
                      }
                      buffer.compact();
                  }
                  catch (NotEnoughDataException ned)
                  {
                      int eodPosition = buffer.position();
                      buffer.reset();

                      // Check if there is a need to enlarge the buffer
                      if (buffer.capacity() < (eodPosition + ned.getNumMissingBytes() - buffer.position())) {
                          // The data will exceed the buffer, even after compacting.
                          int desiredSize = buffer.capacity() + ned.getNumMissingBytes();
                          int doubleSize = buffer.capacity()*2;
                          if (desiredSize < doubleSize) desiredSize = (doubleSize < maxIncomingMessageSize) ? doubleSize : maxIncomingMessageSize;
                          if (desiredSize <= maxIncomingMessageSize) 
                          {
                             ByteBuffer tmp = ByteBuffer.allocate(desiredSize);
                             tmp.order(ByteOrder.LITTLE_ENDIAN);
                             tmp.put(buffer);
                             buffer = tmp;
                          }
                          else {
                             // Not enough memory for the message that is expected, and not allowed to allocate enough memory.
                             throw new UnflattenFormatException("Incoming message too large:  "+(buffer.capacity() + ned.getNumMissingBytes())+"/"+maxIncomingMessageSize);
                          }
                      }
                      else {
                          int newPosition = eodPosition - buffer.position();
                    	  buffer.compact();
                    	  buffer.position(newPosition);
                      }
                  }
               }
            }
            catch(Exception ex) {
               // oops, socket broke... notify out master that we're leaving (by posting ourself as a message, teehee)
               _sendQueue.postMessage(this);
            }
         }
         
         public int connectionID() {return _connectionID;}
         
         private int _connectionID;
         private ByteChannel _in;
      }
      
      private Object _failTag              = null;
      private ByteChannel _channel = null;
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
            _socketChannel = SocketChannel.open();
            _socketChannel.socket().setReceiveBufferSize(128*1024);
            _socketChannel.socket().setSendBufferSize(128*1024);
            _socketChannel.connect(new InetSocketAddress(_cmsg._hostName, _cmsg._port));
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
      
      public SocketChannel _socketChannel = null;
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
         ServerSocketChannel servSock = _cmsg._sock;
       
         try {
            while(true) {
               // At this point we know we want at least one connection, we'll test for continuance at the end.
               _socketChannel = servSock.accept();
               
               if (_continuallyListen == false)
               {
                  // If we aren't supposed to keep listening, post this object, killing this thread.
                  _sendQueue.postMessage(this);
               }
               else
               {
                  // If we are supposed to keep goin, send the new socket back to the object that started the transceiver, and let them deal with it.
                  if (_repliesTo != null) _repliesTo.postMessage(_socketChannel);
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
