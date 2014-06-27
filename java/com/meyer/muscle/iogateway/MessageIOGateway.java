/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.iogateway;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;

import com.meyer.muscle.message.Message;
import com.meyer.muscle.support.NotEnoughDataException;
import com.meyer.muscle.support.UnflattenFormatException;

/** A gateway that converts to and from the 'standard' MUSCLE flattened message byte stream. */
public class MessageIOGateway implements AbstractMessageIOGateway 
{
   private int _outgoingEncoding;
   private ByteBuffer _outgoing;
    
   public MessageIOGateway() 
   {
      _outgoingEncoding = MUSCLE_MESSAGE_DEFAULT_ENCODING;
   }
    
   public MessageIOGateway(int encoding)
   {
      setOutgoingEncoding(encoding);
   }
   
   public void setOutgoingEncoding(int newEncoding) 
   {
      if ((newEncoding < MUSCLE_MESSAGE_DEFAULT_ENCODING) || (newEncoding > MUSCLE_MESSAGE_ENCODING_ZLIB_9)) throw new UnsupportedOperationException("The argument is not a supported encoding");
      _outgoingEncoding = newEncoding;
   }
    
   /** Returns this gateway's current MUSCLE_MESSAGE_ENCODING_* value */
   public final int getOutgoingEncoding()
   {
      return _outgoingEncoding;
   }

   /** Set the largest allowable size for incoming Message objects.  Default value is Integer.MAX_VALUE. */
   public void setMaximumIncomingMessageSize(int maxSize) {_maximumIncomingMessageSize = maxSize;}
    
   /** Returns the current maximum-incoming-message-size.  Default value is Integer.MAX_VALUE. */
   public int getMaximumIncomingMessageSize() {return _maximumIncomingMessageSize;}
    
   public Message unflattenMessage(ByteBuffer in) throws IOException, UnflattenFormatException, NotEnoughDataException
   {
      if (in.remaining() < 8) throw new NotEnoughDataException(8-in.remaining());

      int numBytes = in.getInt();
      if (numBytes > getMaximumIncomingMessageSize()) throw new UnflattenFormatException("Incoming message was too large! (" + numBytes + " bytes, " + getMaximumIncomingMessageSize() + " allowed!)");

      int encoding = in.getInt();
      if (encoding != MUSCLE_MESSAGE_DEFAULT_ENCODING) throw new IOException();
      if (in.remaining() < numBytes) 
      {
          in.position(in.limit());
          throw new NotEnoughDataException(numBytes-in.remaining()); 
      }

      Message pmsg = new Message();
      pmsg.unflatten(in, numBytes);
      return pmsg;
   }
    
   /** Reads from the input stream until a Message can be assembled and returned.
     * @param in The input stream to read from.
     * @throws IOException if there is an error reading from the stream.
     * @throws UnflattenFormatException if there is an error parsing the data in the stream.
     * @return The next assembled Message.
     */
   public Message unflattenMessage(DataInput in) throws IOException, UnflattenFormatException
   {
      int numBytes = in.readInt();
      if (numBytes > getMaximumIncomingMessageSize()) throw new UnflattenFormatException("Incoming message was too large! (" + numBytes + " bytes, " + getMaximumIncomingMessageSize() + " allowed!)");

      int encoding = in.readInt();
      if (encoding != MUSCLE_MESSAGE_DEFAULT_ENCODING) throw new IOException(); 
      Message pmsg = new Message();
      pmsg.unflatten(in, numBytes);
      return pmsg;
   }

   /** Converts the given Message into bytes and sends it out the stream.
     * @param out the data stream to send the converted bytes to.
     * @param msg the Message to convert.
     * @throws IOException if there is an error writing to the stream.
     */
   public void flattenMessage(DataOutput out, Message msg) throws IOException
   {
      out.writeInt(msg.flattenedSize());
      out.writeInt(MUSCLE_MESSAGE_DEFAULT_ENCODING);
      msg.flatten(out);
   }

   /** Converts the given Message into bytes and sends it out the ByteChannel.
     * This method will block if necessary, even if the ByteChannel is
     * non-blocking.  If you want to do 100% proper non-blocking I/O,
     * you'll need to use the version of flattenMessage() that returns
     * a ByteBuffer, and then handle the byte transfers yourself.
     * @param out the ByteChannel to send the converted bytes to.
     * @param msg the Message to convert.
     * @throws IOException if there is an error writing to the stream.
     */
   public void flattenMessage(ByteChannel out, Message msg) throws IOException
   {
      int flattenedSize = msg.flattenedSize();
      if ((_outgoing == null)||(_outgoing.capacity() < 8+flattenedSize))
      {
          _outgoing = ByteBuffer.allocate(8+flattenedSize);
          _outgoing.order(ByteOrder.LITTLE_ENDIAN);
      }
      _outgoing.rewind();
      _outgoing.limit(8+flattenedSize);
      
      _outgoing.putInt(flattenedSize);
      _outgoing.putInt(MUSCLE_MESSAGE_DEFAULT_ENCODING);
      msg.flatten(_outgoing);
      _outgoing.rewind();

      if (out instanceof SelectableChannel)
      {
         SelectableChannel sc = (SelectableChannel) out;
         if (!sc.isBlocking()) 
         {
           int numBytesWritten = 0;
           
           Selector selector = Selector.open();
           sc.register(selector, SelectionKey.OP_WRITE);
           while(_outgoing.remaining() > 0) 
           {
              if (numBytesWritten == 0) 
              {
                 selector.select();

                 // Slight shortcut here - there's only one key registered in this selector object.
                 if (selector.selectedKeys().isEmpty()) continue;
                                                   else selector.selectedKeys().clear();
              }
              numBytesWritten = out.write(_outgoing);
           }
           selector.close();
         }
         else out.write(_outgoing);
      }
      else out.write(_outgoing);
   }
   
   public ByteBuffer flattenMessage(Message msg) throws IOException 
   {
      ByteBuffer buffer;
      int flattenedSize = msg.flattenedSize();
      buffer = ByteBuffer.allocate(flattenedSize+8);
      buffer.order(ByteOrder.LITTLE_ENDIAN);
      buffer.rewind();
      buffer.limit(8+flattenedSize);
       
      buffer.putInt(flattenedSize);
      buffer.putInt(MUSCLE_MESSAGE_DEFAULT_ENCODING);
      msg.flatten(buffer);
      buffer.rewind();
      return buffer;
   }
   
   protected final static int ZLIB_CODEC_HEADER_INDEPENDENT = 2053925219; // 'zlic'    
   protected final static int ZLIB_CODEC_HEADER_DEPENDENT   = 2053925218; // 'zlib'
   private int _maximumIncomingMessageSize = Integer.MAX_VALUE;
}
