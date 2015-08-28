/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "util/ByteBuffer.h"
#include "util/Queue.h"
#include "message/Message.h"

namespace muscle {

using namespace muscle_message_imp;

static void DoIndents(uint32 num, String & s) {for (uint32 i=0; i<num; i++) s += ' ';}

static MessageRef::ItemPool _messagePool;
MessageRef::ItemPool * GetMessagePool() {return &_messagePool;}

static ConstMessageRef _emptyMsgRef(&_messagePool.GetDefaultObject(), false);
const ConstMessageRef & GetEmptyMessageRef() {return _emptyMsgRef;}

#define DECLARECLONE(X)                             \
   AbstractDataArrayRef X :: Clone() const          \
   {                                                \
      AbstractDataArrayRef ref(NEWFIELD(X));        \
      if (ref()) *(static_cast<X*>(ref())) = *this; \
      return ref;                                   \
   }

#ifdef MUSCLE_DISABLE_MESSAGE_FIELD_POOLS
# define NEWFIELD(X)  newnothrow X
# define DECLAREFIELDTYPE(X) DECLARECLONE(X)
#else
# define NEWFIELD(X)  _pool##X.ObtainObject()
# define DECLAREFIELDTYPE(X) static ObjectPool<X> _pool##X; DECLARECLONE(X)
#endif

MessageRef GetMessageFromPool(uint32 what)
{
   MessageRef ref(_messagePool.ObtainObject());
   if (ref()) ref()->what = what;
   return ref;
}

MessageRef GetMessageFromPool(const Message & copyMe)
{
   MessageRef ref(_messagePool.ObtainObject());
   if (ref()) *(ref()) = copyMe;
   return ref;
}

MessageRef GetMessageFromPool(const uint8 * flatBytes, uint32 numBytes)
{
   MessageRef ref(_messagePool.ObtainObject());
   if ((ref())&&(ref()->Unflatten(flatBytes, numBytes) != B_NO_ERROR)) ref.Reset();
   return ref;
}

MessageRef GetMessageFromPool(ObjectPool<Message> & pool, uint32 what)
{
   MessageRef ref(pool.ObtainObject());
   if (ref()) ref()->what = what;
   return ref;
}

MessageRef GetMessageFromPool(ObjectPool<Message> & pool, const Message & copyMe)
{
   MessageRef ref(pool.ObtainObject());
   if (ref()) *(ref()) = copyMe;
   return ref;
}

MessageRef GetMessageFromPool(ObjectPool<Message> & pool, const uint8 * flatBytes, uint32 numBytes)
{
   MessageRef ref(pool.ObtainObject());
   if ((ref())&&(ref()->Unflatten(flatBytes, numBytes) != B_NO_ERROR)) ref.Reset();
   return ref;
}

MessageRef GetLightweightCopyOfMessageFromPool(const Message & copyMe)
{
   MessageRef ref(_messagePool.ObtainObject());
   if (ref()) ref()->BecomeLightweightCopyOf(copyMe);
   return ref;
}

MessageRef GetLightweightCopyOfMessageFromPool(ObjectPool<Message> & pool, const Message & copyMe)
{
   MessageRef ref(pool.ObtainObject());
   if (ref()) ref()->BecomeLightweightCopyOf(copyMe);
   return ref;
}

template <class DataType> class FixedSizeDataArray : public AbstractDataArray
{
public:
   FixedSizeDataArray() {/* empty */}
   virtual ~FixedSizeDataArray() {/* empty */}

   virtual uint32 GetNumItems() const {return _data.GetNumItems();}

   virtual void Clear(bool releaseDataBuffers) {_data.Clear(releaseDataBuffers);}
   virtual void Normalize() {_data.Normalize();}

   virtual status_t AddDataItem(const void * item, uint32 size)
   {
      return (size == sizeof(DataType)) ? (item ? _data.AddTail(*(reinterpret_cast<const DataType *>(item))) : _data.AddTail()) : B_ERROR;
   }

   virtual status_t PrependDataItem(const void * item, uint32 size)
   {
      return (size == sizeof(DataType)) ? (item ? _data.AddHead(*(reinterpret_cast<const DataType *>(item))) : _data.AddHead()) : B_ERROR;
   }

   virtual uint32 GetItemSize(uint32 /*index*/) const {return sizeof(DataType);}
   virtual status_t RemoveDataItem(uint32 index) {return _data.RemoveItemAt(index);}

   virtual status_t FindDataItem(uint32 index, const void ** setDataLoc) const
   {
      if (index < _data.GetNumItems())
      {
         *setDataLoc = &_data[index];
         return (*setDataLoc) ? B_NO_ERROR : B_ERROR;
      }
      return B_ERROR;
   }

   virtual status_t ReplaceDataItem(uint32 index, const void * data, uint32 size)
   {
      return (size == sizeof(DataType)) ? _data.ReplaceItemAt(index, *(static_cast<const DataType *>(data))) : B_ERROR;
   }

   virtual bool ElementsAreFixedSize() const {return true;}

   virtual bool IsFlattenable() const {return true;}

   const DataType & ItemAt(int i) const {return _data[i];}

   FixedSizeDataArray<DataType> & operator=(const FixedSizeDataArray<DataType> & rhs) 
   {
      if (this != &rhs) _data = rhs._data;
      return *this;
   }

protected:
   virtual bool AreContentsEqual(const AbstractDataArray * rhs) const
   {
      const FixedSizeDataArray<DataType> * trhs = dynamic_cast<const FixedSizeDataArray<DataType> *>(rhs);
      if (trhs == NULL) return false;

      for (int32 i=GetNumItems()-1; i>=0; i--) if (ItemAt(i) != trhs->ItemAt(i)) return false;
      return true;
   }

   Queue<DataType> _data;
};

static void AddItemPreambleToString(uint32 indent, uint32 idx, String & s)
{
   DoIndents(indent, s);
   char buf[64]; muscleSprintf(buf, "    " UINT32_FORMAT_SPEC ". ", idx);
   s += buf;
}

// An field of ephemeral objects that won't be flattened
class TagDataArray : public FixedSizeDataArray<RefCountableRef>
{
public:
   TagDataArray() {/* empty */}
   virtual ~TagDataArray() {/* empty */}

   virtual void Flatten(uint8 *) const
   {
      MCRASH("Message::TagDataArray:Flatten()  This method should never be called!");
   }

   // Flattenable interface
   virtual uint32 FlattenedSize() const {return 0;}  // tags don't get flattened, so they take up no space

   virtual status_t Unflatten(const uint8 *, uint32)
   {
      MCRASH("Message::TagDataArray:Unflatten()  This method should never be called!");
      return B_NO_ERROR;  // just to keep the compiler happy
   }

   virtual uint32 TypeCode() const {return B_TAG_TYPE;}

   virtual AbstractDataArrayRef Clone() const; 

   virtual bool IsFlattenable() const {return false;}

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      // This is the best we can do, since we don't know our elements' types
      return TypeCode() + GetNumItems();
   }

   virtual RefCountableRef GetItemAtAsRefCountableRef(uint32 idx) const {return ItemAt(idx);}

   static void AddItemDescriptionToString(uint32 indent, uint32 idx, const RefCountableRef & tag, String & s)
   {
      AddItemPreambleToString(indent, idx, s);
      char buf[128]; muscleSprintf(buf, "%p\n", tag()); s += buf;
   }

protected:
   virtual void AddToString(String & s, uint32, int indent) const
   {
      uint32 numItems = GetNumItems();
      for (uint32 i=0; i<numItems; i++) AddItemDescriptionToString(indent, i, _data[i], s);
   }
};
DECLAREFIELDTYPE(TagDataArray);

// a field of flattened objects, where all flattened objects are guaranteed to be the same size flattened
template <class DataType, int FlatItemSize, uint32 ItemTypeCode> class FixedSizeFlatObjectArray : public FixedSizeDataArray<DataType>
{
public:
   FixedSizeFlatObjectArray() 
   {
      // empty
   }

   virtual ~FixedSizeFlatObjectArray() 
   {
      // empty
   }

   virtual void Flatten(uint8 * buffer) const
   {
      uint32 numItems = this->_data.GetNumItems(); 
      for (uint32 i=0; i<numItems; i++) 
      {
         this->_data[i].Flatten(buffer);
         buffer += FlatItemSize;
      }
   }

   // Flattenable interface
   virtual uint32 FlattenedSize() const {return this->_data.GetNumItems() * FlatItemSize;}

   virtual status_t Unflatten(const uint8 * buffer, uint32 numBytes)
   {
      this->_data.Clear();
      if (numBytes % FlatItemSize) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "FixedSizeDataArray %p:  Unexpected numBytes " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", this, numBytes, FlatItemSize);
         return B_ERROR;  // length must be an even multiple of item size, or something's wrong!
      }

      DataType temp;
      uint32 numItems = numBytes / FlatItemSize;
      if (this->_data.EnsureSize(numItems) == B_NO_ERROR)
      {
         for (uint32 i=0; i<numItems; i++)
         {
            if ((temp.Unflatten(buffer, FlatItemSize) != B_NO_ERROR)||(this->_data.AddTail(temp) != B_NO_ERROR)) 
            {
               LogTime(MUSCLE_LOG_DEBUG, "FixedSizeDataArray %p:  Error unflattened item " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", this, i, numItems);
               return B_ERROR;
            }
            buffer += FlatItemSize;
         }
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   virtual uint32 TypeCode() const {return ItemTypeCode;}

protected:
   virtual void AddToString(String & s, uint32, int indent) const
   {
      uint32 numItems = this->GetNumItems();
      for (uint32 i=0; i<numItems; i++) 
      {
         AddItemPreambleToString(indent, i, s);
         AddItemToString(s, this->_data[i]);
         s += '\n';
      }
   }

   virtual void AddItemToString(String & s, const DataType & item) const = 0;
};

/* This class handles storage of all the primitive numeric types for us. */
template <class DataType> class PrimitiveTypeDataArray : public FixedSizeDataArray<DataType> 
{
public:
   PrimitiveTypeDataArray() {/* empty */}
   virtual ~PrimitiveTypeDataArray() {/* empty */}

   virtual void Flatten(uint8 * buffer) const
   {
      DataType * dBuf = reinterpret_cast<DataType *>(buffer);
      switch(this->_data.GetNumItems())
      {
         case 0:
            // do nothing
         break;

         case 1:
            ConvertToNetworkByteOrder(dBuf, this->_data.HeadPointer(), 1);
         break;

         default:
         {
            uint32 len0 = 0;
            const DataType * field0 = this->_data.GetArrayPointer(0, len0);
            if (field0) ConvertToNetworkByteOrder(dBuf, field0, len0);

            uint32 len1;
            const DataType * field1 = this->_data.GetArrayPointer(1, len1);
            if (field1) ConvertToNetworkByteOrder(&dBuf[len0], field1, len1); 
         }
         break;
      }
   }

   virtual status_t Unflatten(const uint8 * buffer, uint32 numBytes)
   {
      this->_data.Clear();
      if (numBytes % sizeof(DataType)) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "PrimitiveTypeDataArray %p:  Unexpected numBytes " UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC "\n", this, numBytes, (uint32) sizeof(DataType));
         return B_ERROR;  // length must be an even multiple of item size, or something's wrong!
      }

      uint32 numItems = numBytes / sizeof(DataType);
      if (this->_data.EnsureSize(numItems, true) == B_NO_ERROR)
      {  
         // Note that typically you can't rely on the contents of a Queue object to
         // be stored in a single, contiguous field like this, but in this case we've
         // called Clear() and then EnsureSize(), so we know that the field's headPointer
         // is at the front of the field, and we are safe to do this.  --jaf
         ConvertFromNetworkByteOrder(this->_data.HeadPointer(), reinterpret_cast<const DataType *>(buffer), numItems);
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   // Flattenable interface
   virtual uint32 FlattenedSize() const {return this->_data.GetNumItems() * sizeof(DataType);}

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const = 0;
   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const = 0;

   virtual const char * GetFormatString() const = 0;

   virtual void AddToString(String & s, uint32, int indent) const
   {
      uint32 numItems = this->GetNumItems();
      for (uint32 i=0; i<numItems; i++) 
      {
         AddItemPreambleToString(indent, i, s);
         s += '[';
         char temp[100]; muscleSprintf(temp, GetFormatString(), this->ItemAt(i)); s += temp;
         s += ']';
         s += '\n';
      }
   }
};

static String PointToString(const Point & p)
{
   char buf[128]; muscleSprintf(buf, "Point: x=%f y=%f", p.x(), p.y());
   return buf;
}

class PointDataArray : public FixedSizeFlatObjectArray<Point,sizeof(Point),B_POINT_TYPE>
{
public:
   PointDataArray() {/* empty */}
   virtual ~PointDataArray() {/* empty */}
   virtual AbstractDataArrayRef Clone() const;
   virtual void AddItemToString(String & s, const Point & p) const {s += PointToString(p);}

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*_data[i].CalculateChecksum());
      return ret;
   }
};
DECLAREFIELDTYPE(PointDataArray);

static String RectToString(const Rect & r)
{
   char buf[256]; muscleSprintf(buf, "Rect: leftTop=(%f,%f) rightBottom=(%f,%f)", r.left(), r.top(), r.right(), r.bottom());
   return buf;
}

class RectDataArray : public FixedSizeFlatObjectArray<Rect,sizeof(Rect),B_RECT_TYPE>
{
public:
   RectDataArray() {/* empty */}
   virtual ~RectDataArray() {/* empty */}
   virtual AbstractDataArrayRef Clone() const;
   virtual void AddItemToString(String & s, const Rect & r) const {s += RectToString(r);}

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*_data[i].CalculateChecksum());
      return ret;
   }
};
DECLAREFIELDTYPE(RectDataArray);

class Int8DataArray : public PrimitiveTypeDataArray<int8> 
{
public:
   Int8DataArray() {/* empty */}
   virtual ~Int8DataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_INT8_TYPE;}

   virtual const char * GetFormatString() const {return "%i";}
 
   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*((uint32)_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      memcpy(writeToHere, readFromHere, numItems);  // no translation required, really
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      memcpy(writeToHere, readFromHere, numItems);  // no translation required, really
   }
};
DECLAREFIELDTYPE(Int8DataArray);

class BoolDataArray : public PrimitiveTypeDataArray<bool>
{
public:
   BoolDataArray() {/* empty */}
   virtual ~BoolDataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_BOOL_TYPE;}

   virtual const char * GetFormatString() const {return "%i";}

   virtual AbstractDataArrayRef Clone() const;

   virtual void Flatten(uint8 * buffer) const
   {
      uint32 numItems = _data.GetNumItems(); 
      for (uint32 i=0; i<numItems; i++) buffer[i] = (uint8) (_data[i] ? 1 : 0);
   }

   virtual status_t Unflatten(const uint8 * buffer, uint32 numBytes)
   {
      _data.Clear();

      if (_data.EnsureSize(numBytes) == B_NO_ERROR)
      {
         for (uint32 i=0; i<numBytes; i++) if (_data.AddTail((buffer[i] != 0) ? true : false) != B_NO_ERROR) return B_ERROR;
         return B_NO_ERROR;
      }
      else return B_ERROR;
   }

   // Flattenable interface
   virtual uint32 FlattenedSize() const {return _data.GetNumItems()*sizeof(uint8);}  /* bools are always flattened into 1 byte each */

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*(_data[i] ? 1 : 0));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void *, const void *, uint32) const
   {
      MCRASH("BoolDataArray::ConvertToNetworkByteOrder should never be called");
   }

   virtual void ConvertFromNetworkByteOrder(void *, const void *, uint32) const
   {
      MCRASH("BoolDataArray::ConvertFromNetworkByteOrder should never be called");
   }
};
DECLAREFIELDTYPE(BoolDataArray);

class Int16DataArray : public PrimitiveTypeDataArray<int16> 
{
public:
   Int16DataArray() {/* empty */}
   virtual ~Int16DataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_INT16_TYPE;}

   virtual const char * GetFormatString() const {return "%i";}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*((uint32)_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int16 * writeToHere16 = static_cast<int16 *>(writeToHere);
      const int16 * readFromHere16 = (const int16 *) readFromHere;
      for (uint32 i=0; i<numItems; i++) muscleCopyOut(&writeToHere16[i], B_HOST_TO_LENDIAN_INT16(readFromHere16[i]));
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int16 * writeToHere16 = (int16 *) writeToHere;
      const int16 * readFromHere16 = (const int16 *) readFromHere;
      for (uint32 i=0; i<numItems; i++)
      {
         int16 val; muscleCopyIn(val, &readFromHere16[i]);
         writeToHere16[i] = B_LENDIAN_TO_HOST_INT16(val);
      }
   }
};
DECLAREFIELDTYPE(Int16DataArray);

class Int32DataArray : public PrimitiveTypeDataArray<int32> 
{
public:
   Int32DataArray() {/* empty */}
   virtual ~Int32DataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_INT32_TYPE;}

   virtual const char * GetFormatString() const {return INT32_FORMAT_SPEC;}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*((uint32)_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int32 * writeToHere32 = (int32 *) writeToHere;
      const int32 * readFromHere32 = (const int32 *) readFromHere;
      for (uint32 i=0; i<numItems; i++) muscleCopyOut(&writeToHere32[i], B_HOST_TO_LENDIAN_INT32(readFromHere32[i]));
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int32 * writeToHere32 = (int32 *) writeToHere;
      const int32 * readFromHere32 = (const int32 *) readFromHere;
      for (uint32 i=0; i<numItems; i++)
      {
         int32 val; muscleCopyIn(val, &readFromHere32[i]);
         writeToHere32[i] = B_LENDIAN_TO_HOST_INT32(val);
      }
   }
};
DECLAREFIELDTYPE(Int32DataArray);

class Int64DataArray : public PrimitiveTypeDataArray<int64> 
{
public:
   Int64DataArray() {/* empty */}
   virtual ~Int64DataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_INT64_TYPE;}

   virtual const char * GetFormatString() const {return INT64_FORMAT_SPEC;}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*CalculateChecksumForUint64(_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int64 * writeToHere64 = (int64 *) writeToHere;
      const int64 * readFromHere64 = (const int64 *) readFromHere;
      for (uint32 i=0; i<numItems; i++) muscleCopyOut(&writeToHere64[i], B_HOST_TO_LENDIAN_INT64(readFromHere64[i]));
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int64 * writeToHere64 = (int64 *) writeToHere;
      const int64 * readFromHere64 = (const int64 *) readFromHere;
      for (uint32 i=0; i<numItems; i++)
      {
         int64 val; muscleCopyIn(val, &readFromHere64[i]);
         writeToHere64[i] = B_LENDIAN_TO_HOST_INT64(val);
      }
   }
};
DECLAREFIELDTYPE(Int64DataArray);

class FloatDataArray : public PrimitiveTypeDataArray<float> 
{
public:
   FloatDataArray() {/* empty */}
   virtual ~FloatDataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_FLOAT_TYPE;}

   virtual const char * GetFormatString() const {return "%f";}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*CalculateChecksumForFloat(_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int32 * writeToHere32 = (int32 *) writeToHere;  // yeah, they're really floats, but no need to worry about that here
      const int32 * readFromHere32 = (const int32 *) readFromHere;
      for (uint32 i=0; i<numItems; i++) muscleCopyOut(&writeToHere32[i], B_HOST_TO_LENDIAN_INT32(readFromHere32[i]));
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int32 * writeToHere32 = (int32 *) writeToHere;  // yeah, they're really floats, but no need to worry about that here
      const int32 * readFromHere32 = (const int32 *) readFromHere;
      for (uint32 i=0; i<numItems; i++)
      {
         int32 val; muscleCopyIn(val, &readFromHere32[i]);
         writeToHere32[i] = B_LENDIAN_TO_HOST_INT32(val);
      }
   }
};
DECLAREFIELDTYPE(FloatDataArray);

class DoubleDataArray : public PrimitiveTypeDataArray<double> 
{
public:
   DoubleDataArray() {/* empty */}
   virtual ~DoubleDataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_DOUBLE_TYPE;}

   virtual const char * GetFormatString() const {return "%lf";}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*CalculateChecksumForDouble(_data[i]));
      return ret;
   }

protected:
   virtual void ConvertToNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int64 * writeToHere64 = (int64 *) writeToHere;  // yeah, they're really doubles, but no need to worry about that here
      const int64 * readFromHere64 = (const int64 *) readFromHere;
      for (uint32 i=0; i<numItems; i++) muscleCopyOut(&writeToHere64[i], B_HOST_TO_LENDIAN_INT64(readFromHere64[i]));
   }

   virtual void ConvertFromNetworkByteOrder(void * writeToHere, const void * readFromHere, uint32 numItems) const
   {
      int64 * writeToHere64 = (int64 *) writeToHere;  // yeah, they're really doubles, but no need to worry about that here
      const int64 * readFromHere64 = (const int64 *) readFromHere;
      for (uint32 i=0; i<numItems; i++)
      {
         int64 val; muscleCopyIn(val, &readFromHere64[i]);
         writeToHere64[i] = B_LENDIAN_TO_HOST_INT64(val);
      }
   }
};
DECLAREFIELDTYPE(DoubleDataArray);

class PointerDataArray : public PrimitiveTypeDataArray<void *> 
{
public:
   PointerDataArray() {/* empty */}
   virtual ~PointerDataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_POINTER_TYPE;}

   virtual const char * GetFormatString() const {return "%p";}

   virtual bool IsFlattenable() const {return false;}

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      return TypeCode() + GetNumItems();  // Best we can do, since pointer equivalence is not well defined
   }

protected:
   virtual void ConvertToNetworkByteOrder(void *, const void *, uint32) const
   {
      MCRASH("PointerDataArray::ConvertToNetworkByteOrder should never be called");
   }

   virtual void ConvertFromNetworkByteOrder(void *, const void *, uint32) const
   {
      MCRASH("PointerDataArray::ConvertFromNetworkByteOrder should never be called");
   }
};
DECLAREFIELDTYPE(PointerDataArray);

// An abstract field of FlatCountableRefs.
template <class RefType, uint32 ItemTypeCode> class FlatCountableRefDataArray : public FixedSizeDataArray<RefType>
{
public:
   FlatCountableRefDataArray() {/* empty */}
   virtual ~FlatCountableRefDataArray() {/* empty */}

   virtual uint32 GetItemSize(uint32 index) const 
   {
      const FlatCountable * msg = this->ItemAt(index)();
      return msg ? msg->FlattenedSize() : 0;
   }

   virtual uint32 TypeCode() const {return ItemTypeCode;}

   virtual bool ElementsAreFixedSize() const {return false;}

   /** Whether or not we should write the number-of-items element when we flatten this field.
     * Older versions of muscle didn't do this for MessageDataArray objects, so we need to
     * maintain that behaviour so that we don't break compatibility.  (bleah)
     */
   virtual bool ShouldWriteNumItems() const {return true;}

   virtual void Flatten(uint8 * buffer) const
   {
      uint32 writeOffset = 0;
      uint32 numItems = this->_data.GetNumItems(); 

      // Conditional to allow maintaining backwards compatibility with old versions of muscle's MessageDataArrays (sigh)
      if (ShouldWriteNumItems()) 
      {
         uint32 writeNumElements = B_HOST_TO_LENDIAN_INT32(numItems);
         this->WriteData(buffer, &writeOffset, &writeNumElements, sizeof(writeNumElements));
      }

      for (uint32 i=0; i<numItems; i++) 
      {
         const FlatCountable * next = this->ItemAt(i)();
         if (next)
         {
            uint32 fs = next->FlattenedSize();
            uint32 writeFs = B_HOST_TO_LENDIAN_INT32(fs);
            this->WriteData(buffer, &writeOffset, &writeFs, sizeof(writeFs));
            next->Flatten(&buffer[writeOffset]);
            writeOffset += fs;
         }
      }
   }

   // Flattenable interface
   virtual uint32 FlattenedSize() const 
   {
      uint32 numItems = this->GetNumItems();
      uint32 count = (numItems+(ShouldWriteNumItems()?1:0))*sizeof(uint32);
      for (uint32 i=0; i<numItems; i++) count += GetItemSize(i);
      return count;
   }

   virtual RefCountableRef GetItemAtAsRefCountableRef(uint32 idx) const {return this->ItemAt(idx).GetRefCountableRef();}
};

static bool AreByteBufferPointersEqual(const ByteBuffer * myBuf, const ByteBuffer * hisBuf)
{
   if ((myBuf != NULL) != (hisBuf != NULL)) return false;
   return myBuf ? (*myBuf == *hisBuf) : true;
}

class ByteBufferDataArray : public FlatCountableRefDataArray<FlatCountableRef, B_RAW_TYPE>
{
public:
   ByteBufferDataArray() : _typeCode(B_RAW_TYPE) {/* empty */}
   virtual ~ByteBufferDataArray() {/* empty */}

   /** Sets our type code.  Typically called after using the default ctor. */
   void SetTypeCode(uint32 tc) {_typeCode = tc;}

   virtual uint32 TypeCode() const {return _typeCode;}

   virtual status_t Unflatten(const uint8 * buffer, uint32 numBytes)
   {
      Clear(false);

      uint32 readOffset = 0;

      uint32 numItems;
      if (ReadData(buffer, numBytes, &readOffset, &numItems, sizeof(numItems)) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "ByteBufferDataArray %p:  Error reading numItems (numBytes=" UINT32_FORMAT_SPEC ")\n", this, numBytes);
         return B_ERROR;
      }
      numItems = B_LENDIAN_TO_HOST_INT32(numItems);
   
      for (uint32 i=0; i<numItems; i++)
      {
         uint32 readFs;
         if (ReadData(buffer, numBytes, &readOffset, &readFs, sizeof(readFs)) != B_NO_ERROR) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "ByteBufferDataArray %p:  Error reading item size (i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ", readOffset=" UINT32_FORMAT_SPEC ", numBytes=" UINT32_FORMAT_SPEC ")\n", this, i, numItems, readOffset, numBytes);
            return B_ERROR;
         }

         readFs = B_LENDIAN_TO_HOST_INT32(readFs);
         if (readOffset + readFs > numBytes) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "ByteBufferDataArray %p:  Item size too large (i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ", readOffset=" UINT32_FORMAT_SPEC ", numBytes=" UINT32_FORMAT_SPEC ", readFs=" UINT32_FORMAT_SPEC ")\n", this, i, numItems, readOffset, numBytes, readFs);
            return B_ERROR;  // message size too large for our buffer... corruption?
         }
         FlatCountableRef fcRef(GetByteBufferFromPool(readFs, &buffer[readOffset]).GetRefCountableRef(), true);
         if ((fcRef())&&(AddDataItem(&fcRef, sizeof(fcRef)) == B_NO_ERROR)) readOffset += readFs;
                                                                       else return B_ERROR;
      }
      return B_NO_ERROR;
   }

   static void AddItemDescriptionToString(uint32 indent, uint32 idx, const FlatCountableRef & fcRef, String & s)
   {
      AddItemPreambleToString(indent, idx, s);

      FlatCountable * fc = fcRef();
      ByteBuffer temp;
      ByteBuffer * bb = dynamic_cast<ByteBuffer *>(fc);
      if ((bb == NULL)&&(fc))
      {
         temp.SetNumBytes(fc->FlattenedSize(), false);
         if (temp())
         {
            fc->Flatten((uint8*)temp());
            bb = &temp;
         }
      }

      if (bb)
      {
         char buf[100];
         muscleSprintf(buf, "[flattenedSize=" UINT32_FORMAT_SPEC "] ", bb->GetNumBytes()); 
         s += buf;
         uint32 printBytes = muscleMin(bb->GetNumBytes(), (uint32)10);
         if (printBytes > 0)
         {
            s += '[';
            for (uint32 j=0; j<printBytes; j++) 
            {
               muscleSprintf(buf, "%02x%s", (bb->GetBuffer())[j], (j<printBytes-1)?" ":"");
               s += buf;
            }
            if (printBytes > 10) s += " ...";
            s += ']';
         }
      }
      else s += "[NULL]";

      s += '\n';
   }

   virtual void AddToString(String & s, uint32, int indent) const
   {
      uint32 numItems = GetNumItems();
      for (uint32 i=0; i<numItems; i++) AddItemDescriptionToString(indent, i, ItemAt(i), s);
   }

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--)
      {
         const ByteBuffer * buf = dynamic_cast<ByteBuffer *>(_data[i]());  // TODO: possibly make this a static cast?
         if (buf) ret += ((i+1)*buf->CalculateChecksum());
      }
      return ret;
   }

protected:
   /** Overridden to compare objects instead of merely the pointers to them */
   virtual bool AreContentsEqual(const AbstractDataArray * rhs) const
   {
      const ByteBufferDataArray * brhs = dynamic_cast<const ByteBufferDataArray *>(rhs);
      if (brhs == NULL) return false;

      for (int32 i=GetNumItems()-1; i>=0; i--) 
      {
         const ByteBuffer * myBuf  = static_cast<const ByteBuffer *>(this->ItemAt(i)());
         const ByteBuffer * hisBuf = static_cast<const ByteBuffer *>(brhs->ItemAt(i)());
         if (AreByteBufferPointersEqual(myBuf, hisBuf) == false) return false;
      }
      return true;
   }

private:
   uint32 _typeCode;
};
DECLAREFIELDTYPE(ByteBufferDataArray);

static bool AreMessagePointersEqual(const Message * myMsg, const Message * hisMsg)
{
   if ((myMsg != NULL) != (hisMsg != NULL)) return false;
   return myMsg ? (*myMsg == *hisMsg) : true;
}

class MessageDataArray : public FlatCountableRefDataArray<MessageRef, B_MESSAGE_TYPE>
{
public:
   MessageDataArray() {/* empty */}
   virtual ~MessageDataArray() {/* empty */}

   /** For backwards compatibility with older muscle streams */
   virtual bool ShouldWriteNumItems() const {return false;}

   virtual status_t Unflatten(const uint8 * buffer, uint32 numBytes)
   {
      Clear(false);

      uint32 readOffset = 0;
      while(readOffset < numBytes)
      {
         uint32 readFs;
         if (ReadData(buffer, numBytes, &readOffset, &readFs, sizeof(readFs)) != B_NO_ERROR) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "MessageDataArray %p:  Read of sub-message size failed (readOffset=" UINT32_FORMAT_SPEC ", numBytes=" UINT32_FORMAT_SPEC ")\n", this, readOffset, numBytes);
            return B_ERROR;
         }

         readFs = B_LENDIAN_TO_HOST_INT32(readFs);
         if (readOffset + readFs > numBytes) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "MessageDataArray %p:  Sub-message size too large (readOffset=" UINT32_FORMAT_SPEC ", numBytes=" UINT32_FORMAT_SPEC ", readFs=" UINT32_FORMAT_SPEC ")\n", this, readOffset, numBytes, readFs);
            return B_ERROR;  // message size too large for our buffer... corruption?
         }
         MessageRef nextMsg = GetMessageFromPool();
         if (nextMsg())
         {
            if (nextMsg()->Unflatten(&buffer[readOffset], readFs) != B_NO_ERROR) 
            {
               LogTime(MUSCLE_LOG_DEBUG, "MessageDataArray %p:  Sub-message unflatten failed (readOffset=" UINT32_FORMAT_SPEC ", numBytes=" UINT32_FORMAT_SPEC ", readFs=" UINT32_FORMAT_SPEC ")\n", this, readOffset, numBytes, readFs);
               return B_ERROR;
            }
            if (AddDataItem(&nextMsg, sizeof(nextMsg)) != B_NO_ERROR) return B_ERROR;
            readOffset += readFs;
         }
         else return B_ERROR;
      }
      return B_NO_ERROR;
   }

   static void AddItemDescriptionToString(uint32 indent, uint32 i, const MessageRef & msgRef, String & s, uint32 maxRecurseLevel)
   {
      AddItemPreambleToString(indent, i, s);

      const Message * msg = msgRef();
      if (msg)
      {
         char tcbuf[5]; MakePrettyTypeCodeString(msg->what, tcbuf);
         char buf[100]; muscleSprintf(buf, "[what='%s' (" INT32_FORMAT_SPEC "/0x" XINT32_FORMAT_SPEC "), flattenedSize=" UINT32_FORMAT_SPEC ", numFields=" UINT32_FORMAT_SPEC "]\n", tcbuf, msg->what, msg->what, msg->FlattenedSize(), msg->GetNumNames());
         s += buf;

         if (maxRecurseLevel > 0) msg->AddToString(s, maxRecurseLevel-1, indent+3);
      }
      else s += "[NULL]\n";
   }

   virtual void AddToString(String & s, uint32 maxRecurseLevel, int indent) const
   {
      uint32 numItems = GetNumItems();
      for (uint32 i=0; i<numItems; i++) AddItemDescriptionToString(indent, i, ItemAt(i), s, maxRecurseLevel);
   }

   virtual AbstractDataArrayRef Clone() const;

   virtual uint32 CalculateChecksum(bool countNonFlattenableFields) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--)
      {
         const MessageRef & msg = _data[i];
         if (msg()) ret += ((i+1)*(msg()->CalculateChecksum(countNonFlattenableFields)));
      }
      return ret;
   }

protected:
   /** Overridden to compare objects instead of merely the pointers to them */
   virtual bool AreContentsEqual(const AbstractDataArray * rhs) const
   {
      const MessageDataArray * trhs = dynamic_cast<const MessageDataArray *>(rhs);
      if (trhs == NULL) return false;

      for (int32 i=GetNumItems()-1; i>=0; i--) 
      {
         const Message * myMsg  = static_cast<const Message *>(this->ItemAt(i)());
         const Message * hisMsg = static_cast<const Message *>(trhs->ItemAt(i)());
         if (AreMessagePointersEqual(myMsg, hisMsg) == false) return false;
      }
      return true;
   }
};
DECLAREFIELDTYPE(MessageDataArray);

// An field of Flattenable objects which are *not* guaranteed to all have the same flattened size. 
template <class DataType> class VariableSizeFlatObjectArray : public FixedSizeDataArray<DataType>
{
public:
   VariableSizeFlatObjectArray() {/* empty */}
   virtual ~VariableSizeFlatObjectArray() {/* empty */}

   virtual uint32 GetItemSize(uint32 index) const {return this->ItemAt(index).FlattenedSize();}
   virtual bool ElementsAreFixedSize() const {return false;}

   virtual void Flatten(uint8 * buffer) const
   {
      // Format:  0. number of entries (4 bytes)
      //          1. entry size in bytes (4 bytes)
      //          2. entry data (n bytes)
      //          (repeat 1. and 2. as necessary)
      uint32 numElements      = this->GetNumItems();
      uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(numElements);
      uint32 writeOffset      = 0;

      this->WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

      for (uint32 i=0; i<numElements; i++)
      {
         // write element size
         const DataType & s = this->ItemAt(i);
         uint32 nextElementBytes = s.FlattenedSize();
         networkByteOrder = B_HOST_TO_LENDIAN_INT32(nextElementBytes);
         this->WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

         // write element data
         s.Flatten(&buffer[writeOffset]);
         writeOffset += nextElementBytes;
      }
   }

   virtual uint32 FlattenedSize() const 
   {
      uint32 num = this->GetNumItems();
      uint32 sum = (num+1)*sizeof(uint32);  // 1 uint32 for the count, plus 1 per entry for entry-size
      for (uint32 i=0; i<num; i++) sum += this->ItemAt(i).FlattenedSize();
      return sum;
   }

   virtual status_t Unflatten(const uint8 * buffer, uint32 inputBufferBytes)
   {
      this->Clear(false);

      uint32 networkByteOrder;
      uint32 readOffset = 0;

      if (this->ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "VariableSizeFlatObjectArray %p:  Read of numElements failed (inputBufferBytes=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes);
         return B_ERROR;
      }

      uint32 numElements = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if (this->_data.EnsureSize(numElements) != B_NO_ERROR) return B_ERROR;
      for (uint32 i=0; i<numElements; i++)
      {
         if (this->ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "VariableSizeFlatObjectArray %p:  Read of element size failed (inputBufferBytes=" UINT32_FORMAT_SPEC ", i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, i, numElements);
            return B_ERROR;
         }
         uint32 elementSize = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
         if (elementSize == 0) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "VariableSizeFlatObjectArray %p:  Element size was zero! (inputBufferBytes=" UINT32_FORMAT_SPEC ", i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, i, numElements);
            return B_ERROR;  // it should always have at least the trailing NUL byte!
         }

         // read element data
         if ((readOffset + elementSize > inputBufferBytes)||(this->_data.AddTail() != B_NO_ERROR)||(this->_data.TailPointer()->Unflatten(&buffer[readOffset], elementSize) != B_NO_ERROR)) 
         {
            LogTime(MUSCLE_LOG_DEBUG, "VariableSizeFlatObjectArray %p:  Element size was too large! (inputBufferBytes=" UINT32_FORMAT_SPEC ", i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ", readOffset=" UINT32_FORMAT_SPEC ", elementSize=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, i, numElements, readOffset, elementSize);
            return B_ERROR;
         }
         readOffset += elementSize;
      }
      return B_NO_ERROR;
   }
};

class StringDataArray : public VariableSizeFlatObjectArray<String>
{
public:
   StringDataArray() {/* empty */}
   virtual ~StringDataArray() {/* empty */}

   virtual uint32 TypeCode() const {return B_STRING_TYPE;}

   virtual AbstractDataArrayRef Clone() const;

   virtual void AddToString(String & s, uint32, int indent) const
   {
      uint32 numItems = GetNumItems();
      for (uint32 i=0; i<numItems; i++) AddDataItemToString(indent, i, ItemAt(i), s);
   }

   static void AddDataItemToString(uint32 indent, uint32 i, const String & nextStr, String & s)
   {
      AddItemPreambleToString(indent, i, s);
      s += '[';
      s += nextStr;
      s += ']';
      s += '\n';
   }

   virtual uint32 CalculateChecksum(bool /*countNonFlattenableFields*/) const
   {
      uint32 ret = TypeCode() + GetNumItems();
      for (int32 i=GetNumItems()-1; i>=0; i--) ret += ((i+1)*(_data[i].CalculateChecksum()));
      return ret;
   }
};
DECLAREFIELDTYPE(StringDataArray);

void MessageFieldNameIterator :: SkipNonMatchingFieldNames()
{
   // Gotta move ahead until we find the first matching value!
   while((_iter.HasData())&&(_iter.GetValue().TypeCode() != _typeCode)) _iter++;
}

Message & Message :: operator=(const Message & rhs) 
{
   TCHECKPOINT;

   if (this != &rhs)
   {
      Clear((rhs._entries.IsEmpty())&&(_entries.GetNumAllocatedItemSlots()>MUSCLE_HASHTABLE_DEFAULT_CAPACITY));  // FogBugz #10274
      what     = rhs.what;
      _entries = rhs._entries;
      for (HashtableIterator<String, MessageField> iter(_entries); iter.HasData(); iter++) iter.GetValue().EnsurePrivate();  // a copied Message shouldn't share data
   }
   return *this;
}

status_t Message :: GetInfo(const String & fieldName, uint32 * type, uint32 * c, bool * fixedSize) const
{
   const MessageField * field = GetMessageField(fieldName, B_ANY_TYPE);
   if (field == NULL) return B_ERROR;
   if (type)      *type      = field->TypeCode();
   if (c)         *c         = field->GetNumItems();
   if (fixedSize) *fixedSize = field->ElementsAreFixedSize();
   return B_NO_ERROR;
}

uint32 Message :: GetNumNames(uint32 type) const 
{
   if (type == B_ANY_TYPE) return _entries.GetNumItems();

   // oops, gotta count just the entries of the given type
   uint32 total = 0;
   for (HashtableIterator<String, MessageField> it(_entries, HTIT_FLAG_NOREGISTER); it.HasData(); it++) if (it.GetValue().TypeCode() == type) total++;
   return total;
}

void Message :: PrintToStream(FILE * optFile, uint32 maxRecurseLevel, int indent) const 
{
   String s; AddToString(s, maxRecurseLevel, indent);
   fprintf(optFile?optFile:stdout, "%s", s());
}

String Message :: ToString(uint32 maxRecurseLevel, int indent) const 
{
   String s;  
   AddToString(s, maxRecurseLevel, indent);
   return s;
}

void Message :: AddToString(String & s, uint32 maxRecurseLevel, int indent) const 
{
   TCHECKPOINT;

   String ret;

   char prettyTypeCodeBuf[5];
   MakePrettyTypeCodeString(what, prettyTypeCodeBuf);

   char buf[128];
   DoIndents(indent,s); 
   muscleSprintf(buf, "Message:  what='%s' (" INT32_FORMAT_SPEC "/0x" XINT32_FORMAT_SPEC "), entryCount=" INT32_FORMAT_SPEC ", flatSize=" UINT32_FORMAT_SPEC " checksum=" UINT32_FORMAT_SPEC "\n", prettyTypeCodeBuf, what, what, GetNumNames(B_ANY_TYPE), FlattenedSize(), CalculateChecksum());
   s += buf;

   for (HashtableIterator<String, MessageField> iter(_entries, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      const MessageField & mf = iter.GetValue();
      uint32 tc = mf.TypeCode();
      MakePrettyTypeCodeString(tc, prettyTypeCodeBuf);
      DoIndents(indent,s); 
      s += "  Entry: Name=[";
      s += iter.GetKey();
      muscleSprintf(buf, "], GetNumItems()=" INT32_FORMAT_SPEC ", TypeCode()='%s' (" INT32_FORMAT_SPEC ") flatSize=" UINT32_FORMAT_SPEC " checksum=" UINT32_FORMAT_SPEC "\n", mf.GetNumItems(), prettyTypeCodeBuf, tc, mf.FlattenedSize(), mf.CalculateChecksum(false));
      s += buf;
      mf.AddToString(s, maxRecurseLevel, indent);
   }
}

// Returns an pointer to a held field of the given type, if it exists.  If (tc) is B_ANY_TYPE, then any type field is acceptable.
MessageField * Message :: GetMessageField(const String & fieldName, uint32 tc)
{
   MessageField * field;
   return (((field = _entries.Get(fieldName)) != NULL)&&((tc == B_ANY_TYPE)||(tc == field->TypeCode()))) ? field : NULL;
}

// Returns a read-only pointer to a held field of the given type, if it exists.  If (tc) is B_ANY_TYPE, then any type field is acceptable.
const MessageField * Message :: GetMessageField(const String & fieldName, uint32 tc) const
{
   const MessageField * field;
   return (((field = _entries.Get(fieldName)) != NULL)&&((tc == B_ANY_TYPE)||(tc == field->TypeCode()))) ? field : NULL;
}

// Called by FindFlat(), which (due to its templated nature) can't access this info directly
const MessageField * Message :: GetMessageFieldAndTypeCode(const String & fieldName, uint32 index, uint32 * retTC) const
{
   const MessageField * mf = _entries.Get(fieldName);
   if ((mf)&&(index < mf->GetNumItems()))
   {
      *retTC = mf->TypeCode();
      return mf;
   }
   return NULL;
}

MessageField * Message :: GetOrCreateMessageField(const String & fieldName, uint32 tc)
{
   MessageField * mf = GetMessageField(fieldName, tc);
   if (mf) return mf;

   // Make sure the problem isn't that there already exists a field, but of the wrong type...
   // If that's the case, we can't create a same-named field of a different type, so fail.
   return _entries.PutIfNotAlreadyPresent(fieldName, MessageField(tc));
}

status_t Message :: Rename(const String & oldFieldName, const String & newFieldName) 
{
   if (oldFieldName == newFieldName) return B_NO_ERROR;  // nothing needs to be done in this case

   MessageField temp;
   return (_entries.Remove(oldFieldName, temp) == B_NO_ERROR) ? _entries.Put(newFieldName, temp) : B_ERROR;
}

uint32 Message :: FlattenedSize() const 
{
   uint32 sum = 3 * sizeof(uint32);  // For the message header:  4 bytes for the protocol revision #, 4 bytes for the number-of-entries field, 4 bytes for what code

   // For each flattenable field: 4 bytes for the name length, name data, 4 bytes for entry type code, 4 bytes for entry data length, entry data
   for (HashtableIterator<String, MessageField> it(_entries, HTIT_FLAG_NOREGISTER); it.HasData(); it++)
   {
      const MessageField & mf = it.GetValue();
      if (mf.IsFlattenable()) sum += sizeof(uint32) + it.GetKey().FlattenedSize() + sizeof(uint32) + sizeof(uint32) + mf.FlattenedSize();
   }
   return sum;
}

uint32 Message :: CalculateChecksum(bool countNonFlattenableFields) const 
{
   uint32 ret = what;

   // Calculate the number of flattenable entries (may be less than the total number of entries!)
   for (HashtableIterator<String, MessageField> it(_entries, HTIT_FLAG_NOREGISTER); it.HasData(); it++)
   {
      // Note that I'm deliberately NOT considering the ordering of the fields when computing the checksum!
      const MessageField & mf = it.GetValue();
      if ((countNonFlattenableFields)||(mf.IsFlattenable()))
      {
         uint32 fnChk = it.GetKey().CalculateChecksum();
         ret += fnChk;
         if (fnChk == 0) ret++;  // almost-paranoia 
         ret += (fnChk*mf.CalculateChecksum(countNonFlattenableFields));  // multiplying by fnChck helps catch when two fields were swapped
      }
   }
   return ret;
}

void Message :: Flatten(uint8 * buffer) const 
{
   TCHECKPOINT;

   // Format:  0. Protocol revision number (4 bytes, always set to CURRENT_PROTOCOL_VERSION)
   //          1. 'what' code (4 bytes)
   //          2. Number of entries (4 bytes)
   //          3. Entry name length (4 bytes)
   //          4. Entry name string (flattened String)
   //          5. Entry type code (4 bytes)
   //          6. Entry data length (4 bytes)
   //          7. Entry data (n bytes)
   //          8. loop to 3 as necessary

   // Write current protocol version
   uint32 writeOffset = 0;
   uint32 networkByteOrder = B_HOST_TO_LENDIAN_INT32(CURRENT_PROTOCOL_VERSION);
   WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

   // Write 'what' code
   networkByteOrder = B_HOST_TO_LENDIAN_INT32(what);
   WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

   // Remember where to write the number-of-entries value (we'll actually write it at the end of this method)
   uint8 * entryCountPtr = &buffer[writeOffset];
   writeOffset += sizeof(uint32);

   // Write entries
   uint32 numFlattenedEntries = 0;
   for (HashtableIterator<String, MessageField> it(_entries, HTIT_FLAG_NOREGISTER); it.HasData(); it++)
   {
      const MessageField & mf = it.GetValue();
      if (mf.IsFlattenable())
      {
         numFlattenedEntries++;

         // Write entry name length
         uint32 keyNameSize = it.GetKey().FlattenedSize();
         networkByteOrder = B_HOST_TO_LENDIAN_INT32(keyNameSize);
         WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

         // Write entry name
         it.GetKey().Flatten(&buffer[writeOffset]);
         writeOffset += keyNameSize;

         // Write entry type code
         networkByteOrder = B_HOST_TO_LENDIAN_INT32(mf.TypeCode());
         WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));

         // Write entry data length
         uint32 dataSize = mf.FlattenedSize();
         networkByteOrder = B_HOST_TO_LENDIAN_INT32(dataSize);
         WriteData(buffer, &writeOffset, &networkByteOrder, sizeof(networkByteOrder));
         
         // Write entry data
         mf.Flatten(&buffer[writeOffset]);
         writeOffset += dataSize;
      }
   }

   // Write number-of-entries field (now that we know its final value)
   networkByteOrder = B_HOST_TO_LENDIAN_INT32(numFlattenedEntries);
   memcpy(entryCountPtr, &networkByteOrder, sizeof(uint32));
}

status_t Message :: Unflatten(const uint8 * buffer, uint32 inputBufferBytes) 
{
   TCHECKPOINT;

   Clear(true);

   uint32 readOffset = 0;
   
   // Read and check protocol version number
   uint32 networkByteOrder;
   if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Couldn't read message protocol version! (inputBufferBytes=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes);
      return B_ERROR;
   }

   uint32 messageProtocolVersion = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
   if ((messageProtocolVersion < OLDEST_SUPPORTED_PROTOCOL_VERSION)||(messageProtocolVersion > CURRENT_PROTOCOL_VERSION)) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unexpected message protocol version " UINT32_FORMAT_SPEC " (inputBufferBytes=" UINT32_FORMAT_SPEC ")\n", this, messageProtocolVersion, inputBufferBytes);
      return B_ERROR;
   }
   
   // Read 'what' code
   if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR)
   {
      LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Couldn't read what-code! (inputBufferBytes=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes);
      return B_ERROR;
   }
   what = B_LENDIAN_TO_HOST_INT32(networkByteOrder);

   // Read number of entries
   if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Couldn't read number-of-entries! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, what);
      return B_ERROR;
   }

   uint32 numEntries = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
   if (_entries.EnsureSize(numEntries, true) != B_NO_ERROR) return B_ERROR;

   // Read entries
   for (uint32 i=0; i<numEntries; i++)
   {
      // Read entry name length
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Error reading entry name length! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, what, i, numEntries);
         return B_ERROR;
      }
      uint32 nameLength = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if (nameLength > inputBufferBytes-readOffset) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Entry name length too long! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " nameLength=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, what, i, numEntries, nameLength, (uint32)(inputBufferBytes-readOffset));
         return B_ERROR;
      }

      // Read entry name
      String entryName;
      if (entryName.Unflatten(&buffer[readOffset], nameLength) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unable to unflatten entry name! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " nameLength=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, what, i, numEntries, nameLength);
         return B_ERROR;
      }
      readOffset += nameLength;

      // Read entry type code
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unable to read entry type code! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " entryName=[%s])\n", this, inputBufferBytes, what, i, numEntries, entryName());
         return B_ERROR;
      }
      uint32 tc = B_LENDIAN_TO_HOST_INT32(networkByteOrder);

      // Read entry data length
      if (ReadData(buffer, inputBufferBytes, &readOffset, &networkByteOrder, sizeof(networkByteOrder)) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unable to read data length! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " tc=" UINT32_FORMAT_SPEC " entryName=[%s])\n", this, inputBufferBytes, what, i, numEntries, tc, entryName());
         return B_ERROR;
      }
      uint32 eLength = B_LENDIAN_TO_HOST_INT32(networkByteOrder);
      if (eLength > inputBufferBytes-readOffset) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Data length is too long! (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " tc=" UINT32_FORMAT_SPEC " eLength=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " entryName=[%s])\n", this, inputBufferBytes, what, i, numEntries, tc, eLength, (uint32)(inputBufferBytes-readOffset), entryName());
         return B_ERROR;
      }
   
      MessageField * nextEntry = GetOrCreateMessageField(entryName, tc);
      if (nextEntry == NULL) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unable to create data field object!  (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " tc=" UINT32_FORMAT_SPEC " entryName=[%s])\n", this, inputBufferBytes, what, i, numEntries, tc, entryName());
         return B_ERROR;
      }

      if (nextEntry->Unflatten(&buffer[readOffset], eLength) != B_NO_ERROR) 
      {
         LogTime(MUSCLE_LOG_DEBUG, "Message %p:  Unable to unflatten data field object!  (inputBufferBytes=" UINT32_FORMAT_SPEC ", what=" UINT32_FORMAT_SPEC " i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC " tc=" UINT32_FORMAT_SPEC " entryName=[%s] eLength=" UINT32_FORMAT_SPEC ")\n", this, inputBufferBytes, what, i, numEntries, tc, entryName(), eLength);
         Clear();  // fix for occasional crash bug; we were deleting nextEntry here, *and* in the destructor!
         return B_ERROR;
      }
      readOffset += eLength;
   }
   return B_NO_ERROR;
}

status_t Message :: AddFlatAux(const String & fieldName, const FlatCountableRef & ref, uint32 tc, bool prepend)
{
   MessageField * field = ref() ? GetOrCreateMessageField(fieldName, tc) : NULL;
   return field ? (prepend ? field->PrependDataItem(&ref, sizeof(ref)) : field->AddDataItem(&ref, sizeof(ref))) : B_ERROR;
}

status_t Message :: AddString(const String & fieldName, const String & val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_STRING_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddInt8(const String & fieldName, int8 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT8_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddInt16(const String & fieldName, int16 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT16_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddInt32(const String & fieldName, int32 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT32_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddInt64(const String & fieldName, int64 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT64_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddBool(const String & fieldName, bool val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_BOOL_TYPE);
   status_t ret = mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
   return ret;
}

status_t Message :: AddFloat(const String & fieldName, float val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_FLOAT_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddDouble(const String & fieldName, double val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_DOUBLE_TYPE);
   return mf ? mf->AddDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: AddPointer(const String & fieldName, const void * ptr) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_POINTER_TYPE);
   return mf ? mf->AddDataItem(&ptr, sizeof(ptr)) : B_ERROR;
}

status_t Message :: AddPoint(const String & fieldName, const Point & point) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_POINT_TYPE);
   return mf ? mf->AddDataItem(&point, sizeof(point)) : B_ERROR;
}

status_t Message :: AddRect(const String & fieldName, const Rect & rect) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_RECT_TYPE);
   return mf ? mf->AddDataItem(&rect, sizeof(rect)) : B_ERROR;
}

status_t Message :: AddTag(const String & fieldName, const RefCountableRef & tag)
{
   if (tag() == NULL) return B_ERROR;
   MessageField * mf = GetOrCreateMessageField(fieldName, B_TAG_TYPE);
   return mf ? mf->AddDataItem(&tag, sizeof(tag)) : B_ERROR;
}

status_t Message :: AddMessage(const String & fieldName, const MessageRef & ref)
{
   if (ref() == NULL) return B_ERROR;
   MessageField * mf = GetOrCreateMessageField(fieldName, B_MESSAGE_TYPE);
   return (mf) ? mf->AddDataItem(&ref, sizeof(ref)) : B_ERROR;
}

status_t Message :: AddFlat(const String & fieldName, const FlatCountableRef & ref) 
{
   FlatCountable * fc = ref();
   if (fc)
   {
      uint32 tc = fc->TypeCode();
      switch(tc)
      {
         case B_STRING_TYPE:  return B_ERROR;  // sorry, can't do that (Strings aren't FlatCountables)
         case B_POINT_TYPE:   return B_ERROR;  // sorry, can't do that (Points aren't FlatCountables)
         case B_RECT_TYPE:    return B_ERROR;  // sorry, can't do that (Rects aren't FlatCountables)
         case B_MESSAGE_TYPE: return AddMessage(fieldName, MessageRef(ref.GetRefCountableRef(), true));
         default:             return AddFlatAux(fieldName, ref, tc, false);
      }
   }
   return B_ERROR;   
}

uint32 Message :: GetElementSize(uint32 type)
{
   switch(type)
   {
      case B_BOOL_TYPE:    return sizeof(bool);
      case B_DOUBLE_TYPE:  return sizeof(double);
      case B_POINTER_TYPE: return sizeof(void *);
      case B_POINT_TYPE:   return sizeof(Point);
      case B_RECT_TYPE:    return sizeof(Rect);
      case B_FLOAT_TYPE:   return sizeof(float);
      case B_INT64_TYPE:   return sizeof(int64);
      case B_INT32_TYPE:   return sizeof(int32);
      case B_INT16_TYPE:   return sizeof(int16);
      case B_INT8_TYPE:    return sizeof(int8);
      case B_MESSAGE_TYPE: return sizeof(MessageRef);
      case B_STRING_TYPE:  return sizeof(String);
      default:             return 0;
   }
}

status_t Message :: AddDataAux(const String & fieldName, const void * data, uint32 numBytes, uint32 tc, bool prepend)
{
   if (numBytes == 0) return B_ERROR;   // can't add 0 bytes, that's silly
   if (tc == B_STRING_TYPE) 
   {
      String temp((const char *)data);  // kept separate to avoid BeOS gcc optimizer bug (otherwise -O3 crashes here)
      return prepend ? PrependString(fieldName, temp) : AddString(fieldName, temp);
   }

   // for primitive types, we do this:
   bool isVariableSize = false;
   uint32 elementSize = GetElementSize(tc);
   if (elementSize == 0) 
   {
       // zero indicates a variable-sized data item; we will use a ByteBuffer to hold it.
       isVariableSize = true;
       elementSize    = numBytes;
       if (elementSize == 0) return B_ERROR;
   }
   if (numBytes % elementSize) return B_ERROR;  // Can't add half an element, silly!

   MessageField * mf = GetOrCreateMessageField(fieldName, tc);
   if (mf == NULL) return B_ERROR;

   uint32 numElements = numBytes/elementSize;
   const uint8 * dataBuf = (const uint8 *) data;
   for (uint32 i=0; i<numElements; i++) 
   {
      FlatCountableRef fcRef;
      const void * dataToAdd = dataBuf ? &dataBuf[i*elementSize] : NULL;
      uint32 addSize = elementSize;
      if (isVariableSize)
      {
         ByteBufferRef bufRef = GetByteBufferFromPool(elementSize, (const uint8 *)dataToAdd);
         if (bufRef() == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
         fcRef.SetFromRefCountableRef(bufRef.GetRefCountableRef());
         dataToAdd = &fcRef;
         addSize = sizeof(fcRef);
      }
      if ((prepend ? mf->PrependDataItem(dataToAdd, addSize) : mf->AddDataItem(dataToAdd, addSize)) != B_NO_ERROR) return B_ERROR;
   }
   return B_NO_ERROR;
}

void * Message :: GetPointerToNormalizedFieldData(const String & fieldName, uint32 * retNumItems, uint32 typeCode) 
{
   MessageField * e = GetMessageField(fieldName, typeCode);
   if (e)
   {
      e->Normalize();

      const void * ptr;
      if (e->FindDataItem(0, &ptr) == B_NO_ERROR)  // must be called AFTER e->Normalize()
      {
         if (retNumItems) *retNumItems = e->GetNumItems();
         return const_cast<void *>(ptr);
      }
   }
   return NULL;
}

status_t Message :: EnsureFieldIsPrivate(const String & fieldName) 
{
   MessageField * mf = GetMessageField(fieldName, B_ANY_TYPE);
   return mf ? mf->EnsurePrivate() : B_ERROR;
}

status_t Message :: RemoveData(const String & fieldName, uint32 index) 
{
   MessageField * mf = GetMessageField(fieldName, B_ANY_TYPE);
   if (mf) 
   {
      status_t ret = mf->RemoveDataItem(index);
      return mf->IsEmpty() ? RemoveName(fieldName) : ret;
   }
   else return B_ERROR;
}

status_t Message :: FindString(const String & fieldName, uint32 index, const char * & setMe) const
{
   const MessageField * mf = GetMessageField(fieldName, B_STRING_TYPE);
   if ((mf)&&(index < mf->GetNumItems()))
   {
      setMe = mf->GetItemAtAsString(index)();
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: FindString(const String & fieldName, uint32 index, const String ** setMe) const 
{
   const MessageField * mf = GetMessageField(fieldName, B_STRING_TYPE);
   if ((mf)&&(index < mf->GetNumItems()))
   {
      *setMe = &mf->GetItemAtAsString(index);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: FindString(const String & fieldName, uint32 index, String & str) const 
{
   const MessageField * mf = GetMessageField(fieldName, B_STRING_TYPE);
   if ((mf)&&(index < mf->GetNumItems()))
   {
      str = mf->GetItemAtAsString(index);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

const uint8 * Message :: FindFlatAux(const MessageField * mf, uint32 index, uint32 & retNumBytes, const FlatCountable ** optRetFCPtr) const
{
   if (optRetFCPtr) *optRetFCPtr = NULL;
   if (index >= mf->GetNumItems()) return NULL;

   RefCountableRef rcRef = mf->GetItemAtAsRefCountableRef(index);
   const ByteBuffer * bb = dynamic_cast<const ByteBuffer *>(rcRef());

   if (bb)
   {
      retNumBytes = bb->GetNumBytes();
      return bb->GetBuffer();
   }
   else
   {
      FlatCountable * fc = dynamic_cast<FlatCountable *>(rcRef());
      if (fc)
      {
         if (optRetFCPtr) (*optRetFCPtr) = fc;
         return NULL;
      }
   }

   const void * data; 
   status_t ret = mf->FindDataItem(index, &data);
   if (ret == B_NO_ERROR) 
   { 
      retNumBytes = mf->GetItemSize(index); 
      return (const uint8 *)data;
   }
   else return NULL;
}

status_t Message :: FindFlat(const String & fieldName, uint32 index, FlatCountableRef & ref) const
{
   TCHECKPOINT;

   const MessageField * field = GetMessageField(fieldName, B_ANY_TYPE);
   if ((field)&&(index < field->GetNumItems()))
   {
      RefCountableRef rcRef = field->GetItemAtAsRefCountableRef(index);
      ref.SetFromRefCountableRef(rcRef);
      if (ref()) return B_NO_ERROR;
   }
   return B_ERROR;
}

status_t Message :: FindData(const String & fieldName, uint32 tc, uint32 index, const void ** data, uint32 * setSize) const
{
   TCHECKPOINT;

   const MessageField * field = GetMessageField(fieldName, tc);
   if ((field)&&(field->FindDataItem(index, data) == B_NO_ERROR))
   {
      switch(tc)
      {
         case B_STRING_TYPE:  
         {
            const String * str = (const String *) (*data);
            *data = str->Cstr();  
            if (setSize) *setSize = str->FlattenedSize();
         }
         break;

         default:
         {
            uint32 es = GetElementSize(tc);
            if (es > 0) 
            {
               if (setSize) *setSize = es;
            }
            else
            {
               // But for user-generated types, we need to get a pointer to the actual data, not just the ref
               const FlatCountableRef * fcRef = (const FlatCountableRef *)(*data);
               const ByteBuffer * buf = dynamic_cast<ByteBuffer *>(fcRef->GetItemPointer());
               if (buf)
               {
                  const void * b = buf->GetBuffer();
                  if (b)
                  {
                     *data = b;
                     if (setSize) *setSize = buf->GetNumBytes();
                     return B_NO_ERROR;
                  }
               }
               return B_ERROR;
            }
         }
         break;
      }
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: FindDataItemAux(const String & fieldName, uint32 index, uint32 tc, void * setValue, uint32 valueSize) const
{
   const MessageField * field = GetMessageField(fieldName, tc);
   if (field == NULL) return B_ERROR;
   const void * addressOfValue;
   status_t ret = field->FindDataItem(index, &addressOfValue);
   if (ret != B_NO_ERROR) return ret;
   memcpy(setValue, addressOfValue, valueSize);
   return B_NO_ERROR;
}

status_t Message :: FindPoint(const String & fieldName, uint32 index, Point & point) const 
{
   const MessageField * field = GetMessageField(fieldName, B_POINT_TYPE);
   if ((field == NULL)||(index >= field->GetNumItems())) return B_ERROR;
   point = field->GetItemAtAsPoint(index);
   return B_NO_ERROR;
}

status_t Message :: FindRect(const String & fieldName, uint32 index, Rect & rect) const 
{
   const MessageField * field = GetMessageField(fieldName, B_RECT_TYPE);
   if ((field == NULL)||(index >= field->GetNumItems())) return B_ERROR;
   rect = field->GetItemAtAsRect(index);
   return B_NO_ERROR;
}

status_t Message :: FindTag(const String & fieldName, uint32 index, RefCountableRef & tag) const 
{
   const MessageField * field = GetMessageField(fieldName, B_TAG_TYPE);
   if ((field == NULL)||(index >= field->GetNumItems())) return B_ERROR;
   tag = field->GetItemAtAsRefCountableRef(index);
   return B_NO_ERROR;
}

status_t Message :: FindMessage(const String & fieldName, uint32 index, Message & msg) const 
{
   MessageRef msgRef;
   if (FindMessage(fieldName, index, msgRef) == B_NO_ERROR)
   {
      const Message * m = msgRef();
      if (m) 
      {
         msg = *m;
         return B_NO_ERROR;
      }
   }
   return B_ERROR;
}

status_t Message :: FindMessage(const String & fieldName, uint32 index, MessageRef & ref) const
{
   const MessageField * field = GetMessageField(fieldName, B_MESSAGE_TYPE);
   if ((field == NULL)||(index >= field->GetNumItems())) return B_ERROR;

   RefCountableRef rcRef = field->GetItemAtAsRefCountableRef(index);
   if (rcRef())
   {
      ref.SetFromRefCountableRef(rcRef);
      return ref() ? B_NO_ERROR : B_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: FindDataPointer(const String & fieldName, uint32 tc, uint32 index, void ** data, uint32 * setSize) const 
{
   const void * dataLoc;
   status_t ret = FindData(fieldName, tc, index, &dataLoc, setSize);
   if (ret == B_NO_ERROR)
   {
      *data = (void *) dataLoc;  // breaks const correctness, but oh well
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: ReplaceString(bool okayToAdd, const String & fieldName, uint32 index, const String & string) 
{
   MessageField * field = GetMessageField(fieldName, B_STRING_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddString(fieldName, string);
   return field ? field->ReplaceDataItem(index, &string, sizeof(string)) : B_ERROR;
}

status_t Message :: ReplaceFlatAux(bool okayToAdd, const String & fieldName, uint32 index, const ByteBufferRef & bufRef, uint32 tc) 
{
   FlatCountableRef fcRef; fcRef.SetFromRefCountableRefUnchecked(bufRef.GetRefCountableRef());
   return ReplaceDataAux(okayToAdd, fieldName, index, &fcRef, sizeof(fcRef), tc);
}

status_t Message :: ReplaceDataAux(bool okayToAdd, const String & fieldName, uint32 index, void * dataBuf, uint32 bufSize, uint32 tc)
{
   MessageField * field = GetMessageField(fieldName, tc);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddDataAux(fieldName, dataBuf, bufSize, tc, false);
   return field ? field->ReplaceDataItem(index, dataBuf, bufSize) : B_ERROR;
}

status_t Message :: ReplaceInt8(bool okayToAdd, const String & fieldName, uint32 index, int8 val) 
{
   MessageField * field = GetMessageField(fieldName, B_INT8_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddInt8(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceInt16(bool okayToAdd, const String & fieldName, uint32 index, int16 val) 
{
   MessageField * field = GetMessageField(fieldName, B_INT16_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddInt16(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceInt32(bool okayToAdd, const String & fieldName, uint32 index, int32 val) 
{
   MessageField * field = GetMessageField(fieldName, B_INT32_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddInt32(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceInt64(bool okayToAdd, const String & fieldName, uint32 index, int64 val) 
{
   MessageField * field = GetMessageField(fieldName, B_INT64_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddInt64(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceBool(bool okayToAdd, const String & fieldName, uint32 index, bool val) 
{
   MessageField * field = GetMessageField(fieldName, B_BOOL_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddBool(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceFloat(bool okayToAdd, const String & fieldName, uint32 index, float val) 
{
   MessageField * field = GetMessageField(fieldName, B_FLOAT_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddFloat(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplaceDouble(bool okayToAdd, const String & fieldName, uint32 index, double val) 
{
   MessageField * field = GetMessageField(fieldName, B_DOUBLE_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddDouble(fieldName, val);
   return field ? field->ReplaceDataItem(index, &val, sizeof(val)) : B_ERROR;
}

status_t Message :: ReplacePointer(bool okayToAdd, const String & fieldName, uint32 index, const void * ptr) 
{
   MessageField * field = GetMessageField(fieldName, B_POINTER_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddPointer(fieldName, ptr);
   return field ? field->ReplaceDataItem(index, &ptr, sizeof(ptr)) : B_ERROR;
}

status_t Message :: ReplacePoint(bool okayToAdd, const String & fieldName, uint32 index, const Point &point) 
{
   MessageField * field = GetMessageField(fieldName, B_POINT_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddPoint(fieldName, point);
   return field ? field->ReplaceDataItem(index, &point, sizeof(point)) : B_ERROR;
}

status_t Message :: ReplaceRect(bool okayToAdd, const String & fieldName, uint32 index, const Rect &rect) 
{
   MessageField * field = GetMessageField(fieldName, B_RECT_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddRect(fieldName, rect);
   return field ? field->ReplaceDataItem(index, &rect, sizeof(rect)) : B_ERROR;
}

status_t Message :: ReplaceTag(bool okayToAdd, const String & fieldName, uint32 index, const RefCountableRef & tag) 
{
   if (tag() == NULL) return B_ERROR;
   MessageField * field = GetMessageField(fieldName, B_TAG_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddTag(fieldName, tag);
   return field ? field->ReplaceDataItem(index, &tag, sizeof(tag)) : B_ERROR;
}

status_t Message :: ReplaceMessage(bool okayToAdd, const String & fieldName, uint32 index, const MessageRef & msgRef)
{
   if (msgRef() == NULL) return B_ERROR;
   MessageField * field = GetMessageField(fieldName, B_MESSAGE_TYPE);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddMessage(fieldName, msgRef);
   if (field) return field->ReplaceDataItem(index, &msgRef, sizeof(msgRef));
   return B_ERROR;
}

status_t Message :: ReplaceFlat(bool okayToAdd, const String & fieldName, uint32 index, const FlatCountableRef & ref) 
{
   const FlatCountable * fc = ref();
   if (fc)
   {
      uint32 tc = fc->TypeCode();
      MessageField * field = GetMessageField(fieldName, tc);
      if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddFlat(fieldName, ref);
      if (field)
      { 
         switch(tc)
         {
            case B_MESSAGE_TYPE:  
               return (dynamic_cast<const Message *>(fc)) ? ReplaceMessage(okayToAdd, fieldName, index, MessageRef(ref.GetRefCountableRef(), true)) : B_ERROR;

            default:              
               if (GetElementSize(tc) == 0) return field->ReplaceFlatCountableDataItem(index, ref);
            break;
         }
      }
   }
   return B_ERROR;
}

status_t Message :: ReplaceData(bool okayToAdd, const String & fieldName, uint32 type, uint32 index, const void * data, uint32 numBytes) 
{
   TCHECKPOINT;

   if (type == B_STRING_TYPE) 
   {   
      String temp((const char *)data);  // temp to avoid gcc optimizer bug
      return ReplaceString(okayToAdd, fieldName, index, temp);
   }
   MessageField * field = GetMessageField(fieldName, type);
   if ((okayToAdd)&&((field == NULL)||(index >= field->GetNumItems()))) return AddDataAux(fieldName, data, numBytes, type, false);
   if (field == NULL) return B_ERROR;
   
   // for primitive types, we do this:
   bool isVariableSize = false;
   uint32 elementSize = GetElementSize(type);
   if (elementSize == 0) 
   {
      // zero indicates a variable-sized data item
      isVariableSize = true;
      elementSize = numBytes;
      if (elementSize == 0) return B_ERROR;
   }

   if (numBytes % elementSize) return B_ERROR;  // Can't add half an element, silly!
   uint32 numElements = numBytes / elementSize;
   const uint8 * dataBuf = (const uint8 *) data;
   for (uint32 i=index; i<index+numElements; i++) 
   {
      FlatCountableRef ref;
      const void * dataToAdd = &dataBuf[i*elementSize];
      uint32 addSize = elementSize;
      if (isVariableSize)
      {
         ref.SetFromRefCountableRef(GetByteBufferFromPool(elementSize, (const uint8 *)dataToAdd).GetRefCountableRef());
         if (ref() == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
         dataToAdd = &ref;
         addSize = sizeof(ref);
      }
      if (field->ReplaceDataItem(i, dataToAdd, addSize) != B_NO_ERROR) return B_ERROR;
   }
   return B_NO_ERROR;
}

uint32 Message :: GetNumValuesInName(const String & fieldName, uint32 type) const
{
   const MessageField * field = GetMessageField(fieldName, type);
   return field ? field->GetNumItems() : 0;
}

status_t Message :: CopyName(const String & oldFieldName, Message & copyTo, const String & newFieldName) const
{
   if ((this == &copyTo)&&(oldFieldName == newFieldName)) return B_NO_ERROR;  // already done!

   const MessageField * mf = GetMessageField(oldFieldName, B_ANY_TYPE);
   MessageField * newMF = mf ? copyTo._entries.PutAndGet(newFieldName, *mf) : NULL;
   return newMF ? newMF->EnsurePrivate() : B_ERROR;
}

status_t Message :: ShareName(const String & oldFieldName, Message & shareTo, const String & newFieldName) const
{
   if ((this == &shareTo)&&(oldFieldName == newFieldName)) return B_NO_ERROR;  // already done!

   const MessageField * mf = GetMessageField(oldFieldName, B_ANY_TYPE);
   if (mf == NULL) return B_ERROR;

   // for non-array fields I'm falling back to copying rather than forcing a const violation
   return mf->HasArray() ? shareTo._entries.Put(newFieldName, *mf) : CopyName(oldFieldName, shareTo, newFieldName);
}

status_t Message :: MoveName(const String & oldFieldName, Message & moveTo, const String & newFieldName)
{
   if ((this == &moveTo)&&(oldFieldName == newFieldName)) return B_NO_ERROR;  // already done!

   const MessageField * mf = GetMessageField(oldFieldName, B_ANY_TYPE);
   if ((mf)&&(moveTo._entries.Put(newFieldName, *mf) == B_NO_ERROR))
   {
      (void) _entries.Remove(oldFieldName);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t Message :: PrependString(const String & fieldName, const String & val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_STRING_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependInt8(const String & fieldName, int8 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT8_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependInt16(const String & fieldName, int16 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT16_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependInt32(const String & fieldName, int32 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT32_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependInt64(const String & fieldName, int64 val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_INT64_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependBool(const String & fieldName, bool val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_BOOL_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependFloat(const String & fieldName, float val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_FLOAT_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependDouble(const String & fieldName, double val) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_DOUBLE_TYPE);
   return mf ? mf->PrependDataItem(&val, sizeof(val)) : B_ERROR;
}

status_t Message :: PrependPointer(const String & fieldName, const void * ptr) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_POINTER_TYPE);
   return mf ? mf->PrependDataItem(&ptr, sizeof(ptr)) : B_ERROR;
}

status_t Message :: PrependPoint(const String & fieldName, const Point & point) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_POINT_TYPE);
   return mf ? mf->PrependDataItem(&point, sizeof(point)) : B_ERROR;
}

status_t Message :: PrependRect(const String & fieldName, const Rect & rect) 
{
   MessageField * mf = GetOrCreateMessageField(fieldName, B_RECT_TYPE);
   return mf ? mf->PrependDataItem(&rect, sizeof(rect)) : B_ERROR;
}

status_t Message :: PrependTag(const String & fieldName, const RefCountableRef & tag) 
{
   if (tag() == NULL) return B_ERROR;
   MessageField * mf = GetOrCreateMessageField(fieldName, B_TAG_TYPE);
   return mf ? mf->PrependDataItem(&tag, sizeof(tag)) : B_ERROR;
}

status_t Message :: PrependMessage(const String & fieldName, const MessageRef & ref)
{
   if (ref() == NULL) return B_ERROR;
   MessageField * mf = GetOrCreateMessageField(fieldName, B_MESSAGE_TYPE);
   return mf ? mf->PrependDataItem(&ref, sizeof(ref)) : B_ERROR;
}

status_t Message :: PrependFlat(const String & fieldName, const FlatCountableRef & ref)
{
   FlatCountable * fc = ref();
   if (fc)
   {
      uint32 tc = fc->TypeCode();
      switch(tc)
      {
         case B_STRING_TYPE:  return B_ERROR;  // sorry, can't do that (Strings aren't FlatCountables)
         case B_POINT_TYPE:   return B_ERROR;  // sorry, can't do that (Strings aren't FlatCountables)
         case B_RECT_TYPE:    return B_ERROR;  // sorry, can't do that (Strings aren't FlatCountables)
         case B_MESSAGE_TYPE: return PrependMessage(fieldName, MessageRef(ref.GetRefCountableRef(), true)); 
         default:             return AddFlatAux(fieldName, ref, tc, true);
      }
   }
   return B_ERROR;   
}

status_t Message :: CopyFromImplementation(const Flattenable & copyFrom)
{
   const Message * cMsg = dynamic_cast<const Message *>(&copyFrom);
   if (cMsg)
   {
      *this = *cMsg;
      return B_NO_ERROR;
   }
   else return FlatCountable::CopyFromImplementation(copyFrom);
}

bool Message :: operator == (const Message & rhs) const
{
   return ((this == &rhs)||((what == rhs.what)&&(GetNumNames() == rhs.GetNumNames())&&(FieldsAreSubsetOf(rhs, true))));
}

bool Message :: FieldsAreSubsetOf(const Message & rhs, bool compareContents) const
{
   TCHECKPOINT;

   // Returns true iff every one of our fields has a like-named, liked-typed, equal-length field in (rhs).
   for (HashtableIterator<String, MessageField> iter(_entries, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      const MessageField * hisNextValue = rhs._entries.Get(iter.GetKey());
      if ((hisNextValue == NULL)||(iter.GetValue().IsEqualTo(*hisNextValue, compareContents) == false)) return false;
   }
   return true;
}

void Message :: SwapContents(Message & swapWith)
{
   muscleSwap(what, swapWith.what);
   _entries.SwapContents(swapWith._entries);
}

#define CONSTRUCT_DATA_TYPE(TheType) {(void) new (_union._data) TheType();}
#define  DESTRUCT_DATA_TYPE(TheType) {TheType * MUSCLE_MAY_ALIAS p = reinterpret_cast<TheType *>(_union._data); p->~TheType();}

void MessageField :: ChangeType(uint8 newType)
{
   if (newType != _dataType)
   {
      switch(_dataType)
      {
         case DATA_TYPE_POINT:  DESTRUCT_DATA_TYPE(Point);            break;
         case DATA_TYPE_RECT:   DESTRUCT_DATA_TYPE(Rect);             break;
         case DATA_TYPE_STRING: DESTRUCT_DATA_TYPE(String);           break;
         case DATA_TYPE_REF:    DESTRUCT_DATA_TYPE(RefCountableRef);  break;
         default:               /* empty */                           break;
      }
      _dataType = newType;
      switch(_dataType)
      {
         case DATA_TYPE_POINT:  CONSTRUCT_DATA_TYPE(Point);           break;
         case DATA_TYPE_RECT:   CONSTRUCT_DATA_TYPE(Rect);            break;
         case DATA_TYPE_STRING: CONSTRUCT_DATA_TYPE(String);          break;
         case DATA_TYPE_REF:    CONSTRUCT_DATA_TYPE(RefCountableRef); break;
         default:               /* empty */                           break;
      }
   }
}

// All methods below this point are assumed to be called only when the MessageField has no AbstractDataArray object instantiated

uint32 MessageField :: SingleFlattenedSize() const
{
   MASSERT(_state == FIELD_STATE_INLINE, "SingleFlattenedSize() called on empty field");

   if (_typeCode == B_BOOL_TYPE) return sizeof(uint8);                                // bools are always flattened to one uint8 each
   else
   {
      uint32 itemSizeBytes = SingleGetItemSize(0);
           if (_typeCode == B_MESSAGE_TYPE) return sizeof(uint32)+itemSizeBytes;  // special case: Message fields don't write number-of-items, for historical reasons
      else if (_typeCode == B_STRING_TYPE)  return sizeof(uint32)+sizeof(uint32)+GetInlineItemAsString().FlattenedSize();
      else
      {
         uint32 fixedSize = Message::GetElementSize(_typeCode);
         if (fixedSize > 0) return fixedSize;  // for fixed-size elements with known size, no headers are necessary

         // if we got here, then it's the one uint32 for the object-count, one uint32 for the size-of-object, plus the space for the object itself
         const FlatCountable * fc = dynamic_cast<FlatCountable *>(GetInlineItemAsRefCountableRef()());
         return fc ? (sizeof(uint32)+sizeof(uint32)+fc->FlattenedSize()) : 0;
      }
   }
}

void MessageField :: SingleFlatten(uint8 *buffer) const
{
   MASSERT(_state == FIELD_STATE_INLINE, "SingleFlatten() called on empty field");

   switch(_typeCode)
   {
      case B_BOOL_TYPE:    *buffer = (GetInlineItemAsBool() ? 1 : 0);                                                 break;
      case B_DOUBLE_TYPE:  {uint64 d = B_HOST_TO_LENDIAN_IDOUBLE(GetInlineItemAsDouble()); muscleCopyOut(buffer, d);} break;
      case B_FLOAT_TYPE:   {uint32 f = B_HOST_TO_LENDIAN_IFLOAT(GetInlineItemAsFloat());   muscleCopyOut(buffer, f);} break;
      case B_INT64_TYPE:   {uint64 i = B_HOST_TO_LENDIAN_INT64(GetInlineItemAsInt64());    muscleCopyOut(buffer, i);} break;
      case B_INT32_TYPE:   {uint32 i = B_HOST_TO_LENDIAN_INT32(GetInlineItemAsInt32());    muscleCopyOut(buffer, i);} break;
      case B_INT16_TYPE:   {uint16 i = B_HOST_TO_LENDIAN_INT16(GetInlineItemAsInt16());    muscleCopyOut(buffer, i);} break;
      case B_INT8_TYPE:    *buffer = GetInlineItemAsInt8();                                                           break;

      case B_MESSAGE_TYPE:
      {
         const Message * msg = dynamic_cast<Message *>(GetInlineItemAsRefCountableRef()());
         // Note:  No number-of-items field is written, for historical reasons
         uint32 leMsgSize = B_HOST_TO_LENDIAN_INT32(msg->FlattenedSize());
         muscleCopyOut(buffer, leMsgSize); buffer += sizeof(uint32);
         msg->Flatten(buffer);
      }
      break;

      case B_POINTER_TYPE: /* do nothing */ break;
      case B_POINT_TYPE:   GetInlineItemAsPoint().Flatten(buffer); break;
      case B_RECT_TYPE:    GetInlineItemAsRect().Flatten(buffer);  break;

      case B_STRING_TYPE:
      {
         uint32 leItemCount = B_HOST_TO_LENDIAN_INT32(1);  // because we have one string to write
         muscleCopyOut(buffer, leItemCount); buffer += sizeof(uint32);

         const String & s = GetInlineItemAsString();
         uint32 leFlatSize = B_HOST_TO_LENDIAN_INT32(s.FlattenedSize());
         muscleCopyOut(buffer, leFlatSize);  buffer += sizeof(uint32);
         s.Flatten(buffer); 
      }
      break;

      case B_TAG_TYPE:     /* do nothing */ break;

      default:
      {
         // all other types will follow the variable-sized-objects-field convention
         uint32 leItemCount = B_HOST_TO_LENDIAN_INT32(1);  // because we have one variable-sized-object to write
         muscleCopyOut(buffer, leItemCount); buffer += sizeof(uint32);

         const FlatCountable * fc = dynamic_cast<const FlatCountable *>(GetInlineItemAsRefCountableRef()());
         uint32 leFlatSize = B_HOST_TO_LENDIAN_INT32(fc ? fc->FlattenedSize() : 0);
         muscleCopyOut(buffer, leFlatSize); buffer += sizeof(uint32);

         if (fc) fc->Flatten(buffer);
      }
      break;
   }
}

// Note:  we assume here that we have enough bytes, at least for the fixed-size types, because we checked for that in MessageField::Unflatten() 
status_t MessageField :: SingleUnflatten(const uint8 * buffer, uint32 numBytes)
{
   switch(_typeCode)
   {
      case B_BOOL_TYPE:   SetInlineItemAsBool(  buffer[0] != 0);                                          break;
      case B_DOUBLE_TYPE: SetInlineItemAsDouble(B_LENDIAN_TO_HOST_IDOUBLE(muscleCopyIn<uint64>(buffer))); break;
      case B_FLOAT_TYPE:  SetInlineItemAsFloat( B_LENDIAN_TO_HOST_IFLOAT(muscleCopyIn<uint32>(buffer)));  break;
      case B_INT64_TYPE:  SetInlineItemAsInt64( B_LENDIAN_TO_HOST_INT64(muscleCopyIn<uint64>(buffer)));   break;
      case B_INT32_TYPE:  SetInlineItemAsInt32( B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer)));   break;
      case B_INT16_TYPE:  SetInlineItemAsInt16( B_LENDIAN_TO_HOST_INT16(muscleCopyIn<uint16>(buffer)));   break;
      case B_INT8_TYPE:   SetInlineItemAsInt8(  *buffer);                                                 break;

      case B_MESSAGE_TYPE:
      {
         if (numBytes < sizeof(uint32)) return B_ERROR;  // huh?

         // Note:  Message fields have no number-of-items field, for historical reasons
         uint32 msgSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer));
         buffer += sizeof(uint32); numBytes -= sizeof(uint32);  // this line must be exactly here!
         if (msgSize != numBytes) return B_ERROR;

         MessageRef msgRef = GetMessageFromPool(buffer, numBytes);
         if (msgRef() == NULL) return B_ERROR;
         SetInlineItemAsRefCountableRef(msgRef.GetRefCountableRef());
      }
      break;

      case B_POINTER_TYPE: return B_ERROR;  // pointers should not be serialized!
      case B_POINT_TYPE:   {Point p; if (p.Unflatten(buffer, numBytes) == B_NO_ERROR) SetInlineItemAsPoint(p); else return B_ERROR;} break;
      case B_RECT_TYPE:    {Rect  r; if (r.Unflatten(buffer, numBytes) == B_NO_ERROR) SetInlineItemAsRect (r); else return B_ERROR;} break;

      case B_STRING_TYPE:
      {
         if (numBytes < sizeof(uint32)) return B_ERROR;  // paranoia

         // string type follows the variable-sized-objects-field convention
         uint32 itemCount = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer));
         if (itemCount != 1) return B_ERROR;  // wtf, if we're in this function there should only be one item!
         buffer += sizeof(uint32); numBytes -= sizeof(uint32);

         uint32 itemSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer));
         buffer += sizeof(uint32); numBytes -= sizeof(uint32);  // yes, this line MUST be exactly here!
         if (itemSize != numBytes) return B_ERROR;  // our one item should take up the entire buffer, or something is wrong

         String s;
         if (s.Unflatten(buffer, numBytes) != B_NO_ERROR) return B_ERROR;
         SetInlineItemAsString(s);
      }
      break;

      case B_TAG_TYPE:     return B_ERROR;  // tags should not be serialized!

      default:
      {
         if (numBytes < sizeof(uint32)) return B_ERROR;  // paranoia

         // all other types will follow the variable-sized-objects-field convention
         uint32 itemCount = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer));
         if (itemCount != 1) return B_ERROR;  // wtf, if we're in this function there should only be one item!
         buffer += sizeof(uint32); numBytes -= sizeof(uint32);

         uint32 itemSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(buffer));
         buffer += sizeof(uint32); numBytes -= sizeof(uint32);  // yes, this line MUST be exactly here!
         if (itemSize != numBytes) return B_ERROR;  // our one item should take up the entire buffer, or something is wrong

         ByteBufferRef bbRef = GetByteBufferFromPool(numBytes, buffer);
         if (bbRef() == NULL) return B_ERROR;

         SetInlineItemAsRefCountableRef(bbRef.GetRefCountableRef());
      }
      break;
   }

   _state = FIELD_STATE_INLINE;
   return B_NO_ERROR;
}

void MessageField :: SingleSetValue(const void * data, uint32 /*size*/)
{
   _state = FIELD_STATE_INLINE;
   switch(_typeCode)
   {
      case B_BOOL_TYPE:    SetInlineItemAsBool(   *static_cast<const bool          *>(data)); break;
      case B_DOUBLE_TYPE:  SetInlineItemAsDouble( *static_cast<const double        *>(data)); break;
      case B_FLOAT_TYPE:   SetInlineItemAsFloat(  *static_cast<const float         *>(data)); break;
      case B_INT64_TYPE:   SetInlineItemAsInt64(  *static_cast<const int64         *>(data)); break;
      case B_INT32_TYPE:   SetInlineItemAsInt32(  *static_cast<const int32         *>(data)); break;
      case B_INT16_TYPE:   SetInlineItemAsInt16(  *static_cast<const int16         *>(data)); break;
      case B_INT8_TYPE:    SetInlineItemAsInt8(   *static_cast<const int8          *>(data)); break;
      case B_POINTER_TYPE: SetInlineItemAsPointer(*static_cast<const MFVoidPointer *>(data)); break;
      case B_POINT_TYPE:   SetInlineItemAsPoint(  *static_cast<const Point         *>(data)); break;
      case B_RECT_TYPE:    SetInlineItemAsRect(   *static_cast<const Rect          *>(data)); break;
      case B_STRING_TYPE:  SetInlineItemAsString( *static_cast<const String        *>(data)); break;

      case B_MESSAGE_TYPE: case B_TAG_TYPE: default:
         SetInlineItemAsRefCountableRef(*static_cast<const RefCountableRef *>(data));
      break;
   }
}

status_t MessageField :: SingleAddDataItem(const void * data, uint32 size)
{
   if (_state == FIELD_STATE_EMPTY)
   {
      _state = FIELD_STATE_INLINE;
      SingleSetValue(data, size);
      return B_NO_ERROR; 
   }
   else
   {
      // Oops, we need to allocate an array now!
      AbstractDataArrayRef adaRef = CreateDataArray(_typeCode);
      if ((adaRef())&&(adaRef()->AddDataItem(_union._data, size) == B_NO_ERROR))  // add our existing single-item to the array
      {
         if (adaRef()->AddDataItem(data, size) == B_NO_ERROR)
         {
            _state = FIELD_STATE_ARRAY;
            SetInlineItemAsRefCountableRef(adaRef.GetRefCountableRef());
            return B_NO_ERROR;
         } 
      }
      return B_ERROR;
   }
}

status_t MessageField :: SingleRemoveDataItem(uint32 index)
{
   if ((index > 0)||(_state != FIELD_STATE_INLINE)) return B_ERROR;

   SetInlineItemToNull();
   _state = FIELD_STATE_EMPTY;
   return B_NO_ERROR;
}

status_t MessageField :: SinglePrependDataItem(const void * data, uint32 size)
{
   if (_state == FIELD_STATE_EMPTY)
   {
      _state = FIELD_STATE_INLINE;
      SingleSetValue(data, size);
      return B_NO_ERROR; 
   }
   else
   {
      // Oops, we need to allocate an array now!
      AbstractDataArrayRef adaRef = CreateDataArray(_typeCode);
      if ((adaRef())&&(adaRef()->AddDataItem(_union._data, size) == B_NO_ERROR))  // add our existing single-item to the array
      {
         if (adaRef()->PrependDataItem(data, size) == B_NO_ERROR)
         {
            _state = FIELD_STATE_ARRAY;
            SetInlineItemAsRefCountableRef(adaRef.GetRefCountableRef());
            return B_NO_ERROR;
         } 
      }
      return B_ERROR;
   }
}

status_t MessageField :: SingleFindDataItem(uint32 index, const void ** setDataLoc) const
{
   if ((_state != FIELD_STATE_INLINE)||(index > 0)) return B_ERROR;

   *setDataLoc = _union._data;
   return B_NO_ERROR;
}

status_t MessageField :: SingleReplaceDataItem(uint32 index, const void * data, uint32 size)
{
   if ((_state != FIELD_STATE_INLINE)||(index > 0)) return B_ERROR;
   
   SingleSetValue(data, size);
   return B_NO_ERROR;
}

// For a given absolutely-fixed-size-type, returns the number of bytes that this type will flatten to.
// For any other types, returns 0.
static uint32 GetFlattenedSizeForFixedSizeType(uint32 typeCode)
{
   switch(typeCode)
   {
      case B_BOOL_TYPE:    return 1;  // note:  NOT sizeof(bool)!
      case B_DOUBLE_TYPE:  return sizeof(double);
      case B_POINTER_TYPE: return sizeof(void *);   // pointer-fields are not flattened anyway, but for completeness
      case B_POINT_TYPE:   return 2*sizeof(float);
      case B_RECT_TYPE:    return 4*sizeof(float);
      case B_FLOAT_TYPE:   return sizeof(float);
      case B_INT64_TYPE:   return sizeof(int64);
      case B_INT32_TYPE:   return sizeof(int32);
      case B_INT16_TYPE:   return sizeof(int16);
      case B_INT8_TYPE:    return sizeof(int8);
      default:             return 0;
   }
}

uint32 MessageField :: SingleGetItemSize(uint32 index) const
{
   if ((_state == FIELD_STATE_EMPTY)||(index > 0)) return 0;  // no valid items to get the size of!

   switch(_typeCode)
   {
      case B_BOOL_TYPE:    return sizeof(bool);  // note: may be larger than 1, depending on the compiler!
      case B_DOUBLE_TYPE:  return sizeof(double);
      case B_POINTER_TYPE: return sizeof(void *);
      case B_POINT_TYPE:   return sizeof(Point);
      case B_RECT_TYPE:    return sizeof(Rect);
      case B_FLOAT_TYPE:   return sizeof(float);
      case B_INT64_TYPE:   return sizeof(int64);
      case B_INT32_TYPE:   return sizeof(int32);
      case B_INT16_TYPE:   return sizeof(int16);
      case B_INT8_TYPE:    return sizeof(int8);
      case B_STRING_TYPE:  return GetInlineItemAsString().FlattenedSize();

      default:
      {
         const FlatCountable * fc = dynamic_cast<FlatCountable *>(GetInlineItemAsRefCountableRef()());
         return fc ? fc->FlattenedSize() : 0; 
      }
   }
}

uint32 MessageField :: SingleCalculateChecksum(bool countNonFlattenableFields) const
{
   MASSERT(_state == FIELD_STATE_INLINE, "SingleCalculateChecksum() called on empty field");

   uint32 ret = _typeCode + 1;  // +1 is for the one item we have
   switch(_typeCode)
   {
      case B_BOOL_TYPE:    ret += (uint32) (GetInlineItemAsBool() ? 1 : 0);            break;
      case B_DOUBLE_TYPE:  ret += CalculateChecksumForDouble(GetInlineItemAsDouble()); break;
      case B_FLOAT_TYPE:   ret += CalculateChecksumForFloat(GetInlineItemAsFloat());   break;
      case B_INT64_TYPE:   ret += CalculateChecksumForUint64(GetInlineItemAsInt64());  break;
      case B_INT32_TYPE:   ret += (uint32) GetInlineItemAsInt32();                     break;
      case B_INT16_TYPE:   ret += (uint32) GetInlineItemAsInt16();                     break;
      case B_INT8_TYPE:    ret += (uint32) GetInlineItemAsInt8();                      break;
      case B_MESSAGE_TYPE: ret += static_cast<const Message *>(GetInlineItemAsRefCountableRef()())->CalculateChecksum(countNonFlattenableFields); break;
      case B_POINTER_TYPE: /* do nothing */;                                           break;
      case B_POINT_TYPE:   ret += GetInlineItemAsPoint().CalculateChecksum();          break;
      case B_RECT_TYPE:    ret += GetInlineItemAsRect().CalculateChecksum();           break;
      case B_STRING_TYPE:  ret += GetInlineItemAsString().CalculateChecksum();         break;
      case B_TAG_TYPE:     /* do nothing */ break;

      default:
      {
         const ByteBuffer * bb = dynamic_cast<const ByteBuffer *>(GetInlineItemAsRefCountableRef()());
         if (bb) ret += bb->CalculateChecksum();
      }
      break;
   }

   return ret;
}

bool MessageField :: SingleElementsAreFixedSize() const
{
   switch(_typeCode)
   {
      case B_BOOL_TYPE:
      case B_DOUBLE_TYPE:
      case B_POINTER_TYPE:
      case B_POINT_TYPE:
      case B_RECT_TYPE:
      case B_FLOAT_TYPE:
      case B_INT64_TYPE:
      case B_INT32_TYPE:
      case B_INT16_TYPE:
      case B_INT8_TYPE:
         return true;

      default:
         return false;
   }
}

bool MessageField :: SingleIsFlattenable() const
{
   return ((_typeCode != B_TAG_TYPE)&&(_typeCode != B_POINTER_TYPE));
}

static void AddSingleItemToString(uint32 indent, const String & itemStr, String & s)
{
   AddItemPreambleToString(indent, 0, s);
   s += itemStr;
   s += '\n'; 
}

template <typename T> void AddFormattedSingleItemToString(uint32 indent, const char * fmt, T val, String & s)
{
   char buf[64]; muscleSprintf(buf, fmt, val);
   AddSingleItemToString(indent, buf, s); 
}

void MessageField :: AddToString(String & s, uint32 maxRecurseLevel, int indent) const 
{
   if (HasArray()) GetArray()->AddToString(s, maxRecurseLevel, indent); 
              else SingleAddToString(s, maxRecurseLevel, indent);
}

void MessageField :: SingleAddToString(String & s, uint32 maxRecurseLevel, int indent) const
{
   if (_state == FIELD_STATE_INLINE)  // paranoia
   {
      switch(_typeCode)
      {
         case B_BOOL_TYPE:    AddFormattedSingleItemToString(indent, "[%i]", GetInlineItemAsBool(),                     s); break;
         case B_DOUBLE_TYPE:  AddFormattedSingleItemToString(indent, "[%f]", GetInlineItemAsDouble(),                   s); break;
         case B_FLOAT_TYPE:   AddFormattedSingleItemToString(indent, "[%f]", GetInlineItemAsFloat(),                    s); break;
         case B_INT64_TYPE:   AddFormattedSingleItemToString(indent, "[" INT64_FORMAT_SPEC "]", GetInlineItemAsInt64(), s); break;
         case B_INT32_TYPE:   AddFormattedSingleItemToString(indent, "[" INT32_FORMAT_SPEC "]", GetInlineItemAsInt32(), s); break;
         case B_INT16_TYPE:   AddFormattedSingleItemToString(indent, "[%i]", GetInlineItemAsInt16(),                    s); break;
         case B_INT8_TYPE:    AddFormattedSingleItemToString(indent, "[%i]", GetInlineItemAsInt8(),                     s); break;

         case B_MESSAGE_TYPE: 
            MessageDataArray::AddItemDescriptionToString(indent, 0, MessageRef(GetInlineItemAsRefCountableRef(), false), s, maxRecurseLevel);
         break;

         case B_POINTER_TYPE: AddFormattedSingleItemToString(indent, "[%p]", GetInlineItemAsPointer(), s);        break;
         case B_POINT_TYPE:   AddSingleItemToString(indent, PointToString(GetInlineItemAsPoint()), s);            break;
         case B_RECT_TYPE:    AddSingleItemToString(indent, RectToString(GetInlineItemAsRect()), s);              break;
         case B_STRING_TYPE:  AddSingleItemToString(indent, GetInlineItemAsString().Prepend("[").Append("]"), s); break;

         default:
         {
            const ByteBuffer * bb = dynamic_cast<const ByteBuffer *>(GetInlineItemAsRefCountableRef()());
            if (bb) ByteBufferDataArray::AddItemDescriptionToString(indent, 0, FlatCountableRef(GetInlineItemAsRefCountableRef(), true), s);
               else AddFormattedSingleItemToString(indent, "%p", GetInlineItemAsRefCountableRef()(), s);
         }
         break;
      }
   }
}

// If they are byte-buffers, we'll compare the contents, otherwise we can only compare the pointers
static bool CompareRefCountableRefs(const RefCountableRef & myRCR, const RefCountableRef & hisRCR)
{
   const ByteBuffer * myBB  = dynamic_cast<const ByteBuffer *>(myRCR());
   const ByteBuffer * hisBB = dynamic_cast<const ByteBuffer *>(hisRCR());
   return ((myBB)&&(hisBB)) ? ((*myBB) == (*hisBB)) : (myRCR == hisRCR);
}

bool MessageField :: IsEqualTo(const MessageField & rhs, bool compareContents) const
{
   if (_typeCode != rhs._typeCode) return false;

   uint32 mySize  = GetNumItems();
   uint32 hisSize = GetNumItems();
   if (mySize != hisSize) return false;  // can't be equal if we don't have the same sizes
   if (mySize == 0)       return true;   // no more to compare, if we're both empty
   if (compareContents == false) return true;  // if we're not comparing contents, then that's all we need to check

   switch(_state)
   {
      case FIELD_STATE_INLINE:
         switch(rhs._state)
         {
            case FIELD_STATE_INLINE:
               // Case:  I'm inline, he's inline
               switch(_typeCode)
               {
                  case B_BOOL_TYPE:    return (GetInlineItemAsBool()   == rhs.GetInlineItemAsBool());
                  case B_DOUBLE_TYPE:  return (GetInlineItemAsDouble() == rhs.GetInlineItemAsDouble());
                  case B_FLOAT_TYPE:   return (GetInlineItemAsFloat()  == rhs.GetInlineItemAsFloat());
                  case B_INT64_TYPE:   return (GetInlineItemAsInt64()  == rhs.GetInlineItemAsInt64());
                  case B_INT32_TYPE:   return (GetInlineItemAsInt32()  == rhs.GetInlineItemAsInt32());
                  case B_INT16_TYPE:   return (GetInlineItemAsInt16()  == rhs.GetInlineItemAsInt16());
                  case B_INT8_TYPE:    return (GetInlineItemAsInt8()   == rhs.GetInlineItemAsInt8());
 
                  case B_MESSAGE_TYPE:
                  {
                     const Message * myMsg  = dynamic_cast<Message *>(GetInlineItemAsRefCountableRef()());
                     const Message * hisMsg = dynamic_cast<Message *>(rhs.GetInlineItemAsRefCountableRef()());
                     return AreMessagePointersEqual(myMsg, hisMsg);
                  }

                  case B_POINTER_TYPE: return (GetInlineItemAsPointer() == rhs.GetInlineItemAsPointer());
                  case B_POINT_TYPE:   return (GetInlineItemAsPoint()   == rhs.GetInlineItemAsPoint());
                  case B_RECT_TYPE:    return (GetInlineItemAsRect()    == rhs.GetInlineItemAsRect());
                  case B_STRING_TYPE:  return (GetInlineItemAsString()  == rhs.GetInlineItemAsString());
                  case B_TAG_TYPE:     // fall through!
                  default:             return (GetInlineItemAsRefCountableRef() == rhs.GetInlineItemAsRefCountableRef());
               }
            break;

            case FIELD_STATE_ARRAY:
            {
               // Case:  I'm inline, he's array
               const void * hisData;
               if (rhs.GetArray()->FindDataItem(0, &hisData) != B_NO_ERROR) return false;  // semi-paranoia: this call should never fail

               switch(_typeCode)
               {
                  case B_BOOL_TYPE:    return (GetInlineItemAsBool()   == *(static_cast<const bool   *>(hisData)));
                  case B_DOUBLE_TYPE:  return (GetInlineItemAsDouble() == *(static_cast<const double *>(hisData)));
                  case B_FLOAT_TYPE:   return (GetInlineItemAsFloat()  == *(static_cast<const float  *>(hisData)));
                  case B_INT64_TYPE:   return (GetInlineItemAsInt64()  == *(static_cast<const int64  *>(hisData)));
                  case B_INT32_TYPE:   return (GetInlineItemAsInt32()  == *(static_cast<const int32  *>(hisData)));
                  case B_INT16_TYPE:   return (GetInlineItemAsInt16()  == *(static_cast<const int16  *>(hisData)));
                  case B_INT8_TYPE:    return (GetInlineItemAsInt8()   == *(static_cast<const int8   *>(hisData)));
 
                  case B_MESSAGE_TYPE:
                  {
                     const Message * myMsg  = dynamic_cast<Message *>(GetInlineItemAsRefCountableRef()());
                     const Message * hisMsg = dynamic_cast<Message *>((*(static_cast<const RefCountableRef *>(hisData)))());
                     return AreMessagePointersEqual(myMsg, hisMsg);
                  }
                  // no break necessary since we never get here

                  case B_POINTER_TYPE: return (GetInlineItemAsPointer() == *(static_cast<const MFVoidPointer *>(hisData)));
                  case B_POINT_TYPE:   return (GetInlineItemAsPoint()   == *(static_cast<const Point  *>(hisData)));
                  case B_RECT_TYPE:    return (GetInlineItemAsRect()    == *(static_cast<const Rect   *>(hisData)));
                  case B_STRING_TYPE:  return (GetInlineItemAsString()  == *(static_cast<const String *>(hisData)));
                  case B_TAG_TYPE:     // fall through!
                  default:             return CompareRefCountableRefs(GetInlineItemAsRefCountableRef(), *(static_cast<const RefCountableRef *>(hisData)));
               }
            }
            break;

            default:
               MCRASH("MessageField::IsEqualTo():  Bad _state B!");
            break;
         }
      break;

      case FIELD_STATE_ARRAY:
              if (rhs.HasArray())                   return GetArray()->IsEqualTo(rhs.GetArray(), compareContents);
         else if (rhs._state == FIELD_STATE_INLINE) return rhs.IsEqualTo(*this, compareContents);  // no sense duplicating code
         // fall through
      default:
         MCRASH("MessageField::IsEqualTo():  Bad _state C!");
      break;
   }

   return false;  // we should never get here anyway, but having this avoids a compiler warning
}

MessageField & MessageField :: operator = (const MessageField & rhs)
{
   // First, get rid of any existing state we are holding
   SetInlineItemToNull();
   _state = FIELD_STATE_EMPTY;
   
   _typeCode = rhs._typeCode;
   switch(rhs._state)
   {
      case FIELD_STATE_EMPTY:  /* do nothing */                                                      break;
      case FIELD_STATE_INLINE: SingleSetValue(rhs._union._data, Message::GetElementSize(_typeCode)); break;
      case FIELD_STATE_ARRAY:  
         _state = FIELD_STATE_ARRAY;
         SetInlineItemAsRefCountableRef(rhs.GetInlineItemAsRefCountableRef());   // note array is ref-shared at this point!
      break;
   }
   return *this;
}

AbstractDataArrayRef MessageField :: CreateDataArray(uint32 typeCode) const
{
   AbstractDataArrayRef ada;
   switch(typeCode)
   {
      case B_BOOL_TYPE:    ada.SetRef(NEWFIELD(BoolDataArray));    break;
      case B_DOUBLE_TYPE:  ada.SetRef(NEWFIELD(DoubleDataArray));  break;
      case B_POINTER_TYPE: ada.SetRef(NEWFIELD(PointerDataArray)); break;
      case B_POINT_TYPE:   ada.SetRef(NEWFIELD(PointDataArray));   break;
      case B_RECT_TYPE:    ada.SetRef(NEWFIELD(RectDataArray));    break;
      case B_FLOAT_TYPE:   ada.SetRef(NEWFIELD(FloatDataArray));   break;
      case B_INT64_TYPE:   ada.SetRef(NEWFIELD(Int64DataArray));   break;
      case B_INT32_TYPE:   ada.SetRef(NEWFIELD(Int32DataArray));   break;
      case B_INT16_TYPE:   ada.SetRef(NEWFIELD(Int16DataArray));   break;
      case B_INT8_TYPE:    ada.SetRef(NEWFIELD(Int8DataArray));    break;
      case B_MESSAGE_TYPE: ada.SetRef(NEWFIELD(MessageDataArray)); break;
      case B_STRING_TYPE:  ada.SetRef(NEWFIELD(StringDataArray));  break;
      case B_TAG_TYPE:     ada.SetRef(NEWFIELD(TagDataArray));     break;
      default:
         ada.SetRef(NEWFIELD(ByteBufferDataArray));
         if (ada()) (static_cast<ByteBufferDataArray*>(ada()))->SetTypeCode(typeCode);
         break;
   }
   return ada;
}

status_t MessageField :: EnsurePrivate()
{
   switch(_state)
   {
      case FIELD_STATE_ARRAY:
         if (GetArrayRef().IsRefPrivate() == false)
         {
            AbstractDataArrayRef newArrayCopy = GetArray()->Clone();
            if (newArrayCopy() == NULL) return B_ERROR;
            SetInlineItemAsRefCountableRef(newArrayCopy.GetRefCountableRef());
         }
      break;

      case FIELD_STATE_INLINE:
         if (_dataType == DATA_TYPE_REF)
         {
            const RefCountableRef & rcRef = GetInlineItemAsRefCountableRef();
            if ((rcRef())&&(GetArrayRef().IsRefPrivate() == false))
            {
               const Message * msg = dynamic_cast<const Message *>(rcRef());
               if (msg)
               {
                  MessageRef newMsg = GetMessageFromPool(*msg);
                  if (newMsg() == NULL) return B_ERROR;
                  SetInlineItemAsRefCountableRef(newMsg.GetRefCountableRef());
               }
               else
               {
                  const FlatCountable * fc = dynamic_cast<const FlatCountable *>(rcRef());
                  if (fc)
                  {
                     ByteBufferRef newBuf = fc->FlattenToByteBuffer();
                     if (newBuf() == NULL) return B_ERROR;
                     SetInlineItemAsRefCountableRef(newBuf.GetRefCountableRef());
                  }
               }
            }
         }         
      break;

      default:
         // do nothing
      break;
   }

   return B_NO_ERROR;
}

status_t MessageField :: ReplaceFlatCountableDataItem(uint32 index, muscle::Ref<muscle::FlatCountable> const & fcRef)
{
   switch(_state)
   {
      case FIELD_STATE_INLINE:
         SetInlineItemAsRefCountableRef(fcRef.GetRefCountableRef());
      break;

      case FIELD_STATE_ARRAY:
      {
         // Note that I don't check for MessageDataArray here since it's already been handled in the calling method
         ByteBufferDataArray * bbda = dynamic_cast<ByteBufferDataArray *>(GetArray());
         if (bbda) 
         {
            ByteBufferRef bbRef(fcRef.GetRefCountableRef(), true);
            return bbRef() ? bbda->ReplaceDataItem(index, &bbRef, sizeof(bbRef)) : B_ERROR;
         }

         TagDataArray * tda = dynamic_cast<TagDataArray *>(GetArray());
         if (tda) 
         {
            RefCountableRef rcRef = fcRef.GetRefCountableRef();
            return tda->ReplaceDataItem(index, &rcRef, sizeof(rcRef));
         }
      }
      break;
   }
   return B_ERROR;
}

uint32 MessageField :: GetNumItemsInFlattenedBuffer(const uint8 * bytes, uint32 numBytes) const
{
   uint32 fsItemSize = GetFlattenedSizeForFixedSizeType(_typeCode);

        if (fsItemSize > 0) return numBytes/fsItemSize;
   else if (_typeCode == B_MESSAGE_TYPE)
   {
      // special case for the Message type since it doesn't have a number-of-items-count, annoyingly enough
      if (numBytes < sizeof(uint32)) return 0;  // huh?
      uint32 firstMsgSize = B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(bytes));
      bytes += sizeof(uint32); numBytes -= sizeof(uint32);  // this must be exactly here!
      if (firstMsgSize > numBytes) return 0;      // malformed buffer size?
      return (firstMsgSize == numBytes) ? 1 : 2;  // we don't need to count the actual number of Messages for now
   }
   else
   {
      // For all other types, the first four bytes in the buffer is the number-of-items-count
      if (numBytes < sizeof(uint32)) return 0;  // huh?
      return B_LENDIAN_TO_HOST_INT32(muscleCopyIn<uint32>(bytes));
   }
}

status_t MessageField :: Unflatten(const uint8 * bytes, uint32 numBytes)
{
   _state = FIELD_STATE_EMPTY;  // semi-paranoia
   SetInlineItemToNull();       // ditto

   uint32 numItemsInBuffer = GetNumItemsInFlattenedBuffer(bytes, numBytes);
   if (numItemsInBuffer == 1) return SingleUnflatten(bytes, numBytes);
   else
   {
      AbstractDataArrayRef adaRef = CreateDataArray(_typeCode);
      if ((adaRef())&&(adaRef()->Unflatten(bytes, numBytes) == B_NO_ERROR))  // add our existing single-item to the array
      {
         _state = FIELD_STATE_ARRAY;
         SetInlineItemAsRefCountableRef(adaRef.GetRefCountableRef());
         return B_NO_ERROR;
      }
   }

   return B_ERROR;
}

const Rect & MessageField :: GetItemAtAsRect(uint32 index) const
{
   switch(_state)
   {
      case FIELD_STATE_ARRAY:
      {
         const RectDataArray * rda = dynamic_cast<const RectDataArray *>(GetArray());
         if (rda) return rda->ItemAt(index);
      }
      break;

      case FIELD_STATE_INLINE: return GetInlineItemAsRect();
      default:                 /* do nothing */ break;
   }
   return GetDefaultObjectForType<Rect>();
}

const Point & MessageField :: GetItemAtAsPoint(uint32 index) const
{
   switch(_state)
   {
      case FIELD_STATE_ARRAY:
      {
         const PointDataArray * pda = dynamic_cast<const PointDataArray *>(GetArray());
         if (pda) return pda->ItemAt(index);
      }
      break;

      case FIELD_STATE_INLINE: return GetInlineItemAsPoint();
      default:                 /* do nothing */ break;
   }
   return GetDefaultObjectForType<Point>();
}

const String & MessageField :: GetItemAtAsString(uint32 index) const
{
   switch(_state)
   {
      case FIELD_STATE_ARRAY:
      {
         const StringDataArray * sda = dynamic_cast<const StringDataArray *>(GetArray());
         if (sda) return sda->ItemAt(index);
      }
      break;

      case FIELD_STATE_INLINE: return GetInlineItemAsString();
      default:                 /* do nothing */ break;
   }
   return GetDefaultObjectForType<String>();
}

RefCountableRef MessageField :: GetItemAtAsRefCountableRef(uint32 index) const
{
   switch(_state)
   {
      case FIELD_STATE_ARRAY:  return GetArray()->GetItemAtAsRefCountableRef(index);
      case FIELD_STATE_INLINE: return GetInlineItemAsRefCountableRef();
      default:                 /* do nothing */ break;
   }
   return GetDefaultObjectForType<RefCountableRef>();
}

void MessageField :: Clear() 
{
   SetInlineItemToNull();
   _state = FIELD_STATE_EMPTY;
}

}; // end namespace muscle
