/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/DataNode.h"
#include "reflector/StorageReflectSession.h"

namespace muscle {

DataNode :: DataNode()
   : _children(NULL)
   , _orderedIndex(NULL)
   , _orderedCounter(0L)
{
   // empty
}

DataNode :: ~DataNode() 
{
   delete _children;
   delete _orderedIndex;
}

void DataNode :: Init(const String & name, const MessageRef & initData)
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

status_t DataNode :: InsertOrderedChild(const MessageRef & data, const String * optInsertBefore, const String * optNodeName, StorageReflectSession * notifyWithOnSetParent, StorageReflectSession * optNotifyChangedData, Hashtable<String, DataNodeRef> * optRetAdded)
{
   TCHECKPOINT;

   if (_orderedIndex == NULL)
   {
      _orderedIndex = newnothrow Queue<DataNodeRef>;
      MRETURN_OOM_ON_NULL(_orderedIndex);
   }

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
   MRETURN_OOM_ON_NULL(dref());

   uint32 insertIndex = _orderedIndex->GetNumItems();  // default to end of index
   if (optInsertBefore)
   {
      for (int i=_orderedIndex->GetNumItems()-1; i>=0; i--) 
      {
         if ((*_orderedIndex)[i]()->GetNodeName() == *optInsertBefore)
         {
            insertIndex = i;
            break;
         }
      }
   }
 
   // Update the index
   status_t ret;

   if (PutChild(dref, notifyWithOnSetParent, optNotifyChangedData).IsOK(ret))
   {
      if (_orderedIndex->InsertItemAt(insertIndex, dref).IsOK(ret))
      {
         String np;
         if ((optRetAdded)&&(dref()->GetNodePath(np).IsOK())) (void) optRetAdded->Put(np, dref);

         // Notify anyone monitoring this node that the ordered-index has been updated
         notifyWithOnSetParent->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, insertIndex, dref()->GetNodeName());
         return B_NO_ERROR;
      }
      else (void) RemoveChild(dref()->GetNodeName(), notifyWithOnSetParent, false, NULL);  // undo!
   }

   return ret | B_ERROR;
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

   status_t ret;
   DataNodeRef childNode;
   if (_children->Get(&key, childNode).IsOK(ret))
   {
      if (_orderedIndex == NULL)
      {
         _orderedIndex = newnothrow Queue<DataNodeRef>;
         MRETURN_OOM_ON_NULL(_orderedIndex);
      }
      if (_orderedIndex->InsertItemAt(insertIndex, childNode).IsOK(ret))
      {
         // Notify anyone monitoring this node that the ordered-index has been updated
         notifyWithOnSetParent->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, insertIndex, childNode()->GetNodeName());
         return B_NO_ERROR;
      }
   }
   return ret;
}

status_t DataNode :: ReorderChild(const DataNodeRef & child, const String * moveToBeforeThis, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   if (_orderedIndex == NULL) return B_BAD_OBJECT;
   if ((child() == NULL)||((moveToBeforeThis)&&(*moveToBeforeThis == child()->GetNodeName()))) return B_BAD_ARGUMENT;

   // Only do anything if we have an index, and the node isn't going to be moved to before itself (silly) and (child) can be removed from the index
   status_t ret;
   if (RemoveIndexEntry(child()->GetNodeName(), optNotifyWith).IsOK(ret))
   {
      // Then re-add him to the index at the appropriate point
      uint32 targetIndex = _orderedIndex->GetNumItems();  // default to end of index
      if ((moveToBeforeThis)&&(HasChild(*moveToBeforeThis)))
      {
         for (int i=_orderedIndex->GetNumItems()-1; i>=0; i--) 
         { 
            if (*moveToBeforeThis == (*_orderedIndex)[i]()->GetNodeName())
            {
               targetIndex = i;
               break; 
            }
         }
      }

      // Now add the child back into the index at his new position
      if (_orderedIndex->InsertItemAt(targetIndex, child).IsOK(ret))
      {
         // Notify anyone monitoring this node that the ordered-index has been updated
         if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeIndexChanged(*this, INDEX_OP_ENTRYINSERTED, targetIndex, child()->GetNodeName());
         return B_NO_ERROR;
      }
   }
   return ret;
}

status_t DataNode :: PutChild(const DataNodeRef & node, StorageReflectSession * optNotifyWithOnSetParent, StorageReflectSession * optNotifyChangedData)
{
   TCHECKPOINT;

   DataNode * child = node();
   if (child == NULL) return B_BAD_ARGUMENT;

   if (_children == NULL) 
   {
      _children = newnothrow Hashtable<const String *, DataNodeRef>;
      MRETURN_OOM_ON_NULL(_children);
   }

   child->SetParent(this, optNotifyWithOnSetParent);
   DataNodeRef oldNode;

   status_t ret;
   if ((_children->Put(&child->_nodeName, node, oldNode).IsOK(ret))&&(optNotifyChangedData))
   {
      MessageRef oldData; if (oldNode()) oldData = oldNode()->GetData();
      optNotifyChangedData->NotifySubscribersThatNodeChanged(*child, oldData, StorageReflectSession::NodeChangeFlags());
   }
   return ret;
}

void DataNode :: SetParent(DataNode * parent, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   if ((_parent)&&(parent)) LogTime(MUSCLE_LOG_WARNING, "Warning, overwriting previous parent of node [%s]\n", GetNodeName()());
   _parent = parent;
   if (_parent) 
   {
      const char * nn = _nodeName();
      _parent->_maxChildIDHint = muscleMax(_parent->_maxChildIDHint, (uint32) atol(&nn[(*nn=='I')?1:0]));
   }
   else _subscribers.Reset();

   // Calculate our node's depth into the tree
   _depth = 0;
   if (_parent)
   {
      // Calculate the total length that our node path string will be
      const DataNode * node = this;
      while(node->_parent) 
      {
         _depth++;
         node = node->_parent;
      }
      if (optNotifyWith) optNotifyWith->NotifySubscribersOfNewNode(*this);
   }
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

   status_t ret;
   DataNodeRef childRef;
   if (_children->Get(&key, childRef).IsOK(ret))
   {
      DataNode * child = childRef();
      if (child)
      {
         if (recurse) while(child->HasChildren()) (void) child->RemoveChild(**(child->_children->GetFirstKey()), optNotifyWith, recurse, optCurrentNodeCount);

         (void) RemoveIndexEntry(key, optNotifyWith);
         if (optNotifyWith) optNotifyWith->NotifySubscribersThatNodeChanged(*child, child->GetData(), StorageReflectSession::NodeChangeFlags(StorageReflectSession::NODE_CHANGE_FLAG_ISBEINGREMOVED));

         child->SetParent(NULL, optNotifyWith);
      }
      if (optCurrentNodeCount) (*optCurrentNodeCount)--;

      (void) _children->Remove(&key, childRef);
      return B_NO_ERROR;
   }
   else return ret;
}

status_t DataNode :: RemoveIndexEntry(const String & key, StorageReflectSession * optNotifyWith)
{
   TCHECKPOINT;

   // Update our ordered-node index & notify everyone about the change
   if (_orderedIndex)
   {
      for (int i=_orderedIndex->GetNumItems()-1; i>=0; i--)
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

void DataNode :: SetData(const MessageRef & data, StorageReflectSession * optNotifyWith, SetDataFlags setDataFlags)
{
   MessageRef oldData;
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
      if (_orderedIndex) for (int32 i=_orderedIndex->GetNumItems()-1; i>=0; i--) ret += (*_orderedIndex)[i]()->GetNodeName().CalculateChecksum();
      if (_children) for (HashtableIterator<const String *, DataNodeRef> iter(*_children); iter.HasData(); iter++) ret += iter.GetValue()()->CalculateChecksum(maxRecursionDepth-1);
      return ret;
   }
}

static void PrintIndent(FILE * file, int indentLevel) {for (int i=0; i<indentLevel; i++) fputc(' ', file);}

void DataNode :: PrintToStream(FILE * optFile, uint32 maxRecursionDepth, int indentLevel) const
{
   if (optFile == NULL) optFile = stdout;

   PrintIndent(optFile, indentLevel);
   String np; (void) GetNodePath(np);
   fprintf(optFile, "DataNode [%s] numChildren=" UINT32_FORMAT_SPEC " orderedIndex=" INT32_FORMAT_SPEC " checksum=" UINT32_FORMAT_SPEC " msgChecksum=" UINT32_FORMAT_SPEC "\n", np(), _children?_children->GetNumItems():0, _orderedIndex?(int32)_orderedIndex->GetNumItems():(int32)-1, CalculateChecksum(maxRecursionDepth), _data()?_data()->CalculateChecksum():0);
   if (_data()) _data()->PrintToStream(optFile, true, indentLevel+1);
   if (maxRecursionDepth > 0)
   {
      if (_orderedIndex)
      {
         for (uint32 i=0; i<_orderedIndex->GetNumItems(); i++)
         {
            PrintIndent(optFile, indentLevel);
            fprintf(optFile, "   Index slot " UINT32_FORMAT_SPEC " = %s\n", i, (*_orderedIndex)[i]()->GetNodeName()());
         }
      }
      if (_children)
      {
         PrintIndent(optFile, indentLevel); fprintf(optFile, "Children for node [%s] follow:\n", np());
         for (HashtableIterator<const String *, DataNodeRef> iter(*_children); iter.HasData(); iter++) iter.GetValue()()->PrintToStream(optFile, maxRecursionDepth-1, indentLevel+2);
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

DataNodeSubscribersTable :: DataNodeSubscribersTable(const DataNodeSubscribersTable * optCopyMe, const String & sessionIDString, int32 delta)
{
   if (delta < 0)
   {
      const uint32 * c = optCopyMe ? optCopyMe->_subscribers.Get(sessionIDString) : NULL;
      delta = c ? muscleMax(delta, -((int32)(*c))) : 0;   // so we won't have to deal with underflow cases, below
   }

   if (optCopyMe) 
   {
      // This will allow _subscribers to avoid an array-reallocation when we call GetOrPut() below
      if ((delta > 0)&&(optCopyMe->_subscribers.ContainsKey(sessionIDString) == false)) (void) _subscribers.EnsureSize(optCopyMe->_subscribers.GetNumItems()+1);
      _subscribers = optCopyMe->_subscribers;
   }

   _hashCode = DataNodeSubscribersTable::HashCodeAfterModification(optCopyMe?optCopyMe->HashCode():0, sessionIDString, delta);

   if (delta > 0)
   {
      uint32 * c = _subscribers.GetOrPut(sessionIDString);
      if (c) (*c) += delta;
        else MWARN_OUT_OF_MEMORY;
   }
   else if (delta < 0)
   {
      uint32 * c = _subscribers.Get(sessionIDString);  // guaranteed not to be NULL, because we checked above and if it was, we would've set (delta) to zero and wouldn't be here
      (*c) -= delta;
      if ((*c) == 0) (void) _subscribers.Remove(sessionIDString);
   }
}

// This method should only be called if we know that the proposed modification won't change the number of key-value pairs in the table
bool DataNodeSubscribersTable :: IsEqualToAfterModificationAux(const DataNodeSubscribersTable & toMe, const String & sessionIDString, int32 delta) const
{
   if (_subscribers.GetNumItems() != toMe._subscribers.GetNumItems()) return false;

   for (HashtableIterator<String, uint32> iter(_subscribers); iter.HasData(); iter++)
   {
      const String & key      = iter.GetKey();
      const uint32 myCount    = iter.GetValue();
      const uint32 * hisCount = toMe._subscribers.Get(key);
      if ((hisCount == NULL)||((myCount+((key==sessionIDString)?delta:0)) != *hisCount)) return false;
   }
   return true;
}

bool DataNodeSubscribersTable :: IsEqualToAfterModification(const DataNodeSubscribersTable & toMe, const String & sessionIDString, int32 delta) const
{
        if (delta == 0) return (*this == toMe);
   else if (delta > 0)
   {
      if (_subscribers.ContainsKey(sessionIDString)) return IsEqualToAfterModificationAux(toMe, sessionIDString, delta);
      else
      {
         // In this case, after our modification we'll have one more key-value pair in our table than we do now
         if (toMe._subscribers.GetNumItems() != (_subscribers.GetNumItems()+1)) return false;

         const uint32 uDelta = delta;
         for (HashtableIterator<String, uint32> hisIter(toMe._subscribers); hisIter.HasData(); hisIter++)
         {
            const String & key     = hisIter.GetKey();
            const uint32 hisCount  = hisIter.GetValue();
            const uint32 * myCount = (key == sessionIDString) ? &uDelta : _subscribers.Get(key);
            if ((myCount == NULL)||(*myCount != hisCount)) return false;
         }
         return true;
      }
   }
   else
   {
      const uint32 * myCurCount = _subscribers.Get(sessionIDString);
      if (myCurCount)
      {
         const uint32 uDelta = (uint32)(-delta);  // how many to deduct
         if (*myCurCount > uDelta) return IsEqualToAfterModificationAux(toMe, sessionIDString, delta);
         else
         {
            // In this case, after our modification we'll have one less key-value pair in our table than we do now
            if (toMe._subscribers.GetNumItems() != (_subscribers.GetNumItems()-1)) return false;

            for (HashtableIterator<String, uint32> hisIter(toMe._subscribers); hisIter.HasData(); hisIter++)
            {
               const String & key = hisIter.GetKey();
               if (key == sessionIDString) return false;  // After our modification we shouldn't have this key anymore!

               const uint32 hisCount  = hisIter.GetValue();
               const uint32 * myCount = _subscribers.Get(key);
               if ((myCount == NULL)||(*myCount != hisCount)) return false;
            }
            return true;
         }
      }
      else return (*this == toMe);
   }
}

} // end namespace muscle
