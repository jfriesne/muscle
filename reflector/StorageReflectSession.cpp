/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <limits.h>
#include "reflector/ReflectServer.h"
#include "reflector/StorageReflectSession.h"
#include "iogateway/MessageIOGateway.h"

namespace muscle {

#define DEFAULT_PATH_PREFIX "*/*"  // when we get a path name without a leading '/', prepend this
#define DEFAULT_MAX_SUBSCRIPTION_MESSAGE_SIZE   50   // no more than 50 items/update message, please

// field under which we file our shared data in the central-state message
static const String SRS_SHARED_DATA = "srs_shared";

StorageReflectSessionFactory :: StorageReflectSessionFactory()
   : _maxIncomingMessageSize(MUSCLE_NO_LIMIT)
{
   // empty
}

AbstractReflectSessionRef StorageReflectSessionFactory :: CreateSession(const String &, const IPAddressAndPort &)
{
   TCHECKPOINT;

   StorageReflectSessionRef srs(newnothrow StorageReflectSession);
   if ((srs())&&(SetMaxIncomingMessageSizeFor(srs()).IsOK())) return srs;
   else
   {
      MWARN_OUT_OF_MEMORY;
      return AbstractReflectSessionRef();
   }
}

status_t StorageReflectSessionFactory :: SetMaxIncomingMessageSizeFor(AbstractReflectSession * session) const
{
   if (_maxIncomingMessageSize != MUSCLE_NO_LIMIT) 
   {
      if (session->GetGateway()() == NULL) session->SetGateway(session->CreateGateway());
      MessageIOGateway * gw = dynamic_cast<MessageIOGateway*>(session->GetGateway()());
      if (gw) gw->SetMaxIncomingMessageSize(_maxIncomingMessageSize);
         else return B_BAD_OBJECT;
   }
   return B_NO_ERROR;
}

StorageReflectSession ::
StorageReflectSession() : 
   _parameters(PR_RESULT_PARAMETERS), 
   _sharedData(NULL),
   _subscriptionsEnabled(true), 
   _maxSubscriptionMessageItems(DEFAULT_MAX_SUBSCRIPTION_MESSAGE_SIZE), 
   _indexingPresent(false),
   _currentNodeCount(0),
   _maxNodeCount(MUSCLE_NO_LIMIT)
{
   // empty
}

StorageReflectSession :: 
~StorageReflectSession() 
{
   // empty
}

StorageReflectSession::StorageReflectSessionSharedData * 
StorageReflectSession ::
InitSharedData()
{
   TCHECKPOINT;

   Message & state = GetCentralState();

   void * sp = NULL; (void) state.FindPointer(SRS_SHARED_DATA, sp);
   StorageReflectSession::StorageReflectSessionSharedData * sd = (StorageReflectSession::StorageReflectSessionSharedData *) sp;
   if (sd) return sd;
  
   // oops, there's no shared data object!  We must be the first session.
   // So we'll create the root node and the shared data object, and
   // add it to the central-state Message ourself.
   DataNodeRef globalRoot = GetNewDataNode("", CastAwayConstFromRef(GetEmptyMessageRef()));
   if (globalRoot()) 
   {
      sd = newnothrow StorageReflectSessionSharedData(globalRoot);
      if (sd)
      {
         if (state.ReplacePointer(true, SRS_SHARED_DATA, sd).IsOK()) return sd;
         delete sd;
      }
      else MWARN_OUT_OF_MEMORY;
   }
   return NULL;
}

status_t
StorageReflectSession ::
AttachedToServer()
{
   TCHECKPOINT;

   MRETURN_ON_ERROR(DumbReflectSession::AttachedToServer());

   _sharedData = InitSharedData();
   if (_sharedData == NULL) return B_OUT_OF_MEMORY;
   
   Message & state = GetCentralState();
   const String & hostname = GetHostName();
   const String & sessionid = GetSessionIDString();

   // Is there already a node for our hostname?
   DataNodeRef hostDir;
   if (GetGlobalRoot().GetChild(hostname, hostDir).IsError())
   {
      // nope.... we'll add one then
      hostDir = GetNewDataNode(hostname, CastAwayConstFromRef(GetEmptyMessageRef()));
      if ((hostDir() == NULL)||(GetGlobalRoot().PutChild(hostDir, this, this).IsError())) {Cleanup(); MRETURN_OUT_OF_MEMORY;}
   }

   // Create a new node for our session (we assume no such
   // node already exists, as session id's are supposed to be unique)
   if (hostDir() == NULL) {Cleanup(); MRETURN_OUT_OF_MEMORY;}
   if (hostDir()->HasChild(sessionid())) LogTime(MUSCLE_LOG_WARNING, "WARNING:  Non-unique session id [%s] being overwritten!\n", sessionid());

   SetSessionRootPath(hostname.Prepend("/") + "/" + sessionid);

   DataNodeRef sessionNode = GetNewDataNode(sessionid, CastAwayConstFromRef(GetEmptyMessageRef()));
   if (sessionNode())
   {
#ifdef MUSCLE_AVOID_IPV6
      const String & matchHostname = hostname;
#else
      String matchHostname = hostname;
      {
         // match against IPv4-style address-strings for IPv4 addresses, per Lior
         IPAddress ip = Inet_AtoN(hostname());
         if ((ip.IsValid())&&(ip.IsIPv4())) matchHostname = Inet_NtoA(ip, true);
      }
#endif

      // See if we get any special privileges
      int32 privBits = 0;
      for (int p=0; p<=PR_NUM_PRIVILEGES; p++)
      {
         char temp[32]; muscleSprintf(temp, "priv%i", p);
         const String * privPattern;
         for (int q=0; (state.FindString(temp, q, &privPattern).IsOK()); q++)
         {
            if (StringMatcher(*privPattern).Match(matchHostname()))
            {
               if (p == PR_NUM_PRIVILEGES) privBits = ~0;  // all privileges granted!
                                      else privBits |= (1L<<p);
               break;
            } 
         }
      }
      if (privBits != 0L) 
      {
         _parameters.RemoveName(PR_NAME_PRIVILEGE_BITS);
         _parameters.AddInt32(PR_NAME_PRIVILEGE_BITS, privBits);
      }

      _sessionDir = sessionNode;
      status_t ret;
      if (hostDir()->PutChild(_sessionDir, this, this).IsError(ret)) {Cleanup(); return ret;}
 
      // do subscription notifications here
      PushSubscriptionMessages();

      // Get our node-creation limit.  For now, this is the same for all sessions.
      // (someday maybe I'll work out a way to give different limits to different sessions)
      uint32 nodeLimit;
      if (state.FindInt32(PR_NAME_MAX_NODES_PER_SESSION, nodeLimit).IsOK()) _maxNodeCount = nodeLimit;
   
      return B_NO_ERROR;
   }

   Cleanup(); 
   MRETURN_OUT_OF_MEMORY;
}

void
StorageReflectSession ::
AboutToDetachFromServer()
{
   Cleanup();
   DumbReflectSession::AboutToDetachFromServer();
}

/** Used to pass arguments to the SubscribeRefCallback method */
class SubscribeRefCallbackArgs
{
public:
   explicit SubscribeRefCallbackArgs(int32 refCountDelta) : _refCountDelta(refCountDelta) {/* empty */}

   int32 GetRefCountDelta() const {return _refCountDelta;}

private:
   const int32 _refCountDelta;
};

void
StorageReflectSession ::
Cleanup()
{
   TCHECKPOINT;

   if (_sharedData)
   {
      DataNodeRef hostNodeRef;
      if (GetGlobalRoot().GetChild(GetHostName(), hostNodeRef).IsOK())
      {
         DataNode * hostNode = hostNodeRef();
         if (hostNode)
         {
            // make sure our session node is gone
            (void) hostNode->RemoveChild(GetSessionIDString(), this, true, NULL);

            // If our host node is now empty, it goes too
            if (hostNode->HasChildren() == false) (void) GetGlobalRoot().RemoveChild(hostNode->GetNodeName(), this, true, NULL);
         }
      
         PushSubscriptionMessages();
      }

      // If the global root is now empty, it goes too
      if (GetGlobalRoot().HasChildren() == false)
      {
         GetCentralState().RemoveName(SRS_SHARED_DATA);
         _sharedData->_root.Reset(); // do this first!
         delete _sharedData;
      }
      else
      {
         // Remove all of our subscription-marks from neighbor's nodes
         SubscribeRefCallbackArgs srcArgs(-2147483647);  // remove all of our subscriptions no matter how many ref-counts we have
         (void) _subscriptions.DoTraversal((PathMatchCallback)DoSubscribeRefCallbackFunc, this, GetGlobalRoot(), false, &srcArgs);

         // Remove any cached tables that reference our session-ID String, as we know they can no longer be useful to anyone.
         for (HashtableIterator<uint32, DataNodeSubscribersTableRef> iter(_sharedData->_cachedSubscribersTables); iter.HasData(); iter++)
         {
            if (iter.GetValue()()->GetSubscribers().ContainsKey(GetSessionIDString())) (void) _sharedData->_cachedSubscribersTables.Remove(iter.GetKey()); 
         }
      }

      _sharedData = NULL;
   }
   _nextSubscriptionMessage.Reset();
   _nextIndexSubscriptionMessage.Reset();
   TCHECKPOINT;
}

void 
StorageReflectSession ::
NotifySubscribersThatNodeChanged(DataNode & modifiedNode, const MessageRef & oldData, NodeChangeFlags nodeChangeFlags)
{
   TCHECKPOINT;

   for (HashtableIterator<String, uint32> subIter(modifiedNode.GetSubscribers()); subIter.HasData(); subIter++)
   {
      StorageReflectSession * next = dynamic_cast<StorageReflectSession *>(GetSession(subIter.GetKey())());
      if ((next)&&((next != this)||(IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF)))) next->NodeChanged(modifiedNode, oldData, nodeChangeFlags);
   }

   TCHECKPOINT;
}

void 
StorageReflectSession ::
NotifySubscribersThatNodeIndexChanged(DataNode & modifiedNode, char op, uint32 index, const String & key)
{
   TCHECKPOINT;

   for (HashtableIterator<String, uint32> subIter(modifiedNode.GetSubscribers()); subIter.HasData(); subIter++)
   {
      AbstractReflectSessionRef nRef = GetSession(subIter.GetKey());
      StorageReflectSession * s = dynamic_cast<StorageReflectSession *>(nRef());
      if (s) s->NodeIndexChanged(modifiedNode, op, index, key);
   }

   TCHECKPOINT;
}

void
StorageReflectSession ::
NotifySubscribersOfNewNode(DataNode & newNode)
{
   TCHECKPOINT;

   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      StorageReflectSession * next = dynamic_cast<StorageReflectSession *>(iter.GetValue()());
      if (next) next->NodeCreated(newNode);  // always notify; !Self filtering will be done elsewhere
   }

   TCHECKPOINT;
}

DataNodeSubscribersTableRef
StorageReflectSession ::
GetDataNodeSubscribersTableFromPool(const DataNodeSubscribersTableRef & curTable, const String & sessionIDString, int32 delta)
{
        if (delta == 0) return curTable;  // nothing to do!
   else if (delta < 0)
   {
      // See if we can just set the DataNode back to its default/null/empty-subscribers-table state
      const uint32 * soleRefCount = ((curTable())&&(curTable()->GetSubscribers().GetNumItems() == 1)) ? curTable()->GetSubscribers().Get(sessionIDString) : NULL;
      if ((soleRefCount)&&(*soleRefCount <= (uint32)(-delta))) return DataNodeSubscribersTableRef();
   }

   DataNodeSubscribersTableRef * cachedTable = _sharedData->_cachedSubscribersTables.Get(DataNodeSubscribersTable::HashCodeAfterModification(curTable()?curTable()->HashCode():0, sessionIDString, delta));
   if (cachedTable)
   {
      if (curTable())
      {
         if (curTable()->IsEqualToAfterModification(*cachedTable->GetItemPointer(), sessionIDString, delta)) return *cachedTable;
      }
      else if (delta > 0)
      {
         const Hashtable<String, uint32> & cachedSubs = cachedTable->GetItemPointer()->GetSubscribers();
         const uint32 * soleRefCount = (cachedSubs.GetNumItems() == 1) ? cachedSubs.Get(sessionIDString) : NULL;
         if ((soleRefCount)&&(*soleRefCount == (uint32)delta)) return *cachedTable; 
      }
   }

   // If we got here, we didn't have anything in our cache for the requested table, so we'll create a new table and store and return it
   DataNodeSubscribersTableRef newRef(newnothrow DataNodeSubscribersTable(curTable(), sessionIDString, delta));
   if ((newRef() == NULL)||(_sharedData->_cachedSubscribersTables.Put(newRef()->HashCode(), newRef).IsError())) MWARN_OUT_OF_MEMORY;
   return newRef; 
}

void
StorageReflectSession ::
NodeCreated(DataNode & newNode)
{
   newNode._subscribers = GetDataNodeSubscribersTableFromPool(newNode._subscribers, GetSessionIDString(), _subscriptions.GetMatchCount(newNode, NULL, 0));  // FogBugz #14596
}

void
StorageReflectSession ::
NodeChanged(DataNode & modifiedNode, const MessageRef & oldData, NodeChangeFlags nodeChangeFlags)
{
   TCHECKPOINT;

   if (GetSubscriptionsEnabled())
   {
      ConstMessageRef constNewData = modifiedNode.GetData();
      if (_subscriptions.GetNumFilters() > 0)
      {
         ConstMessageRef constOldData = oldData;  // We need a non-const ConstMessageRef to pass to MatchesNode(), in case MatchesNode() changes the ref -- even though we won't use the changed data
         const bool matchedBefore = _subscriptions.MatchesNode(modifiedNode, constOldData, 0);

         // uh oh... we gotta determine whether the modified node's status wrt QueryFilters has changed!
         // Based on that, we will simulate for the client the node's "addition" or "removal" at the appropriate times.
         if (nodeChangeFlags.IsBitSet(NODE_CHANGE_FLAG_ISBEINGREMOVED))
         {
            if (matchedBefore == false) return;  // since the node didn't match before either, no node-removed-update is necessary now
         }
         else if (constOldData())
         {
            const bool matchesNow = _subscriptions.MatchesNode(modifiedNode, constNewData, 0);

                 if ((matchedBefore == false)&&(matchesNow == false)) return;                 // no change in status, so no update is necessary
            else if ((matchedBefore)&&(matchesNow == false))          nodeChangeFlags.SetBit(NODE_CHANGE_FLAG_ISBEINGREMOVED);  // no longer matches, so we need to send a node-removed update
         }
         else if (matchedBefore == false) return;  // Adding a new node:  only notify the client if it matches at least one of his QueryFilters
         else (void) _subscriptions.MatchesNode(modifiedNode, constNewData, 0);  // just in case one our QueryFilters needs to modify (constNewData)
      }

      NodeChangedAux(modifiedNode, CastAwayConstFromRef(constNewData), nodeChangeFlags);
   }
}

void
StorageReflectSession ::
NodeChangedAux(DataNode & modifiedNode, const MessageRef & nodeData, NodeChangeFlags nodeChangeFlags)
{
   TCHECKPOINT;

   if (_nextSubscriptionMessage() == NULL) _nextSubscriptionMessage = GetMessageFromPool(PR_RESULT_DATAITEMS);
   if (_nextSubscriptionMessage())
   {
      _sharedData->_subsDirty = true;

      String np;
      if (modifiedNode.GetNodePath(np).IsOK())
      {
         if (nodeChangeFlags.IsBitSet(NODE_CHANGE_FLAG_ISBEINGREMOVED))
         {
            if (_nextSubscriptionMessage()->HasName(np, B_MESSAGE_TYPE))
            {
               // Oops!  We can't specify a remove-then-add operation for a given node in a single Message,
               // because the removes and the adds are expressed via different mechanisms.
               // So in this case we have to force a flush of the current message now, and 
               // then add the new notification to the next one!
               PushSubscriptionMessages();
               NodeChangedAux(modifiedNode, nodeData, nodeChangeFlags);  // and then start again
            }
            else (void) UpdateSubscriptionMessage(*_nextSubscriptionMessage(), np, MessageRef());
         }
         else
         {
            // If Supercede is enabled, get rid of any previous update for this nodePath, as it is now superceded by the new update
            // Note that for efficiency's sake I stop searching after finding just the most recent previous update for our nodePath
            if ((nodeChangeFlags.IsBitSet(NODE_CHANGE_FLAG_ENABLESUPERCEDE))&&(PruneSubscriptionMessage(*_nextSubscriptionMessage(), np).IsError()))
            {
               AbstractMessageIOGateway * gw = GetGateway()();
               if (gw)
               {
                  Queue<MessageRef> & oq = gw->GetOutgoingMessageQueue();
                  for (int32 i=oq.GetNumItems()-1; i>=0; i--)
                  {
                     Message & m = *oq[i]();
                     if ((m.what == PR_RESULT_DATAITEMS)&&(PruneSubscriptionMessage(m, np).IsOK()))
                     {
                        if (m.HasNames() == false) (void) oq.RemoveItemAt(i);
                        break;
                     }
                  }
               }
            }

            (void) UpdateSubscriptionMessage(*_nextSubscriptionMessage(), np, nodeData);
         }
      }

      if ((_nextSubscriptionMessage())&&(_nextSubscriptionMessage()->GetNumNames() >= _maxSubscriptionMessageItems)) PushSubscriptionMessages(); 
   }
   else MWARN_OUT_OF_MEMORY;
}

status_t
StorageReflectSession ::
UpdateSubscriptionMessage(Message & subscriptionMessage, const String & nodePath, const MessageRef & optMessageData)
{
   return optMessageData() ? subscriptionMessage.AddMessage(nodePath, optMessageData) : subscriptionMessage.AddString(PR_NAME_REMOVED_DATAITEMS, nodePath);
}

status_t
StorageReflectSession ::
UpdateSubscriptionIndexMessage(Message & subscriptionIndexMessage, const String & nodePath, char op, uint32 index, const String & key)
{
   char temp[100]; muscleSprintf(temp, "%c" UINT32_FORMAT_SPEC ":", op, index);
   return subscriptionIndexMessage.AddString(nodePath, key.Prepend(temp));
}

void
StorageReflectSession ::
NodeIndexChanged(DataNode & modifiedNode, char op, uint32 index, const String & key)
{
   TCHECKPOINT;

   if (GetSubscriptionsEnabled())
   {
      if (_nextIndexSubscriptionMessage() == NULL) _nextIndexSubscriptionMessage = GetMessageFromPool(PR_RESULT_INDEXUPDATED);

      String np;
      if ((_nextIndexSubscriptionMessage())&&(modifiedNode.GetNodePath(np).IsOK()))
      {
         _sharedData->_subsDirty = true;
         (void) UpdateSubscriptionIndexMessage(*_nextIndexSubscriptionMessage(), np, op, index, key);
      }
      else MWARN_OUT_OF_MEMORY;

      // don't push subscription messages here.... it will be done elsewhere
   }
}

status_t
StorageReflectSession ::
SetDataNode(const String & nodePath, const MessageRef & dataMsgRef, SetDataNodeFlags flags, const String *optInsertBefore)
{
   TCHECKPOINT;

   DataNode * node = _sessionDir();
   if (node == NULL) return B_BAD_OBJECT;
 
   if ((nodePath.HasChars())&&(nodePath[0] != '/'))
   {
      int32 prevSlashPos = -1;
      int32 slashPos = 0;
      DataNodeRef childNodeRef;
      String nextClause;
      while(slashPos >= 0)
      {
         slashPos = nodePath.IndexOf('/', prevSlashPos+1);
         nextClause = nodePath.Substring(prevSlashPos+1, (slashPos >= 0) ? slashPos : (int32)nodePath.Length());
         DataNodeRef allocedNode;
         if (node->GetChild(nextClause, childNodeRef).IsError())
         {
            if ((_currentNodeCount >= _maxNodeCount)||(flags.IsBitSet(SETDATANODE_FLAG_DONTCREATENODE))) return B_ACCESS_DENIED;

            allocedNode = GetNewDataNode(nextClause, ((slashPos < 0)&&(flags.IsBitSet(SETDATANODE_FLAG_ADDTOINDEX) == false)) ? dataMsgRef : CastAwayConstFromRef(GetEmptyMessageRef()));
            if (allocedNode())
            {
               childNodeRef = allocedNode;
               if ((slashPos < 0)&&(flags.IsBitSet(SETDATANODE_FLAG_ADDTOINDEX)))
               {
                  if (node->InsertOrderedChild(dataMsgRef, optInsertBefore, (nextClause.HasChars())?&nextClause:NULL, this, flags.IsBitSet(SETDATANODE_FLAG_QUIET)?NULL:this, NULL).IsOK())
                  {
                     _currentNodeCount++;
                     _indexingPresent = true;
                  }
               }
               else if (node->PutChild(childNodeRef, this, ((flags.IsBitSet(SETDATANODE_FLAG_QUIET))||(slashPos < 0)) ? NULL : this).IsOK()) _currentNodeCount++;
            }
            else MRETURN_OUT_OF_MEMORY;
         }

         node = childNodeRef();
         if ((slashPos < 0)&&(flags.IsBitSet(SETDATANODE_FLAG_ADDTOINDEX) == false))
         {
            if ((node == NULL)||(flags.IsBitSet(SETDATANODE_FLAG_DONTOVERWRITEDATA)&&(node != allocedNode()))) return B_ACCESS_DENIED;
            DataNode::SetDataFlags setDataFlags;
            if (node == allocedNode()) setDataFlags.SetBit(DataNode::SET_DATA_FLAG_ISBEINGCREATED);
            if (flags.IsBitSet(SETDATANODE_FLAG_ENABLESUPERCEDE)) setDataFlags.SetBit(DataNode::SET_DATA_FLAG_ENABLESUPERCEDE);
            node->SetData(dataMsgRef, flags.IsBitSet(SETDATANODE_FLAG_QUIET) ? NULL : this, setDataFlags);  // do this to trigger the changed-notification
         }
         prevSlashPos = slashPos;
      }
   }

   return B_NO_ERROR;
}

bool
StorageReflectSession ::
HasPrivilege(int priv) const
{
   return ((_parameters.GetInt32(PR_NAME_PRIVILEGE_BITS) & (1<<priv)) != 0);
}

void
StorageReflectSession ::
AfterMessageReceivedFromGateway(const MessageRef & msgRef, void * userData)
{
   AbstractReflectSession::AfterMessageReceivedFromGateway(msgRef, userData);
   PushSubscriptionMessages();
}

class GetSubtreesCallbackArgs
{
public:
   GetSubtreesCallbackArgs(Message * replyMsg, int32 maxDepth)
      : _replyMsg(replyMsg)
      , _maxDepth(maxDepth) 
   {
      // empty
   }

   Message * GetReplyMessage() const {return _replyMsg;}
   int32 GetMaxDepth() const {return _maxDepth;}

private:
   Message * _replyMsg;
   int32 _maxDepth;
};

void 
StorageReflectSession :: 
MessageReceivedFromGateway(const MessageRef & msgRef, void * userData)
{
   TCHECKPOINT;

   Message * msgp = msgRef();
   if (msgp == NULL) return;

   Message & msg = *msgp;
   if (muscleInRange(msg.what, (uint32)BEGIN_PR_COMMANDS, (uint32)END_PR_COMMANDS))
   {
      switch(msg.what)
      {
         case PR_COMMAND_JETTISONDATATREES:
         {
            if (msg.HasName(PR_NAME_TREE_REQUEST_ID, B_STRING_TYPE)) 
            {
               const String * str;
               for (int32 i=0; msg.FindString(PR_NAME_TREE_REQUEST_ID, i, &str).IsOK(); i++) JettisonOutgoingSubtrees(str);
            }
            else JettisonOutgoingSubtrees(NULL);
         }
         break;

         case PR_COMMAND_SETDATATREES:
            BounceMessage(PR_RESULT_ERRORUNIMPLEMENTED, msgRef);    // not implemented, for now
         break;

         case PR_COMMAND_GETDATATREES:
         {
            const String * id = NULL; (void) msg.FindString(PR_NAME_TREE_REQUEST_ID, &id);
            MessageRef reply = GetMessageFromPool(PR_RESULT_DATATREES);
            if ((reply())&&((id==NULL)||(reply()->AddString(PR_NAME_TREE_REQUEST_ID, *id).IsOK())))
            {
               if (msg.HasName(PR_NAME_KEYS, B_STRING_TYPE)) 
               {
                  int32 maxDepth = -1;  (void) msg.FindInt32(PR_NAME_MAXDEPTH, maxDepth);

                  NodePathMatcher matcher;
                  matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, DEFAULT_PATH_PREFIX);

                  GetSubtreesCallbackArgs args(reply(), maxDepth);
                  (void) matcher.DoTraversal((PathMatchCallback)GetSubtreesCallbackFunc, this, GetGlobalRoot(), true, &args);
               }
               MessageReceivedFromSession(*this, reply, NULL);  // send the result back to our client
            }
         }
         break;

         case PR_COMMAND_NOOP:
            // do nothing!
         break;

         case PR_COMMAND_BATCH:
         {
            MessageRef subRef;
            for (int i=0; msg.FindMessage(PR_NAME_KEYS, i, subRef).IsOK(); i++) CallMessageReceivedFromGateway(subRef, userData);
         }
         break;

         case PR_COMMAND_KICK:
         {
            if (HasPrivilege(PR_PRIVILEGE_KICK))
            { 
               if (msg.HasName(PR_NAME_KEYS, B_STRING_TYPE)) 
               {
                  NodePathMatcher matcher;
                  (void) matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, DEFAULT_PATH_PREFIX);
                  (void) matcher.DoTraversal((PathMatchCallback)KickClientCallbackFunc, this, GetGlobalRoot(), true, NULL);
               }
            }
            else BounceMessage(PR_RESULT_ERRORACCESSDENIED, msgRef);
         }
         break;

         case PR_COMMAND_ADDBANS: case PR_COMMAND_ADDREQUIRES:
            if (HasPrivilege(PR_PRIVILEGE_ADDBANS)) 
            {
               ReflectSessionFactoryRef factoryRef = GetFactory(GetPort());
               if (factoryRef()) factoryRef()->MessageReceivedFromSession(*this, msgRef, NULL);
            }
            else BounceMessage(PR_RESULT_ERRORACCESSDENIED, msgRef);
         break;

         case PR_COMMAND_REMOVEBANS: case PR_COMMAND_REMOVEREQUIRES:
            if (HasPrivilege(PR_PRIVILEGE_REMOVEBANS))  
            {
               ReflectSessionFactoryRef factoryRef = GetFactory(GetPort());
               if (factoryRef()) factoryRef()->MessageReceivedFromSession(*this, msgRef, NULL);
            }
            else BounceMessage(PR_RESULT_ERRORACCESSDENIED, msgRef);
         break;

         case PR_COMMAND_SETPARAMETERS:
         {
            bool updateDefaultMessageRoute = false;
            const bool subscribeQuietly = msg.HasName(PR_NAME_SUBSCRIBE_QUIETLY);
            Message getMsg(PR_COMMAND_GETDATA);
            for (MessageFieldNameIterator it = msg.GetFieldNameIterator(); it.HasData(); it++)
            {
               const String & fn = it.GetFieldName();
               bool copyField = true;
               if (strncmp(fn(), "SUBSCRIBE:", 10) == 0)
               {
                  ConstQueryFilterRef filter;
                  MessageRef filterMsgRef;
                  if (msg.FindMessage(fn, filterMsgRef).IsOK()) filter = GetGlobalQueryFilterFactory()()->CreateQueryFilter(*filterMsgRef());
                  
                  const String path = fn.Substring(10);
                  String fixPath(path);
                  _subscriptions.AdjustStringPrefix(fixPath, DEFAULT_PATH_PREFIX);
                  const PathMatcherEntry * e = _subscriptions.GetEntries().Get(fixPath);
                  if (e)
                  {
                     const QueryFilter * subscriptionFilter = e->GetFilter()();
                     if ((GetSubscriptionsEnabled())&&((filter() != NULL)||(subscriptionFilter != NULL)))
                     {
                        // If the filter is different, then we need to change our subscribed-set to
                        // report the addition of nodes that match the new filter but not the old, and 
                        // report the removal of the nodes that match the old filter but not the new.  Whew!
                        NodePathMatcher temp;
                        if (temp.PutPathString(fixPath, ConstQueryFilterRef()).IsOK())
                        {
                           void * args[] = {(void *)subscriptionFilter, (void *)filter()};
                           (void) temp.DoTraversal((PathMatchCallback)ChangeQueryFilterCallbackFunc, this, GetGlobalRoot(), false, args);
                        }
                     }

                     // And now, set e's filter to the new filter.
                     (void) _subscriptions.SetFilterForEntry(fixPath, filter);  // FogBugz #5803
                  }
                  else                  
                  {
                     // This marks any currently existing matching nodes so they know to notify us
                     // It must be done once per subscription path, as it uses per-sub ref-counting
                     NodePathMatcher temp;
                     if ((temp.PutPathString(fixPath, ConstQueryFilterRef()).IsOK())&&(_subscriptions.PutPathString(fixPath, filter).IsOK())) 
                     {
                        SubscribeRefCallbackArgs srcArgs(+1);   // add one subscription-reference to each matching node
                        (void) temp.DoTraversal((PathMatchCallback)DoSubscribeRefCallbackFunc, this, GetGlobalRoot(), false, &srcArgs);
                     }
                  }
                  if ((subscribeQuietly == false)&&(getMsg.AddString(PR_NAME_KEYS, path).IsOK()))
                  {
                     // We have to have a filter message to match each string, to prevent "bleed-down" of earlier
                     // filters matching later strings.  So add a dummy filter Message if we don't have an actual one.
                     if (filterMsgRef() == NULL) filterMsgRef.SetRef(const_cast<Message *>(&GetEmptyMessage()), false);
                     (void) getMsg.AddMessage(PR_NAME_FILTERS, filterMsgRef);
                  }
               }
               else if (fn == PR_NAME_REFLECT_TO_SELF)            SetRoutingFlag(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF,      true);
               else if (fn == PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS) SetRoutingFlag(MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS, true);
               else if (fn == PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY) SetRoutingFlag(MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY, true);
               else if (fn == PR_NAME_DISABLE_SUBSCRIPTIONS)
               {
                  SetSubscriptionsEnabled(false);
               }
               else if ((fn == PR_NAME_KEYS)||(fn == PR_NAME_FILTERS))
               {
                  msg.MoveName(fn, _defaultMessageRouteMessage);
                  updateDefaultMessageRoute = true;
               }
               else if (fn == PR_NAME_SUBSCRIBE_QUIETLY)
               {
                  // don't add this to the parameter set; it's just an "argument" for the SUBSCRIBE: fields.
                  copyField = false;
               }
               else if (fn == PR_NAME_MAX_UPDATE_MESSAGE_ITEMS)
               {
                  (void) msg.FindInt32(PR_NAME_MAX_UPDATE_MESSAGE_ITEMS, _maxSubscriptionMessageItems);
               }
               else if (fn == PR_NAME_PRIVILEGE_BITS)
               {
                  // don't add this to the parameter set; clients aren't allowed to change
                  // their privilege bits (maybe someday, for some clients, but not now)
                  copyField = false;
               }
               else if (fn == PR_NAME_REPLY_ENCODING)
               {
                  int32 enc;
                  if (msg.FindInt32(PR_NAME_REPLY_ENCODING, enc).IsError()) enc = MUSCLE_MESSAGE_ENCODING_DEFAULT;
                  MessageIOGateway * gw = dynamic_cast<MessageIOGateway *>(GetGateway()());
                  if (gw) gw->SetOutgoingEncoding(enc);
               }

               if (copyField) msg.CopyName(fn, _parameters);
            }
            if (updateDefaultMessageRoute) UpdateDefaultMessageRoute();
            if (getMsg.HasName(PR_NAME_KEYS)) DoGetData(getMsg);  // return any data that matches the subscription
         }
         break;

         case PR_COMMAND_GETPARAMETERS:
         {
            MessageRef resultMessage = GetEffectiveParameters();
            if (resultMessage()) MessageReceivedFromSession(*this, resultMessage, NULL);
         }
         break;

         case PR_COMMAND_REMOVEPARAMETERS:
         {
            bool updateDefaultMessageRoute = false;
            const String * nextName;
            for (int i=0; msg.FindString(PR_NAME_KEYS, i, &nextName).IsOK(); i++) 
            {
               // Search the parameters message for all parameters that match (nextName)...
               StringMatcher matcher;
               if (matcher.SetPattern(*nextName).IsOK())
               {
                  if (matcher.IsPatternUnique()) (void) RemoveParameter(RemoveEscapeChars(*nextName), updateDefaultMessageRoute);  // FogBugz #10055
                  else
                  {
                     for (MessageFieldNameIterator it = _parameters.GetFieldNameIterator(); it.HasData(); it++) if (matcher.Match(it.GetFieldName()())) (void) RemoveParameter(it.GetFieldName(), updateDefaultMessageRoute);
                  }
               }
            }
            if (updateDefaultMessageRoute) UpdateDefaultMessageRoute();
         }
         break;

         case PR_COMMAND_SETDATA:
         {
            SetDataNodeFlags flags;
            if (msg.FindFlat<SetDataNodeFlags>(PR_NAME_FLAGS, flags).IsError())
            {
               uint32 cStyleFlags = 0;
                    if (msg.FindInt32(PR_NAME_FLAGS, cStyleFlags).IsOK()) flags.SetWord(0, cStyleFlags);  // Since C-based clients might find it difficult to flatten a BitChord
               else if (msg.HasName(PR_NAME_SET_QUIETLY))                 flags.SetBit(SETDATANODE_FLAG_QUIET);
            }

            for (MessageFieldNameIterator it = msg.GetFieldNameIterator(B_MESSAGE_TYPE); it.HasData(); it++)
            {
               MessageRef dataMsgRef;
               for (int32 i=0; msg.FindMessage(it.GetFieldName(), i, dataMsgRef).IsOK(); i++) SetDataNode(it.GetFieldName(), dataMsgRef, flags);
            }
         }
         break;

         case PR_COMMAND_INSERTORDEREDDATA:
            (void) InsertOrderedData(msgRef, NULL);
         break;

         case PR_COMMAND_REORDERDATA:
         {
            // Because REORDERDATA operates solely on pre-existing nodes, we can allow wildcards in our node paths.
            if (_sessionDir())
            {
               // Do each field as a separate operation (so they won't mess each other up)
               for (MessageFieldNameIterator iter = msg.GetFieldNameIterator(B_STRING_TYPE); iter.HasData(); iter++)
               {
                  const String * value;
                  if (msg.FindString(iter.GetFieldName(), &value).IsOK())
                  {
                     Message temp;
                     temp.AddString(PR_NAME_KEYS, iter.GetFieldName());

                     NodePathMatcher matcher;
                     (void) matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, temp, NULL);
                     (void) matcher.DoTraversal((PathMatchCallback)ReorderDataCallbackFunc, this, *_sessionDir(), true, (void *)value);
                  }
               }
            }
         }
         break;

         case PR_COMMAND_GETDATA:
            DoGetData(msg);
         break;

         case PR_COMMAND_REMOVEDATA:
         {
            NodePathMatcher matcher;
            matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, NULL);
            DoRemoveData(matcher, msg.HasName(PR_NAME_REMOVE_QUIETLY));
         }
         break;

         case PR_RESULT_PARAMETERS:
            // fall-thru
         case PR_RESULT_DATAITEMS:
            LogTime(MUSCLE_LOG_WARNING, "Warning, client at [%s] sent me a PR_RESULT_* code.  Bad client!\n", GetHostName()());
         break;

         case PR_COMMAND_JETTISONRESULTS:
         {
            if (msg.HasName(PR_NAME_KEYS, B_STRING_TYPE))
            {
               NodePathMatcher matcher;
               matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, DEFAULT_PATH_PREFIX);
               JettisonOutgoingResults(&matcher);
            }
            else JettisonOutgoingResults(NULL);
         }
         break;

         case PR_COMMAND_PING:
         {
            msg.what = PR_RESULT_PONG;                        // mark it as processed...
            MessageReceivedFromSession(*this, msgRef, NULL);  // and send it right back to our client
         }
         break;

         default:
            BounceMessage(PR_RESULT_ERRORUNIMPLEMENTED, msgRef);
         break;
      }
   }
   else
   {
      // New for v1.85; if the message has a PR_NAME_SESSION field in it, make sure it's the correct one!
      // This is to foil certain people (olorin ;^)) who would otherwise be spoofing messages from other people.
      (void) msg.ReplaceString(false, PR_NAME_SESSION, GetSessionIDString());

      // what code not in our reserved range:  must be a client-to-client message
      if (msg.HasName(PR_NAME_KEYS, B_STRING_TYPE)) 
      {
         NodePathMatcher matcher;
         (void) matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, DEFAULT_PATH_PREFIX);
         (void) matcher.DoTraversal((PathMatchCallback)PassMessageCallbackFunc, this, GetGlobalRoot(), true, const_cast<MessageRef *>(&msgRef));
      }
      else if (_parameters.HasName(PR_NAME_KEYS, B_STRING_TYPE)) 
      {
         (void) _defaultMessageRoute.DoTraversal((PathMatchCallback)PassMessageCallbackFunc, this, GetGlobalRoot(), true, const_cast<MessageRef *>(&msgRef));
      }
      else DumbReflectSession::MessageReceivedFromGateway(msgRef, userData);
   }

   TCHECKPOINT;
}

MessageRef StorageReflectSession :: GetEffectiveParameters() const
{
   String np;
   MessageRef resultMessage = GetMessageFromPool(_parameters);
   if ((resultMessage())&&(_sessionDir())&&(_sessionDir()->GetNodePath(np).IsOK()))
   {
      // Add hard-coded params 

      (void) resultMessage()->RemoveName(PR_NAME_REFLECT_TO_SELF);
      if (IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF)) (void) resultMessage()->AddBool(PR_NAME_REFLECT_TO_SELF, true);

      (void) resultMessage()->RemoveName(PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS);
      if (IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS)) (void) resultMessage()->AddBool(PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS, true);

      (void) resultMessage()->RemoveName(PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY);
      if (IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY)) (void) resultMessage()->AddBool(PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY, true);

      (void) resultMessage()->RemoveName(PR_NAME_SESSION_ROOT);
      (void) resultMessage()->AddString(PR_NAME_SESSION_ROOT, np);

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_VERSION);
      (void) resultMessage()->AddString(PR_NAME_SERVER_VERSION, MUSCLE_VERSION_STRING);

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_MEM_AVAILABLE);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_MEM_AVAILABLE, GetNumAvailableBytes());

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_MEM_USED);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_MEM_USED, GetNumUsedBytes());

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_MEM_MAX);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_MEM_MAX, GetMaxNumBytes());

      const uint64 now = GetRunTime64();

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_UPTIME);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_UPTIME, now-GetServerStartTime());

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_CURRENTTIMEUTC);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_CURRENTTIMEUTC, GetCurrentTime64(MUSCLE_TIMEZONE_UTC));

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_CURRENTTIMELOCAL);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_CURRENTTIMELOCAL, GetCurrentTime64(MUSCLE_TIMEZONE_LOCAL));

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_RUNTIME);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_RUNTIME, now);

      (void) resultMessage()->RemoveName(PR_NAME_MAX_NODES_PER_SESSION);
      (void) resultMessage()->AddInt64(PR_NAME_MAX_NODES_PER_SESSION, _maxNodeCount);

      (void) resultMessage()->RemoveName(PR_NAME_SERVER_SESSION_ID);
      (void) resultMessage()->AddInt64(PR_NAME_SERVER_SESSION_ID, GetServerSessionID());

      AddApplicationSpecificParametersToParametersResultMessage(*resultMessage());

      return resultMessage;
   }
   else return MessageRef();
}

void StorageReflectSession :: UpdateDefaultMessageRoute()
{
   _defaultMessageRoute.Clear();
   _defaultMessageRoute.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, _defaultMessageRouteMessage, DEFAULT_PATH_PREFIX);
}

/** A little bitty class just to hold the find-sessions-traversal's results properly */
class FindMatchingSessionsData
{
public:
   FindMatchingSessionsData(Hashtable<const String *, AbstractReflectSessionRef> & results, uint32 maxResults)
      : _results(results)
      , _ret(B_NO_ERROR)
      , _maxResults(maxResults)
   {
      // empty
   }

   Hashtable<const String *, AbstractReflectSessionRef> & _results;
   status_t _ret;
   uint32 _maxResults;
};

AbstractReflectSessionRef StorageReflectSession :: FindMatchingSession(const String & nodePath, const ConstQueryFilterRef & filter, bool matchSelf) const
{
   AbstractReflectSessionRef ret;
   Hashtable<const String *, AbstractReflectSessionRef> results;
   if ((FindMatchingSessions(nodePath, filter, results, matchSelf, 1).IsOK())&&(results.HasItems())) ret = results.GetFirstValueWithDefault();
   return ret;
}

status_t StorageReflectSession :: FindMatchingSessions(const String & nodePath, const ConstQueryFilterRef & filter, Hashtable<const String *, AbstractReflectSessionRef> & retSessions, bool includeSelf, uint32 maxResults) const
{
   TCHECKPOINT;

   status_t ret;

   if (nodePath.HasChars())
   {
      const char * s;
      String temp;
      if (nodePath.StartsWith('/')) s = nodePath()+1;
      else
      {
         temp = nodePath.Prepend(DEFAULT_PATH_PREFIX "/");
         s = temp();
      }

      NodePathMatcher matcher;
      if (matcher.PutPathString(s, filter).IsOK(ret))
      {
         FindMatchingSessionsData data(retSessions, maxResults);
         (void) matcher.DoTraversal((PathMatchCallback)FindSessionsCallbackFunc, const_cast<StorageReflectSession*>(this), GetGlobalRoot(), true, &data);
         ret = data._ret;
      }
   }
   else return retSessions.Put(GetSessions());

   if (includeSelf == false) (void) retSessions.Remove(&GetSessionIDString());
   return ret;
}

status_t StorageReflectSession :: SendMessageToMatchingSessions(const MessageRef & msgRef, const String & nodePath, const ConstQueryFilterRef & filter, bool includeSelf)
{
   TCHECKPOINT;

   if (nodePath.HasChars())
   {
      const char * s;
      String temp;
      if (nodePath.StartsWith("/")) s = nodePath()+1;
      else
      {
         temp = nodePath.Prepend(DEFAULT_PATH_PREFIX "/");
         s = temp();
      }

      status_t ret;
      NodePathMatcher matcher;
      if (matcher.PutPathString(s, filter).IsOK(ret))
      {
         void * sendMessageData[] = {const_cast<MessageRef *>(&msgRef), &includeSelf}; // gotta include the includeSelf param too, alas
         (void) matcher.DoTraversal((PathMatchCallback)SendMessageCallbackFunc, this, GetGlobalRoot(), true, sendMessageData);
         return B_NO_ERROR;
      }
      return ret;
   }
   else
   {
      BroadcastToAllSessions(msgRef, NULL, includeSelf);
      return B_NO_ERROR;
   }
}

/** A little bitty class just to hold the find-nodes-traversal's results properly */
class FindMatchingNodesData
{
public:
   FindMatchingNodesData(Queue<DataNodeRef> & results, uint32 maxResults)
      : _results(results)
      , _ret(B_NO_ERROR)
      , _maxResults(maxResults)
   {
      // empty
   }

   Queue<DataNodeRef> & _results;
   status_t _ret;
   uint32 _maxResults;
};

int StorageReflectSession :: FindNodesCallback(DataNode & node, void * userData)
{
   FindMatchingNodesData * data = static_cast<FindMatchingNodesData *>(userData);
   if (data->_results.AddTail(DataNodeRef(&node)).IsError(data->_ret)) 
   {
      return -1;  // abort now
   }
   else return (data->_results.GetNumItems() == data->_maxResults) ? -1 : (int)node.GetDepth();  // continue traversal as usual unless, we have reached our limit
}

DataNodeRef StorageReflectSession :: FindMatchingNode(const String & nodePath, const ConstQueryFilterRef & filter) const
{
   Queue<DataNodeRef> results;
   return ((FindMatchingNodes(nodePath, filter, results, 1).IsOK())&&(results.HasItems())) ? results.Head() : DataNodeRef();
}

status_t StorageReflectSession :: FindMatchingNodes(const String & nodePath, const ConstQueryFilterRef & filter, Queue<DataNodeRef> & retNodes, uint32 maxResults) const
{
   status_t ret;

   const bool isGlobal = nodePath.StartsWith('/');
   NodePathMatcher matcher;
   if (matcher.PutPathString(isGlobal?nodePath.Substring(1):nodePath, filter).IsOK(ret))
   {
      FindMatchingNodesData data(retNodes, maxResults);
      (void) matcher.DoTraversal((PathMatchCallback)FindNodesCallbackFunc, const_cast<StorageReflectSession*>(this), isGlobal?GetGlobalRoot():*_sessionDir(), true, &data);
      ret = data._ret;
   }

   return ret;
}

int
StorageReflectSession ::
SendMessageCallback(DataNode & node, void * userData)
{
   void ** a = (void **) userData;
   return PassMessageCallbackAux(node, *((MessageRef *)a[0]), *((bool *)a[1]));
}

status_t StorageReflectSession :: InsertOrderedData(const MessageRef & msgRef, Hashtable<String, DataNodeRef> * optNewNodes)
{
   TCHECKPOINT;

   if (_sessionDir() == NULL) return B_BAD_OBJECT;
   if (msgRef())
   {
      // Because INSERTORDEREDDATA operates solely on pre-existing nodes, we can allow wildcards in our node paths.
      void * args[2] = {msgRef(), optNewNodes};
      NodePathMatcher matcher;
      (void) matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, *msgRef(), NULL);
      (void) matcher.DoTraversal((PathMatchCallback)InsertOrderedDataCallbackFunc, this, *_sessionDir(), true, args);
      return B_NO_ERROR;
   }
   else return B_BAD_ARGUMENT;
}

void
StorageReflectSession :: 
BounceMessage(uint32 errorCode, const MessageRef & msgRef)
{
   // Unknown code; bounce it back to our client
   MessageRef bounce = GetMessageFromPool(errorCode);
   if (bounce())
   {
      bounce()->AddMessage(PR_NAME_REJECTED_MESSAGE, msgRef);
      MessageReceivedFromSession(*this, bounce, NULL);  // send rejection notice to client
   }
}

void
StorageReflectSession :: 
DoGetData(const Message & msg)
{
   TCHECKPOINT;

   NodePathMatcher matcher;
   matcher.PutPathsFromMessage(PR_NAME_KEYS, PR_NAME_FILTERS, msg, DEFAULT_PATH_PREFIX);

   MessageRef messageArray[2];  // first is the DATAITEMS message, second is the INDEXUPDATED message (both demand-allocated)
   (void) matcher.DoTraversal((PathMatchCallback)GetDataCallbackFunc, this, GetGlobalRoot(), true, messageArray);

   // Send any still-pending "get" results...
   SendGetDataResults(messageArray[0]);
   SendGetDataResults(messageArray[1]);
}

void
StorageReflectSession ::
SendGetDataResults(MessageRef & replyMessage)
{
   TCHECKPOINT;

   if (replyMessage())
   {
      MessageReceivedFromSession(*this, replyMessage, NULL);
      replyMessage.Reset();  // force re-alloc later if need be
   }
}

void StorageReflectSession :: DoRemoveData(NodePathMatcher & matcher, bool quiet)
{
   TCHECKPOINT;

   if (_sessionDir())
   {
      Queue<DataNodeRef> removeSet;
      (void) matcher.DoTraversal((PathMatchCallback)RemoveDataCallbackFunc, this, *_sessionDir(), true, &removeSet);
      for (int i=removeSet.GetNumItems()-1; i>=0; i--)
      {
         DataNode * next = removeSet[i]();
         if (next)
         {
            DataNode * parent = next->GetParent();
            if (parent) (void) parent->RemoveChild(next->GetNodeName(), quiet ? NULL : this, true, &_currentNodeCount);
         }
      }
   }
}

status_t StorageReflectSession :: RemoveDataNodes(const String & nodePath, const ConstQueryFilterRef & filterRef, bool quiet)
{
   TCHECKPOINT;

   NodePathMatcher matcher;
   MRETURN_ON_ERROR(matcher.PutPathString(nodePath, filterRef));

   DoRemoveData(matcher, quiet);
   return B_NO_ERROR;
}

status_t StorageReflectSession :: MoveIndexEntries(const String & nodePath, const String * optBefore, const ConstQueryFilterRef & filterRef)
{
   TCHECKPOINT;

   NodePathMatcher matcher;
   MRETURN_ON_ERROR(matcher.PutPathString(nodePath, filterRef));

   (void) matcher.DoTraversal((PathMatchCallback)ReorderDataCallbackFunc, this, *_sessionDir(), true, (void *)optBefore);
   return B_NO_ERROR;
}

void
StorageReflectSession ::
PushSubscriptionMessages()
{
   TCHECKPOINT;

   if (_sharedData->_subsDirty)
   {
      _sharedData->_subsDirty = false;

      // Send out any subscription results that were generated...
      for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
      {
         StorageReflectSession * nextSession = dynamic_cast<StorageReflectSession *>(iter.GetValue()());
         if (nextSession)
         {
            nextSession->PushSubscriptionMessage(nextSession->_nextSubscriptionMessage);
            nextSession->PushSubscriptionMessage(nextSession->_nextIndexSubscriptionMessage);
         }
      }
      PushSubscriptionMessages();  // in case these generated even more messages...
   }
}

void
StorageReflectSession ::
PushSubscriptionMessage(MessageRef & ref)
{
   TCHECKPOINT;

   MessageRef oldRef = ref;  // (ref) is one of our _next*SubscriptionMessage members
   while(oldRef()) 
   {
      ref.Reset();  /* In case FromSession() wants to add more suscriptions... */
      MessageReceivedFromSession(*this, oldRef, NULL);
      oldRef = ref;
   }
}

int
StorageReflectSession ::
PassMessageCallback(DataNode & node, void * userData)
{
   return PassMessageCallbackAux(node, *((MessageRef *)userData), IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF));
}

int
StorageReflectSession ::
PassMessageCallbackAux(DataNode & node, const MessageRef & msgRef, bool includeSelfOkay)
{
   TCHECKPOINT;

   StorageReflectSession * next = dynamic_cast<StorageReflectSession *>(GetSession(node.GetAncestorNode(NODE_DEPTH_SESSIONNAME, &node)->GetNodeName())());
   if ((next)&&((next != this)||(includeSelfOkay))) next->MessageReceivedFromSession(*this, msgRef, &node);
   return NODE_DEPTH_SESSIONNAME; // This causes the traversal to immediately skip to the next session
}

int
StorageReflectSession ::
FindSessionsCallback(DataNode & node, void * userData)
{
   TCHECKPOINT;

   AbstractReflectSessionRef sref = GetSession(node.GetAncestorNode(NODE_DEPTH_SESSIONNAME, &node)->GetNodeName());

   StorageReflectSession * next = dynamic_cast<StorageReflectSession *>(sref());
   FindMatchingSessionsData * data = static_cast<FindMatchingSessionsData *>(userData);
   if ((next)&&(data->_results.Put(&next->GetSessionIDString(), sref).IsError(data->_ret)))
   {
      return -1;  // abort now
   }
   else return (data->_results.GetNumItems() == data->_maxResults) ? -1 : NODE_DEPTH_SESSIONNAME; // This causes the traversal to immediately skip to the next session
}

int
StorageReflectSession ::
KickClientCallback(DataNode & node, void * /*userData*/)
{
   TCHECKPOINT;

   AbstractReflectSessionRef sref = GetSession(node.GetAncestorNode(NODE_DEPTH_SESSIONNAME, &node)->GetNodeName());
   StorageReflectSession * next = dynamic_cast<StorageReflectSession *>(sref());
   if ((next)&&(next != this)) 
   {
      LogTime(MUSCLE_LOG_DEBUG, "Session [%s/%s] is kicking session [%s/%s] off the server.\n", GetHostName()(), GetSessionIDString()(), next->GetHostName()(), next->GetSessionIDString()());
      next->EndSession();  // die!!
   }
   return NODE_DEPTH_SESSIONNAME; // This causes the traversal to immediately skip to the next session
}

int
StorageReflectSession ::
GetSubtreesCallback(DataNode & node, void * ud)
{
   TCHECKPOINT;

   GetSubtreesCallbackArgs & args = *(static_cast<GetSubtreesCallbackArgs *>(ud));

   // Make sure (node) isn't part of our own tree!  If it is, move immediately to the next session
   if ((_indexingPresent == false)&&(IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF) == false)&&(GetSession(node.GetAncestorNode(NODE_DEPTH_SESSIONNAME, &node)->GetNodeName())() == this)) return NODE_DEPTH_SESSIONNAME;

   MessageRef subMsg = GetMessageFromPool();
   String nodePath;
   return ((subMsg() == NULL)||(node.GetNodePath(nodePath).IsError())||(args.GetReplyMessage()->AddMessage(nodePath, subMsg).IsError())||(SaveNodeTreeToMessage(*subMsg(), &node, "", true, (args.GetMaxDepth()>=0)?(uint32)args.GetMaxDepth():MUSCLE_NO_LIMIT, NULL).IsError())) ? 0 : node.GetDepth();
}

int
StorageReflectSession ::
ChangeQueryFilterCallback(DataNode & node, void * ud)
{
   void ** args = (void **) ud;
   const QueryFilter * oldFilter = static_cast<const QueryFilter *>(args[0]);
   const QueryFilter * newFilter = static_cast<const QueryFilter *>(args[1]);
   ConstMessageRef constMsg1 = node.GetData();
   ConstMessageRef constMsg2 = node.GetData();
   const bool oldMatches = ((constMsg1() == NULL)||(oldFilter == NULL)||(oldFilter->Matches(constMsg1, &node)));
   const bool newMatches = ((constMsg2() == NULL)||(newFilter == NULL)||(newFilter->Matches(constMsg2, &node)));
   if (oldMatches != newMatches) NodeChangedAux(node, CastAwayConstFromRef(constMsg2), oldMatches?NodeChangeFlags(NODE_CHANGE_FLAG_ISBEINGREMOVED):NodeChangeFlags());
   return node.GetDepth();  // continue traversal as usual
}

int
StorageReflectSession ::
DoSubscribeRefCallback(DataNode & node, void * userData)
{
   const SubscribeRefCallbackArgs * srcArgs = static_cast<const SubscribeRefCallbackArgs *>(userData);
   node._subscribers = GetDataNodeSubscribersTableFromPool(node._subscribers, GetSessionIDString(), srcArgs->GetRefCountDelta());
   return node.GetDepth();  // continue traversal as usual
}

int
StorageReflectSession ::
GetDataCallback(DataNode & node, void * userData)
{
   TCHECKPOINT;

   MessageRef * messageArray = (MessageRef *) userData;

   // Make sure (node) isn't part of our own tree!  If it is, move immediately to the next session
   if ((_indexingPresent == false)&&(IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF) == false)&&(GetSession(node.GetAncestorNode(NODE_DEPTH_SESSIONNAME, &node)->GetNodeName())() == this)) return NODE_DEPTH_SESSIONNAME;
 
   // Don't send our own data to our own client; he already knows what we have, because he uploaded it!
   MessageRef & resultMsg = messageArray[0];
   if (resultMsg() == NULL) resultMsg = GetMessageFromPool(PR_RESULT_DATAITEMS);
   String np1;
   if ((resultMsg())&&(node.GetNodePath(np1).IsOK()))
   {
      (void) resultMsg()->AddMessage(np1, node.GetData());
      if (resultMsg()->GetNumNames() >= _maxSubscriptionMessageItems) SendGetDataResults(resultMsg);
   }
   else
   {      
      MWARN_OUT_OF_MEMORY;
      return 0;  // abort!
   }

   // But indices we need to send to ourself no matter what, as they are generated on the server side.
   const Queue<DataNodeRef> * index = node.GetIndex();
   if (index)
   {
      const uint32 indexLen = index->GetNumItems();
      if (indexLen > 0)
      {
         MessageRef & indexUpdateMsg = messageArray[1];
         if (indexUpdateMsg() == NULL) indexUpdateMsg = GetMessageFromPool(PR_RESULT_INDEXUPDATED);
         String np2;
         if ((indexUpdateMsg())&&(node.GetNodePath(np2).IsOK()))
         {
            const char clearStr[] = {INDEX_OP_CLEARED, '\0'};
            (void) indexUpdateMsg()->AddString(np2, clearStr);
            for (uint32 i=0; i<indexLen; i++) 
            {
               char temp[100]; muscleSprintf(temp, "%c" UINT32_FORMAT_SPEC ":", INDEX_OP_ENTRYINSERTED, i);
               (void) indexUpdateMsg()->AddString(np2, (*index)[i]()->GetNodeName().Prepend(temp));
            }
            if (indexUpdateMsg()->GetNumNames() >= _maxSubscriptionMessageItems) SendGetDataResults(messageArray[1]);
         }
         else
         {
            MWARN_OUT_OF_MEMORY;
            return 0;  // abort!
         }
      }
   }

   return node.GetDepth();  // continue traveral as usual
}

int
StorageReflectSession ::
RemoveDataCallback(DataNode & node, void * userData)
{
   TCHECKPOINT;

   if (node.GetDepth() > NODE_DEPTH_SESSIONNAME)  // ensure that we never remove host nodes or session nodes this way
   {
      DataNodeRef nodeRef;
      if (node.GetParent()->GetChild(node.GetNodeName(), nodeRef).IsOK()) 
      {
         (void) ((Queue<DataNodeRef> *)userData)->AddTail(nodeRef);
         return node.GetDepth()-1;  // no sense in recursing down a node that we're going to delete anyway
      }
   }
   return node.GetDepth();
}

int
StorageReflectSession ::
InsertOrderedDataCallback(DataNode & node, void * userData)
{
   TCHECKPOINT;

   void ** args = (void **) userData;
   const Message * insertMsg = static_cast<const Message *>(args[0]);
   Hashtable<String, DataNodeRef> * optRetResults = (Hashtable<String, DataNodeRef> *) args[1];
   for (MessageFieldNameIterator iter = insertMsg->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
   {
      MessageRef nextRef;
      for (int i=0; (insertMsg->FindMessage(iter.GetFieldName(), i, nextRef).IsOK()); i++) (void) InsertOrderedChildNode(node, &iter.GetFieldName(), nextRef, optRetResults);
   }
   return node.GetDepth();
}

status_t
StorageReflectSession ::
InsertOrderedChildNode(DataNode & node, const String * optInsertBefore, const MessageRef & childNodeMsg, Hashtable<String, DataNodeRef> * optAddNewChildren)
{
   if (_currentNodeCount >= _maxNodeCount) return B_ACCESS_DENIED;

   status_t ret;
   if (node.InsertOrderedChild(childNodeMsg, optInsertBefore, NULL, this, this, optAddNewChildren).IsOK(ret))
   {
      _indexingPresent = true;  // disable optimization in GetDataCallback()
      _currentNodeCount++;
      return B_NO_ERROR;
   }
   else return ret;
}

int
StorageReflectSession ::
ReorderDataCallback(DataNode & node, void * userData)
{
   DataNode * indexNode = node.GetParent();
   if (indexNode)
   {
      DataNodeRef childNodeRef;
      if (indexNode->GetChild(node.GetNodeName(), childNodeRef).IsOK()) indexNode->ReorderChild(childNodeRef, static_cast<const String *>(userData), this);
   }
   return node.GetDepth();
}

bool
StorageReflectSession :: NodePathMatcher ::
MatchesNode(DataNode & node, ConstMessageRef & optData, int rootDepth) const
{
   for (HashtableIterator<String, PathMatcherEntry> iter(GetEntries()); iter.HasData(); iter++) if (PathMatches(node, optData, iter.GetValue(), rootDepth)) return true;
   return false;
}

uint32
StorageReflectSession :: NodePathMatcher ::
GetMatchCount(DataNode & node, const Message * optData, int rootDepth) const
{
   TCHECKPOINT;

   uint32 matchCount = 0;
   ConstMessageRef fakeRef(optData, false);
   for (HashtableIterator<String, PathMatcherEntry> iter(GetEntries()); iter.HasData(); iter++) if (PathMatches(node, fakeRef, iter.GetValue(), rootDepth)) matchCount++;
   return matchCount;
}

bool
StorageReflectSession :: NodePathMatcher ::
PathMatches(DataNode & node, ConstMessageRef & optData, const PathMatcherEntry & entry, int rootDepth) const
{
   TCHECKPOINT;

   const StringMatcherQueue * nextSubscription = entry.GetParser()();
   if ((int32)nextSubscription->GetStringMatchers().GetNumItems() != ((int32)node.GetDepth())-rootDepth) return false;  // only paths with the same number of clauses as the node's path (less rootDepth) can ever match

   DataNode * travNode = &node;
   for (int j=nextSubscription->GetStringMatchers().GetNumItems()-1; j>=rootDepth; j--,travNode=travNode->GetParent())
   {
      const StringMatcher * nextMatcher = nextSubscription->GetStringMatchers().GetItemAt(j)->GetItemPointer();
      if ((nextMatcher)&&(nextMatcher->Match(travNode->GetNodeName()()) == false)) return false;
   }
   return entry.FilterMatches(optData, &node);
}

uint32
StorageReflectSession :: NodePathMatcher ::
DoTraversal(PathMatchCallback cb, StorageReflectSession * This, DataNode & node, bool useFilters, void * userData)
{
   TraversalContext ctxt(cb, This, useFilters, userData, node.GetDepth());
   (void) DoTraversalAux(ctxt, node);
   return ctxt.GetVisitCount();
}

int 
StorageReflectSession :: NodePathMatcher ::
DoTraversalAux(TraversalContext & data, DataNode & node)
{
   TCHECKPOINT;

   int depth = node.GetDepth();
   bool parsersHaveWildcards = false;  // optimistic default
   {
      // If none of our parsers are using wildcarding at our current level, we can use direct hash lookups (faster)
      for (HashtableIterator<String, PathMatcherEntry> iter(GetEntries()); iter.HasData(); iter++)
      {
         const StringMatcherQueue * nextQueue = iter.GetValue().GetParser()();
         if ((nextQueue)&&((int)nextQueue->GetStringMatchers().GetNumItems() > depth-data.GetRootDepth()))
         {
            const StringMatcher * nextMatcher = nextQueue->GetStringMatchers().GetItemAt(depth-data.GetRootDepth())->GetItemPointer();
            if ((nextMatcher == NULL)||((nextMatcher->IsPatternUnique() == false)&&(nextMatcher->IsPatternListOfUniqueValues() == false)))
            {
               parsersHaveWildcards = true;  // Oops, there will be some pattern matching involved, gotta iterate
               break;
            }
         }
      }
   }

   if (parsersHaveWildcards)
   {
      // general case -- iterate over all children of our node and see if any match
      for (DataNodeRefIterator it = node.GetChildIterator(); it.HasData(); it++) if (CheckChildForTraversal(data, it.GetValue()(), -1, depth)) return depth;
   }
   else
   {
      // optimized case -- since our parsers are all node-specific, we can do a single lookup for each,
      // and avoid having to iterate over all the children of this node.
      String scratchStr;
      Hashtable<DataNode *, Void> alreadyDid;  // To make sure we don't do the same child twice (could happen if two matchers are the same)
      int32 entryIdx = 0;
      for (HashtableIterator<String, PathMatcherEntry> iter(GetEntries()); iter.HasData(); iter++)
      {
         scratchStr.Clear();  // otherwise we might get leftover data from the previous PathMatcherEntry in the iteration

         const StringMatcherQueue * nextQueue = iter.GetValue().GetParser()();
         if ((nextQueue)&&((int)nextQueue->GetStringMatchers().GetNumItems() > depth-data.GetRootDepth()))
         {
            const StringMatcher * nextMatcher = nextQueue->GetStringMatchers().GetItemAt(depth-data.GetRootDepth())->GetItemPointer();
            const String & key = nextMatcher->GetPattern();
            if (nextMatcher->IsPatternListOfUniqueValues())
            {
               // comma-separated-list-of-unique-values case
               bool prevCharWasEscape = false;
               const char * k = key();
               while(*k)
               {
                  const char c = *k;
                  const bool curCharIsEscape = ((c == '\\')&&(prevCharWasEscape == false));
                  if (curCharIsEscape == false)
                  {
                          if ((prevCharWasEscape)||(c != ',')) scratchStr += c;
                     else if (scratchStr.HasChars())
                     {
                        if (DoDirectChildLookup(data, node, scratchStr, entryIdx, alreadyDid, depth)) return depth;
                        scratchStr.Clear();
                     }
                  }
                  prevCharWasEscape = curCharIsEscape; 
                  k++;
               }
               if (scratchStr.HasChars())
               {
                  if (DoDirectChildLookup(data, node, scratchStr, entryIdx, alreadyDid, depth)) return depth;
               }
            }
            else if (DoDirectChildLookup(data, node, key, entryIdx, alreadyDid, depth)) return depth;  // single-value-lookup case (most efficient)
         }
         entryIdx++;
      }
   }

   return node.GetDepth();
}

bool
StorageReflectSession :: NodePathMatcher ::
DoDirectChildLookup(TraversalContext & data, const DataNode & node, const String & key, int32 entryIdx, Hashtable<DataNode *, Void> & alreadyDid, int & depth)
{
   // single-unique-value case
   DataNodeRef nextChildRef;
   if ((node.GetChild(RemoveEscapeChars(key), nextChildRef).IsOK())&&(alreadyDid.ContainsKey(nextChildRef()) == false))
   {
      if (CheckChildForTraversal(data, nextChildRef(), entryIdx, depth)) return true;
      (void) alreadyDid.PutWithDefault(nextChildRef());
   }
   return false;
}

bool 
StorageReflectSession :: NodePathMatcher ::
CheckChildForTraversal(TraversalContext & data, DataNode * nextChild, int32 optKnownMatchingEntryIdx, int & depth)
{
   TCHECKPOINT;

   if (nextChild)
   {
      const String & nextChildName = nextChild->GetNodeName();
      bool matched  = false;  // set if we have called the callback on this child already
      bool recursed = false;  // set if we have recursed to this child already

      // Try all parsers and see if any of them match at this level
      int32 entryIdx = 0;
      for (HashtableIterator<String, PathMatcherEntry> iter(GetEntries()); iter.HasData(); iter++)
      {
         const StringMatcherQueue * nextQueue = iter.GetValue().GetParser()();
         if (nextQueue)
         {
            const int numClausesInParser = nextQueue->GetStringMatchers().GetNumItems();
            if (numClausesInParser > depth-data.GetRootDepth())
            {
               const StringMatcher * nextMatcher = (entryIdx==optKnownMatchingEntryIdx) ? NULL : nextQueue->GetStringMatchers().GetItemAt(depth-data.GetRootDepth())->GetItemPointer();
               if ((nextMatcher == NULL)||(nextMatcher->Match(nextChildName())))
               {
                  // A match!  Now, depending on whether this match is the
                  // last clause in the path or not, we either do the callback or descend.
                  // But we make sure not to do either of these things more than once per node.
                  if (depth == data.GetRootDepth()+numClausesInParser-1)
                  {
                     if (matched == false)
                     {
                        // when there is more than one string being used to match,
                        // it's possible that two or more strings can "conspire"
                        // to match a node even though any given string doesn't match it.
                        // For example, if we have the match-strings:
                        //    /j*/k*
                        //    /k*/j*
                        // The node /jeremy/jenny would match, even though it isn't
                        // specified by any of the subscription strings.  This is bad.
                        // So for multiple match-strings, we do an additional check 
                        // to make sure there is a NodePathMatcher for this node.
                        ConstMessageRef constDataRef; 
                        if (data.IsUseFiltersOkay()) constDataRef = nextChild->GetData();
                        if (((GetEntries().GetNumItems() == 1)&&((data.IsUseFiltersOkay() == false)||(iter.GetValue().GetFilter()() == NULL)))||(MatchesNode(*nextChild, constDataRef, data.GetRootDepth())))
                        {
                           int nextDepth;
                           if ((constDataRef() == NULL)||(constDataRef() == nextChild->GetData()())) nextDepth = data.CallCallbackMethod(*nextChild);  // the usual/simple case
                           else
                           {
                              // Hey, the QueryFilter retargetted the ConstMessageRef!  So we need the callback to see the modified Message, not the original one.
                              // We'll do that the sneaky way, by temporarily swapping out (nextChild)'s MessageRef, and then swapping it back in afterwards.
                              MessageRef origNodeMsg = nextChild->GetData(); 
                              nextChild->SetData(CastAwayConstFromRef(constDataRef), NULL, DataNode::SetDataFlags());
                              nextDepth = data.CallCallbackMethod(*nextChild);
                              nextChild->SetData(origNodeMsg, NULL, DataNode::SetDataFlags());
                           }

                           if (nextDepth < ((int)nextChild->GetDepth())-1) 
                           {
                              depth = nextDepth;
                              return true;
                           }
                           matched = true;
                           if (recursed) break;  // done both possible actions, so be lazy
                        }
                     }
                  }
                  else
                  {
                     if (recursed == false)
                     {
                        // If we match a non-terminal clause in the path, recurse to the child.
                        const int nextDepth = DoTraversalAux(data, *nextChild);
                        if (nextDepth < ((int)nextChild->GetDepth())-1) 
                        {
                           depth = nextDepth;
                           return true;
                        }
                        recursed = true;
                        if (matched) break;  // done both possible actions, so be lazy
                     }
                  }
               }
            }
         }
         entryIdx++;
      }
   }
   return false;
}

DataNodeRef
StorageReflectSession ::
GetNewDataNode(const String & name, const MessageRef & initialValue)
{
   static DataNodeRef::ItemPool _nodePool;

   DataNodeRef ret(_nodePool.ObtainObject());
   if (ret()) ret()->Init(name, initialValue);
   return ret;
}

void
StorageReflectSession ::
JettisonOutgoingSubtrees(const String * optMatchString)
{
   TCHECKPOINT;

   AbstractMessageIOGateway * gw = GetGateway()();
   if (gw)
   {
      StringMatcher sm;
      if ((optMatchString == NULL)||(sm.SetPattern(*optMatchString).IsOK()))
      {
         Queue<MessageRef> & oq = gw->GetOutgoingMessageQueue();
         for (int i=oq.GetNumItems()-1; i>=0; i--)  // must do this backwards!
         {
            Message * msg = oq.GetItemAt(i)->GetItemPointer();
            if ((msg)&&(msg->what == PR_RESULT_DATATREES))
            {
               bool removeIt = false;
               const char * batchID = NULL; (void) msg->FindString(PR_NAME_TREE_REQUEST_ID, &batchID);
               if (optMatchString)
               {
                  if ((batchID)&&(sm.Match(batchID))) removeIt = true;
               }
               else if (batchID == NULL) removeIt = true;
   
               if (removeIt) oq.RemoveItemAt(i);
            }
         }
      }
   }
}

void
StorageReflectSession ::
JettisonOutgoingResults(const NodePathMatcher * matcher)
{
   TCHECKPOINT;

   AbstractMessageIOGateway * gw = GetGateway()();
   if (gw)
   {
      Queue<MessageRef> & oq = gw->GetOutgoingMessageQueue();
      for (int i=oq.GetNumItems()-1; i>=0; i--)  // must do this backwards!
      {
         Message * msg = oq.GetItemAt(i)->GetItemPointer();
         if ((msg)&&(msg->what == PR_RESULT_DATAITEMS))
         {
            if (matcher)
            {
               // Remove any PR_NAME_REMOVED_DATAITEMS entries that match... 
               int nextr = 0;
               const String * rname;
               while(msg->FindString(PR_NAME_REMOVED_DATAITEMS, nextr, &rname).IsOK())
               {
                  if (matcher->MatchesPath(rname->Cstr(), NULL, NULL)) msg->RemoveData(PR_NAME_REMOVED_DATAITEMS, nextr);
                                                                  else nextr++;
               }
   
               // Remove all matching items from the Message.  (Yes, the iterator can handle this!  :^))
               for (MessageFieldNameIterator iter = msg->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
               {
                  const String & nextFieldName = iter.GetFieldName();
                  if (matcher->GetNumFilters() > 0)
                  {
                     MessageRef nextSubMsgRef;
                     for (uint32 j=0; msg->FindMessage(nextFieldName, j, nextSubMsgRef).IsOK(); /* empty */)
                     {
                        if (matcher->MatchesPath(nextFieldName(), nextSubMsgRef(), NULL)) msg->RemoveData(nextFieldName, 0);
                                                                                     else j++;
                     }
                  }
                  else if (matcher->MatchesPath(nextFieldName(), NULL, NULL)) msg->RemoveName(nextFieldName);
               }
            }
            else msg->Clear();

            if (msg->HasNames() == false) (void) oq.RemoveItemAt(i);
         }
      }
   }
}

status_t
StorageReflectSession :: CloneDataNodeSubtree(const DataNode & node, const String & destPath, SetDataNodeFlags flags, const String * optInsertBefore, const ITraversalPruner * optPruner)
{
   TCHECKPOINT;

   {
      MessageRef payload = node.GetData();
      if ((optPruner)&&(optPruner->MatchPath(destPath, payload) == false)) return B_NO_ERROR;
      if (payload() == NULL) return B_BAD_OBJECT;
      MRETURN_ON_ERROR(SetDataNode(destPath, payload, flags, optInsertBefore));
   }

   // Then clone all of his children
   for (DataNodeRefIterator iter = node.GetChildIterator(); iter.HasData(); iter++)
   {
      // Note that we don't deal with the index-cloning here; we do it separately (below) instead, for efficiency
      SetDataNodeFlags subFlags = flags;
      subFlags.SetBit(SETDATANODE_FLAG_DONTOVERWRITEDATA);
      subFlags.ClearBit(SETDATANODE_FLAG_DONTCREATENODE);
      subFlags.ClearBit(SETDATANODE_FLAG_ADDTOINDEX);
      if (iter.GetValue()()) MRETURN_ON_ERROR(CloneDataNodeSubtree(*iter.GetValue()(), destPath+'/'+(*iter.GetKey()), subFlags, NULL, optPruner));
   }

   // Lastly, if he has an index, make sure the clone ends up with an equivalent index
   const Queue<DataNodeRef> * index = node.GetIndex();
   if (index)
   {
      DataNode * clone = GetDataNode(destPath);
      if (clone)
      {
         const uint32 idxLen = index->GetNumItems();
         for (uint32 i=0; i<idxLen; i++) MRETURN_ON_ERROR(clone->InsertIndexEntryAt(i, this, (*index)[i]()->GetNodeName()));
      }
      else return B_DATA_NOT_FOUND;
   }

   return B_NO_ERROR;
}

// Recursive helper function
status_t
StorageReflectSession :: SaveNodeTreeToMessage(Message & msg, const DataNode * node, const String & path, bool saveData, uint32 maxDepth, const ITraversalPruner * optPruner) const
{
   TCHECKPOINT;

   {
      MessageRef payload = node->GetData();
      if ((optPruner)&&(optPruner->MatchPath(path, payload) == false)) return B_NO_ERROR;
      if (saveData) MRETURN_ON_ERROR(msg.AddMessage(PR_NAME_NODEDATA, payload));
   }
   
   if ((node->HasChildren())&&(maxDepth > 0))
   {
      // Save the node-index, if there is one
      const Queue<DataNodeRef> * index = node->GetIndex();
      if (index)
      {
         const uint32 indexSize = index->GetNumItems();
         if (indexSize > 0)
         {
            MessageRef indexMsgRef(GetMessageFromPool());
            MRETURN_OOM_ON_NULL(indexMsgRef());
            MRETURN_ON_ERROR(msg.AddMessage(PR_NAME_NODEINDEX, indexMsgRef));

            Message * indexMsg = indexMsgRef();
            for (uint32 i=0; i<indexSize; i++) MRETURN_ON_ERROR(indexMsg->AddString(PR_NAME_KEYS, (*index)[i]()->GetNodeName()));
         }
      }

      // Then save the children, recursing to each one as necessary
      {
         MessageRef childrenMsgRef(GetMessageFromPool());
         MRETURN_OOM_ON_NULL(childrenMsgRef());
         MRETURN_ON_ERROR(msg.AddMessage(PR_NAME_NODECHILDREN, childrenMsgRef));
         for (DataNodeRefIterator childIter = node->GetChildIterator(); childIter.HasData(); childIter++)
         {
            DataNode * child = childIter.GetValue()();
            if (child)
            {
               String childPath(path);
               if (childPath.HasChars()) childPath += '/';
               childPath += child->GetNodeName();

               MessageRef childMsgRef(GetMessageFromPool());
               MRETURN_OOM_ON_NULL(childMsgRef());
               MRETURN_ON_ERROR(childrenMsgRef()->AddMessage(child->GetNodeName(), childMsgRef));
               MRETURN_ON_ERROR(SaveNodeTreeToMessage(*childMsgRef(), child, childPath, true, maxDepth-1, optPruner));
            }
         }
      }
   }

   return B_NO_ERROR;
}

status_t
StorageReflectSession :: RestoreNodeTreeFromMessage(const Message & msg, const String & path, bool loadData, SetDataNodeFlags flags, uint32 maxDepth, const ITraversalPruner * optPruner)
{
   TCHECKPOINT;

   if (loadData)
   {
      MessageRef payload;
      MRETURN_ON_ERROR(msg.FindMessage(PR_NAME_NODEDATA, payload));
      if ((optPruner)&&(optPruner->MatchPath(path, payload) == false)) return B_NO_ERROR;
      MRETURN_ON_ERROR(SetDataNode(path, payload, flags));
   }
   else if (optPruner)
   {
      MessageRef junk = GetMessageFromPool();
      MRETURN_OOM_ON_NULL(junk());
      if (optPruner->MatchPath(path, junk) == false) return B_NO_ERROR;
   }

   MessageRef childrenRef;
   if ((maxDepth > 0)&&(msg.FindMessage(PR_NAME_NODECHILDREN, childrenRef).IsOK())&&(childrenRef()))
   {
      // First recurse to the indexed nodes, adding them as indexed children
      Hashtable<const String *, uint32> indexLookup;
      {
         MessageRef indexRef;
         if (msg.FindMessage(PR_NAME_NODEINDEX, indexRef).IsOK())
         {
            const String * nextFieldName;
            for (int i=0; indexRef()->FindString(PR_NAME_KEYS, i, &nextFieldName).IsOK(); i++)
            {
               MessageRef nextChildRef;
               if (childrenRef()->FindMessage(*nextFieldName, nextChildRef).IsOK()) 
               {
                  String childPath(path);
                  if (childPath.HasChars()) childPath += '/';
                  childPath += *nextFieldName;
                  MRETURN_ON_ERROR(RestoreNodeTreeFromMessage(*nextChildRef(), childPath, true, flags.WithBit(SETDATANODE_FLAG_ADDTOINDEX), maxDepth-1, optPruner));
                  MRETURN_ON_ERROR(indexLookup.Put(nextFieldName, i));
               }
            }
         }
      }

      // Then recurse to the non-indexed child nodes 
      {
         for (MessageFieldNameIterator iter = childrenRef()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
         {
            const String & nextFieldName = iter.GetFieldName();
            if (indexLookup.ContainsKey(&nextFieldName) == false)
            {
               MessageRef nextChildRef;
               if ((childrenRef()->FindMessage(nextFieldName, nextChildRef).IsOK())&&(nextChildRef()))
               {
                  String childPath(path);
                  if (childPath.HasChars()) childPath += '/';
                  childPath += nextFieldName;
                  MRETURN_ON_ERROR(RestoreNodeTreeFromMessage(*nextChildRef(), childPath, true, flags.WithoutBit(SETDATANODE_FLAG_ADDTOINDEX), maxDepth-1, optPruner));
               }
            }
         }
      }
   }

   return B_NO_ERROR;   
}

status_t StorageReflectSession :: RemoveParameter(const String & paramName, bool & retUpdateDefaultMessageRoute)
{
   if (_parameters.HasName(paramName) == false) return B_DATA_NOT_FOUND;  // FogBugz #6348:  DO NOT remove paramName until the end of this method!

   if (paramName.StartsWith("SUBSCRIBE:"))
   {
      String str = paramName.Substring(10);
      _subscriptions.AdjustStringPrefix(str, DEFAULT_PATH_PREFIX);
      if (_subscriptions.RemovePathString(str).IsOK())
      {
         // Remove the references from this subscription from all nodes
         NodePathMatcher temp;
         (void) temp.PutPathString(str, ConstQueryFilterRef());
         SubscribeRefCallbackArgs srcArgs(-1);
         (void) temp.DoTraversal((PathMatchCallback)DoSubscribeRefCallbackFunc, this, GetGlobalRoot(), false, &srcArgs);
      }
   }
   else if (paramName == PR_NAME_REFLECT_TO_SELF)            SetRoutingFlag(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF,      false);
   else if (paramName == PR_NAME_ROUTE_GATEWAY_TO_NEIGHBORS) SetRoutingFlag(MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS, false);
   else if (paramName == PR_NAME_ROUTE_NEIGHBORS_TO_GATEWAY) SetRoutingFlag(MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY, false);
   else if (paramName == PR_NAME_DISABLE_SUBSCRIPTIONS)      SetSubscriptionsEnabled(true);
   else if (paramName == PR_NAME_MAX_UPDATE_MESSAGE_ITEMS)   _maxSubscriptionMessageItems = DEFAULT_MAX_SUBSCRIPTION_MESSAGE_SIZE;  // back to the default
   else if (paramName == PR_NAME_REPLY_ENCODING)
   {
      MessageIOGateway * gw = dynamic_cast<MessageIOGateway *>(GetGateway()());
      if (gw) gw->SetOutgoingEncoding(MUSCLE_MESSAGE_ENCODING_DEFAULT);
   }
   else if ((paramName == PR_NAME_KEYS)||(paramName == PR_NAME_FILTERS))
   {
      _defaultMessageRouteMessage.RemoveName(paramName);
      retUpdateDefaultMessageRoute = true;
   }

   return _parameters.RemoveName(paramName);  // FogBugz #6348:  MUST BE DONE LAST, because this call may clear (paramName)
}

void StorageReflectSession :: AddApplicationSpecificParametersToParametersResultMessage(Message & msg) const
{
   (void) msg;
}

void StorageReflectSession :: TallyNodeBytes(const DataNode & n, uint32 & retNumNodes, uint32 & retNodeBytes) const
{
   retNumNodes++;
   retNodeBytes += n.GetData()()->FlattenedSize();
   for (DataNodeRefIterator iter = n.GetChildIterator(); iter.HasData(); iter++) TallyNodeBytes(*iter.GetValue()(), retNumNodes, retNodeBytes);
}

void StorageReflectSession :: PrintFactoriesInfo() const
{
   printf("There are " UINT32_FORMAT_SPEC " factories attached:\n", GetFactories().GetNumItems());
   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      const ReflectSessionFactory & f = *iter.GetValue()();
      printf("   %s [%p] is listening at [%s] (%sid=" UINT32_FORMAT_SPEC ").\n", f.GetTypeName(), &f, iter.GetKey().ToString()(), f.IsReadyToAcceptSessions()?"ReadyToAcceptSessions, ":"", f.GetFactoryID());
   }
}

void StorageReflectSession :: PrintSessionsInfo() const
{
   const Hashtable<const String *, AbstractReflectSessionRef> & t = GetSessions();
   printf("There are " UINT32_FORMAT_SPEC " sessions attached, and " UINT32_FORMAT_SPEC " subscriber-tables cached:\n", t.GetNumItems(), _sharedData->_cachedSubscribersTables.GetNumItems());
   uint32 totalNumOutMessages = 0, totalNumOutBytes = 0, totalNumNodes = 0, totalNumNodeBytes = 0;
   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(t); iter.HasData(); iter++)
   {
      AbstractReflectSession * ars = iter.GetValue()();
      uint32 numOutMessages = 0, numOutBytes = 0, numNodes = 0, numNodeBytes = 0;
      const AbstractMessageIOGateway * gw = ars->GetGateway()();
      if (gw)
      {
         const Queue<MessageRef> & q = gw->GetOutgoingMessageQueue();
         numOutMessages = q.GetNumItems();
         for (uint32 i=0; i<numOutMessages; i++) numOutBytes = q[i]()->FlattenedSize();
      }

      StorageReflectSession * srs = dynamic_cast<StorageReflectSession *>(iter.GetValue()()); 
      const DataNode * dn = srs ? srs->GetSessionNode()() : NULL;
      if (dn) TallyNodeBytes(*dn, numNodes, numNodeBytes);

      String stateStr;
      if (ars->IsConnectingAsync()) stateStr = stateStr.AppendWord("ConnectingAsync", ", ");
      if (ars->IsConnected()) stateStr = stateStr.AppendWord("Connected", ", ");
      if (ars->IsExpendable()) stateStr = stateStr.AppendWord("Expendable", ", ");
      if (ars->IsReadyForInput()) stateStr = stateStr.AppendWord("IsReadyForInput", ", ");
      if (ars->HasBytesToOutput()) stateStr = stateStr.AppendWord("HasBytesToOutput", ", ");
      if (ars->WasConnected()) stateStr = stateStr.AppendWord("WasConnected", ", ");
      if (stateStr.HasChars()) stateStr = stateStr.Prepend(", ");
      printf("  Session [%s] (rfd=%i,wfd=%i) is [%s]:  (" UINT32_FORMAT_SPEC " outgoing Messages, " UINT32_FORMAT_SPEC " Message-bytes, " UINT32_FORMAT_SPEC " nodes, " UINT32_FORMAT_SPEC " node-bytes%s)\n", iter.GetKey()->Cstr(), ars->GetSessionReadSelectSocket().GetFileDescriptor(), ars->GetSessionWriteSelectSocket().GetFileDescriptor(), ars->GetSessionDescriptionString()(), numOutMessages, numOutBytes, numNodes, numNodeBytes, stateStr());
      totalNumOutMessages += numOutMessages;
      totalNumOutBytes    += numOutBytes;
      totalNumNodes       += numNodes;
      totalNumNodeBytes   += numNodeBytes;
   }
   printf("------------------------------------------------------------\n");
   printf("Totals: " UINT32_FORMAT_SPEC " messages, " UINT32_FORMAT_SPEC " message-bytes, " UINT32_FORMAT_SPEC " nodes, " UINT32_FORMAT_SPEC " node-bytes.\n", totalNumOutMessages, totalNumOutBytes, totalNumNodes, totalNumNodeBytes);
}

} // end namespace muscle

