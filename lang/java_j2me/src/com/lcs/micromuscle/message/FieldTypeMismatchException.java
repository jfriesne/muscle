/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.message;

/** Exception that is thrown if you try to access a field in a Message
 *  by the wrong type (e.g. calling getInt() on a string field or somesuch)
 */
public class FieldTypeMismatchException extends MessageException
{
   private static final long serialVersionUID = -8562539748755552587L;

   public FieldTypeMismatchException(String s)
   {
      super(s);
   }
    
   public FieldTypeMismatchException()
   {
      super("Message entry type mismatch");
   }
}
