/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */

package com.meyer.micromuscle.iogateway;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import com.meyer.micromuscle.support.UnflattenFormatException;
import com.meyer.micromuscle.message.Message;

/** Interface for an object that knows how to translate bytes
  * into Messages, and vice versa.
  */
public interface AbstractMessageIOGateway 
{
   public final static int MUSCLE_MESSAGE_DEFAULT_ENCODING =  1164862256;  // 'Enc0'  ... our default (plain-vanilla) encoding 
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_1  =  1164862257;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_2  =  1164862258;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_3  =  1164862259;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_4  =  1164862260;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_5  =  1164862261;  
   /** This is the recommended CPU vs space-savings setting for zlib */
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_6  =  1164862262;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_7  =  1164862263;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_8  =  1164862264;  
   public final static int MUSCLE_MESSAGE_ENCODING_ZLIB_9  =  1164862265;
   
   /** Reads from the input stream until a Message can be assembled and returned.
     * @param in The input stream to read from.
     * @throws IOException if there is an error reading from the stream.
     * @throws UnflattenFormatException if there is an error parsing the data in the stream.
     * @return The next assembled Message.
     */
   public Message unflattenMessage(DataInput in) throws IOException, UnflattenFormatException;
   
   /** Converts the given Message into bytes and sends it out the stream.
     * @param out the data stream to send the converted bytes to.
     * @param message the Message to convert.
     * @throws IOException if there is an error writing to the stream.
     */
   public void flattenMessage(DataOutput out, Message msg) throws IOException;
}
