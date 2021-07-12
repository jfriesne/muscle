using muscle.support;

namespace muscle.message {
  using System;
  using System.Text;
  using System.IO;
  using System.Collections;
  using System.Diagnostics;
  using System.Collections.Specialized;

  /// <summary> 
  /// This class is sort of similar to Be's BMessage class.  When flattened,
  /// the resulting byte stream is compatible with the flattened
  /// buffers of MUSCLE's C++ Message class.
  /// It only acts as a serializable data container; it does not 
  /// include any threading capabilities.  
  /// </summary>

  public class Message : Flattenable
    {
      /// Oldest serialization protocol version parsable by this code's 
      /// unflatten() methods
      public const int 
	OLDEST_SUPPORTED_PROTOCOL_VERSION = 1347235888; // 'PM00'
      
      /// Newest serialization protocol version parsable by this code's 
      /// unflatten() methods, as well as the version of the protocol 
      /// produce by this code's flatten() methods.
      public const int CURRENT_PROTOCOL_VERSION = 1347235888; // 'PM00'
      
      /// 32 bit 'what' code, for quick identification of message types.  
      /// Set this however you like.
      public int what = 0;

      /// static constructor
      static Message() {
	_empty = new HybridDictionary();
      }
      
      /// Default Constructor.
      public Message() { }
      
      /// Constructor.
      /// <param name="w">
      /// The 'what' member variable will be set to the value you specify here
      /// </param>
      ///
      public Message(int w)
      {
	what = w;
      }
      
      /// Copy Constructor.
      /// <param name="copyMe">
      /// the Message to make us a (wholly independant) copy.
      /// </param>
      public Message(Message copyMe)
      {
	setEqualTo(copyMe);
      }
      
      /// Returns an independent copy of this Message
      public override Flattenable cloneFlat()
      {
	Message clone = new Message();  
	clone.setEqualTo(this); 
	return clone;
      }
      
      /// Sets this Message equal to (c)
      /// <param name="c">What to clone.</param>
      /// <exception cref="System.InvalidCastException"/>
      public override void setEqualTo(Flattenable c)
      {
	clear();
	Message copyMe = (Message)c;
	what = copyMe.what;
	IEnumerator fields = copyMe.fieldNames();
	while(fields.MoveNext())  {
	  copyMe.copyField((string)fields.Current, this);
	}
      }
      
      /// Returns an Enumeration of Strings that are the
      /// field names present in this Message
      public IEnumerator fieldNames()
      {
	return (_fieldTable != null) ? 
	  _fieldTable.Keys.GetEnumerator() : _empty.Keys.GetEnumerator();
      }
      
      /// Returns the given 'what' constant as a human readable 4-byte 
      /// string, e.g. "BOOL", "BYTE", etc.
      /// <param name="w">Any 32-bit value you would like to have turned 
      /// into a string</param>
      public static string whatString(int w)
      {
	byte [] temp = new byte[4];
	temp[0] = (byte)((w >> 24) & 0xFF);
	temp[1] = (byte)((w >> 16) & 0xFF);
	temp[2] = (byte)((w >> 8)  & 0xFF);
	temp[3] = (byte)((w >> 0)  & 0xFF);
	
	Decoder d = Encoding.UTF8.GetDecoder();

	int charArrayLen = d.GetCharCount(temp, 0, temp.Length);
	char [] charArray = new char[charArrayLen];
	  
	int charsDecoded = d.GetChars(temp, 0, temp.Length, charArray, 0);
	
	return new string(charArray, 0, charsDecoded - 1);
      }
      
      /// Returns the number of field names of the given type that 
      /// are present in the Message.
      /// <param name="type">
      /// The type of field to count, or B_ANY_TYPE to 
      /// count all field types.
      /// </param>
      /// <returns>The number of matching fields, or zero if there are 
      ///  no fields of the appropriate type.</returns>
      ///
      public int countFields(int type) 
      {
	if (_fieldTable == null) 
	  return 0;
	if (type == B_ANY_TYPE) 
	  return _fieldTable.Count;
	
	int count = 0;
	IEnumerator e = _fieldTable.Values.GetEnumerator();
	while(e.MoveNext()) {
	  MessageField field = (MessageField) e.Current;
	  if (field.typeCode() == type)
	    count++;
	}
	return count;
      }
      
      /// Returns the total number of fields in this Message.
      ///
      public int countFields() 
      {
	return countFields(B_ANY_TYPE);
      }
      
      /// Returns true iff there are no fields in this Message.
      ///
      public bool isEmpty() {return (countFields() == 0);}

      
      /// Returns a string that is a summary of the contents of this Message.
      /// Good for debugging.
      ///
      public override string ToString()
      {
	string ret = String.Format("Message: what='{0}' ({1}), countField={2} flattenedSize={3}\n", whatString(what), what, countFields(), flattenedSize());
	  
	IEnumerator e = this.fieldNames();
	
	while(e.MoveNext()) {
	  string fieldName = (string) e.Current;
	  ret += String.Format("   {0}: {1}\n", fieldName, (MessageField) _fieldTable[fieldName]);
	}           
	
	return ret;
      }
      
      /// Renames a field.
      /// If a field with this name already exists, it will be replaced.
      /// <param name="old_entry">Old field name to rename</param>
      /// <param name="new_entry">New field name to rename</param>
      /// <exception cref="FieldNotFoundException"/>
      ///
      public void renameField(string old_entry, string new_entry)
      {
	MessageField field = getField(old_entry); 
	_fieldTable.Remove(old_entry);
	_fieldTable.Add(new_entry, field);
      }
      
      /// <returns>false as messages can be of varying size.</returns>
      public override bool isFixedSize() {
	return false;
      }
      
      /// <returns>Returns B_MESSAGE_TYPE</returns>
      public override int typeCode() {
	return B_MESSAGE_TYPE;
      }
      
      /// Returns The number of bytes it would take to flatten this 
      /// Message into a byte buffer.
      public override int flattenedSize() 
      {
	// 4 bytes for the protocol revision #, 4 bytes for the 
	// number-of-entries field, 4 bytes for what code
	int sum = 4 + 4 + 4;  
	if (_fieldTable != null) {
	  IEnumerator e = fieldNames();
	  while(e.MoveNext()) {
	    string fieldName = (string) e.Current;
	    MessageField field = (MessageField) _fieldTable[fieldName];
	    
	    // 4 bytes for the name length, name data, 
	    // 4 bytes for entry type code, 
	    /// 4 bytes for entry data length, entry data
	    sum += 4 + (fieldName.Length + 1) + 4 + 4 + field.flattenedSize();
	  }
	}
	return sum;
      }
      
      /// Returns true iff (code) is B_MESSAGE_TYPE
      public override bool allowsTypeCode(int code) {
	return (code == B_MESSAGE_TYPE);
      }
      
      public override void flatten(BinaryWriter writer) 
      {
	// Format:  0. Protocol revision number 
	//             (4 bytes, always set to CURRENT_PROTOCOL_VERSION)
	//          1. 'what' code (4 bytes)
	//          2. Number of entries (4 bytes)
	//          3. Entry name length (4 bytes)
	//          4. Entry name string (flattened String)
	//          5. Entry type code (4 bytes)
	//          6. Entry data length (4 bytes)
	//          7. Entry data (n bytes)
	//          8. loop to 3 as necessary         
	writer.Write((int) CURRENT_PROTOCOL_VERSION);
	writer.Write((int) what);
	writer.Write((int) countFields());
	IEnumerator e = fieldNames();
	while(e.MoveNext()) {
	  string name = (string) e.Current;
	  MessageField field = (MessageField) _fieldTable[name];
	  
	  byte [] byteArray = Encoding.UTF8.GetBytes(name);
	  
	  writer.Write((int) (byteArray.Length + 1));
	  writer.Write(byteArray);
	  writer.Write((byte)0);  // terminating NUL byte
	  
	  writer.Write(field.typeCode());
	  writer.Write(field.flattenedSize());
	  field.flatten(writer);
	}
      }

      public override void unflatten(BinaryReader reader, int numBytes)
      {
	clear();
	int protocolVersion = reader.ReadInt32();
	
	if ((protocolVersion > CURRENT_PROTOCOL_VERSION)||
	    (protocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)) 
	  throw new UnflattenFormatException("Version mismatch error");
	
	what = reader.ReadInt32();
	int numEntries = reader.ReadInt32();
	
	if (numEntries > 0) 
	  ensureFieldTableAllocated();
	
	char [] charArray = null;
	
	Decoder d = Encoding.UTF8.GetDecoder();
	
	for (int i=0; i<numEntries; i++) {
	  int fieldNameLength = reader.ReadInt32();
	  byte [] byteArray = reader.ReadBytes(fieldNameLength);
	  
	  int charArrayLen = d.GetCharCount(byteArray, 0, fieldNameLength);
	  
	  if (charArray == null || charArray.Length < charArrayLen)
	    charArray = new char[charArrayLen];
	  
	  int charsDecoded = d.GetChars(byteArray, 0, fieldNameLength, 
					charArray, 0);
	  
	  string f = new string(charArray, 0, charsDecoded - 1);
	  MessageField field = 
	    getCreateOrReplaceField(f, reader.ReadInt32());

	  field.unflatten(reader, reader.ReadInt32());
	  
	  _fieldTable.Remove(f);
	  _fieldTable.Add(f, field);
	}
      }
      
      /// Sets the given field name to contain a single boolean value.  
      /// Any previous field contents are replaced. 
      /// <param name="Name"/>
      /// <param name="val"/>
      ///
      public void setBoolean(string name, bool val)
      {
	MessageField field = getCreateOrReplaceField(name, B_BOOL_TYPE);
	bool [] array = (bool[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new bool[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      /// Sets the given field name to contain the given boolean values.
      /// Any previous field contents are replaced. 
      /// Note that the array is not copied; rather the passed-in array 
      /// becomes part of the Message.
      /// <param name="name"/>
      /// <param name="val"/>
      public void setBooleans(string name, bool [] vals) 
      {
	setObjects(name, B_BOOL_TYPE, vals, vals.Length);
      }
      
      /// Returns the first boolean value in the given field. 
      /// <param name="name"/>
      /// <returns>The first boolean value in the field.</returns>
      /// <exception cref="FieldNotFoundException"/>
      /// <exception cref="FieldTypeMismatchException">if the field
      /// with the given name is not a B_BOOL_TYPE field</exception>
      ///
      public bool getBoolean(string name) 
      {
	return getBooleans(name)[0];
      }
      
      public bool getBoolean(string name, bool def) {
	try {
	  return getBoolean(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public bool[] getBooleans(string name) 
      {
	return (bool []) getData(name, B_BOOL_TYPE);
      }
      
      public void setByte(string name, byte val)
      {
	MessageField field = getCreateOrReplaceField(name, B_INT8_TYPE);
	byte [] array = (byte[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new byte[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setBytes(string name, byte [] vals) 
      {
	setObjects(name, B_INT8_TYPE, vals, vals.Length);
      }
      
      public byte getByte(string name)
      {
	return getBytes(name)[0];
      }
      
      public byte getByte(string name, byte def) {
	try {
	  return getByte(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public byte[] getBytes(string name)
      {
	return (byte[]) getData(name, B_INT8_TYPE);
      }
    
      public void setShort(string name, short val)
      {
	MessageField field = getCreateOrReplaceField(name, B_INT16_TYPE);
	short [] array = (short[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new short[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setShorts(string name, short [] vals) 
      {
	setObjects(name, B_INT16_TYPE, vals, vals.Length);
      }
      
      public short getShort(string name)
      {
	return getShorts(name)[0];
      }
      
      public short getShort(string name, short def) {
	try {
	  return getShort(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public short[] getShorts(string name) 
      {
	return (short[]) getData(name, B_INT16_TYPE);
      }
      
      public void setInt(string name, int val)
      {
	MessageField field = getCreateOrReplaceField(name, B_INT32_TYPE);
	int [] array = (int[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new int[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setInts(string name, int [] vals) 
      {
	setObjects(name, B_INT32_TYPE, vals, vals.Length);
      }
      
      public int getInt(string name)
      {
	return getInts(name)[0];
      }
      
      public int getInt(string name, int def) {
	try {
	  return getInt(name);
	} catch (MessageException) {
	  return def;
	}
      }
      
      public int[] getInts(string name)
      {
	return (int[]) getData(name, B_INT32_TYPE);
      }
      
      public int[] getInts(string name, int[] defs) {
	try {
	  return getInts(name);
	} 
	catch (MessageException) {
	  return defs;
	}
      }
      
      public void setLong(string name, long val)
      {
	MessageField field = getCreateOrReplaceField(name, B_INT64_TYPE);
	long [] array = (long[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new long[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setLongs(string name, long [] vals) {
	setObjects(name, B_INT64_TYPE, vals, vals.Length);
      }
      
      public long getLong(string name)
      {
	return getLongs(name)[0];
      }
      
      public long getLong(string name, long def) {
	try {
	  return getLong(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public long[] getLongs(string name) 
      {
	return (long[]) getData(name, B_INT64_TYPE);
      }
      
      public void setFloat(string name, float val)
      {
	MessageField field = getCreateOrReplaceField(name, B_FLOAT_TYPE);
	float [] array = (float[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new float[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setFloats(string name, float [] vals) {
	setObjects(name, B_FLOAT_TYPE, vals, vals.Length);
      }
      
      public float getFloat(string name) {
	return getFloats(name)[0];
      }
      
      public float getFloat(string name, float def) {
	try {
	  return getFloat(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public float[] getFloats(string name) 
      {
	return (float[]) getData(name, B_FLOAT_TYPE);
      }
      
      public void setDouble(string name, double val)
      {
	MessageField field = getCreateOrReplaceField(name, B_DOUBLE_TYPE);
	double [] array = (double[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new double[1];
	array[0] = val;
	field.setPayload(array, 1); 
      }
      
      public void setDoubles(string name, double [] vals) 
      {
	setObjects(name, B_DOUBLE_TYPE, vals, vals.Length);
      }
      
      public double getDouble(string name)
      {
	return getDoubles(name)[0];
      }
      
      public double getDouble(string name, double def) {
	try {
	  return getDouble(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public double[] getDoubles(string name)
      {
	return (double[]) getData(name, B_DOUBLE_TYPE);
      }
      
      public void setString(string name, string val) 
      {
	MessageField field = getCreateOrReplaceField(name, B_STRING_TYPE);
	string [] array = (string[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new string[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      public void setStrings(string name, string [] vals) 
      {
	setObjects(name, B_STRING_TYPE, vals, vals.Length);
      }
      
      public string getString(string name)
      {
	return getStrings(name)[0];
      }
      
      public string getString(string name, string def) {
	try {
	  return getString(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public string[] getStrings(string name)
      {
	return (string[]) getData(name, B_STRING_TYPE);
      }
      
      public string[] getStrings(string name, string[] def) {
	try {
	  return (string[]) getData(name, B_STRING_TYPE);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      /// Sets the given field name to contain a single Message value.  
      /// Any previous field contents are replaced. 
      /// Note that a copy of (val) is NOT made; the passed-in mesage 
      /// object becomes part of this Message.
      ///
      /// <param name="name"/>
      /// <param name="val/>
      public void setMessage(string name, Message val) 
      {
	MessageField field = getCreateOrReplaceField(name, B_MESSAGE_TYPE);
	Message [] array = (Message[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new Message[1];
	array[0] = val;
	field.setPayload(array, 1);      
      }
      
      /// Sets the given field name to contain a single Message value.  
      /// Any previous field contents are replaced. 
      /// Note that the neither the array nor the Message objects are copied;
      /// rather both the array and the Messages become part of this Message.
      /// <param name="name"/>
      /// <param name="val/>
      public void setMessages(string name, Message [] vals) {
	setObjects(name, B_MESSAGE_TYPE, vals, vals.Length);
      }
      
      public Message getMessage(string name)
      {
	return getMessages(name)[0];
      }
      
      public Message[] getMessages(string name)
      {
	return (Message[]) getData(name, B_MESSAGE_TYPE);
      }
      
      public void setPoint(string name, Point val) 
      {
	MessageField field = getCreateOrReplaceField(name, B_POINT_TYPE);
	Point [] array = (Point[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new Point[1];
	array[0] = val;
	field.setPayload(array, 1);      
      }
      
      public void setPoints(string name, Point [] vals) {
	setObjects(name, B_POINT_TYPE, vals, vals.Length);
      }
      
      public Point getPoint(string name)
      {
	return getPoints(name)[0];
      }
      
      public Point getPoint(string name, Point def) {
	try {
	  return getPoint(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public Point[] getPoints(string name)
      {
	return (Point[]) getData(name, B_POINT_TYPE);
      }
      
      public void setRect(string name, Rect val)
      {
	MessageField field = getCreateOrReplaceField(name, B_RECT_TYPE);
	Rect [] array = (Rect[]) field.getData();
	if ((array == null)||(field.size() != 1)) 
	  array = new Rect[1];
	array[0] = val;
	field.setPayload(array, 1);
      }
   
      public void setRects(string name, Rect [] vals) 
      {
	setObjects(name, B_RECT_TYPE, vals, vals.Length);
      }

      public Rect getRect(string name)
      {
	return getRects(name)[0];
      }
      
      public Rect getRect(string name, Rect def) {
	try {
	  return getRect(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      public Rect[] getRects(string name)
      {
	return (Rect[]) getData(name, B_RECT_TYPE);
      }
      
      /// Sets the given field name to contain the flattened bytes of 
      /// the single given Flattenable object. Any previous field contents
      /// are replaced.  The type code of the field is determined by 
      /// calling val.typeCode().
      /// (val) will be flattened and the resulting bytes kept.
      /// (val) does not become part of the Message object.
      /// <param name="name"/>
      /// <param name="val">
      /// the object whose bytes are to be flattened out and put into 
      /// this field.</param>

      public void setFlat(string name, Flattenable val) 
      {
	int type = val.typeCode();
	MessageField field = getCreateOrReplaceField(name, type);
	
	object payload = field.getData();
	
	switch(type) {
	  // For these types, we have explicit support for holding 
	  // the objects in memory, so we'll just clone them
	case B_MESSAGE_TYPE:  
	  {
	    Message [] array = 
	      ((payload != null)&&(((Message[])payload).Length == 1)) ? ((Message[])payload) : new Message[1];
	    
            array[1] = (Message) val.cloneFlat();
            field.setPayload(array, 1);
	  }
	  break;
	  
	case B_POINT_TYPE:  
	  {
            Point [] array = 
	      ((payload != null)&&(((Point[])payload).Length == 1)) ? 
	      ((Point[])payload) : new Point[1];
            array[1] = (Point) val.cloneFlat();
            field.setPayload(array, 1);            
	  }
	  break;
	  
	case B_RECT_TYPE: 
	  {
            Rect [] array = 
	      ((payload != null)&&(((Rect[])payload).Length == 1)) ? 
	      ((Rect[])payload) : new Rect[1];
            array[1] = (Rect) val.cloneFlat();
            field.setPayload(array, 1);            
	  }
	  break;         
	  
	  // For everything else, we have to store the objects as byte buffers
	default:
	  {
            byte [][] array = 
	      ((payload != null)&&(((byte[][])payload).Length == 1)) ? 
	      ((byte[][])payload) : new byte[1][];
            array[0] = flattenToArray(val, array[0]);
            field.setPayload(array, 1);
	  }
	  break;
	}
      }
      
      
      /// Sets the given field name to contain the given Flattenable values.
      /// Any previous field contents are replaced.
      /// Note that if the objects are Messages, Points, or Rects, 
      /// they will be cloned rather than flattened.
      /// <param name="name"/>
      /// <param name="val">
      /// Array of Flattenable objects to assign to the field.  
      /// The objects are all flattened and
      /// the flattened data is put into the Message; 
      /// the objects themselves do not become part of the message.
      /// </param>
      public void setFlats(string name, Flattenable [] vals) 
      {
	int type = vals[0].typeCode();
	int len = vals.Length;
	MessageField field = getCreateOrReplaceField(name, type);
	
	// For these types, we have explicit support for holding 
	// the objects in memory, so we'll just clone them
	switch(type) {
	case B_MESSAGE_TYPE:
	  {
	    Message [] array = new Message[len];
	    for (int i=0; i<len; i++) 
	      array[i] = (Message) vals[i].cloneFlat();
	    field.setPayload(array, len);
	  }
	  break;

	case B_POINT_TYPE:  
	  {
	    Point [] array = new Point[len];
	    for (int i=0; i<len; i++) 
	      array[i] = (Point) vals[i].cloneFlat();
	    field.setPayload(array, len);

	  }
	  break;	  
	  
	case B_RECT_TYPE: 
	  {
	    Rect [] array = new Rect[len];
	    for (int i=0; i<len; i++) 
	      array[i] = (Rect) vals[i].cloneFlat();
	    field.setPayload(array, len); 
	  }
	  break;
	    
	default:
	  {
	    byte [][] array = (byte[][]) field.getData();
	    if ((array == null)||(field.size() != len)) 
	      array = new byte[len][];
	    
	    for (int i=0; i<len; i++) array[i] = 
	      flattenToArray(vals[i], array[i]);
	    field.setPayload(array, len);
	  }
	  break;
	  
	}
      }
      
      /// Retrieves the first Flattenable value in the given field. 
      /// <param name="name"/>
      /// <param name="returnObject"> 
      /// A Flattenable object that, on success, will be set to reflect 
      /// the value held in this field. This object will not be referenced 
      /// by this Message.
      /// </param>
      public void getFlat(string name, Flattenable returnObject) 
      {
	MessageField field = getField(name);
	if (returnObject.allowsTypeCode(field.typeCode()))
	  {
	    object o = field.getData();
	    if (o is byte[][]) 
	      unflattenFromArray(returnObject, ((byte[][])o)[0]);
	    else if (o is Message[]) 
	      returnObject.setEqualTo(((Message[])o)[0]);
	    else if (o is Point[])
	      returnObject.setEqualTo(((Point[])o)[0]);
	    else if (o is Rect[])
	      returnObject.setEqualTo(((Rect[])o)[0]);      
	    else 
	      throw new FieldTypeMismatchException(name + " isn't a flattened-data field");
	  }
	else 
	  throw new FieldTypeMismatchException("Passed-in object doesn't like typeCode " + whatString(field.typeCode()));
      }
      
      /// Retrieves the contents of the given field as an array of 
      /// Flattenable values. 
      /// <param name="name">Name of the field to look for 
      /// </param> Flattenable values in.
      /// <param name="returnObjects">Should be an array of pre-allocated 
      /// Flattenable objects of the correct type.  On success, this 
      /// array's objects will be set to the proper states as determined by 
      /// the held data in this Message.  All the objects should be of the 
      /// same type.  This method will unflatten as many objects as exist or 
      /// can fit in the array.  These objects will not be referenced by 
      /// this Message.</param>
      /// <returns>The number of objects in (returnObjects) that were 
      /// actually unflattened.  May be less than (returnObjects.length).
      /// </returns>
      /// <exception cref="FieldNotFoundException"/>
      /// <exception cref="FieldTypeMismatchException"/>
      /// <exception cref="UnflattenFormatException"/>
      /// <exception cref="InvalidCastException"/>
      ///
      public int getFlats(string name, Flattenable [] returnObjects) 
      {
	MessageField field = getField(name);
	if (returnObjects[0].allowsTypeCode(field.typeCode()))
	  {
	    object objs = field.getData();      
	    int num;
	    if (objs is byte[][])
	      {
		byte [][] bufs = (byte[][]) objs;
		num = (bufs.Length < returnObjects.Length) ? bufs.Length : returnObjects.Length;         
		for (int i=0; i<num; i++) 
		  unflattenFromArray(returnObjects[i], bufs[i]);
	      }
	    else if (objs is Message[]) 
	      {
		Message [] messages = (Message[]) objs;
		num = (messages.Length < returnObjects.Length) ? messages.Length : returnObjects.Length;
		for (int i=0; i<num; i++) 
		  returnObjects[i].setEqualTo(messages[i]);
	      }
	    else if (objs is Point[])   
	      {
		Point [] points = (Point[]) objs;
		num = (points.Length < returnObjects.Length) ? points.Length : returnObjects.Length;
		for (int i=0; i<num; i++) 
		  returnObjects[i].setEqualTo(points[i]);
	      }
	    else if (objs is Rect[])    
	      {
		Rect [] rects = (Rect[]) objs;
		num = (rects.Length < returnObjects.Length) ? rects.Length : returnObjects.Length;
		for (int i=0; i<num; i++) 
		  returnObjects[i].setEqualTo(rects[i]);
	      }
	    else throw new FieldTypeMismatchException(name + " wasn't an unflattenable data field");      
	    
	    return num;
	  }
	else throw new FieldTypeMismatchException("Passed-in objects doen't like typeCode " + whatString(field.typeCode()));
      }
      
      /// Sets the given field name to contain a single byte buffer value.  Any previous field contents are replaced. 
      /// <param name="name"/>
      /// <param name="type">The type code to give the field.  
      /// May not be a B_*_TYPE that contains non-byte-buffer 
      /// data (e.g. B_STRING_TYPE or B_INT32_TYPE)
      /// </param>
      /// <param name="val">Value that will become the sole value in the 
      /// specified field.</param>
      /// <exception cref="FieldTypeMismatchException"/>
      ///
      public void setByteBuffer(string name, int type, byte[] val)
      {
	checkByteBuffersOkay(type); 
	MessageField field = getCreateOrReplaceField(name, type);
	byte [][] array = (byte[][]) field.getData();
	if ((array == null)||(field.size() != 1)) array = new byte[1][];
	array[0] = val;
	field.setPayload(array, 1);
      }
      
      /// Sets the given field name to contain the given byte buffer 
      /// values.  Any previous field contents are replaced.
      /// <param name="name"/>
      /// <param name="type">The type code to file the byte buffers under.  
      /// May not be any a B_*_TYPE that contains non-byte-buffer 
      /// data (e.g. B_STRING_TYPE or B_INT32_TYPE).</param>
      /// <param name="vals">Array of byte buffers to assign to the 
      /// field.  Note that the neither the array nor the buffers it 
      /// contains are copied; rather the all the passed-in data becomes 
      /// part of the Message.</param>
      /// <exception cref="FieldTypeMismatchException"/>
      ///
      public void setByteBuffers(string name, int type, byte [][] vals) 
      {
	checkByteBuffersOkay(type); 
	setObjects(name, type, vals, vals.Length);
      }
      
      /// Returns the first byte buffer value in the given field. 
      /// Note that the returned data is still held by this Message object.
      public byte[] getBuffer(string name, int type)
      {
	return getBuffers(name,type)[0];
      }
      
      /** Returns the first byte buffer value in the given field.
       * Note that the returned data is still held by this Message object.
       * @param def The Default value to return if the field is not found or has the wrong type.
       * @param name Name of the field to look for a byte buffer value in.
       * @return The first byte buffer value in the field.
       */
      public byte[] getBuffer(string name, int type, byte[] def) {
	try {
	  return getBuffer(name, type);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      /// Returns the contents of the given field as an array of byte 
      /// buffer values.
      public byte[][] getBuffers(string name, int type)
      {
	object ret = ((object[])getData(name, type))[0];
	if (ret is byte[][]) 
	  return (byte[][]) ret;
	throw new FieldTypeMismatchException();
      }
      
      //// Utility method to get the data of a field 
      /// (any type acceptable) with must-be-there semantics
      public object getData(string name)
      {
	return getField(name).getData();
      }
      
      /// Gets the data of a field, returns def if any exceptions occur.
      public object getData(string name, object def) {
	try {
	  return getData(name);
	} 
	catch (MessageException) {
	  return def;
	}
      }
      
      /// Utility method to get the data of a field 
      /// (of a given type, or B_ANY_TYPE) using standard must-be-there 
      /// semantics
      public object getData(string name, int type)
      {
	return getField(name, type).getData();
      }
      
      /// Removes the specified field and its contents from the Message.  
      /// Has no effect if the field doesn't exist.
      public void removeField(string name)
      {
	if (_fieldTable != null) 
	  _fieldTable.Remove(name);
      }
      
      /// Clears all fields from the Message. */
      public void clear()
      {
	if (_fieldTable != null) _fieldTable.Clear();
      }
      
      
      /// Returns true iff there is a field with the given name and 
      /// type present in the Message
      public bool hasField(string fieldName, int type) {
	return (getFieldIfExists(fieldName, type) != null);
      }
      
      /// Returns true iff this Message contains a field named (fieldName).
      public bool hasField(string fieldName) {
	return hasField(fieldName, B_ANY_TYPE);
      }
      
      /// Returns the number of items present in the given field, or 
      /// zero if the field isn't present or is of the wrong type.
      /// <exception cref="FieldNotFound"/>
      public int countItemsInField(string name, int type)
      {
	MessageField field = getFieldIfExists(name, type);
	return (field != null) ? field.size() : 0;
      }
      
      /// Returns the number of items present in the given field, 
      /// or zero if the field isn't present
      /// <exception cref="FieldNotFound"/>
      public int countItemsInField(string fieldName) {
	return countItemsInField(fieldName, B_ANY_TYPE);
      }
      
      /// Returns the B_*_TYPE type code of the given field. 
      /// <exception cref="FieldNotFound"/>
      public int getFieldTypeCode(string name) {
	return getField(name).typeCode();
      }
      
      /// Take the data under (name) in this message, and moves it 
      /// into (moveTo).
      /// <exception cref="FieldNotFound"/>
      public void moveField(string name, Message moveTo)
      {
	MessageField field = getField(name);
	_fieldTable.Remove(name);
	moveTo.ensureFieldTableAllocated();
	moveTo._fieldTable.Add(name, field);
      }
      
      /// Take the data under (name) in this message, and copies it 
      /// into (moveTo). 
      /// <exception cref="FieldNotFound"/>
      public void copyField(string name, Message copyTo)
      {
	MessageField field = getField(name);
	copyTo.ensureFieldTableAllocated();
	copyTo._fieldTable.Add(name, field.cloneFlat());
      }
      
      /// Flattens the given flattenable object into an array and 
      /// returns it.  Pass in an old array to use if you like; 
      /// it may be reused, or not. */
      private byte[] flattenToArray(Flattenable flat, byte[] optOldBuf)
      {
	int fs = flat.flattenedSize();
	if ((optOldBuf == null)||(optOldBuf.Length < fs)) 
	  optOldBuf = new byte[fs];
	
	using (MemoryStream memStream = new MemoryStream(optOldBuf)) {
	  BinaryWriter writer = new BinaryWriter(memStream);
	  flat.flatten(writer);
	}

	return optOldBuf;
      }
      
      /// Unflattens the given array into the given object.  
      /// <exception cref="UnflattenFormatException"/>
      ///
      private void unflattenFromArray(Flattenable flat, byte [] buf)
      {
	using (MemoryStream memStream = new MemoryStream(buf)) {
	  BinaryReader reader = new BinaryReader(memStream);
	  flat.unflatten(reader, buf.Length);
	}
      }
      
      /// Convenience method:  when it returns, _fieldTable is guaranteed 
      /// to be non-null.
      private void ensureFieldTableAllocated()
      {
	if (_fieldTable == null) {
	  _fieldTable = new HybridDictionary();
	}
      }
      
      /// Sets the contents of a field
      private void setObjects(string name, 
			      int type, 
			      System.Array values, 
			      int numValues) {
	getCreateOrReplaceField(name, type).setPayload(values, numValues);
      }
      
      /// Utility method to get a field (if it exists), create it if it 
      /// doesn't, or replace it if it exists but is the wrong type.
      /// <param name="name"/>
      /// <param name="type">B_*_TYPE of the field to get or create.  
      /// B_ANY_TYPE should not be used.</param>
      /// <returns>The (possibly new, possible pre-existing) 
      /// MessageField object.</returns>
      private MessageField getCreateOrReplaceField(string name, int type)
      {
	ensureFieldTableAllocated();
	MessageField field = (MessageField) _fieldTable[name];
	if (field != null) {
	  if (field.typeCode() != type) {
	    _fieldTable.Remove(name);
	    return getCreateOrReplaceField(name, type);
	  }
	}
	else {
	  field = new MessageField(type);
	  _fieldTable.Add(name, field);
	}
	return field;
      }
      
      /// Utility method to get a field (any type acceptable) with 
      /// must-be-there semantics
      private MessageField getField(string name)
      {
	MessageField field;
	if ((_fieldTable != null)&&((field = (MessageField)_fieldTable[name]) != null)) 
	  return field;
	throw new FieldNotFoundException("Field " + name + " not found.");
      }
      
      /// Utility method to get a field (of a given type) using standard 
      /// must-be-there semantics
      private MessageField getField(string name, int type)
      {
	MessageField field;
	if ((_fieldTable != null)&&((field = (MessageField)_fieldTable[name]) != null))
	  {
	    if ((type == B_ANY_TYPE)||(type == field.typeCode())) 
	      return field;
	    throw new FieldTypeMismatchException(String.Format("Field type mismatch in entry {0}. Requested type code: {1} but got type code {2}", name, type, field.typeCode()));
	  }
	else throw new FieldNotFoundException("Field " + name + " not found.");
      }
      
      /// Utility method to get a field using standard 
      /// may-or-may-not-be-there semantics
      private MessageField getFieldIfExists(string name, int type)
      {
	MessageField field;
	return ((_fieldTable != null)&&((field = (MessageField)_fieldTable[name]) != null)&&((type == B_ANY_TYPE)||(type == field.typeCode()))) ? field : null;
      }
      
      /// Throws an exception iff byte-buffers aren't allowed as type (type)
      private void checkByteBuffersOkay(int type)
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
      
      /// Represents a single array of like-typed objects
      private class MessageField : Flattenable
	{
	  public MessageField(int type) {_type = type;}
	  public int size() {return _numItems;}
	  public object getData() {return _payload;}
	  public override bool isFixedSize() {return false;}
	  public override int typeCode() {return _type;}
	  
	  /** Returns the number of bytes in a single flattened item, or 0 if our items are variable-size */
	  public int flattenedItemSize()
	  {
	    switch(_type) {
	    case B_BOOL_TYPE:
	    case B_INT8_TYPE:
	      return 1;
	    case B_INT16_TYPE:
	      return 2;
	    case B_FLOAT_TYPE:
	    case B_INT32_TYPE:
	      return 4;
	    case B_INT64_TYPE: 
	    case B_DOUBLE_TYPE: 
	    case B_POINT_TYPE:
	      return 8;
	    case B_RECT_TYPE:
	      return 16;
	    default:
	      return 0;
	    }
	  }
	  
	  /// Returns the number of bytes the MessageField will take 
	  /// up when it is flattened.
	  public override int flattenedSize()
	  {
	    int ret = 0;
	    switch(_type)
	      {
	      case B_BOOL_TYPE:
	      case B_INT8_TYPE:
	      case B_INT16_TYPE:
	      case B_FLOAT_TYPE:
	      case B_INT32_TYPE: 
	      case B_INT64_TYPE:
	      case B_DOUBLE_TYPE:
	      case B_POINT_TYPE:
	      case B_RECT_TYPE:                                       
		ret += _numItems * flattenedItemSize();
		break;
	      case B_MESSAGE_TYPE:
		{
		  // there is no number-of-items field for 
		  // B_MESSAGE_TYPE (for historical reasons, sigh)
		  Message [] array = (Message[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    ret += 4 + array[i].flattenedSize();
		  // 4 for the size int
		}
		break;
		
	      case B_STRING_TYPE:
		{
		  ret += 4;  // for the number-of-items field
		  string [] array = (string[]) _payload;
		  for (int i=0; i<_numItems; i++) {
		    ret += 4;
		    ret += Encoding.UTF8.GetByteCount(array[i]) + 1;
		    // 4 for the size int, 1 for the nul byte
		  }
		}
		break;
		
	      default:
		{
		  ret += 4;  // for the number-of-items field
		  byte [][] array = (byte[][]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    ret += 4 + array[i].Length;
		  // 4 for the size int
		}
		break;
	      }
	    return ret;
	  }
	  
	  /// Unimplemented, throws a ClassCastException exception
	  public override void setEqualTo(Flattenable setFromMe)
	  {
	    throw new System.InvalidCastException("MessageField.setEqualTo() not supported");
	  }
	  
	  
	  /// Flattens our data into the given stream
	  /// <exception cref="IOException"/>
	  ///
	  public override void flatten(BinaryWriter writer)
	  {
	    switch(typeCode())
	      {
	      case B_BOOL_TYPE:  
		{ 
		  bool [] array = (bool[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((byte) (array[i] ? 1 : 0));
		}
		break;
		
	      case B_INT8_TYPE:                       
		{
		  byte [] array = (byte[]) _payload;
		  writer.Write(array, 0, _numItems);  // wow, easy!
		}
		break;
		
	      case B_INT16_TYPE:                                  
		{
		  short [] array = (short[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((short) array[i]);
		}
		break;
		
	      case B_FLOAT_TYPE: 
		{ 
		  float [] array = (float[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((float) array[i]);
		}
		break;
		
	      case B_INT32_TYPE:
		{ 
		  int [] array = (int[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((int) array[i]);
		}
		break;
		
	      case B_INT64_TYPE: 
		{ 
		  long [] array = (long[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((long) array[i]);
		}
		break;
		
	      case B_DOUBLE_TYPE: 
		{ 
		  double [] array = (double[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    writer.Write((double) array[i]);
		}
		break;
		
	      case B_POINT_TYPE:  
		{ 
		  Point [] array = (Point[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    array[i].flatten(writer);
		}
		break;
		
	      case B_RECT_TYPE:                                          
		{ 
		  Rect [] array = (Rect[]) _payload;
		  for (int i=0; i<_numItems; i++) 
		    array[i].flatten(writer);
		}
		break;
		
	      case B_MESSAGE_TYPE:
		{
		  Message [] array = (Message[]) _payload;
		  for (int i=0; i<_numItems; i++) {
		    writer.Write((int) array[i].flattenedSize());
		    array[i].flatten(writer);
		  }
		}
		break;
		
	      case B_STRING_TYPE:
		{
		  string [] array = (string[]) _payload;
		  writer.Write((int) _numItems);
		  
		  for (int i=0; i<_numItems; i++) {
		      byte [] utf8Bytes = Encoding.UTF8.GetBytes(array[i]);
		      writer.Write(utf8Bytes.Length + 1);
		      writer.Write(utf8Bytes);
		      writer.Write((byte) 0);  // nul terminator
		  }
		}
		break;
		
	      default:
		{
		  byte [][] array = (byte[][]) _payload;
		  writer.Write((int) _numItems);
		  
		  for (int i=0; i<_numItems; i++) {
		    writer.Write(array[i].Length);
		    writer.Write(array[i]);
		  }
		}
		break;
	      }
	  }
	  
	  /// Returns true iff (code) equals our type code */
	  public override bool allowsTypeCode(int code) {
	    return (code == typeCode());
	  }
	  
	  /// Restores our state from the given stream.
	  /// Assumes that our _type is already set correctly.
	  /// <exception cref="IOException"/>
	  ///
	  public override void unflatten(BinaryReader reader, int numBytes)
	  {
	    // For fixed-size types, calculating the number of items 
	    // to read is easy...
	    int flattenedCount = flattenedItemSize();

	    if (flattenedCount > 0) 
	      _numItems = numBytes / flattenedCount;
	    
	    switch(_type)
	      {
	      case B_BOOL_TYPE:  
		{
		  bool [] array = new bool[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = (reader.ReadByte() > 0) ? true : false;
		  _payload = array;
		}
		break;
		
	      case B_INT8_TYPE:             
		{
		  byte [] array = reader.ReadBytes(_numItems);
		  _payload = array;
		}
		break;
		
	      case B_INT16_TYPE:                                         
		{ 
		  short [] array = new short[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = reader.ReadInt16();
		  _payload = array;
		}
		break;
		
	      case B_FLOAT_TYPE: 
		{ 
		  float [] array = new float[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = reader.ReadSingle();
		  _payload = array;
		}
		break;
		
	      case B_INT32_TYPE:
		{ 
		  int [] array = new int[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = reader.ReadInt32();
		  _payload = array;
		}
		break;
		
	      case B_INT64_TYPE: 
		{ 
		  long [] array = new long[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = reader.ReadInt64();
		  _payload = array;
		}
		break;
		
	      case B_DOUBLE_TYPE: 
		{
		  double [] array = new double[_numItems];
		  for (int i=0; i<_numItems; i++) 
		    array[i] = reader.ReadDouble();
		  _payload = array;
		}
		break;

	      case B_POINT_TYPE:  
		{ 
		  Point [] array = new Point[_numItems];
		  for (int i=0; i<_numItems; i++)  {
		    Point p = array[i] = new Point();
		    p.unflatten(reader, p.flattenedSize());
		  }
		  _payload = array;
		}
		break;
		
	      case B_RECT_TYPE:                                          
		{
		  Rect [] array = new Rect[_numItems];
		  for (int i=0; i<_numItems; i++)  {
		    Rect r = array[i] = new Rect();
		    r.unflatten(reader, r.flattenedSize());
		  }
		  _payload = array;
		}
		break;
		
	      case B_MESSAGE_TYPE:
		{
		  ArrayList temp = new ArrayList();
		  while(numBytes > 0) {
		    Message subMessage = new Message();
		    int subMessageSize = reader.ReadInt32(); 
		    subMessage.unflatten(reader, subMessageSize);
		    temp.Add(subMessage);
		    numBytes -= (subMessageSize + 4);  // 4 for the size int
		  }
		  _numItems = temp.Count;
		  Message [] array = new Message[_numItems];
		  for (int j=0; j<_numItems; j++) 
		    array[j] = (Message) temp[j];
		  _payload = array;
		}
		
		break;
		
	      case B_STRING_TYPE:
		{
		  Decoder d = Encoding.UTF8.GetDecoder();
		  
		  _numItems = reader.ReadInt32();
		  string [] array = new string[_numItems];

		  byte [] byteArray = null;
		  char [] charArray = null;
		  for (int i=0; i<_numItems; i++) {
		    int nextStringLen = reader.ReadInt32();
		    byteArray = reader.ReadBytes(nextStringLen);
		    
		    int charsRequired = d.GetCharCount(byteArray, 
						       0, 
						       nextStringLen);

		    if (charArray == null || charArray.Length < charsRequired)
		      charArray = new char[charsRequired];
		    
		    int charsDecoded = d.GetChars(byteArray, 
						  0, 
						  byteArray.Length, 
						  charArray, 
						  0);
		    
		    array[i] = new string(charArray, 0, charsDecoded - 1);
		  }
		  _payload = array;
		}
		break;
		
	      default:
		{
		  _numItems = reader.ReadInt32();
		  byte [][] array = new byte[_numItems][];
		  for (int i=0; i<_numItems; i++) {
		    int length = reader.ReadInt32();
		    array[i] = new byte[length];
		    array[i] = reader.ReadBytes(length);
		  }
		  _payload = array;
		}
		break;
	      }
	  }
	  
	  /// Prints some debug info about our state to (out)
	  public override string ToString()
	  {
	    string ret = "  Type='"+whatString(_type)+"', " 
	      + _numItems + " items: ";
	    int pitems = (_numItems < 10) ? _numItems : 10;
	    switch(_type)
	      {
	      case B_BOOL_TYPE:  
		{
		  byte [] array = (byte[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += ((array[i] != 0) ? "true " : "false ");
		}
		break;
		
	      case B_INT8_TYPE:                       
		{
		  byte [] array = (byte[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_INT16_TYPE:                                         
		{ 
		  short [] array = (short[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_FLOAT_TYPE: 
		{ 
		  float [] array = (float[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_INT32_TYPE:
		{ 
		  int [] array = (int[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_INT64_TYPE: 
		{ 
		  long [] array = (long[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_DOUBLE_TYPE: 
		{
		  double [] array = (double[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += (array[i] + " ");
		}
		break;
		
	      case B_POINT_TYPE:  
		{ 
		  Point [] array = (Point[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += array[i];
		}
		break;
		
	      case B_RECT_TYPE:                                          
		{
		  Rect [] array = (Rect[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += array[i];
		}
		break;
		
	      case B_MESSAGE_TYPE:
		{
		  Message [] array = (Message[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += ("["+whatString(array[i].what)+", "
			    + array[i].countFields()+" fields] ");
		}
		break;
		
	      case B_STRING_TYPE:
		{
		  string [] array = (string[]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += ("[" + array[i] + "] ");
		}
		break;
		
	      default:
		{
		  byte [][] array = (byte[][]) _payload;
		  for (int i=0; i<pitems; i++) 
		    ret += ("["+array[i].Length+" bytes] ");
		}
		break;
	      }
	    return ret;
	  }
	  
	  /// Makes an independent clone of this field object 
	  /// (a bit expensive)
	  public override Flattenable cloneFlat()
	  {
	    MessageField clone = new MessageField(_type);
	    System.Array newArray;  // this will be a copy of our data array
	    switch(_type)
	      {
	      case B_BOOL_TYPE:    newArray = new bool[_numItems]; break;            
	      case B_INT8_TYPE:    newArray = new byte[_numItems];    break;
	      case B_INT16_TYPE:   newArray = new short[_numItems];   break;
	      case B_FLOAT_TYPE:   newArray = new float[_numItems];   break;
	      case B_INT32_TYPE:   newArray = new int[_numItems];     break;
	      case B_INT64_TYPE:   newArray = new long[_numItems];    break;
	      case B_DOUBLE_TYPE:  newArray = new double[_numItems];  break;
	      case B_STRING_TYPE:  newArray = new string[_numItems];  break;
	      case B_POINT_TYPE:   newArray = new Point[_numItems];   break;
	      case B_RECT_TYPE:    newArray = new Rect[_numItems];    break;
	      case B_MESSAGE_TYPE: newArray = new Message[_numItems]; break;
	      default:             newArray = new byte[_numItems][];  break;
	      }
	    
	    newArray = (System.Array) _payload.Clone();
	    
	    // If the contents of newArray are modifiable, we need to 
	    // clone the contents also
	    switch(_type)
	      {
	      case B_POINT_TYPE: 
		{
		  Point [] points = (Point[]) newArray;
		  for (int i=0; i<_numItems; i++) 
		    points[i] = (Point) points[i].cloneFlat();
		}
		break;
		
	      case B_RECT_TYPE: 
		{
		  Rect [] rects = (Rect[]) newArray;
		  for (int i=0; i<_numItems; i++) 
		    rects[i] = (Rect) rects[i].cloneFlat();               
		}
		break;
		
	      case B_MESSAGE_TYPE:
		{
		  Message [] messages = (Message[]) newArray;
		  for (int i=0; i<_numItems; i++) 
		    messages[i] = (Message) messages[i].cloneFlat();
		}
		break;
		
	      default:            
		{
		  if (newArray is byte[][])
		    {
		      // Clone the byte arrays, since they are modifiable
		      byte [][] array = (byte[][]) newArray;
		      for (int i=0; i<_numItems; i++) 
			{
			  byte [] newBuf = (byte []) array[i].Clone();
			  array[i] = newBuf;
			}
		    }
		}
		break;
	      }
	    
	    clone.setPayload(newArray, _numItems);
	    return clone;
	  }
	  
	  /// Sets our payload and numItems fields.
	  public void setPayload(System.Array payload, int numItems)
	  {
	    _payload = payload;
	    _numItems = numItems;
	  }
	  
	  private int _type;
	  private int _numItems;
	  private System.Array _payload;
	}
      
      private IDictionary _fieldTable = null;
      private static IDictionary _empty = null;
    }
}


