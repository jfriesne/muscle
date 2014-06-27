/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.iogateway;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

import com.meyer.micromuscle.message.Message;
import com.meyer.micromuscle.support.UnflattenFormatException;

/** A gateway that converts to and from the 'standard' MUSCLE flattened message byte stream. */
public class MessageIOGateway implements AbstractMessageIOGateway 
{
    protected int _outgoingEncoding;
    
    public MessageIOGateway() 
    {
       _outgoingEncoding = MUSCLE_MESSAGE_DEFAULT_ENCODING;
    }
    
    public MessageIOGateway(int encoding) 
    {
       if (encoding < MUSCLE_MESSAGE_DEFAULT_ENCODING || encoding > MUSCLE_MESSAGE_ENCODING_ZLIB_9) 
       {
          throw new RuntimeException("The encoding passed is not supported");
       }
       _outgoingEncoding = encoding;
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
       int encoding = in.readInt();
       if (encoding != MUSCLE_MESSAGE_DEFAULT_ENCODING) throw new IOException(); 
       Message pmsg = pmsg = new Message();
       pmsg.unflatten(in, numBytes);
       return pmsg;
    }
    
    /** Converts the given Message into bytes and sends it out the stream.
      * @param out the data stream to send the converted bytes to.
      * @param message the Message to convert.
      * @throws IOException if there is an error writing to the stream.
      */
    public void flattenMessage(DataOutput out, Message msg) throws IOException
    {
       out.writeInt(msg.flattenedSize());
       out.writeInt(MUSCLE_MESSAGE_DEFAULT_ENCODING);
       msg.flatten(out);
    }
    
    protected final static int ZLIB_CODEC_HEADER_INDEPENDENT    = 2053925219;               // 'zlic'    
    protected final static int ZLIB_CODEC_HEADER_DEPENDENT      = 2053925218;               // 'zlib'
}
