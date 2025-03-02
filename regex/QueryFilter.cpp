/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/DataNode.h"
#include "regex/QueryFilter.h"
#include "regex/StringMatcher.h"
#include "util/MiscUtilityFunctions.h"  // for MemMem()
#include "util/OutputPrinter.h"

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
   p.printf("%s: ", GetUnmangledSymbolName(typeid(*this).name())());
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
   p.printf(" _minWhatCode= " UINT32_FORMAT_SPEC " _maxWhatCode=" UINT32_FORMAT_SPEC "\n", _minWhatCode, _maxWhatCode);
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
   p.printf(" _fieldName=[%s] _index=" UINT32_FORMAT_SPEC "\n", _fieldName(), _index);
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
   p.printf(" _typeCode=" UINT32_FORMAT_SPEC "\n", _typeCode);
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

void MessageQueryFilter :: Print(const OutputPrinter & p) const
{
   ValueQueryFilter::Print(p);
   if (_childFilter())
   {
      p.printf(" _childFilter=");
      _childFilter()->Print(p.WithIndent(3));
   }
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

enum {
   LTOKEN_LPAREN = 0,      // (
   LTOKEN_RPAREN,          // )
   LTOKEN_LT,              // <
   LTOKEN_GT,              // >
   LTOKEN_EQ,              // ==
   LTOKEN_LEQ,             // <=
   LTOKEN_GEQ,             // >=
   LTOKEN_NEQ,             // !=
   LTOKEN_AND,             // &&
   LTOKEN_OR ,             // ||
   LTOKEN_XOR,             // ^
   LTOKEN_STARTSWITH,      // startswith
   LTOKEN_ENDSWITH,        // endswith
   LTOKEN_CONTAINS,        // contains
   LTOKEN_ISSTARTOF,       // isstartof
   LTOKEN_ISENDOF,         // isendof
   LTOKEN_ISSUBSTRINGOF,   // issubstringof
   LTOKEN_MATCHES,         // matches
   LTOKEN_MATCHESREGEX,    // matchesregex
   LTOKEN_INT64,           // (int64)
   LTOKEN_INT32,           // (int32)
   LTOKEN_INT16,           // (int16)
   LTOKEN_INT8,            // (int8)
   LTOKEN_BOOL,            // (bool)
   LTOKEN_FLOAT,           // (float)
   LTOKEN_DOUBLE,          // (double)
   LTOKEN_NOT,             // not
   LTOKEN_WHAT,            // what
   LTOKEN_EXISTS,          // exists
   LTOKEN_VALUESTRING,     // some other token supplied by the user
   NUM_LTOKENS
};

static const char * _tokStrs[] =
{
   "(",              // LTOKEN_LPAREN
   ")",              // LTOKEN_RPAREN
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
   "not",            // LTOKEN_NOT
   "what",           // LTOKEN_WHAT    (lack of space is intentional)
   "exists ",        // LTOKEN_EXISTS  (space is intentional)
   NULL,             // LTOKEN_VALUESTRING
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_tokStrs, NUM_LTOKENS);

static bool _tokStrlensNeedsInit = true;
static size_t _tokStrlens[NUM_LTOKENS] ;

class LexerToken
{
public:
   LexerToken() : _tok(NUM_LTOKENS), _wasQuoted(false) {/* empty */}
   LexerToken(uint32 tok) : _tok(tok), _wasQuoted(false) {/* empty */}
   LexerToken(uint32 tok, const String & valStr, bool wasQuoted) : _tok(tok), _valStr(valStr), _wasQuoted(wasQuoted) {/* empty */}

   uint32 GetToken() const {return _tok;}
   const String & GetValueString() const {return _valStr;}

   String ToString() const
   {
      const String tokStr = (_tok < NUM_LTOKENS) ? _tokStrs[_tok] : "???";
      return _valStr.HasChars() ? (tokStr+String(" %1").Arg(_valStr)) : tokStr;
   }

   // Convenience method:  Given a token like "myfield:4", returns the field name "myfield" and the retIdx=4
   status_t ParseFieldName(String & retFieldName, uint32 & retIdx, LexerToken * optRetDefaultValue) const {return ParseFieldNameAux(_valStr, retFieldName, retIdx, optRetDefaultValue);}

   // returns the B_*_TYPE code representing our value's type, or B_ANY_TYPE on failure/unknown
   uint32 GetValueStringType(uint32 explicitCastType) const
   {
      if (_tok != LTOKEN_VALUESTRING) return B_ANY_TYPE;
      if (_wasQuoted) return (explicitCastType == B_ANY_TYPE) ? B_STRING_TYPE : B_ANY_TYPE;  // (int16)"hi" is too weird to let slide
      if (explicitCastType != B_ANY_TYPE) return explicitCastType;
      if ((_valStr.EqualsIgnoreCase("true"))||(_valStr.EqualsIgnoreCase("false"))) return B_BOOL_TYPE;

      switch(_valStr.GetNumInstancesOf(','))
      {
         case 0:
         {
            if (_valStr.Contains('.'))
            {
               return _valStr.EndsWith('f') ? B_FLOAT_TYPE : B_DOUBLE_TYPE;
            }
            else return B_INT32_TYPE;  // I guess this is a good default?
         }
         break;

         case 1:  return B_POINT_TYPE;
         case 3:  return B_RECT_TYPE;
         default: return B_ANY_TYPE;
      }
   }

   // Returns the StringQueryFilter::OP_* value associated with this infix operator, or StringQueryFilter::NUM_STRING_OPERATORS on failure
   uint8 GetStringQueryFilterOp(bool isIgnoreCase) const
   {
      switch(GetToken())
      {
         case LTOKEN_EQ:            return isIgnoreCase ? StringQueryFilter::OP_EQUAL_TO_IGNORECASE                 : StringQueryFilter::OP_EQUAL_TO;                 // ==
         case LTOKEN_LT:            return isIgnoreCase ? StringQueryFilter::OP_LESS_THAN_IGNORECASE                : StringQueryFilter::OP_LESS_THAN;                // ==
         case LTOKEN_GT:            return isIgnoreCase ? StringQueryFilter::OP_GREATER_THAN_IGNORECASE             : StringQueryFilter::OP_GREATER_THAN;             // ==
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

   // Returns the B_*_TYPE associated with this token if this token is an explicit-cast, or B_ANY_TYPE otherwise
   uint32 GetExplicitCastTypeCode() const
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
         default:            return B_ANY_TYPE;
      }
   }

private:
   uint32 _tok;
   String _valStr;
   bool _wasQuoted;

   status_t ParseFieldNameAux(const String & valStr, String & retFieldName, uint32 & retIdx, LexerToken * optRetDefaultValue) const
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
};

static int32 GetMatchingToken(const char * s)
{
   for (int32 i=ARRAYITEMS(_tokStrs)-1; i>=0; i--)
   {
      const char * ts    = _tokStrs[i];
      const size_t tsLen = _tokStrlens[i];
      if ((tsLen > 0)&&(Strncasecmp(s, ts, tsLen) == 0)) return i;
   }
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
         const int32 matchedTok = GetMatchingToken(s);
         if (matchedTok >= 0)
         {
            _curPos += _tokStrlens[matchedTok];
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

               retTok   = LexerToken(LTOKEN_VALUESTRING, String(s, endQuote-s), true);
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
                  if (((*t == '\0')||(isspace(*t)))||(GetMatchingToken(t) >= 0))
                  {
                     retTok   = LexerToken(LTOKEN_VALUESTRING, String(s, t-s), false);
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

static QueryFilterRef MaybeNegate(bool doNegate, const QueryFilterRef & qf) {return doNegate ? QueryFilterRef(new NorQueryFilter(qf)) : qf;}

static QueryFilterRef CreateQueryFilterFromExpressionAux(Lexer & lexer)
{
   Queue<LexerToken> localToks;

   bool keepGoing = true, isNegated = false;
   LexerToken nextTok;
   while((keepGoing)&&(lexer.GetNextToken(nextTok).IsOK()))
   {
      switch(nextTok.GetToken())
      {
         case LTOKEN_NOT:
            if (localToks.HasItems()) return B_ERROR("'not' must be the first token in a subexpression");
            isNegated = !isNegated;
         break;

         case LTOKEN_LPAREN:          // (
         {
            QueryFilterRef subRet = CreateQueryFilterFromExpressionAux(lexer);
         }
         break;

         case LTOKEN_RPAREN:          // )
            keepGoing = false; // our subexpression ends here
         break;

         default:
            MRETURN_ON_ERROR(localToks.AddTail(nextTok));
            if (localToks.GetNumItems() > 4) return B_ERROR("Subexpression cannot contain more than four tokens");
         break;
      }
   }

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
         if (localToks[0].GetToken() != LTOKEN_EXISTS) return B_ERROR("Two-token subexpression must start with 'exists'");

         String fieldName;
         uint32 subIdx = 0;
         MRETURN_ON_ERROR(localToks[1].ParseFieldName(fieldName, subIdx, NULL));
         return MaybeNegate(isNegated, QueryFilterRef(new ValueExistsQueryFilter(fieldName, explicitCastType, subIdx)));
      }
      break;

      case 3:
      {
         const LexerToken & fieldNameTok = localToks[0];
         const LexerToken & infixOpTok   = localToks[1];
         const LexerToken & valTok       = localToks[2];

printf("fieldNameTok=[%s] infixOpTok=[%s] valTok=[%s] isNegated=%i\n", fieldNameTok.ToString()(), infixOpTok.ToString()(), valTok.ToString()(), isNegated);
         String fieldName;
         uint32 subIdx = 0;
         LexerToken optDefaultValue;
         if (fieldNameTok.GetToken() != LTOKEN_WHAT) MRETURN_ON_ERROR(fieldNameTok.ParseFieldName(fieldName, subIdx, &optDefaultValue));

         const uint32 valueType = valTok.GetValueStringType(explicitCastType);
         if (valueType == B_ANY_TYPE) return B_ERROR("Unable to determine type of value-token at end of subexpression");

         switch(fieldNameTok.GetToken())
         {
            case LTOKEN_WHAT:
            {
               if (valueType != B_INT32_TYPE) return B_ERROR("'what' keyword requires a value of type int32");

               const uint32 whatVal = (uint32) atol(valTok.GetValueString()());

               uint32 minWhat = 0, maxWhat = MUSCLE_NO_LIMIT;
               switch(infixOpTok.GetToken())
               {
                  case LTOKEN_NEQ: // fall through
                  case LTOKEN_EQ:  minWhat = maxWhat = whatVal; break;
                  case LTOKEN_LT:  maxWhat = (whatVal-1);       break;
                  case LTOKEN_GT:  minWhat = (whatVal+1);       break;
                  case LTOKEN_LEQ: maxWhat = whatVal;           break;
                  case LTOKEN_GEQ: minWhat = whatVal;           break;
               }

               QueryFilterRef ret(new WhatCodeQueryFilter(minWhat, maxWhat));
               if (infixOpTok.GetToken() == LTOKEN_NEQ) ret.SetRef(new NorQueryFilter(ret));
               return MaybeNegate(isNegated, ret);
            }
            break;

            case LTOKEN_VALUESTRING:
            {
               if (valueType == B_STRING_TYPE)
               {
                  const uint8 stringOp = infixOpTok.GetStringQueryFilterOp(false);  // TODO:  figure out a syntax for ignore-case option
                  if (stringOp == StringQueryFilter::NUM_STRING_OPERATORS) return B_ERROR("Unsupported infix operator for value type string");

                  StringQueryFilterRef ret(new StringQueryFilter(fieldName, stringOp, valTok.GetValueString(), subIdx));
                  if (optDefaultValue.GetToken() == LTOKEN_VALUESTRING) ret()->SetAssumedDefault(optDefaultValue.GetValueString());
                  return MaybeNegate(isNegated, ret);
               }
               else
               {
                  return B_ERROR("TODO:  implement me");
               }
            }
            break;

            default:
               return B_ERROR("Unspported first token for three-token subexpression");
         }
      }
   }

   return B_LOGIC_ERROR;
}

QueryFilterRef CreateQueryFilterFromExpression(const String & expression)
{
   Lexer lexer(expression);
   //while(lexer.GetNextToken(nextTok).IsOK()) printf(" -> %s\n", nextTok.ToString()());
   return CreateQueryFilterFromExpressionAux(lexer);
}


} // end namespace muscle
