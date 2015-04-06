/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleStorageReflectSession_h
#define MuscleStorageReflectSession_h

#include "reflector/DataNode.h"
#include "reflector/DumbReflectSession.h"
#include "reflector/StorageReflectConstants.h"
#include "regex/PathMatcher.h"

namespace muscle {

/**
 *  This is a factory class that returns new StorageReflectSession objects.
 */
class StorageReflectSessionFactory : public ReflectSessionFactory, private CountedObject<StorageReflectSessionFactory>
{
public:
   /** Default constructor.  The maximum incoming message size is set to "unlimited" by default.  */
   StorageReflectSessionFactory();

   /** Returns a new StorageReflectSession */
   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   /** Sets the maximum-bytes-per-incoming-message limit that we will set on the StorageReflectSession
     * objects that we create.
     * @param maxIncomingMessageBytes The maximum byte size of incoming flattened messages to allow.
     */
   void SetMaxIncomingMessageSize(uint32 maxIncomingMessageBytes) {_maxIncomingMessageSize = maxIncomingMessageBytes;}

   /** Returns our current setting for the maximum incoming message size for sessions we produce. */
   uint32 GetMaxIncomingMessageSize() const {return _maxIncomingMessageSize;}

protected:
   /** If we have a limited maximum size for incoming messages, then this method 
     * demand-allocate the session's gateway, and set its max incoming message size if possible.
     * @return B_NO_ERROR on success, or B_ERROR on failure (out of memory or the created gateway
     *         wasn't a MessageIOGateway)
     */
   status_t SetMaxIncomingMessageSizeFor(AbstractReflectSession * session) const;

private:
   uint32 _maxIncomingMessageSize;
};

/** This class is an interface to an object that can prune the traversals used
  * by RestoreNodeTreeFromMessage(), SaveNodeTreeToMessage(), and CloneDataNodeSubtree()
  * so that only a subset of the traversal is done.
  */
class ITraversalPruner
{
public:
   /** Default Constructor */
   ITraversalPruner() {/* empty */}

   /** Destructor */
   virtual ~ITraversalPruner() {/* empty */}

   /** Should be implemented to return true iff we should traverse the node specified by (path)
     * and his descendants.  If this method returns false, the node specified by (path)
     * will not be traversed, nor will any of his descendants.
     * @param path The relative path of the node that is about to be traversed.
     * @param nodeData A reference to the Message to be associated with this node.
     *                 If desired, this can be replaced with a different MessageRef instead
     *                 (but be careful not to modify the Message that (nodeData) points to; 
     *                 instead, allocate a new Message and set (nodeData) to point to it.
     */
   virtual bool MatchPath(const String & path, MessageRef & nodeData) const = 0;
};

#define DECLARE_MUSCLE_TRAVERSAL_CALLBACK(sessionClass, funcName) \
 int funcName(DataNode & node, void * userData); \
 static int funcName##Func(StorageReflectSession * This, DataNode & node, void * userData) {return (static_cast<sessionClass *>(This))->funcName(node, userData);}

/** An intelligent AbstractReflectSession that knows how to
 *  store data on the server, and filter using wildcards.  This class
 *  is used by the muscled server program to handle incoming connections.
 *  See StorageReflectConstants.h and/or "The Beginner's Guide.html"
 *  for details.
 */
class StorageReflectSession : public DumbReflectSession, private CountedObject<StorageReflectSession>
{
public:
   /** Default constructor. */
   StorageReflectSession();

   /** Virtual Destructor. */
   virtual ~StorageReflectSession();

   /** Called after the constructor, when the session is ready to interact with the server.
    *  @return B_NO_ERROR if everything is okay to go, B_ERROR if there was a problem
    *          setting up, or if the IP address of our client has been banned. 
    */
   virtual status_t AttachedToServer();

   /** Implemented to remove our nodes from the server-side database and do misc cleanup. */
   virtual void AboutToDetachFromServer();

   /** Called when a new message is received from our IO gateway. */
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Overridden to call PushSubscriptionMessages() */
   virtual void AfterMessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Returns a human-readable label for this session type:  "Session" */
   virtual const char * GetTypeName() const {return "Session";}

   /** Prints to stdout a report of what sessions are currently present on this server, and how
     * much memory each of them is currently using for various things.  Useful for understanding
     * what your RAM is being used for.
     */
   void PrintSessionsInfo() const;

   /** Prints to stdout a report of what ReflectSessionFactories are currently present on this server, 
     * and what interfaces and ports they are listening on.
     */
   void PrintFactoriesInfo() const;

   /** Returns a read-only reference to our parameters message */
   const Message & GetParametersConst() const {return _parameters;}

protected:
   /**
    * Create or Set the value of a data node.
    * @param nodePath Should be the path relative to the home dir (e.g. "MyNode/Child1/Grandchild2")
    * @param dataMsgRef The value to set the node to
    * @param allowOverwriteData Indicates whether existing node-data may be overwritten.  If false, the method will fail if the specified node already exists.
    * @param allowCreateNode indicates whether new nodes may be created.  (If false, the method will fail if any node in the specified node path doesn't already exist)
    * @param quiet If set to true, subscribers won't be updated regarding this change to the database.
    * @param addToIndex If set to true, this node will be inserted under its parent as a new indexed node, rather than doing the regular add/replace bit.
    * @param optInsertBefore If (addToIndex) is true, this may be the name of the node to insert this new node before in the index.
    *                        If NULL, the new node will be appended to the end of the index.  If (addToIndex) is false, this argument is ignored.
    * @return B_NO_ERROR on success, or B_ERROR on failure.
    */
   virtual status_t SetDataNode(const String & nodePath, const MessageRef & dataMsgRef, bool allowOverwriteData=true, bool allowCreateNode=true, bool quiet=false, bool addToIndex=false, const String *optInsertBefore=NULL);

   /** Remove all nodes that match (nodePath).
    *  @param nodePath A relative path indicating node(s) to remove.  Wildcarding is okay.
    *  @param filterRef If non-NULL, we'll use the given QueryFilter object to filter out our result set.
    *                   Only nodes whose Messages match the QueryFilter will be removed.  Default is a NULL reference.
    *  @param quiet If set to true, subscribers won't be updated regarding this change to the database
    *  @return B_NO_ERROR on success, or B_ERROR on failure.
    */
   virtual status_t RemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef = ConstQueryFilterRef(), bool quiet = false);

   /**
    * Recursively saves a given subtree of the node database into the given Message object, for safe-keeping.
    * (It's a bit more efficient than it looks, since all data Messages are reference counted rather than copied)
    * @param msg the Message to save the subtree into.  This object can later be provided to RestoreNodeTreeFromMessage() to restore the subtree.
    * @param node The node to begin recursion from (i.e. the root of the subtree)
    * @param path The path to prepend to the paths of children of the node.  Used in the recursion; you typically want to pass in "" here.
    * @param saveData Whether or not the payload Message of (node) should be saved.  The payload Messages of (node)'s children will always be saved no matter what, as long as (maxDepth) is greater than zero.
    * @param maxDepth How many levels of children should be saved to the Message.  If left as MUSCLE_NO_LIMIT (the default),
    *                 the entire subtree will be saved; otherwise the tree will be clipped to at most (maxDepth) levels.
    *                 If (maxDepth) is zero, only (node) will be saved.
    * @param optPruner If set non-NULL, this object will be used as a callback to prune the traversal, and optionally
    *                  to filter the data that gets saved into (msg).
    * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t SaveNodeTreeToMessage(Message & msg, const DataNode * node, const String & path, bool saveData, uint32 maxDepth = MUSCLE_NO_LIMIT, const ITraversalPruner * optPruner = NULL) const;

   /**
    * Recursively creates or updates a subtree of the node database from the given Message object.
    * (It's a bit more efficient than it looks, since all data Messages are reference counted rather than copied)
    * @param msg the Message to restore the subtree into.  This Message is typically one that was created earlier by SaveNodeTreeToMessage().
    * @param path The relative path of the root node, e.g. "" is your home session node.
    * @param loadData Whether or not the payload Message of (node) should be restored.  The payload Messages of (node)'s children will always be restored no matter what.
    * @param appendToIndex Used in the recursion to handle restoring indexed nodes.  You will usually want to Leave it as false when you call this method.
    * @param maxDepth How many levels of children should be restored from the Message.  If left as MUSCLE_NO_LIMIT (the default),
    *                 the entire subtree will be restored; otherwise the tree will be clipped to at most (maxDepth) levels.
    *                 If (maxDepth) is zero, only (node) will be restored.
    * @param optPruner If set non-NULL, this object will be used as a callback to prune the traversal, and optionally
    *                  to filter the data that gets loaded from (msg).
    * @param quiet If set to true, subscribers won't be updated regarding this change to the database
    * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t RestoreNodeTreeFromMessage(const Message & msg, const String & path, bool loadData, bool appendToIndex = false, uint32 maxDepth = MUSCLE_NO_LIMIT, const ITraversalPruner * optPruner = NULL, bool quiet = false);

   /** 
     * Create and insert a new node into one or more ordered child indices in the node tree.
     * This method is similar to calling MessageReceivedFromGateway() with a PR_COMMAND_INSERTORDEREDDATA 
     * Message, only it gives more information back about what happened.
     * @param insertMsg a PR_COMMAND_INSERTORDEREDDATA Message specifying what insertions should be done.
     * @param optRetNewNodes If non-NULL, any newly-created DataNodes will be adde to this table for your inspection.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   virtual status_t InsertOrderedData(const MessageRef & insertMsg, Hashtable<String, DataNodeRef> * optRetNewNodes);

   /**
     * Utility method:  Adds a new child node to the specified parent node. 
     * This method calls through to parentNode.InsertOrderedChild(), but also updates the StorageReflectSession's own internal state as necessary.
     * @param parentNode The node to add the new child node to.
     * @param data Reference to a message to create a new child node for.
     * @param optInsertBefore if non-NULL, the name of the child to put the new child before in our index.  If NULL, (or the specified child cannot be found) the new child will be appended to the end of the index.
     * @param optAddNewChildren If non-NULL, any newly formed nodes will be added to this hashtable, keyed on their absolute node path.
     * @return B_NO_ERROR on success, B_ERROR if out of memory
     */
   status_t InsertOrderedChildNode(DataNode & parentNode, const String * optInsertBefore, const MessageRef & data, Hashtable<String, DataNodeRef> * optAddNewChildren);

   /**
    * This typedef represents the proper signature of a node-tree traversal callback function.
    * Functions with this signature may be used with the NodePathMatcher::DoTraversal() method.
    * @param This The StorageReflectSession doing the traveral, as specified in the DoTraversal() call
    * @param node The DataNode that was matched the criteria of the traversal
    * @param userData the same value that was passed in to the DoTraversal() method.
    * @return The callback function should return the depth at which the traversal should continue after
    *         the callback is done.  So to allow the traversal to continue normally, return (node.GetDepth()),
    *         or to terminate the traversal immediately, return 0, or to resume the search at the next
    *         session, return 2, or etc.
    */
   typedef int (*PathMatchCallback)(StorageReflectSession * This, DataNode & node, void * userData);

   /** A slightly extended version of PathMatcher that knows how to handle DataNodes directly. */
   class NodePathMatcher : public PathMatcher
   {
   public:
      /** Default constructor. */
      NodePathMatcher()  {/* empty */}

      /** Destructor. */
      ~NodePathMatcher() {/* empty */}

      /**
       * Returns true iff the given node matches our query.
       * @param node the node to check to see if it matches
       * @param optData Reference to a Message to use for QueryFilter filtering, or NULL to disable filtering.
       *                Note that a filter may optionally retarget this ConstMessageRef to point to a different
       *                Message, but it is not allowed to modify the Message that (optData) points to.
       * @param rootDepth the depth at which the traversal started (i.e. 0 if started at root)
       */
      bool MatchesNode(DataNode & node, ConstMessageRef & optData, int rootDepth) const;

      /**
       * Does a depth-first traversal of the node tree, starting with (node) as the root.
       * @param cb The callback function to call whenever a node is encountered in the traversal
       *           that matches at least one of our path strings.
       * @param This pointer to our owner StorageReflectSession object.
       * @param node The node to begin the traversal at.
       * @param useFilters If true, we will only call (cb) on nodes whose Messages match our filter; otherwise
       *                   we'll call (cb) on any node whose path matches, regardless of filtering status.
       * @param userData Any value you wish; it will be passed along to the callback method.
       * @returns The number of times (cb) was called by this traversal.
       */
      uint32 DoTraversal(PathMatchCallback cb, StorageReflectSession * This, DataNode & node, bool useFilters, void * userData);
 
      /**
       * Returns the number of path-strings that we contain that match (node).
       * Note this is a bit more expensive than MatchesNode(), as we can't use short-circuit boolean logic here!
       * @param node A node to check against our set of path-matcher strings.
       * @param optData If non-NULL, any QueryFilters will use this Message to filter against.
       * @param nodeDepth the depth at which the traversal started (i.e. 0 if started at root)
       */
      uint32 GetMatchCount(DataNode & node, const Message * optData, int nodeDepth) const;

   private:
      // This little structy class is used to pass context data around more efficiently
      class TraversalContext
      {
      public:
         TraversalContext(PathMatchCallback cb, StorageReflectSession * This, bool useFilters, void * userData, int rootDepth) : _cb(cb), _This(This), _useFilters(useFilters), _userData(userData), _rootDepth(rootDepth), _visitCount(0) {/* empty */}

         int CallCallbackMethod(DataNode & nextChild) {_visitCount++; return _cb(_This, nextChild, _userData);}
         uint32 GetVisitCount() const {return _visitCount;}
         int GetRootDepth() const {return _rootDepth;}
         bool IsUseFiltersOkay() const {return _useFilters;}

      private:
         PathMatchCallback _cb;
         StorageReflectSession * _This;
         bool _useFilters;
         void * _userData;
         int _rootDepth;
         uint32 _visitCount;
      };

      int DoTraversalAux(TraversalContext & data, DataNode & node);
      bool PathMatches(DataNode & node, ConstMessageRef & optData, const PathMatcherEntry & entry, int rootDepth) const;
      bool CheckChildForTraversal(TraversalContext & data, DataNode * nextChild, int & depth);
   };

   friend class DataNode;

   /**
    * Returns true iff we have the given PR_PRIVILEGE_* privilege.
    * @return Default implementation looks at the PR_NAME_PRIVILEGE_BITS parameter.
    */
   virtual bool HasPrivilege(int whichPriv) const;

   /**
    * Returns the given Message to our client, inside an error message
    * with the given error code.
    */
   void BounceMessage(uint32 errorCode, const MessageRef & msgRef);

   /** Removes our nodes from the tree and removes our subscriptions from our neighbors.  Called by the destructor.
     */
   void Cleanup();

   /** Convenience method:  Adds sessions that contain nodes that match the given pattern to the passed-in Hashtable.
    *  @param nodePath the node path to match against.  May be absolute (e.g. "/0/1234/frc*") or relative (e.g. "blah")
    *                  If the nodePath is a zero-length String, all sessions will match.
    *  @param filter If non-NULL, only nodes whose data Messages match this filter will have their sessions added 
    *                to the (retSessions) table.
    *  @param retSessions A table that will on return contain the set of matching sessions, keyed by their session ID strings.
    *  @param matchSelf If true, we will include as a candidate for pattern matching.  Otherwise we won't.
    *  @param maxResults Maximum number of matching sessions to return.  Defaults to MUSCLE_NO_LIMIT.
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t FindMatchingSessions(const String & nodePath, const ConstQueryFilterRef & filter, Hashtable<const String *, AbstractReflectSessionRef> & retSessions, bool matchSelf, uint32 maxResults = MUSCLE_NO_LIMIT) const;

   /** Convenience method:  Same as FindMatchingSessions(), but finds only the first matching session.  
     * Returns a reference to the first matching session on success, or a NULL reference on failue.
     */
   AbstractReflectSessionRef FindMatchingSession(const String & nodePath, const ConstQueryFilterRef & filter, bool matchSelf) const;

   /** Convenience method:  Passes the given Message on to the sessions who match the given nodePath.
    *  (that is, any sessions who have nodes that match (nodePath) will have their MessageReceivedFromSession()
    *  method called with the given Message)
    *  @param msgRef the Message to pass on.
    *  @param nodePath the node path to match against.  May be absolute (e.g. "/0/1234/frc*") or relative (e.g. "blah")
    *                  If the nodePath is a zero-length String, all sessions will match.
    *  @param filter If non-NULL, only nodes whose data Messages match this filter will receive the Message.
    *  @param matchSelf If true, we will include as a candidate for pattern matching.  Otherwise we won't.
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t SendMessageToMatchingSessions(const MessageRef & msgRef, const String & nodePath, const ConstQueryFilterRef & filter, bool matchSelf);

   /** Convenience method:  Adds nodes that match the specified path to the passed-in Queue.
    *  @param nodePath the node path to match against.  May be absolute (e.g. "/0/1234/frc*") or relative (e.g. "blah").  
                       If it's a relative path, only nodes in the current session's subtree will be searched.
    *  @param filter If non-NULL, only nodes whose data Messages match this filter will be added to the (retMatchingNodes) table.
    *  @param retMatchingNodes A Queue that will on return contain the list of matching nodes.
    *  @param maxResults Maximum number of matching nodes to return.  Defaults to MUSCLE_NO_LIMIT.
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t FindMatchingNodes(const String & nodePath, const ConstQueryFilterRef & filter, Queue<DataNodeRef> & retMatchingNodes, uint32 maxResults = MUSCLE_NO_LIMIT) const;

   /** Convenience method:  Same as FindMatchingNodes(), but finds only the first matching node.  
     * Returns a reference to the first matching node on success, or a NULL reference on failue.
     */
   DataNodeRef FindMatchingNode(const String & nodePath, const ConstQueryFilterRef & filter) const;

   /** Convenience method (used by some customized daemons) -- Given a source node and a destination path,
    * Make (path) a deep, recursive clone of (node).
    * @param sourceNode Reference to a DataNode to clone.
    * @param destPath Path of where the newly created node subtree will appear.  Should be relative to our home node.
    * @param allowOverwriteData If true, we will clobber any previously existing node at the destination path.
    *                           Otherwise, the existence of a pre-existing node there will cause us to fail.
    * @param allowCreateNode If true, we will create a node at the destination path if necessary.
    *                        Otherwise, the non-existence of a pre-existing node there will cause us to fail.
    * @param quiet If false, no subscribers will be notified of the changes we make.
    * @param addToTargetIndex If true, the newly created subtree will be added to the target node using InsertOrderedChild().
    *                         If false, it will be added using PutChild().
    * @param optInsertBefore If (addToTargetIndex) is true, this argument will be passed on to InsertOrderedChild().
    *                        Otherwise, this argument is ignored.
    * @param optPruner If non-NULL, this object can be used as a callback to prune the traversal or filter
    *                  the MessageRefs cloned.
    * @return B_NO_ERROR on success, or B_ERROR on failure (may leave a partially cloned subtree on failure)
    */
   status_t CloneDataNodeSubtree(const DataNode & sourceNode, const String & destPath, bool allowOverwriteData=true, bool allowCreateNode=true, bool quiet=false, bool addToTargetIndex=false, const String * optInsertBefore = NULL, const ITraversalPruner * optPruner = NULL);

   /** Tells other sessions that we have modified (node) in our node subtree.
    *  @param node The node that has been modfied.
    *  @param oldData If the node is being modified, this argument contains the node's previously
    *                 held data.  If it is being created, this is a NULL reference.  If the node
    *                 is being destroyed, this will contain the node's current data.
    *  @param isBeingRemoved If true, this node is about to go away.
    */
   virtual void NotifySubscribersThatNodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved);

   /** Tells other sessions that we have changed the index of (node) in our node subtree.
    *  @param node The node whose index was changed.
    *  @param op The INDEX_OP_* opcode of the change.  (Note that currently INDEX_OP_CLEARED is never used here)
    *  @param index The index at which the operation took place (not defined for clear operations)
    *  @param key The key of the operation (aka the name of the associated node)
    */
   virtual void NotifySubscribersThatNodeIndexChanged(DataNode & node, char op, uint32 index, const String & key);

   /** Called by NotifySubscribersThatNodeChanged(), to tell us that (node) has been 
    *  created, modified, or is about to be destroyed.
    *  @param node The node that was modified, created, or is about to be destroyed.
    *  @param oldData If the node is being modified, this argument contains the node's previously
    *                 held data.  If it is being created, this is a NULL reference.  If the node
    *                 is being destroyed, this will contain the node's current data.
    *  @param isBeingRemoved True iff this node is about to be destroyed.
    */
   virtual void NodeChanged(DataNode & node, const MessageRef & oldData, bool isBeingRemoved);

   /** Called by NotifySubscribersThatIndexChanged() to tell us how (node)'s index has been modified.  
    *  @param node The node whose index was changed.
    *  @param op The INDEX_OP_* opcode of the change.  (Note that currently INDEX_OP_CLEARED is never used here)
    *  @param index The index at which the operation took place (not defined for clear operations)
    *  @param key The key of the operation (aka the name of the associated node)
    */
   virtual void NodeIndexChanged(DataNode & node, char op, uint32 index, const String & key);

   /**
    * Takes any messages that were created in the NodeChanged() callbacks and 
    * sends them to their owner's MessageReceivedFromSession() method for
    * processing and eventual forwarding to the client.
    */
   void PushSubscriptionMessages();

   /** Auxilliary helper method for PushSubscriptionMessages() */
   void PushSubscriptionMessage(MessageRef & msgRef); 

   /**
    * Executes a data-gathering tree traversal based on the PR_NAME_KEYS specified in the given message.
    * @param getMsg a Message whose PR_NAME_KEYS field specifies which nodes we are interested in.
    */
   void DoGetData(const Message & getMsg);

   /**
    * Executes a node-removal traversal using the given NodePathMatcher.
    * Note that you may find it easier to call RemoveDataNodes() than to call this method directly.
    * @param matcher Reference to the NodePathMatcher object to use to guide the node-removal traversal.
    * @param quiet If set to true, subscribers won't be updated regarding this change to the database
    */
   void DoRemoveData(NodePathMatcher & matcher, bool quiet = false);

   /** Auxilliary helper function. */
   void SendGetDataResults(MessageRef & msg);

   /**
    * If set false, we won't receive subscription updates.
    * @param e Whether or not we wish to get update messages from our subscriptions.
    */
   void SetSubscriptionsEnabled(bool e) {_subscriptionsEnabled = e;}

   /** Returns true iff our "subscriptions enabled" flag is set.  Default state is of this flag is true.  */
   bool GetSubscriptionsEnabled() const {return _subscriptionsEnabled;}

   /** Called when a PR_COMMAND_GETPARAMETERS Message is received from our client.   After filling the usual
     * data into the PR_RESULTS_PARAMETERS reply Message, the StorageReflectSession class will call this method,
     * giving the subclass an opportunity to add additional (application-specific) data to the Message if it wants to.
     * Default implementation is a no-op.
     * @param parameterResultsMsg The PR_RESULT_PARAMETERS Message that is about to be sent back to the client.
     */
   virtual void AddApplicationSpecificParametersToParametersResultMessage(Message & parameterResultsMsg) const;

   /**
    * Convenience method:  Uses the given path to lookup a single node in the node tree
    * and return it.  As of MUSCLE v4.11, wildcarding is supported in the path argument.
    * If (path) begins with a '/', the search will begin with the root node of the tree; 
    * if not, it will begin with this session's node.  Returns NULL on failure.
    * @param path The fully specified path to a single node in the database.
    * @return A pointer to the specified DataNode, or NULL if the node wasn't found.
    */
   DataNode * GetDataNode(const String & path) const {return _sessionDir() ? _sessionDir()->FindFirstMatchingNode(path()) : NULL;}

   /**
    * Call this to get a new DataNode, instead of using the DataNode ctor directly.
    * @param nodeName The name to be given to the new DataNode that will be created.
    * @param initialValue The Message payload to be given to the new DataNode that will be created.
    * @return A reference to the new DataNode or a NULL reference if out of memory.
    */
   DataNodeRef GetNewDataNode(const String & nodeName, const MessageRef & initialValue);

   /**
    * Call this when you are done with a DataNode, instead of the DataNode destructor.
    * @param node The DataNode to delete or recycle.
    */
   void ReleaseDataNode(DataNode * node);

   /**
    * This method goes through the outgoing-messages list looking for PR_RESULT_DATAITEMS
    * messages.  For each such message that it finds, it will remove all result items
    * (both update items and items in PR_NAME_REMOVED_DATAITEMS) that match the expressions
    * in the given NodePathMatcher.  If the NodePathMatcher argument is NULL, all results
    * will be removed.  Any PR_RESULT_DATAITEMS messages that become empty will be removed
    * completely from the queue.
    * @param matcher Specifies which data items to delete from messages in the outgoing message queue.
    */
   void JettisonOutgoingResults(const NodePathMatcher * matcher);

   /**
    * This method goes through the outgoing-messages list looking for PR_RESULT_SUBTREE
    * messages.  For each such message that it finds, it will see if the message's PR_NAME_REQUEST_TREE_ID
    * field matches the pattern specified by (optMatchString), and if it does, it will remove
    * that Message.  If (optMatchString) is NULL, on the other hand, all PR_RESULT_SUBTREE
    * Messages that have no PR_NAME_REQUEST_TREE_ID field will be removed.
    * @param optMatchString Optional pattern to match PR_NAME_REQUEST_TREE_ID strings against.  If
    *                       NULL, all PR_RESULT_SUBTREE Messages with no PR_NAME_TREE_REQUEST_ID field will match.
    */
   void JettisonOutgoingSubtrees(const String * optMatchString);

   /** Returns a reference to our session node */
   DataNodeRef GetSessionNode() const {return _sessionDir;}

   /** Returns a reference to our parameters message */
   Message & GetParameters() {return _parameters;}

   /** Returns a reference to the global root node of the database */
   DataNode & GetGlobalRoot() const {return *(_sharedData->_root());}

   /** Macro to declare the proper callback declarations for the message-passing callback.  */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, PassMessageCallback);    /** Matching nodes are sent the given message.  */

private:
   void NodeChangedAux(DataNode & modifiedNode, const MessageRef & nodeData, bool isBeingRemoved);
   void UpdateDefaultMessageRoute();
   status_t RemoveParameter(const String & paramName, bool & retUpdateDefaultMessageRoute);
   int PassMessageCallbackAux(DataNode & node, const MessageRef & msgRef, bool matchSelfOkay);
   void TallyNodeBytes(const DataNode & n, uint32 & retNumNodes, uint32 & retNodeBytes) const;

   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, KickClientCallback);     /** Sessions of matching nodes are EndSession()'d  */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, InsertOrderedDataCallback); /** Matching nodes have ordered data inserted into them as child nodes */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, ReorderDataCallback);    /** Matching nodes area reordered in their parent's index */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, GetSubtreesCallback);    /** Matching nodes are added in to the user Message as archived subtrees */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, GetDataCallback);        /** Matching nodes are added to the (userData) Message */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, RemoveDataCallback);     /** Matching nodes are placed in a list (userData) for later removal. */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, DoSubscribeRefCallback); /** Matching nodes are ref'd or unref'd with subscribed session IDs */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, ChangeQueryFilterCallback); /** Matching nodes are ref'd or unref'd depending on the QueryFilter change */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, FindSessionsCallback);   /** Sessions of matching nodes are added to the given Hashtable */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, FindNodesCallback);      /** Matching nodes are added to the given Queue */
   DECLARE_MUSCLE_TRAVERSAL_CALLBACK(StorageReflectSession, SendMessageCallback);    /** Similar to PassMessageCallback except matchSelf is an argument */

   /**
    * Called by SetParent() to tell us that (node) has been created at a given location.
    * We then respond by letting any matching subscriptions add their mark to the node.
    * Private because subclasses should override NodeChanged(), not this.
    * @param node the new Node that has been added to the database.
    */
   void NodeCreated(DataNode & node);

   /** Tells other sessions that we have a new node available.  
    *  Private because subclasses should override NotifySubscriberThatNodeChanged(), not this.
    *  @param newNode The newly available node.
    */
   void NotifySubscribersOfNewNode(DataNode & newNode);

   /** This class holds data that needs to be shared by all attached instances
     * of the StorageReflectSession class.  An instance of this class is stored
     * on demand in the central-state Message.
     */
   class StorageReflectSessionSharedData
   {
   public:
      StorageReflectSessionSharedData(const DataNodeRef & root) : _root(root), _subsDirty(false) {/* empty */}

      DataNodeRef _root;
      bool _subsDirty;
   };

   /** Sets up the global root and other shared data */
   StorageReflectSessionSharedData * InitSharedData();

   /** our current parameter set */
   Message _parameters;  

   /** cached to be sent when a subscription triggers */
   MessageRef _nextSubscriptionMessage;  

   /** cached to be sent when an index subscription triggers */
   MessageRef _nextIndexSubscriptionMessage;  

   /** Points to shared data object; this object is the same for all StorageReflectSessions */
   StorageReflectSessionSharedData * _sharedData;       

   /** this session's subdir (grandchild of _globalRoot) */
   DataNodeRef _sessionDir;      

   /** Our session's set of active subscriptions */
   NodePathMatcher _subscriptions;  

   /** Where user messages get sent if no PR_NAME_KEYS field is present */
   NodePathMatcher _defaultMessageRoute;  
   Message _defaultMessageRouteMessage;

   /** Whether or not we set to report subscription updates or not */
   bool _subscriptionsEnabled;            

   /** Maximum number of subscription update fields per PR_RESULT message */
   uint32 _maxSubscriptionMessageItems;    

   /** Optimization flag:  set true the first time we index a node */
   bool _indexingPresent;                 

   /** The number of database nodes we currently have created */
   uint32 _currentNodeCount;              

   /** The maximum number of database nodes we are allowed to create */
   uint32 _maxNodeCount;                  

   /** Our node class needs access to our internals too */
   friend class StorageReflectSession :: NodePathMatcher;
};

}; // end namespace muscle

#endif

