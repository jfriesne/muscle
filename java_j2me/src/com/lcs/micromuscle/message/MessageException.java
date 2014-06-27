/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.message;

/** Base class for the various Exceptions that the Message class may throw */
public class MessageException extends Exception
{
   private static final long serialVersionUID = -7107595031653595454L;

   public MessageException(String s)
   {
      super(s);
   }
    
   public MessageException()
   {
      super("Error accessing a Message field");
   }
}
