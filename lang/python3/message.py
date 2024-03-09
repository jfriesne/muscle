"""
Python implementation of the MUSCLE Message class.
Messages Flatten()'d by this class produce data buffers that are compatible with the MUSCLE standard.
"""

__author__    = "Jeremy Friesner (jaf@meyersound.com)"
__version__   = "$Revision: 1.18 $"
__date__      = "$Date: 2005/01/01 01:03:21 $"
__copyright__ = "Copyright (c) 2000-2013 Meyer Sound Laboratories Inc"
__license__   = "See enclosed LICENSE.TXT file"

import struct
import types
import array
import sys
import io

# Standard MUSCLE field type-constants
B_ANY_TYPE     = 1095653716 # 'ANYT' : wild card
B_BOOL_TYPE    = 1112493900 # 'BOOL'
B_DOUBLE_TYPE  = 1145195589 # 'DBLE'
B_FLOAT_TYPE   = 1179406164 # 'FLOT'
B_INT64_TYPE   = 1280069191 # 'LLNG'
B_INT32_TYPE   = 1280265799 # 'LONG'
B_INT16_TYPE   = 1397248596 # 'SHRT'
B_INT8_TYPE    = 1113150533 # 'BYTE'
B_MESSAGE_TYPE = 1297303367 # 'MSGG'
B_POINTER_TYPE = 1347310674 # 'PNTR'
B_POINT_TYPE   = 1112559188 # 'BPNT'
B_RECT_TYPE    = 1380270932 # 'RECT'
B_STRING_TYPE  = 1129534546 # 'CSTR'
B_OBJECT_TYPE  = 1330664530 # 'OPTR' : used for flattened objects
B_RAW_TYPE     = 1380013908 # 'RAWT' : used for raw byte arrays
B_BITCHORD_TYPE= 1112818504 # 'BTCH' : BitChord type

CURRENT_PROTOCOL_VERSION = 1347235888 # 'PM00' -- our magic number

_dataNeedsSwap = (sys.byteorder != 'little')  # MUSCLE's serialized-bytes-protocol expected little-endian data

def GetHumanReadableTypeString(t):
   """Given a B_*_TYPE value, returns a human-readable string showing its bytes"""
   ret = ""
   for dummy in range(4):
      nextByte = t%256
      ret = chr(nextByte) + ret
      t = t//256
   return ret

class Message:
   """Python implementation of the MUSCLE Message data-storage class.

   This class can hold various types of data, and be flattened out into a
   buffer of bytes that is compatible with the MUSCLE flattened-data standard.
   """
   def __init__(self, what=0):
      self.what   = what
      self.__fields = {}

   def Clear(self):
      """Removes all fields from this Message and sets the what code to zero"""
      self.what = 0
      self.__fields = {}

   def GetFieldNames(self):
      """Returns the list of field names currently in this Message"""
      return list(self.__fields.keys())

   def HasFieldName(self, fieldName, fieldTypeCode=B_ANY_TYPE):
      """Returns 1 if the given fieldName exists and is of the given type, or 0 otherwise."""
      if self.GetFieldContents(fieldName, fieldTypeCode) is not None:
         return 1
      else:
         return 0

   def PutFieldContents(self, fieldName, fieldTypeCode, fieldContents):
      """Adds (or replaces) a new field into this Message.

      (fieldName) should be a string representing the unique name of the field.
      (fieldTypeCode) should be a numeric type code for the field (usually B_*_TYPE).
      (fieldContents) should be the field's contents (either an item or a list or array of items)
      Returns None.
      """
      if isinstance(fieldContents, (list, array.array)):
         self.__fields[fieldName] = (fieldTypeCode, fieldContents)
      else:
         self.__fields[fieldName] = (fieldTypeCode, [fieldContents])

   def GetFieldContents(self, fieldName, fieldTypeCode=B_ANY_TYPE, defaultValue=None):
      """Returns the contents of an existing field in this message (if any).

      (fieldName) should be a string representing the unique name of the field.
      (fieldTypeCode) may specify the only type code we want.  If not specified,
                      the default value, B_ANY_TYPE, means that any field found
                      under the given field name will be returned, regardless of type.
      (defaultValue) is the value that should be returned if the requested data is
                     not available.  Default value of this parameter is None.
      Returns the contents of the field, or None if the field doesn't exist or
      is of the wrong type.
      """
      if fieldName in self.__fields:
         ret = self.__fields[fieldName]
         if fieldTypeCode != B_ANY_TYPE and fieldTypeCode != ret[0]:
            return defaultValue # oops!  typecode mismatch
         else:
            return ret[1]       # return just the data portion
      else:
         return defaultValue

   def GetFieldItem(self, fieldName, fieldTypeCode=B_ANY_TYPE, defaultValue=None, index=0):
      """Returns the (index)'th item of an existing field in this message (if any).

      (fieldName) should be a string representing the unique name of the field.
      (fieldTypeCode) may specify the only type code we want.  If not specified,
                      the default value, B_ANY_TYPE, means that any field found
                      under the given field name will be returned, regardless of type.
      (defaultValue) what should be returned if the requested item does not exist.
                     Defaults to None.
      (index) specifies which item from the given field's items-list we want.
              Defaults to zero (the first item in the list).  If negative, this
              value indicates an item via its position relative to the end of the list.
      Returns the (index)'th item of the given field's contents, or (defaultValue) if
      the field doesn't exist or is of the wrong type, or if the index isn't
      a valid one for that list.
      """
      fc = self.GetFieldContents(fieldName, fieldTypeCode)
      if fc is not None:
         num = len(fc)
         if index < -num or index >= num:
            return defaultValue
         else:
            return fc[index]
      else:
         return defaultValue

   def GetFieldType(self, fieldName):
      """Returns the type code of the given field, or None if the field doesn't exist.

      (fieldName) should be a string representing the unique name of the field.
      """
      if fieldName in self.__fields:
         return self.__fields[fieldName][0]
      else:
         return None

   def PrintToStream(self, maxRecurseLevel = 2147483648, linePrefix = ""):
      """Dumps our state to stdout.  Handy for debugging purposes"""
      print(self.ToString(maxRecurseLevel, linePrefix))

   def __str__(self):
      return self.ToString()

   def ToString(self, maxRecurseLevel = 2147483648, linePrefix = ""):
      """Returns our state as a string, up to the specified recursion level."""
      s = "%sMessage: what=%i FlattenedSize=%i\n" % (linePrefix, self.what, self.FlattenedSize())
      for x in list(self.__fields.keys()):
         ft = self.GetFieldType(x)
         s = s + "%s [ %s : %s : %s ]\n" % (linePrefix, x, GetHumanReadableTypeString(ft), self.GetFieldContents(x))
         if (ft == B_MESSAGE_TYPE) and (maxRecurseLevel > 0):
            subMsgs = self.GetMessages(x)
            subPrefix = linePrefix + "   "
            for sm in subMsgs:
               s = s + sm.ToString(maxRecurseLevel-1, subPrefix)
      return s

   def FlattenedSize(self):
      """Returns the number of bytes that this Message would take up if flattened"""

      # Header is 12 bytes: protocol_version(4) + num_entries(4) + what(4)
      ret = 3*4

      # Now calculate the space for each field
      fields = self.GetFieldNames()
      for fieldName in fields:
         ret += 4+len(fieldName)+1+4+4  # namelen(4), name(n), NUL(1), type(4), fieldBytes(4)
         fieldContents = self.GetFieldContents(fieldName)
         fieldType     = self.GetFieldType(fieldName)
         ret += self.GetFieldContentsLength(fieldType, fieldContents)
      return ret

   def Unflatten(self, inFile):
      """Reads in the new contents of this Message from the given file object."""
      global _dataNeedsSwap
      self.Clear()
      protocolVersion, self.what, numFields = struct.unpack("<3L", inFile.read(3*4))
      if protocolVersion != CURRENT_PROTOCOL_VERSION:
         raise IOError("Bad flattened-Message Protocol version ").with_traceback(protocolVersion)
      for dummy in range(numFields):
         fieldName = inFile.read(struct.unpack("<L", inFile.read(4))[0]-1).decode()
         inFile.read(1)  # throw away the NUL terminator byte, we don't need it
         fieldTypeCode, fieldDataLength = struct.unpack("<2L", inFile.read(2*4))
         if fieldTypeCode == B_BOOL_TYPE:
            fieldContents = array.array('b')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_DOUBLE_TYPE:
            fieldContents = array.array('d')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_FLOAT_TYPE:
            fieldContents = array.array('f')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_INT32_TYPE:
            fieldContents = array.array('i')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_INT16_TYPE:
            fieldContents = array.array('h')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_INT8_TYPE:
            fieldContents = array.array('b')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_INT64_TYPE:
            fieldContents = array.array('q')
            fieldContents.frombytes(inFile.read(fieldDataLength))
         elif fieldTypeCode == B_MESSAGE_TYPE:
            fieldContents = []
            while fieldDataLength > 0:
               subMessageLength = struct.unpack("<L", inFile.read(4))[0]
               subMsg = Message()
               subMsg.Unflatten(inFile)
               fieldContents.append(subMsg)
               fieldDataLength = fieldDataLength-(subMessageLength+4)
         elif fieldTypeCode == B_POINT_TYPE:
            fieldContents = []
            for dummy in range(fieldDataLength//8):
               fieldContents.append(struct.unpack("<2f", inFile.read(8)))
         elif fieldTypeCode == B_RECT_TYPE:
            fieldContents = []
            for dummy in range(fieldDataLength//16):
               fieldContents.append(struct.unpack("<4f", inFile.read(16)))
         elif fieldTypeCode == B_STRING_TYPE:
            fieldContents = []
            numItems = struct.unpack("<L", inFile.read(4))[0]
            for dummy in range(numItems):
               fieldContents.append(inFile.read(struct.unpack("<L", inFile.read(4))[0]-1).decode())
               inFile.read(1)  # throw away the NUL byte, we don't need it
         else:
            fieldContents = []
            numItems = struct.unpack("<L", inFile.read(4))[0]
            for dummy in range(numItems):
               fieldContents.append(inFile.read(struct.unpack("<L", inFile.read(4))[0]))

         if _dataNeedsSwap and isinstance(fieldContents, array.array):
            fieldContents.byteswap()

         self.PutFieldContents(fieldName, fieldTypeCode, fieldContents)

   def Flatten(self, outFile):
      """Writes the state of this Message out to the given file object, in the standard platform-neutral flattened represenation."""
      global _dataNeedsSwap
      outFile.write(struct.pack("<3L", CURRENT_PROTOCOL_VERSION, self.what, len(self.__fields)))
      for fieldName in list(self.__fields.keys()):
         self.__writeUtf8StringWithHeader(outFile, fieldName)
         fieldContents = self.GetFieldContents(fieldName)
         fieldType = self.GetFieldType(fieldName)
         outFile.write(struct.pack("<2L", fieldType, self.GetFieldContentsLength(fieldType, fieldContents)))

         # Convert to array form and byte swap, if necessary
         isFieldContentsArray = isinstance(fieldContents, array.array)
         if _dataNeedsSwap or (not isFieldContentsArray):
            wasFieldContentsArray = isFieldContentsArray
            isFieldContentsArray = True
            if fieldType == B_BOOL_TYPE:
               fieldContents = array.array('b', fieldContents)
            elif fieldType == B_DOUBLE_TYPE:
               fieldContents = array.array('d', fieldContents)
            elif fieldType == B_FLOAT_TYPE:
               fieldContents = array.array('f', fieldContents)
            elif fieldType == B_INT32_TYPE:
               fieldContents = array.array('i', fieldContents)
            elif (fieldType == B_INT64_TYPE):
               fieldContents = array.array('q', fieldContents)
            elif fieldType == B_INT16_TYPE:
               fieldContents = array.array('h', fieldContents)
            elif fieldType == B_INT8_TYPE:
               fieldContents = array.array('b', fieldContents)
            else:
               isFieldContentsArray = wasFieldContentsArray  # roll back!

            if _dataNeedsSwap and isFieldContentsArray:
               fieldContents.byteswap()

         # Add the actual data for this field's contents
         if isFieldContentsArray:
            if fieldType == B_BOOL_TYPE or fieldType == B_DOUBLE_TYPE or fieldType == B_FLOAT_TYPE or fieldType == B_INT32_TYPE or fieldType == B_INT16_TYPE or fieldType == B_INT8_TYPE or fieldType == B_INT64_TYPE:
               outFile.write(bytes(fieldContents))
            else:
               raise ValueError("Array fieldContents found for non-Array type in field ").with_traceback(fieldName)
         else:
            if (fieldType == B_INT64_TYPE):
               for fieldItem in fieldContents:
                  outFile.write(struct.pack("<q", fieldItem))
            elif fieldType == B_MESSAGE_TYPE:
               for fieldItem in fieldContents:
                  outFile.write(struct.pack("<L", fieldItem.FlattenedSize()))
                  fieldItem.Flatten(outFile)
            elif fieldType == B_POINT_TYPE:
               for fieldItem in fieldContents:
                  outFile.write(struct.pack("<2f", fieldItem[0], fieldItem[1]))
            elif fieldType == B_RECT_TYPE:
               for fieldItem in fieldContents:
                  outFile.write(struct.pack("<4f", fieldItem[0], fieldItem[1], fieldItem[2], fieldItem[3]))
            elif fieldType == B_STRING_TYPE:
               outFile.write(struct.pack("<L", len(fieldContents)))
               for fieldItem in fieldContents:
                  self.__writeUtf8StringWithHeader(outFile, fieldItem)
            else:
               outFile.write(struct.pack("<L", len(fieldContents)))
               for fieldItem in fieldContents:
                  if (isinstance(fieldItem, str)):
                     self.__writeUtf8StringWithHeader(outFile, fieldItem)
                  else:
                     outFile.write(struct.pack("<L", len(fieldItem)))
                     outFile.write(fieldItem)

   def GetFlattenedBuffer(self):
      """Convenience method:  returns a binary buffer that is the platform-neutral flattened representation of this Message."""
      f = io.BytesIO()
      self.Flatten(f)
      return f.getvalue()

   def SetFromFlattenedBuffer(self, buf):
      """Convenience method:  sets the state of this Message from the specified binary buffer."""
      self.Unflatten(io.BytesIO(buf))

   def GetFieldContentsLength(self, fieldType, fieldContents):
      """Returns the number of bytes it will take to flatten a field's contents.  Used internally."""
      ret = 0
      itemCount = len(fieldContents)
      if fieldType == B_BOOL_TYPE:
         ret += itemCount*1
      elif fieldType == B_DOUBLE_TYPE:
         ret += itemCount*8
      elif fieldType == B_FLOAT_TYPE:
         ret += itemCount*4
      elif fieldType == B_INT64_TYPE:
         ret += itemCount*8
      elif fieldType == B_INT32_TYPE:
         ret += itemCount*4
      elif fieldType == B_INT16_TYPE:
         ret += itemCount*2
      elif fieldType == B_INT8_TYPE:
         ret += itemCount*1
      elif fieldType == B_MESSAGE_TYPE:
         ret += itemCount*4  # item length fields (4 each) (no num_items field for this type!)
         for item in fieldContents:
            ret += item.FlattenedSize()
      elif fieldType == B_POINT_TYPE:
         ret += itemCount*8   # 2 floats per point
      elif fieldType == B_RECT_TYPE:
         ret += itemCount*16  # 4 floats per rect
      elif fieldType == B_STRING_TYPE:
         ret += 4  # num_strings(4)
         for item in fieldContents:
            ret += 4+len(item.encode())+1  # string_length(4), string(n), NUL(1)
      else:
         ret += 4  # num_bufs(4)
         for item in fieldContents:
            ret += 4+len(item)
      return ret

   def PutString(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_STRING_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_STRING_TYPE, fieldContents)

   def PutInt8(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_INT8_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_INT8_TYPE, fieldContents)

   def PutInt16(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_INT16_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_INT16_TYPE, fieldContents)

   def PutInt32(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_INT32_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_INT32_TYPE, fieldContents)

   def PutInt64(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_INT64_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_INT64_TYPE, fieldContents)

   def PutBool(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_BOOL_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_BOOL_TYPE, fieldContents)

   def PutFlat(self, fieldName, fieldContents):
      """Flattens the specified Flattenable object (or objects) and places it/them into a field of its specified type
         Note that objects passed to this method must have their TypeCode() and Flatten() method defined appropriately.
      """
      ctype = type(fieldContents)
      if ctype == list or ctype == array.ArrayType:
         vals = []
         for obj in fieldContents:
            bio = io.BytesIO()
            obj.Flatten(bio)
            vals.append(bio.getvalue())
         if (len(vals) > 0):
            return self.PutFieldContents(fieldName, fieldContents[0].TypeCode(), vals)
      else:
         bio = io.BytesIO()
         fieldContents.Flatten(bio)
         return self.PutFieldContents(fieldName, fieldContents.TypeCode(), bio.getvalue())

   def PutFloat(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_FLOAT_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_FLOAT_TYPE, fieldContents)

   def PutDouble(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_DOUBLE_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_DOUBLE_TYPE, fieldContents)

   def PutMessage(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_MESSAGE_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_MESSAGE_TYPE, fieldContents)

   def PutPoint(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_POINT_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_POINT_TYPE, fieldContents)

   def PutRect(self, fieldName, fieldContents):
      """Convenience method; identical to PutFieldContents(fieldName, B_RECT_TYPE, fieldContents)"""
      return self.PutFieldContents(fieldName, B_RECT_TYPE, fieldContents)

   def RemoveName(self, fieldName):
      """Removes the given field from the Message, if it exists."""
      if fieldName in self.__fields:
         del self.__fields[fieldName]

   def GetStrings(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all strings under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_STRING_TYPE, defaultValue)

   def GetInt8s(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all int8s under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_INT8_TYPE, defaultValue)

   def GetInt16s(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all int16s under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_INT16_TYPE, defaultValue)

   def GetInt32s(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all int32s under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_INT32_TYPE, defaultValue)

   def GetInt64s(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all int64s under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_INT64_TYPE, defaultValue)

   def GetBools(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all bools under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_BOOL_TYPE, defaultValue)

   def GetFloats(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all floats under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_FLOAT_TYPE, defaultValue)

   def GetDoubles(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all doubles under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_DOUBLE_TYPE, defaultValue)

   def GetMessages(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all messages under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_MESSAGE_TYPE, defaultValue)

   def GetPoints(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all points under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_POINT_TYPE, defaultValue)

   def GetRects(self, fieldName, defaultValue=[]):
      """Convenience method; returns a list of all rects under the given name, or (defaultValue/[]) if there are none."""
      return self.GetFieldContents(fieldName, B_RECT_TYPE, defaultValue)

   def GetString(self, fieldName, defaultValue="", index=0):
      """Convenience method; returns the (index)'th String item under (fieldName), or (defaultValue/"") if the requested string isn't present."""
      return self.GetFieldItem(fieldName, B_STRING_TYPE, defaultValue, index)

   def GetInt8(self, fieldName, defaultValue=0, index=0):
      """Convenience method; returns the (index)'th Int8 item under (fieldName), or (defaultValue/0) if the requested int8 isn't present."""
      return self.GetFieldItem(fieldName, B_INT8_TYPE, defaultValue, index)

   def GetInt16(self, fieldName, defaultValue=0, index=0):
      """Convenience method; returns the (index)'th Int16 item under (fieldName), or (defaultValue/0) if the requested int16 isn't present."""
      return self.GetFieldItem(fieldName, B_INT16_TYPE, defaultValue, index)

   def GetInt32(self, fieldName, defaultValue=0, index=0):
      """Convenience method; returns the (index)'th Int32 item under (fieldName), or (defaultValue/0) if the requested int32 isn't present."""
      return self.GetFieldItem(fieldName, B_INT32_TYPE, defaultValue, index)

   def GetInt64(self, fieldName, defaultValue=0, index=0):
      """Convenience method; returns the (index)'th Int64 item under (fieldName), or (defaultValue/0) if the requested int64 isn't present."""
      return self.GetFieldItem(fieldName, B_INT64_TYPE, defaultValue, index)

   def GetBool(self, fieldName, defaultValue=False, index=0):
      """Convenience method; returns the (index)'th Bool item under (fieldName), or (defaultValue/False) if the requested boolean isn't present."""
      return self.GetFieldItem(fieldName, B_BOOL_TYPE, defaultValue, index)

   def GetFlat(self, fieldName, flattenableObject, defaultValue=None, index=0):
      """Convenience method; Unflattens the the (index)'th Flattenable item under (fieldName) into (flattenableObject) and then returns it, or (defaultValue/None) if the requested object isn't present."""
      blob = self.GetFieldItem(fieldName, flattenableObject.TypeCode(), defaultValue, index)
      if (blob is not None):
         flattenableObject.Unflatten(io.BytesIO(blob))
         return flattenableObject
      return defaultValue

   def GetFloat(self, fieldName, defaultValue=0.0, index=0):
      """Convenience method; returns the (index)'th Float item under (fieldName), or (defaultValue/0.0) if the requested float isn't present."""
      return self.GetFieldItem(fieldName, B_FLOAT_TYPE, defaultValue, index)

   def GetDouble(self, fieldName, defaultValue=0.0, index=0):
      """Convenience method; returns the (index)'th Double item under (fieldName), or (defaultValue/0.0) if the requested double isn't present."""
      return self.GetFieldItem(fieldName, B_DOUBLE_TYPE, defaultValue, index)

   def GetMessage(self, fieldName, defaultValue=None, index=0):
      """Convenience method; returns the (index)'th Message item under (fieldName), or (defaultValue/None) if the requested Message isn't present."""
      return self.GetFieldItem(fieldName, B_MESSAGE_TYPE, defaultValue, index)

   def GetPoint(self, fieldName, defaultValue=[0.0,0.0], index=0):
      """Convenience method; returns the (index)'th Point item under (fieldName), or (defaultValue/[0.0,0.0]) if the requested Point isn't present."""
      return self.GetFieldItem(fieldName, B_POINT_TYPE, defaultValue, index)

   def GetRect(self, fieldName, defaultValue=[0.0,0.0,0.0,0.0], index=0):
      """Convenience method; returns the (index)'th Rect item under (fieldName), or (defaultValue/[0.0,0.0,0.0,0.0]) if the requested Rect isn't present."""
      return self.GetFieldItem(fieldName, B_RECT_TYPE, defaultValue, index)

   def __writeUtf8StringWithHeader(self, outFile, s):
      utf8s = s.encode()
      outFile.write(struct.pack("<L", len(utf8s)+1))
      outFile.write(utf8s)
      outFile.write(bytes([0]))


# --------------------------------------------------------------------------------------------

# Silly little test stub.  Just writes a flattened Message out to a file, then reads in back in.
if __name__ == "__main__":
   tm = Message(666)
   tm.PutBool("bool", [True,False])
   tm.PutInt8("int8", [8,9,10])
   tm.PutInt16("int16", [16,18,19])
   tm.PutInt32("int32", [32,31,30])
   tm.PutInt64("int64", [64,63,62, -20, -25])
   tm.PutString("string", ["stringme!", "strungme!", "strongme!"])
   tm.PutFloat("float", [3.14159, 6.141, 9.999, 2.1, 4])
   tm.PutDouble("double", [2.7172, 3.4, 5.6, -1.0])
   tm.PutPoint("point", [(6.5, 7.5), (9,10), (11,15)])
   tm.PutRect("rect", [(9.1,10,11,12.5), (1,2,3,4), (2,3,4,5)])
   tm.PutFieldContents("data", 555, ["testing...", "stuff", "out"])
   subMsg = Message(777)
   subMsg.PutString("hola", "senor")
   tm.PutMessage("submsg", subMsg)

   tm.PrintToStream()
   outFile = open('test.msg', 'wb')
   tm.Flatten(outFile)
   outFile.close()

   print("Unflattening...")
   inFile = open('test.msg', 'rb')
   m2 = Message()
   m2.Unflatten(inFile)
   inFile.close()
   m2.PrintToStream()
