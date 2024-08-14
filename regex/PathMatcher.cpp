/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "regex/PathMatcher.h"
#include "util/StringTokenizer.h"

namespace muscle {

StringMatcherQueueRef::ItemPool _stringMatcherQueuePool;
StringMatcherQueueRef::ItemPool * GetStringMatcherQueuePool() {return &_stringMatcherQueuePool;}

void PathMatcher :: AdjustStringPrefix(String & path, const char * optPrepend) const
{
   if (path.HasChars())
   {
           if (path[0] == '/') path = path.Substring(1);
      else if (optPrepend)
      {
         String temp(optPrepend);  // gcc chokes on more compact code than this :^P
         temp += '/';
         temp += path;
         path = temp;
      }
   }
}

status_t PathMatcher :: RemovePathString(const String & wildpath)
{
   PathMatcherEntry temp;
   const uint32 depth = GetPathDepth(wildpath());
   Hashtable<String, PathMatcherEntry> * subTable = _entries.Get(depth);
   if ((subTable)&&(subTable->Remove(wildpath, temp).IsOK()))
   {
      if (temp.GetFilter()()) _numFilters--;
      if (subTable->IsEmpty()) (void) _entries.Remove(depth);
      return B_NO_ERROR;
   }
   return B_DATA_NOT_FOUND;
}

status_t PathMatcher :: PutPathString(const String & path, const ConstQueryFilterRef & filter)
{
   TCHECKPOINT;

   status_t ret;
   if (path.HasChars())
   {
      StringMatcherQueue * newQ = GetStringMatcherQueuePool()->ObtainObject();
      if (newQ)
      {
         StringMatcherQueueRef qRef(newQ);

         StringMatcherRef::ItemPool * smPool = GetStringMatcherPool();
         String temp;
         int32 lastSlashPos = -1;
         int32 slashPos = 0;
         while(slashPos >= 0)
         {
            slashPos = path.IndexOf('/', lastSlashPos+1);
            temp = path.Substring(lastSlashPos+1, (slashPos >= 0) ? slashPos : (int32)path.Length());
            StringMatcherRef smRef;
            if (strcmp(temp(), "*") != 0)
            {
               smRef.SetRef(smPool->ObtainObject());
               MRETURN_OOM_ON_NULL(smRef());
               MRETURN_ON_ERROR(smRef()->SetPattern(temp()));
            }
            MRETURN_ON_ERROR(newQ->GetStringMatchers().AddTail(smRef));
            lastSlashPos = slashPos;
         }

         const uint32 depth = newQ->GetStringMatchers().GetNumItems();
         Hashtable<String, PathMatcherEntry> * subTable = _entries.GetOrPut(depth);
         if (subTable == NULL) MRETURN_OUT_OF_MEMORY;

         const PathMatcherEntry * oldPME = subTable->Get(path);
         const bool alreadyHadFilter = ((oldPME)&&(oldPME->GetFilter()() != NULL));
         if (subTable->Put(path, PathMatcherEntry(qRef, filter)).IsOK(ret))
         {
            if ((alreadyHadFilter == false)&&(filter())) _numFilters++;
            return B_NO_ERROR;
         }
         else if (subTable->IsEmpty()) (void) _entries.Remove(depth);
      }
   }
   return ret;
}

status_t PathMatcher :: PutPathsFromMessage(const char * pathFieldName, const char * optFilterFieldName, const Message & msg, const char * prependIfNoLeadingSlash)
{
   TCHECKPOINT;

   status_t ret;

   ConstQueryFilterRef filter;  // declared here so that queries can "bleed down" the list without being specified multiple times
   const String * str;
   for (uint32 i=0; msg.FindString(pathFieldName, i, &str).IsOK(); i++)
   {
      if (optFilterFieldName)
      {
         ConstMessageRef filterMsgRef;
         if (msg.FindMessage(optFilterFieldName, i, filterMsgRef).IsOK()) filter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*filterMsgRef());
      }
      (void) PutPathFromString(*str, filter, prependIfNoLeadingSlash).IsError(ret);
   }
   return ret;
}

status_t PathMatcher :: SetFilterForEntry(const String & path, const ConstQueryFilterRef & newFilter)
{
   Hashtable<String, PathMatcherEntry> * subTable = _entries.Get(GetPathDepth(path()));
   PathMatcherEntry * pme = subTable ? subTable->Get(path) : NULL;
   if (pme == NULL) return B_DATA_NOT_FOUND;

   if ((newFilter() != NULL) != (pme->GetFilter()() != NULL)) _numFilters += ((newFilter() != NULL) ? 1 : -1);  // FogBugz #5803
   pme->SetFilter(newFilter);
   return B_NO_ERROR;
}

status_t PathMatcher :: PutPathFromString(const String & str, const ConstQueryFilterRef & filter, const char * prependIfNoLeadingSlash)
{
   String s = str;
   AdjustStringPrefix(s, prependIfNoLeadingSlash);
   return PutPathString(s, filter);
}

status_t PathMatcher :: PutPathsFromMatcher(const PathMatcher & matcher)
{
   status_t ret;
   for (HashtableIterator<uint32, Hashtable<String, PathMatcherEntry> > iter(matcher.GetEntries(), HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      for (HashtableIterator<String, PathMatcherEntry> subIter(iter.GetValue(), HTIT_FLAG_NOREGISTER); subIter.HasData(); subIter++)
      {
         const String & pathStr = subIter.GetKey();
         Hashtable<String, PathMatcherEntry> * subTable = _entries.GetOrPut(iter.GetKey());
         const PathMatcherEntry * prevVal = subTable ? subTable->Get(pathStr) : NULL;
         const bool alreadyHadFilter = ((prevVal)&&(prevVal->GetFilter()() != NULL));
         if ((subTable)&&(subTable->Put(pathStr, subIter.GetValue()).IsOK(ret)))
         {
            if ((alreadyHadFilter == false)&&(subIter.GetValue().GetFilter()())) _numFilters++;
         }
         else
         {
            if ((subTable)&&(subTable->IsEmpty())) (void) _entries.Remove(iter.GetKey()); // roll back!
            break;
         }
      }
   }
   return ret;
}

bool PathMatcher :: MatchesPath(const char * path, const Message * optMessage, const DataNode * optNode) const
{
   TCHECKPOINT;

   const uint32 numClauses = GetPathDepth(path);
   for (HashtableIterator<String, PathMatcherEntry> iter(_entries[numClauses], HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      const StringMatcherQueue * nextSubscription = iter.GetValue().GetParser()();
      if (nextSubscription)
      {
         bool matched = true;  // default

         StringTokenizer tok(path+((path[0]=='/')?1:0), "//");
         for (uint32 j=0; j<numClauses; j++)
         {
            const char * nextToken = tok();
            const StringMatcher * nextMatcher = nextSubscription->GetStringMatchers().GetItemAt(j)->GetItemPointer();
            if ((nextToken == NULL)||((nextMatcher)&&(nextMatcher->Match(nextToken) == false)))
            {
               matched = false;
               break;
            }
         }

         if (matched)
         {
            DummyConstMessageRef constMsg(optMessage);
            const QueryFilter * filter = iter.GetValue().GetFilter()();
            if ((filter == NULL)||(optMessage == NULL)||(filter->Matches(constMsg, optNode))) return true;
         }
      }
   }
   return false;
}

// Returns a pointer into (path) after the (depth)'th '/' char
const char * GetPathClause(int depth, const char * path)
{
   for (int i=0; i<depth; i++)
   {
      const char * nextSlash = strchr(path, '/');
      if (nextSlash == NULL)
      {
         path = NULL;
         break;
      }
      path = nextSlash + 1;
   }
   return path;
}

String GetPathClauseString(int depth, const char * path)
{
   String ret;
   const char * str = GetPathClause(depth, path);
   if (str)
   {
      ret = str;
      ret = ret.Substring(0, "/");
   }
   return ret;
}

int GetPathDepth(const char * path)
{
   if (path[0] == '/') path++;  // ignore any leading slash

   int depth = 0;
   while(true)
   {
      if (path[0]) depth++;

      path = strchr(path, '/');
      if (path) path++;
           else break;
   }
   return depth;
}

/** Returns a human-readable string representing this StringMatcherQueue, for easier debugging */
String StringMatcherQueue :: ToString() const
{
   String ret;
   for (uint32 i=0; i<_queue.GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += ' ';
      const StringMatcherRef & smr = _queue[i];
      if (smr()) ret += smr()->ToString();
            else ret += "(null)";
   }
   return ret;
}

/** Returns a human-readable string representing this PathMatcherEntry, for easier debugging */
String PathMatcherEntry :: ToString() const
{
   String ret;
   if (_parser()) ret += _parser()->ToString().WithPrepend("Parser=[").WithAppend("]");
   if (_filter())
   {
      char buf[128]; muscleSprintf(buf, "%sfilter=%p", ret.HasChars()?" ":"", _filter());
      ret += buf;
   }
   return ret;
}

} // end namespace muscle
