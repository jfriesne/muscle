/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/DataNode.h"
#include "regex/ISubexpressionFactory.h"
#include "regex/QueryFilter.h"
#include "regex/StringMatcher.h"
#include "util/MiscUtilityFunctions.h"  // for MemMem()
#include "util/OutputPrinter.h"
#include "util/StringTokenizer.h"

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

void QueryFilter :: Print(const OutputPrinter & p) const
{
   p.printf("%s: ", GetUnmangledSymbolName(typeid(*this).name())());  // deliberately not including a newline here
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

void WhatCodeQueryFilter :: Print(const OutputPrinter & p) const
{
   QueryFilter::Print(p);
   p.printf(" _minWhatCode=" UINT32_FORMAT_SPEC " _maxWhatCode=" UINT32_FORMAT_SPEC "\n", _minWhatCode, _maxWhatCode);
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

void ValueQueryFilter :: Print(const OutputPrinter & p) const
{
   QueryFilter::Print(p);

   // deliberately not including a newline here; subclasses will add to this line and then add their own newline char
   p.printf(" _fieldName=[%s] _index=" UINT32_FORMAT_SPEC, _fieldName(), _index);
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

void ValueExistsQueryFilter :: Print(const OutputPrinter & p) const
{
   ValueQueryFilter::Print(p);
   char buf[5]; MakePrettyTypeCodeString(_typeCode, buf);
   p.printf(" _typeCode=" UINT32_FORMAT_SPEC "/%s\n", _typeCode, buf);
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

void MultiQueryFilter :: Print(const OutputPrinter & p) const
{
   QueryFilter::Print(p);
   p.printf(" _children=" UINT32_FORMAT_SPEC ":\n", _children.GetNumItems());
   for (uint32 i=0; i<_children.GetNumItems(); i++) _children[i]()->Print(p.WithIndent(3));
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
   return archive.CAddMessage("defmsg", CastAwayConstFromRef(_optDefaultChildMessage));
}

status_t MessageQueryFilter :: SetFromArchive(const Message & archive)
{
   MRETURN_ON_ERROR(ValueQueryFilter::SetFromArchive(archive));

   _optDefaultChildMessage = archive.GetMessage("defmsg");

   ConstMessageRef subMsg;
   if (archive.FindMessage("kid", subMsg).IsOK())
   {
      _childFilter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*subMsg());
      if (_childFilter() == NULL) return B_ERROR("CreateQueryFilter() failed");
   }
   else _childFilter.Reset();

   return B_NO_ERROR;
}

void MessageQueryFilter :: Print(const OutputPrinter & p) const
{
   ValueQueryFilter::Print(p);
   if (_childFilter())
   {
      p.printf(" _childFilter=");
      _childFilter()->Print(p.WithIndent(3));
   }
   if (_optDefaultChildMessage())
   {
      p.printf(" optDefaultChildMessage=");
      _optDefaultChildMessage()->Print(p.WithIndent(3));
   }
}

bool MessageQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   ConstMessageRef subMsg;
   if (msg()->FindMessage(GetFieldName(), GetIndex(), subMsg).IsError()) subMsg = _optDefaultChildMessage;
   if (subMsg() == NULL) return false;

   if (_childFilter() == NULL) return true;

   ConstMessageRef constSubMsg = std_move_if_available(subMsg);
   return _childFilter()->Matches(constSubMsg, optNode);
}

uint32 MessageQueryFilter :: CalculateChecksum() const
{
   return ValueQueryFilter::CalculateChecksum() + CalculatePODChecksums(_childFilter, _optDefaultChildMessage);
}

bool MessageQueryFilter :: IsEqualTo(const QueryFilter & rhs) const
{
   const MessageQueryFilter * mrhs = ValueQueryFilter::IsEqualTo(rhs) ? dynamic_cast<const MessageQueryFilter *>(&rhs) : NULL;
   if (mrhs == NULL) return false;
   if (_childFilter.IsDeeplyEqualTo(mrhs->_childFilter) == false) return false;
   if (_optDefaultChildMessage.IsDeeplyEqualTo(mrhs->_optDefaultChildMessage) == false) return false;
   return true;
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

void StringQueryFilter :: Print(const OutputPrinter & p) const
{
   ValueQueryFilter::Print(p);
   p.printf(" _op=%u _value=[%s] _assumeDefault=%i _default=[%s]\n", _op, _value(), _assumeDefault, _default());
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

void RawDataQueryFilter :: Print(const OutputPrinter & p) const
{
   ValueQueryFilter::Print(p);
   p.printf(" _op=%u _typeCode=" UINT32_FORMAT_SPEC " _value=[%s] _default=[%s]\n", _op, _typeCode, HexBytesToString(_value)(), HexBytesToString(_default)());
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

static const char * _tokStrs[] =
{
   "(",              // LTOKEN_LPAREN
   ")",              // LTOKEN_RPAREN
   "!",              // LTOKEN_NOT (must come before LTOKEN_NEQ)
   "<",              // LTOKEN_LT
   ">",              // LTOKEN_GT
   "==",             // LTOKEN_EQ
   "<=",             // LTOKEN_LEQ
   ">=",             // LTOKEN_GEQ
   "!=",             // LTOKEN_NEQ
   "&&",             // LTOKEN_AND
   "||",             // LTOKEN_OR
   "^",              // LTOKEN_XOR
   "startswith ",    // LTOKEN_STARTSWITH    (space is intentional)
   "endswith ",      // LTOKEN_ENDSWITH      (space is intentional)
   "contains ",      // LTOKEN_CONTAINS      (space is intentional)
   "isstartof ",     // LTOKEN_ISSTARTOF     (space is intentional)
   "isendof ",       // LTOKEN_ISENDOF       (space is intentional)
   "issubstringof ", // LTOKEN_ISSUBSTRINGOF (space is intentional)
   "matches ",       // LTOKEN_MATCHES       (space is intentional)
   "matchesregex ",  // LTOKEN_MATCHESREGEX  (space is intentional)
   "(int64)",        // LTOKEN_INT64
   "(int32)",        // LTOKEN_INT32
   "(int16)",        // LTOKEN_INT16
   "(int8)",         // LTOKEN_INT8
   "(bool)",         // LTOKEN_BOOL
   "(float)",        // LTOKEN_FLOAT
   "(double)",       // LTOKEN_DOUBLE
   "(string)",       // LTOKEN_STRING
   "(point)",        // LTOKEN_POINT
   "(rect)",         // LTOKEN_RECT
   "what",           // LTOKEN_WHAT    (lack of space is intentional)
   "exists ",        // LTOKEN_EXISTS  (space is intentional)
   NULL,             // LTOKEN_VALUESTRING
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_tokStrs, NUM_LTOKENS);

static bool _tokStrlensNeedsInit = true;
static size_t _tokStrlens[NUM_LTOKENS] ;

template<typename T> T GetValueAs(const String & v);
template<> bool   GetValueAs(const String & v) {return    ParseBool(v());}
template<> double GetValueAs(const String & v) {return         atof(v());}
template<> float  GetValueAs(const String & v) {return (float) atof(v());}
template<> int64  GetValueAs(const String & v) {return        Atoll(v());}
template<> int32  GetValueAs(const String & v) {return (int32) atol(v());}
template<> int16  GetValueAs(const String & v) {return (int16) atol(v());}
template<> int8   GetValueAs(const String & v) {return (int8)  atol(v());}

template<> Point GetValueAs(const String & v)
{
   StringTokenizer tok(v(), ",");
   const char * xStr = tok();
   const char * yStr = tok();
   return Point(xStr ? (float) atof(xStr) : 0.0f, yStr ? (float) atof(yStr) : 0.0f);
}

template<> Rect GetValueAs(const String & v)
{
   StringTokenizer tok(v(), ",");
   const char * lStr = tok();
   const char * tStr = tok();
   const char * rStr = tok();
   const char * bStr = tok();
   return Rect(lStr ? (float) atof(lStr) : 0.0f,
               tStr ? (float) atof(tStr) : 0.0f,
               rStr ? (float) atof(rStr) : 0.0f,
               bStr ? (float) atof(bStr) : 0.0f);
}

// returns the B_*_TYPE code representing our value's type, or B_ANY_TYPE on failure/unknown
uint32 LexerToken :: GetValueStringType(uint32 explicitCastType) const
{
   if (_tok != LTOKEN_VALUESTRING) return B_ANY_TYPE;
   if (_wasQuoted) return (explicitCastType == B_ANY_TYPE) ? B_STRING_TYPE : B_ANY_TYPE;  // (int16)"hi" is too weird to let slide
   if (explicitCastType != B_ANY_TYPE) return explicitCastType;
   if ((_valStr.EqualsIgnoreCase("true"))||(_valStr.EqualsIgnoreCase("false"))) return B_BOOL_TYPE;

   const char c = _valStr.HasChars() ? _valStr[0] : '\0';
   if ((muscleIsDigit(c))||(c=='-')||(c=='.')||(c=='+'))
   {
      switch(_valStr.GetNumInstancesOf(','))
      {
         case 0:
         {
                 if (_valStr.EndsWith('f')) return B_FLOAT_TYPE;
            else if (_valStr.Contains('.')) return B_DOUBLE_TYPE;
            else return B_INT32_TYPE;  // I guess this is a good default?
         }
         break;

         case 1:  return B_POINT_TYPE;
         case 3:  return B_RECT_TYPE;
         default: return B_ANY_TYPE;
      }
   }
   else return _valStr.HasChars() ? B_STRING_TYPE : B_ANY_TYPE;
}

// Returns the B_*_TYPE associated with this token if this token is an explicit-cast, or B_ANY_TYPE otherwise
uint32 LexerToken :: GetExplicitCastTypeCode() const
{
   switch(_tok)
   {
      case LTOKEN_INT64:  return B_INT64_TYPE;
      case LTOKEN_INT32:  return B_INT32_TYPE;
      case LTOKEN_INT16:  return B_INT16_TYPE;
      case LTOKEN_INT8:   return B_INT8_TYPE;
      case LTOKEN_BOOL:   return B_BOOL_TYPE;
      case LTOKEN_FLOAT:  return B_FLOAT_TYPE;
      case LTOKEN_DOUBLE: return B_DOUBLE_TYPE;
      case LTOKEN_STRING: return B_STRING_TYPE;
      case LTOKEN_POINT:  return B_POINT_TYPE;
      case LTOKEN_RECT:   return B_RECT_TYPE;
      default:            return B_ANY_TYPE;
   }
}

String LexerToken :: ToString() const
{
   const String tokStr = (_tok < NUM_LTOKENS) ? _tokStrs[_tok] : "???";
   return _valStr.HasChars() ? (tokStr+String(" %1").Arg(_valStr)) : std_move_if_available(tokStr);
}

status_t LexerToken :: ParseFieldNameAux(const String & valStr, String & retFieldName, uint32 & retIdx, LexerToken * optRetDefaultValue) const
{
   if ((_tok != LTOKEN_VALUESTRING)||((_wasQuoted == false)&&(valStr.IsEmpty()))) return B_BAD_ARGUMENT;

   const int32 barIdx = ((_wasQuoted == false)&&(optRetDefaultValue)) ? valStr.LastIndexOf('|') : -1;
   if (barIdx >= 0)
   {
      *optRetDefaultValue = LexerToken(LTOKEN_VALUESTRING, valStr.Substring(barIdx+1), false);
      return ParseFieldNameAux(valStr.Substring(0, barIdx), retFieldName, retIdx, NULL);
   }

   const int32 colIdx = _wasQuoted ? -1 : valStr.LastIndexOf(':');
   if (colIdx > 0)
   {
      retFieldName = valStr.Substring(0, colIdx);
      retIdx       = (uint32) atol(valStr()+colIdx+1);
   }
   else
   {
      retFieldName = valStr;
      retIdx       = 0;
   }

   return B_NO_ERROR;
}

// Returns the StringQueryFilter::OP_* value associated with this infix operator, or StringQueryFilter::NUM_STRING_OPERATORS on failure
uint8 LexerToken :: GetStringQueryFilterOp(bool isIgnoreCase) const
{
   switch(GetToken())
   {
      case LTOKEN_EQ:            return isIgnoreCase ? StringQueryFilter::OP_EQUAL_TO_IGNORECASE                 : StringQueryFilter::OP_EQUAL_TO;                 // ==
      case LTOKEN_LT:            return isIgnoreCase ? StringQueryFilter::OP_LESS_THAN_IGNORECASE                : StringQueryFilter::OP_LESS_THAN;                // <
      case LTOKEN_GT:            return isIgnoreCase ? StringQueryFilter::OP_GREATER_THAN_IGNORECASE             : StringQueryFilter::OP_GREATER_THAN;             // >
      case LTOKEN_LEQ:           return isIgnoreCase ? StringQueryFilter::OP_LESS_THAN_OR_EQUAL_TO_IGNORECASE    : StringQueryFilter::OP_LESS_THAN_OR_EQUAL_TO;    // <=
      case LTOKEN_GEQ:           return isIgnoreCase ? StringQueryFilter::OP_GREATER_THAN_OR_EQUAL_TO_IGNORECASE : StringQueryFilter::OP_GREATER_THAN_OR_EQUAL_TO; // >=
      case LTOKEN_NEQ:           return isIgnoreCase ? StringQueryFilter::OP_NOT_EQUAL_TO_IGNORECASE             : StringQueryFilter::OP_NOT_EQUAL_TO;             // !=
      case LTOKEN_STARTSWITH:    return isIgnoreCase ? StringQueryFilter::OP_STARTS_WITH_IGNORECASE              : StringQueryFilter::OP_STARTS_WITH;              // startswith
      case LTOKEN_ENDSWITH:      return isIgnoreCase ? StringQueryFilter::OP_ENDS_WITH_IGNORECASE                : StringQueryFilter::OP_ENDS_WITH;                // endswith
      case LTOKEN_CONTAINS:      return isIgnoreCase ? StringQueryFilter::OP_CONTAINS_IGNORECASE                 : StringQueryFilter::OP_CONTAINS;                 // contains
      case LTOKEN_ISSTARTOF:     return isIgnoreCase ? StringQueryFilter::OP_START_OF_IGNORECASE                 : StringQueryFilter::OP_START_OF;                 // isstartof
      case LTOKEN_ISENDOF:       return isIgnoreCase ? StringQueryFilter::OP_END_OF_IGNORECASE                   : StringQueryFilter::OP_END_OF;                   // isendof
      case LTOKEN_ISSUBSTRINGOF: return isIgnoreCase ? StringQueryFilter::OP_SUBSTRING_OF_IGNORECASE             : StringQueryFilter::OP_SUBSTRING_OF;             // issubstringof
      case LTOKEN_MATCHES:       return StringQueryFilter::OP_SIMPLE_WILDCARD_MATCH;    // matches
      case LTOKEN_MATCHESREGEX:  return StringQueryFilter::OP_REGULAR_EXPRESSION_MATCH; // matchesregex
      default:                   return StringQueryFilter::NUM_STRING_OPERATORS;        // failure
   }
}

// Returns the NumericQueryFilter::OP_* value associated with this infix operator, or NumericQueryFilter::NUM_STRING_OPERATORS on failure
uint8 LexerToken :: GetNumericQueryFilterOp() const
{
   // Note that I just chose Int32QueryFilter here arbitrarily for convenience; the OP_* values are the same for all NumericQueryFilter template-instantiations
   switch(GetToken())
   {
      case LTOKEN_EQ:  return Int32QueryFilter::OP_EQUAL_TO;                 // ==
      case LTOKEN_LT:  return Int32QueryFilter::OP_LESS_THAN;                // <
      case LTOKEN_GT:  return Int32QueryFilter::OP_GREATER_THAN;             // >
      case LTOKEN_LEQ: return Int32QueryFilter::OP_LESS_THAN_OR_EQUAL_TO;    // <=
      case LTOKEN_GEQ: return Int32QueryFilter::OP_GREATER_THAN_OR_EQUAL_TO; // >=
      case LTOKEN_NEQ: return Int32QueryFilter::OP_NOT_EQUAL_TO;             // !=
      default:         return Int32QueryFilter::NUM_NUMERIC_OPERATORS;       // failure
   }
}

#define RETURN_ON_SYNONYM_FOR_TOKEN(s, syn, theTok) {if (Strncasecmp(s, syn, sizeof(syn)-1) == 0) {retNumCharsConsumed = sizeof(syn)-1; return theTok;}}

static int32 GetMatchingToken(const char * s, uint32 & retNumCharsConsumed)
{
   for (int32 i=ARRAYITEMS(_tokStrs)-1; i>=0; i--)
   {
      const char * ts    = _tokStrs[i];
      const size_t tsLen = _tokStrlens[i];
      if ((tsLen > 0)&&(Strncasecmp(s, ts, tsLen) == 0)) {retNumCharsConsumed = (uint32) _tokStrlens[i]; return i;}
   }

   // Some special-case synonyms, just to be user-friendly
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "and",    LTOKEN_AND);
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "or",     LTOKEN_OR);
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "xor",    LTOKEN_XOR);
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "not",    LTOKEN_NOT);
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "=",      LTOKEN_EQ);
   RETURN_ON_SYNONYM_FOR_TOKEN(s, "equals", LTOKEN_EQ);

   retNumCharsConsumed = 0;
   return -1;
}

class Lexer
{
public:
   Lexer(const String & expression) : _expression(expression), _curPos(0)
   {
      if (_tokStrlensNeedsInit)
      {
         for (uint32 i=0; i<ARRAYITEMS(_tokStrlens); i++) _tokStrlens[i] = _tokStrs[i] ? strlen(_tokStrs[i]) : 0;
         _tokStrlensNeedsInit = false;
      }
   }

   status_t GetNextToken(LexerToken & retTok)
   {
      while(_curPos < _expression.Length())
      {
         // Try to find the next fixed-format token
         const char * s = _expression()+_curPos;
         uint32 numCharsInToken = 0;
         const int32 matchedTok = GetMatchingToken(s, numCharsInToken);
         if (matchedTok >= 0)
         {
            _curPos += numCharsInToken;
            retTok   = LexerToken(matchedTok);
            return B_NO_ERROR;
         }

         // if we got here, we didn't find any fixed-format token, so we'll have to do some custom handling instead
         switch(*s)
         {
            case '\"':
            {
               s++;  // move past initial quote
               const char * endQuote = strchr(s, '\"');
               if (endQuote == NULL) return B_BAD_DATA;  // no closing quote

               retTok   = LexerToken(LTOKEN_VALUESTRING, String(s, (uint32) (endQuote-s)), true);
               _curPos += retTok.GetValueString().Length()+2;  // +2 for the two quotes
            }
            return B_NO_ERROR;

            case ' ': case '\t': case '\r': case '\n':
               _curPos++;  // ignore whitespace
            break;

            default:
            {
               // For anything else we just parse until the first known token or whitespace
               String retStr;
               const char * t = s;
               while(1)  // we'll handle the NUL char in the if statement below
               {
                  uint32 numCharsInToken = 0;
                  if (((*t == '\0')||(muscleIsSpace(*t)))||(GetMatchingToken(t, numCharsInToken) >= 0))
                  {
                     retTok   = LexerToken(LTOKEN_VALUESTRING, String(s, (uint32) (t-s)), false);
                     _curPos += retTok.GetValueString().Length();
                     return B_NO_ERROR;
                  }
                  else t++;
               }
            }
            break;
         }
      }
      return B_DATA_NOT_FOUND;
   }

private:
   String _expression;
   uint32 _curPos;
};

static ConstQueryFilterRef MaybeNegate(bool doNegate, const ConstQueryFilterRef & qf) {return ((doNegate)&&(qf())) ? QueryFilterRef(new NorQueryFilter(qf)) : qf;}

static ConstQueryFilterRef CreateQueryFilterFromExpressionAux(Lexer & lexer, const ISubexpressionFactory & sef)
{
   Queue<LexerToken> localToks;

   LexerToken conjunctionTok;
   MultiQueryFilterRef conjunctionRef;

   ConstQueryFilterRef subRef;  // if we recurse downwards into a (subexpression), we'll place the result here

   bool keepGoing = true, isNegated = false;
   LexerToken nextTok;
   while((keepGoing)&&(lexer.GetNextToken(nextTok).IsOK()))
   {
      switch(nextTok.GetToken())
      {
         case LTOKEN_NOT:
            if (localToks.HasItems()) return B_ERROR("'!' must be the first token in a subexpression");
            isNegated = !isNegated;
         break;

         case LTOKEN_LPAREN:          // (
            subRef = CreateQueryFilterFromExpressionAux(lexer, sef);
            MRETURN_ON_ERROR(subRef);
         break;

         case LTOKEN_RPAREN:          // )
            keepGoing = false; // our subexpression ends here
         break;

         case LTOKEN_AND:             // &&
         case LTOKEN_OR:              // ||
         case LTOKEN_XOR:             // ^
         {
            if (subRef() == NULL) return B_ERROR("Conjunction-operator must appear after a subexpression");
            if ((conjunctionRef())&&(conjunctionTok.GetToken() != nextTok.GetToken())) return B_ERROR("Mixed-operator conjunctions aren't supported, use parentheses to disambiguate");
            if (conjunctionRef() == NULL)
            {
               switch(nextTok.GetToken())
               {
                  case LTOKEN_AND: conjunctionRef.SetRef(new AndQueryFilter); break;
                  case LTOKEN_OR:  conjunctionRef.SetRef(new  OrQueryFilter); break;
                  case LTOKEN_XOR: conjunctionRef.SetRef(new XorQueryFilter); break;
                  default:         return B_LOGIC_ERROR;  // unreachable!
               }
               conjunctionTok = nextTok;
            }
            MRETURN_ON_ERROR(conjunctionRef()->GetChildren().AddTail(subRef));
            subRef.Reset();
         }
         break;

         default:
            if (conjunctionRef()) return B_ERROR("Non-subexpression token not permitted within a conjunction");
            MRETURN_ON_ERROR(localToks.AddTail(nextTok));
            if (localToks.GetNumItems() > 4) return B_ERROR("Subexpression cannot contain more than four tokens");
         break;
      }
   }

   if (conjunctionRef())
   {
      if (subRef())
      {
         MRETURN_ON_ERROR(conjunctionRef()->GetChildren().AddTail(subRef));
         return conjunctionRef;
      }
      else return B_ERROR("No subexpression after conjunction-operator");
   }
   else if (subRef()) return MaybeNegate(isNegated, subRef);

   if (localToks.GetNumItems() < 2) return B_ERROR("Subexpression must contain at least two tokens");

   uint32 explicitCastType = B_ANY_TYPE;
   {
      // See if there is an explicit cast; if so, we'll make a note of it, and then remove it,
      // in order to simplify the rest of the parsing
      const uint32 castIdx = (localToks[0].GetToken() == LTOKEN_EXISTS) ? 1 : 2;
      if (castIdx < localToks.GetNumItems())
      {
         explicitCastType = localToks[castIdx].GetExplicitCastTypeCode();
         if (explicitCastType != B_ANY_TYPE) (void) localToks.RemoveItemAt(castIdx);
      }
   }
   if (localToks.GetNumItems() >= 4) return B_ERROR("Subexpression without an explicit cast cannot contain more than three tokens");

   // At this point the only remaining options are 2 or 3
   switch(localToks.GetNumItems())
   {
      case 2:
      {
         const LexerToken & firstTok = localToks[0];
         if (firstTok.GetToken() != LTOKEN_EXISTS) return B_ERROR("Two-token subexpression must start with 'exists'");

         // Second token in a two-token clause should be a Message field-name
         String fieldName;
         uint32 valueIndexInField = 0;
         const LexerToken & fieldNameTok = localToks[1];
         MRETURN_ON_ERROR(fieldNameTok.ParseFieldName(fieldName, valueIndexInField, NULL));

         return MaybeNegate(isNegated, sef.CreateSubexpression(fieldNameTok, valueIndexInField, firstTok, LexerToken(), explicitCastType, LexerToken()));
      }
      break;

      case 3:
      {
         const LexerToken & fieldNameTok = localToks[0];
         const LexerToken & infixOpTok   = localToks[1];
         const LexerToken & valTok       = localToks[2];

         String fieldName;
         uint32 valueIndexInField = 0;
         LexerToken optDefaultValue;
         if (fieldNameTok.GetToken() != LTOKEN_WHAT) MRETURN_ON_ERROR(fieldNameTok.ParseFieldName(fieldName, valueIndexInField, &optDefaultValue));

         const uint32 valueType = valTok.GetValueStringType(explicitCastType);
         if (valueType == B_ANY_TYPE) return B_ERROR("Unable to determine type of value-token at end of subexpression");

         return MaybeNegate(isNegated, sef.CreateSubexpression(fieldNameTok, valueIndexInField, infixOpTok, valTok, valueType, optDefaultValue));
      }
   }

   return B_LOGIC_ERROR;
}

ConstQueryFilterRef CreateQueryFilterFromExpression(const String & expression, const ISubexpressionFactory * optSubexpressionFactory)
{
   DefaultSubexpressionFactory defSef;
   if (optSubexpressionFactory == NULL) optSubexpressionFactory = &defSef;

   Lexer lexer(expression);
   return CreateQueryFilterFromExpressionAux(lexer, *optSubexpressionFactory);
}

template<typename NQFType> QueryFilterRef GetNumericQueryFilter(const LexerToken & infixOpTok, const String & fieldName, uint32 subIdx, const LexerToken & valTok, const LexerToken & optDefaultValue)
{
   const uint8 numOp = infixOpTok.GetNumericQueryFilterOp();
   if (numOp == NQFType::NUM_NUMERIC_OPERATORS) return B_ERROR("Unsupported infix operator for numeric value type");

   typedef typename NQFType::DataType ValType;
   return (optDefaultValue.GetToken() == LTOKEN_VALUESTRING)
        ? QueryFilterRef(new NQFType(fieldName, numOp, GetValueAs<ValType>(valTok.GetValueString()), subIdx, GetValueAs<ValType>(optDefaultValue.GetValueString())))
        : QueryFilterRef(new NQFType(fieldName, numOp, GetValueAs<ValType>(valTok.GetValueString()), subIdx));
}

static QueryFilterRef GetStringQueryFilter(const LexerToken & infixOpTok, const String & fieldName, uint32 subIdx, const LexerToken & valTok, const LexerToken & optDefaultValue)
{
   const uint8 stringOp = infixOpTok.GetStringQueryFilterOp(false);  // TODO:  figure out a reasonable syntax to specify the ignore-case options
   if (stringOp == StringQueryFilter::NUM_STRING_OPERATORS) return B_ERROR("Unsupported infix operator for value type string");

   StringQueryFilterRef sqf(new StringQueryFilter(fieldName, stringOp, valTok.GetValueString(), subIdx));
   if (optDefaultValue.GetToken() == LTOKEN_VALUESTRING) sqf()->SetAssumedDefault(optDefaultValue.GetValueString());
   return sqf;
}

ConstQueryFilterRef DefaultSubexpressionFactory :: CreateSubexpression(const LexerToken & fieldNameTok, uint32 valueIndexInField, const LexerToken & infixOpTok, const LexerToken & valTok, uint32 valueType, const LexerToken & optDefaultValue) const
{
   ConstQueryFilterRef ret;

   switch(fieldNameTok.GetToken())
   {
      case LTOKEN_EXISTS:
         ret.SetRef(new ValueExistsQueryFilter(fieldNameTok.GetValueString(), valueType, valueIndexInField));
      break;

      case LTOKEN_WHAT:
      {
         if (valueType != B_INT32_TYPE) return B_ERROR("'what' keyword requires a value of type int32");

         const uint32 whatVal = (uint32) atol(valTok.GetValueString()());

         bool impossible = false;
         uint32 minWhat = 0, maxWhat = MUSCLE_NO_LIMIT;
         switch(infixOpTok.GetToken())
         {
            case LTOKEN_NEQ: // fall through
            case LTOKEN_EQ:  minWhat = maxWhat = whatVal; break;

            case LTOKEN_LT:
               if (whatVal == 0) impossible = true;   // there are no what-codes less than zero!
                            else    maxWhat = (whatVal-1);
            break;

            case LTOKEN_GT:
               if (whatVal == MUSCLE_NO_LIMIT) impossible = true;   // there are no what-codes greater than MUSCLE_NO_LIMIT!
                                          else    minWhat = (whatVal+1);
            break;

            case LTOKEN_LEQ: maxWhat = whatVal;           break;
            case LTOKEN_GEQ: minWhat = whatVal;           break;
         }

         if (impossible) ret.SetRef(new WhatCodeQueryFilter(1, 0));  // will never return true, because the requested condition is impossible
         else
         {
            ret.SetRef(new WhatCodeQueryFilter(minWhat, maxWhat));
            if (infixOpTok.GetToken() == LTOKEN_NEQ) ret.SetRef(new NorQueryFilter(ret));
         }
      }
      break;

      case LTOKEN_VALUESTRING:
      {
         const String & fieldName = fieldNameTok.GetValueString();
         switch(valueType)
         {
            case B_STRING_TYPE: ret = GetStringQueryFilter                    (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_BOOL_TYPE:   ret = GetNumericQueryFilter<BoolQueryFilter>  (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_DOUBLE_TYPE: ret = GetNumericQueryFilter<DoubleQueryFilter>(infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_FLOAT_TYPE:  ret = GetNumericQueryFilter<FloatQueryFilter> (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_INT64_TYPE:  ret = GetNumericQueryFilter<Int64QueryFilter> (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_INT32_TYPE:  ret = GetNumericQueryFilter<Int32QueryFilter> (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_INT16_TYPE:  ret = GetNumericQueryFilter<Int16QueryFilter> (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_INT8_TYPE:   ret = GetNumericQueryFilter<Int8QueryFilter>  (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_POINT_TYPE:  ret = GetNumericQueryFilter<PointQueryFilter> (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            case B_RECT_TYPE:   ret = GetNumericQueryFilter<RectQueryFilter>  (infixOpTok, fieldName, valueIndexInField, valTok, optDefaultValue); break;
            default:            return B_ERROR("Unsupported value-type");
         }
      }
      break;

      default:
         return B_ERROR("Unspported first token for three-token subexpression");
   }

   return ret;
}

} // end namespace muscle
