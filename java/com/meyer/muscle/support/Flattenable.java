/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.support;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;

/** Interface for objects that can be flattened and unflattened from Be-style byte streams */
public interface Flattenable extends TypeConstants
{
   /** Should return true iff every object of this type has a flattened size that is known at compile time. */
   public boolean isFixedSize();
   
   /** Should return the type code identifying this type of object.  */
   public int typeCode();
   
   /** Should return the number of bytes needed to store this object in its current state.  */
   public int flattenedSize();
   
   /** Should return a clone of this object */
   public Flattenable cloneFlat();

   /** Should set this object's state equal to that of (setFromMe), or throw an UnflattenFormatException if it can't be done.
    *  @param setFromMe The object we want to be like.
    *  @throws ClassCastException if (setFromMe) is the wrong type.
    */
   public void setEqualTo(Flattenable setFromMe) throws ClassCastException;
   
   /** 
    *  Should store this object's state into (buffer). 
    *  @param out The DataOutput to send the data to.
    *  @throws IOException if there is a problem writing out the output bytes.
    */
   public void flatten(DataOutput out) throws IOException;
   
   /** 
    *  Should return true iff a buffer with type_code (code) can be used to reconstruct
    *  this object's state.
    *  @param code A type code ant, e.g. B_RAW_TYPE or B_STRING_TYPE, or something custom.
    *  @return True iff this object can Unflatten from a buffer of the given type, false otherwise.
    */
   public boolean allowsTypeCode(int code);
   
   /** 
    *  Should attempt to restore this object's state from the given buffer.
    *  @param in The stream to read the object from.
    *  @param numBytes The number of bytes the object takes up in the stream, or negative if this is unknown.
    *  @throws IOException if there is a problem reading in the input bytes.
    *  @throws UnflattenFormatException if the bytes that were read in weren't in the expected format.
    */
   public void unflatten(DataInput in, int numBytes) throws IOException, UnflattenFormatException;
}
