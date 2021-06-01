/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleImmutableHashtablePool_h
#define MuscleImmutableHashtablePool_h

#include "util/Hashtable.h"
#include "util/ObjectPool.h"
#include "util/RefCount.h"

namespace muscle {

template <class KeyType, class ValueType, class KeyHashFunctorType, class ValueHashFunctorType> class ImmutableHashtablePool; // forward reference

/** A reference-countable object that contains an immutable Hashtable.  Objects of this type are returned by the methods of the ImmutableHashtablePool class. */
template <class KeyType, class ValueType, class KeyHashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType), class ValueHashFunctorType=typename DEFAULT_HASH_FUNCTOR(ValueType) > class ImmutableHashtable MUSCLE_FINAL_CLASS : public RefCountable
{
public:
   /** Default constructor */
   ImmutableHashtable() : _hashCodeSum(0) {/* empty */}

   /** Returns a read-only reference to the immutable Hashtable. */
   const Hashtable<KeyType, ValueType, KeyHashFunctorType> & GetTable() const {return _table;}

private:
   friend class ImmutableHashtablePool<KeyType, ValueType, KeyHashFunctorType, ValueHashFunctorType>;

   uint64 _hashCodeSum;
   Hashtable<KeyType, ValueType, KeyHashFunctorType> _table;
};
//DECLARE_REFTYPES(ImmutableHashtable);

/** This class is used to reduce RAM usage by allowing a large number of objects to share references to a smaller number
  * of Hashtables.  For example, a MUSCLE database with 100,000 DataNodes might have only a few clients subscribed to those
  * DataNodes at a time, so instead of storing 100,000 individual (but mostly identical) subscribers-tables (one for each
  * DataNode), they can instead all reference the same handful of ImmutableHashtable objects, each one of which represents
  * a single combination of subscribed clients.
  */
template <class KeyType, class ValueType, class KeyHashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType), class ValueHashFunctorType=typename DEFAULT_HASH_FUNCTOR(ValueType) > class ImmutableHashtablePool MUSCLE_FINAL_CLASS : private NotCopyable
{
public:
   /** Default constructor. */
   ImmutableHashtablePool() {/* empty */}

   /** Convenience definition of the type of ImmutableHashtable this ImmutableHashtablePool is handling. */
   typedef ImmutableHashtable<KeyType, ValueType, KeyHashFunctorType> ImmutableHashtableType;

   /** Convenience definition of the a ConstRef for the type of ImmutableHashtable this ImmutableHashtablePool is handling. */
   typedef ConstRef<ImmutableHashtable<KeyType, ValueType, KeyHashFunctorType> > ConstImmutableHashtableTypeRef;

   /** Returns a reference to an empty immutable Hashtable */
   ConstImmutableHashtableTypeRef GetEmptyTable() const {return DummyConstRef<ImmutableHashtableType>(GetDefaultObjectForType<ImmutableHashtableType>());}

   /** Returns a reference to an immutable Hashtable that is identical to one that was passed in as the first argument, except that
     * the returned Hashtable has been updated with a Put(key, value) call using the specified key and value arguments.
     * @param startWith a Reference to the ImmutableHashtable to use as an initial state.  A NULL reference will be treated as if it was a reference to an empty table.
     * @param key the key to pass to the Put() call.
     * @param value the value to pass to the Put() call.
     * @returns a reference to an ImmutableHashtable in the new/updated state, or a NULL reference on error (out of memory?)
     */
   ConstImmutableHashtableTypeRef GetWithPut(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType & value) {return GetWithAux(startWith, key, &value);}

   /** Returns a reference to an immutable Hashtable that is identical to one that was passed in as the first argument, except that
     * the returned Hashtable has been updated with a Remove(key) call using the specified key argument.
     * @param startWith a Reference to the ImmutableHashtable to use as an initial state.  A NULL reference will be treated as if it was a reference to an empty table.
     * @param key the key to pass to the Remove() call.
     * @returns a reference to an ImmutableHashtable in the new/updated state, or a NULL reference on error (out of memory?)
     */
   ConstImmutableHashtableTypeRef GetWithRemove(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key) {return GetWithAux(startWith, key, NULL);}

   /** Returns the number of ConstImmutableHashtableTypeRef's that are currently being held in our cache. */
   uint32 GetNumCachedItems() const {return _cache.GetNumItems();}

   /** Clears all cached ImmutableHashtables from our cache */
   void ClearCache() {_cache.Clear();}

   /** Clears all cached ImmutableHashtable from our cache that contain the specified key.
     * @param key the key to look for in each cached ImmutableHashtable object.  If found, the object will be dropped from our cache.
     */
   void DropAllCacheEntriesContainingKey(const KeyType & key)
   {
      for (HashtableIterator<uint64, ConstImmutableHashtableTypeRef> iter(_cache); iter.HasData(); iter++)
         if (iter.GetValue()()->GetTable().ContainsKey(key)) (void) _cache.Remove(iter.GetKey());
   }

private:
   uint32 GetHashCodeForKey(const KeyType & key) const {return GetDefaultObjectForType<KeyHashFunctorType>()(key);}
   uint32 GetHashCodeForValue(const ValueType & val) const {return GetDefaultObjectForType<ValueHashFunctorType>()(val);}

   uint64 GetHashCodeForKeyValuePair(const KeyType & key, const ValueType & val) const
   {
      const uint64 keyHash64 = GetHashCodeForKey(key);    // deliberately storing this in a uint64
      const uint64 valHash64 = GetHashCodeForValue(val);  // deliberately storing this in a uint64
      return (keyHash64?keyHash64:1) * (valHash64?valHash64:1);
   }

   uint64 HashCodeAfterModification(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType * optNewVal) const
   {
      uint64 newSum = startWith()->_hashCodeSum;
      const ValueType * oldVal = startWith()->GetTable().Get(key);
      if (oldVal)    newSum -= GetHashCodeForKeyValuePair(key, *oldVal);
      if (optNewVal) newSum += GetHashCodeForKeyValuePair(key, *optNewVal);
      return newSum;
   }

   ConstImmutableHashtableTypeRef GetWithAux(const ConstImmutableHashtableTypeRef & startWith, const KeyType & key, const ValueType * optNewVal)
   {
      if (startWith() == NULL) return GetWithAux(GetEmptyTable(), key, optNewVal);  // handle the NULL-ref case gracefully, as an empty-set case

      // See if we can find the new Hash table already in our cache and re-use it
      const uint64 newSum = HashCodeAfterModification(startWith, key, optNewVal);
      const Hashtable<KeyType, ValueType, KeyHashFunctorType> & oldTable = startWith()->GetTable();
      {
         const ConstImmutableHashtableTypeRef * ret = _cache.Get(newSum);
         if ((ret)&&(oldTable.WouldBeEqualToAfterModification(ret->GetItemPointer()->GetTable(), key, optNewVal))) return *ret;
      }

      // Calculate how many key/value pairs will be in the new table so we can EnsureSize() the exact amount of slots needed for it
      const bool alreadyHadKey = oldTable.ContainsKey(key);
      const uint32 newSize = oldTable.GetNumItems() + (optNewVal ? (alreadyHadKey?0:1) : (alreadyHadKey?-1:0));
 
      // Demand-create a new table and add it to our cache for potential re-use later
      ImmutableHashtableType * newObj = _pool.ObtainObject();
      ConstImmutableHashtableTypeRef newRef(newObj);
      Hashtable<KeyType, ValueType> * newTab = newObj ? &newObj->_table : NULL;
      if ((newTab)
       && (newTab->EnsureSize(newSize).IsOK())
       && (newTab->CopyFrom(startWith()->GetTable()).IsOK())
       && ((optNewVal?newTab->Put(key, *optNewVal):newTab->Remove(key)).IsOK())
       && (_cache.Put(newSum, newRef).IsOK()))
      {
         newObj->_hashCodeSum = newSum;
         return newRef;
      }
      else {MWARN_OUT_OF_MEMORY; return ConstImmutableHashtableTypeRef();}
   }

   ObjectPool<ImmutableHashtable<KeyType, ValueType, KeyHashFunctorType, ValueHashFunctorType> > _pool;  // for efficiently creating new tables
   Hashtable<uint64, ConstImmutableHashtableTypeRef> _cache;  // hash code -> cached immutable hash table
};

} // end namespace muscle

#endif
