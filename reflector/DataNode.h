/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDataNode_h
#define MuscleDataNode_h

#include "reflector/DumbReflectSession.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"

namespace muscle {

class StorageReflectSession;
class DataNode;

DECLARE_REFTYPES(DataNode);

/** Iterator type for our child objects */
typedef HashtableIterator<const String *, DataNodeRef> DataNodeRefIterator;

/** Each object of this class represents one node in the server-side data-storage tree.  */
class DataNode : public RefCountable, private CountedObject<DataNode>
{
public:
   /** Default Constructor.  Don't create DataNode objects yourself though, call StorageReflectSession::GetNewDataNode() instead!  */
   DataNode();

   /** Destructor.   Don't delete DataNode objects yourself though, let the DataNodeRef objects do it for you */
   ~DataNode();

   /**
    * Put a child without changing the ordering index
    * @param child Reference to the child to accept into our list of children
    * @param optNotifyWithOnSetParent If non-NULL, a StorageReflectSession to use to notify subscribers that the new node has been added
    * @param optNotifyWithOnChangedData If non-NULL, a StorageReflectSession to use to notify subscribers when the node's data has been alterered
    * @return B_NO_ERROR on success, B_ERROR if out of memory
    */
   status_t PutChild(const DataNodeRef & child, StorageReflectSession * optNotifyWithOnSetParent, StorageReflectSession * optNotifyWithOnChangedData);

   /**
    * Create and add a new child node for (data), and put it into the ordering index
    * @param data Reference to a message to create a new child node for.
    * @param optInsertBefore if non-NULL, the name of the child to put the new child before in our index.  If NULL, (or the specified child cannot be found) the new child will be appended to the end of the index.
    * @param optNodeName If non-NULL, the inserted node will have the specified name.  Otherwise, a name will be generated for the node.
    * @param optNotifyWithOnSetParent If non-NULL, a StorageReflectSession to use to notify subscribers that the new node has been added
    * @param optNotifyWithOnChangedData If non-NULL, a StorageReflectSession to use to notify subscribers when the node's data has been alterered
    * @param optAddNewChildren If non-NULL, any newly formed nodes will be added to this hashtable, keyed on their absolute node path.
    * @return B_NO_ERROR on success, B_ERROR if out of memory
    */
   status_t InsertOrderedChild(const MessageRef & data, const String * optInsertBefore, const String * optNodeName, StorageReflectSession * optNotifyWithOnSetParent, StorageReflectSession * optNotifyWithOnChangedData, Hashtable<String, DataNodeRef> * optAddNewChildren);
 
   /** 
    * Moves the given node (which must be a child of ours) to be just before the node named
    * (moveToBeforeThis) in our index.  If (moveToBeforeThis) is not a node in our index,
    * then (child) will be moved back to the end of the index. 
    * @param child Reference to a child node of ours, to be moved in the node ordering index.
    * @param moveToBeforeThis name of another child node of ours.  If this name is NULL or
    *                         not found in our index, we'll move (child) to the end of the index.
    * @param optNotifyWith If non-NULL, this will be used to sent INDEXUPDATE message to the
    *                      interested clients, notifying them of the change.
    * @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t ReorderChild(const DataNodeRef & child, const String * moveToBeforeThis, StorageReflectSession * optNotifyWith);

   /** Returns true iff we have a child with the given name */
   bool HasChild(const String & key) const {return ((_children)&&(_children->ContainsKey(&key)));}

   /** Retrieves the child with the given name.
    *  @param key The name of the child we wish to retrieve
    *  @param returnChild On success, a reference to the retrieved child is written into this object.
    *  @return B_NO_ERROR if a child node was successfully retrieved, or B_ERROR if it was not found.
    */
   status_t GetChild(const String & key, DataNodeRef & returnChild) const {return ((_children)&&(_children->Get(&key, returnChild) == B_NO_ERROR)) ? B_NO_ERROR : B_ERROR;}

   /** As above, except the reference to the child is returned as the return value rather than in a parameter.
    *  @param key The name of the child we wish to retrieve
    *  @return On success, a reference to the specified child node is returned.  On failure, a NULL DataNodeRef is returned.
    */
   DataNodeRef GetChild(const String & key) const {return _children ? _children->GetWithDefault(&key) : DataNodeRef();}

   /** Finds and returns a descendant node (i.e child, grandchild, etc) by following the provided
    *  slash-separated relative node path.
    *  @param subPath A list of child-names to descend down, with the child node-name separated from each other via slashes.
    *  @returns A valid DataNodeRef on success, or a NULL DataNodeRef if no child could be found at that sub-path.
    */
   DataNodeRef GetDescendant(const String & subPath) const {return GetDescendantAux(subPath());}

   /** Removes the child with the given name.
    *  @param key The name of the child we wish to remove.
    *  @param optNotifyWith If non-NULL, the StorageReflectSession that should be used to notify subscribers that the given node has been removed
    *  @param recurse if true, the removed child's children will be removed from it, and so on, and so on...
    *  @param optCounter if non-NULL, this value will be incremented whenever a child is removed (recursively)
    *  @return B_NO_ERROR if the given child is found and removed, or B_ERROR if it could not be found.
    */
   status_t RemoveChild(const String & key, StorageReflectSession * optNotifyWith, bool recurse, uint32 * optCounter);

   /** Returns an iterator that can be used for iterating over our set of children.
     * @param flags If specified, this is the set of HTIT_FLAG_* flags to pass to the Hashtable iterator constructor.
     */
   DataNodeRefIterator GetChildIterator(uint32 flags = 0) const {return _children ? _children->GetIterator(flags) : DataNodeRefIterator();}

   /** Returns the number of child nodes this node contains. */
   uint32 GetNumChildren() const {return _children ? _children->GetNumItems() : 0;}

   /** Returns true iff this node contains any child nodes. */
   bool HasChildren() const {return ((_children)&&(_children->HasItems()));}

   /** Returns the ASCII name of this node (e.g. "joe") */
   const String & GetNodeName() const {return _nodeName;}

   /** Generates and returns the full node path of this node (e.g. "/12.18.240.15/1234/beshare/files/joe").
     * @param retPath On success, this String will contain this node's absolute path.
     * @param startDepth The depth at which the path should start.  Defaults to zero, meaning the full path.
     *                   Values greater than zero will return a partial path (e.g. a startDepth of 1 in the
     *                   above example would return "12.18.240.15/1234/beshare/files/joe", and a startDepth
     *                   of 2 would return "1234/beshare/files/joe")
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t GetNodePath(String & retPath, uint32 startDepth = 0) const;

   /** A more convenient verseion of the above GetNodePath() implementation.
     * @param startDepth The depth at which the path should start.  Defaults to zero, meaning the full path.
     *                   Values greater than zero will return a partial path (e.g. a startDepth of 1 in the
     *                   above example would return "12.18.240.15/1234/beshare/files/joe", and a startDepth
     *                   of 2 would return "1234/beshare/files/joe")
     * @returns this node's node path as a String.
     */
   String GetNodePath(uint32 startDepth = 0) const {String ret; (void) GetNodePath(ret, startDepth); return ret;}

   /** Returns the name of the node in our path at the (depth) level.
     * @param depth The node name we are interested in.  For example, 0 will return the name of the
     *              root node, 1 would return the name of the IP address node, etc.  If this number
     *              is greater than (depth), this method will return NULL.
     */
   const String * GetPathClause(uint32 depth) const;

   /** Replaces this node's payload message with that of (data).
    *  @param data the new Message to associate with this node.
    *  @param optNotifyWith if non-NULL, this StorageReflectSession will be used to notify subscribers that this node's data has changed.
    *  @param isBeingCreated Should be set true only if this is the first time SetData() was called on this node after its creation.
    *                        Which is to say, this should almost always be false.
    */
   void SetData(const MessageRef & data, StorageReflectSession * optNotifyWith, bool isBeingCreated);

   /** Returns a reference to this node's Message payload. */
   const MessageRef & GetData() const {return _data;}

   /** Returns our node's parent, or NULL if this node doesn't have a parent node. */
   DataNode * GetParent() const {return _parent;}

   /** Returns this node's depth in the tree (e.g. zero if we are the root node, 1 if we are its child, etc) */
   uint32 GetDepth() const {return _depth;}

   /** Returns us to our virgin, pre-Init() state, by clearing all our children, subscribers, parent, etc.  */
   void Reset();  

   /**
    * Modifies the refcount for the given sessionID.
    * Any sessionID's with (refCount > 0) will be in the GetSubscribers() list.
    * @param sessionID the sessionID whose reference count is to be modified
    * @param delta the amount to add to the reference count.
    */
   void IncrementSubscriptionRefCount(const String & sessionID, long delta);

   /** Returns an iterator that can be used to iterate over our list of active subscribers */
   HashtableIterator<const String *, uint32> GetSubscribers() const {return _subscribers ? _subscribers->GetIterator() : HashtableIterator<const String *, uint32>();}

   /** Returns a pointer to our ordered-child index */
   const Queue<DataNodeRef> * GetIndex() const {return _orderedIndex;}

   /** Insert a new entry into our ordered-child list at the (nth) entry position.
    *  Don't call this function unless you really know what you are doing!
    *  @param insertIndex Offset into the array to insert at
    *  @param optNotifyWith Session to use to inform everybody that the index has been changed.
    *  @param key Name of an existing child of this node that should be associated with the given entry.
    *             This child must not already be in the ordered-entry index!
    *  @return B_NO_ERROR on success, or B_ERROR on failure (bad index or unknown child).
    */
   status_t InsertIndexEntryAt(uint32 insertIndex, StorageReflectSession * optNotifyWith, const String & key);

   /** Removes the (nth) entry from our ordered-child index, if possible.
    *  Don't call this function unless you really know what you are doing!
    *  @param removeIndex Offset into the array to remove
    *  @param optNotifyWith Session to use to inform everybody that the index has been changed.
    *  @return B_NO_ERROR on success, or B_ERROR on failure.
    */
   status_t RemoveIndexEntryAt(uint32 removeIndex, StorageReflectSession * optNotifyWith);

   /** Returns the largest ID value that this node has seen in one of its children, since the
     * time it was created or last cleared.  Note that child nodes' names are assumed to include
     * their ID value in ASCII format, possible with a preceding letter "I" (for indexed nodes).
     * Child nodes whose names aren't in this format will be counted having ID zero.
     * This value can be useful as a hint for generating new IDs.
     */
   uint32 GetMaxKnownChildIDHint() const {return _maxChildIDHint;}

   /** You can manually set the max-known-child-ID hint here if you want to. */
   void SetMaxKnownChildIDHint(uint32 maxID) {_maxChildIDHint = maxID;}

   /** Convenience method:  Returns true iff (ancestor) exists
     * anywhere along the path between this node and the root node.
     * (note that a node is not considered a descendant of itself)
     * @param ancestor The node to check to see if we are a descendant of
     */
   bool IsDescendantOf(const DataNode & ancestor) const {const DataNode * n = GetParent(); while(n) {if (n == &ancestor) return true; else n = n->GetParent();} return false;}

   /** Convenience method:  Returns true iff (this) exists
     * anywhere along the path between (descendant) and the root node.
     * (note that a node is not considered an ancestor of itself)
     * @param descendant The node to check to see if we are a descendant of it
     */
   bool IsAncestorOf(const DataNode & descendant) const {return descendant.IsDescendantOf(*this);}

   /** Convenience method:  Parses (path) as a series of slash-separated 
     * tokens (e.g. "some/node/names/here") which may contain regex chars 
     * if you wish.  Returns the first DataNode whose path (relative to this 
     * node) matches (path).  Returns NULL if no matching node is found.  
     * If the path is empty (""), this function returns (this).
     * @param path The path to match against.  May contain regex chars.
     *             If the path starts with a slash ("/"), the search will
     *             begin at the root node; otherwise the path is considered
     *             to be relative to this node.
     * @param maxDepth The maximum number of tokens in the path to parse.
     *                 If left at MUSCLE_NO_LIMIT (the default), then the
     *                 entire path will be parsed.  If set to zero, then
     *                 this method will either return (this) or NULL.  If set
     *                 to one, only the first token will be parsed, and the
     *                 method will return either one of this node's children,
     *                 or NULL.   Etc.
     * @returns The first DataNode whose path matches (path), or NULL if no match is found.
     */
   DataNode * FindFirstMatchingNode(const char * path, uint32 maxDepth = MUSCLE_NO_LIMIT) const;

   /** Convenience function:  returns the root node of the node tree (by 
     * traversing parent links up to the top of the tree) 
     */
   DataNode * GetRootNode() const {DataNode * r = const_cast<DataNode *>(this); while(r->GetParent()) r = r->GetParent(); return r;}

   /** Convenience function:  Given a depth value less than or equal to our depth, returns a pointer to our ancestor node at that depth.
     * @param depth The depth of the node we want returned, relative to the root of the tree.  Zero would be the root node, one would be a child 
     *              of the root node, and so on.
     * @returns an ancestor DataNode, or NULL if such a node could not be found (most likely because (depth) is greater than this node's depth)
     */ 
   DataNode * GetAncestorNode(uint32 depth) const 
   {
      DataNode * r = const_cast<DataNode *>(this); 
      while((r)&&(depth <= r->GetDepth()))
      {
         if (depth == r->GetDepth()) return r;
         r = r->GetParent();
      }
      return NULL;
   }

   /** Returns a checksum representing the state of this node and the nodes
     * beneath it, up to the specified recursion depth.  Each node's checksum
     * includes its nodename, its ordered-children-index (if any), and its payload Message.
     * @param maxRecursionCount The maximum number of times to recurse.  Zero would
     *                          result in a checksum for this node only; one for this
     *                          node and its children only, etc.  Defaults to 
     *                          MUSCLE_NO_LIMIT.
     * @note This method can be CPU-intensive; it is meant primarily for debugging.
     * @returns a 32-bit checksum value based on the contents of this node and
     *          its descendants. 
     */
   uint32 CalculateChecksum(uint32 maxRecursionCount = MUSCLE_NO_LIMIT) const;

   /** For debugging purposes; prints the current state of this node (and
     * optionally its descendants) to stdout or to another file you specify.
     * @param optFile If non-NULL, the text will be printed to this file.  If left as NULL, stdout will be used as a default.
     * @param maxRecursionDepth The maximum number of times to recurse.  Zero would
     *                          result in a checksum for this node only; one for this
     *                          node and its children only, etc.  Defaults to 
     *                          MUSCLE_NO_LIMIT.
     * @param indentLevel how many spaces to indent the generated text
     */
   void PrintToStream(FILE * optFile = NULL, uint32 maxRecursionDepth = MUSCLE_NO_LIMIT, int indentLevel = 0) const;

private:
   friend class StorageReflectSession;
   friend class ObjectPool<DataNode>;
   DataNodeRef GetDescendantAux(const char * subPath) const;

   /** Assignment operator.  Note that this operator is only here to assist with ObjectPool recycling operations, and doesn't actually
     * make this DataNode into a copy of (rhs)... that's why we have it marked private, so that it won't be accidentally used in the traditional manner.
     */
   DataNode & operator = (const DataNode & /*rhs*/) {Reset(); return *this;}

   void Init(const String & nodeName, const MessageRef & initialValue);
   void SetParent(DataNode * _parent, StorageReflectSession * optNotifyWith);
   status_t RemoveIndexEntry(const String & key, StorageReflectSession * optNotifyWith);

   DataNode * _parent;
   MessageRef _data;
   mutable uint32 _cachedDataChecksum;
   Hashtable<const String *, DataNodeRef> * _children;  // lazy-allocated
   Queue<DataNodeRef> * _orderedIndex;  // only used when tracking the ordering of our children (lazy-allocated)
   uint32 _orderedCounter;
   String _nodeName;
   uint32 _depth;  // number of ancestors our node has (e.g. root's _depth is zero)
   uint32 _maxChildIDHint;  // keep track of the largest child ID, for easier allocation of non-conflicting future child IDs

   Hashtable<const String *, uint32> * _subscribers;  // lazy-allocated
};

}; // end namespace muscle

#endif
