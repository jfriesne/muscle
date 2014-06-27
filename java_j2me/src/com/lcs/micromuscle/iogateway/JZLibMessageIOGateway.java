package com.meyer.micromuscle.iogateway;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

import com.jcraft.jzlib.JZlib;
import com.jcraft.jzlib.ZStream;
import com.meyer.micromuscle.message.Message;
import com.meyer.micromuscle.support.LEDataInputStream;
import com.meyer.micromuscle.support.LEDataOutputStream;
import com.meyer.micromuscle.support.UnflattenFormatException;

class JZLibMessageIOGateway extends MessageIOGateway 
{
   protected ZStream _deflateStream;
   protected ZStream _inflateStream;
   
   protected ByteArrayOutputStream _outputByteBuffer;
   protected LEDataOutputStream _leOutputStream;

   public JZLibMessageIOGateway() 
   {
      super();
   }

   public JZLibMessageIOGateway(int encoding) 
   {
      super(encoding);
   }

   public Message unflattenMessage(DataInput in) throws IOException, UnflattenFormatException 
   {
      int numBytes = in.readInt();
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
      // Get the data
      in.readFully(currChunk);
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
      if ((_outgoingEncoding <= MUSCLE_MESSAGE_DEFAULT_ENCODING)||(flatSize <= 32)) 
      {
         super.flattenMessage(out, msg);
         return;
      }
      // Else, we need to compress the message.
   
      boolean independent = false;
      if (_deflateStream == null) 
      {
         _deflateStream = new ZStream();
         _deflateStream.deflateInit(_outgoingEncoding - MUSCLE_MESSAGE_DEFAULT_ENCODING, 15); // suppress the zlib headers
         
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
      out.writeInt(_outgoingEncoding);
      out.writeInt(independent ? ZLIB_CODEC_HEADER_INDEPENDENT : ZLIB_CODEC_HEADER_DEPENDENT);
      out.writeInt(flatSize);
      out.write(compressed, 0, _deflateStream.next_out_index);
      _outputByteBuffer.reset();
   }
}
