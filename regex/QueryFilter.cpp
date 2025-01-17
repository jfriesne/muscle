/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/DataNode.h"
#include "regex/QueryFilter.h"
#include "regex/StringMatcher.h"
#include "util/MiscUtilityFunctions.h"  // for MemMem()

namespace muscle {

status_t QueryFilter :: SaveToArchive(Message & archive) const
{
   archive.what = TypeCode();
   return B_NO_ERROR;
}

status_t QueryFilter :: SetFromArchive(const Message & archive)
{
   return AcceptsTypeCode(archive.what) ? B_NO_ERROR : B_TYPE_MISMATCH;
}

status_t WhatCodeQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(QueryFilter::SaveToArchive(archive));

   return archive.CAddInt32("min", _minWhatCode)
        | archive.CAddInt32("max", _maxWhatCode, _minWhatCode);
}

status_t WhatCodeQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(QueryFilter::SetFromArchive(archive));

   _minWhatCode = archive.GetInt32("min");
   _maxWhatCode = archive.GetInt32("max", _minWhatCode);
   return B_NO_ERROR;
}

bool WhatCodeQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * /*optNode*/) const
{
   return muscleInRange(msg()->what, _minWhatCode, _maxWhatCode);
}

uint32 WhatCodeQueryFilter :: CalculateChecksum() const
{
   return QueryFilter::CalculateChecksum() + CalculatePODChecksums(_minWhatCode, _maxWhatCode);
}

bool WhatCodeQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const WhatCodeQueryFilter * wcrhs = QueryFilter::IsEqualTo(rhs) ? dynamic_cast<const WhatCodeQueryFilter *>(&rhs) : NULL;
   return ((wcrhs)&&(_minWhatCode == wcrhs->_minWhatCode)&&(_maxWhatCode == wcrhs->_maxWhatCode));
}

status_t ValueQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(QueryFilter::SaveToArchive(archive));

   return archive.AddString("fn", _fieldName)
        | archive.CAddInt32("idx", _index);
}

status_t ValueQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(QueryFilter::SetFromArchive(archive));

   _index = archive.GetInt32("idx");
   return archive.FindString("fn", _fieldName);
}

uint32 ValueQueryFilter :: CalculateChecksum() const
{
   return QueryFilter::CalculateChecksum() + CalculatePODChecksums(_fieldName, _index);
}

bool ValueQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const ValueQueryFilter * vrhs = QueryFilter::IsEqualTo(rhs) ? dynamic_cast<const ValueQueryFilter *>(&rhs) : NULL;
   return ((vrhs)&&(_fieldName == vrhs->_fieldName)&&(_index == vrhs->_index));
}

status_t ValueExistsQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(ValueQueryFilter::SaveToArchive(archive));

   return archive.CAddInt32("type", _typeCode, B_ANY_TYPE);
}

status_t ValueExistsQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(ValueQueryFilter::SetFromArchive(archive));

   _typeCode = archive.GetInt32("type", B_ANY_TYPE);
   return B_NO_ERROR;
}

bool ValueExistsQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * /*optNode*/) const
{
   const void * junk;
   return msg()->FindData(GetFieldName(), _typeCode, &junk, NULL).IsOK();
}

uint32 ValueExistsQueryFilter :: CalculateChecksum() const
{
   return ValueQueryFilter::CalculateChecksum() + CalculatePODChecksum(_typeCode);
}

bool ValueExistsQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const ValueExistsQueryFilter * verhs = QueryFilter::IsEqualTo(rhs) ? dynamic_cast<const ValueExistsQueryFilter *>(&rhs) : NULL;
   return ((verhs)&&(_typeCode == verhs->_typeCode));
}

status_t MultiQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(QueryFilter::SaveToArchive(archive));

   const uint32 numChildren = _children.GetNumItems();
   for (uint32 i=0; i<numChildren; i++)
   {
      const QueryFilter * nextChild = _children[i]();
      if (nextChild) MRETURN_ON_ERROR(archive.AddArchiveMessage("kid", *nextChild));
   }
   return B_NO_ERROR;
}

status_t MultiQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(QueryFilter::SetFromArchive(archive));

   _children.Clear();
   ConstMessageRef next;
   for (uint32 i=0; archive.FindMessage("kid", i, next).IsOK(); i++)
   {
      ConstQueryFilterRef kid = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*next());
      if (kid() == NULL) return B_ERROR("CreateQueryFilter() failed");
      MRETURN_ON_ERROR(_children.AddTail(kid));
   }
   return B_NO_ERROR;
}

uint32 MultiQueryFilter :: CalculateChecksum() const
{
   return QueryFilter::CalculateChecksum() + CalculatePODChecksum(_children);
}

bool MultiQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const MultiQueryFilter * mrhs = QueryFilter::IsEqualTo(rhs) ? dynamic_cast<const MultiQueryFilter *>(&rhs) : NULL;
   if ((mrhs==NULL)||(_children.GetNumItems() != mrhs->_children.GetNumItems())) return false;
   for (uint32 i=0; i<_children.GetNumItems(); i++) if (_children[i].IsDeeplyEqualTo(mrhs->_children[i]) == false) return false;
   return true;
}

status_t MinimumThresholdQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(MultiQueryFilter::SaveToArchive(archive));

   return archive.CAddInt32("min", _minMatches, MUSCLE_NO_LIMIT);
}

status_t MinimumThresholdQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(MultiQueryFilter::SetFromArchive(archive));

   _minMatches = archive.GetInt32("min", MUSCLE_NO_LIMIT);
   return B_NO_ERROR;
}

uint32 MinimumThresholdQueryFilter :: CalculateChecksum() const
{
   return MultiQueryFilter::CalculateChecksum() + CalculatePODChecksum(_minMatches);
}

bool MinimumThresholdQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const MinimumThresholdQueryFilter * mrhs = MultiQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const MinimumThresholdQueryFilter *>(&rhs) : NULL;
   return ((mrhs)&&(_minMatches == mrhs->_minMatches));
}

static bool ThresholdMaxAux(const Queue<ConstQueryFilterRef> & kids, uint32 numMatches, ConstMessageRef & msg, const DataNode * optNode)
{
   const uint32 numKids = kids.GetNumItems();
   if (numKids == 0) return true;  // avoid potential underflow in next line

   const uint32 threshold = muscleMin(numMatches, numKids-1);
   uint32 matchCount = 0;
   for (uint32 i=0; i<numKids; i++)
   {
      if ((1+threshold-matchCount) > (numKids-i)) break;  // might as well give up, even all-true wouldn't get us there now

      const QueryFilter * next = kids[i]();
      if ((next)&&(next->Matches(msg, optNode))&&(++matchCount > threshold)) return true;
   }
   return false;
}

bool MinimumThresholdQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   return ThresholdMaxAux(GetChildren(), _minMatches, msg, optNode);
}

bool MaximumThresholdQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   return (ThresholdMaxAux(GetChildren(), _maxMatches, msg, optNode) == false);
}

status_t MaximumThresholdQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(MultiQueryFilter::SaveToArchive(archive));

   return archive.CAddInt32("max", _maxMatches);
}

status_t MaximumThresholdQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(MultiQueryFilter::SetFromArchive(archive));

   _maxMatches = archive.GetInt32("max");
   return B_NO_ERROR;
}

uint32 MaximumThresholdQueryFilter :: CalculateChecksum() const
{
   return MultiQueryFilter::CalculateChecksum() + CalculatePODChecksum(_maxMatches);
}

bool MaximumThresholdQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const MaximumThresholdQueryFilter * mrhs = MultiQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const MaximumThresholdQueryFilter *>(&rhs) : NULL;
   return ((mrhs)&&(_maxMatches == mrhs->_maxMatches));
}

bool XorQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   const Queue<ConstQueryFilterRef> & kids = GetChildren();
   const uint32 numKids = kids.GetNumItems();
   uint32 matchCount = 0;
   for (uint32 i=0; i<numKids; i++)
   {
      const QueryFilter * next = kids[i]();
      if ((next)&&(next->Matches(msg, optNode))) matchCount++;
   }
   return ((matchCount % 2) != 0) ? true : false;
}

status_t MessageQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(ValueQueryFilter::SaveToArchive(archive));

   if (_childFilter()) MRETURN_ON_ERROR(archive.AddArchiveMessage("kid", *_childFilter()));
   return B_NO_ERROR;
}

status_t MessageQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(ValueQueryFilter::SetFromArchive(archive));

   ConstMessageRef subMsg;
   if (archive.FindMessage("kid", subMsg).IsOK())
   {
      _childFilter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*subMsg());
      if (_childFilter() == NULL) return B_ERROR("CreateQueryFilter() failed");
   }
   else _childFilter.Reset();

   return B_NO_ERROR;
}

bool MessageQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   ConstMessageRef subMsg;
   if (msg()->FindMessage(GetFieldName(), GetIndex(), subMsg).IsError()) return false;
   if (_childFilter() == NULL) return true;

   ConstMessageRef constSubMsg = std_move_if_available(subMsg);
   return _childFilter()->Matches(constSubMsg, optNode);
}

uint32 MessageQueryFilter :: CalculateChecksum() const
{
   return ValueQueryFilter::CalculateChecksum() + CalculatePODChecksum(_childFilter);
}

bool MessageQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const MessageQueryFilter * mrhs = ValueQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const MessageQueryFilter *>(&rhs) : NULL;
   return ((mrhs)&&(_childFilter.IsDeeplyEqualTo(mrhs->_childFilter)));
}

status_t StringQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(ValueQueryFilter::SaveToArchive(archive));

   status_t ret;
   return ((archive.AddString("val", _value).IsOK(ret))&&((_assumeDefault == false)||(archive.AddString("val", _default).IsOK(ret)))) ? archive.AddInt8("op", _op) : ret;
}

status_t StringQueryFilter :: SetFromArchive(const Message & archive)
{
   FreeMatcher();
   _default.Clear();
   _assumeDefault = (archive.FindString("val", 1, _default).IsOK());

   MRETURN_ON_ERROR(ValueQueryFilter::SetFromArchive(archive));

   status_t ret;
   return archive.FindString("val", _value).IsOK(ret) ? archive.FindInt8("op", _op) : ret;
}

uint32 StringQueryFilter :: CalculateChecksum() const
{
   return ValueQueryFilter::CalculateChecksum() + CalculatePODChecksums(_value, _op, _assumeDefault, _default); // deliberately not including _matcher as it's only an optimization
}

bool StringQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const StringQueryFilter * srhs = ValueQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const StringQueryFilter *>(&rhs) : NULL;
   return (srhs           != NULL)
       && (_value         == srhs->_value)
       && (_op            == srhs->_op)
       && (_assumeDefault == srhs->_assumeDefault)
       && (_default       == srhs->_default);  // deliberately not including _matcher as it's only an optimization
}

bool NodeNameQueryFilter :: Matches(ConstMessageRef &, const DataNode * dataNode) const
{
   return ((dataNode)&&(MatchesString(dataNode->GetNodeName())));
}

bool StringQueryFilter :: Matches(ConstMessageRef & msg, const DataNode *) const
{
   const String * ps;
   if (msg()->FindString(GetFieldName(), GetIndex(), &ps).IsError())
   {
      if (_assumeDefault) ps = &_default;
                     else return false;
   }
   return MatchesString(*ps);
}

bool StringQueryFilter :: MatchesString(const String & s) const
{
   switch(_op)
   {
      case OP_EQUAL_TO:                            return s == _value;
      case OP_LESS_THAN:                           return s  < _value;
      case OP_GREATER_THAN:                        return s  > _value;
      case OP_LESS_THAN_OR_EQUAL_TO:               return s <= _value;
      case OP_GREATER_THAN_OR_EQUAL_TO:            return s >= _value;
      case OP_NOT_EQUAL_TO:                        return s != _value;
      case OP_STARTS_WITH:                         return s.StartsWith(_value);
      case OP_ENDS_WITH:                           return s.EndsWith(_value);
      case OP_CONTAINS:                            return (s.IndexOf(_value) >= 0);
      case OP_START_OF:                            return _value.StartsWith(s);
      case OP_END_OF:                              return _value.EndsWith(s);
      case OP_SUBSTRING_OF:                        return (_value.IndexOf(s) >= 0);
      case OP_EQUAL_TO_IGNORECASE:                 return s.EqualsIgnoreCase(_value);
      case OP_LESS_THAN_IGNORECASE:                return (s.CompareToIgnoreCase(_value) < 0);
      case OP_GREATER_THAN_IGNORECASE:             return (s.CompareToIgnoreCase(_value) > 0);
      case OP_LESS_THAN_OR_EQUAL_TO_IGNORECASE:    return (s.CompareToIgnoreCase(_value) <= 0);
      case OP_GREATER_THAN_OR_EQUAL_TO_IGNORECASE: return (s.CompareToIgnoreCase(_value) >= 0);
      case OP_NOT_EQUAL_TO_IGNORECASE:             return (s.EqualsIgnoreCase(_value) == false);
      case OP_STARTS_WITH_IGNORECASE:              return s.StartsWithIgnoreCase(_value);
      case OP_ENDS_WITH_IGNORECASE:                return s.EndsWithIgnoreCase(_value);
      case OP_CONTAINS_IGNORECASE:                 return (s.IndexOfIgnoreCase(_value) >= 0);
      case OP_START_OF_IGNORECASE:                 return _value.StartsWith(s);
      case OP_END_OF_IGNORECASE:                   return _value.EndsWith(s);
      case OP_SUBSTRING_OF_IGNORECASE:             return (_value.IndexOf(s) >= 0);
      case OP_SIMPLE_WILDCARD_MATCH:               return DoMatch(s);
      case OP_REGULAR_EXPRESSION_MATCH:            return DoMatch(s);
      default:                                     /* do nothing */  break;
   }
   return false;
}

void StringQueryFilter :: FreeMatcher()
{
   delete _matcher;
   _matcher = NULL;
}

bool StringQueryFilter :: DoMatch(const String & s) const
{
   if (_matcher == NULL)
   {
      switch(_op)
      {
         case OP_SIMPLE_WILDCARD_MATCH:
            _matcher = new StringMatcher(_value, true);
         break;

         case OP_REGULAR_EXPRESSION_MATCH:
            _matcher = new StringMatcher(_value, false);
         break;

         default:
            return false;
      }
   }
   return _matcher ? _matcher->Match(s()) : false;
}

status_t RawDataQueryFilter :: SaveToArchive(Message & archive) const
{
   MRETURN_ON_ERROR(ValueQueryFilter::SaveToArchive(archive));
   MRETURN_ON_ERROR(archive.AddInt8("op", _op));
   MRETURN_ON_ERROR(archive.CAddInt32("type", _typeCode, B_ANY_TYPE));

   const ByteBuffer * bb = _value();
   if (bb)
   {
      const uint32 numBytes = bb->GetNumBytes();
      const uint8 * bytes = bb->GetBuffer();
      if ((bytes)&&(numBytes > 0)) MRETURN_ON_ERROR(archive.AddData("val", B_RAW_TYPE, bytes, numBytes));
   }

   const ByteBuffer * dd = _default();
   if (dd)
   {
      const uint32 numBytes = dd->GetNumBytes();
      const uint8 * bytes = dd->GetBuffer();
      if (bytes) MRETURN_ON_ERROR(archive.AddData("def", B_RAW_TYPE, bytes, numBytes));  // I'm deliberately not testing if (numBytes>0) here!
   }

   return B_NO_ERROR;
}

status_t RawDataQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(ValueQueryFilter::SetFromArchive(archive));
   MRETURN_ON_ERROR(archive.FindInt8("op", _op));

   _typeCode = archive.GetInt32("type", B_ANY_TYPE);

   _value.Reset();
   const void * data;
   uint32 numBytes;
   if (archive.FindData("val", B_RAW_TYPE, &data, &numBytes).IsOK())
   {
      _value = GetByteBufferFromPool(numBytes, (const uint8 *) data);
      MRETURN_ON_ERROR(_value);
   }

   _default.Reset();
   if (archive.FindData("def", B_RAW_TYPE, &data, &numBytes).IsOK())
   {
      _default = GetByteBufferFromPool(numBytes, (const uint8 *) data);
      MRETURN_ON_ERROR(_default);
   }

   return B_NO_ERROR;
}

bool RawDataQueryFilter :: Matches(ConstMessageRef & msg, const DataNode *) const
{
   const void * hb;
   uint32 hisNumBytes;
   if (msg()->FindData(GetFieldName(), _typeCode, &hb, &hisNumBytes).IsError())
   {
      if (_default())
      {
         hb = _default()->GetBuffer();
         hisNumBytes = _default()->GetNumBytes();
      }
      else return false;
   }

   const uint8 * hisBytes  = (const uint8 *) hb;
   const uint32 myNumBytes = _value() ? _value()->GetNumBytes() : 0;
   const uint8 * myBytes   = _value() ? _value()->GetBuffer()   : NULL;
   const uint32 clen       = muscleMin(myNumBytes, hisNumBytes);
   if (myBytes == NULL) return false;

   switch(_op)
   {
      case OP_EQUAL_TO:     return ((hisNumBytes == myNumBytes)&&(memcmp(myBytes, hisBytes, clen) == 0));

      case OP_LESS_THAN:
      {
         const int mret = memcmp(hisBytes, myBytes, clen);
         if (mret < 0) return true;
         return (mret == 0) ? (hisNumBytes < myNumBytes) : false;
      }

      case OP_GREATER_THAN:
      {
         const int mret = memcmp(hisBytes, myBytes, clen);
         if (mret > 0) return true;
         return (mret == 0) ? (hisNumBytes > myNumBytes) : false;
      }

      case OP_LESS_THAN_OR_EQUAL_TO:
      {
         const int mret = memcmp(hisBytes, myBytes, clen);
         if (mret < 0) return true;
         return (mret == 0) ? (hisNumBytes <= myNumBytes) : false;
      }

      case OP_GREATER_THAN_OR_EQUAL_TO:
      {
         const int mret = memcmp(hisBytes, myBytes, clen);
         if (mret > 0) return true;
         return (mret == 0) ? (hisNumBytes >= myNumBytes) : false;
      }

      case OP_NOT_EQUAL_TO: return ((hisNumBytes != myNumBytes)||(memcmp(myBytes, hisBytes, clen) != 0));
      case OP_STARTS_WITH:  return ((myNumBytes <= hisNumBytes)&&(memcmp(myBytes, hisBytes, clen) == 0));
      case OP_ENDS_WITH:    return ((myNumBytes <= hisNumBytes)&&(memcmp(&myBytes[myNumBytes-clen], &hisBytes[hisNumBytes-clen], clen) == 0));
      case OP_CONTAINS:     return (MemMem(hisBytes, hisNumBytes, myBytes, myNumBytes) != NULL);
      case OP_START_OF:     return ((hisNumBytes <= myNumBytes)&&(memcmp(hisBytes, myBytes, clen) == 0));
      case OP_END_OF:       return ((hisNumBytes <= myNumBytes)&&(memcmp(&hisBytes[hisNumBytes-clen], &myBytes[myNumBytes-clen], clen) == 0));
      case OP_SUBSET_OF:    return (MemMem(myBytes, myNumBytes, hisBytes, hisNumBytes) != NULL);
      default:              /* do nothing */  break;
   }
   return false;
}

uint32 RawDataQueryFilter :: CalculateChecksum() const
{
   return ValueQueryFilter::CalculateChecksum() + CalculatePODChecksums(_op, _typeCode, _value, _default);
}

bool RawDataQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const RawDataQueryFilter * rrhs = ValueQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const RawDataQueryFilter *>(&rhs) : NULL;
   return (rrhs      != NULL)
       && (_op       == rrhs->_op)
       && (_typeCode == rrhs->_typeCode)
       && (_value.IsDeeplyEqualTo(rrhs->_value))
       && (_default.IsDeeplyEqualTo(rrhs->_default));
}

bool ChildCountQueryFilter :: Matches(ConstMessageRef & /*msg*/, const DataNode * optNode) const
{
   Message temp; (void) temp.AddInt32(GetEmptyString(), optNode ? optNode->GetNumChildren() : 0);
   DummyConstMessageRef tempRef(temp);
   return NumericQueryFilter<int32, B_INT32_TYPE, QUERY_FILTER_TYPE_CHILDCOUNT>::Matches(tempRef, optNode);
}

QueryFilterRef QueryFilterFactory :: CreateQueryFilter(const Message & msg) const
{
   QueryFilterRef ret = CreateQueryFilter(msg.what);
   if ((ret())&&(ret()->SetFromArchive(msg).IsError())) ret.Reset();
   return ret;
}

QueryFilterRef MuscleQueryFilterFactory :: CreateQueryFilter(uint32 typeCode) const
{
   QueryFilter * f = NULL;
   switch(typeCode)
   {
      case QUERY_FILTER_TYPE_WHATCODE:    f = new WhatCodeQueryFilter;    break;
      case QUERY_FILTER_TYPE_VALUEEXISTS: f = new ValueExistsQueryFilter; break;
      case QUERY_FILTER_TYPE_BOOL:        f = new BoolQueryFilter;        break;
      case QUERY_FILTER_TYPE_DOUBLE:      f = new DoubleQueryFilter;      break;
      case QUERY_FILTER_TYPE_FLOAT:       f = new FloatQueryFilter;       break;
      case QUERY_FILTER_TYPE_INT64:       f = new Int64QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT32:       f = new Int32QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT16:       f = new Int16QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT8:        f = new Int8QueryFilter;        break;
      case QUERY_FILTER_TYPE_POINT:       f = new PointQueryFilter;       break;
      case QUERY_FILTER_TYPE_RECT:        f = new RectQueryFilter;        break;
      case QUERY_FILTER_TYPE_STRING:      f = new StringQueryFilter;      break;
      case QUERY_FILTER_TYPE_MESSAGE:     f = new MessageQueryFilter;     break;
      case QUERY_FILTER_TYPE_RAWDATA:     f = new RawDataQueryFilter;     break;
      case QUERY_FILTER_TYPE_MAXMATCH:    f = new MaximumThresholdQueryFilter(0); break;
      case QUERY_FILTER_TYPE_MINMATCH:    f = new MinimumThresholdQueryFilter(0); break;
      case QUERY_FILTER_TYPE_XOR:         f = new XorQueryFilter;         break;
      case QUERY_FILTER_TYPE_CHILDCOUNT:  f = new ChildCountQueryFilter;  break;
      case QUERY_FILTER_TYPE_NODENAME:    f = new NodeNameQueryFilter;    break;
      default:                            return B_UNIMPLEMENTED;  /* unknown type code! */
   }
   return QueryFilterRef(f);
}

static MuscleQueryFilterFactory _defaultQueryFilterFactory;
static const DummyQueryFilterFactoryRef _defaultQueryFactoryRef(_defaultQueryFilterFactory);
static QueryFilterFactoryRef _customQueryFilterFactoryRef;

const QueryFilterFactoryRef & GetGlobalQueryFilterFactory()
{
   if (_customQueryFilterFactoryRef()) return _customQueryFilterFactoryRef;
   return _defaultQueryFactoryRef;  // don't combine these two lines, they return different types so ternary operator won't work
}

void SetGlobalQueryFilterFactory(const QueryFilterFactoryRef & newFactory)
{
   _customQueryFilterFactoryRef = newFactory;
}

} // end namespace muscle
