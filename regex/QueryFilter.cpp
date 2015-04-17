/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "regex/QueryFilter.h"
#include "regex/StringMatcher.h"
#include "util/MiscUtilityFunctions.h"  // for MemMem()

namespace muscle {

QueryFilter :: ~QueryFilter()
{
   // empty
}

status_t QueryFilter :: SaveToArchive(Message & archive) const
{
   archive.what = TypeCode();
   return B_NO_ERROR;
}

status_t QueryFilter :: SetFromArchive(const Message & archive)
{
   return AcceptsTypeCode(archive.what) ? B_NO_ERROR : B_ERROR;
}

status_t WhatCodeQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((QueryFilter::SaveToArchive(archive)   == B_NO_ERROR) &&
           (archive.AddInt32("min", _minWhatCode) == B_NO_ERROR) &&
           ((_maxWhatCode == _minWhatCode)||(archive.AddInt32("max", _maxWhatCode) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
}

status_t WhatCodeQueryFilter :: SetFromArchive(const Message & archive)
{
   if (QueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("min", _minWhatCode) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("max", _maxWhatCode) != B_NO_ERROR) _maxWhatCode = _minWhatCode;
   return B_NO_ERROR;
}

status_t ValueQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((QueryFilter::SaveToArchive(archive) == B_NO_ERROR) &&
           (archive.AddString("fn", _fieldName) == B_NO_ERROR) &&
           ((_index == 0)||(archive.AddInt32("idx", _index) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
}

status_t ValueQueryFilter :: SetFromArchive(const Message & archive)
{
   if (QueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("idx", _index) != B_NO_ERROR) _index = 0;
   return archive.FindString("fn", _fieldName);
}

status_t ValueExistsQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((ValueQueryFilter::SaveToArchive(archive) == B_NO_ERROR)&&((_typeCode == B_ANY_TYPE)||(archive.AddInt32("type", _typeCode) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
}

status_t ValueExistsQueryFilter :: SetFromArchive(const Message & archive)
{
   if (ValueQueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("type", _typeCode) != B_NO_ERROR) _typeCode = B_ANY_TYPE;
   return B_NO_ERROR;
}

status_t MultiQueryFilter :: SaveToArchive(Message & archive) const
{
   if (QueryFilter::SaveToArchive(archive) != B_NO_ERROR) return B_ERROR;

   uint32 numChildren = _children.GetNumItems();
   for (uint32 i=0; i<numChildren; i++)
   {
      const QueryFilter * nextChild = _children[i]();
      if ((nextChild)&&(archive.AddArchiveMessage("kid", *nextChild) != B_NO_ERROR)) return B_ERROR;
   }
   return B_NO_ERROR;
}

status_t MultiQueryFilter :: SetFromArchive(const Message & archive)
{
   if (QueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;

   _children.Clear();
   MessageRef next;
   for (uint32 i=0; archive.FindMessage("kid", i, next) == B_NO_ERROR; i++)
   {
      ConstQueryFilterRef kid = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*next());
      if ((kid() == NULL)||(_children.AddTail(kid) != B_NO_ERROR)) return B_ERROR;
   }
   return B_NO_ERROR;
}

status_t AndOrQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((MultiQueryFilter::SaveToArchive(archive) == B_NO_ERROR)&&
           ((_minMatches == MUSCLE_NO_LIMIT)||(archive.AddInt32("min", _minMatches) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
}

status_t AndOrQueryFilter :: SetFromArchive(const Message & archive)
{
   if (MultiQueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("min", _minMatches) != B_NO_ERROR) _minMatches = MUSCLE_NO_LIMIT;
   return B_NO_ERROR;
}

bool AndOrQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   const Queue<ConstQueryFilterRef> & kids = GetChildren();
   uint32 numKids = kids.GetNumItems();
   uint32 matchCount = 0;
   uint32 threshold  = muscleMin(_minMatches, numKids);
   for (uint32 i=0; i<numKids; i++)
   {
      if ((threshold-matchCount) > (numKids-i)) return false;  // might as well give up, even all-true wouldn't get us there now

      const QueryFilter * next = kids[i]();
      if ((next)&&(next->Matches(msg, optNode))&&(++matchCount == threshold)) return true;
   }
   return false;
}

status_t NandNotQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((MultiQueryFilter::SaveToArchive(archive) == B_NO_ERROR)&&
           ((_maxMatches == 0)||(archive.AddInt32("max", _maxMatches) == B_NO_ERROR))) ? B_NO_ERROR : B_ERROR;
}

status_t NandNotQueryFilter :: SetFromArchive(const Message & archive)
{
   if (MultiQueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_ERROR;
   if (archive.FindInt32("max", _maxMatches) != B_NO_ERROR) _maxMatches = 0;
   return B_NO_ERROR;
}

bool NandNotQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   const Queue<ConstQueryFilterRef> & kids = GetChildren();
   uint32 numKids = kids.GetNumItems();
   uint32 matchCount = 0;
   uint32 threshold  = muscleMin(_maxMatches, numKids);
   for (uint32 i=0; i<numKids; i++)
   {
      if ((threshold-matchCount) > (numKids-i)) return true;  // might as well give up, even all-true wouldn't get us there now

      const QueryFilter * next = kids[i]();
      if ((next)&&(next->Matches(msg, optNode))&&(++matchCount > threshold)) return false;
   }
   return (matchCount < numKids);
}

bool XorQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   const Queue<ConstQueryFilterRef> & kids = GetChildren();
   uint32 numKids = kids.GetNumItems();
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
   if (ValueQueryFilter::SaveToArchive(archive) != B_NO_ERROR) return B_NO_ERROR;
   if ((_childFilter())&&(archive.AddArchiveMessage("kid", *_childFilter()) != B_NO_ERROR)) return B_ERROR;
   return B_NO_ERROR;
}

status_t MessageQueryFilter :: SetFromArchive(const Message & archive)
{
   if (ValueQueryFilter::SetFromArchive(archive) != B_NO_ERROR) return B_NO_ERROR;

   MessageRef subMsg;
   if (archive.FindMessage("kid", subMsg) == B_NO_ERROR)
   {
      _childFilter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*subMsg());
      if (_childFilter() == NULL) return B_ERROR;
   }
   else _childFilter.Reset();

   return B_NO_ERROR;
}

bool MessageQueryFilter :: Matches(ConstMessageRef & msg, const DataNode * optNode) const
{
   MessageRef subMsg;
   if (msg()->FindMessage(GetFieldName(), GetIndex(), subMsg) != B_NO_ERROR) return false;
   if (_childFilter() == NULL) return true;

   ConstMessageRef constSubMsg = subMsg;
   return _childFilter()->Matches(constSubMsg, optNode);
}

status_t StringQueryFilter :: SaveToArchive(Message & archive) const
{
   return ((ValueQueryFilter::SaveToArchive(archive) == B_NO_ERROR)&&
           (archive.AddString("val", _value)         == B_NO_ERROR)&&
           ((_assumeDefault == false)||(archive.AddString("val", _default) == B_NO_ERROR))) ? archive.AddInt8("op", _op) : B_ERROR;
}

status_t StringQueryFilter :: SetFromArchive(const Message & archive)
{
   FreeMatcher();
   _default.Clear();
   _assumeDefault = (archive.FindString("val", 1, _default) == B_NO_ERROR);
   return ((ValueQueryFilter::SetFromArchive(archive) == B_NO_ERROR)&&(archive.FindString("val", _value) == B_NO_ERROR)) ? archive.FindInt8("op", _op) : B_ERROR;
}

bool StringQueryFilter :: Matches(ConstMessageRef & msg, const DataNode *) const
{
   const String * ps;
   if (msg()->FindString(GetFieldName(), GetIndex(), &ps) != B_NO_ERROR)
   {
      if (_assumeDefault) ps = &_default;
                     else return false;
   }

   const String & s = *ps;
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
            _matcher = newnothrow StringMatcher(_value, true);
            if (_matcher == NULL) WARN_OUT_OF_MEMORY;
         break;

         case OP_REGULAR_EXPRESSION_MATCH:
            _matcher = newnothrow StringMatcher(_value, false);
            if (_matcher == NULL) WARN_OUT_OF_MEMORY;
         break;
      }
   }
   return _matcher ? _matcher->Match(s()) : false;
}

status_t RawDataQueryFilter :: SaveToArchive(Message & archive) const
{
   if ((ValueQueryFilter::SaveToArchive(archive) != B_NO_ERROR)||(archive.AddInt8("op", _op) != B_NO_ERROR)||((_typeCode != B_ANY_TYPE)&&(archive.AddInt32("type", _typeCode) != B_NO_ERROR))) return B_ERROR;

   const ByteBuffer * bb = _value();
   if (bb)
   {
      uint32 numBytes = bb->GetNumBytes();
      const uint8 * bytes = bb->GetBuffer();
      if ((bytes)&&(numBytes > 0)&&(archive.AddData("val", B_RAW_TYPE, bytes, numBytes) != B_NO_ERROR)) return B_ERROR;
   }

   const ByteBuffer * dd = _default();
   if (dd)
   {
      uint32 numBytes = dd->GetNumBytes();
      const uint8 * bytes = dd->GetBuffer();
      if ((bytes)&&(archive.AddData("def", B_RAW_TYPE, bytes, numBytes) != B_NO_ERROR)) return B_ERROR;  // I'm deliberately not checking (numBytes>0) here!
   }

   return B_NO_ERROR;
}

status_t RawDataQueryFilter :: SetFromArchive(const Message & archive)
{
   if ((ValueQueryFilter::SetFromArchive(archive) != B_NO_ERROR)||(archive.FindInt8("op", _op) != B_NO_ERROR)) return B_ERROR;
   if (archive.FindInt32("type", _typeCode) != B_NO_ERROR) _typeCode = B_ANY_TYPE;

   _value.Reset();
   const void * data;
   uint32 numBytes;
   if (archive.FindData("val", B_RAW_TYPE, &data, &numBytes) == B_NO_ERROR)
   {
      _value = GetByteBufferFromPool(numBytes, (const uint8 *) data);
      if (_value() == NULL) return B_ERROR;
   }

   _default.Reset();
   if (archive.FindData("def", B_RAW_TYPE, &data, &numBytes) == B_NO_ERROR)
   {
      _default = GetByteBufferFromPool(numBytes, (const uint8 *) data);
      if (_default() == NULL) return B_ERROR;
   }

   return B_NO_ERROR;
}

bool RawDataQueryFilter :: Matches(ConstMessageRef & msg, const DataNode *) const
{
   const void * hb;
   uint32 hisNumBytes;
   if (msg()->FindData(GetFieldName(), _typeCode, &hb, &hisNumBytes) != B_NO_ERROR)
   {
      if (_default())
      {
         hb = _default()->GetBuffer();
         hisNumBytes = _default()->GetNumBytes();
      }
      else return false;
   }

   const uint8 * hisBytes = (const uint8 *) hb;
   uint32 myNumBytes     = _value() ? _value()->GetNumBytes() : 0;
   const uint8 * myBytes = _value() ? _value()->GetBuffer()   : NULL;
   uint32 clen           = muscleMin(myNumBytes, hisNumBytes);
   if (myBytes == NULL) return false;

   switch(_op)
   {
      case OP_EQUAL_TO:     return ((hisNumBytes == myNumBytes)&&(memcmp(myBytes, hisBytes, clen) == 0));

      case OP_LESS_THAN:
      {
         int mret = memcmp(hisBytes, myBytes, clen);
         if (mret < 0) return true;
         return (mret == 0) ? (hisNumBytes < myNumBytes) : false;
      }

      case OP_GREATER_THAN:
      {
         int mret = memcmp(hisBytes, myBytes, clen);
         if (mret > 0) return true;
         return (mret == 0) ? (hisNumBytes > myNumBytes) : false;
      }

      case OP_LESS_THAN_OR_EQUAL_TO:
      {
         int mret = memcmp(hisBytes, myBytes, clen);
         if (mret <= 0) return true;
         return (mret == 0) ? (hisNumBytes <= myNumBytes) : false;
      }

      case OP_GREATER_THAN_OR_EQUAL_TO:
      {
         int mret = memcmp(hisBytes, myBytes, clen);
         if (mret >= 0) return true;
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

QueryFilterRef QueryFilterFactory :: CreateQueryFilter(const Message & msg) const
{
   QueryFilterRef ret = CreateQueryFilter(msg.what);
   if ((ret())&&(ret()->SetFromArchive(msg) != B_NO_ERROR)) ret.Reset();
   return ret;
}

QueryFilterRef MuscleQueryFilterFactory :: CreateQueryFilter(uint32 typeCode) const
{
   QueryFilter * f = NULL;
   switch(typeCode)
   {
      case QUERY_FILTER_TYPE_WHATCODE:    f = newnothrow WhatCodeQueryFilter;    break;
      case QUERY_FILTER_TYPE_VALUEEXISTS: f = newnothrow ValueExistsQueryFilter; break;
      case QUERY_FILTER_TYPE_BOOL:        f = newnothrow BoolQueryFilter;        break;
      case QUERY_FILTER_TYPE_DOUBLE:      f = newnothrow DoubleQueryFilter;      break;
      case QUERY_FILTER_TYPE_FLOAT:       f = newnothrow FloatQueryFilter;       break;
      case QUERY_FILTER_TYPE_INT64:       f = newnothrow Int64QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT32:       f = newnothrow Int32QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT16:       f = newnothrow Int16QueryFilter;       break;
      case QUERY_FILTER_TYPE_INT8:        f = newnothrow Int8QueryFilter;        break;
      case QUERY_FILTER_TYPE_POINT:       f = newnothrow PointQueryFilter;       break;
      case QUERY_FILTER_TYPE_RECT:        f = newnothrow RectQueryFilter;        break;
      case QUERY_FILTER_TYPE_STRING:      f = newnothrow StringQueryFilter;      break;
      case QUERY_FILTER_TYPE_MESSAGE:     f = newnothrow MessageQueryFilter;     break;
      case QUERY_FILTER_TYPE_RAWDATA:     f = newnothrow RawDataQueryFilter;     break;
      case QUERY_FILTER_TYPE_NANDNOT:     f = newnothrow NandNotQueryFilter;     break;
      case QUERY_FILTER_TYPE_ANDOR:       f = newnothrow AndOrQueryFilter;       break;
      case QUERY_FILTER_TYPE_XOR:         f = newnothrow XorQueryFilter;         break;
      default:                            return QueryFilterRef();  /* unknown type code! */
   }
   if (f == NULL) WARN_OUT_OF_MEMORY;
   return QueryFilterRef(f);
}

static MuscleQueryFilterFactory _defaultQueryFilterFactory;
static QueryFilterFactoryRef _customQueryFilterFactoryRef;

QueryFilterFactoryRef GetGlobalQueryFilterFactory()
{
   if (_customQueryFilterFactoryRef()) return _customQueryFilterFactoryRef;
                                  else return QueryFilterFactoryRef(&_defaultQueryFilterFactory, false);
}

void SetGlobalQueryFilterFactory(const QueryFilterFactoryRef & newFactory)
{
   _customQueryFilterFactoryRef = newFactory;
}

}; // end namespace muscle
