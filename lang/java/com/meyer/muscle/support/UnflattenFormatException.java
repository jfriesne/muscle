/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.support;

import java.io.IOException;

/** Exception that is thrown when an unflatten() attempt fails, usually
  * due to unrecognized data in the input stream, or a type mismatch.
  */
public class UnflattenFormatException extends IOException
{
    private static final long serialVersionUID = 234010774012207620L;

    public UnflattenFormatException()
    {
       super("unexpected bytes during Flattenable unflatten");
    }

    public UnflattenFormatException(String s)
    {
       super(s);
    }
}

