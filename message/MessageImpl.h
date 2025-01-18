/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

// This header contains private implementation details of the Message class.  Do not include this header in your
// code and do not reference any of the classes inside it, as they are private and subject to change without notice.

#ifndef MuscleMessage_impl_h
#define MuscleMessage_impl_h

#ifndef MuscleMessage_h
# error "This is a private header file, and no other file except Message.h should include it!"
#endif

#include "support/Point.h"
#include "support/Rect.h"
#include "util/ByteBuffer.h"
#include "util/String.h"
#include "util/Hashtable.h"
#include "util/OutputPrinter.h"

namespace muscle {

class Message;

/** The muscle_private namespace contains implementation details that are for MUSCLE's internal use only.
  * User code should not use or reference anything inside the muscle_private namespace, as it its contents
  * are all subject to change without notice at any time.
  */
namespace muscle_private {

/** This class is a private part of the Message class's implementation.  User code should not access this class directly.
  * It is used to hold the values of a Message field that contains multiple values.
  */
class AbstractDataArray : public FlatCountable
{
public:
   // Should add the given item to our internal field.
   virtual status_t AddDataItem(const void * data, uint32 numBytes) = 0;

   // Should remove the (index)'th item from our internal field.
   virtual status_t RemoveDataItem(uint32 index) = 0;

   // Prepends the given data item to the beginning of our field.
   virtual status_t PrependDataItem(const void * data, uint32 numBytes) = 0;

   // Clears the field
   virtual void Clear(bool releaseDataBuffers) = 0;

   // Normalizes the field
   virtual void Normalize() = 0;

   // Sorts the data items in the field
   virtual void Sort(uint32 from=0, uint32 to=MUSCLE_NO_LIMIT) = 0;

   // Sets (setDataLoc) to point to the (index)'th item in our field.
   // Result is not guaranteed to remain valid after this object is modified.
   virtual status_t FindDataItem(uint32 index, const void ** setDataLoc) const = 0;

   // Should replace the (index)'th data item in the field with (data).
   virtual status_t ReplaceDataItem(uint32 index, const void * data, uint32 numBytes) = 0;

   // Returns the size (in bytes) of the item in the (index)'th slot.
   MUSCLE_NODISCARD virtual uint32 GetItemSize(uint32 index) const = 0;

   // Returns the number of items currently in the field
   MUSCLE_NODISCARD virtual uint32 GetNumItems() const = 0;

   // Convenience methods
   MUSCLE_NODISCARD bool HasItems() const {return (GetNumItems()>0);}
   MUSCLE_NODISCARD bool IsEmpty() const {return (GetNumItems()==0);}

   // Returns a 32-bit checksum for this field
   MUSCLE_NODISCARD virtual uint32 CalculateChecksum(bool countNonFlattenableFields) const = 0;

   // Returns true iff all elements in the field have the same size
   MUSCLE_NODISCARD virtual bool ElementsAreFixedSize() const = 0;

   // Flattenable interface
   MUSCLE_NODISCARD virtual bool IsFixedSize() const {return false;}

   // returns a separate (deep) copy of this field
   virtual Ref<AbstractDataArray> Clone() const = 0;

   // Returns true iff this field should be included when flattening.
   MUSCLE_NODISCARD virtual bool IsFlattenable() const = 0;

   // For debugging:  returns a description of our contents as a String
   virtual void Print(const OutputPrinter & p, uint32 maxRecurseLevel, int indent) const = 0;

   // Returns true iff this field is identical to (rhs).  If (compareContents) is false,
   // only the field lengths and type codes are checked, not the data itself.
   MUSCLE_NODISCARD bool IsEqualTo(const AbstractDataArray * rhs, bool compareContents) const
   {
      if ((TypeCode() != rhs->TypeCode())||(GetNumItems() != rhs->GetNumItems())) return false;
      return compareContents ? AreContentsEqual(rhs) : true;
   }

   /** Subclasses that hold their items via RefCountableRefs should override this to return a RefCountableRef to the (idx)'th item,
     * or a NULL RefCountableRef if (idx) isn't valid.  Default implemantation always returns a NULL RefCountableRef.
     */
   virtual RefCountableRef GetItemAtAsRefCountableRef(uint32 idx) const {(void) idx; return GetDefaultObjectForType<RefCountableRef>();}

   /** Used by the TemplatedFlatten() methods */
   virtual void FlattenAux(DataFlattener flat, uint32 maxItemsToFlatten) const = 0;

   /** Used by the regular Flatten() methods */
   virtual void Flatten(DataFlattener flat) const {FlattenAux(flat, MUSCLE_NO_LIMIT);}

protected:
   /** Must be implemented by each subclass to return true iff (rhs) is of the same type
    *  and has the same data as (*this).  The TypeCode() and GetNumItems() of (rhs) are
    *  guaranteed to equal those of this AbstractDataArray.  Called by IsEqualTo().
    */
   MUSCLE_NODISCARD virtual bool AreContentsEqual(const AbstractDataArray * rhs) const = 0;

private:
   DECLARE_COUNTED_OBJECT(AbstractDataArray);
};
DECLARE_REFTYPES(AbstractDataArray);

/** This class is a private part of the Message class's implementation.  User code should not access this class directly.
  * This class represents the value-data of one field in a Message object.
  */
class MUSCLE_NODISCARD MessageField MUSCLE_FINAL_CLASS : public PseudoFlattenable<MessageField>
{
public:
   /** Default ctor:  Creates a MessageField with no type */
   MessageField(uint32 typeCode = 0) : _typeCode(typeCode), _state(FIELD_STATE_EMPTY), _dataType(DATA_TYPE_NULL) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   MessageField(const MessageField & rhs) : _typeCode(0), _state(FIELD_STATE_EMPTY), _dataType(DATA_TYPE_NULL) {*this = rhs;}

   /** Dtor */
   ~MessageField() {Clear();}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   MessageField & operator = (const MessageField & rhs);

   /** @copydoc DoxyTemplate::Print(const OutputPrinter &) const */
   void Print(const OutputPrinter & p = stdout) const;

   /** Returns a human-readable string representing our state (for debugging) */
   String ToString() const;

   // Flattenable Pseudo-Interface
   MUSCLE_NODISCARD uint32 TypeCode() const {return _typeCode;}
   MUSCLE_NODISCARD uint32 FlattenedSize() const {return HasArray() ? GetArray()->FlattenedSize() : SingleFlattenedSize();}
   void Flatten(DataFlattener flat) const {FlattenAux(flat, MUSCLE_NO_LIMIT);}
   status_t Unflatten(DataUnflattener & unflat);

   // Pseudo-AbstractDataArray interface
   status_t AddDataItem(const void * data, uint32 numBytes) {return HasArray() ? GetArray()->AddDataItem(data, numBytes) : SingleAddDataItem(data, numBytes);}
   status_t RemoveDataItem(uint32 index) {return HasArray() ? GetArray()->RemoveDataItem(index) : SingleRemoveDataItem(index);}
   status_t PrependDataItem(const void * data, uint32 numBytes) {return HasArray() ? GetArray()->PrependDataItem(data, numBytes) : SinglePrependDataItem(data, numBytes);}
   void Clear();
   void Normalize() {if (HasArray()) GetArray()->Normalize();}
   void Sort(uint32 from, uint32 to) {if (HasArray()) GetArray()->Sort(from, to);}
   status_t FindDataItem(uint32 index, const void ** setDataLoc) const {return HasArray() ? GetArray()->FindDataItem(index, setDataLoc) : SingleFindDataItem(index, setDataLoc);}
   status_t ReplaceDataItem(uint32 index, const void * data, uint32 numBytes) {return HasArray() ? GetArray()->ReplaceDataItem(index, data, numBytes) : SingleReplaceDataItem(index, data, numBytes);}
   MUSCLE_NODISCARD uint32 GetItemSize(uint32 index) const {return HasArray() ? GetArray()->GetItemSize(index) : SingleGetItemSize(index);}
   MUSCLE_NODISCARD uint32 GetNumItems() const {return (_state == FIELD_STATE_EMPTY) ? 0 : (HasArray() ? GetArray()->GetNumItems() : 1);}
   MUSCLE_NODISCARD bool HasItems() const {return (GetNumItems() > 0);}
   MUSCLE_NODISCARD bool IsEmpty()  const {return (GetNumItems() == 0);}
   MUSCLE_NODISCARD uint32 CalculateChecksum(bool countNonFlattenableFields) const {return HasArray() ? GetArray()->CalculateChecksum(countNonFlattenableFields) : SingleCalculateChecksum(countNonFlattenableFields);}
   MUSCLE_NODISCARD bool ElementsAreFixedSize() const {return HasArray() ? GetArray()->ElementsAreFixedSize() : SingleElementsAreFixedSize();}
   MUSCLE_NODISCARD bool IsFixedSize() const {return false;}
   MUSCLE_NODISCARD bool IsFlattenable() const {return HasArray() ? GetArray()->IsFlattenable() : SingleIsFlattenable();}
   void Print(const OutputPrinter & p, uint32 maxRecurseLevel, int indent) const;
   MUSCLE_NODISCARD bool IsEqualTo(const MessageField & rhs, bool compareContents) const;
   status_t EnsurePrivate();  // un-shares our data, if necessary

   MUSCLE_NODISCARD const String & GetItemAtAsString(uint32 index) const;
   MUSCLE_NODISCARD const Point & GetItemAtAsPoint(uint32 index) const;
   MUSCLE_NODISCARD const Rect & GetItemAtAsRect(uint32 index) const;
   RefCountableRef GetItemAtAsRefCountableRef(uint32 index) const;
   status_t ReplaceFlatCountableDataItem(uint32 index, const FlatCountableRef & fcRef);
   status_t ShareTo(MessageField & shareToMe) const;
   MUSCLE_NODISCARD bool HasArray() const {return (_state == FIELD_STATE_ARRAY);}  // returns true iff we have an AbstractDataArray object allocated

   MUSCLE_NODISCARD uint64 TemplatedHashCode64() const {return ((uint64)GetNumItems())*((uint64)TypeCode());}
   MUSCLE_NODISCARD uint32 TemplatedFlattenedSize(const MessageField * optPayloadField) const;
   void TemplatedFlatten(const MessageField * optPayloadField, uint8 * & buf) const;
   status_t TemplatedUnflatten(Message & unflattenTo, const String & fieldName, DataUnflattener & unflat) const;

protected:
   void FlattenAux(DataFlattener flat, uint32 maxItemsToFlatten) const {if (HasArray()) GetArray()->FlattenAux(flat, maxItemsToFlatten); else SingleFlatten(flat);}

private:
   MUSCLE_NODISCARD const AbstractDataArray * GetArray() const {return static_cast<AbstractDataArray *>(GetInlineItemAsRefCountableRef()());}
   MUSCLE_NODISCARD AbstractDataArray * GetArray() {return static_cast<AbstractDataArray *>(GetInlineItemAsRefCountableRef()());}
   AbstractDataArrayRef GetArrayRef() const {AbstractDataArrayRef ret; (void) ret.SetFromRefCountableRef(GetInlineItemAsRefCountableRef()); return ret;}
   MUSCLE_NODISCARD uint32 GetNumItemsInFlattenedBuffer(const uint8 * bytes, uint32 numBytes) const;

   // single-item implementation of the AbstractDataArray methods
   MUSCLE_NODISCARD uint32 SingleFlattenedSize() const;
   void SingleFlatten(DataFlattener flat) const;
   status_t SingleUnflatten(DataUnflattener & unflat);
   status_t SingleAddDataItem(const void * data, uint32 numBytes);
   status_t SingleRemoveDataItem(uint32 index);
   status_t SinglePrependDataItem(const void * data, uint32 numBytes);
   status_t SingleFindDataItem(uint32 index, const void ** setDataLoc) const;
   status_t SingleReplaceDataItem(uint32 index, const void * data, uint32 numBytes);
   MUSCLE_NODISCARD uint32 SingleGetItemSize(uint32 index) const;
   MUSCLE_NODISCARD uint32 SingleCalculateChecksum(bool countNonFlattenableFields) const;
   MUSCLE_NODISCARD bool SingleElementsAreFixedSize() const;
   MUSCLE_NODISCARD bool SingleIsFlattenable() const;
   void SinglePrint(const OutputPrinter & p, uint32 maxRecurseLevel, int indent) const;
   void SingleSetValue(const void * data, uint32 numBytes);
   AbstractDataArrayRef CreateDataArray(uint32 typeCode) const;
   void ChangeType(uint8 newType);

   typedef void * MFVoidPointer;  // just to make the code syntax easier

   // _data needs to have at least as many bytes as our largest value type, and needs to be int-aligned
   template <int S1, int S2> struct _maxx {enum {sz = (S1>S2)?S1:S2};};
   #define mfmax(a,b) (_maxx< (a), (b) >::sz)

   enum {UNIONSIZE = mfmax(sizeof(bool), mfmax(sizeof(double), mfmax(sizeof(float), mfmax(sizeof(int8), mfmax(sizeof(int16), mfmax(sizeof(int32), mfmax(sizeof(int64), mfmax(sizeof(void *), mfmax(sizeof(Point), mfmax(sizeof(Rect), mfmax(sizeof(String), sizeof(RefCountableRef))))))))))))};

   // enumeration of the various value types that can be held inline by this variant class
   enum {
      DATA_TYPE_NULL = 0,
      DATA_TYPE_BOOL,
      DATA_TYPE_DOUBLE,
      DATA_TYPE_FLOAT,
      DATA_TYPE_INT8,
      DATA_TYPE_INT16,
      DATA_TYPE_INT32,
      DATA_TYPE_INT64,
      DATA_TYPE_POINTER,
      DATA_TYPE_POINT,
      DATA_TYPE_RECT,
      DATA_TYPE_STRING,
      DATA_TYPE_REF,
      NUM_DATA_TYPES
   };

   void SetInlineItemToNull()
   {
      ChangeType(DATA_TYPE_NULL);
   }

   void SetInlineItemAsBool(bool b)
   {
      ChangeType(DATA_TYPE_BOOL);
      bool * p MUSCLE_MAY_ALIAS = reinterpret_cast<bool *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsDouble(double b)
   {
      ChangeType(DATA_TYPE_DOUBLE);
      double * p MUSCLE_MAY_ALIAS = reinterpret_cast<double *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsFloat(float b)
   {
      ChangeType(DATA_TYPE_FLOAT);
      float * p MUSCLE_MAY_ALIAS = reinterpret_cast<float *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsInt8(int8 b)
   {
      ChangeType(DATA_TYPE_INT8);
      int8 * p MUSCLE_MAY_ALIAS = reinterpret_cast<int8 *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsInt16(int16 b)
   {
      ChangeType(DATA_TYPE_INT16);
      int16 * p MUSCLE_MAY_ALIAS = reinterpret_cast<int16 *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsInt32(int32 b)
   {
      ChangeType(DATA_TYPE_INT32);
      int32 * p MUSCLE_MAY_ALIAS = reinterpret_cast<int32 *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsInt64(int64 b)
   {
      ChangeType(DATA_TYPE_INT64);
      int64 * p MUSCLE_MAY_ALIAS = reinterpret_cast<int64 *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsPointer(MFVoidPointer b)
   {
      ChangeType(DATA_TYPE_POINTER);
      MFVoidPointer * p MUSCLE_MAY_ALIAS = reinterpret_cast<MFVoidPointer *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsPoint(const Point & b)
   {
      ChangeType(DATA_TYPE_POINT);
      Point * p MUSCLE_MAY_ALIAS = reinterpret_cast<Point *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsRect(const Rect & b)
   {
      ChangeType(DATA_TYPE_RECT);
      Rect * p MUSCLE_MAY_ALIAS = reinterpret_cast<Rect *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsString(const String & b)
   {
      ChangeType(DATA_TYPE_STRING);
      String * p MUSCLE_MAY_ALIAS = reinterpret_cast<String *>(_union._data);
      *p = b;
   }

   void SetInlineItemAsRefCountableRef(const RefCountableRef & b)
   {
      ChangeType(DATA_TYPE_REF);
      RefCountableRef * p MUSCLE_MAY_ALIAS = reinterpret_cast<RefCountableRef *>(_union._data);
      *p = b;
   }

   MUSCLE_NODISCARD bool GetInlineItemAsBool() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_BOOL)), "GetInlineItemAsBool:  invalid state!");
      const bool * p MUSCLE_MAY_ALIAS = reinterpret_cast<const bool *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD double GetInlineItemAsDouble() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_DOUBLE)), "GetInlineItemAsDouble:  invalid state!");
      const double * p MUSCLE_MAY_ALIAS = reinterpret_cast<const double *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD float GetInlineItemAsFloat() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_FLOAT)), "GetInlineItemAsFloat:  invalid state!");
      const float * p MUSCLE_MAY_ALIAS = reinterpret_cast<const float *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD int8 GetInlineItemAsInt8() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_INT8)), "GetInlineItemAsInt8:  invalid state!");
      const int8 * p MUSCLE_MAY_ALIAS = reinterpret_cast<const int8 *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD int16 GetInlineItemAsInt16() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_INT16)), "GetInlineItemAsInt16:  invalid state!");
      const int16 * p MUSCLE_MAY_ALIAS = reinterpret_cast<const int16 *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD int32 GetInlineItemAsInt32() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_INT32)), "GetInlineItemAsInt32:  invalid state!");
      const int32 * p MUSCLE_MAY_ALIAS = reinterpret_cast<const int32 *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD int64 GetInlineItemAsInt64() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_INT64)), "GetInlineItemAsInt64:  invalid state!");
      const int64 * p MUSCLE_MAY_ALIAS = reinterpret_cast<const int64 *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD MFVoidPointer GetInlineItemAsPointer() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_POINTER)), "GetInlineItemAsPointer:  invalid state!");
      const MFVoidPointer * p MUSCLE_MAY_ALIAS = reinterpret_cast<const MFVoidPointer *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD const Point & GetInlineItemAsPoint() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_POINT)), "GetInlineItemAsPoint:  invalid state!");
      const Point * p MUSCLE_MAY_ALIAS = reinterpret_cast<const Point *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD const Rect & GetInlineItemAsRect() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_RECT)), "GetInlineItemAsRect:  invalid state!");
      const Rect * p MUSCLE_MAY_ALIAS = reinterpret_cast<const Rect *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD const String & GetInlineItemAsString() const
   {
      MASSERT(((_state == FIELD_STATE_INLINE)&&(_dataType == DATA_TYPE_STRING)), "GetInlineItemAsString:  invalid state!");
      const String * p MUSCLE_MAY_ALIAS = reinterpret_cast<const String *>(_union._data); return *p;
   }

   MUSCLE_NODISCARD const RefCountableRef & GetInlineItemAsRefCountableRef() const
   {
      // No assert here, since we also use this method to grab the array object
      const RefCountableRef * p MUSCLE_MAY_ALIAS = reinterpret_cast<const RefCountableRef *>(_union._data); return *p;
   }

   uint32 _typeCode; // B_*_TYPE for this field
   uint8 _state;     // FIELD_STATE_*
   uint8 _dataType;  // DATA_TYPE_*

   enum {
      FIELD_STATE_EMPTY = 0,  // no data items, no AbstractDataArray object
      FIELD_STATE_INLINE,     // a single, inline data item
      FIELD_STATE_ARRAY,      // we have allocated an AbstractDataArray object
      NUM_FIELD_STATES
   };

   union {
      uint64 _forceAlignment;
      char _data[UNIONSIZE];
   } _union;
};

} // end namespace muscle_private

} // end namespace muscle

#endif /* MuscleMessage_impl_h */
