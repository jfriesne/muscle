/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/DataNode.h"
#include "reflector/StorageReflectSession.h"

namespace muscle {

const char * DataNode :: _setDataFlagsLabels[] = {
   "IsBeingCreated",
   "EnableSupercede"
};

DataNode :: DataNode()
   : _parent(NULL)
   , _cachedDataChecksum(0)
   , _children(NULL)
   , _orderedIndex(NULL)
   , _orderedCounter(0)
   , _depth(0)
   , _maxChildIDHint(0)
{
   MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(DataNode::_setDataFlagsLabels, NUM_SET_DATA_FLAGS);  // placed here only to get around privacy restrictions
}

DataNode :: ~DataNode()
{
   delete _children;
   delete _orderedIndex;
}

void DataNode :: Init(const String & name, const ConstMessageRef & initData)
{
   _nodeName           = name;
   _parent             = NULL;
   _depth              = 0;
   _maxChildIDHint     = 0;
   _data               = initData;
   _cachedDataChecksum = 0;
}

void DataNode :: Reset()
{
   TCHECKPOINT;

   // Note that I'm now deleting these auxiliary objects instead of
   // just clearing them.  That will save memory, and also makes a
   // newly-reset DataNode behavior more like a just-created one
   // (See FogBugz #9845 for details)
   delete _children;     _children     = NULL;
   delete _orderedIndex; _orderedIndex = NULL;
   _subscribers.Reset();

   _parent             = NULL;
   _depth              = 0;
   _maxChildIDHint     = 0;
   _data.Reset();
   _cachedDataChecksum = 0;
}

status_t DataNode :: InsertOrderedChild(const ConstMessageRef & data, const String * optInsertBefore, const String * optNodeName, StorageReflectSession * notifyWithOnSetParent, StorageReflectSession * optNotifyChangedData, Hashtable<String, DataNodeRef> * optRetAdded)
{
   TCHECKPOINT;

   if (_orderedIndex == NULL) _orderedIndex = new Queue<DataNodeRef>;

   // Find a unique ID string for our new kid
   String temp;  // must be declared out here!
   if (optNodeName == NULL)
   {
      while(true)
      {
         char buf[50];
         muscleSprintf(buf, "I" UINT32_FORMAT_SPEC, _orderedCounter++);
         if (HasChild(buf) == false) {temp = buf; break;}
      }
      optNodeName = &temp;
   }

   DataNodeRef dref = notifyWithOnSetParent->GetNewDataNode(*optNodeName, data);
   MRETURN_ON_ERROR(dref);

   uint32 insertIndex = _orderedIndex->GetNumItems();  // default to end of index
   if (optInsertBefore)
   {
      for (int32 i=_orderedIndex->GetLastValidIndex(); i>=0; i--)
      {
         if ((*_orderedIndex)[i]()->GetNodeName() == *optInsertBefore)
         {
            insertIndex = i;
            break;
         }
      }
   }

   MRETURN_ON_ERROR(PutChild(dref, notifyWithOnSetParent, optNotifyChangedData));

   // Update the index
   status_t ret;
   if (_orderedIndex->InsertItemAt(insertIndex, dref).IsOK(ret))
   {
      String np;
      if ((optRetAdded)&&(dref()->GetNodePath(np).IsOK())) (void) optRetAdded->Put(np, dref);

      // Notify anyone monitoring this node that the ordered-index has been updated
      notifyWithOnSetParent->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, insertIndex, dref()->GetNodeName());
   }
   else (void) RemoveChild(dref()->GetNodeName(), notifyWithOnSetParent, false, NULL);  // roll back!

   return ret;
}

status_t DataNode :: RemoveIndexEntryAt(uint32 removeIndex, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   if ((_orderedIndex == NULL)||(removeIndex >= _orderedIndex->GetNumItems())) return B_DATA_NOT_FOUND;

   DataNodeRef holdKey = _orderedIndex->RemoveItemAtWithDefault(removeIndex);  // gotta make a temp copy here, or it's dangling pointer time
   if ((holdKey())&&(optNotifyWith)) optNotifyWith->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYREMOVED, removeIndex, holdKey()->GetNodeName());
   return B_NO_ERROR;
}

status_t DataNode :: InsertIndexEntryAt(uint32 insertIndex, StorageReflectSession * notifyWithOnSetParent, const String & key)
{
   TCHECKPOINT;

   if (_children == NULL) return B_BAD_OBJECT;

   DataNodeRef childNode;
   MRETURN_ON_ERROR(_children->Get(&key, childNode));

   if (_orderedIndex == NULL) _orderedIndex = new Queue<DataNodeRef>;
   MRETURN_ON_ERROR(_orderedIndex->InsertItemAt(insertIndex, childNode));

   // Notify anyone monitoring this node that the ordered-index has been updated
   notifyWithOnSetParent->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, insertIndex, childNode()->GetNodeName());
   return B_NO_ERROR;
}

status_t DataNode :: ReorderChild(const DataNodeRef & child, const String * optMoveToBeforeThis, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   if (child() == NULL) return B_BAD_ARGUMENT;
   if (_orderedIndex == NULL) return B_DATA_NOT_FOUND;
   if ((optMoveToBeforeThis)&&(*optMoveToBeforeThis == child()->GetNodeName())) return B_NO_ERROR;  // moving a child to before his own position is a no-op, but it's ok

   MRETURN_ON_ERROR(RemoveIndexEntry(child()->GetNodeName(), optNotifyWith));

   // Then re-add him to the index at the appropriate point
   uint32 targetIndex = _orderedIndex->GetNumItems();  // default to end of index
   if ((optMoveToBeforeThis)&&(HasChild(*optMoveToBeforeThis)))
   {
      for (int32 i=_orderedIndex->GetLastValidIndex(); i>=0; i--)
      {
         if (*optMoveToBeforeThis == (*_orderedIndex)[i]()->GetNodeName())
         {
            targetIndex = i;
            break;
         }
      }
   }

   // Now add the child back into the index at his new position
   MRETURN_ON_ERROR(_orderedIndex->InsertItemAt(targetIndex, child));

   // Notify anyone monitoring this node that the ordered-index has been updated
   if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, targetIndex, child()->GetNodeName());
   return B_NO_ERROR;
}

status_t DataNode :: PutChild(const DataNodeRef & node, StorageReflectSession * optNotifyWithOnSetParent, StorageReflectSession * optNotifyChangedData)
{
   TCHECKPOINT;

   DataNode * child = node();
   if (child == NULL) return B_BAD_ARGUMENT;

   if (_children == NULL) _children = new Hashtable<const String *, DataNodeRef>;
   if ((optNotifyWithOnSetParent)&&(_children->GetNumItems() >= optNotifyWithOnSetParent->_maxChildrenPerDataNodeCount)&&(_children->ContainsKey(&child->_nodeName) == false)) return B_RESOURCE_LIMIT;  // BAB-1081

   MRETURN_ON_ERROR(child->SetParent(this, optNotifyWithOnSetParent));

   DataNodeRef oldNode;
   MRETURN_ON_ERROR(_children->Put(&child->_nodeName, node, oldNode));

   if (optNotifyChangedData)
   {
      ConstMessageRef oldData; if (oldNode()) oldData = oldNode()->GetData();
      optNotifyChangedData->NotifySubscribersThatNodeChanged(*child, oldData, StorageReflectSession::NodeChangeFlags());
   }
   return B_NO_ERROR;
}

status_t DataNode :: SetParent(DataNode * parent, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   if (parent)
   {
      if (_parent) LogTime(MUSCLE_LOG_WARNING, "Warning, overwriting previous parent of node [%s]\n", GetNodeName()());
      if (parent->_depth >= MUSCLE_MAX_NODE_DEPTH)
      {
         LogTime(MUSCLE_LOG_ERROR, "DataNode::SetParent():  Can't set node [%s] as parent of [%s], maximum node depth (%i) exceeded!\n", parent->GetNodePath()(), GetNodeName()(), MUSCLE_MAX_NODE_DEPTH);
         return B_RESOURCE_LIMIT;
      }
   }

   _parent = parent;
   if (_parent)
   {
      const char * nn = _nodeName();
      _parent->_maxChildIDHint = muscleMax(_parent->_maxChildIDHint, (uint32) atol(&nn[(*nn=='I')?1:0]));
   }
   else _subscribers.Reset();

   if (_parent)
   {
      _depth = _parent->_depth+1;
      if (optNotifyWith) optNotifyWith->NotifySubscribersOfNewNode(*this);
   }
   else _depth = 0;

   return B_NO_ERROR;
}

const String * DataNode :: GetPathClause(uint32 depth) const
{
   if (depth <= _depth)
   {
      const DataNode * node = this;
      for (uint32 i=depth; i<_depth; i++) if (node) node = node->_parent;
      if (node) return &node->GetNodeName();
   }
   return NULL;
}

status_t DataNode :: GetNodePath(String & retPath, uint32 startDepth) const
{
   TCHECKPOINT;

   // Calculate node path and node depth
   if (_parent)
   {
      // Calculate the total length that our node path string will be
      uint32 pathLen = 0;
      {
         uint32 d = _depth;
         const DataNode * node = this;
         while((d-- >= startDepth)&&(node->_parent))
         {
            pathLen += 1 + node->_nodeName.Length();  // the 1 is for the slash
            node = node->_parent;
         }
      }

      if ((pathLen > 0)&&(startDepth > 0)) pathLen--;  // for (startDepth>0), there will be no initial slash

      // Might as well make sure we have enough memory to return it, up front
      MRETURN_ON_ERROR(retPath.Prealloc(pathLen));

      char * dynBuf = NULL;
      const uint32 stackAllocSize = 256;
      char stackBuf[stackAllocSize] = ""; // try to do this without a dynamic allocation...
      if (pathLen >= stackAllocSize)  // but do a dynamic allocation if we have to (should be rare)
      {
         dynBuf = newnothrow_array(char, pathLen+1);
         MRETURN_OOM_ON_NULL(dynBuf);
      }

      char * writeAt = (dynBuf ? dynBuf : stackBuf) + pathLen;  // points to last char in buffer
      *writeAt = '\0';  // terminate the string first (!)
      const DataNode * node = this;
      uint32 d = _depth;
      while((d >= startDepth)&&(node->_parent))
      {
         const int len = node->_nodeName.Length();
         writeAt -= len;
         memcpy(writeAt, node->_nodeName(), len);
         if ((startDepth == 0)||(d > startDepth)) *(--writeAt) = '/';
         node = node->_parent;
         d--;
      }

      retPath = (dynBuf ? dynBuf : stackBuf);
      delete [] dynBuf;
   }
   else retPath = (startDepth == 0) ? "/" : "";

   return B_NO_ERROR;
}

status_t DataNode :: RemoveChild(const String & key, StorageReflectSession * optNotifyWith, bool recurse, uint32 * optCurrentNodeCount)
{
   TCHECKPOINT;

   if (_children == NULL) return B_DATA_NOT_FOUND;

   DataNodeRef childRef;
   MRETURN_ON_ERROR(_children->Get(&key, childRef));

   DataNode * child = childRef();
   if (child)
   {
      if (recurse) while(child->HasChildren()) (void) child->RemoveChild(**(child->_children->GetFirstKey()), optNotifyWith, recurse, optCurrentNodeCount);

      (void) RemoveIndexEntry(key, optNotifyWith);
      if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeChanged(*child, child->GetData(), StorageReflectSession::NodeChangeFlags(StorageReflectSession::NODE_CHANGE_FLAG_ISBEINGREMOVED));

      (void) child->SetParent(NULL, optNotifyWith);  // guaranteed not to fail because first arg is NULL
   }
   if (optCurrentNodeCount) (*optCurrentNodeCount)--;

   (void) _children->Remove(&key, childRef);
   return B_NO_ERROR;
}

status_t DataNode :: RemoveIndexEntry(const String & key, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   // Update our ordered-node index & notify everyone about the change
   if (_orderedIndex)
   {
      for (int32 i=_orderedIndex->GetLastValidIndex(); i>=0; i--)
      {
         if (key == (*_orderedIndex)[i]()->GetNodeName())
         {
            (void) _orderedIndex->RemoveItemAt(i);
            if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYREMOVED, i, key);
            return B_NO_ERROR;
         }
      }
   }
   return B_DATA_NOT_FOUND;
}

void DataNode :: SetData(const ConstMessageRef & data, StorageReflectSession * optNotifyWith, SetDataFlags setDataFlags)
{
   ConstMessageRef oldData;
   if (setDataFlags.IsBitSet(SET_DATA_FLAG_ISBEINGCREATED) == false) oldData = _data;
   _data = data;
   _cachedDataChecksum = 0;
   if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeChanged(*this, oldData, setDataFlags.IsBitSet(SET_DATA_FLAG_ENABLESUPERCEDE)?StorageReflectSession::NodeChangeFlags(StorageReflectSession::NODE_CHANGE_FLAG_ENABLESUPERCEDE):StorageReflectSession::NodeChangeFlags());
}

uint32 DataNode :: CalculateChecksum(uint32 maxRecursionDepth) const
{
   // demand-calculate the local checksum and cache the result, since it can be expensive if the Message is big
   if (_cachedDataChecksum == 0) _cachedDataChecksum = _nodeName.CalculateChecksum()+(_data()?_data()->CalculateChecksum():0);
   if (maxRecursionDepth == 0) return _cachedDataChecksum;
   else
   {
      uint32 ret = _cachedDataChecksum;
      if (_orderedIndex) for (int32 i=_orderedIndex->GetLastValidIndex(); i>=0; i--) ret += (*_orderedIndex)[i]()->GetNodeName().CalculateChecksum();
      if (_children) for (HashtableIterator<const String *, DataNodeRef> iter(*_children); iter.HasData(); iter++) ret += iter.GetValue()()->CalculateChecksum(maxRecursionDepth-1);
      return ret;
   }
}

void DataNode :: Print(const OutputPrinter & p, uint32 maxRecursionDepth, int indentLevel) const
{
   p.putc(' ', indentLevel);
   String np; (void) GetNodePath(np);
   p.printf("DataNode [%s] numChildren=" UINT32_FORMAT_SPEC " orderedIndex=" INT32_FORMAT_SPEC " checksum=" UINT32_FORMAT_SPEC " msgChecksum=" UINT32_FORMAT_SPEC "\n", np(), _children?_children->GetNumItems():0, _orderedIndex?(int32)_orderedIndex->GetNumItems():(int32)-1, CalculateChecksum(maxRecursionDepth), _data()?_data()->CalculateChecksum():0);
   if (_data()) _data()->Print(p.WithIndent(1), true);
   if (maxRecursionDepth > 0)
   {
      if (_orderedIndex)
      {
         for (uint32 i=0; i<_orderedIndex->GetNumItems(); i++)
         {
            p.putc(' ', indentLevel);
            p.printf("   Index slot " UINT32_FORMAT_SPEC " = %s\n", i, (*_orderedIndex)[i]()->GetNodeName()());
         }
      }
      if (_children)
      {
         p.putc(' ', indentLevel);
         p.printf("Children for node [%s] follow:\n", np());
         for (HashtableIterator<const String *, DataNodeRef> iter(*_children); iter.HasData(); iter++) iter.GetValue()()->Print(p, maxRecursionDepth-1, indentLevel+2);
      }
   }
}

DataNode * DataNode :: FindFirstMatchingNode(const char * path, uint32 maxDepth) const
{
   switch(path[0])
   {
      case '\0': return const_cast<DataNode *>(this);
      case '/':  return GetRootNode()->FindFirstMatchingNode(path+1, maxDepth);

      default:
      {
         if ((_children == NULL)||(maxDepth == 0)) return NULL;

         const char * nextSlash = strchr(path, '/');
         const String childKey(path, (nextSlash)?((uint32)(nextSlash-path)):MUSCLE_NO_LIMIT);
         const char * recurseArg = nextSlash?(nextSlash+1):"";

         if (CanWildcardStringMatchMultipleValues(childKey))
         {
            StringMatcher sm(childKey);
            for (DataNodeRefIterator iter = GetChildIterator(); iter.HasData(); iter++)
            {
               if (sm.Match(*iter.GetKey()))
               {
                  const DataNodeRef * childRef = _children->Get(iter.GetKey());
                  if (childRef)
                  {
                     DataNode * ret = childRef->GetItemPointer()->FindFirstMatchingNode(recurseArg, maxDepth-1);
                     if (ret) return ret;
                  }
               }
            }
         }
         else
         {
            const DataNodeRef * childRef = _children->Get(&childKey);
            if (childRef) return childRef->GetItemPointer()->FindFirstMatchingNode(recurseArg, maxDepth-1);
         }
      }
      return NULL;
   }
}

DataNodeRef DataNode :: GetDescendantAux(const char * subPath) const
{
   const char * slash = strchr(subPath, '/');
   if (slash)
   {
      DataNodeRef child = GetChild(String(subPath, (uint32)(slash-subPath)));
      return child() ? child()->GetDescendantAux(slash+1) : DataNodeRef();
   }
   else return GetChild(subPath);
}

} // end namespace muscle
