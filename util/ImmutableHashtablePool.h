/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleImmutableHashtablePool_h
#define MuscleImmutableHashtablePool_h

#include "util/Hashtable.h"
#include "util/CountedObject.h"
#include "util/ObjectPool.h"
#include "util/RefCount.h"

namespace muscle {

template <class KeyType, class ValueType, uint32 MaxCacheableTableSize, class KeyHashFunctorType, class ValueHashFunctorType> class ImmutableHashtablePool; // forward reference

/** This macro declares typedefs for given ImmutableHashtablePool types that follow the standard naming convention.
  * Given a user-provided type name (e.g. MyTable), a Key class (e.g. String), and a Value class (e.g. uint32)
  * it will create typedefs named MyTable, MyTablePool, and ConstMyTableRef.  These types will refer to the
  * desired ImmutableHashtable class, the ImmutableHashtablePool class that uses it, and the read-only
  * reference class used to reference it.
  */
#define DECLARE_IMMUTABLE_HASHTABLE_POOL_TYPES(ImmutableTableTypeName, KeyType, ValueType, MaxCacheableTableSize) \
   typedef ImmutableHashtable<    KeyType, ValueType, MaxCacheableTableSize> ImmutableTableTypeName;              \
   typedef ImmutableHashtablePool<KeyType, ValueType, MaxCacheableTableSize> ImmutableTableTypeName##Pool;        \
   typedef ImmutableHashtablePool<KeyType, ValueType, MaxCacheableTableSize>::ConstImmutableHashtableTypeRef Const##ImmutableTableTypeName##Ref

/** A reference-countable object that contains an immutable Hashtable.  Objects of this type are returned by the methods of the ImmutableHashtablePool class. */
template <class KeyType, class ValueType, uint32 MaxCacheableTableSize, class KeyHashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType), class ValueHashFunctorType=typename DEFAULT_HASH_FUNCTOR(ValueType) > class ImmutableHashtable MUSCLE_FINAL_CLASS : public RefCountable
{
public:
   /** Default constructor */
   ImmutableHashtable() : _hashCodeSum(0) {/* empty */}

   /** Explicit constructor that creates an immutale-table with one entry in it */
   ImmutableHashtable(const KeyType & key, const ValueType & value)
   {
      _hashCodeSum = _table.Put(key, value).IsOK() ? GetHashCodeForKeyValuePair(key, value) : 0;
   }

   /** Creates an ImmutableHashtable containing a copy of the contents of the specified Hashtable.
     * @param rhs the Hashtable to make our Hashtable a duplicate of
     */
   template<class RHSHashFunctorType, class RHSSubclassType> ImmutableHashtable(const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) : _hashCodeSum(0), _table(rhs)
   {
      for (HashtableIterator<KeyType, ValueType> iter(_table, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++) _hashCodeSum += GetHashCodeForKeyValuePair(iter.GetKey(), iter.GetValue());
   }

   /** Returns a read-only reference to the immutable Hashtable. */
   const Hashtable<KeyType, ValueType, KeyHashFunctorType> & GetTable() const {return _table;}

private:
   friend class ImmutableHashtablePool<KeyType, ValueType, MaxCacheableTableSize, KeyHashFunctorType, ValueHashFunctorType>;

   static uint32 GetHashCodeForKey(const KeyType & key) {return GetDefaultObjectForType<KeyHashFunctorType>()(key);}
   static uint32 GetHashCodeForValue(const ValueType & val) {return GetDefaultObjectForType<ValueHashFunctorType>()(val);}
   static uint64 GetHashCodeForKeyValuePair(const KeyType & key, const ValueType & val)
   {
      const uint64 keyHash64 = GetHashCodeForKey(key);    // deliberately storing this in a uint64
      const uint64 valHash64 = GetHashCodeForValue(val);  // deliberately storing this in a uint64
      return (keyHash64?keyHash64:1) * (valHash64?valHash64:1);
   }

   uint64 _hashCodeSum;
   Hashtable<KeyType, ValueType, KeyHashFunctorType> _table;

   typedef ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize, KeyHashFunctorType, ValueHashFunctorType> SelfType;  // just to avoid the parsing limitations of the DECLARE_COUNTED_OBJECT macro
   DECLARE_COUNTED_OBJECT(SelfType);
};

/** This class is used to reduce RAM usage by allowing a large number of objects to share references to a smaller number
  * of Hashtables.  For example, a MUSCLE database with 100,000 DataNodes might have only a few clients subscribed to those
  * DataNodes at a time, so instead of storing 100,000 individual (but mostly identical) subscribers-tables (one for each
  * DataNode), they can instead all reference the same handful of ImmutableHashtable objects, each one of which represents
  * a single combination of subscribed clients.
  */
template <class KeyType, class ValueType, uint32 MaxCacheableTableSize, class KeyHashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType), class ValueHashFunctorType=typename DEFAULT_HASH_FUNCTOR(ValueType) > class ImmutableHashtablePool MUSCLE_FINAL_CLASS : private NotCopyable
{
public:
   /** Default constructor. */
   ImmutableHashtablePool() {/* empty */}

   /** Convenience definition of the type of ImmutableHashtable this ImmutableHashtablePool is handling. */
   typedef ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize, KeyHashFunctorType> ImmutableHashtableType;

   /** Convenience definition of the a ConstRef for the type of ImmutableHashtable this ImmutableHashtablePool is handling. */
   typedef ConstRef<ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize, KeyHashFunctorType> > ConstImmutableHashtableTypeRef;

   /** Returns a reference to an empty immutable Hashtable */
   ConstImmutableHashtableTypeRef GetEmptyTable() const {return DummyConstRef<ImmutableHashtableType>(GetDefaultObjectForType<ImmutableHashtableType>());}

   /** Returns a reference to an immutable Hashtable that is identical to one that was passed in as the first argument, except that
     * the returned Hashtable has been updated with a Put(key, value) call using the specified key and value arguments.
     * @param startWith a Reference to the ImmutableHashtable to use as an initial state.  A NULL reference will be treated as if it was a reference to an empty table.
     * @param key the key to pass to the Put() call.
     * @param value the value to pass to the Put() call.
     * @param maxLRUCacheSize a maximum size for our LRU table cache.  If set to something other than MUSCLE_NO_LIMIT (the default) and we add an item
     *                        to the cache, then we will remove not-recently-used items from the LRU cache until the cache has this many items or fewer.
     * @returns a reference to an ImmutableHashtable in the new/updated state, or a NULL reference on error (out of memory?)
     * @note if (startWith) is a private reference, then this method may update the table that (startWith()) points to and return (startWith).
     */
   ConstImmutableHashtableTypeRef GetWithPut(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType & value, uint32 maxLRUCacheSize = MUSCLE_NO_LIMIT) {return GetWithAux(startWith, key, &value, maxLRUCacheSize);}

   /** Returns a reference to an immutable Hashtable that is identical to one that was passed in as the first argument, except that
     * the returned Hashtable has been updated with a Remove(key) call using the specified key argument.
     * @param startWith a Reference to the ImmutableHashtable to use as an initial state.  A NULL reference will be treated as if it was a reference to an empty table.
     * @param key the key to pass to the Remove() call.
     * @param maxLRUCacheSize a maximum size for our LRU table cache.  If set to something other than MUSCLE_NO_LIMIT (the default) and we add an item
     *                        to the cache, then we will remove not-recently-used items from the LRU cache until the cache has this many items or fewer.
     * @returns a reference to an ImmutableHashtable in the new/updated state, or a NULL reference on error (out of memory?)
     * @note if (startWith) is a private reference, then this method may update the table that (startWith()) points to and return (startWith)..
     */
   ConstImmutableHashtableTypeRef GetWithRemove(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, uint32 maxLRUCacheSize = MUSCLE_NO_LIMIT) {return GetWithAux(startWith, key, NULL, maxLRUCacheSize);}

   /** Returns the number of ConstImmutableHashtableTypeRef's that are currently being held in our cache. */
   uint32 GetNumCachedItems() const {return _lruCache.GetNumItems();}

   /** Clears all cached ImmutableHashtables from our cache */
   void ClearCache() {_lruCache.Clear();}

   /** Clears all cached ImmutableHashtable from our cache that contain the specified key.
     * @param key the key to look for in each cached ImmutableHashtable object.  If found, the object will be dropped from our cache.
     */
   void DropAllCacheEntriesContainingKey(const KeyType & key)
   {
      for (HashtableIterator<uint64, ConstImmutableHashtableTypeRef> iter(_lruCache); iter.HasData(); iter++)
         if (iter.GetValue()()->GetTable().ContainsKey(key)) (void) _lruCache.Remove(iter.GetKey());
   }

   /** Returns true iff the specified immutable Hashtable is part of our current immutable-tables-cache */
   bool Contains(const ConstImmutableHashtableTypeRef & tableRef) const
   {
      if (tableRef() == NULL) return Contains(GetEmptyTable());
      const ConstImmutableHashtableTypeRef * r = _lruCache.Get(tableRef()->_hashCodeSum);
      return ((r)&&(r->GetItemPointer()->GetTable() == tableRef()->GetTable()));
   }

private:
   uint64 HashCodeAfterModification(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType * optNewVal) const
   {
      uint64 newSum = startWith()->_hashCodeSum;
      const ValueType * oldVal = startWith()->GetTable().Get(key);
      if (oldVal)    newSum -= ImmutableHashtableType::GetHashCodeForKeyValuePair(key, *oldVal);
      if (optNewVal) newSum += ImmutableHashtableType::GetHashCodeForKeyValuePair(key, *optNewVal);
      return newSum;
   }

   ConstImmutableHashtableTypeRef GetWithAux(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType * optNewVal, uint32 maxLRUCacheSize)
   {
      if (startWith() == NULL) return GetWithAux(GetEmptyTable(), key, optNewVal, maxLRUCacheSize);  // handle the NULL-ref case gracefully, as an empty-set case

      const uint64 newSum = HashCodeAfterModification(startWith, key, optNewVal);
      // See if we can find the new Hash table already in our cache and re-use it
      const Hashtable<KeyType, ValueType, KeyHashFunctorType> & oldTable = startWith()->GetTable();
      {
         const ConstImmutableHashtableTypeRef * ret = _lruCache.GetAndMoveToFront(newSum);
         if ((ret)&&(oldTable.WouldBeEqualToAfterPutOrRemove(ret->GetItemPointer()->GetTable(), key, optNewVal))) return *ret;
      }

      // Calculate how many key/value pairs will be in the new table so we can EnsureSize() the exact amount of slots needed for it
      const bool alreadyHadKey = oldTable.ContainsKey(key);
      const uint32 newSize = oldTable.GetNumItems() + (optNewVal ? (alreadyHadKey?0:1) : (alreadyHadKey?-1:0));

      if ((optNewVal == NULL)||(newSize > MaxCacheableTableSize))
      {
         const uint32 refStatus = GetRefStatus(startWith);
         if (refStatus != REF_STATUS_PUBLIC)
         {
            // No sense creating a new table if nobody else has access to (startWith()) anyway; it's cheaper to just modify (startWith()) in-place, ImmutableHashtable notwithstanding
            ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize> * isw = const_cast<ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize> *>(startWith());
            if (optNewVal)
            {
               if (isw->_table.Put(key, *optNewVal).IsError()) return ConstImmutableHashtableTypeRef();
            }
            else if (isw->_table.Remove(key).IsError()) return startWith;  // didn't change anything but the key isn't in the table, so all's good, I guess?

            if (refStatus == REF_STATUS_INLRUCACHE) (void) _lruCache.Remove(isw->_hashCodeSum); // old sum is outdated
            isw->_hashCodeSum = newSum;
            if (refStatus == REF_STATUS_INLRUCACHE) (void) _lruCache.Put(isw->_hashCodeSum, startWith); // new sum is correct
            return startWith;
         }
      }

      // Demand-create a new table and add it to our tables-cache for potential re-use later by others
      ImmutableHashtableType * newObj = _pool.ObtainObject();
      ConstImmutableHashtableTypeRef newRef(newObj);
      Hashtable<KeyType, ValueType> * newTab = newObj ? &newObj->_table : NULL;
      if ((newTab)&&(newTab->EnsureSize(newSize,true).IsOK()))
      {
         // Copy over all of the old table's contents, but with our one update applied to the new table
         for (HashtableIterator<KeyType, ValueType> oldIter(oldTable); oldIter.HasData(); oldIter++)
         {
            const KeyType & nextKey = oldIter.GetKey();
            if (nextKey == key)
            {
               if (optNewVal) (void) newTab->Put(nextKey, *optNewVal);  // guaranteed not to fail!
            }
            else (void) newTab->Put(nextKey, oldIter.GetValue());  // guaranteed not to fail!
         }
         if ((alreadyHadKey == false)&&(optNewVal)) (void) newTab->Put(key, *optNewVal);  // guaranteed not to fail!

         newObj->_hashCodeSum = newSum;
         if (newTab->GetNumItems() <= MaxCacheableTableSize)
         {
            (void) _lruCache.PutAtFront(newSum, newRef);  // even if it fails, we can still at least return our table to the caller
            while(_lruCache.GetNumItems() > maxLRUCacheSize) (void) _lruCache.RemoveLast();  // keep the cache size down to something reasonable
         }
         return newRef;
      }
      else MWARN_OUT_OF_MEMORY;

      return ConstImmutableHashtableTypeRef();
   }

   enum {
      REF_STATUS_PRIVATE,    ///< only the caller has access to the table
      REF_STATUS_INLRUCACHE, ///< the caller and the LRUCache both have access to the table
      REF_STATUS_PUBLIC,     ///< others have access to the table as well
      NUM_REF_STATUSES       ///< guard value
   };

   // Returns a REF_STATUS_* value for the given reference
   uint32 GetRefStatus(const ConstImmutableHashtableTypeRef & ref) const
   {
      if (ref.IsRefPrivate())           return REF_STATUS_PRIVATE;
      if (ref.IsRefCounting() == false) return REF_STATUS_PUBLIC;  // paranoia?
      if (ref()->GetRefCount() == 2)
      {
         const ConstImmutableHashtableTypeRef * inCache = _lruCache.Get(ref()->_hashCodeSum);
         if ((inCache)&&(inCache->GetItemPointer() == ref())) return REF_STATUS_INLRUCACHE;
      }
      return REF_STATUS_PUBLIC;
   }

   ObjectPool<ImmutableHashtable<KeyType, ValueType, MaxCacheableTableSize, KeyHashFunctorType, ValueHashFunctorType> > _pool;  // for efficiently creating new tables
   Hashtable<uint64, ConstImmutableHashtableTypeRef> _lruCache;  // hash code -> cached immutable hash table
};

} // end namespace muscle

#endif
