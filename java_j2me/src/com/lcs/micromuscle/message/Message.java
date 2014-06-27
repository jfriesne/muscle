/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.micromuscle.message;

import java.io.DataInput;
import java.io.DataOutput;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import com.meyer.micromuscle.support.Flattenable;
import com.meyer.micromuscle.support.LEDataInputStream;
import com.meyer.micromuscle.support.LEDataOutputStream;
import com.meyer.micromuscle.support.Point;
import com.meyer.micromuscle.support.Rect;
import com.meyer.micromuscle.support.UnflattenFormatException;

/** 
 *  This class is sort of similar to Be's BMessage class.  When flattened,
 *  the resulting byte stream is compatible with the flattened
 *  buffers of MUSCLE's C++ Message class.
 *  It only acts as a serializable data container; it does not 
 *  include any threading capabilities.  
 */
public final class Message implements Flattenable
{
   /** Oldest serialization protocol version parsable by this code's unflatten() methods */
   public static final int OLDEST_SUPPORTED_PROTOCOL_VERSION = 1347235888; // 'PM00'

   /** Newest serialization protocol version parsable by this code's unflatten() methods,
     * as well as the version of the protocol produce by this code's flatten() methods. */
   public static final int CURRENT_PROTOCOL_VERSION          = 1347235888; // 'PM00'
   
   /** 32 bit 'what' code, for quick identification of message types.  Set this however you like. */
   public int what = 0;

   /** Default Constructor. */
   public Message()
   {
      if (_empty == null) _empty = new Hashtable();  // IE5 is lame
   }

   /** Constructor.
    *  @param w The 'what' member variable will be set to the value you specify here.
    */
   public Message(int w)
   {
      this();
      what = w;
   }

   /** Copy Constructor.
    *  @param copy The Message to make us a (wholly independant) copy of.
     */
   public Message(Message copyMe)
   {
      this();
      setEqualTo(copyMe);
   }
   
   /** Returns an independent copy of this Message */
   public Flattenable cloneFlat()
   {
      Message clone = new Message();  
      clone.setEqualTo(this); 
      return clone;
   }

   /** Sets this Message equal to (c)
     * @param c What to make ourself a clone of.  Should be a Message
     * @throws ClassCastException if (c) isn't a Message.
     */
   public void setEqualTo(Flattenable c) throws ClassCastException
   {
      clear();
      Message copyMe = (Message)c;
      what = copyMe.what;
      Enumeration fields = copyMe.fieldNames();
      while(fields.hasMoreElements()) 
      {
         try {
            copyMe.copyField((String)fields.nextElement(), this);
         }
         catch(FieldNotFoundException ex) {
            ex.printStackTrace();  // should never happen
         }
      }
   }

   /** Returns an Enumeration of Strings that are the
     * field names present in this Message
     */
   public Enumeration fieldNames()
   {
      return (_fieldTable != null) ? _fieldTable.keys() : _empty.keys();
   }

   /** Returns the given 'what' constant as a human readable 4-byte string, e.g. "BOOL", "BYTE", etc.
    *  @param w Any 32-bit value you would like to have turned into a String.
    */
   public static String whatString(int w)
   {
      byte [] temp = new byte[4];
      for (int i=0; i<4; i++)
      {
         byte b = (byte)((w >> ((3-i)*8)) & 0xFF); 
         if ((b<' ')||(b>'~')) b = '?';
         temp[i] = b;
      }
      return new String(temp);
   }

   /** Returns the number of field names of the given type that are present in the Message.
    *  @param type The type of field to count, or B_ANY_TYPE to count all field types.
    *  @return The number of matching fields, or zero if there are no fields of the appropriate type.
    */
   public int countFields(int type) 
   {
      if (_fieldTable == null) return 0;
      if (type == B_ANY_TYPE) return _fieldTable.size();
      
      int count = 0;
      Enumeration e = _fieldTable.elements();
      while(e.hasMoreElements())
      {
         MessageField field = (MessageField) e.nextElement();
         if (field.typeCode() == type) count++;
      }
      return count;
   }

   /** Returns the total number of fields in this Message. */
   public int countFields() {return countFields(B_ANY_TYPE);}
   
   /** Returns true iff there are no fields in this Message. */
   public boolean isEmpty() {return (countFields() == 0);}

   /** Returns a string that is a summary of the contents of this Message.  Good for debugging. */
   public String toString()
   {
      String ret = "Message:  what='" + whatString(what) + "' ("+what+"), countFields=" + countFields() + ", flattenedSize=" + flattenedSize() + "\n";
      // Better to not use the keyword "enum" - it is reserved in JDK 1.5
      Enumeration enumeration = fieldNames();
      while(enumeration.hasMoreElements())
      {
         String fieldName = (String) enumeration.nextElement();
         ret += "   " + fieldName + ": " + _fieldTable.get(fieldName).toString() + "\n";
      }
      return ret;
   }
   
   /** Renames a field.
    *  @param old_entry Field name to rename from.
    *  @param new_entry Field name to rename to.  If a field with this name already exists, it will be replaced.
    *  @throws FieldNotFoundException if a field named (old_entry) can't be found.
    */
   public void renameField(String old_entry, String new_entry) throws FieldNotFoundException
   {
      MessageField field = getField(old_entry); 
      _fieldTable.remove(old_entry);
      _fieldTable.put(new_entry, field);
   }

   /** Returns false (Messages can be of various sizes, depending on how many fields they have, etc.) */
   public boolean isFixedSize() {return false;}

   /** Returns B_MESSAGE_TYPE */
   public int typeCode() {return B_MESSAGE_TYPE;}

   /** Returns The number of bytes it would take to flatten this Message into a byte buffer. */
   public int flattenedSize() 
   {
      int sum = 4 + 4 + 4;  // 4 bytes for the protocol revision #, 4 bytes for the number-of-entries field, 4 bytes for what code
      if (_fieldTable != null)
      {
         Enumeration e = fieldNames();
         while(e.hasMoreElements())
         {
            String fieldName = (String) e.nextElement();
            MessageField field = (MessageField) _fieldTable.get(fieldName);
 
            // 4 bytes for the name length, name data, 4 bytes for entry type code, 4 bytes for entry data length, entry data
            sum += 4 + (fieldName.length()+1) + 4 + 4 + field.flattenedSize();
         }
      }
      return sum;
   }
   
   /** Returns true iff (code) is B_MESSAGE_TYPE */
   public boolean allowsTypeCode(int code) {return (code == B_MESSAGE_TYPE);}
   
   /**
    *  Converts this Message into a flattened stream of bytes that can be saved to disk
    *  or sent over a network, and later converted back into an identical Message object.
    *  @param out The stream to output bytes to.  (Should generally be an LEOutputDataStream object)
    *  @throws IOException if there is a problem outputting the bytes.
    */
   public void flatten(DataOutput out) throws IOException 
   {
      // Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION)
      //          1. 'what' code (4 bytes)
      //          2. Number of entries (4 bytes)
      //          3. Entry name length (4 bytes)
      //          4. Entry name string (flattened String)
      //          5. Entry type code (4 bytes)
      //          6. Entry data length (4 bytes)
      //          7. Entry data (n bytes)
      //          8. loop to 3 as necessary         
      out.writeInt(CURRENT_PROTOCOL_VERSION);
      out.writeInt(what);
      out.writeInt(countFields());
      Enumeration e = fieldNames();
      while(e.hasMoreElements())
      {
         String name = (String) e.nextElement();
         MessageField field = (MessageField) _fieldTable.get(name);
         out.writeInt(name.length()+1);
	    byte[] nameChars = name.getBytes();
	    out.write(nameChars, 0, nameChars.length);
         out.writeByte(0);  // terminating NUL byte
         out.writeInt(field.typeCode());
         out.writeInt(field.flattenedSize());
         field.flatten(out);
      }
   }

   /**
    *  Convert bytes from the given stream back into a Message.  Any previous contents of
    *  this Message will be erased, and replaced with the data specified in the byte buffer.
    *  @param in The stream to get bytes from.  (Should generally be an LEInputDataStream object)
    *  @param numBytes The number of bytes this object takes up in the stream, or negative if the number is not known.
    *  @throws UnflattenFormatException if the data in (buf) wasn't well-formed.
    *  @throws IOException if there was an error reading from the input stream.
    */
   public void unflatten(DataInput in, int numBytes) throws UnflattenFormatException, IOException
   {
      clear();
      int protocolVersion = in.readInt(); 
      if ((protocolVersion > CURRENT_PROTOCOL_VERSION)||(protocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)) throw new UnflattenFormatException("Version mismatch error");
      what = in.readInt();
      int numEntries = in.readInt();
      byte [] stringBuf = null;
      if (numEntries > 0) ensureFieldTableAllocated();
      for (int i=0; i<numEntries; i++)
      {
         int fieldNameLength = in.readInt();
         if ((stringBuf == null)||(stringBuf.length < fieldNameLength)) stringBuf = new byte[((fieldNameLength>5)?fieldNameLength:5)*2];
         in.readFully(stringBuf, 0, fieldNameLength);
         MessageField field = getCreateOrReplaceField(new String(stringBuf, 0, fieldNameLength-1), in.readInt());
         field.unflatten(in, in.readInt());
         _fieldTable.put(new String(stringBuf, 0, fieldNameLength-1), field);
      }
   }

   /** Sets the given field name to contain a single boolean value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setBoolean(String name, boolean val)
   {
      MessageField field = getCreateOrReplaceField(name, B_BOOL_TYPE);
      boolean [] array = (boolean[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new boolean[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given boolean values.  Any previous field contents are replaced. 
    *  @param name Name of the field to set vlaues.
    *  @param val Array of boolean values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setBooleans(String name, boolean [] vals) {setObjects(name, B_BOOL_TYPE, vals, vals.length);}

   /** Returns the first boolean value in the given field. 
    *  @param name Name of the field to look for a boolean value in.
    *  @return The first boolean value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_BOOL_TYPE field.
    */
   public boolean getBoolean(String name) throws MessageException {return getBooleans(name)[0];}

   /** Returns the first boolean value in the given field, or the default specified.
    * @param name Name of the field to look for a boolean value in
    * @param def Default value if the field dosen't exist or is the wrong type.
    */
   public boolean getBoolean(String name, boolean def) {
      try {
         return getBoolean(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of boolean values. 
    *  @param name Name of the field to look for boolean values in.
    *  @return The array of boolean values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_BOOL_TYPE field.
    */
   public boolean[] getBooleans(String name) throws MessageException {return (boolean[]) getData(name, B_BOOL_TYPE);}

   /** Sets the given field name to contain a single byte value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setByte(String name, byte val)
   {
      MessageField field = getCreateOrReplaceField(name, B_INT8_TYPE);
      byte [] array = (byte[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new byte[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given byte values.  Any previous field contents are replaced. 
    *  @param name Name of the field to set vlaues.
    *  @param val Array of byte values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setBytes(String name, byte [] vals) {setObjects(name, B_INT8_TYPE, vals, vals.length);}

   /** Returns the first byte value in the given field. 
    *  @param name Name of the field to look for a byte value in.
    *  @return The first byte value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT8_TYPE field.
    */
   public byte getByte(String name) throws MessageException {return getBytes(name)[0];}
   
   /** Returns the first byte value in the given field. 
    *  @param name Name of the field to look for a byte value in.
    *  @param def Default value to return if the field dosen't exist or is the wrong type.
    *  @return The first byte value in the field.
    */
   public byte getByte(String name, byte def) {
      try {
         return getByte(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of byte values. 
    *  @param name Name of the field to look for byte values in.
    *  @return The array of byte values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT8_TYPE field.
    */
   public byte[] getBytes(String name) throws MessageException {return (byte[]) getData(name, B_INT8_TYPE);}
    
   /** Sets the given field name to contain a single short value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setShort(String name, short val)
   {
      MessageField field = getCreateOrReplaceField(name, B_INT16_TYPE);
      short [] array = (short[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new short[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given short values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of short values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setShorts(String name, short [] vals) {setObjects(name, B_INT16_TYPE, vals, vals.length);}

   /** Returns the first short value in the given field. 
    *  @param name Name of the field to look for a short value in.
    *  @return The first short value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT16_TYPE field.
    */
   public short getShort(String name) throws MessageException {return getShorts(name)[0];}
   
   /** Returns the first short value in the given field. 
    *  @param name Name of the field to look for a short value in.
    *  @param def the default value to return if the field dosen't exist or is the wrong type.
    *  @return The first short value in the field.
    */
   public short getShort(String name, short def) {
      try {
         return getShort(name);
      } catch (MessageException me) {
         return def;
      }
   }

   /** Returns the contents of the given field as an array of short values. 
    *  @param name Name of the field to look for short values in.
    *  @return The array of short values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT16_TYPE field.
    */
   public short[] getShorts(String name) throws MessageException {return (short[]) getData(name, B_INT16_TYPE);}
    
   /** Sets the given field name to contain a single int value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setInt(String name, int val)
   {
      MessageField field = getCreateOrReplaceField(name, B_INT32_TYPE);
      int [] array = (int[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new int[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given int values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of int values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setInts(String name, int [] vals) {setObjects(name, B_INT32_TYPE, vals, vals.length);}

   /** Returns the first int value in the given field. 
    *  @param name Name of the field to look for a int value in.
    *  @return The first int value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT32_TYPE field.
    */
   public int getInt(String name) throws MessageException {return getInts(name)[0];}
   
   /** Returns the first int value in the given field. 
    *  @param name Name of the field to look for a int value in.
    *  @param def The value to return if the field dosen't exist, or if the field is the incorrect type.
    *  @return The first int value in the field.
    */
   public int getInt(String name, int def) {
      try {
         return getInt(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of int values. 
    *  @param name Name of the field to look for int values in.
    *  @return The array of int values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT32_TYPE field.
    */
   public int[] getInts(String name) throws MessageException {return (int[]) getData(name, B_INT32_TYPE);}
   
   /** Returns the contents of the given field as an array of int values. 
    *  @param name Name of the field to look for int values in.
    *  @param defs The Default array to return in the event that one does not exist, or an error occurs.
    *  @return The array of int values associated with (name).  Note that this array is still part of this Message.
    */
   public int[] getInts(String name, int[] defs) {
      try {
         return getInts(name);
      } catch (MessageException me) {
         return defs;
      }
   }
   
   /** Sets the given field name to contain a single long value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setLong(String name, long val)
   {
      MessageField field = getCreateOrReplaceField(name, B_INT64_TYPE);
      long [] array = (long[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new long[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given long values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of long values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setLongs(String name, long [] vals) {setObjects(name, B_INT64_TYPE, vals, vals.length);}

   /** Returns the first long value in the given field. 
    *  @param name Name of the field to look for a long value in.
    *  @return The first long value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT64_TYPE field.
    */
   public long getLong(String name) throws MessageException {return getLongs(name)[0];}
   
   /** Returns the first long value in the given field. 
    *  @param name Name of the field to look for a long value in.
    *  @param def The Default value to return if the field dosen't exist, or if it's the wrong type.
    *  @return The first long value in the field.
    */
   public long getLong(String name, long def) {
      try {
         return getLong(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of long values. 
    *  @param name Name of the field to look for long values in.
    *  @return The array of long values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_INT64_TYPE field.
    */
   public long[] getLongs(String name) throws MessageException {return (long[]) getData(name, B_INT64_TYPE);}
    
   /** Sets the given field name to contain a single float value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setFloat(String name, float val)
   {
      MessageField field = getCreateOrReplaceField(name, B_FLOAT_TYPE);
      float [] array = (float[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new float[1];
      array[0] = val;
      field.setPayload(array, 1);
   }

   /** Sets the given field name to contain the given float values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of float values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setFloats(String name, float [] vals) {setObjects(name, B_FLOAT_TYPE, vals, vals.length);}

   /** Returns the first float value in the given field. 
    *  @param name Name of the field to look for a float value in.
    *  @return The first float value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_FLOAT_TYPE field.
    */
   public float getFloat(String name) throws MessageException {return getFloats(name)[0];}
   
   /** Returns the first float value in the given field. 
    *  @param name Name of the field to look for a float value in.
    *  @param def the Default value to return if the field dosen't exist, or is the wrong type.
    *  @return The first float value in the field.
    */
   public float getFloat(String name, float def) {
      try {
         return getFloat(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of float values. 
    *  @param name Name of the field to look for float values in.
    *  @return The array of float values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_FLOAT_TYPE field.
    */
   public float[] getFloats(String name) throws MessageException {return (float[]) getData(name, B_FLOAT_TYPE);}
    
   /** Sets the given field name to contain a single double value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setDouble(String name, double val)
   {
      MessageField field = getCreateOrReplaceField(name, B_DOUBLE_TYPE);
      double [] array = (double[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new double[1];
      array[0] = val;
      field.setPayload(array, 1); 
   }

   /** Sets the given field name to contain the given double values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of double values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setDoubles(String name, double [] vals) {setObjects(name, B_DOUBLE_TYPE, vals, vals.length);}

   /** Returns the first double value in the given field. 
    *  @param name Name of the field to look for a double value in.
    *  @return The first double value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_DOUBLE_TYPE field.
    */
   public double getDouble(String name) throws MessageException {return getDoubles(name)[0];}
   
   /** Returns the first double value in the given field. 
    *  @param name Name of the field to look for a double value in.
    *  @param def The Default value to return if the field dosen't exist.
    *  @return The first double value in the field.
    */
   public double getDouble(String name, double def) {
      try {
         return getDouble(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of double values. 
    *  @param name Name of the field to look for double values in.
    *  @return The array of double values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_DOUBLE_TYPE field.
    */
   public double[] getDoubles(String name) throws MessageException {return (double[]) getData(name, B_DOUBLE_TYPE);}
    
   /** Sets the given field name to contain a single String value.  Any previous field contents are replaced. 
    *  @param name Name of the field to set
    *  @param val Value that will become the sole value in the specified field.
    */
   public void setString(String name, String val) 
   {
      MessageField field = getCreateOrReplaceField(name, B_STRING_TYPE);
      String [] array = (String[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new String[1];
      array[0] = val;
      field.setPayload(array, 1);
   }
   
   /** Sets the given field name to contain the given String values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of String values to assign to the field.  Note that the array is not copied; rather the passed-in array becomes part of the Message.
    */
   public void setStrings(String name, String [] vals) {setObjects(name, B_STRING_TYPE, vals, vals.length);}

   /** 
    *  @param name The name of the field to access.
    *  @return the value(s) associated with the requested field name
    *  @throws MessageFieldNotFoundException If the given field name isn't present in the message
    *  @throws MessageFieldTypeMismatchException If the given field exists, but is the wrong type of data.
    */
   public String getString(String name) throws MessageException {return getStrings(name)[0];}
   
   /** 
    *  @param name The name of the field to access.
    *  @param def the default value to return if the field dosen't exist or is the wrong type.
    *  @return the value(s) associated with the requested field name
    */
   public String getString(String name, String def) {
      try {
         return getString(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of String values. 
    *  @param name Name of the field to look for String values in.
    *  @return The array of String values associated with (name).  Note that this array is still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_STRING_TYPE field.
    */
   public String[] getStrings(String name) throws MessageException {return (String[]) getData(name, B_STRING_TYPE);}
   
   /** Returns the contents of the given field as an array of String values. 
    *  @param name Name of the field to look for String values in.
    *  @param def the Default values to return.
    *  @return The array of String values associated with (name).  Note that this array is still part of this Message.
    */
   public String[] getStrings(String name, String[] def) {
      try {
         return (String[]) getData(name, B_STRING_TYPE);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Sets the given field name to contain a single Message value.  
     * Any previous field contents are replaced. 
     * @param name Name of the field to set
     * @param val Value that will become the sole value in the specified field.
     * Note that a copy of (val) is NOT made; the passed-in mesage object becomes part of this Message.
     */
   public void setMessage(String name, Message val) 
   {
      MessageField field = getCreateOrReplaceField(name, B_MESSAGE_TYPE);
      Message [] array = (Message[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new Message[1];
      array[0] = val;
      field.setPayload(array, 1);      
   }
   
   /** Sets the given field name to contain the given Message values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of Message objects to assign to the field.  Note that the neither the array nor the
    *             Message objects are copied; rather both the array and the Messages become part of this Message.
    */
   public void setMessages(String name, Message [] vals) {setObjects(name, B_MESSAGE_TYPE, vals, vals.length);}

   /** Returns the first Message value in the given field. 
    *  @param name Name of the field to look for a Message value in.
    *  @return The first Message value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_MESSAGE_TYPE field.
    */
   public Message getMessage(String name) throws MessageException {return getMessages(name)[0];}

   /** Returns the contents of the given field as an array of Message values.
    *  @param name Name of the field to look for Message values in.
    *  @return The array of Message values associated with (name).  Note that the array and the Messages
    *          it holds are still part of this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_MESSAGE_TYPE field.
    */
   public Message[] getMessages(String name) throws MessageException {return (Message[]) getData(name, B_MESSAGE_TYPE);}
    
   /** Sets the given field name to contain a single Point value.
     * Any previous field contents are replaced. 
     * @param name Name of the field to set
     * @param val Value that will become the sole value in the specified field.
     * Note that a copy of (val) is NOT made; the passed-in object becomes part of this Message.
     */
   public void setPoint(String name, Point val) 
   {
      MessageField field = getCreateOrReplaceField(name, B_POINT_TYPE);
      Point [] array = (Point[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new Point[1];
      array[0] = val;
      field.setPayload(array, 1);      
   }
   
   /** Sets the given field name to contain the given Point values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of Point objects to assign to the field.  Note that neither the array nor the 
    *             Point objects are copied; rather both the array and the Points become part of this Message.
    */
   public void setPoints(String name, Point [] vals) {setObjects(name, B_POINT_TYPE, vals, vals.length);}

   /** Returns the first Point value in the given field. 
    *  @param name Name of the field to look for a Point value in.
    *  @return The first Point value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_POINT_TYPE field.
    */
   public Point getPoint(String name) throws MessageException {return getPoints(name)[0];}
   
   /** Returns the first Point value in the given field. 
    *  @param name Name of the field to look for a Point value in.
    *  @param def The Default value to return if the field dosen't exist or is the wrong type.
    *  @return The first Point value in the field.
    */
   public Point getPoint(String name, Point def) {
      try {
         return getPoint(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of Point values.  
    *  @param name Name of the field to look for Point values in. 
    *  @return The array of Point values associated with (name).  Note that the array and the
    *          objects it holds are still part of the Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_POINT_TYPE field.
    */
   public Point[] getPoints(String name) throws MessageException {return (Point[]) getData(name, B_POINT_TYPE);}
    
   /** Sets the given field name to contain a single Rect value. 
     * Any previous field contents are replaced. 
     * @param name Name of the field to set
     * @param val Value that will become the sole value in the specified field.
     * Note that a copy of (val) is NOT made; the passed-in object becomes part of this Message.
     */
   public void setRect(String name, Rect val) 
   {
      MessageField field = getCreateOrReplaceField(name, B_RECT_TYPE);
      Rect [] array = (Rect[]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new Rect[1];
      array[0] = val;
      field.setPayload(array, 1);
   }
   
   /** Sets the given field name to contain the given Rect values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of Rect objects to assign to the field.  Note that neither the array nor the
    *             Rect objects are copied; rather both the array and the Rects become part of this Message.
    */
   public void setRects(String name, Rect [] vals) {setObjects(name, B_RECT_TYPE, vals, vals.length);}

   /** Returns the first Rect value in the given field.  Note that the returned object is still part of this Message.
    *  @param name Name of the field to look for a Rect value in.
    *  @return The first Rect value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_RECT_TYPE field.
    */
   public Rect getRect(String name) throws MessageException {return getRects(name)[0];}
   
   /** Returns the first Rect value in the given field.  Note that the returned object is still part of this Message.
    *  @param name Name of the field to look for a Rect value in.
    *  @param def the default value to return if the field dosen't exist or is the wrong type.
    *  @return The first Rect value in the field.
    */
   public Rect getRect(String name, Rect def) {
      try {
         return getRect(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of Rect values.  
    *  @param name Name of the field to look for Rect values in.
    *  @return The array of Rect values associated with (name).  Note that the array and the Rects that 
    *          it holds are still part of the Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name is not a B_RECT_TYPE field.
    */
   public Rect[] getRects(String name) throws MessageException {return (Rect[]) getData(name, B_RECT_TYPE);}
    
   /** Sets the given field name to contain the flattened bytes of the single given Flattenable object.
     * Any previous field contents are replaced.  The type code of the field is determined by calling val.typeCode().
     * @param name Name of the field to set
     * @param val The object whose bytes are to be flattened out and put into this field.
     * (val) will be flattened and the resulting bytes kept.  (val) does not become part of the Message object.
     */
   public void setFlat(String name, Flattenable val) 
   {
      int type = val.typeCode();
      MessageField field = getCreateOrReplaceField(name, type);
      Object payload = field.getData();
      switch(type)
      {
         // For these types, we have explicit support for holding the objects in memory, so we'll just clone them
         case B_MESSAGE_TYPE:  
         {
            Message array[] = ((payload != null)&&(((Message[])payload).length == 1)) ? ((Message[])payload) : new Message[1];
            array[1] = (Message) val.cloneFlat();
            field.setPayload(array, 1);
         }
         break;
         
         case B_POINT_TYPE:  
         {
            Point array[] = ((payload != null)&&(((Point[])payload).length == 1)) ? ((Point[])payload) : new Point[1];
            array[1] = (Point) val.cloneFlat();
            field.setPayload(array, 1);            
         }
         break;
         
         case B_RECT_TYPE: 
         {
            Rect array[] = ((payload != null)&&(((Rect[])payload).length == 1)) ? ((Rect[])payload) : new Rect[1];
            array[1] = (Rect) val.cloneFlat();
            field.setPayload(array, 1);            
         }
         break;         

         // For everything else, we have to store the objects as byte buffers
         default:
         {
            byte array[][] = ((payload != null)&&(((byte[][])payload).length == 1)) ? ((byte[][])payload) : new byte[1][];
            array[0] = flattenToArray(val, array[0]);
            field.setPayload(array, 1);
         }
         break;
      }
   }
      
   /** Sets the given field name to contain the given Flattenable values.  Any previous field contents are replaced.
    *  @param name Name of the field to set vlaues.
    *  @param val Array of Flattenable objects to assign to the field.  The objects are all flattened and
    *             the flattened data is put into the Message; the objects themselves do not become part of the message.
    *  Note that if the objects are Messages, Points, or Rects, they will be cloned rather than flattened.
    */
   public void setFlats(String name, Flattenable [] vals) 
   {
      int type = vals[0].typeCode();
      int len = vals.length;
      MessageField field = getCreateOrReplaceField(name, type);
      switch(type)
      {
         // For these types, we have explicit support for holding the objects in memory, so we'll just clone them
         case B_MESSAGE_TYPE:  
         {
            Message array[] = new Message[len];
            for (int i=0; i<len; i++) array[i] = (Message) vals[i].cloneFlat();
            field.setPayload(array, len);
         }
         break;
         
         case B_POINT_TYPE:  
         {
            Point array[] = new Point[len];
            for (int i=0; i<len; i++) array[i] = (Point) vals[i].cloneFlat();
            field.setPayload(array, len);
         }
         break;
         
         case B_RECT_TYPE: 
         {
            Rect array[] = new Rect[len];
            for (int i=0; i<len; i++) array[i] = (Rect) vals[i].cloneFlat();
            field.setPayload(array, len);
         }
         break;         

         default:
         {
            byte [][] array = (byte[][]) field.getData();
            if ((array == null)||(field.size() != len)) array = new byte[len][];
            for (int i=0; i<len; i++) array[i] = flattenToArray(vals[i], array[i]);
            field.setPayload(array, len);
         }
      }
   }
      
   /** Retrieves the first Flattenable value in the given field. 
    *  @param name Name of the field to look for a Flattenable value in.
    *  @param returnObject A Flattenable object that, on success, will be set to reflect the value held in this field.
    *                      This object will not be referenced by this Message.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name does not contain Flattenable objects or byte buffers.
    *  @throws UnflattenFormatException if the passed-in object can not be set or unflattened from the byte buffer.
    *  @throws ClassCastException if the passed-in object is the wrong type for the held Flattenable objects in the field to copy themselves to.
    */
   public void getFlat(String name, Flattenable returnObject) throws MessageException, UnflattenFormatException, ClassCastException 
   {
      MessageField field = getField(name);
      if (returnObject.allowsTypeCode(field.typeCode()))
      {
         Object o = field.getData();
              if (o instanceof byte[][]) unflattenFromArray(returnObject, ((byte[][])o)[0]);
         else if (o instanceof Message[]) returnObject.setEqualTo(((Message[])o)[0]);
         else if (o instanceof Point[])   returnObject.setEqualTo(((Point[])o)[0]);
         else if (o instanceof Rect[])    returnObject.setEqualTo(((Rect[])o)[0]);      
         else throw new FieldTypeMismatchException(name + " isn't a flattened-data field");
      }
      else throw new FieldTypeMismatchException("Passed-in object doesn't like typeCode " + whatString(field.typeCode()));
   }

   /** Retrieves the contents of the given field as an array of Flattenable values. 
    *  @param name Name of the field to look for Flattenable values in.
    *  @param returnObjects Should be an array of pre-allocated Flattenable objects of the correct type.
    *                       On success, this array's objects will be set to the proper states as determined by the held data in
    *                       this Message.  All the objects should be of the same type.  This method will unflatten as
    *                       many objects as exist or can fit in the array.  These objects will not be referenced by this Message.
    *  @return The number of objects in (returnObjects) that were actually unflattened.  May be less than (returnObjects.length).
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name does not contain Flattenable objects or byte buffers.
    *  @throws UnflattenFormatException If any of the held byte buffers or Flattenable objects cannot be copied or unflattened into
    *                                   the the objects in the passed-in Flattenable array   
    *  @throws ClassCastException if the passed-in object is the wrong type for the held Flattenable objects in the field to copy themselves to.
    */
   public int getFlats(String name, Flattenable [] returnObjects) throws MessageException, UnflattenFormatException, ClassCastException
   {
      MessageField field = getField(name);
      if (returnObjects[0].allowsTypeCode(field.typeCode()))
      {
         Object objs = field.getData();      
         int num;
         if (objs instanceof byte[][])
         {
            byte bufs[][] = (byte[][]) objs;
            num = (bufs.length < returnObjects.length) ? bufs.length : returnObjects.length;         
            for (int i=0; i<num; i++) unflattenFromArray(returnObjects[i], bufs[i]);
         }
         else if (objs instanceof Message[]) 
         {
            Message messages[] = (Message[]) objs;
            num = (messages.length < returnObjects.length) ? messages.length : returnObjects.length;
            for (int i=0; i<num; i++) returnObjects[i].setEqualTo(messages[i]);
         }
         else if (objs instanceof Point[])   
         {
            Point points[] = (Point[]) objs;
            num = (points.length < returnObjects.length) ? points.length : returnObjects.length;
            for (int i=0; i<num; i++) returnObjects[i].setEqualTo(points[i]);
         }
         else if (objs instanceof Rect[])    
         {
            Rect rects[] = (Rect[]) objs;
            num = (rects.length < returnObjects.length) ? rects.length : returnObjects.length;
            for (int i=0; i<num; i++) returnObjects[i].setEqualTo(rects[i]);
         }
         else throw new FieldTypeMismatchException(name + " wasn't an unflattenable data field");      
      
         return num;
      }
      else throw new FieldTypeMismatchException("Passed-in objects doen't like typeCode " + whatString(field.typeCode()));
   }
    
   /** Sets the given field name to contain a single byte buffer value.  Any previous field contents are replaced. 
     * @param name Name of the field to set
     * @param type The type code to give the field.  May not be a B_*_TYPE that contains non-byte-buffer data (e.g. B_STRING_TYPE or B_INT32_TYPE).
     * @param val Value that will become the sole value in the specified field.
     * @throws FieldTypeMismatchException if (type) is not a type code that can legally hold byte buffer data 
     */
   public void setByteBuffer(String name, int type, byte[] val) throws FieldTypeMismatchException 
   {
      checkByteBuffersOkay(type); 
      MessageField field = getCreateOrReplaceField(name, type);
      byte [][] array = (byte[][]) field.getData();
      if ((array == null)||(field.size() != 1)) array = new byte[1][];
      array[0] = val;
      field.setPayload(array, 1);
   }
   
   /** Sets the given field name to contain the given byte buffer values.  Any previous field contents are replaced.
     * @param name Name of the field to set vlaues.
     * @param type The type code to file the byte buffers under.  
     *             May not be any a B_*_TYPE that contains non-byte-buffer data (e.g. B_STRING_TYPE or B_INT32_TYPE).
     * @param vals Array of byte buffers to assign to the field.  Note that the neither the array nor the buffers it contains
     *             are copied; rather the all the passed-in data becomes part of the Message.
     * @throws FieldTypeMismatchException if (type) is not a type code that can legally hold byte buffer data 
     */
   public void setByteBuffers(String name, int type, byte [][] vals) throws FieldTypeMismatchException {checkByteBuffersOkay(type); setObjects(name, type, vals, vals.length);}

   /** Returns the first byte buffer value in the given field. 
    *  Note that the returned data is still held by this Message object.
    *  @param name Name of the field to look for a byte buffer value in.
    *  @return The first byte buffer value in the field.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name does not contain byte buffers.
    */
   public byte[] getBuffer(String name, int type) throws MessageException {return getBuffers(name,type)[0];}
   
   /** Returns the first byte buffer value in the given field.
    * Note that the returned data is still held by this Message object.
    * @param def The Default value to return if the field is not found or has the wrong type.
    * @param name Name of the field to look for a byte buffer value in.
    * @return The first byte buffer value in the field.
    */
   public byte[] getBuffer(String name, int type, byte[] def) {
      try {
         return getBuffer(name, type);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Returns the contents of the given field as an array of byte buffer values.
    *  @param name Name of the field to look for byte buffer values in.
    *  @return The array of byte buffer values associated with (name).  Note that the returned data is still held by this Message object.
    *  @throws FieldNotFoundException if a field with the given name does not exist.
    *  @throws FieldTypeMismatchException if the field with the given name does not contain byte buffers.
    */
   public byte[][] getBuffers(String name, int type) throws MessageException
   {
      Object ret = getData(name, type);
      if (ret instanceof byte[][]) return (byte[][])ret;
      throw new FieldTypeMismatchException();
   }

   /** Utility method to get the data of a field (any type acceptable) with must-be-there semantics */
   public Object getData(String name) throws FieldNotFoundException {return getField(name).getData();}
   
   /** Gets the data of a field, returns def if any exceptions occur. */
   public Object getData(String name, Object def) {
      try {
         return getData(name);
      } catch (MessageException me) {
         return def;
      }
   }
   
   /** Utility method to get the data of a field (of a given type, or B_ANY_TYPE) using standard must-be-there semantics */
   public Object getData(String name, int type) throws FieldNotFoundException, FieldTypeMismatchException {return getField(name, type).getData();}

   /** Removes the specified field and its contents from the Message.  Has no effect if the field doesn't exist.
    *  @param name Name of the field to remove.
    */
   public void removeField(String name)
   {
      if (_fieldTable != null) _fieldTable.remove(name);
   }

   /** Clears all fields from the Message. */
   public void clear()
   {
      if (_fieldTable != null) _fieldTable.clear();
   }

   /** Returns true iff there is a field with the given name and type present in the Message
    *  @param fieldName the field name to look for.
    *  @param type the type to look for, or B_ANY_TYPE if type isn't important to you.
    *  @return true if such a field exists, else false. 
    */
   public boolean hasField(String fieldName, int type) {return (getFieldIfExists(fieldName, type) != null);}

   /** Returns true iff this Message contains a field named (fieldName).
    *  @param fieldName Name of the field to check for.
    *  @return true if the field exists, else false. 
    */
   public boolean hasField(String fieldName) {return hasField(fieldName, B_ANY_TYPE);}
   
   /** Returns the number of items present in the given field, or zero if the field isn't present or is of the wrong type.
    *  @param name Name of the field to check.
    *  @param type Type of field to limit query to, or B_ANY_TYPE if type doesn't matter
    *  @return The number of items in the field, or zero if the field doesn't exist or is the wrong type.
    */
   public int countItemsInField(String name, int type)
   {
      MessageField field = getFieldIfExists(name, type);
      return (field != null) ? field.size() : 0;
   }

   /** Returns the number of items present in the given field, or zero if the field isn't present
    *  @param name Name of the field to check.
    *  @return The number of items in the field, or zero if the field doesn't exist.
    */
   public int countItemsInField(String fieldName) {return countItemsInField(fieldName, B_ANY_TYPE);}

   /** Returns the B_*_TYPE type code of the given field. 
    *  @param name Name of the field to check.
    *  @throws FieldNotFoundException if no field with the given name exists. 
    */
   public int getFieldTypeCode(String name) throws FieldNotFoundException {return getField(name).typeCode();}

   /** Take the data under (name) in this message, and moves it into (moveTo). 
    *  Any data that was under (name) in (moveTo) will be replaced.
    *  @param name Name of an existing field to be moved.
    *  @param moveTo A Message to move the field into.
    *  @throws FieldNotFoundException if no field named (name) exists in this Message.
    */
   public void moveField(String name, Message moveTo) throws FieldNotFoundException
   {
      MessageField field = getField(name);
      _fieldTable.remove(name);
      moveTo.ensureFieldTableAllocated();
      moveTo._fieldTable.put(name, field);
   }
   
   /**
    * Take the data under (name) in this message, and copies it into (moveTo). 
    * Any data that was under (name) in (moveTo) will be replaced.
    * @param name Name of an existing field to be copied.
    * @param copyTo A Message to copy the field into.
    * @throws FieldNotFound exception if a field named (name) does not exist in this Message. 
    */
   public void copyField(String name, Message copyTo) throws FieldNotFoundException
   {
      MessageField field = getField(name);
      copyTo.ensureFieldTableAllocated();
      copyTo._fieldTable.put(name, field.cloneFlat());
   }

   /** Flattens the given flattenable object into an array and returns it.  Pass in an old array to use if you like; it may be reused, or not. */
   private synchronized byte[] flattenToArray(Flattenable flat, byte[] optOldBuf)
   {
      try {
         int fs = flat.flattenedSize();
         if ((optOldBuf == null)||(optOldBuf.length != fs)) optOldBuf = new byte[fs];
      
         if (_paos == null)
         {
            _paos = new PreAllocatedByteArrayOutputStream();
            _lepaos = new LEDataOutputStream(_paos);
         }
         _paos.setOutputBuffer(optOldBuf);      
         flat.flatten(_lepaos);
         _paos.setOutputBuffer(null);   // allow gc
      }
      catch(IOException ex) {
         ex.printStackTrace();  // shouldn't ever happen
      }
      return optOldBuf;
   }
   
   /** Unflattens the given array into the given object.  Throws UnflattenFormatException on failure. */
   private synchronized void unflattenFromArray(Flattenable flat, byte [] buf) throws UnflattenFormatException
   {
      try {
         if (_pais == null)
         {
            _pais = new PreAllocatedByteArrayInputStream();
            _lepais = new LEDataInputStream(_pais);
         }
         _pais.setInputBuffer(buf);
         flat.unflatten(_lepais, buf.length);
         _pais.setInputBuffer(null);  // allow gc
      }
      catch(IOException ex) {
         throw new UnflattenFormatException();
      }
   }

   /** Convenience method:  when it returns, _fieldTable is guaranteed to be non-null. */
   private void ensureFieldTableAllocated()
   {
      if (_fieldTable == null) _fieldTable = new Hashtable();
   }

   /** Sets the contents of a field */
   private void setObjects(String name, int type, Object values, int numValues) {getCreateOrReplaceField(name, type).setPayload(values, numValues);}

   /** Utility method to get a field (if it exists), create it if it doesn't, or
    *  replace it if it exists but is the wrong type.
    *  @param name Name of the field to get or create
    *  @param type B_*_TYPE of the field to get or create.  B_ANY_TYPE should not be used.
    *  @return The (possibly new, possible pre-existing) MessageField object.
    */
   private MessageField getCreateOrReplaceField(String name, int type)
   {
      ensureFieldTableAllocated();
      MessageField field = (MessageField) _fieldTable.get(name);
      if (field != null)
      {
         if (field.typeCode() != type) 
         {
            _fieldTable.remove(name);
            return getCreateOrReplaceField(name, type);
         }
      }
      else 
      {
         field = new MessageField(type);
         _fieldTable.put(name, field);
      }
      return field;
   }
   
   /** Utility method to get a field (any type acceptable) with must-be-there semantics */
   private MessageField getField(String name) throws FieldNotFoundException
   {
      MessageField field;
      if ((_fieldTable != null)&&((field = (MessageField)_fieldTable.get(name)) != null)) return field;
      throw new FieldNotFoundException("Field " + name + " not found.");
   }
   
   /** Utility method to get a field (of a given type) using standard must-be-there semantics */
   private MessageField getField(String name, int type) throws FieldNotFoundException, FieldTypeMismatchException
   {
      MessageField field;
      if ((_fieldTable != null)&&((field = (MessageField)_fieldTable.get(name)) != null))
      {
         if ((type == B_ANY_TYPE)||(type == field.typeCode())) return field;
         throw new FieldTypeMismatchException("Field type mismatch in entry " + name);
      }
      else throw new FieldNotFoundException("Field " + name + " not found.");
   }

   /** Utility method to get a field using standard may-or-may-not-be-there semantics */
   private MessageField getFieldIfExists(String name, int type)
   {
      MessageField field;
      return ((_fieldTable != null)&&((field = (MessageField)_fieldTable.get(name)) != null)&&((type == B_ANY_TYPE)||(type == field.typeCode()))) ? field : null;
   }

   /** Throws an exception iff byte-buffers aren't allowed as type (type) */
   private void checkByteBuffersOkay(int type) throws FieldTypeMismatchException
   {
      switch(type)
      {
         case B_BOOL_TYPE: case B_DOUBLE_TYPE: case B_FLOAT_TYPE:
         case B_INT64_TYPE: case B_INT32_TYPE: case B_INT16_TYPE:
         case B_MESSAGE_TYPE: case B_POINT_TYPE:  case B_RECT_TYPE: case B_STRING_TYPE: 
            throw new FieldTypeMismatchException();

         default:
            /* do nothing */
         break;
      }
   }

   /** Represents a single array of like-typed objects */
   private final class MessageField implements Flattenable
   {
      public MessageField(int type) {_type = type;}
      public int size() {return _numItems;}
      public Object getData() {return _payload;}
      public boolean isFixedSize() {return false;}
      public int typeCode() {return _type;}

      /** Returns the number of bytes in a single flattened item, or 0 if our items are variable-size */
      public int flattenedItemSize()
      {
         switch(_type)
         {
            case B_BOOL_TYPE:  case B_INT8_TYPE:                       return 1;
            case B_INT16_TYPE:                                         return 2;
            case B_FLOAT_TYPE: case B_INT32_TYPE:                      return 4;
            case B_INT64_TYPE: case B_DOUBLE_TYPE: case B_POINT_TYPE:  return 8;
            case B_RECT_TYPE:                                          return 16;
            default:                                                   return 0;
         }
      }

      /** Returns the number of bytes the MessageField will take up when it is flattened. */
      public int flattenedSize()
      {
         int ret = 0;
         switch(_type)
         {
            case B_BOOL_TYPE:  case B_INT8_TYPE: case B_INT16_TYPE: case B_FLOAT_TYPE: case B_INT32_TYPE: 
            case B_INT64_TYPE: case B_DOUBLE_TYPE: case B_POINT_TYPE: case B_RECT_TYPE:                                       
               ret += _numItems * flattenedItemSize();
            break;

            case B_MESSAGE_TYPE:
            {
               // there is no number-of-items field for B_MESSAGE_TYPE (for historical reasons, sigh)
               Message array[] = (Message[]) _payload;
               for (int i=0; i<_numItems; i++) ret += 4 + array[i].flattenedSize();   // 4 for the size int
            }
            break;

            case B_STRING_TYPE:
            {
               ret += 4;  // for the number-of-items field
               String array[] = (String[]) _payload;
               try {
                  for (int i=0; i<_numItems; i++) ret += 4 + array[i].getBytes("UTF8").length + 1;   // 4 for the size int, 1 for the nul byte
               } catch (UnsupportedEncodingException uee) {
                  for (int i=0; i<_numItems; i++) ret += 4 + array[i].length() + 1;   // 4 for the size int, 1 for the nul byte
               }
            }
            break;

            default:
            {
               ret += 4;  // for the number-of-items field
               byte [][] array = (byte[][]) _payload;
               for (int i=0; i<_numItems; i++) ret += 4 + array[i].length;   // 4 for the size int
            }
            break;
         }
         return ret;
      }

      /** Unimplemented, throws a ClassCastException exception */
      public void setEqualTo(Flattenable setFromMe) throws ClassCastException
      {
         throw new ClassCastException("MessageField.setEqualTo() not supported");
      }

      /** Flattens our data into the given stream */
      public void flatten(DataOutput out) throws IOException
      {
         switch(typeCode())
         {
            case B_BOOL_TYPE:  
            { 
               boolean [] array = (boolean[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeByte(array[i] ? 1 : 0);
            }
            break;

            case B_INT8_TYPE:                       
            {
               byte [] array = (byte[]) _payload;
               out.write(array, 0, _numItems);  // wow, easy!
            }
            break;

            case B_INT16_TYPE:                                         
            {
               short [] array = (short[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeShort(array[i]);
            }
            break;

            case B_FLOAT_TYPE: 
            { 
               float [] array = (float[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeFloat(array[i]);
            }
            break;

            case B_INT32_TYPE:
            { 
               int [] array = (int[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeInt(array[i]);
            }
            break;

            case B_INT64_TYPE: 
            { 
               long [] array = (long[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeLong(array[i]);
            }
            break;

            case B_DOUBLE_TYPE: 
            { 
               double [] array = (double[]) _payload;
               for (int i=0; i<_numItems; i++) out.writeDouble(array[i]);
            }
            break;

            case B_POINT_TYPE:  
            { 
               Point [] array = (Point[]) _payload;
               for (int i=0; i<_numItems; i++) array[i].flatten(out);
            }
            break;

            case B_RECT_TYPE:                                          
            { 
               Rect [] array = (Rect[]) _payload;
               for (int i=0; i<_numItems; i++) array[i].flatten(out);
            }
            break;

            case B_MESSAGE_TYPE:
            {
               Message array[] = (Message[]) _payload;
               for (int i=0; i<_numItems; i++)
               {
                  out.writeInt(array[i].flattenedSize());
                  array[i].flatten(out);
               }
            }
            break;

            case B_STRING_TYPE:
            {
               String array[] = (String[]) _payload;
               out.writeInt(_numItems);
               for (int i=0; i<_numItems; i++) 
               {
                  try {
                     byte[] utf8Bytes = array[i].getBytes("UTF8");
                     out.writeInt(utf8Bytes.length + 1);
                     out.write(utf8Bytes);
                  } catch (UnsupportedEncodingException uee) {
                     out.writeInt(array[i].length() + 1);
                     byte[] arrayChars = array[i].getBytes();
				 out.write(arrayChars, 0, arrayChars.length);
                  }
                  out.writeByte(0);  // nul terminator
               }
            }
            break;

            default:
            {
               byte [][] array = (byte[][]) _payload;
               out.writeInt(_numItems);
               for (int i=0; i<_numItems; i++) 
               {
                  out.writeInt(array[i].length);
                  out.write(array[i]);
               }
            }
            break;
         }
      }

      /** Returns true iff (code) equals our type code */
      public boolean allowsTypeCode(int code) {return (code == typeCode());}

      /** Restores our state from the given stream.
        * Assumes that our _type is already set correctly.
        */
      public void unflatten(DataInput in, int numBytes) throws IOException, UnflattenFormatException
      {
         // For fixed-size types, calculating the number of items to read is easy...
         int flattenedItemSize = flattenedItemSize();
         if (flattenedItemSize > 0) _numItems = numBytes / flattenedItemSize;

         switch(_type)
         {
            case B_BOOL_TYPE:  
            {
               boolean [] array = new boolean[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = (in.readByte() > 0) ? true : false;
               _payload = array;
            }
            break;

            case B_INT8_TYPE:             
            {
               byte [] array = new byte[_numItems];
               in.readFully(array);  // wow, easy
               _payload = array;
            }
            break;

            case B_INT16_TYPE:                                         
            { 
               short [] array = new short[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = in.readShort();
               _payload = array;
            }
            break;

            case B_FLOAT_TYPE: 
            { 
               float [] array = new float[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = in.readFloat();
               _payload = array;
            }
            break;

            case B_INT32_TYPE:
            { 
               int [] array = new int[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = in.readInt();
               _payload = array;
            }
            break;

            case B_INT64_TYPE: 
            { 
               long [] array = new long[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = in.readLong();
               _payload = array;
            }
            break;

            case B_DOUBLE_TYPE: 
            {
               double [] array = new double[_numItems];
               for (int i=0; i<_numItems; i++) array[i] = in.readDouble();
               _payload = array;
            }
            break;

            case B_POINT_TYPE:  
            { 
               Point [] array = new Point[_numItems];
               for (int i=0; i<_numItems; i++) 
               {
                  Point p = array[i] = new Point();
                  p.unflatten(in, p.flattenedSize());
               }
               _payload = array;
            }
            break;

            case B_RECT_TYPE:                                          
            {
               Rect [] array = new Rect[_numItems];
               for (int i=0; i<_numItems; i++) 
               {
                  Rect r = array[i] = new Rect();
                  r.unflatten(in, r.flattenedSize());
               }
               _payload = array;
            }
            break;

            case B_MESSAGE_TYPE:
            {
               Vector temp = new Vector();
               while(numBytes > 0)
               {
                  Message subMessage = new Message();
                  int subMessageSize = in.readInt(); 
                  subMessage.unflatten(in, subMessageSize);
                  temp.addElement(subMessage);
                  numBytes -= (subMessageSize + 4);  // 4 for the size int
               }
               _numItems = temp.size();
               Message array[] = new Message[_numItems];
               for (int j=0; j<_numItems; j++) array[j] = (Message) temp.elementAt(j);
               _payload = array;
            }
            break;

            case B_STRING_TYPE:
            {
               _numItems = in.readInt();
               String array[] = new String[_numItems];
               byte [] temp = null;  // lazy-allocated
               for (int i=0; i<_numItems; i++) 
               {
                  int nextStringLen = in.readInt();
                  if ((temp == null)||(temp.length < nextStringLen)) temp = new byte[nextStringLen];
                  in.readFully(temp, 0, nextStringLen);  // includes nul terminator
                  try {
                     array[i] = new String(temp, 0, nextStringLen-1, "UTF8");  // don't include the NUL byte
                  } catch (UnsupportedEncodingException uee) {
                     array[i] = new String(temp, 0, nextStringLen-1);  // don't include the NUL byte
                  }
               }
               _payload = array;
            }
            break;

            default:
            {
               _numItems = in.readInt();
               byte [][] array = new byte[_numItems][];
               for (int i=0; i<_numItems; i++) 
               {
                  array[i] = new byte[in.readInt()];
                  in.readFully(array[i]);
               }
               _payload = array;
            }
            break;
         }
      }

      /** Prints some debug info about our state to (out) */
      public String toString()
      {
         String ret = "  Type='"+whatString(_type)+"', " + _numItems + " items: ";
         int pitems = (_numItems < 10) ? _numItems : 10;
         switch(_type)
         {
            case B_BOOL_TYPE:  
            {
               boolean [] array = (boolean[]) _payload;
               for (int i=0; i<pitems; i++) ret += ((array[i]) ? "true " : "false ");
            }
            break;

            case B_INT8_TYPE:                       
            {
               byte [] array = (byte[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_INT16_TYPE:                                         
            { 
               short [] array = (short[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_FLOAT_TYPE: 
            { 
               float [] array = (float[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_INT32_TYPE:
            { 
               int [] array = (int[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_INT64_TYPE: 
            { 
               long [] array = (long[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_DOUBLE_TYPE: 
            {
               double [] array = (double[]) _payload;
               for (int i=0; i<pitems; i++) ret += (array[i] + " ");
            }
            break;

            case B_POINT_TYPE:  
            { 
               Point [] array = (Point[]) _payload;
               for (int i=0; i<pitems; i++) ret += array[i];
            }
            break;

            case B_RECT_TYPE:                                          
            {
               Rect [] array = (Rect[]) _payload;
               for (int i=0; i<pitems; i++) ret += array[i];
            }
            break;

            case B_MESSAGE_TYPE:
            {
               Message [] array = (Message[]) _payload;
               for (int i=0; i<pitems; i++) ret += ("["+whatString(array[i].what)+", "+array[i].countFields()+" fields] ");
            }
            break;

            case B_STRING_TYPE:
            {
               String [] array = (String[]) _payload;
               for (int i=0; i<pitems; i++) ret += ("[" + array[i] + "] ");
            }
            break;

            default:
            {
               byte [][] array = (byte[][]) _payload;
               for (int i=0; i<pitems; i++) ret += ("["+array[i].length+" bytes] ");
            }
            break;
         }
         return ret;
      }

      /** Makes an independent clone of this field object (a bit expensive) */
      public Flattenable cloneFlat()
      {
         MessageField clone = new MessageField(_type);
         Object newArray;  // this will be a copy of our data array
         switch(_type)
         {
            case B_BOOL_TYPE:    newArray = new boolean[_numItems]; break;            
            case B_INT8_TYPE:    newArray = new byte[_numItems];    break;
            case B_INT16_TYPE:   newArray = new short[_numItems];   break;
            case B_FLOAT_TYPE:   newArray = new float[_numItems];   break;
            case B_INT32_TYPE:   newArray = new int[_numItems];     break;
            case B_INT64_TYPE:   newArray = new long[_numItems];    break;
            case B_DOUBLE_TYPE:  newArray = new double[_numItems];  break;
            case B_STRING_TYPE:  newArray = new String[_numItems];  break;
            case B_POINT_TYPE:   newArray = new Point[_numItems];   break;
            case B_RECT_TYPE:    newArray = new Rect[_numItems];    break;
            case B_MESSAGE_TYPE: newArray = new Message[_numItems]; break;
            default:             newArray = new byte[_numItems][];  break;
         }
         System.arraycopy(_payload, 0, newArray, 0, _numItems);
         
         // If the contents of newArray are modifiable, we need to clone the contents also
         switch(_type)
         {
            case B_POINT_TYPE: 
            {
               Point points[] = (Point[]) newArray;
               for (int i=0; i<_numItems; i++) points[i] = (Point) points[i].cloneFlat();
            }
            break;
            
            case B_RECT_TYPE: 
            {
               Rect rects[] = (Rect[]) newArray;
               for (int i=0; i<_numItems; i++) rects[i] = (Rect) rects[i].cloneFlat();               
            }
            break;
            
            case B_MESSAGE_TYPE:
            {
               Message messages[] = (Message[]) newArray;
               for (int i=0; i<_numItems; i++) messages[i] = (Message) messages[i].cloneFlat();
            }
            break;
            
            default:            
            {
               if (newArray instanceof byte[][])
               {
                  // Clone the byte arrays, since they are modifiable
                  byte [][] array = (byte[][]) newArray;
                  for (int i=0; i<_numItems; i++) 
                  {
                     byte [] newBuf = new byte[array[i].length];                     
                     System.arraycopy(array[i], 0, newBuf, 0, array[i].length);
                     array[i] = newBuf;
                  }
               }
            }
            break;
         }
         clone.setPayload(newArray, _numItems);
         return clone;
      }

      /** Sets our payload and numItems fields. */
      public void setPayload(Object payload, int numItems)
      {
         _payload = payload;
         _numItems = numItems;
      }

      private int _type;
      private int _numItems;
      private Object _payload;
   }

   /** Used for more efficient unflattening of Flattenables from byte arrays */
   private final class PreAllocatedByteArrayInputStream extends InputStream
   {
      public int available() throws IOException {return (_buf != null) ? (_buf.length - _bytesRead) : 0;}      
      public void close() throws IOException {setInputBuffer(null);}
      public void mark(int readLimit) {_mark = _bytesRead;}
      public boolean markSupported() {return true;}
      public int read() throws IOException {return ((_buf != null)&&(_bytesRead < _buf.length)) ? _buf[_bytesRead++] : -1;}
      public int read(byte[] b, int offset, int len) throws IOException
      {
         int numLeft = _buf.length - _bytesRead;
         if (numLeft > 0)
         {
            int numToCopy = (len < numLeft) ? len : numLeft;
            System.arraycopy(_buf, _bytesRead, b, offset, numToCopy);
            _bytesRead += numToCopy;
            return numToCopy;
         }
         else return -1;
      }
      public synchronized void reset() throws IOException {_bytesRead = _mark;}
      public long skip(long n) throws IOException 
      {
         if (_buf != null)
         {
            int numLeft = _buf.length - _bytesRead;
            if (numLeft >= n)
            {                
               _bytesRead += n;
               return n;
            }
            else
            {
               _bytesRead += numLeft;
               return numLeft;
            }
         }
         else return 0;
      }
 
      public void setInputBuffer(byte [] buf)
      {
         _buf = buf;
         _bytesRead = 0;
         _mark = 0;
      }
      
      private byte [] _buf = null;
      private int _mark = 0;
      private int _bytesRead = 0;
   }

   /** Used for more efficient flattening of Flattenables to byte arrays */
   private class PreAllocatedByteArrayOutputStream extends OutputStream
   {      
      public void close() throws IOException {_buf = null;}     
      public void write(byte[] b, int off, int len) throws IOException
      {
         if (_buf != null)
         {
            int numLeft = _buf.length - _bytesWritten;
            if (numLeft >= len)
            {
               System.arraycopy(b, off, _buf, _bytesWritten, len);
               _bytesWritten += len;
               return;
            }            
         }
         throw new IOException("no space left in buffer ");
      }

      public void write(int b) throws IOException
      {
         if ((_buf != null)&&(_bytesWritten < _buf.length)) _buf[_bytesWritten++] = (byte) b;
         else throw new IOException("no space left in buffer");
      }
      
      public void setOutputBuffer(byte [] buf) 
      {
         _buf = buf;
         _bytesWritten = 0;
      }
      
      private byte _buf[] = null;
      private int _bytesWritten = 0;
   }

   // These are all allocated the first time they are needed, and then kept in case they are needed again
   private PreAllocatedByteArrayInputStream  _pais = null;
   private PreAllocatedByteArrayOutputStream _paos = null;
   private LEDataInputStream _lepais = null;
   private LEDataOutputStream _lepaos = null;   

   private Hashtable _fieldTable = null;    // our name -> MessageField table
   private static Hashtable _empty; // = new Hashtable(0);  // had to change this to be done on-demand in the ctor, as IE was throwing fits :^(
}

