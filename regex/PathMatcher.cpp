/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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
         String temp(optPrepend);  // gcc/BeOS chokes on more compact code than this :^P
         temp += '/';
         temp += path;
         path = temp;
      }
   }
}

status_t PathMatcher :: RemovePathString(const String & wildpath) 
{
   PathMatcherEntry temp;
   if (_entries.Remove(wildpath, temp) == B_NO_ERROR)
   {
      if (temp.GetFilter()()) _numFilters--;
      return B_NO_ERROR;
   }
   return B_ERROR;
}

status_t PathMatcher :: PutPathString(const String & path, const ConstQueryFilterRef & filter)
{
   TCHECKPOINT;

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
            if (strcmp(temp(), "*"))
            {
               smRef.SetRef(smPool->ObtainObject());
               if ((smRef() == NULL)||(smRef()->SetPattern(temp()) != B_NO_ERROR)) return B_ERROR;
            }
            if (newQ->AddTail(smRef) != B_NO_ERROR) return B_ERROR;
            lastSlashPos = slashPos;
         }
         if (_entries.Put(path, PathMatcherEntry(qRef, filter)) == B_NO_ERROR)
         {
            if (filter()) _numFilters++;
            return B_NO_ERROR;
         }
      }
   }
   return B_ERROR;
}

status_t PathMatcher :: PutPathsFromMessage(const char * pathFieldName, const char * optFilterFieldName, const Message & msg, const char * prependIfNoLeadingSlash)
{
   TCHECKPOINT;

   status_t ret = B_NO_ERROR;

   ConstQueryFilterRef filter;  // declared here so that queries can "bleed down" the list without being specified multiple times
   String str;
   for (uint32 i=0; msg.FindString(pathFieldName, i, str) == B_NO_ERROR; i++) 
   {
      if (optFilterFieldName)
      {
         MessageRef filterMsgRef;
         if (msg.FindMessage(optFilterFieldName, i, filterMsgRef) == B_NO_ERROR) filter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*filterMsgRef());
      }
      if (PutPathFromString(str, filter, prependIfNoLeadingSlash) != B_NO_ERROR) ret = B_ERROR;
   }
   return ret;
}

status_t PathMatcher :: SetFilterForEntry(const String & path, const ConstQueryFilterRef & newFilter)
{
   PathMatcherEntry * pme = _entries.Get(path);
   if (pme == NULL) return B_ERROR;

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
   TCHECKPOINT;

   for (HashtableIterator<String, PathMatcherEntry> iter(matcher.GetEntries(), HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      if (_entries.Put(iter.GetKey(), iter.GetValue()) == B_NO_ERROR)
      {
         if (iter.GetValue().GetFilter()()) _numFilters++;
      }
      else return B_ERROR;
   }
   return B_NO_ERROR;
}

bool PathMatcher :: MatchesPath(const char * path, const Message * optMessage, const DataNode * optNode) const
{
   TCHECKPOINT;

   uint32 numClauses = GetPathDepth(path);
   for (HashtableIterator<String, PathMatcherEntry> iter(_entries, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
   {
      const StringMatcherQueue * nextSubscription = iter.GetValue().GetParser()();      
      if ((nextSubscription)&&(nextSubscription->GetNumItems() == numClauses))
      {
         bool matched = true;  // default

         StringTokenizer tok(path+((path[0]=='/')?1:0), "/");
         for (uint32 j=0; j<numClauses; j++)
         {
            const char * nextToken = tok();
            const StringMatcher * nextMatcher = nextSubscription->GetItemAt(j)->GetItemPointer();
            if ((nextToken == NULL)||((nextMatcher)&&(nextMatcher->Match(nextToken) == false))) 
            {
               matched = false;
               break;
            }
         }

         if (matched) 
         {
            ConstMessageRef constMsg(optMessage, false);
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
   for (uint32 i=0; i<GetNumItems(); i++)
   {
      if (ret.HasChars()) ret += ' ';
      const StringMatcherRef & smr = (*this)[i];
      if (smr()) ret += smr()->ToString();
            else ret += "(null)";
   }
   return ret;
}

/** Returns a human-readable string representing this PathMatcherEntry, for easier debugging */
String PathMatcherEntry :: ToString() const
{
   String ret;
   if (_parser()) ret += _parser()->ToString().Prepend("Parser=[").Append("]");
   if (_filter())
   {
      char buf[128]; sprintf(buf, "%sfilter=%p", ret.HasChars()?" ":"", _filter());
      ret += buf;
   }
   return ret;
}

}; // end namespace muscle
