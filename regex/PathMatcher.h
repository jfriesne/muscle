/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MusclePathMatcher_h
#define MusclePathMatcher_h

#include "util/Queue.h"
#include "regex/QueryFilter.h"
#include "regex/StringMatcher.h"
#include "message/Message.h"

namespace muscle {

DECLARE_REFTYPES(StringMatcher);
 
/** A reference-countable list of references to StringMatcher objects */
class StringMatcherQueue : public Queue<StringMatcherRef>, public RefCountable
{
public:
   /** Constructor. */
   StringMatcherQueue() {/* empty */}

   /** Returns a human-readable string representing this StringMatcherQueue, for easier debugging */
   String ToString() const;
};

/** Type for a reference to a queue of StringMatcher objects. */
DECLARE_REFTYPES(StringMatcherQueue);

/** Returns a point to a singleton ObjectPool that can be used
 *  to minimize the number of StringMatcherQueue allocations and deletions
 *  by recycling the StringMatcherQueue objects 
 */
StringMatcherQueueRef::ItemPool * GetStringMatcherQueuePool();

/** This class represents one entry in a PathMatcher object.  It contains the StringMatcher objects
 *  that test a wildcarded path, and optionally, the QueryFilter object that tests the content
 *  of matching Messages.
 */
class PathMatcherEntry
{
public:
   /** Default constructor */
   PathMatcherEntry() {/* empty */}

   /** Constructor 
     * @param parser Reference to the list of StringMatcher objects that represent our wildcarded path.
     * @param filter Optional reference to the QueryFilter object that filters matching nodes by content.
     */
   PathMatcherEntry(const StringMatcherQueueRef & parser, const ConstQueryFilterRef & filter) : _parser(parser), _filter(filter) {/* empty */}

   /** Returns a reference to our list of StringMatchers. */
   StringMatcherQueueRef GetParser() const {return _parser;}
 
   /** Returns a reference to our QueryFilter object.  May be a NULL reference. */
   ConstQueryFilterRef GetFilter() const {return _filter;}

   /** Sets our QueryFilter object to the specified reference.  Pass in a NULL reference to remove any existing QueryFilter. */
   void SetFilter(const ConstQueryFilterRef & filter) {_filter = filter;}

   /** Returns true iff we our filter matches the given Message, or if either (optMsg) or our filter is NULL. */
   bool FilterMatches(ConstMessageRef & optMsg, const DataNode * optNode) const
   {
      const QueryFilter * filter = GetFilter()();
      return ((filter == NULL)||(optMsg() == NULL)||(filter->Matches(optMsg, optNode)));
   }

   /** Returns a human-readable string representing this PathMatcherEntry, for easier debugging */
   String ToString() const;

private:
   StringMatcherQueueRef _parser;
   ConstQueryFilterRef _filter;
};

/** This class is used to do efficient regex-pattern-matching of one or more query strings (e.g. ".*./.*./j*remy/fries*") 
  * against various path strings (e.g. "12.18.240.15/123/jeremy/friesner").  A given path string is said to 'match'
  * if it matches at least one of the query strings added to this object.
  * Note that the search strings are always treated as relative paths -- if you pass in a search string with
  * a leading slash, then it will be interpeted as a relative query with the first level of the query looking
  * for nodes with name "".
  * As of MUSCLE 2.40, this class also supports QueryFilter objects, so that only nodes whose Messages match the
  * criteria of the QueryFilter are considered to match the query.  This filtering is optional -- specify a null
  * ConstQueryFilterRef to disable it.
  */
class PathMatcher : public RefCountable
{
public:
   /** Default Constructor.  Creates a PathMatcher with no query strings in it */
   PathMatcher() : _numFilters(0) {/* empty */}

   /** Destructor */
   ~PathMatcher() {/* empty */}

   /** Removes all path nodes from this object */
   void Clear() {_entries.Clear(); _numFilters = 0;}

   /** Parses the given query string (e.g. "12.18.240.15/1234/beshare/j*") to this PathMatcher's set of query strings.
    *  Note that the search strings are always treated as relative paths -- if you pass in a search string with
    *  a leading slash, then it will be interpeted as a relative query with the first level of the query looking
    *  for nodes with name "".
    *  @param path a string of form "x/y/z/...", representing a pattern-matching function.
    *  @param filter Reference to a QueryFilter object to use to filter Messages that match our path.  If the 
    *                reference is a NULL reference, then no filtering will be done.
    *  @return B_NO_ERROR on success, B_ERROR if out of memory.
    */
   status_t PutPathString(const String & path, const ConstQueryFilterRef & filter);

   /** Adds all of (matcher)'s StringMatchers to this matcher */
   status_t PutPathsFromMatcher(const PathMatcher & matcher);

   /** Adds zero or more wild paths to this matcher based on the contents of a string field in a Message.
    *  @param pathFieldName the name of a string field to look for node path expressions in.
    *  @param optFilterFieldName If non-NULL, the name of a Message field to look for archived QueryFilter objects (one for each node path expression) in.
    *  @param msg the Message to look for node path expressions in
    *  @param optPrependIfNoLeadingSlash If non-NULL, a '/' and this string will be prepended to any found path string that doesn't start with a '/' character.
    *  @return B_NO_ERROR on success, or B_ERROR if out of memory.
    */
   status_t PutPathsFromMessage(const char * pathFieldName, const char * optFilterFieldName, const Message & msg, const char * optPrependIfNoLeadingSlash);

   /** Convenience method:  Essentially the same as PutPathString(), except that (path) is first run through
    *  AdjustPathString() before being added to the path matcher.  So this method has the same semantics as
    *  PutPathsFromMessage(), except that it adds a single string instead of a list of Strings.
    *  @param path Matching-string to add to this matcher.
    *  @param optPrependIfNoLeadingSlash If non-NULL, a '/' and this string will be prepended to any found path string that doesn't start with a '/' character.
    *  @param filter Reference to a QueryFilter object to use for content-based Message filter, or a NULL reference if
    *                no additional filtering is necessary.
    *  @return B_NO_ERROR on success, or B_ERROR if out of memory.
    */
   status_t PutPathFromString(const String & path, const ConstQueryFilterRef & filter, const char * optPrependIfNoLeadingSlash);

   /** Removes the given path string and its associated StringMatchers from this matcher.
    *  @param wildpath the path string to remove
    *  @return B_NO_ERROR if the given path string was found and removed, or B_ERROR if it couldn't be found.
    */
   status_t RemovePathString(const String & wildpath);

   /**
    * Returns true iff the given fully qualified path string matches our query.
    * @param path the path string to check to see if it matches
    * @param optMessage if non-NULL, this Message will be tested by the QueryFilter objects.
    * @param optNode this DataNode pointer will be passed to the QueryFilter objects.
    */
   bool MatchesPath(const char * path, const Message * optMessage, const DataNode * optNode) const;
    
   /**
    * Utility method.
    * If (w) starts with '/', this method will remove the slash.
    * If it doesn't start with a '/', and (optPrepend) is non-NULL,
    * this method will prepend (optPrepend+"/") to (w).
    * @param w A string to inspect, and potentially modify.
    * @param optPrepend If non-NULL, a string to potentially prepend to (w)
    */
   void AdjustStringPrefix(String & w, const char * optPrepend) const;

   /** Sets the new QueryFilter to be associated with the PathMatcherEntry that has the specified name.
     * @param path Name of the PathMatcherEntry to update the filter of
     * @param newFilter The new filter to give to PathMatcherEntry.  May be a NULL ref if no filter is desired.
     * @note You should always set an entries filter via this method rather than calling SetEntry() on
     *       the PathMatcherEntry object directly; that way, the PathMatcher class can keep its filters-count 
     *       up to date.
     * @returns B_NO_ERROR on success, or B_ERROR if an entry with the given path could not be found. 
     */
   status_t SetFilterForEntry(const String & path, const ConstQueryFilterRef & newFilter);

   /** Returns a read-only reference to our table of PathMatcherEntries. */
   const Hashtable<String, PathMatcherEntry> & GetEntries() const {return _entries;}

   /** Returns the number of QueryFilters we are currently using. */
   uint32 GetNumFilters() const {return _numFilters;}

private:
   Hashtable<String, PathMatcherEntry> _entries;
   uint32 _numFilters;  // count how many filters are installed; so we can optimize when there are none
};
DECLARE_REFTYPES(PathMatcher);

/** Returns a pointer into (path) after the (depth)'th '/' char
 *  @param depth the depth in the path to search for (0 == root, 1 == first level, 2 == second, etc.)
 *  @param path the path string (e.g. "/x/y/z/...") to search through
 *  @returns a pointer into (path), or NULL on failure.
 */
const char * GetPathClause(int depth, const char * path);

/** As above, but returns a String object for just the given clause,
 *  instead of the entire remainder of the string.  This version is 
 *  somewhat less efficient, but easier to user.
 *  @param depth the depth in the path to search for (0 == root, 1 == first level, 2 == second, etc.)
 *  @param path the path string (e.g. "/x/y/z/...") to search through
 *  @returns The string that is the (nth) item in the path, or "" if the depth is invalid.
 */
String GetPathClauseString(int depth, const char * path);

/** Returns the number of clauses in the given path string.
 *  @param path A path string.  Any leading slash will be ignored.
 *  @returns The number of clauses in (path).  (a.k.a the 'depth' of (path))
 *           For example, "" and "/" return 0, "/test" returns 1, "test/me"
 *           returns 2, "/test/me/thoroughly" returns 3.
 *          
 */
int GetPathDepth(const char * path);

}; // end namespace muscle

#endif
