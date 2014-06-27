/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */

package com.meyer.micromuscle.message;

/** Exception that is thrown if you try to access a Message
 *  field that isn't present in the Message.
 */
public class FieldNotFoundException extends MessageException
{
   private static final long serialVersionUID = -1387490200161526582L;

   public FieldNotFoundException(String s)
   {
      super(s);
   }
    
   public FieldNotFoundException()
   {
      super("Message entry not found");
   }
}
