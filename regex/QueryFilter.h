/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQueryFilter_h
#define MuscleQueryFilter_h

#include "util/Queue.h"
#include "util/ByteBuffer.h"
#include "message/Message.h"

namespace muscle {

class DataNode;
class StringMatcher;

/** Enumeration of QueryFilter type codes for the included QueryFilter classes */
enum {
   QUERY_FILTER_TYPE_WHATCODE = 1902537776, // 'qfl0'
   QUERY_FILTER_TYPE_VALUEEXISTS,
   QUERY_FILTER_TYPE_BOOL,
   QUERY_FILTER_TYPE_DOUBLE,
   QUERY_FILTER_TYPE_FLOAT,
   QUERY_FILTER_TYPE_INT64,
   QUERY_FILTER_TYPE_INT32,
   QUERY_FILTER_TYPE_INT16,
   QUERY_FILTER_TYPE_INT8,
   QUERY_FILTER_TYPE_POINT,
   QUERY_FILTER_TYPE_RECT,
   QUERY_FILTER_TYPE_STRING,
   QUERY_FILTER_TYPE_MESSAGE,
   QUERY_FILTER_TYPE_RAWDATA,
   QUERY_FILTER_TYPE_NANDNOT,
   QUERY_FILTER_TYPE_ANDOR,
   QUERY_FILTER_TYPE_XOR,
   // add more codes here...
   LAST_QUERY_FILTER_TYPE
};

/** Interface for any object that can examine a Message and tell whether it
  * matches some criterion.  Used primarily for filtering queries based on content.
  */
class QueryFilter : public RefCountable
{
public:
   /** Default constructor */
   QueryFilter() {/* empty */}

   /** Destructor */
   virtual ~QueryFilter();

   /** Dumps our state into the given (archive).   Default implementation
    *  just writes our TypeCode() into the 'what' of the Message
    *  and returns B_NO_ERROR.
    *  @param archive the Message to write our state into.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure.
    */
   virtual status_t SaveToArchive(Message & archive) const;

   /** Restores our state from the given (archive).  Default implementation
    *  returns B_NO_ERROR iff the archive's what code matches our TypeCode().
    *  @param archive The archive to restore our state from.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure.
    */     
   virtual status_t SetFromArchive(const Message & archive);

   /** Should be overridden to return the appropriate QUERY_FILTER_TYPE_* code. */
   virtual uint32 TypeCode() const = 0;

   /** Must be implemented to return true iff (msg) matches the criterion.
     * @param msg Reference to a read-only Message to check to see whether it matches our criteria or not.  The QueryFilter is allowed to
     *            retarget this ConstMessageRef to point at a different Message if it wants to; the different Message will be used in the 
     *            resulting query output.
     * @param optNode The DataNode object the matching is being done on, or NULL if the DataNode is not available.
     * @returns true iff the Messages matches, else false.
     */
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const = 0;

   /** Returns true iff we can be instantiated using a Message with the given
     * 'what' code.  Default implementation returns true iff (what) equals the
     * value returned by our own TypeCode() method.
     */
   virtual bool AcceptsTypeCode(uint32 what) const {return TypeCode() == what;}
};
DECLARE_REFTYPES(QueryFilter);

/** This filter tests the 'what' value of the Message. */
class WhatCodeQueryFilter : public QueryFilter
{
public:
   /** Default constructor
     * @param what The 'what' code to match on.  Only Messages with this 'what' code will be matched.
     */
   WhatCodeQueryFilter(uint32 what = 0) : _minWhatCode(what), _maxWhatCode(what) {/* empty */}

   /** Default constructor
     * @param minWhat The minimum 'what' code to match on.  Only messages with 'what' codes greater than or equal to this code will be matched.
     * @param maxWhat The maximum 'what' code to match on.  Only messages with 'what' codes less than or equal to this code will be matched.
     */
   WhatCodeQueryFilter(uint32 minWhat, uint32 maxWhat) : _minWhatCode(minWhat), _maxWhatCode(maxWhat) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const {(void) optNode; return muscleInRange(msg()->what, _minWhatCode, _maxWhatCode);}
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_WHATCODE;}

private:
   uint32 _minWhatCode;
   uint32 _maxWhatCode;
};

/** Semi-abstract base class for all query filters that test a single item in a Message */
class ValueQueryFilter : public QueryFilter
{
public:
   /** Default constructor */
   ValueQueryFilter() : _index(0) {/* Empty */}

   /** Constructor.
     * @param fieldName Name of the field in the Message to look at
     * @param index Index of the item within the field to look at.  Defaults to zero (i.e. the first item)
     */
   ValueQueryFilter(const String & fieldName, uint32 index = 0) : _fieldName(fieldName), _index(index) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);

   /** Sets our index-in-field setting. */
   void SetIndex(uint32 index) {_index = index;}

   /** Returns our current index-in-field setting, as set by SetIndex() or in our constructor */
   uint32 GetIndex() const {return _index;}

   /** Sets the field name to use. */
   void SetFieldName(const String & fieldName) {_fieldName = fieldName;}

   /** Returns the current field name, as set by SetFieldName() or in our constructor. */
   const String & GetFieldName() const {return _fieldName;}

private:
   String _fieldName;
   uint32 _index;
};

/** This filter merely checks to see if the specified value exists in the target Message. */
class ValueExistsQueryFilter : public ValueQueryFilter
{
public:
   /** Default constructor.  
     * @param typeCode Type code to check for.  Default to B_ANY_TYPE (a wildcard value)
     */
   ValueExistsQueryFilter(uint32 typeCode = B_ANY_TYPE) : _typeCode(typeCode) {/* empty */}

   /** Constructor
     * @param fieldName Field name to look for
     * @param typeCode type code to look for, or B_ANY_TYPE if you don't care what the type code is.
     * @param index Optional index of the item within the field.  Defaults to zero.
     */
   ValueExistsQueryFilter(const String & fieldName, uint32 typeCode = B_ANY_TYPE, uint32 index = 0) : ValueQueryFilter(fieldName, index), _typeCode(typeCode) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_VALUEEXISTS;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const {(void) optNode; const void * junk; return (msg()->FindData(GetFieldName(), _typeCode, &junk, NULL) == B_NO_ERROR);}

   /** Sets the type code that we will look for in the target Message.
     * @param typeCode the type code to look for.  Use B_ANY_TYPE to indicate that you don't care what the type code is.
     */
   void SetTypeCode(uint32 typeCode) {_typeCode = typeCode;}

   /** Returns the type code we are to look in the target Message for.
     * Note that this method is different from TypeCode()!
     */
   uint32 GetTypeCode() const {return _typeCode;}

private:
   uint32 _typeCode;
};


/** Enumeration of mask operations available to NumericQueryFilter classes */
enum {
   NQF_MASK_OP_NONE = 0,
   NQF_MASK_OP_AND,
   NQF_MASK_OP_OR,
   NQF_MASK_OP_XOR,
   NQF_MASK_OP_NAND,
   NQF_MASK_OP_NOR,
   NQF_MASK_OP_XNOR,
   NUM_NQF_MASK_OPS
};

template <typename DataType> inline DataType NQFDoMaskOp(uint8 maskOp, const DataType & msgVal, const DataType & mask)
{
   switch(maskOp)
   {
      case NQF_MASK_OP_NONE: return msgVal;
      case NQF_MASK_OP_AND:  return msgVal & mask;
      case NQF_MASK_OP_OR:   return msgVal | mask;
      case NQF_MASK_OP_XOR:  return msgVal ^ mask;
      case NQF_MASK_OP_NAND: return ~(msgVal & mask);
      case NQF_MASK_OP_NOR:  return ~(msgVal | mask);
      case NQF_MASK_OP_XNOR: return ~(msgVal ^ mask);
      default:               return msgVal;
   }
}

// Separate implementation for bool because you can't use bitwise negate on a bool, the result is undefined 
// and causes unecessary implicit int<->bool casts (per Mika)
template<> inline bool NQFDoMaskOp(uint8 maskOp, const bool & msgVal, const bool & mask)
{
   switch(maskOp)
   {
      case NQF_MASK_OP_NONE: return msgVal;
      case NQF_MASK_OP_AND:  return msgVal & mask;
      case NQF_MASK_OP_OR:   return msgVal | mask;
      case NQF_MASK_OP_XOR:  return msgVal ^ mask;
      case NQF_MASK_OP_NAND: return !(msgVal & mask);
      case NQF_MASK_OP_NOR:  return !(msgVal | mask);
      case NQF_MASK_OP_XNOR: return !(msgVal ^ mask);
      default:               return msgVal;
   }
}
// Dummy specializations for mask operations, for types that don't have bitwise operations defined.
template<> inline Point  NQFDoMaskOp(uint8 /*maskOp*/, const Point &  /*msgVal*/, const Point &  /*argVal*/) {return Point();}
template<> inline Rect   NQFDoMaskOp(uint8 /*maskOp*/, const Rect &   /*msgVal*/, const Rect &   /*argVal*/) {return Rect();}
template<> inline float  NQFDoMaskOp(uint8 /*maskOp*/, const float &  /*msgVal*/, const float &  /*argVal*/) {return float();}
template<> inline double NQFDoMaskOp(uint8 /*maskOp*/, const double & /*msgVal*/, const double & /*argVal*/) {return double();}

/** This templated class is used to generate a number of numeric-comparison-query classes, all of which are quite similar to each other. */
template <typename DataType, uint32 DataTypeCode, uint32 ClassTypeCode>
class NumericQueryFilter : public ValueQueryFilter
{
public:
   /** Default constructor.  Sets our value to its default (usually zero), and the operator to OP_EQUAL_TO. */
   NumericQueryFilter() : _value(), _mask(), _op(OP_EQUAL_TO), _maskOp(NQF_MASK_OP_NONE), _assumeDefault(false) {/* empty */}

   /** Constructor.  This constructor will create a QueryFilter that only returns true from Match()
     * If the matched Message has the field item with the specified value in it.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The value to compare to the value found in the Message.
     * @param index Optional index of the item within the field.  Defaults to zero.
     */
   NumericQueryFilter(const String & fieldName, uint8 op, DataType value, uint32 index = 0) : ValueQueryFilter(fieldName, index), _value(value), _mask(), _op(op), _maskOp(NQF_MASK_OP_NONE), _assumeDefault(false) {/* empty */}

   /** Constructor.  This constructor is similar to the constructor shown above,
     * except that when this constructor is used, if the specified item does not exist in 
     * the matched Message, this QueryFilter will act as if the Message contained the
     * specified assumedValue.  This is useful when the Message has been encoded with
     * the expectation that missing fields should be assumed equivalent to well known
     * default values.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The value to compare to the value found in the Message.
     * @param index Index of the item within the field.  Defaults to zero.
     * @param assumedValue The value to pretend that the Message contained, if we don't find our value in the Message.
     */
   NumericQueryFilter(const String & fieldName, uint8 op, DataType value, uint32 index, const DataType & assumedValue) : ValueQueryFilter(fieldName, index), _value(value), _mask(), _op(op), _maskOp(NQF_MASK_OP_NONE), _assumeDefault(true), _default(assumedValue) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const
   {
      return ((ValueQueryFilter::SaveToArchive(archive)                                                      == B_NO_ERROR)&&
              (archive.CAddInt8("op", _op)                                                                   == B_NO_ERROR)&&
              (archive.CAddInt8("mop", _maskOp)                                                              == B_NO_ERROR)&&
              (archive.AddData("val", DataTypeCode, &_value, sizeof(_value))                                 == B_NO_ERROR)&&
              (archive.AddData("msk", DataTypeCode, &_mask,  sizeof(_mask))                                  == B_NO_ERROR)&&
              ((_assumeDefault == false)||(archive.AddData("val", DataTypeCode, &_default, sizeof(_default)) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
   }

   virtual status_t SetFromArchive(const Message & archive)
   {
      _assumeDefault = false;

      const void * dt;
      uint32 numBytes;
      if ((ValueQueryFilter::SetFromArchive(archive) == B_NO_ERROR)&&(archive.FindData("val", DataTypeCode, &dt, &numBytes) == B_NO_ERROR)&&(numBytes == sizeof(_value)))
      {
         _op     = archive.GetInt8("op");
         _value  = *((DataType *)dt);
         _maskOp = archive.GetInt8("mop");
         _mask   = ((archive.FindData("msk", DataTypeCode, &dt, &numBytes) == B_NO_ERROR)&&(numBytes == sizeof(_mask))) ? *((DataType *)dt) : DataType();
         
         if (archive.FindData("val", DataTypeCode, 1, &dt, &numBytes) == B_NO_ERROR)
         {
            _assumeDefault = true;
            _default = *((DataType *)dt);
         }
         return B_NO_ERROR;
      }
      return B_ERROR;
   }

   virtual uint32 TypeCode() const {return ClassTypeCode;}

   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const
   {
      (void) optNode;  // shut compiler and DOxygen up

      const DataType * valueInMsg;
     
      const void * p;
           if (msg()->FindData(GetFieldName(), DataTypeCode, GetIndex(), &p, NULL) == B_NO_ERROR) valueInMsg = (const DataType *)p;
      else if (_assumeDefault) valueInMsg = &_default;
      else return false;

      return (_maskOp == NQF_MASK_OP_NONE) ? MatchesAux(*valueInMsg) : MatchesAux(NQFDoMaskOp(_maskOp, *valueInMsg, _mask));
   }

   /** Set the operator to use.  
     * @param op One of the OP_* values enumerated below.
     */
   void SetOperator(uint8 op) {_op = op;}

   /** Returns the currently specified operator, as specified in the constructor or in SetOperator() */
   uint8 GetOperator() const {return _op;}

   /** Set the value to compare against.
     * @param value The new value.
     */
   void SetValue(DataType value) {_value = value;}

   /** Returns the currently specified value, as specified in the constructor or in SetValue() */
   DataType GetValue() const {return _value;}
 
   /** Operators defined for our expressions */
   enum {
      OP_EQUAL_TO = 0,             /**< This operator represents '==' */
      OP_LESS_THAN,                /**< This operator represents '<'  */
      OP_GREATER_THAN,             /**< This operator represents '>'  */
      OP_LESS_THAN_OR_EQUAL_TO,    /**< This operator represents '<=' */
      OP_GREATER_THAN_OR_EQUAL_TO, /**< This operator represents '>=' */
      OP_NOT_EQUAL_TO,             /**< This operator represents '!=' */
      NUM_NUMERIC_OPERATORS        /**< This is a guard value         */
   };

   /** Returns true iff this filter will assume a default value if it can't find an actual value in the Message it tests. */
   bool IsAssumedDefault() const {return _assumeDefault;}

   /** Sets the assumed default value to the specified value.
     * @param d The value to match against if we don't find a value in the Message.
     */
   void SetAssumedDefault(const DataType & d) {_default = d; _assumeDefault = true;}

   /** Unsets the assumed default.  After calling this, our Match() method will simply always
     * return false if the specified data item is not found in the Message.
     */
   void UnsetAssumedDefault() {_default = DataType(); _assumeDefault = false;}

   /** Sets the mask operation to perform on the discovered data value before applying the OP_* test.
     * Note that mask operations are not defined for floats, doubles, Points, or Rects.
     * @param maskOp a NQF_MASK_OP_* value.  Default value is NQF_MASK_OP_NONE.
     * @param maskValue The mask value to apply to the discovered data value, before applying the OP_* test.
     */
   void SetMask(uint8 maskOp, const DataType & maskValue) {_maskOp = maskOp, _mask = maskValue;}

   /** Returns this QueryFilter's current NQF_MASK_OP_* setting. */
   uint8 GetMaskOp() const {return _maskOp;}

   /** Returns this QueryFilter's current mask value. */
   uint8 GetMaskValue() const {return _mask;}

private:
   bool MatchesAux(const DataType & valueInMsg) const
   {
      switch(_op)
      {
         case OP_EQUAL_TO:                 return (valueInMsg == _value);
         case OP_LESS_THAN:                return (valueInMsg <  _value);
         case OP_GREATER_THAN:             return (valueInMsg >  _value);
         case OP_LESS_THAN_OR_EQUAL_TO:    return (valueInMsg <= _value);
         case OP_GREATER_THAN_OR_EQUAL_TO: return (valueInMsg >= _value);
         case OP_NOT_EQUAL_TO:             return (valueInMsg != _value);
         default:                          /* do nothing */  break;
      }
      return false;
   }

   DataType _value;
   DataType _mask;
   uint8 _op, _maskOp;
   
   bool _assumeDefault;
   DataType _default;
};

typedef NumericQueryFilter<bool,   B_BOOL_TYPE,   QUERY_FILTER_TYPE_BOOL>   BoolQueryFilter;
typedef NumericQueryFilter<double, B_DOUBLE_TYPE, QUERY_FILTER_TYPE_DOUBLE> DoubleQueryFilter;
typedef NumericQueryFilter<float,  B_FLOAT_TYPE,  QUERY_FILTER_TYPE_FLOAT>  FloatQueryFilter;
typedef NumericQueryFilter<int64,  B_INT64_TYPE,  QUERY_FILTER_TYPE_INT64>  Int64QueryFilter;
typedef NumericQueryFilter<int32,  B_INT32_TYPE,  QUERY_FILTER_TYPE_INT32>  Int32QueryFilter;
typedef NumericQueryFilter<int16,  B_INT16_TYPE,  QUERY_FILTER_TYPE_INT16>  Int16QueryFilter;
typedef NumericQueryFilter<int8,   B_INT8_TYPE,   QUERY_FILTER_TYPE_INT8>   Int8QueryFilter;
typedef NumericQueryFilter<Point,  B_POINT_TYPE,  QUERY_FILTER_TYPE_POINT>  PointQueryFilter;
typedef NumericQueryFilter<Point,  B_RECT_TYPE,   QUERY_FILTER_TYPE_RECT>   RectQueryFilter;

/** A semi-abstract base class for any QueryFilter that holds a list of references to child filters. */
class MultiQueryFilter : public QueryFilter
{
public:
   /** Default constructor. */
   MultiQueryFilter() {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);

   /** Returns a read-only reference to our Queue of child ConstQueryFilterRefs. */
   const Queue<ConstQueryFilterRef> & GetChildren() const {return _children;}

   /** Returns a read/write reference to our Queue of child ConstQueryFilterRefs. */
   Queue<ConstQueryFilterRef> & GetChildren() {return _children;}

private:
   Queue<ConstQueryFilterRef> _children;
};

/** This class matches iff at least (n) of its children match.  As such, it can be used as an OR operator,
  * an AND operator, or something in-between the two.
  */
class AndOrQueryFilter : public MultiQueryFilter
{
public:
   /** Default constructor.  Creates an AND filter with no children. 
     * @param minMatches The minimum number of children that must match before this filter considers
     *                   the match to be valid.  Default to MUSCLE_NO_LIMIT, meaning all children must match.
     */
   AndOrQueryFilter(uint32 minMatches = MUSCLE_NO_LIMIT) : _minMatches(minMatches) {/* empty */}

   /** Convenience constructor.  Note that you usually want to manually add at least one more child as well.
     * @param isAnd If true, the operation will be an 'and' operation.  Otherwise it will be an 'or' operation.
     * @param child First argument to the operation
     */
   AndOrQueryFilter(bool isAnd, const ConstQueryFilterRef & child) : _minMatches(isAnd ? MUSCLE_NO_LIMIT : 1)
   {
      GetChildren().AddTail(child);
   }

   /** Convenience constructor for simple binary 'or' or 'and' operations.
     * @param isAnd If true, the operation will be an 'and' operation.  Otherwise it will be an 'or' operation.
     * @param child1 First argument to the operation
     * @param child2 Second argument to the operation
     */
   AndOrQueryFilter(bool isAnd, const ConstQueryFilterRef & child1, const ConstQueryFilterRef & child2) : _minMatches(isAnd ? MUSCLE_NO_LIMIT : 1)
   {
      GetChildren().AddTail(child1);
      GetChildren().AddTail(child2);
   }

   /** Convenience constructor for simple ternary 'or' or 'and' operations.
     * @param isAnd If true, the operation will be an 'and' operation.  Otherwise it will be an 'or' operation.
     * @param child1 First argument to the operation
     * @param child2 Second argument to the operation
     * @param child3 Third argument to the operation
     */
   AndOrQueryFilter(bool isAnd, const ConstQueryFilterRef & child1, const ConstQueryFilterRef & child2, const ConstQueryFilterRef & child3) : _minMatches(isAnd ? MUSCLE_NO_LIMIT : 1)
   {
      GetChildren().AddTail(child1);
      GetChildren().AddTail(child2);
      GetChildren().AddTail(child3);
   }

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_ANDOR;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;

   /** Set the minimum number of children that must match the target Message in order for this
     * filter to match the target Message.  If the specified number is greater than the number of
     * child filters held by this QueryFilter, then this filter matches only if every one of the child
     * filters match.
     * @param minMatches How many child filters must match, at a minimum.
     */
   void SetMinMatchCount(uint32 minMatches) {_minMatches = minMatches;}

   /** Returns the minimum-match-count for this filter, as specified in the constructor or by SetMinMatchCount(). */
   uint32 GetMinMatchCount() const {return _minMatches;}

private:
   uint32 _minMatches;
};

/** This class matches iff at most (n) of its children match.  As such, it can be used as a NAND operator,
  * a NOT operator, or something in-between the two.
  */
class NandNotQueryFilter : public MultiQueryFilter
{
public:
   /** Default constructor.  Creates an NAND filter with no children. 
     * @param maxMatches The maximum number of children that may match before this filter considers
     *                   the match to be invalid.  Defaults to 0, meaning no children may match.
     */
   NandNotQueryFilter(uint32 maxMatches = 0) : _maxMatches(maxMatches) {/* empty */}

   /** Convenience constructor for simple unary 'not' operation.
     * @param child Child whose logic we should negate.  This child is added to our child list, and the MaxMatchCount is set to zero. 
     */
   NandNotQueryFilter(const ConstQueryFilterRef & child) : _maxMatches(0)
   {
      GetChildren().AddTail(child);
   }

   /** Convenience constructor for simple binary 'nand' operation.  MaxMatchCount is set to one.
     * @param child1 First argument to the operation
     * @param child2 Second argument to the operation
     */
   NandNotQueryFilter(const ConstQueryFilterRef & child1, const ConstQueryFilterRef & child2) : _maxMatches(1)
   {
      GetChildren().AddTail(child1);
      GetChildren().AddTail(child2);
   }

   /** Convenience constructor for simple ternary 'nand' operation.  MaxMatchCount is set to one.
     * @param child1 First argument to the operation
     * @param child2 Second argument to the operation
     * @param child3 Third argument to the operation
     */
   NandNotQueryFilter(const ConstQueryFilterRef & child1, const ConstQueryFilterRef & child2, const ConstQueryFilterRef & child3) : _maxMatches(1)
   {
      GetChildren().AddTail(child1);
      GetChildren().AddTail(child2);
      GetChildren().AddTail(child3);
   }

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_NANDNOT;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;

   /** Set the maximum number of children that may match the target Message in order for this
     * filter to match the target Message.  If the specified number is greater than the number of
     * child filters held by this QueryFilter, then this filter fails to match only if every one 
     * of the child filters match.
     * @param maxMatches Maximum number of child filters that may match.
     */
   void SetMaxMatchCount(uint32 maxMatches) {_maxMatches = maxMatches;}

   /** Returns the maximum-match-count for this filter, as specified in the constructor or by SetMaxMatchCount(). */
   uint32 GetMaxMatchCount() const {return _maxMatches;}

private:
   uint32 _maxMatches;
};

/** This class matches only if an odd number of its children match. */
class XorQueryFilter : public MultiQueryFilter
{
public:
   /** Default constructor.  You'll want to add children to this object manually. */
   XorQueryFilter() {/* empty */}

   /** Convenience constructor for simple binary 'xor' operation.
     * @param child1 First argument to the operation
     * @param child2 Second argument to the operation
     */
   XorQueryFilter(const ConstQueryFilterRef & child1, const ConstQueryFilterRef & child2)
   {
      GetChildren().AddTail(child1);
      GetChildren().AddTail(child2);
   }

   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_XOR;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;
};

/** This class matches iff the specified sub-Message exists in our target Message,
  * and (optionally) our child ConstQueryFilterRef can match that sub-Message.
  */
class MessageQueryFilter : public ValueQueryFilter
{
public:
   /** Default constructor.  The child filter is left NULL, so that any child Message will match */
   MessageQueryFilter() {/* Empty */}

   /** Constructor.
     * @param childFilter Reference to the filter to use to match the sub-Message found at (fieldName:index)
     * @param fieldName Name of the field in the Message to look at
     * @param index Index of the item within the field to look at.  Defaults to zero (i.e. the first item)
     */
   MessageQueryFilter(const ConstQueryFilterRef & childFilter, const String & fieldName, uint32 index = 0) : ValueQueryFilter(fieldName, index), _childFilter(childFilter) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_MESSAGE;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;

   /** Set the sub-filter to use on the target's sub-Message.
     * @param childFilter Filter to use, or a NULL reference to indicate that any sub-Message found should match. 
     */
   void SetChildFilter(const ConstQueryFilterRef & childFilter) {_childFilter = childFilter;}

   /** Returns our current sub-filter as set in our constructor or in SetChildFilter() */
   ConstQueryFilterRef GetChildFilter() const {return _childFilter;}

private:
   ConstQueryFilterRef _childFilter;
};

/** This class matches on string field values.  */
class StringQueryFilter : public ValueQueryFilter
{
public:
   /** Default constructor.  The string is set to "", and the operator is set to OP_EQUAL_TO. */
   StringQueryFilter() : _op(OP_EQUAL_TO), _assumeDefault(false), _matcher(NULL) {/* Empty */}

   /** Constructor.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The string to compare to the string found in the Message.
     * @param index Optional index of the item within the field.  Defaults to zero.
     */
   StringQueryFilter(const String & fieldName, uint8 op, const String & value, uint32 index = 0) : ValueQueryFilter(fieldName, index), _value(value), _op(op), _assumeDefault(false), _matcher(NULL) {/* empty */}

   /** Constructor.  This constructor is similar to the constructor shown above,
     * except that when this constructor is used, if the specified item does not exist in 
     * the matched Message, this QueryFilter will act as if the Message contained the
     * specified assumedValue.  This is useful when the Message has been encoded with
     * the expectation that missing fields should be assumed equivalent to well known
     * default values.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The string to compare to the string found in the Message.
     * @param index Optional index of the item within the field.  Defaults to zero.
     * @param assumedValue The value to pretend that the Message contained, if we don't find our value in the Message.
     */
   StringQueryFilter(const String & fieldName, uint8 op, const String & value, uint32 index, const String & assumedValue) : ValueQueryFilter(fieldName, index), _value(value), _op(op), _assumeDefault(true), _default(assumedValue), _matcher(NULL) {/* empty */}

   /** Destructor */
   ~StringQueryFilter() {FreeMatcher();}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_STRING;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;

   /** Set the operator to use.  
     * @param op One of the OP_* values enumerated below.
     */
   void SetOperator(uint8 op) {if (op != _op) {_op = op; FreeMatcher();}}

   /** Returns the currently specified operator, as specified in the constructor or in SetOperator() */
   uint8 GetOperator() const {return _op;}

   /** Set the value to compare against.
     * @param value The new value.
     */
   void SetValue(const String & value) {if (value != _value) {_value = value; FreeMatcher();}}

   /** Returns the currently specified value, as specified in the constructor or in SetValue() */
   const String & GetValue() const {return _value;}
 
   enum {
      OP_EQUAL_TO = 0,                         /**< This token represents '==', e.g. nextValue==myValue (case sensitive) */
      OP_LESS_THAN,                            /**< This token represents '<',  e.g. nextValue<myValue  (case sensitive) */
      OP_GREATER_THAN,                         /**< This token represents '>',  e.g. nextValue>myValue  (case sensitive) */
      OP_LESS_THAN_OR_EQUAL_TO,                /**< This token represents '<=', e.g. nextValue<=myValue (case sensitive) */
      OP_GREATER_THAN_OR_EQUAL_TO,             /**< This token represents '>=', e.g. nextValue>=myValue (case sensitive) */
      OP_NOT_EQUAL_TO,                         /**< This token represents '!=', e.g. nextValue!=myValue (case sensitive) */
      OP_STARTS_WITH,                          /**< This token represents a prefix match, e.g. nextValue.StartsWith(myValue) (case sensitive) */
      OP_ENDS_WITH,                            /**< This token represents a suffix match, e.g. nextValue.EndsWith(myValue) (case sensitive) */
      OP_CONTAINS,                             /**< This token represents an infix match, e.g. (nextValue.IndexOf(myValue)>=0) (case sensitive) */
      OP_START_OF,                             /**< This token represents an inverse prefix match, e.g. myValue.StartsWith(nextValue) (case sensitive) */
      OP_END_OF,                               /**< This token represents an inverse suffix match, e.g. myValue.StartsWith(nextValue) (case sensitive) */
      OP_SUBSTRING_OF,                         /**< This token represents an inverse infix match, e.g. myValue.StartsWith(nextValue) (case sensitive) */
      OP_EQUAL_TO_IGNORECASE,                  /**< This token is the same as OP_EQUAL_TO, except it is case insensitive */
      OP_LESS_THAN_IGNORECASE,                 /**< This token is the same as OP_LESS_THAN, except it is case insensitive */
      OP_GREATER_THAN_IGNORECASE,              /**< This token is the same as OP_GREATER_THAN, except it is case insensitive */
      OP_LESS_THAN_OR_EQUAL_TO_IGNORECASE,     /**< This token is the same as OP_LESS_THAN_OR_EQUAL_TO, except it is case insensitive */
      OP_GREATER_THAN_OR_EQUAL_TO_IGNORECASE,  /**< This token is the same as OP_GREATER_THAN_OR_EQUAL_TO, except it is case insensitive */
      OP_NOT_EQUAL_TO_IGNORECASE,              /**< This token is the same as OP_GREATER_THAN_OR_EQUAL_TO, except it is case insensitive */
      OP_STARTS_WITH_IGNORECASE,               /**< This token is the same as OP_STARTS_WITH, except it is case insensitive */
      OP_ENDS_WITH_IGNORECASE,                 /**< This token is the same as OP_ENDS_WITH, except it is case insensitive */
      OP_CONTAINS_IGNORECASE,                  /**< This token is the same as OP_CONTAINS, except it is case insensitive */
      OP_START_OF_IGNORECASE,                  /**< This token is the same as OP_START_OF, except it is case insensitive */
      OP_END_OF_IGNORECASE,                    /**< This token is the same as OP_END_OF, except it is case insensitive */
      OP_SUBSTRING_OF_IGNORECASE,              /**< This token is the same as OP_SUBSTRING_OF, except it is case insensitive */
      OP_SIMPLE_WILDCARD_MATCH,                /**< This token represents a wildcard match, e.g. StringMatcher(myValue, true).Matches(nextValue) */
      OP_REGULAR_EXPRESSION_MATCH,             /**< This token represents a proper regex match, e.g. StringMatcher(myValue, false).Matches(nextValue) */
      NUM_STRING_OPERATORS                     /**< This is a guard token */
   };

   /** Returns true iff this filter will assume a default value if it can't find an actual value in the Message it tests. */
   bool IsAssumedDefault() const {return _assumeDefault;}

   /** Sets the assumed default value to the specified value.
     * @param d The value to match against if we don't find a value in the Message.
     */
   void SetAssumedDefault(const String & d) {_default = d; _assumeDefault = true;}

   /** Unsets the assumed default.  After calling this, our Match() method will simply always
     * return false if the specified data item is not found in the Message.
     */
   void UnsetAssumedDefault() {_default.Clear(); _assumeDefault = false;}

private:
   void FreeMatcher();
   bool DoMatch(const String & s) const;

   String _value;
   uint8 _op;

   bool _assumeDefault;
   String _default;

   mutable StringMatcher * _matcher;
};

/** This class matches on raw data buffers.  */
class RawDataQueryFilter : public ValueQueryFilter
{
public:
   /** Default constructor.  The byte buffer is set to a zero-length/NULL buffer, and the operator is set to OP_EQUAL_TO. */
   RawDataQueryFilter() : _op(OP_EQUAL_TO) {/* Empty */}

   /** Constructor.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The string to compare to the string found in the Message.
     * @param typeCode Typecode to look for in the target Message.  Default is B_ANY_TYPE, indicating that any type code is acceptable.
     * @param index Optional index of the item within the field.  Defaults to zero.
     */
   RawDataQueryFilter(const String & fieldName, uint8 op, const ByteBufferRef & value, uint32 typeCode = B_ANY_TYPE, uint32 index = 0) : ValueQueryFilter(fieldName, index), _value(value), _op(op), _typeCode(typeCode) {/* empty */}

   /** Constructor.  This constructor is similar to the constructor shown above,
     * except that when this constructor is used, if the specified item does not exist in 
     * the matched Message, this QueryFilter will act as if the Message contained the
     * specified assumedValue.  This is useful when the Message has been encoded with
     * the expectation that missing fields should be assumed equivalent to well known
     * default values.
     * @param fieldName Field name to look under.
     * @param op The operator to use (should be one of the OP_* values enumerated below)
     * @param value The string to compare to the string found in the Message.
     * @param typeCode Typecode to look for in the target Message.  Default is B_ANY_TYPE, indicating that any type code is acceptable.
     * @param index Optional index of the item within the field.  Defaults to zero.
     * @param assumedValue The value to use if no actual value is found at the specified location in the Message we are filtering.
     */
   RawDataQueryFilter(const String & fieldName, uint8 op, const ByteBufferRef & value, uint32 typeCode, uint32 index, const ByteBufferRef & assumedValue) : ValueQueryFilter(fieldName, index), _value(value), _op(op), _typeCode(typeCode), _default(assumedValue) {/* empty */}

   virtual status_t SaveToArchive(Message & archive) const;
   virtual status_t SetFromArchive(const Message & archive);
   virtual uint32 TypeCode() const {return QUERY_FILTER_TYPE_RAWDATA;}
   virtual bool Matches(ConstMessageRef & msg, const DataNode * optNode) const;

   /** Set the operator to use.  
     * @param op One of the OP_* values enumerated below.
     */
   void SetOperator(uint8 op) {_op = op;}

   /** Returns the currently specified operator, as specified in the constructor or in SetOperator() */
   uint8 GetOperator() const {return _op;}

   /** Set the value to compare against.
     * @param value The new value.
     */
   void SetValue(const ByteBufferRef & value) {_value = value;}

   /** Sets the type code that we will look for in the target Message.
     * @param typeCode the type code to look for.  Use B_ANY_TYPE to indicate that you don't care what the type code is.
     */
   void SetTypeCode(uint32 typeCode) {_typeCode = typeCode;}

   /** Returns the type code we are to look in the target Message for.
     * Note that this method is different from TypeCode()!
     */
   uint32 GetTypeCode() const {return _typeCode;}

   /** Returns the currently specified value, as specified in the constructor or in SetValue() */
   ByteBufferRef GetValue() const {return _value;}
 
   enum {
      OP_EQUAL_TO = 0,              /**< This token represents '==' */
      OP_LESS_THAN,                 /**< This token represents '<'  */
      OP_GREATER_THAN,              /**< This token represents '>'  */
      OP_LESS_THAN_OR_EQUAL_TO,     /**< This token represents '<=' */
      OP_GREATER_THAN_OR_EQUAL_TO,  /**< This token represents '>=' */
      OP_NOT_EQUAL_TO,              /**< This token represents '!=' */
      OP_STARTS_WITH,               /**< This token represents a prefix match, e.g. nextValue.StartsWith(myValue) */
      OP_ENDS_WITH,                 /**< This token represents a suffix match, e.g. nextValue.EndsWith(myValue)   */
      OP_CONTAINS,                  /**< This token represents an infix match, e.g. (nextValue.IndexOf(myValue)>=0) */
      OP_START_OF,                  /**< This token represents an inverse prefix match, e.g. myValue.StartsWith(nextValue) */
      OP_END_OF,                    /**< This token represents an inverse suffix match, e.g. myValue.EndsWith(nextValue) */
      OP_SUBSET_OF,                 /**< This token represents an inverse infix match,  e.g. (myValue.IndexOf(nextValue)>=0) */
      NUM_RAWDATA_OPERATORS         /**< This is a guard value */
   };

   /** Call this to specify an assumed default value that should be used when the
     * Message we are matching against doesn't have an actual value itself. 
     * Call this with a NULL reference if you don't want to use an assumed default value.
     */ 
   void SetAssumedDefault(const ByteBufferRef & bufRef) {_default = bufRef;}

   /** Returns the current assumed default value, or a NULL reference if there is none. */
   const ByteBufferRef & GetAssumedDefault() const {return _default;}

private:
   ByteBufferRef _value;
   uint8 _op;
   uint32 _typeCode;
   ByteBufferRef _default;
};

/** Interface for any object that knows how to instantiate QueryFilter objects */
class QueryFilterFactory : public RefCountable
{
public:
   /** Default constructor */
   QueryFilterFactory() {/* empty */}

   /** Destructor */
   virtual ~QueryFilterFactory() {/* empty */}

   /** Attempts to create and return a QueryFilter object from the given typeCode.
     * @param typeCode One of the QUERY_FILTER_TYPE_* values enumerated above.
     * @returns Reference to the new QueryFilter object on success, or a NULL reference on failure.
     */
   virtual QueryFilterRef CreateQueryFilter(uint32 typeCode) const = 0;

   /** Convenience method:  Attempts to create, populate, and return a QueryFilter object from 
     *                      the given Message, by first calling CreateQueryFilter(msg.what),
     *                      and then calling SetFromArchive(msg) on the return QueryFilter object.
     * @param msg A Message object that was previously filled out by the SaveToArchive() method
     *            of a QueryFilter object.
     * @returns Reference to the new QueryFilter object on success, or a NULL reference on failure.
     * @note Do not override this method in subclasses; override CreateQueryFilter(uint32) instead.
     */
   QueryFilterRef CreateQueryFilter(const Message & msg) const;
};
DECLARE_REFTYPES(QueryFilterFactory);

/** This class is MUSCLE's built-in implementation of a QueryFilterFactory.
  * It knows how to create all of the filter types listed in the QUERY_FILTER_TYPE_*
  * enum above.
  */
class MuscleQueryFilterFactory : public QueryFilterFactory
{
public:
   /** Default ctor */
   MuscleQueryFilterFactory() {/* empty */}

   virtual QueryFilterRef CreateQueryFilter(uint32 typeCode) const;
};

/** Returns a reference to the globally installed QueryFilterFactory object
  * that is used to create QueryFilter objects.  This method is guaranteed
  * never to return a NULL reference -- even if you call 
  * SetglobalQueryFilterFactory(QueryFilterFactoryRef()), this method
  * will fall back to returning a reference to a MuscleQueryFilterFactory
  * object (which is also what it does by default).
  */
QueryFilterFactoryRef GetGlobalQueryFilterFactory();

/** Call this method if you want to install a custom QueryFilterFactory
  * object as the global QueryFilterFactory.  Calling this method with
  * a NULL reference will revert the system back to using the default
  * MuscleQueryFilterFactory object.
  */
void SetGlobalQueryFilterFactory(const QueryFilterFactoryRef & newFactory);

}; // end namespace muscle

#endif
