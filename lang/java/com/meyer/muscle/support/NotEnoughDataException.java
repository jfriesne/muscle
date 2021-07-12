/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.support;

import java.io.IOException;

/** Exception that is thrown when there is enough data in a buffer
  * to unflatten an entire Message.
  */
public final class NotEnoughDataException extends IOException
{
   private static final long serialVersionUID = 234010774012207621L;
   private int _numMissingBytes;

   public NotEnoughDataException(int numMissingBytes)
   {
      _numMissingBytes = numMissingBytes;
   }
    
   public final int getNumMissingBytes() {return _numMissingBytes;}
}
