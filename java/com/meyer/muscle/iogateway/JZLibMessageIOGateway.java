package com.meyer.muscle.iogateway;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.ByteChannel;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;

import com.jcraft.jzlib.JZlib;
import com.jcraft.jzlib.ZStream;
import com.meyer.muscle.message.Message;
import com.meyer.muscle.support.LEDataInputStream;
import com.meyer.muscle.support.LEDataOutputStream;
import com.meyer.muscle.support.NotEnoughDataException;
import com.meyer.muscle.support.UnflattenFormatException;

public class JZLibMessageIOGateway extends MessageIOGateway 
{
   protected ZStream _deflateStream;
   protected ZStream _inflateStream;
   
   protected ByteArrayOutputStream _outputByteBuffer;
   protected LEDataOutputStream _leOutputStream;
   private ByteBuffer _msgBuf;
   private ByteBuffer _outgoing;
   private ByteBuffer _uncompressedDataBuffer;

   public JZLibMessageIOGateway() 
   {
      super();
   }

   public JZLibMessageIOGateway(int encoding) 
   {
      super(encoding);
   }

   public void setOutgoingEncoding(int newEncoding)
   {
      super.setOutgoingEncoding(newEncoding);
      _deflateStream = null;
   }
   
   public Message unflattenMessage(DataInput in) throws IOException, UnflattenFormatException 
   {
      int numBytes = in.readInt();
      if (numBytes > getMaximumIncomingMessageSize()) throw new UnflattenFormatException("Incoming message was too large! (" + numBytes + " bytes, " + getMaximumIncomingMessageSize() + " allowed!)");

      int encoding = in.readInt();
      if (encoding == MUSCLE_MESSAGE_DEFAULT_ENCODING) 
      {
         Message pmsg = new Message();
         pmsg.unflatten(in, numBytes);
         return pmsg;
      }
      int independentValue = in.readInt();
      int flatSize = in.readInt();
      boolean needToInit = false;
      boolean independent = (independentValue == ZLIB_CODEC_HEADER_INDEPENDENT);
      if (_inflateStream == null) 
      {
         _inflateStream = new ZStream();
         needToInit = true;
      }
      if (independent) needToInit = true;
      byte[] currChunk = new byte[numBytes - 8];
      byte[] uncompressedData = new byte[flatSize];
      in.readFully(currChunk); // Get the data

      // Uncompress the data
      _inflateStream.next_in = currChunk;
      _inflateStream.next_in_index = 0;
      _inflateStream.next_out = uncompressedData;
      _inflateStream.next_out_index = 0;
      if (needToInit) _inflateStream.inflateInit();

      _inflateStream.avail_in = currChunk.length;
      _inflateStream.avail_out = uncompressedData.length;
      int err = JZlib.Z_DATA_ERROR;
      while((_inflateStream.next_out_index<flatSize)&&(_inflateStream.next_in_index<numBytes))
      {
         err = _inflateStream.inflate(JZlib.Z_SYNC_FLUSH);
         if (err != JZlib.Z_OK) break;
      }

      if (err != JZlib.Z_OK) throw new IOException("Inflator failed");
      
      // Reassemble the message
      Message pmsg = new Message();
      LEDataInputStream dis = new LEDataInputStream(new ByteArrayInputStream(uncompressedData));
      pmsg.unflatten(dis, flatSize);
      return pmsg;
   }
   
   public void flattenMessage(DataOutput out, Message msg) throws IOException 
   {
      int flatSize = msg.flattenedSize();
      int oge = getOutgoingEncoding();
      if ((oge <= MUSCLE_MESSAGE_DEFAULT_ENCODING)||(flatSize <= 32))
      {
         super.flattenMessage(out, msg);
         return;
      }
      // Else, we need to compress the message.
   
      boolean independent = false;
      if (_deflateStream == null) 
      {
         _deflateStream = new ZStream();
         _deflateStream.deflateInit(oge - MUSCLE_MESSAGE_DEFAULT_ENCODING, 15); // suppress the zlib headers
         
         _outputByteBuffer = new ByteArrayOutputStream(flatSize);
         _leOutputStream = new LEDataOutputStream (_outputByteBuffer);
         independent = true;
      }
      msg.flatten(_leOutputStream);
      _leOutputStream.flush();
      
      byte[] compressed = new byte[flatSize];
      
      _deflateStream.next_in = _outputByteBuffer.toByteArray();
      _deflateStream.next_in_index = 0;
      _deflateStream.next_out = compressed;
      _deflateStream.next_out_index = 0;
      _deflateStream.avail_in = _deflateStream.next_in.length;
      _deflateStream.avail_out = compressed.length;
      
      int err = JZlib.Z_DATA_ERROR;
      while (_deflateStream.avail_in > 0) 
      {
         err = _deflateStream.deflate(JZlib.Z_SYNC_FLUSH);
         if ((err == JZlib.Z_STREAM_ERROR)||(err == JZlib.Z_DATA_ERROR)||(err == JZlib.Z_NEED_DICT)||(err == JZlib.Z_BUF_ERROR)) throw new IOException("Problem compressing the outgoing message.");
      }

      out.writeInt(_deflateStream.next_out_index + 8);
      out.writeInt(oge);
      out.writeInt(independent ? ZLIB_CODEC_HEADER_INDEPENDENT : ZLIB_CODEC_HEADER_DEPENDENT);
      out.writeInt(flatSize);
      out.write(compressed, 0, _deflateStream.next_out_index);
      _outputByteBuffer.reset();
   }
   
   public Message unflattenMessage(ByteBuffer in) throws IOException, UnflattenFormatException , NotEnoughDataException
   {
      if (in.remaining() < 8) throw new NotEnoughDataException(8-in.remaining());

      int numBytes = in.getInt();
      if (numBytes > getMaximumIncomingMessageSize()) throw new UnflattenFormatException("Incoming message was too large! (" + numBytes + " bytes, " + getMaximumIncomingMessageSize() + " allowed!)");

      int encoding = in.getInt();
      if (in.remaining() < numBytes) 
      {
         in.position(in.limit());
         throw new NotEnoughDataException(numBytes-in.remaining());
      }
       
      if (encoding == MUSCLE_MESSAGE_DEFAULT_ENCODING) 
      {
         Message pmsg = new Message();
         pmsg.unflatten(in, numBytes);
         return pmsg;
      }
      int independentValue = in.getInt();
      int flatSize = in.getInt();
      boolean needToInit = false;
      boolean independent = (independentValue == ZLIB_CODEC_HEADER_INDEPENDENT);
      if (_inflateStream == null) 
      {
         _inflateStream = new ZStream();
         needToInit = true;
      }
      if (independent) needToInit = true;
      if (_uncompressedDataBuffer == null || _uncompressedDataBuffer.capacity() < flatSize) 
      {
          _uncompressedDataBuffer = ByteBuffer.allocate(flatSize);
          _uncompressedDataBuffer.order(ByteOrder.LITTLE_ENDIAN);          
      }
      _uncompressedDataBuffer.rewind();
      _uncompressedDataBuffer.limit(flatSize);

      // Uncompress the data, use the byte buffer's backing array to do the work.
      _inflateStream.next_in = in.array();
      _inflateStream.next_in_index = in.position();
      _inflateStream.next_out = _uncompressedDataBuffer.array();
      _inflateStream.next_out_index = 0;
      if (needToInit) _inflateStream.inflateInit();

      _inflateStream.avail_in = (numBytes - 8);
      _inflateStream.avail_out = flatSize;
      int err = JZlib.Z_DATA_ERROR;

      do {
         err = _inflateStream.inflate(JZlib.Z_SYNC_FLUSH);
      } while(err == JZlib.Z_OK && (_inflateStream.next_out_index<flatSize)&&(_inflateStream.next_in_index<numBytes));
      if (err != JZlib.Z_OK) throw new IOException("Inflator failed");

      // Make sure that the incoming buffer is advanced to after the current message.
      in.position(in.position()+(numBytes-8));
      
      // Reassemble the message
      Message pmsg = new Message();
      pmsg.unflatten(_uncompressedDataBuffer, flatSize);
      return pmsg;
   }
   
   public ByteBuffer flattenMessage(Message msg) throws IOException 
   {
      if (getOutgoingEncoding() <= MUSCLE_MESSAGE_DEFAULT_ENCODING) return super.flattenMessage(msg);
      int flatSize = msg.flattenedSize();
      if (flatSize <= 32) return super.flattenMessage(msg);

      prepareBuffers(msg, flatSize);
      ByteBuffer buffer = ByteBuffer.allocate(_outgoing.limit());
      buffer.put(_outgoing);
      buffer.flip();
      return buffer;
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
      if (getOutgoingEncoding() <= MUSCLE_MESSAGE_DEFAULT_ENCODING) 
      {
         super.flattenMessage(out, msg);
         return;
      }
      int flatSize = msg.flattenedSize();
      if (flatSize <= 32) 
      {
         super.flattenMessage(out, msg);
         return;
      }
      prepareBuffers(msg, flatSize);
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
         } else out.write(_outgoing);
      } else out.write(_outgoing);
   }

   private void prepareBuffers(Message msg, int flatSize) throws IOException 
   {
      int oge = getOutgoingEncoding();

      // Else, we need to compress the message.
      boolean independent = false;
      if (_deflateStream == null) 
      {
         _deflateStream = new ZStream();
         _deflateStream.deflateInit(oge - MUSCLE_MESSAGE_DEFAULT_ENCODING, 15); // suppress the zlibheaders

         independent = true;
      }
      if ((_msgBuf == null)||(_msgBuf.capacity() < flatSize)) 
      {
         _msgBuf = ByteBuffer.allocate(flatSize);
         _msgBuf.order(ByteOrder.LITTLE_ENDIAN);
      }
      _msgBuf.rewind();
      _msgBuf.limit(flatSize);
      msg.flatten(_msgBuf);

      if ((_outgoing == null)||(_outgoing.capacity() < (flatSize + 16))) 
      {
         _outgoing = ByteBuffer.allocate(flatSize + 16);
         _outgoing.order(ByteOrder.LITTLE_ENDIAN);
      }

      _deflateStream.next_in = _msgBuf.array();
      _deflateStream.next_in_index = 0;
      _deflateStream.next_out = _outgoing.array();
      _deflateStream.next_out_index = 16;
      _deflateStream.avail_in = flatSize;
      _deflateStream.avail_out = flatSize;

      int err = JZlib.Z_DATA_ERROR;
      while(_deflateStream.avail_in > 0) 
      {
         err = _deflateStream.deflate(JZlib.Z_SYNC_FLUSH);
         if ((err == JZlib.Z_STREAM_ERROR)||(err == JZlib.Z_DATA_ERROR)||(err == JZlib.Z_NEED_DICT)||(err == JZlib.Z_BUF_ERROR)) throw new IOException( "Problem compressing the outgoing message.");
      }

      _outgoing.rewind();
      _outgoing.limit(_deflateStream.next_out_index);

      _outgoing.putInt(_deflateStream.next_out_index - 8);
      _outgoing.putInt(oge);
      _outgoing.putInt(independent ? ZLIB_CODEC_HEADER_INDEPENDENT : ZLIB_CODEC_HEADER_DEPENDENT);
      _outgoing.putInt(flatSize);
      _outgoing.rewind();
   }
}
