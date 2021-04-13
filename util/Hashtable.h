/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleHashtable_h
#define MuscleHashtable_h

#include "support/MuscleSupport.h"
#include "support/Void.h"  // only here since various Hashtable users may need it
#include "util/DemandConstructedObject.h"

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
#  define MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS 1
# endif
#endif

#ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
# include "system/AtomicCounter.h"
# include "system/SetupSystem.h"  // for GetCurrentThreadID()
#endif

#ifdef _MSC_VER
# pragma warning(disable: 4786)
#endif

#ifndef MUSCLE_HASHTABLE_DEFAULT_CAPACITY
/** The number of key/value-pairs in the array an empty Hashtable object allocates from the heap the first time data is inserted into it.  Note that 8 is an awkward size because it means that after 5 doublings, the table will be of size 256, which is just slightly too large to use with uint8-indices, since ((uint8)-1) is used as a guard value.  That's why this defaults to 7 rather than 8. */
# define MUSCLE_HASHTABLE_DEFAULT_CAPACITY 7
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
# ifndef MUSCLE_AVOID_CPLUSPLUS11
// Enable move semantics (when possible) for C++11
# define HT_UniversalSinkKeyRef template<typename HT_KeyParam>
# define HT_UniversalSinkValueRef template<typename HT_ValueParam>
# define HT_UniversalSinkKeyValueRef template<typename HT_KeyParam, typename HT_ValueParam>
# define HT_SinkKeyParam HT_KeyParam &&
# define HT_SinkValueParam HT_ValueParam &&
# define HT_PlunderKey(key) std::move(key)
# define HT_ForwardKey(key) std::forward<HT_KeyParam>(key)
# define HT_PlunderValue(val) std::move(val)
# define HT_ForwardValue(val) std::forward<HT_ValueParam>(val)
# else
// For earlier versions of C++, use the traditional copy/ref semantics
# define HT_UniversalSinkKeyRef
# define HT_UniversalSinkValueRef
# define HT_UniversalSinkKeyValueRef
# define HT_SinkKeyParam const KeyType &
# define HT_SinkValueParam const ValueType &
# define HT_KeyParam const KeyType &
# define HT_ValueParam const ValueType &
# define HT_PlunderKey(key) (key)
# define HT_ForwardKey(key) (key)
# define HT_PlunderValue(val) (val)
# define HT_ForwardValue(val) (val)
# endif
#endif

namespace muscle {

// implementation detail; please ignore this!
static const uint32 MUSCLE_HASHTABLE_INVALID_HASH_CODE  = (uint32)-1;
static const uint32 MUSCLE_HASHTABLE_INVALID_SLOT_INDEX = (uint32)-1;

// Forward declarations
template <class KeyType, class ValueType, class HashFunctorType>                     class HashtableIterator;
template <class KeyType, class ValueType, class HashFunctorType>                     class HashtableBase;
template <class KeyType, class ValueType, class HashFunctorType, class SubclassType> class HashtableMid;

/** These flags can be passed to the HashtableIterator constructor (or to the GetIterator()/GetIteratorAt() 
  * functions in the Hashtable class) to modify the iterator's behaviour.
  */
enum {
   HTIT_FLAG_BACKWARDS  = (1<<0), /**< iterate backwards. */
   HTIT_FLAG_NOREGISTER = (1<<1), /**< don't register the iterator object with Hashtable object */
};

/**
 * This class is an iterator object, used for iterating over the set
 * of keys or values in a Hashtable.  Note that the Hashtable class
 * maintains the ordering of its keys and values, unlike many hash table
 * implementations.
 *
 * Given a Hashtable object, you can obtain one or more of these
 * iterator objects by calling the Hashtable's GetIterator() method;
 * or you can just specify the Hashtable you want to iterate over as 
 * an argument to the HashtableIterator constructor.
 *
 * The most common form for a Hashtable iteration is this:
 *
 * for (HashtableIterator<String, int> iter(table); iter.HasData(); iter++)
 * {
 *    const String & nextKey = iter.GetKey(); 
 *    int nextValue = iter.GetValue(); 
 *    [...]
 * }
 *
 * It is safe to modify or delete a Hashtable during an iteration-traversal (from the same 
 * thread only); any HashtableIterators that are in the middle of iterating over the Hashtable 
 * will be automatically notified about the modification, so that they can do the right thing 
 * (and in particular, not continue to point to any no-longer-existing hashtable entries).
 */
template <class KeyType, class ValueType, class HashFunctorType = typename AutoChooseHashFunctorHelper<KeyType>::Type > class HashtableIterator MUSCLE_FINAL_CLASS
{
public:
   /** Convenience typedef for the type of Hashtable this HashtableIterator is associated with. */
   typedef HashtableBase<KeyType, ValueType, HashFunctorType> HashtableType;

   /**
    * Default constructor.  It's here only so that you can include HashtableIterators
    * as member variables, in arrays, etc.  HashtableIterators created with this
    * constructor are "empty", so they won't be useful until you set them equal to a 
    * HashtableIterator that was returned by Hashtable::GetIterator().
    */
   HashtableIterator();

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   HashtableIterator(const HashtableIterator & rhs);

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIterator().  
     * @param table the Hashtable to iterate over.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Defaults to zero for default behaviour.  
     */
   HashtableIterator(const HashtableType & table, uint32 flags = 0);

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIteratorAt().  
     * @param table the Hashtable to iterate over.
     * @param startAt the first key that should be returned by the iteration.  If (startAt) is not in the table,
     *                the iterator will not return any results.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Set to zero to get the default behaviour.
     */
   HT_UniversalSinkKeyRef HashtableIterator(const HashtableType & table, HT_SinkKeyParam startAt, uint32 flags);

   /** Destructor */
   ~HashtableIterator();

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   HashtableIterator & operator=(const HashtableIterator & rhs);

   /** Advances this iterator by one entry in the table.  */
   void operator++(int) 
   {
      if (_scratchKeyAndValue.EnsureObjectDestructed() == false) _iterCookie = _owner ? _owner->GetSubsequentEntry(_iterCookie, _flags) : NULL;
      UpdateKeyAndValuePointers();
   }

   /** Retracts this iterator by one entry in the table.  The opposite of the ++ operator. */
   void operator--(int) {bool b = IsBackwards(); SetBackwards(!b); (*this)++; SetBackwards(b);}

   /** Returns true iff this iterator is pointing to valid key/value data.  Do not call GetKey() or GetValue()
     * unless this method returns true!  Note that the value returned by this method can change if the 
     * Hashtable is modified.
     */
   bool HasData() const {return (_currentKey != NULL);}

   /**
    * Returns a reference to the key this iterator is currently pointing at.  This method does not change the state of the iterator.
    * @note Be careful with this method, if this iterator isn't currently pointing at any key,
    *       it will return a NULL reference and your program will crash when you try to use it.
    *       Typically you should only call this function after checking to see that HasData() returns true.
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   const KeyType & GetKey() const 
   {
      assert(_currentKey != NULL);  // this makes clang++SA happy
      return *_currentKey;
   }

   /**
    * Returns a reference to the value this iterator is currently pointing at.
    * @note Be careful with this method, if this iterator isn't currently pointing at any value,
    *       it will return a NULL reference and your program will crash when you try to use it.
    *       Typically you should only call this function after checking to see that HasData() returns true.
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   ValueType & GetValue() const 
   { 
      assert(_currentVal != NULL);  // this makes clang++SA happy
      return *_currentVal;
   }

   /** Returns this iterator's HTIT_FLAG_* bit-chord value. */
   uint32 GetFlags() const {return _flags;}

   /** Sets or unsets the HTIT_FLAG_BACWARDS flag on this iterator. 
     * @param backwards If true, this iterator will be set to iterate backwards from wherever it is currently; 
     *                  if false, this iterator will be set to iterate forwards from wherever it is currently.
     */
   void SetBackwards(bool backwards) {if (backwards) _flags |= HTIT_FLAG_BACKWARDS; else _flags &= ~HTIT_FLAG_BACKWARDS;}

   /** Returns true iff this iterator is set to iterate in reverse order -- i.e. if HTIT_FLAG_BACKWARDS
     * was passed in to the constructor, or if SetBackwards(true) was called.
     */
   bool IsBackwards() const {return ((_flags & HTIT_FLAG_BACKWARDS) != 0);}

private:
   friend class HashtableBase<KeyType, ValueType, HashFunctorType>;

   HT_UniversalSinkKeyValueRef void SetScratchValues(HT_SinkKeyParam key, HT_SinkValueParam val)
   {
      KeyAndValue & kav = _scratchKeyAndValue.GetObject();
      kav._key   = HT_ForwardKey(key);
      kav._value = HT_ForwardValue(val);
   }

   void UpdateKeyAndValuePointers()
   {
      if (_scratchKeyAndValue.IsObjectConstructed())
      {
         _currentKey = &_scratchKeyAndValue.GetObjectUnchecked()._key;
         _currentVal = &_scratchKeyAndValue.GetObjectUnchecked()._value;
      }
      else if ((_iterCookie)&&(_owner))  // (_owner) test isn't strictly necessary, but it keeps clang++ happy
      {
         _currentKey = &_owner->GetKeyFromCookie(_iterCookie); 
         _currentVal = &_owner->GetValueFromCookie(_iterCookie); 
      }
      else
      {
         _currentKey = NULL;
         _currentVal = NULL;
      }
   }

   void * _scratchSpace;         // ignore this; it's temp scratch space used by EnsureSize().
   void * _iterCookie;           // points to the HashtableEntryBase object that we are currently associated with
   const KeyType * _currentKey;  // cached result, so that GetKey() can be a branch-free inline method
   ValueType * _currentVal;      // cached result, so that GetValue() can be a branch-free inline method

   uint32 _flags;
   HashtableIterator * _prevIter; // for the doubly linked list so that the table can notify us if it is modified
   HashtableIterator * _nextIter; // for the doubly linked list so that the table can notify us if it is modified
   const HashtableType * _owner;  // table that we are associated with

   // Used for emergency storage of scratch values
   class KeyAndValue
   {
   public:
      KeyAndValue() {/* empty */}
      HT_UniversalSinkKeyValueRef KeyAndValue(HT_SinkKeyParam key, HT_SinkValueParam value) 
         : _key(HT_ForwardKey(key))
         , _value(HT_ForwardValue(value)) 
      {/* empty */}
   
      KeyType _key;
      ValueType _value;
   };
   DemandConstructedObject<KeyAndValue> _scratchKeyAndValue;
   bool _okayToUnsetThreadID;
};

/** This internal superclass is an implementation detail and should not be instantiated directly.  Instantiate a Hashtable, OrderedKeysHashtable, or OrderedValuesHashtable instead. */
template<class KeyType, class ValueType, class HashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType) > class HashtableBase
{
public:
   /** Returns the number of items stored in the table. */
   uint32 GetNumItems() const {return _numItems;}

   /** Convenience method;  Returns true iff the table is empty (i.e. if GetNumItems() is zero). */
   bool IsEmpty() const {return (_numItems == 0);}

   /** Convenience method;  Returns true iff the table is non-empty (i.e. if GetNumItems() is non-zero). */
   bool HasItems() const {return (_numItems > 0);}

   /** Returns true iff the table contains a mapping with the given key.  (O(1) search time)
     * @param key the key to inquire about
     */
   bool ContainsKey(const KeyType & key) const {return (this->GetEntry(this->ComputeHash(key), key) != NULL);}

   /** Returns true iff the table contains a mapping with the given value.  (O(n) search time) 
     * @param value the value to inquire about
     */
   bool ContainsValue(const ValueType & value) const;

   /** Returns true iff this Hashtable has the same contents as (rhs).
     * @param rhs The table to compare this table against
     * @param considerOrdering Whether the order of the key/value pairs within the tables should be considered as part of "equality".  Defaults to false. 
     * @returns true iff (rhs) is equal to to (this), false if they differ.
     */
   bool IsEqualTo(const HashtableBase & rhs, bool considerOrdering = false) const;

   /** Returns true iff the keys in this Hashtable are the same as the keys in (rhs).
     * Note that the ordering of the keys is not considered, only their values.  Values in either table are not considered, either.
     * @param rhs The Hashtable whose keys we should compare against our keys.
     * @returns true iff both Hashtables have the same set of Key objects.
     */
   template<class HisValueType, class HisHashFunctorType> bool AreKeySetsEqual(const HashtableBase<KeyType, HisValueType, HisHashFunctorType> & rhs) const;

   /** Returns true iff every key present in this Hashtable is also present in (rhs).
     * @param rhs the Hashtable to check the keys of against our own.
     */
   template<class HisValueType, class HisHashFunctorType> bool AreKeysASubsetOf(const HashtableBase<KeyType, HisValueType, HisHashFunctorType> & rhs) const;

   /** Returns true iff every key present in (rhs) is also present in this Hashtable.
     * @param rhs the Hashtable to check the keys of against our own.
     */
   template<class HisValueType, class HisHashFunctorType> bool AreKeysASupersetOf(const HashtableBase<KeyType, HisValueType, HisHashFunctorType> & rhs) const {return rhs.AreKeysASubsetOf(*this);}

   /** Returns the given key's position in the hashtable's linked list, or -1 if the key wasn't found.  O(n) count time (if the key exists, O(1) if it doesn't)
     * @param key a key value to find the index of
     * @returns the position of the key in the iteration list, or -1 if the key is not in the table.
     */
   int32 IndexOfKey(const KeyType & key) const;

   /** Returns the index of the first (or last) occurrance of (value), or -1 if (value) does
     * not exist in this Hashtable.  Note that the search is O(N).
     * @param value The value to search for.
     * @param searchBackwards If set to true, the table will be searched in reverse order.
     *                        and the index returned (if valid) will be the index of the
     *                        last instance of (value) in the table, rather than the first.
     * @returns The index into the table, or -1 if (value) doesn't exist in the table's set of values.
     */
   int32 IndexOfValue(const ValueType & value, bool searchBackwards = false) const;

   /** Attempts to retrieve the associated value from the table for a given key.  (O(1) lookup time)
    *  @param key The key to use to look up a value.
    *  @param setValue On success, the associated value is copied into this object.
    *  @return B_NO_ERROR on success, B_DATA_NOT_FOUND if their was no value found for the given key.
    */
   status_t GetValue(const KeyType & key, ValueType & setValue) const; 

   /** Retrieve a pointer to the associated value object for the given key.  (O(1) lookup time)
    *  @param key The key to use to look up a value.
    *  @return A pointer to the internally held value object for the given key,
    *          or NULL of no object was found.  Note that this object is only
    *          guaranteed to remain valid as long as the Hashtable remains unchanged.
    */
   ValueType * GetValue(const KeyType & key);

   /** As above, but read-only. 
    *  @param key The key to use to look up a value.
    *  @return A pointer to the internally held value object for the given key,
    *          or NULL of no object was found.  Note that this object is only
    *          guaranteed to remain valid as long as the Hashtable remains unchanged.
    */
   const ValueType * GetValue(const KeyType & key) const;

   /** Given a lookup key, returns the a copy of the actual key as held by the table.
    *  This method is only useful in rare situations where the hashing or comparison
    *  functions are such that lookupKeys and held keys are not guaranteed equivalent.
    *  @param lookupKey The key used to look up the held key object.
    *  @param setKey On success, the actual key held in the table is placed here.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure.
    */
   status_t GetKey(const KeyType & lookupKey, KeyType & setKey) const;

   /** Given a key, returns a pointer to the actual key object in this table that matches
    *  that key, or NULL if there is no matching key.  This method is only useful in rare
    *  situations where the hashing or comparison functions are such that lookup keys and
    *  held keys are not guaranteed equivalent.
    *  @param lookupKey The key used to look up the key object
    *  @return A pointer to an internally held key object on success, or NULL on failure.
    */
   const KeyType * GetKey(const KeyType & lookupKey) const;

   /** The iterator type that goes with this HashtableBase type */
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** Get an iterator for use with this table.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Defaults to zero, for default behaviour.
     * @return an iterator object that can be used to examine the items in the hash table, starting at
     *         the specified key.  If the specified key is not in this table, an empty iterator will be returned.
     */
   IteratorType GetIterator(uint32 flags = 0) const {return IteratorType(*this, flags);}

   /** Get an iterator for use with this table, starting at the given entry.
     * @param startAt The key in this table to start the iteration at.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Defaults to zero, for default behaviour.
     * @return an iterator object that can be used to examine the items in the hash table, starting at
     *         the specified key.  If the specified key is not in this table, an empty iterator will be returned.
     */
   HT_UniversalSinkKeyRef IteratorType GetIteratorAt(HT_KeyParam startAt, uint32 flags = 0) const 
   {
      return IteratorType(*this, HT_ForwardKey(startAt), flags);
   }

   /** Returns a pointer to the (index)'th key in this Hashtable.
    *  (This Hashtable class keeps its entries in a well-defined order)
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the key to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @returns Pointer to the key at position (index) on success, or NULL on failure (bad index?)
    */
   const KeyType * GetKeyAt(uint32 index) const;

   /** Returns the (index)'th key in this Hashtable.
    *  (This Hashtable class keeps its entries in a well-defined order)
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the key to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @param retKey On success, the contents of the (index)'th key will be written into this object.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure.
    */
   status_t GetKeyAt(uint32 index, KeyType & retKey) const;

   /** Returns a reference to the (index)'th key in this Hashtable, or a default-constructed key if this table doesn't contain that many keys.
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the key to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @return A reference to a key value, either one from the table or the one specified.
    */
   const KeyType & GetKeyAtWithDefault(uint32 index) const;

   /** Returns a copy of the (index)'th key in this Hashtable, or (defaultKey) if this table doesn't contain that many keys.
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the key to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @param defaultKey If (index) is not less than the number of items in this table, then this will be returned.
    *  @return A reference to a key value, either one from the table or the one specified.
    *  @note that this method returns by-value rather than by-reference, because
    *        returning (defaultKey) by-reference makes it too easy to call this method
    *        in a way that would cause it to return a dangling-reference-to-a-temporary-object.
    */
   KeyType GetKeyAtWithDefault(uint32 index, const KeyType & defaultKey) const;

   /** Returns a pointer to the (index)'th value in this Hashtable.
    *  (This Hashtable class keeps its entries in a well-defined order)
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the value to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @returns Pointer to the value at position (index) on success, or NULL on failure (bad index?)
    */
   const ValueType * GetValueAt(uint32 index) const;

   /** Returns the (index)'th value in this Hashtable.
    *  (This Hashtable class keeps its entries in a well-defined order)
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the value to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @param retValue On success, the contents of the (index)'th value will be written into this object.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure.
    */
   status_t GetValueAt(uint32 index, ValueType & retValue) const;

   /** Returns a reference to the (index)'th value in this Hashtable, or a default-constructed value if this table doesn't contain that many value.
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the value to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @return A reference to a value value, either one from the table or the one specified.
    */
   const ValueType & GetValueAtWithDefault(uint32 index) const;

   /** Returns a copy of the (index)'th value in this Hashtable, or (defaultValue) if this table doesn't contain that many value.
    *  Note that this method is an O(N) operation, so for iteration, always use GetIterator() instead.
    *  @param index Index of the value to return a pointer to.  Should be in the range [0, GetNumItems()-1].
    *  @param defaultValue If (index) is not less than the number of items in this table, then this will be returned.
    *  @return A reference to a value value, either one from the table or the one specified.
    *  @note that this method returns by-value rather than by-reference, because
    *        returning (defaultValue) by-reference makes it too easy to call this method
    *        in a way that would cause it to return a dangling-reference-to-a-temporary-object.
    */
   ValueType GetValueAtWithDefault(uint32 index, const ValueType & defaultValue) const;

   /** Given a value, returns a pointer to the first key in the table that
     * is paired with that value.  Note that this method is an O(N) operation.
     * @param value a value to look for in the table.
     * @returns A pointer to an associated key on success, or NULL if no matching value is found.
     */
   const KeyType * GetFirstKeyWithValue(const ValueType & value) const {return GetKeyWithValueAux(value, false);}

   /** Given a value, returns a pointer to the last key in the table that
     * is paired with that value.  Note that this method is an O(N) operation.
     * @param value a value to look for in the table.
     * @returns A pointer to an associated key on success, or NULL if no matching value is found.
     */
   const KeyType * GetLastKeyWithValue(const ValueType & value) const {return GetKeyWithValueAux(value, true);}

   /** Removes a mapping from the table.  (O(1) removal time)
    *  @param key The key of the key-value mapping to remove.
    *  @return B_NO_ERROR if a key was found and the mapping removed, or B_DATA_NOT_FOUND if the key wasn't found.
    */
   status_t Remove(const KeyType & key) {return RemoveAux(key, NULL);}

   /** Removes the mapping with the given (key) and places its value into (setRemovedValue).  (O(1) removal time)
    *  @param key The key of the key-value mapping to remove.
    *  @param setRemovedValue On success, the removed value is copied into this object.
    *  @return B_NO_ERROR if a key was found and the mapping removed, or B_DATA_NOT_FOUND if the key wasn't found.
    */
   status_t Remove(const KeyType & key, ValueType & setRemovedValue) {return RemoveAux(key, &setRemovedValue);}

   /** Convenience method:  For each key in the passed-in-table, removes that key (and its associated value) from this table. 
     * @param pairs A table containing keys that should be removed from this table.
     * @returns the number of items actually removed from this table.
     */
   uint32 Remove(const HashtableBase & pairs);

   /** Removes a mapping from the table and returns the removed value.
     * If the mapping did not exist in the table, a default value is returned instead.
     * @param key The key of the key-value mapping to remove.
     * @return The value of the removed key-value mapping, or a default-constructed value if the key wasn't found.
     */
   ValueType RemoveWithDefault(const KeyType & key) {ValueType ret; return (RemoveAux(key, &ret).IsOK()) ? ret : GetDefaultValue();}

   /** Removes a mapping from the table and returns the removed value.
     * If the mapping did not exist in the table, the specified default value is returned instead.
     * @param key The key of the key-value mapping to remove.
     * @param defaultValue The default value to return if (key) wasn't found.
     * @return The value of the removed key-value mapping, or the specified default value if the key wasn't found.
     */
   HT_UniversalSinkValueRef ValueType RemoveWithDefault(const KeyType & key, HT_SinkValueParam defaultValue) {ValueType ret; return (RemoveAux(key, &ret).IsOK()) ? ret : HT_ForwardValue(defaultValue);}

   /** Convenience method:  Removes from this Hashtable all key/value pairs for which the same key is not present in (pairs)
     * @param pairs the other table to intersect our table against
     * @returns the number of items actually removed from this table.
     */
   uint32 Intersect(const HashtableBase & pairs);

   /** Convenience method:  Removes the first key/value mapping in the table.  (O(1) removal time)
    *  @return B_NO_ERROR if the first mapping was removed, or B_DATA_NOT_FOUND if this table was already empty.
    */
   status_t RemoveFirst() {return RemoveEntryByIndex(_iterHeadIdx, NULL);}

   /** Convenience method:  Removes the first key/value mapping in the table and places the removed key
    *  into (setRemovedKey).  (O(1) removal time)
    *  @param setRemovedKey On success, the removed key is copied into this object.
    *  @return B_NO_ERROR if the first mapping was removed and the key placed into (setRemovedKey), or B_DATA_NOT_FOUND if the table was already empty.
    */
   status_t RemoveFirst(KeyType & setRemovedKey) 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      if (e == NULL) return B_DATA_NOT_FOUND;
      setRemovedKey = e->_key;
      return RemoveEntry(e, NULL);
   }

   /** Convenience method:  Removes the first key/value mapping in the table and places its 
    *  key into (setRemovedKey) and its value into (setRemovedValue).  (O(1) removal time)
    *  @param setRemovedKey On success, the removed key is copied into this object.
    *  @param setRemovedValue On success, the removed value is copied into this object.
    *  @return B_NO_ERROR if the first mapping was removed and the key and value placed into the arguments, or B_DATA_NOT_FOUND if the table was already empty.
    */
   status_t RemoveFirst(KeyType & setRemovedKey, ValueType & setRemovedValue) 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      if (e == NULL) return B_DATA_NOT_FOUND;
      setRemovedKey = e->_key;
      return RemoveEntry(e, &setRemovedValue);
   }

   /** Convenience method:  Removes the last key/value mapping in the table.  (O(1) removal time)
    *  @return B_NO_ERROR if the last mapping was removed, or B_DATA_NOT_FOUND if the table was already empty.
    */
   status_t RemoveLast() 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? RemoveEntry(e, NULL) : B_DATA_NOT_FOUND;
   }

   /** Convenience method:  Removes the last key/value mapping in the table and places the removed key
    *  into (setRemovedKey).  (O(1) removal time)
    *  @param setRemovedKey On success, the removed key is copied into this object.
    *  @return B_NO_ERROR if the last mapping was removed and the key placed into (setRemovedKey), or B_DATA_NOT_FOUND if the table was already empty.
    */
   status_t RemoveLast(KeyType & setRemovedKey) 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      if (e == NULL) return B_DATA_NOT_FOUND;
      setRemovedKey = e->_key;
      return RemoveEntry(e, NULL);
   }

   /** Convenience method:  Removes the last key/value mapping in the table and places its 
    *  key into (setRemovedKey) and its value into (setRemovedValue).  (O(1) removal time)
    *  @param setRemovedKey On success, the removed key is copied into this object.
    *  @param setRemovedValue On success, the removed value is copied into this object.
    *  @return B_NO_ERROR if the last mapping was removed and the key and value placed into the arguments, or B_DATA_NOT_FOUND if the table was already empty.
    */
   status_t RemoveLast(KeyType & setRemovedKey, ValueType & setRemovedValue) 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      setRemovedKey = e->_key;
      return RemoveEntry(e, &setRemovedValue);
   }

   /** Removes all mappings from the hash table.  (O(N) clear time)
    *  @param releaseCachedData If set true, we will immediately free any buffers we may contain.  
    *                           Otherwise, we'll keep them around in case they can be re-used later on.
    */
   void Clear(bool releaseCachedData = false);

   /** Computes the average number of key-comparisons that will be required for
     * looking up the current contents of this table.  
     * Note that this method iterates over the entire table, so it should only
     * be called when performance is not important (e.g. when trying to debug/optimize a hash function)
     * @param printStatistics If true, text describing the table's layout will be printed to stdout also.
     * @returns The average number of key-comparisons needed to find an item in this table, given its current contents.
     */
   float CountAverageLookupComparisons(bool printStatistics = false) const;

   /** Sorts the iteration-traversal list of this table by key, using the given key-comparison functor object.
     * This method uses a very efficient O(log(N)) MergeSort algorithm.
     * @param func The key-comparison functor to use.
     * @param optCompareCookie Optional cookie value to pass to func.Compare().  Its meaning is specific to the functor type.
     */
   template<class KeyCompareFunctorType> void SortByKey(const KeyCompareFunctorType & func, void * optCompareCookie = NULL) {SortByEntry(ByKeyEntryCompareFunctor<KeyCompareFunctorType>(func), optCompareCookie);}

   /** As above, except that the comparison functor is not specified.  The default comparison functor for our key type will be used.
     * @param optCompareCookie Optional cookie value to pass to func.Compare().  Its meaning is specific to the functor type.
     */
   void SortByKey(void * optCompareCookie = NULL) {SortByKey(CompareFunctor<KeyType>(), optCompareCookie);}

   /** Sorts the iteration-traversal list of this table by value, using the given value-comparison functor object.
     * This method uses a very efficient O(log(N)) MergeSort algorithm.
     * @param func The value-comparison functor to use.
     * @param optCompareCookie Optional cookie value to pass to func.Compare().  Its meaning is specific to the functor type.
     */
   template<class ValueCompareFunctorType> void SortByValue(const ValueCompareFunctorType & func, void * optCompareCookie = NULL) {SortByEntry(ByValueEntryCompareFunctor<ValueCompareFunctorType>(func), optCompareCookie);}

   /** As above, except that the comparison functor is not specified.  The default comparison functor for our value type will be used.
     * @param optCompareCookie Optional cookie value to pass to func.Compare().  Its meaning is specific to the functor type.
     */
   void SortByValue(void * optCompareCookie = NULL) {SortByValue(CompareFunctor<ValueType>(), optCompareCookie);}

   /** This method does an efficient zero-copy swap of this hash table's contents with those of (swapMe).  
    *  That is to say, when this method returns, (swapMe) will be identical to the old state of this 
    *  Hashtable, and this Hashtable will be identical to the old state of (swapMe). 
    *  Any active iterators present for either table will swap owners also, becoming associated with the other table.
    *  @param swapMe The table whose contents and iterators are to be swapped with this table's.
    */
   void SwapContents(HashtableBase & swapMe) {SwapContentsAux(swapMe, true);}

   /** Moves the given entry to the head of the HashtableIterator traversal sequence.
     * @param moveMe Key of the item to be moved to the front of the sequence.
     * @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if (moveMe) was not found in the table.
     * @note calling this method is generally a bad idea if the table is in auto-sort mode,
     *       as it is likely to unsort the traversal ordering and thus break auto-sorting.  However,
     *       calling Sort() will restore the sort-order and make auto-sorting work again)
     */
   status_t MoveToFront(const KeyType & moveMe);

   /** Moves the given entry to the tail of the HashtableIterator traversal sequence.
     * @param moveMe Key of the item to be moved to the end of the sequence.
     * @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if (moveMe) was not found in the table.
     * @note calling this method is generally a bad idea if the table is in auto-sort mode,
     *       as it is likely to unsort the traversal ordering and thus break auto-sorting.  However,
     *       calling Sort() will restore the sort-order and make auto-sorting work again)
     */
   status_t MoveToBack(const KeyType & moveMe);

   /** Moves the given entry to the spot just in front of the other specified entry in the 
     * HashtableIterator traversal sequence.
     * @param moveMe Key of the item to be moved.
     * @param toBeforeMe Key of the item that (moveMe) should be placed in front of.
     * @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if (moveMe) was not found in the table, 
     *         or B_BAD_ARGUMENT if (moveMe) and (toBeforeMe) are equal.
     * @note calling this method is generally a bad idea if the table is in auto-sort mode,
     *       as it is likely to unsort the traversal ordering and thus break auto-sorting.  However,
     *       calling Sort() will restore the sort-order and make auto-sorting work again)
     */
   status_t MoveToBefore(const KeyType & moveMe, const KeyType & toBeforeMe);

   /** Moves the given entry to the spot just behind the other specified entry in the 
     * HashtableIterator traversal sequence.
     * @param moveMe Key of the item to be moved.
     * @param toBehindMe Key of the item that (moveMe) should be placed behind.
     * @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if (moveMe) was not found in the table, 
     *         or B_BAD_ARGUMENT if (moveMe) and (toBehindMe) are equal.
     * @note calling this method is generally a bad idea if the table is in auto-sort mode,
     *       as it is likely to unsort the traversal ordering and thus break auto-sorting.  However,
     *       calling Sort() will restore the sort-order and make auto-sorting work again)
     */
   status_t MoveToBehind(const KeyType & moveMe, const KeyType & toBehindMe);

   /** Moves the given entry to the (nth) position in the HashtableIterator traversal sequence.
     * Note that this is an O(N) operation.
     * @param moveMe Key of the item to be moved.
     * @param toPosition The position that this item should be moved to.  Zero would move
     *                   the item to the head of the traversal sequence, one to the second
     *                   position, and so on.  Values greater than or equal to the number
     *                   of items in the Hashtable will move the item to the last position.
     * @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if (moveMe) was not found in the table.
     */
   status_t MoveToPosition(const KeyType & moveMe, uint32 toPosition);

   /** Convenience synonym for GetValue() 
     * @param key the key to inquire about
     * @param retValue on success, the value associated with (key) is written here
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND on failure (key-not-found)
     */
   status_t Get(const KeyType & key, ValueType & retValue) const {return GetValue(key, retValue);}

   /** Convenience synonym for GetValue() 
     * @param key the key to lookup a value for
     * @returns A pointer to the value associated with key on success, or NULL on failure (key-not-found)
     */
   ValueType * Get(const KeyType & key) {return GetValue(key);}

   /** As above, but read-only.
     * @param key the key to lookup a value for
     * @returns A pointer to the value associated with key on success, or NULL on failure (key-not-found)
     */
   const ValueType * Get(const KeyType & key) const {return GetValue(key);}

   /** Similar to Get(), except that if the specified key is not found, the ValueType's default value is returned.
     * @param key The key whose value should be returned.
     * @returns (key)'s associated value, or the default ValueType value.
     */
   const ValueType & GetWithDefault(const KeyType & key) const
   {
      const ValueType * v = Get(key);
      return v ? *v : GetDefaultValue();
   }

   /** The const [] operator returns the Value associated with the specified key, or a 
     * default-constructed Value object if the specified key is not present in this Hashtable.  
     * That is to say, calling myTable[5] is equivalent to calling myTable.GetWithDefault(5).
     * @param key A key whose associated value we wish to look up.
     * @returns a reference to the associated value, or to a default-constructed value if (key) wasn't present in this Hashtable.
     */
   const ValueType & operator[](const KeyType & key) const {return GetWithDefault(key);}

   /** Similar to Get(), except that if the specified key is not found,
     * the specified default value is returned.  Note that this method
     * returns its value by value, not by reference, to avoid any
     * dangling-reference issues that might occur if (defaultValue)
     * was a temporary.
     * @param key The key whose value should be returned.
     * @param defaultValue The value to return is (key) is not in the table.
     *                     Defaults to a default-constructed item of the value type.
     * @returns (key)'s associated value, or (defaultValue).
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defaultValue) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   ValueType GetWithDefault(const KeyType & key, const ValueType & defaultValue) const
   {
      const ValueType * v = Get(key);
      return v ? *v : defaultValue;
   }

   /** Convenience method.  Returns a pointer to the first key in our iteration list, or NULL if the table is empty. */
   const KeyType * GetFirstKey() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? &e->_key : NULL;
   }

   /** Convenience method.  Returns a reference to the first key in our iteration list, or a default key if the table is empty. */
   const KeyType & GetFirstKeyWithDefault() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? e->_key : GetDefaultKey();
   }

   /** Convenience method.  Returns a reference to the first key in our iteration list, or the specified default key object if the table is empty.
     * @param defaultKey The key value to return if the table is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defaultKey) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   KeyType GetFirstKeyWithDefault(const KeyType & defaultKey) const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? e->_key : defaultKey;
   }

   /** As above, but read-ony. */
   const KeyType * GetLastKey() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? & e->_key : NULL;
   }

   /** Convenience method.  Returns a reference to the last key in our iteration list, or a default key object if the table is empty.  */
   const KeyType & GetLastKeyWithDefault() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? e->_key : GetDefaultKey();
   }

   /** Convenience method.  Returns a reference to the last key in our iteration list, or the specified default key object if the table is empty. 
     * @param defaultKey The key value to return if the table is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defaultKey) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   KeyType GetLastKeyWithDefault(const KeyType & defaultKey) const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? e->_key : defaultKey;
   }

   /** Convenience method.  Returns a pointer to the last value in our iteration list, or NULL if the table is empty. */
   ValueType * GetFirstValue() 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? &e->_value : NULL;
   }

   /** As above, but read-only. */
   const ValueType * GetFirstValue() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? &e->_value : NULL;
   }

   /** Convenience method.  Returns a reference to the first value in our iteration list, or a default value object if the table is empty.  */
   const ValueType & GetFirstValueWithDefault() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? e->_value : GetDefaultValue();
   }

   /** Convenience method.  Returns a copy of the first value in our iteration list, or the specified default value object if the table is empty. 
     * @param defaultValue The value to return if the table is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defaultValue) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   ValueType GetFirstValueWithDefault(const ValueType & defaultValue) const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      return e ? e->_value : defaultValue;
   }

   /** Convenience method.  Returns a pointer to the last value in our iteration list, or NULL if the table is empty. */
   ValueType * GetLastValue() 
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? &e->_value : NULL;
   }

   /** As above, but read-only. */
   const ValueType * GetLastValue() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? &e->_value : NULL;
   }

   /** Convenience method.  Returns a reference to the last value in our iteration list, or a default value object if the table is empty.  */
   const ValueType & GetLastValueWithDefault() const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? e->_value : GetDefaultValue();
   }

   /** Convenience method.  Returns a copy of the last value in our iteration list, or the specified default value object if the table is empty.
     * @param defaultValue The value to return if the table is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defaultValue) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   ValueType GetLastValueWithDefault(const ValueType & defaultValue) const 
   {
      const HashtableEntryBase * e = this->IndexToEntryChecked(_iterTailIdx);
      return e ? e->_value : defaultValue;
   }

   /** Similar to Get(), but on success this method will have the side-effect of moving
     * the returned value to the front of the iteration-ordering-list of this Hashtable.
     * This can be useful when using a Hashtable as an LRU cache.
     * @param key the key to lookup a value for
     * @returns A pointer to the value associated with key on success, or NULL on failure (key-not-found)
     */ 
   ValueType * GetAndMoveToFront(const KeyType & key)
   {
      HashtableEntryBase * e = this->GetEntry(this->ComputeHash(key), key);
      if (e)
      {
         MoveToFrontAux(e);
         return &e->_value;
      }
      else return NULL;
   }

   /** Similar to Get(), but on success this method will have the side-effect of moving
     * the returned value to the front of the iteration-ordering-list of this Hashtable.
     * This can be useful when using a Hashtable as an LRU cache.
     * @param key the key to lookup a value for
     * @returns A pointer to the value associated with key on success, or NULL on failure (key-not-found)
     */ 
   ValueType * GetAndMoveToBack(const KeyType & key)
   {
      HashtableEntryBase * e = this->GetEntry(this->ComputeHash(key), key);
      if (e)
      {
         MoveToBackAux(e);
         return &e->_value;
      }
      else return NULL;
   }

   /** Returns the number of table-slots that we currently have allocated.  Since we often
    *  pre-allocate slots to avoid unnecessary reallocations, this number will usually be
    *  greater than the value returned by GetNumItems().  It will never be less than that value.
    */
   uint32 GetNumAllocatedItemSlots() const {return _tableSize;}

   /** Returns true iff the given key-object has an address that is located within this
     * Hashtable's internal data-array.
     * @param key Reference to a key-object to check the location of.
     * @note This method checks whether the specified key-object is physically located
     *       inside this Hashtable's internal data-array, NOT whether a key/value pair
     *       with a key equivalent to the argument is present in the table.
     *       (i.e. this method is NOT a synonym for the ContainsKey() method!)
     */
   bool IsKeyLocatedInThisContainer(const KeyType & key) {return IsPointerPointingIntoDataTable(&key);}

   /** Returns true iff the given value-object has an address that is located within this
     * Hashtable's internal data-array.
     * @param value Reference to a value-object to check the location of.
     * @note This method checks whether the specified value-object is physically located
     *       inside this Hashtable's internal data-array, NOT whether a key/value pair
     *       with a value equivalent to the argument is present in the table.
     *       (i.e. this method is NOT a synonym for the ContainsValue() method!)
     */
   bool IsValueLocatedInThisContainer(const ValueType & value) {return IsPointerPointingIntoDataTable(&value);}

   /** Returns a reference to a default-constructed Key item.  The reference will remain valid for as long as this Hashtable is valid. */
   const KeyType & GetDefaultKey() const {return GetDefaultObjectForType<KeyType>();}

   /** Returns a reference to a default-constructed Value item.  The reference will remain valid for as long as this Hashtable is valid. */
   const ValueType & GetDefaultValue() const {return GetDefaultObjectForType<ValueType>();}

   /** Returns the number of bytes of memory taken up by this Hashtable and its data and metadata */
   uint32 GetTotalDataSize() const 
   {
      uint32 sizePerItem = 0;
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            sizePerItem = sizeof(HashtableEntry<uint8>);  
         break;
#endif

         case TABLE_INDEX_TYPE_UINT16:
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            sizePerItem = sizeof(HashtableEntry<uint16>); 
         break;
#endif

         default:
            sizePerItem = sizeof(HashtableEntry<uint32>); 
         break;
      }
      return sizeof(*this)+(this->GetNumAllocatedItemSlots()*sizePerItem);
   }

private:
   /** If we don't already have the storage array allocated for this Hashtable, 
     * allocates one.  If the table was already allocated, does nothing and returns B_NO_ERROR.
     * @returns B_NO_ERROR if the table is present, B_OUT_OF_MEMORY on failure.
     */
   status_t EnsureTableAllocated();

   enum {
      HTE_INDEX_BUCKET_PREV = 0, // slot index: for making linked lists in our bucket
      HTE_INDEX_BUCKET_NEXT,     // slot index: for making linked lists in our bucket
      HTE_INDEX_ITER_PREV,       // slot index: for user's table iteration
      HTE_INDEX_ITER_NEXT,       // slot index: for user's table iteration
      HTE_INDEX_MAP_TO,          // slot index
      HTE_INDEX_MAPPED_FROM,     // slot index
      NUM_HTE_INDICES
   };

   enum {
      TABLE_INDEX_TYPE_UINT8 = 0,
      TABLE_INDEX_TYPE_UINT16,
      TABLE_INDEX_TYPE_UINT32,
      NUM_TABLE_INDEX_TYPES
   };

   void SwapContentsAux(HashtableBase & swapMe, bool swapIterators);

   HashtableBase(uint32 tableSize) 
      : _numItems(0)
      , _tableSize(tableSize)
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
      , _tableIndexType(this->ComputeTableIndexTypeForTableSize(tableSize))
#endif
      , _iterHeadIdx(MUSCLE_HASHTABLE_INVALID_SLOT_INDEX)
      , _iterTailIdx(MUSCLE_HASHTABLE_INVALID_SLOT_INDEX)
      , _freeHeadIdx(MUSCLE_HASHTABLE_INVALID_SLOT_INDEX)
      , _table(NULL)
      , _iterList(NULL) {/* empty */}

   ~HashtableBase() {Clear(true);}

   void CopyFromAux(const HashtableBase & rhs)
   {
      const bool wasEmpty = IsEmpty();
      const HashtableEntryBase * e = rhs.IndexToEntryChecked(rhs._iterHeadIdx);  // start of linked list to iterate through
      while(e)
      {
         HashtableEntryBase * myEntry = wasEmpty ? NULL : this->GetEntry(e->_hash, e->_key);
         if (myEntry) myEntry->_value = e->_value;
         else
         { 
            this->InsertIterationEntry(PutAuxAux(e->_hash, e->_key, e->_value), this->IndexToEntryChecked(_iterTailIdx));
            this->_numItems++;
         }
         e = rhs.GetEntryIterNextChecked(e);
      }
   }

   /** Returns the TABLE_INDEX_TYPE_* value associated with our current table-size */
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
   int GetTableIndexType() const {return _tableIndexType;}
#else
   int GetTableIndexType() const {return this->ComputeTableIndexTypeForTableSize(_tableSize);}
#endif

   static uint32 ComputeTableIndexTypeForTableSize(uint32 tableSize) {return (tableSize>=255) + (tableSize>=65535);}

   /** This class is an implementation detail, please ignore it.  Do not access it directly. */
   class HashtableEntryBase
   {
   protected:
      // Note:  All member variables are initialized by CreateEntriesArray(), not by the ctor!
      HashtableEntryBase()  {/* empty */}
      ~HashtableEntryBase() {/* empty */}

   public:
      uint32 _hash;       // precalculated for efficiency
      KeyType _key;       // used for '==' checking
      ValueType _value;   // payload
   };
   uint32 PopFromFreeList(HashtableEntryBase * e, uint32 freeHeadIdx);

   /** This class is an implementation detail, please ignore it.  Do not access it directly. */
   template <class IndexType> class HashtableEntry MUSCLE_FINAL_CLASS : public HashtableEntryBase
   {
   public:
      // Note:  All member variables are initialized by CreateEntriesArray(), not by the ctor!
      HashtableEntry()  {/* empty */}
      ~HashtableEntry() {/* empty */}

      /** Returns this entry to the free-list, and resets its key and value to their default values. */
      void PushToFreeList(const KeyType & defaultKey, const ValueType & defaultValue, uint32 & getRetFreeHeadIdx, HashtableBase * table)
      {
         this->_indices[HTE_INDEX_ITER_PREV]   = this->_indices[HTE_INDEX_ITER_NEXT] = this->_indices[HTE_INDEX_BUCKET_PREV] = (IndexType)-1;
         this->_indices[HTE_INDEX_BUCKET_NEXT] = (IndexType) getRetFreeHeadIdx;

         const uint32 thisIdx = table->EntryToIndexUnchecked(this);
         if (getRetFreeHeadIdx != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) static_cast<HashtableEntry *>(table->IndexToEntryUnchecked(getRetFreeHeadIdx))->_indices[HTE_INDEX_BUCKET_PREV] = (IndexType) thisIdx;
         getRetFreeHeadIdx = thisIdx;

         this->_hash = MUSCLE_HASHTABLE_INVALID_HASH_CODE;
         if (this->IsPerKeyClearNecessary())   this->_key   = defaultKey;    // NOTE:  These lines could have side-effects due to code in the templatized
         if (this->IsPerValueClearNecessary()) this->_value = defaultValue;  //        classes!  So it's important that the Hashtable be in a consistent state here
      }

      /** Removes this entry from the free list, so that we are ready for use.
        * @param freeHeadIdx Index of the current head of the free list
        * @param table Pointer to the first entry in the HashtableEntry array
        * @returns the index of the new head of the free list
        */
      uint32 PopFromFreeList(uint32 freeHeadIdx, HashtableBase * table) 
      {
         HashtableEntry * h = static_cast<HashtableEntry *>(table->GetEntriesArrayPointer());
         IndexType & myBucketNext = this->_indices[HTE_INDEX_BUCKET_NEXT];
         IndexType & myBucketPrev = this->_indices[HTE_INDEX_BUCKET_PREV];
         if (myBucketNext != (IndexType)-1) h[myBucketNext]._indices[HTE_INDEX_BUCKET_PREV] = myBucketPrev;
         if (myBucketPrev != (IndexType)-1) h[myBucketPrev]._indices[HTE_INDEX_BUCKET_NEXT] = myBucketNext;
         const uint32 ret = (freeHeadIdx == table->EntryToIndexUnchecked(this)) ? ((myBucketNext == (IndexType)-1) ? MUSCLE_HASHTABLE_INVALID_SLOT_INDEX : myBucketNext) : freeHeadIdx;
         myBucketPrev = myBucketNext = (IndexType)-1;
         return ret;
      }

      /** Allocates and returns an array if (size) HashtableEnty objects. */
      static HashtableEntryBase * CreateEntriesArray(uint32 size)
      {
         HashtableEntry * ret = newnothrow_array(HashtableEntry,size);
         if (ret)
         {
            for (uint32 i=0; i<size; i++) 
            {
               HashtableEntry * e = &ret[i];
               e->_hash                           = MUSCLE_HASHTABLE_INVALID_HASH_CODE;
               e->_indices[HTE_INDEX_BUCKET_PREV] = (IndexType)(i-1);  // yes, _bucketPrev will be set to (IndexType)-1 when (i==0)
               e->_indices[HTE_INDEX_BUCKET_NEXT] = (IndexType)(i+1);
               e->_indices[HTE_INDEX_ITER_PREV]   = e->_indices[HTE_INDEX_ITER_NEXT]   = (IndexType)-1;
               e->_indices[HTE_INDEX_MAP_TO]      = e->_indices[HTE_INDEX_MAPPED_FROM] = (IndexType)i;
            }
            ret[size-1]._indices[HTE_INDEX_BUCKET_NEXT] = (IndexType)-1;
         }
         else MWARN_OUT_OF_MEMORY;
         return ret;
      }

      /** Returns true iff we need to set our KeyType objects to their default-constructed state when we're done using them */
      inline bool IsPerKeyClearNecessary() const
      {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
         return !std::is_trivial<KeyType>::value;
#else
         return true;
#endif
      }

      /** Returns true iff we need to set our ValueType objects to their default-constructed state when we're done using them */
      inline bool IsPerValueClearNecessary() const
      {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
         return !std::is_trivial<ValueType>::value;
#else
         return true;
#endif
      }
  
      /** Returns true iff the given ValueType pointer is located within our allocated array */
      static bool IsPointerPointingIntoDataTable(const HashtableBase * table, const void * ptr)
      {
         const uint32 numSlots = table->GetNumAllocatedItemSlots();
         if ((numSlots == 0)||(ptr == NULL)) return false;

         const HashtableEntry * h = static_cast<const HashtableEntry *>(table->GetEntriesArrayPointer());
         const void * first       = &h[0];
         const void * afterLast   = &h[numSlots];
         return ((ptr >= first)&&(ptr < afterLast));
      }

      IndexType _indices[NUM_HTE_INDICES];
   };

   uint32 GetEntryIndexValue(const HashtableEntryBase * entry, uint32 whichIndex) const
   {
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            {const uint8  r = (static_cast<const HashtableEntry<uint8>  *>(entry))->_indices[whichIndex]; return (r==(uint8)-1)?MUSCLE_HASHTABLE_INVALID_SLOT_INDEX:((uint32)r);}
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            {const uint16 r = (static_cast<const HashtableEntry<uint16> *>(entry))->_indices[whichIndex]; return (r==(uint16)-1)?MUSCLE_HASHTABLE_INVALID_SLOT_INDEX:((uint32)r);}
#endif

         default:
            {const uint32 r = (static_cast<const HashtableEntry<uint32> *>(entry))->_indices[whichIndex]; return r;}
      }
   }

   void SetEntryIndexValue(HashtableEntryBase * entry, uint32 whichIndex, uint32 value) const
   {
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            static_cast<HashtableEntry<uint8>  *>(entry)->_indices[whichIndex] = (value == MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) ? (uint8)255    : ((uint8)  value); 
         break;
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            static_cast<HashtableEntry<uint16> *>(entry)->_indices[whichIndex] = (value == MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) ? (uint16)65535 : ((uint16) value); 
         break;
#endif

         default: 
            static_cast<HashtableEntry<uint32> *>(entry)->_indices[whichIndex] = (uint32) value; 
         break;
      }
   }

   // Convenience methods (these check the requested value, and return NULL if it's not valid)
   HashtableEntryBase * GetEntryBucketNextChecked(const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT));}
   HashtableEntryBase * GetEntryBucketPrevChecked(const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV));}
   HashtableEntryBase * GetEntryIterNextChecked(  const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_ITER_NEXT));}
   HashtableEntryBase * GetEntryIterPrevChecked(  const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_ITER_PREV));}
   HashtableEntryBase * GetEntryMapToChecked(     const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_MAP_TO));}
   HashtableEntryBase * GetEntryMappedFromChecked(const HashtableEntryBase * e) const {return this->IndexToEntryChecked(this->GetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM));}

   // Convenience methods (these assume the requested value will be valid -- be careful, as calling them when it isn't valid will mess things up badly!) 
   HashtableEntryBase * GetEntryBucketNextUnchecked(const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT));}
   HashtableEntryBase * GetEntryBucketPrevUnchecked(const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV));}
   HashtableEntryBase * GetEntryIterNextUnchecked(  const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_ITER_NEXT));}
   HashtableEntryBase * GetEntryIterPrevUnchecked(  const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_ITER_PREV));}
   HashtableEntryBase * GetEntryMapToUnchecked(     const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_MAP_TO));}
   HashtableEntryBase * GetEntryMappedFromUnchecked(const HashtableEntryBase * e) const {return this->IndexToEntryUnchecked(this->GetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM));}

   // Convenience methods (these methods return the index value as a uint32) 
   uint32 GetEntryBucketNext(const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT);}
   uint32 GetEntryBucketPrev(const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV);}
   uint32 GetEntryIterNext(  const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_ITER_NEXT);}
   uint32 GetEntryIterPrev(  const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_ITER_PREV);}
   uint32 GetEntryMapTo(     const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_MAP_TO);}
   uint32 GetEntryMappedFrom(const HashtableEntryBase * e) const {return this->GetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM);}

   // Convenience methods (these check the requested value, and set the field to MUSCLE_HASHTABLE_INVALID_SLOT_INDEX if it isn't valid)
   void SetEntryBucketNextChecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT, this->EntryToIndexChecked(v));}
   void SetEntryBucketPrevChecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV, this->EntryToIndexChecked(v));}
   void SetEntryIterNextChecked(  HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_NEXT,   this->EntryToIndexChecked(v));}
   void SetEntryIterPrevChecked(  HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_PREV,   this->EntryToIndexChecked(v));}
   void SetEntryMapToChecked(     HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_MAP_TO,      this->EntryToIndexChecked(v));}
   void SetEntryMappedFromChecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->GetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM, this->EntryToIndexChecked(v));}

   // Convenience methods (these assume the requested value will be valid -- be careful, as calling them when it isn't valid will mess things up badly!) 
   void SetEntryBucketNextUnchecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT, this->EntryToIndexUnchecked(v));}
   void SetEntryBucketPrevUnchecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV, this->EntryToIndexUnchecked(v));}
   void SetEntryIterNextUnchecked(  HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_NEXT,   this->EntryToIndexUnchecked(v));}
   void SetEntryIterPrevUnchecked(  HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_PREV,   this->EntryToIndexUnchecked(v));}
   void SetEntryMapToUnchecked(     HashtableEntryBase * e, const HashtableEntryBase * v) {this->SetEntryIndexValue(e, HTE_INDEX_MAP_TO,      this->EntryToIndexUnchecked(v));}
   void SetEntryMappedFromUnchecked(HashtableEntryBase * e, const HashtableEntryBase * v) {this->GetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM, this->EntryToIndexUnchecked(v));}

   // Convenience methods
   void SetEntryBucketNext(HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_NEXT, idx);}
   void SetEntryBucketPrev(HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_BUCKET_PREV, idx);}
   void SetEntryIterNext(  HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_NEXT,   idx);}
   void SetEntryIterPrev(  HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_ITER_PREV,   idx);}
   void SetEntryMapTo(     HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_MAP_TO,      idx);}
   void SetEntryMappedFrom(HashtableEntryBase * e, uint32 idx) {this->SetEntryIndexValue(e, HTE_INDEX_MAPPED_FROM, idx);}
   
   uint32 EntryToIndexUnchecked(const HashtableEntryBase * entry) const
   {
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            return ((uint32)(static_cast<const HashtableEntry<uint8>  *>(entry)-static_cast<const HashtableEntry<uint8>  *>(_table)));
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            return ((uint32)(static_cast<const HashtableEntry<uint16> *>(entry)-static_cast<const HashtableEntry<uint16> *>(_table)));
#endif

         default:
            return ((uint32)(static_cast<const HashtableEntry<uint32> *>(entry)-static_cast<const HashtableEntry<uint32> *>(_table)));
      }
   }
   uint32 EntryToIndexChecked(const HashtableEntryBase * optEntry) const {return optEntry ? EntryToIndexUnchecked(optEntry) : MUSCLE_HASHTABLE_INVALID_SLOT_INDEX;}

   HashtableEntryBase * IndexToEntryUnchecked(uint32 idx) const
   {
      HashtableEntryBase * t = const_cast<HashtableEntryBase *>(_table);
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            return &(static_cast<HashtableEntry<uint8>  *>(t))[idx];
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            return &(static_cast<HashtableEntry<uint16> *>(t))[idx];
#endif

         default:
            return &(static_cast<HashtableEntry<uint32> *>(t))[idx];
      }
   }
   HashtableEntryBase * IndexToEntryChecked(uint32 idx) const {return (idx == MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) ? NULL : IndexToEntryUnchecked(idx);}

   friend class HashtableIterator<KeyType, ValueType, HashFunctorType>;

   void InitializeIterator(IteratorType & iter) const
   {
      RegisterIterator(&iter);
      iter._iterCookie = this->IndexToEntryChecked((iter._flags & HTIT_FLAG_BACKWARDS) ? _iterTailIdx : _iterHeadIdx);
      iter.UpdateKeyAndValuePointers();
   }

   void InitializeIteratorAt(IteratorType & iter, const KeyType & startAt) const
   {
      RegisterIterator(&iter);
      iter._iterCookie = this->GetEntry(this->ComputeHash(startAt), startAt);
      iter.UpdateKeyAndValuePointers();
   }

   // These ugly methods are here to expose the HashtableIterator's privates to the Hashtable class without exposing them to the rest of the world
   void * GetIteratorScratchSpace(const IteratorType & iter) const {return iter._scratchSpace;}
   void SetIteratorScratchSpace(IteratorType & iter, void * val) const {iter._scratchSpace = val;}
   void * GetIteratorNextCookie(const IteratorType & iter) const {return iter._iterCookie;}
   void SetIteratorNextCookie(IteratorType & iter, void * val) const {iter._iterCookie = val;}
   void UpdateIteratorKeyAndValuePointers(IteratorType & iter) const {iter.UpdateKeyAndValuePointers();}
   IteratorType * GetIteratorNextIterator(const IteratorType & iter) const {return iter._nextIter;}

   // Give these classes (and only these classes!) access to the HashtableEntryBase inner class
   template<class HisKeyType, class HisValueType, class HisHashFunctorType, class HisSubclassType>          friend class HashtableMid;
   template<class HisKeyType, class HisValueType, class HisHashFunctorType>                                 friend class Hashtable;
   template<class HisKeyType, class HisValueType, class HisHashFunctorType, class HisEntryCompareFunctorType, class HisSubclassType> friend class OrderedHashtable;
   template<class HisKeyType, class HisValueType, class HisKeyCompareFunctorType, class HisHashFunctorType> friend class OrderedKeysHashtable;
   template<class HisKeyType, class HisValueType, class HisKeyCompareFunctorType, class HisHashFunctorType> friend class OrderedValuesHashtable;

   HashtableEntryBase * GetEntriesArrayPointer() const {return _table;}
   HT_UniversalSinkKeyValueRef HashtableEntryBase * PutAuxAux(uint32 hash, HT_SinkKeyParam key, HT_SinkValueParam value);
   status_t RemoveAux(uint32 hash, const KeyType & key, ValueType * optSetValue)
   {
      HashtableEntryBase * e = this->GetEntry(hash, key);
      return e ? RemoveEntry(e, optSetValue) : B_DATA_NOT_FOUND;
   }
   status_t RemoveAux(const KeyType & key, ValueType * optSetValue)
   {
      HashtableEntryBase * e = this->GetEntry(this->ComputeHash(key), key);
      return e ? RemoveEntry(e, optSetValue) : B_DATA_NOT_FOUND;
   }
   status_t RemoveEntry(HashtableEntryBase * e, ValueType * optSetValue);
   status_t RemoveEntryByIndex(uint32 idx, ValueType * optSetValue);

   void SwapEntryMaps(uint32 idx1, uint32 idx2);

   inline uint32 ComputeHash(const KeyType & key) const
   {
      const uint32 ret = GetHashFunctor()(key);
      return (ret == MUSCLE_HASHTABLE_INVALID_HASH_CODE) ? (ret+1) : ret;  // avoid using the guard value as a hash code (unlikely but possible)
   }

   inline bool AreKeysEqual(const KeyType & k1, const KeyType & k2) const
   {
      return GetHashFunctor().AreKeysEqual(k1, k2);
   }

   void InsertIterationEntry(HashtableEntryBase * e, HashtableEntryBase * optBehindThisOne);
   void RemoveIterationEntry(HashtableEntryBase * e);
   HashtableEntryBase * GetEntry(uint32 hash, const KeyType & key) const;
   HashtableEntryBase * GetEntryAt(uint32 idx) const;
   bool IsBucketHead(const HashtableEntryBase * e) const 
   {
      if (e->_hash == MUSCLE_HASHTABLE_INVALID_HASH_CODE) return false;
      return (GetEntryMapToUnchecked(IndexToEntryUnchecked(e->_hash%_tableSize)) == e);
   }

   // HashtableIterator's private API
   void RegisterIterator(IteratorType * iter) const
   {
      if (iter->_flags & HTIT_FLAG_NOREGISTER) iter->_prevIter = iter->_nextIter = NULL;
      else
      {
#ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
         // This logic keeps iterator-registration from causing race conditions when multiple
         // threads are iterating over the same Hashtable at once.  Only the first thread is 
         // allowed to register; subsequent threads' iterators are forced into non-registering mode.
         // Of course that means they won't handle modifications to the Hashtable properly, but
         // then again if modifications are being made during multi-thread read-access periods,
         // you're pretty much screwed anyway.  This is just so that iteration (which is nominally
         // a read-only operation) can be thread-safe even if the user didn't explicitly
         // specify HTIT_FLAG_NOREGISTER.
         if (_iteratorCount.AtomicIncrement())
         {
            _iteratorThreadID = muscle_thread_id::GetCurrentThreadID();  // we're the first iterator on this Hashtable!
            iter->_okayToUnsetThreadID = true;  // remember that (iter) is the iterator who has to unset _iteratorThreadID
         }
         else if (_iteratorThreadID != muscle_thread_id::GetCurrentThreadID())  // there's a race condition here but it's harmless
         {
            // If we got here, then we're in a different thread from the one that has permission
            // to register iterators.  So for thread-safety, we're going to refrain from registering (iter).
            iter->_flags |= HTIT_FLAG_NOREGISTER;  // so that UnregisterIterator() will also be a no-op for (iter)
            iter->_prevIter = iter->_nextIter = NULL;
            (void) _iteratorCount.AtomicDecrement();  // roll back our AtomicIncrement(), since we aren't registered
            return;
         }
#endif

         // add (iter) to the head of our linked list of iterators
         iter->_prevIter = NULL;
         iter->_nextIter = _iterList;  
         if (_iterList) _iterList->_prevIter = iter;
         _iterList = iter;
      }
   }

   void UnregisterIterator(IteratorType * iter) const
   {
      if (iter->_flags & HTIT_FLAG_NOREGISTER) iter->_prevIter = iter->_nextIter = NULL;
      else
      {
         if (iter->_prevIter) iter->_prevIter->_nextIter = iter->_nextIter;
         if (iter->_nextIter) iter->_nextIter->_prevIter = iter->_prevIter;
         if (iter == _iterList) _iterList = iter->_nextIter;
         iter->_prevIter = iter->_nextIter = NULL;

#ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
         if (iter->_okayToUnsetThreadID) {iter->_okayToUnsetThreadID = false; _iteratorThreadID = muscle_thread_id();}
         (void) _iteratorCount.AtomicDecrement();
#endif
      }
   }

   const KeyType & GetKeyFromCookie(void * c) const {return ((static_cast<const HashtableEntryBase *>(c))->_key);}
   ValueType & GetValueFromCookie(  void * c) const {return ((static_cast<HashtableEntryBase *>(c))->_value);}

   HashtableEntryBase * GetSubsequentEntry(void * entryPtr, uint32 flags) const 
   {
      if (entryPtr == NULL) return NULL;
      HashtableEntryBase * b = static_cast<HashtableEntryBase *>(entryPtr);
      return (flags & HTIT_FLAG_BACKWARDS) ? this->GetEntryIterPrevChecked(b) : this->GetEntryIterNextChecked(b);
   }

   void MoveToBackAux(HashtableEntryBase * moveMe)
   {
      if (this->GetEntryIterNext(moveMe) != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX)
      {
         RemoveIterationEntry(moveMe);
         InsertIterationEntry(moveMe, this->IndexToEntryChecked(_iterTailIdx));
      }
   }

   void MoveToFrontAux(HashtableEntryBase * moveMe)
   {
      if (this->GetEntryIterPrev(moveMe) != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX)
      {
         RemoveIterationEntry(moveMe);
         InsertIterationEntry(moveMe, NULL);
      }
   }

   void MoveToBeforeAux(HashtableEntryBase * moveMe, HashtableEntryBase * toBeforeMe)
   {
      if (this->GetEntryIterNextChecked(moveMe) != toBeforeMe)
      {
         RemoveIterationEntry(moveMe);
         InsertIterationEntry(moveMe, GetEntryIterPrevChecked(toBeforeMe));
      }
   }

   void MoveToBehindAux(HashtableEntryBase * moveMe, HashtableEntryBase * toBehindMe)
   {
      if (GetEntryIterPrevChecked(moveMe) != toBehindMe)
      {
         RemoveIterationEntry(moveMe);
         InsertIterationEntry(moveMe, toBehindMe);
      }
   }

   void MoveToPositionAux(HashtableEntryBase * moveMe, uint32 idx)
   {
           if (idx == 0)             this->MoveToFrontAux(moveMe);
      else if (idx >= GetNumItems()) this->MoveToBackAux(moveMe);
      else 
      {
         RemoveIterationEntry(moveMe);

         HashtableEntryBase * insertAfter;
         if (idx < GetNumItems()/2)
         {
            insertAfter = this->IndexToEntryChecked(_iterHeadIdx);
            while(--idx > 0) insertAfter = this->GetEntryIterNextUnchecked(insertAfter);
         }
         else
         {
            insertAfter = this->IndexToEntryChecked(_iterTailIdx);
            while(++idx < GetNumItems()) insertAfter = this->GetEntryIterPrevUnchecked(insertAfter);
         }
         InsertIterationEntry(moveMe, insertAfter);
      }
   }

   const KeyType * GetKeyWithValueAux(const ValueType & value, bool backwards) const
   {
      for (IteratorType iter(*this, HTIT_FLAG_NOREGISTER|(backwards?HTIT_FLAG_NOREGISTER:0)); iter.HasData(); iter++)
         if (iter.GetValue() == value) return &iter.GetKey();
      return NULL;
   }

   bool IsPointerPointingIntoDataTable(const void * ptr) const;

#if !defined(__clang__) && defined(__GNUC__)
public:  // work-around for an apparent bug in g++ -- friend-template doesn't work for the Ordered*Hashtable constructors!?
#endif
   template<class KeyCompareFunctor> class ByKeyEntryCompareFunctor
   {
   public:
      ByKeyEntryCompareFunctor(const KeyCompareFunctor & kf) : _kf(kf) {}
      int Compare(const HashtableEntryBase & e1, const HashtableEntryBase & e2, void * cookie) const {return _kf.Compare(e1._key, e2._key, cookie);}

   private:
      const KeyCompareFunctor & _kf;
   };

   template<class ValueCompareFunctor> class ByValueEntryCompareFunctor
   {
   public:
      ByValueEntryCompareFunctor(const ValueCompareFunctor & vf) : _vf(vf) {}
      int Compare(const HashtableEntryBase & e1, const HashtableEntryBase & e2, void * cookie) const {return _vf.Compare(e1._value, e2._value, cookie);}

   private:
      const ValueCompareFunctor & _vf;
   };

private:
   template <class EntryCompareFunctorType> void SortByEntry(                        const EntryCompareFunctorType & ecf, void * cookie);
   template <class EntryCompareFunctorType> void InsertIterationEntryInOrder(        const EntryCompareFunctorType & ecf, HashtableEntryBase * e, bool isAutoSortEnabled, void * compareCookie);
   template <class EntryCompareFunctorType> void MoveIterationEntryToCorrectPosition(const EntryCompareFunctorType & ecf, HashtableEntryBase * e, void * compareCookie);

   const HashFunctorType & GetHashFunctor() const {return GetDefaultObjectForType<HashFunctorType>();}  // used to compute hash codes for key objects

   uint32 _numItems;       // the number of valid elements in the hashtable
   uint32 _tableSize;      // the number of entries in _table (or the number to allocate if _table is NULL)
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
   uint32 _tableIndexType; // will be a TABLE_INDEX_TYPE_* value
#endif
   uint32 _iterHeadIdx;    // index of the start of linked list to iterate through
   uint32 _iterTailIdx;    // index of the end of linked list to iterate through
   uint32 _freeHeadIdx;    // index of the head of the list of unused HashtableEntries in our _table array

   HashtableEntryBase * _table; // our array of table entries (actually an array of HashtableEntry objects of some flavor: NOT an array of HashtableEntryBase objects!  Be careful!)

   mutable IteratorType * _iterList;  // list of existing iterators for this table
#ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
   mutable muscle_thread_id _iteratorThreadID; // this is the ID of the thread that is allowed to register iterators (or 0 if none are registered)
   mutable AtomicCounter _iteratorCount;       // this represents the number of HashtableIterators currently registered with this Hashtable
#endif
};

/** This class should not be instantiated directly.  Instantiate a Hashtable, OrderedKeysHashtable, or OrderedValuesHashtable instead. */
template <class KeyType, class ValueType, class HashFunctorType, class SubclassType> class HashtableMid : public HashtableBase<KeyType, ValueType, HashFunctorType>
{
public:
   /** The iterator type that goes with this HashtableMid type */
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** Equality operator.  Returns true iff both hash tables contains the same set of keys and values.
     * Note that the ordering of the keys is NOT taken into account!
     * @param rhs the Hashtable to compare against
     */
   bool operator== (const HashtableMid & rhs) const {return this->IsEqualTo(rhs, false);}

   /** Templated psuedo-equality operator.  Returns true iff both hash tables contains the same set of keys and values.
     * Note that the ordering of the keys is NOT taken into account!
     * @param rhs the Hashtable to compare against
     */
   template<class RHSHashFunctorType,class RHSSubclassType> bool operator== (const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) const {return this->IsEqualTo(rhs, false);}

   /** Returns true iff the contents of this table differ from the contents of (rhs).
     * Note that the ordering of the keys is NOT taken into account!
     * @param rhs the Hashtable to compare against
     */
   bool operator!= (const HashtableMid & rhs) const {return !this->IsEqualTo(rhs, false);}

   /** Templated psuedo-inequality operator.  Returns false iff both hash tables contains the same set of keys and values.
     * Note that the ordering of the keys is NOT taken into account!
     * @param rhs the Hashtable to compare against
     */
   template<class RHSHashFunctorType,class RHSSubclassType> bool operator!= (const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) const {return !this->IsEqualTo(rhs, false);}

   /** Makes this table into a copy of a table passed in as an argument.
     * @param rhs The HashtableMid to make this HashtableMid a copy of.  Note that only (rhs)'s items are
     *            copied in; other settings such as sort mode and key/value cookies are not copied in.
     * @param clearFirst If true, this HashtableMid will be cleared before the contents of (rhs) are copied
     *                   into it.  Otherwise, the contents of (rhs) will be copied in on top of the current contents.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   template<class RHSHashFunctorType> status_t CopyFrom(const HashtableBase<KeyType,ValueType,RHSHashFunctorType> & rhs, bool clearFirst = true);

   /** Places the given (key, value) mapping into the table.  Any previous entry with a key of (key) will be replaced.  
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time for
    *  keys that are not already in the table)
    *  @param key The key that the new value is to be associated with.
    *  @param value The value to associate with the new key.
    *  @param setPreviousValue If there was a previously existing value associated with (key), it will be copied into this object.
    *  @param optSetReplaced If set non-NULL, this boolean will be set to true if (setPreviousValue) was written into, false otherwise.
    *  @return B_NO_ERROR If the operation succeeded, B_OUT_OF_MEMORY if it failed.
    */
   HT_UniversalSinkKeyValueRef status_t Put(HT_SinkKeyParam key, HT_SinkValueParam value, ValueType & setPreviousValue, bool * optSetReplaced = NULL) {return (PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(value), &setPreviousValue, optSetReplaced) != NULL) ? B_NO_ERROR : B_OUT_OF_MEMORY;}

   /** Places the given (key, value) mapping into the table.  Any previous entry with a key of (key) will be replaced. 
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time for
    *  keys that are not already in the table)
    *  @param key The key that the new value is to be associated with.
    *  @param value The value to associate with the new key.
    *  @return B_NO_ERROR If the operation succeeded, B_OUT_OF_MEMORY if it failed.
    */
   HT_UniversalSinkKeyValueRef status_t Put(HT_SinkKeyParam key, HT_SinkValueParam value) {return (PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(value), NULL, NULL) != NULL) ? B_NO_ERROR : B_OUT_OF_MEMORY;}

   /** Places a (key, value) mapping into the table.  The value will be a default-constructed item of the value type.
    *  This call is equivalent to calling Put(key, ValueType()), but slightly more efficient.
    *  @param key The key to be used in the placed mapping.
    *  @return B_NO_ERROR If the operation succeeded, B_OUT_OF_MEMORY if it failed.
    */
   HT_UniversalSinkKeyRef status_t PutWithDefault(HT_SinkKeyParam key) {return Put(HT_ForwardKey(key), this->GetDefaultValue());}

   /** Convenience method:  For each key/value pair in the passed-in-table, Put()'s that
     * key/value pair into this table.  Any existing items in this table with the same
     * key as any in (pairs) will be overwritten.
     * @param pairs A table full of items to Put() into this table.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   template<class RHSHashFunctorType, class RHSSubclassType> status_t Put(const HashtableMid<KeyType, ValueType, RHSHashFunctorType, RHSSubclassType> & pairs) {return this->CopyFrom(pairs, false);}

   /** Same as Put(), except this call also makes sure that on success, (key) will
     * be located at the front of the table's iteration-sequence.
     * @param key Key of the item to be placed at the front of the sequence.
     * @param v Value of the item to be associated with (key).
     * @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   HT_UniversalSinkKeyValueRef status_t PutAtFront(HT_SinkKeyParam key, HT_SinkValueParam v);

   /** Same as Put(), except this call also makes sure that on success, (key) will
     * be located at the end of the table's iteration-sequence.
     * @param key Key of the item to be placed at the end of the sequence.
     * @param v Value of the item to be associated with (key).
     */
   HT_UniversalSinkKeyValueRef status_t PutAtBack(HT_SinkKeyParam key, HT_SinkValueParam v);

   /** Same as Put(), except this call also makes sure that on success, (key) will
     * be located just before (placeBeforeMe) in the table's iteration-sequence.
     * @param key Key of the item to be placed into th table.
     * @param placeBeforeMe Key of the item that (key) should be placed in front of.
     *                      If (placeBeforeMe==key), or no key equal to (placeBeforeMe) 
     *                      currently exists in the table, then this method will act the 
     *                      same as a call to Put().
     * @param v Value of the item to be associated with (key).
     * @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   HT_UniversalSinkKeyValueRef status_t PutBefore(HT_SinkKeyParam key, HT_SinkKeyParam placeBeforeMe, HT_SinkValueParam v);

   /** Same as Put(), except this call also makes sure that on success, (key) will
     * be located just behind (placeBehindMe) in the table's iteration-sequence.
     * @param key Key of the item to be placed into th table.
     * @param placeBehindMe Key of the item that (key) should be placed in front of.
     *                      If (placeBehindMe==key), or no key equal to (placeBehindMe) 
     *                      currently exists in the table, then this method will act the 
     *                      same as a call to Put().
     * @param v Value of the item to be associated with (key).
     * @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   HT_UniversalSinkKeyValueRef status_t PutBehind(HT_SinkKeyParam key, HT_SinkKeyParam placeBehindMe, HT_SinkValueParam v);

   /** Places the given value at the (nth) position in the HashtableIterator traversal sequence.
     * Note that this is an O(N) operation.
     * @param key Key of the item to be placed.
     * @param atPosition The position that this item should be placed at.  Zero would place
     *                   the item at the head of the traversal sequence, one to the second
     *                   position, and so on.  Values greater than or equal to the number
     *                   of items in the Hashtable will place the item at the last position.
     * @param v Value of the item to be associated with (key).
     * @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   HT_UniversalSinkKeyValueRef status_t PutAtPosition(HT_SinkKeyParam key, uint32 atPosition, HT_SinkValueParam v);

   /** Convenience method -- returns a pointer to the value specified by (key),
    *  or if no such value exists, it will Put() a (key,value) pair in the HashtableMid,
    *  and then return a pointer to the newly-placed value.  Returns NULL on
    *  error only (out of memory?)
    *  @param key The key to look for a value with
    *  @param defaultValue The value to auto-place in the HashtableMid if (key) isn't found.
    *  @returns a Pointer to the retrieved or placed value.
    */
   HT_UniversalSinkKeyValueRef ValueType * GetOrPut(HT_SinkKeyParam key, HT_SinkValueParam defaultValue)
   {
      const uint32 hash = this->ComputeHash(key);
      HashtableEntryBaseType * e = this->GetEntry(hash, key);
      if (e == NULL) e = PutAux(hash, HT_ForwardKey(key), HT_ForwardValue(defaultValue), NULL, NULL);
      return e ? &e->_value : NULL;
   }

   /** Convenience method -- returns a pointer to the value specified by (key),
    *  or if no such value exists, it will Put() a (key,value) pair in the HashtableMid,
    *  using a default value (as specified by ValueType's default constructor)
    * and then return a pointer to the newly-placed value.  Returns NULL on
    *  error only (out of memory?)
    *  @param key The key to look for a value with
    *  @returns a Pointer to the retrieved or placed value.
    */
   HT_UniversalSinkKeyRef ValueType * GetOrPut(HT_KeyParam key)
   {
      const uint32 hash = this->ComputeHash(key);
      HashtableEntryBaseType * e = this->GetEntry(hash, key);
      if (e == NULL) e = PutAux(hash, HT_ForwardKey(key), this->GetDefaultValue(), NULL, NULL);
      return e ? &e->_value : NULL;
   }

   /** Places the given (key, value) mapping into the table.  Any previous entry with a key of (key) will be replaced. 
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time for
    *  keys that are not already in the table)
    *  @param key The key that the new value is to be associated with.
    *  @param value The value to associate with the new key.  If not specified, a value object created using
    *               the default constructor will be placed by default.
    *  @return A pointer to the value object in the table on success, or NULL on failure (out of memory?)
    */
   HT_UniversalSinkKeyValueRef ValueType * PutAndGet(HT_SinkKeyParam key, HT_SinkValueParam value) 
   { 
      HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(value), NULL, NULL);
      return e ? &e->_value : NULL;
   }

   /** As above, except that a default value is placed into the table and returned. 
     * @param key The key that the new value is to be associated with.
     * @return A pointer to the value object in the table on success, or NULL on failure (out of memory?)
     */
   HT_UniversalSinkKeyRef ValueType * PutAndGet(HT_SinkKeyParam key) {return PutAndGet(HT_ForwardKey(key), this->GetDefaultValue());}

   /** Places the given (key, value) mapping into the table.  Any previous entry with a key of (key) will be replaced. 
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time for
    *  keys that are not already in the table)
    *  @param key The key that the new value is to be associated with.
    *  @param value The value to associate with the new key.  If not specified, a value object created using
    *               the default constructor will be placed by default.
    *  @return A pointer to the key object in the table on success, or NULL on failure (out of memory?)
    */
   HT_UniversalSinkKeyValueRef const KeyType * PutAndGetKey(HT_SinkKeyParam key, HT_SinkValueParam value) 
   { 
      HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(value), NULL, NULL);
      return e ? &e->_key : NULL;
   }

   /** As above, except that a default value is placed into the table and returned. 
     * @param key The key that the new value is to be associated with.
     * @return A pointer to the key object in the table on success, or NULL on failure (out of memory?)
     */
   HT_UniversalSinkKeyRef const KeyType * PutAndGetKey(HT_SinkKeyParam key) {return PutAndGetKey(HT_ForwardKey(key), this->GetDefaultValue());}

   /** Convenience method:  If (value) is the different from (defaultValue), then (key/value) is placed into the table and a pointer
     *                      to the placed value object is returned.
     *                      If (value) is equal to (defaultValue), on the other hand, (key) will be removed from the table, and NULL will be returned.
     * @param key The key value to affect.
     * @param value The value to possibly place into the table.
     * @param defaultValue The value to compare (value) with to decide whether to Put() or Remove() the key.
     * @returns B_NO_ERROR on success (i.e. if the table has been updated to the appropriate state) or an error code on failure (out of memory?)
     */
   HT_UniversalSinkKeyValueRef status_t PutOrRemove(HT_SinkKeyParam key, HT_SinkValueParam value, const ValueType & defaultValue)  // yes, defaultValue is not declared as HT_SinkValueParam!
   {
      if (value == defaultValue)
      {
         const status_t ret = this->Remove(HT_ForwardKey(key));
         return (ret == B_DATA_NOT_FOUND) ? B_NO_ERROR : ret;  // we count removing a key that isn't present as a successful operation, since the table is in the expected state
      }
      else return Put(HT_ForwardKey(key), HT_ForwardValue(value));
   }

   /** As above, except no (defaultValue) is specified.  The default-constructed ValueType is assumed.
     * @param key The key value to affect.
     * @param value The value to possibly place into the table.
     * @returns B_NO_ERROR on success (i.e. if the table has been updated to the appropriate state) or an error code on failure (out of memory?)
     */
   HT_UniversalSinkKeyValueRef status_t PutOrRemove(HT_SinkKeyParam key, HT_SinkValueParam value)
   {
      if (value == this->GetDefaultValue())
      {
         const status_t ret = this->Remove(HT_ForwardKey(key));
         return (ret == B_DATA_NOT_FOUND) ? B_NO_ERROR : ret;  // we count removing a key that isn't present as a successful operation, since the table is in the expected state
      }
      else return Put(HT_ForwardKey(key), HT_ForwardValue(value));
   }

   /** If (optValue) is non-NULL, this call places the specified key/value pair into the table.
     * If (optValue) is NULL, this call removes the key/value pair from the table.
     * @param key The key value to affect.
     * @param optValue A pointer to the value to place into the table, or a NULL pointer to remove the key/value pair of the specified key.
     * @returns B_NO_ERROR on success (i.e. if the table has been updated to the appropriate state) or an error code on failure (out of memory?)
     */
   HT_UniversalSinkKeyRef status_t PutOrRemove(HT_SinkKeyParam key, const ValueType * optValue)
   {
      if (optValue) return Put(HT_ForwardKey(key), *optValue);
      else
      {
         const status_t ret = this->Remove(HT_ForwardKey(key));
         return (ret == B_DATA_NOT_FOUND) ? B_NO_ERROR : ret;  // we count removing a key that isn't present as a successful operation, since the table is in the expected state
      }
   }

   /** Convenience method.  If the given key already exists in the Hashtable, this method returns NULL.
    *  Otherwise, this method puts a copy of specified value into the table and returns a pointer to the 
    *  just-placed value.
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time)
    *  @param key The key that the new value is to be associated with.
    *  @param value The value to associate with the new key.
    *  @return A pointer to the value object in the table on success, or NULL on failure (key already exists, out of memory)
    */
   HT_UniversalSinkKeyValueRef ValueType * PutIfNotAlreadyPresent(HT_SinkKeyParam key, HT_SinkValueParam value) 
   {
      const uint32 hash = this->ComputeHash(key);
      HashtableEntryBaseType * e = this->GetEntry(hash, key);
      if (e) return NULL;
      else
      {
         e = PutAux(hash, HT_ForwardKey(key), HT_ForwardValue(value), NULL, NULL);
         return e ? &e->_value : NULL;
      }
   }

   /** Convenience method.  If the given key already exists in the Hashtable, this method returns NULL.
    *  Otherwise, this method puts a default value into the table and returns a pointer to the just-placed value.
    *  (average O(1) insertion time, unless auto-sorting is enabled, in which case it becomes O(N) insertion time)
    *  @param key The key that the new value is to be associated with.
    *  @return A pointer to the value object in the table on success, or NULL on failure (key already exists, out of memory)
    */
   HT_UniversalSinkKeyRef ValueType * PutIfNotAlreadyPresent(HT_SinkKeyParam key) 
   {
      const uint32 hash = this->ComputeHash(key);
      HashtableEntryBaseType * e = this->GetEntry(hash, key);
      if (e) return NULL;
      else
      {
         e = PutAux(hash, HT_ForwardKey(key), this->GetDefaultValue(), NULL, NULL);
         return e ? &e->_value : NULL;
      }
   }

   /** Convenience method:  Moves an item from this table to another table.
     * @param moveMe The key in this table of the item that should be moved.
     * @param toTable The table that the item should be in when this operation completes.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on memory failure, or B_DATA_NOT_FOUND
     *          if (moveMe) was not found in this table.  Note that trying to move an item into its
     *          own table will simply return B_NO_ERROR with no side effects.
     */
   template<class RHSHashFunctorType, class RHSSubclassType> status_t MoveToTable(const KeyType & moveMe, HashtableMid<KeyType, ValueType, RHSHashFunctorType, RHSSubclassType> & toTable);

   /** Convenience method:  Copies an item from this table to another table.
     * @param copyMe The key in this table of the item that should be copied.
     * @param toTable The table that the item should be in when this operation completes.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on memory failure, or B_DATA_NOT_FOUND
     *          if (moveMe) was not found in this table.  Note that trying to copy an item into its
     *          own table will simply return B_NO_ERROR with no side effects.
     */
   template<class RHSHashFunctorType, class RHSSubclassType> status_t CopyToTable(const KeyType & copyMe, HashtableMid<KeyType, ValueType, RHSHashFunctorType, RHSSubclassType> & toTable) const;

   /** This method resizes the Hashtable larger if necessary, so that it has at least (newTableSize)
    *  entries in it.  It is not necessary to call this method, but if you know in advance how many
    *  items you will be adding to the table, you can make the population of the table more efficient
    *  by calling this method before adding the items.  Unless (allowShrink) is set to true, this
    *  method will only grow the table, it will never shrink it.
    *  @param newTableSize Number of slots that you wish to have in the table (note that this is different
    *                      from the number of actual items in the table)
    *  @param allowShrink If set to true and (newTableSize) is less than the current number of slots in
    *                     the table, EnsureSize() will shrink the table down to muscleMax(newTableSize, GetNumItems()).
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t EnsureSize(uint32 newTableSize, bool allowShrink = false);

   /** Convenience wrapper around EnsureSize():  This method ensures that this Hashtable has enough 
     * extra space allocated to fit another (numExtras) items without having to do a reallocation.
     * If it doesn't, it will do a reallocation so that it does have at least that much extra space.
     * @param numExtraSlots How many extra items we want to ensure room for.  Defaults to 1.
     * @returns B_NO_ERROR if the extra space now exists, or B_OUT_OF_MEMORY on failure.
     */
   status_t EnsureCanPut(uint32 numExtraSlots = 1) {return EnsureSize(this->GetNumItems()+numExtraSlots, false);}

   /** Convenience wrapper around EnsureSize():  This method shrinks the Hashtable so that its size is
     * equal to the size of the data it contains, plus (numExtraSlots).
     * @param numExtraSlots the number of extra empty slots the Hashtable should contains after the shrink.
     *                      Defaults to zero.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t ShrinkToFit(uint32 numExtraSlots = 0) {return EnsureSize(this->GetNumItems()+numExtraSlots, true);}

   /** Sorts this Hashtable's contents according to its built-in sort ordering. 
     * Specifically, if this object is an OrderedKeysHashtable, the contents will be sorted by key;
     * if this object is an OrderedValuesHashtable, the contents will be sorted by value; and if this
     * class is a plain Hashtable, this method will be a no-op (since a plain Hashtable doesn't have
     * a built-in sort ordering).  If you want to sort a plain Hashtable, have a look at the SortByKey()
     * and SortByValue() methods instead; those allow you to provide a sort ordering on the fly.
     */
   void Sort() {static_cast<SubclassType *>(this)->SortAux();}

private:
   typedef typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase HashtableEntryBaseType;

   HashtableMid(uint32 tableSize) : HashtableBase<KeyType,ValueType,HashFunctorType>(tableSize) {/* empty */}

   // Only these classes are allowed to construct a HashtableMid object
   template<class HisKeyType, class HisValueType, class HisHashFunctorType> friend class Hashtable;
   template<class HisKeyType, class HisValueType, class HisHashFunctorType, class HisEntryCompareFunctorType, class HisSubclassType > friend class OrderedHashtable;

   HT_UniversalSinkKeyValueRef HashtableEntryBaseType * PutAux(uint32 hash, HT_SinkKeyParam key, HT_SinkValueParam value, ValueType * optSetPreviousValue, bool * optReplacedFlag);
};

/**
 *  This is a handy templated Hashtable class, somewhat similar to Java's java.util.Hashtable,
 *  or the STL's unordered_map<>, but with some useful features not typically found in hash table
 *  implementations.  These extra features include:
 *   - Any POD type or user type with a default constructor may be used as a Hashtable Key type.  
 *     If the type is a POD type, MUSCLE's CalculateHashCode() function (which is a wrapper 
 *     around the MurmurHash2 algorithm) will be used to scan the bytes of the Key object, in 
 *     order to calculate a hash code for a given Key.  However, if the Key class has a method 
 *     with the signature "uint32 HashCode() const", then the HashCode() method will be automatically 
 *     called instead.  Unless -DMUSCLE_AVOID_CPLUSPLUS11 is defined, attempting to use
 *     a non-POD type as a key will cause a compile error, unless the HashCode() method is defined for the Key type.
 *   - Pointers can be used as Key types if desired; if the pointers point to a class with
 *     a "uint32 HashCode() const" method; that method will be called to generate the entry's
 *     hash code.  Note that it is up to the calling code to ensure the pointers point to valid
 *     objects, and that the pointed-to objects remain valid for as long as they are pointed
 *     to by the keys in this Hashtable.
 *   - Ordering is preserved:  When iterating over a Hashtable, the iteration will traverse the 
 *     Key/Value pairs in the same order that they were added to the Hashtable.  Also, the ordering 
 *     of Key/Value pairs will be preserved as additional entries are added to (or removed from) 
 *     the Hashtable.
 *   - It is possible to manually modify the iteration-traversal ordering of a table's items via the
 *     MoveToFront(), MoveToBack(), MoveToBefore(), MoveToBehind(), or MoveToPosition() methods.
 *   - Adding or removing items from a Hashtable will not break or invalidate HashtableIterator iterations
 *     that are in progress on that same Hashtable.  This makes it possible, for example, to
 *     iterate over a Hashtable, removing undesired items as you go.  (Note that all concurrent 
 *     iterations must be within the same thread, however -- modifying a Hashtable is NOT thread-safe)
 *   - It is possible to iterate backwards over the contents of a Hashtable, or iterate in either
 *     direction, starting at a specified entry.
 *   - It is possible for a HashtableIterator iteration to skip backwards as well as forwards
 *     in its iteration sequence, by calling iter-- instead of iter++.
 *   - In-place modification of Value objects contained in the Hashtable is allowed.
 *   - Keys and Values in the Hashtable are guaranteed never to change their locations in memory,
 *     except when the Hashtable has to resize its internal array to fit more entries.  That means
 *     that if you know in advance the maximum number of items the Hashtable will ever need to
 *     contain, you can call myHashTable.EnsureSize(maxNumItems) in advance, and then safely retain
 *     pointers to the Key or Value items in the Hashtable without worrying about them becoming
 *     invalidated due to implicit data movement.
 *   - The Hashtable class never does any dynamic memory allocations, except for when it resizes
 *     its internal array larger.  That means that if you know in advance the maximum number of items
 *     a Hashtable will ever need to contain, you can call myHashtable.EnsureSize(maxNumItems) at
 *     program start, and be guaranteed thereafter that your Hashtable will never access the heap or
 *     suffer from an out-of-memory error.
 *   - The Hashtable's contents may be sorted by Key or by Value at any time, by calling SortByKey()
 *     or SortByValue().  Sorting will use the Key or Value class's less-than operator by default, 
 *     or a custom compare-functor can be provided for more sophisticated sorts.  Sorting is done
 *     using MergeSort with, O(log(N)) complexity.
 *   - OrderedKeysHashtable and OrderedValuesHashtable subclasses are available; these work similarly
 *     to the regular Hashtable class, except that they automatically keep the table's contents sorted
 *     by Key (or by Value) at all times.  Note that these classes are necessarily less efficient than
 *     a regular Hashtable, however; in particular they cause Put() to become a O(N) operation instead
 *     of O(1).
 *   - The SwapContents() method will swap the contents of two Hashtables as an O(1) operation --
 *     that is, swapping the contents of two million-entry Hashtables is just as fast as swapping the
 *     contents of two one-entry Hashtables.
 *   - Various convenience methods such as PutAndGet(), GetOrPut(), PutOrRemove(), and PutIfNotAlreadyPresent()
 *     allow common usage patterns to be reduced to a single method call, making their intent explicit 
 *     and their implementation uniform.
 *   - Methods like GetFirstKey(), GetFirstValue(), RemoveFirst() and RemoveLast() allow the
 *     Hashtable to be used as an efficient, keyed, double-ended FIFO queue, if desired.  This
 *     (along with MoveToFront() or GetAndMoveToFront()) makes it very easy to implement an efficient
 *     LRU cache using a Hashtable.
 *   - The CountAverageLookupComparisons() method can be called during development to easily
 *     quantify the performance of the hash functions being used.
 *   - Memory overhead is 6 bytes per key-value entry if the table's capacity is less than 256;
 *     12 bytes per key-value entry if the table's capacity is less than 65535, and 24 bytes per
 *     key-value entry for tables larger than that.
 */
template <class KeyType, class ValueType, class HashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType) > class Hashtable MUSCLE_FINAL_CLASS : public HashtableMid<KeyType, ValueType, HashFunctorType, Hashtable<KeyType, ValueType, HashFunctorType> >
{
public:
   /** The iterator type that goes with this HashtableMid type */
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** Default constructor. */
   Hashtable() : HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >(MUSCLE_HASHTABLE_DEFAULT_CAPACITY) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   Hashtable(const Hashtable & rhs) : HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >(rhs.GetNumAllocatedItemSlots()) {(void) this->CopyFrom(rhs);}

   /** Templated pseudo-copy-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types.
     * @param rhs the Hashtable to make this Hashtable a duplicate of
     */
   template<class RHSHashFunctorType, class RHSSubclassType> Hashtable(const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) : HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >(rhs.GetNumAllocatedItemSlots()) {(void) this->CopyFrom(rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   Hashtable & operator=(const Hashtable & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** Templated pseudo-assignment-operator:  Allows us to be set from similar table with different functor types.
     * @param rhs the Hashtable to make this Hashtable a duplicate of
     */
   template<class RHSHashFunctorType, class RHSSubclassType> Hashtable & operator=(const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** This method does an efficient zero-copy swap of this hash table's contents with those of (swapMe).  
    *  That is to say, when this method returns, (swapMe) will be identical to the old state of this 
    *  Hashtable, and this Hashtable will be identical to the old state of (swapMe). 
    *  Any active iterators present for either table will swap owners also, becoming associated with the other table.
    *  @param swapMe The table whose contents and iterators are to be swapped with this table's.
    *  @note This method is redeclared here solely to make sure that the muscleSwap() SFINAE magic sees it.
    */
   void SwapContents(Hashtable & swapMe) {HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >::SwapContents(swapMe);}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   Hashtable(Hashtable && rhs) : HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >(0) {this->SwapContents(rhs);}

   /** Templated pseudo-move-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types.
     * @param rhs the table to steal the contents of, to make them our own
     */
   template<class RHSHashFunctorType, class RHSSubclassType> Hashtable(HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> && rhs) : HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >(0) {this->SwapContents(rhs);}

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   Hashtable & operator=(Hashtable && rhs) {this->SwapContents(rhs); return *this;}

   /** Templated pseudo-assignment-move-operator:  Allows us to be set from similar table with different functor types.
     * @param rhs the table to steal the contents of, to make them our own
     */
   template<class RHSHashFunctorType, class RHSSubclassType> Hashtable & operator=(HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> && rhs) {this->SwapContents(rhs); return *this;}
#endif

private:
   typedef typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase HashtableEntryBaseType;

   // These are the "fake virtual functions" that can be called by HashtableMid as part of the
   // Curiously Recurring Template Pattern that Hashtable uses to implement polymorphic behavior at compile time.
   friend class HashtableMid<KeyType,ValueType,HashFunctorType,Hashtable<KeyType,ValueType,HashFunctorType> >;
   void InsertIterationEntryAux(HashtableEntryBaseType * e) {this->InsertIterationEntry(e, this->IndexToEntryChecked(this->_iterTailIdx));}
   void MoveIterationEntryToCorrectPositionAux(HashtableEntryBaseType *) {/* empty -- this class doesn't have a well-defined sort ordering */}
   void DisableAutoSort() const {/* empty -- this class never auto-sorts anyway  */}
   void SortAux() {/* empty -- this class doesn't have a well-defined sort ordering */}
};

/** This is an intermediate class containing functionality common to both the OrderedKeysHashtable and the
  * OrderedValuesHashtable classes.  This class is not intended to be instantiated directly -- to use it,
  * you should instantiate an OrderedKeysHashtable or an OrderedValuesHashtable instead.
  */
template <class KeyType, class ValueType, class HashFunctorType, class EntryCompareFunctorType, class SubclassType > class OrderedHashtable : public HashtableMid<KeyType, ValueType, HashFunctorType, SubclassType>
{
public:
   /** The iterator type that goes with this HashtableMid type */
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** This method can be used to deactivate or re-activate auto-sorting on this Hashtable.
     * Auto-sorting is active by default.
     *
     * If active, auto-sorting ensures that whenever Put() is called, the new/updated item is
     * automatically moved to the correct place in the iterator traversal list.  Note that when
     * auto-sort is enabled, Put() becomes an O(N) operation instead of O(1).
     *
     * @param enabled True if autosort should be enabled; false if it shouldn't be.
     * @param sortNow If true, Sort() will be called when entering a new auto-sort mode, to ensure that
     *                the table is in a sorted state.  You can avoid an immediate sort by specifying this
     *                parameter as false, but be aware that when auto-sorting is enabled, Put() expects
     *                the table's contents to already be sorted according to the current auto-sort state,
     *                and if they aren't, it won't insert its new item at the correct location.  Defaults to true.
     */
   void SetAutoSortEnabled(bool enabled, bool sortNow = true)
   {
      if (enabled != this->_autoSortEnabled)
      {
         this->_autoSortEnabled = enabled;
         if ((sortNow)&&(enabled)) this->Sort();
      }
   }

   /** Returns true iff auto-sort is currently enabled on this Hashtable.  Default state is true/enabled. */
   bool GetAutoSortEnabled() const {return _autoSortEnabled;}

   /** This method can be used to set the "cookie value" that will be passed in to the comparison-functor
     * objects that this class is templated on.  The functor objects can then use this value to access
     * any context information that might be necessary to do their comparison.
     * This value will be passed along during whenever a user-defined compare-functor is called implicitly
     * as part of this table's auto-sorting mechanism.
     * @param cookie the new cookie value (meaning is application-specific)
     */
   void SetCompareCookie(void * cookie) {_compareCookie = cookie;}

   /** Returns the current comparison cookie, as previously set by SetCompareCookie().  Default value is NULL. */
   void * GetCompareCookie() const {return _compareCookie;}

   /** Moves the specified key/value pair so that it is in the correct position based on the
     * class's current iteration-ordering.  Generally this call isn't necessary if the table's
     * auto-sort mode is enabled, but you might want to call it after e.g. manually modifying
     * that table's iteration-order.
     * @param key The key object of the key/value pair that may need to be repositioned to
     *            its correct (sorted-by-value) location.
     * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if (key) was not found in the table.
     */
   status_t Reposition(const KeyType & key)
   {
      HashtableEntryBaseType * e = this->GetEntry(this->ComputeHash(key), key);
      if (e)
      {
         this->MoveIterationEntryToCorrectPositionAux(e);
         return B_NO_ERROR;
      }
      else return B_DATA_NOT_FOUND;
   }
   
private:
   typedef HashtableMid<KeyType, ValueType, HashFunctorType, SubclassType> HashtableMidType;
   typedef typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase HashtableEntryBaseType;

   template<class HisKeyType, class HisValueType, class HisKeyCompareFunctorType,   class HisHashFunctorType> friend class OrderedKeysHashtable;
   template<class HisKeyType, class HisValueType, class HisValueCompareFunctorType, class HisHashFunctorType> friend class OrderedValuesHashtable;

   OrderedHashtable(uint32 tableSize, const EntryCompareFunctorType & efc, void * optCompareCookie = NULL) : HashtableMidType(tableSize), _entryCompareFunctor(efc), _autoSortEnabled(true), _compareCookie(optCompareCookie) {/* empty */}

   const EntryCompareFunctorType _entryCompareFunctor;

   // These are the "fake virtual functions" that can be called by HashtableMid as part of the
   // Curiously Recurring Template Pattern that Hashtable uses to implement polymorphic behavior at compile time.
   friend class HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>;
   void InsertIterationEntryAux(HashtableEntryBaseType * e) {this->InsertIterationEntryInOrder(_entryCompareFunctor, e, _autoSortEnabled, _compareCookie);}
   void MoveIterationEntryToCorrectPositionAux(HashtableEntryBaseType * e) {this->MoveIterationEntryToCorrectPosition(_entryCompareFunctor, e, _compareCookie);}
   void DisableAutoSort() {_autoSortEnabled = false;}
   void SortAux() {this->SortByEntry(_entryCompareFunctor, _compareCookie);}

   bool _autoSortEnabled;  // only used in the ordered Hashtable subclasses, but kept here so that its state can be swapped by SwapContents(), etc.
   void * _compareCookie;
};

/** This is a specialized Hashtable that keeps its iteration entries sorted-by-key at all times (unless you specifically call SetAutoSortEnabled(false)) */
template <class KeyType, class ValueType, class KeyCompareFunctorType=CompareFunctor<KeyType>, class HashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType)> class OrderedKeysHashtable MUSCLE_FINAL_CLASS : public OrderedHashtable<KeyType, ValueType, HashFunctorType, typename HashtableBase<KeyType,ValueType,HashFunctorType>::template ByKeyEntryCompareFunctor<KeyCompareFunctorType>, OrderedKeysHashtable<KeyType, ValueType, KeyCompareFunctorType, HashFunctorType> >
{
public:
   // Some convenient typedefs, for brevity's sake
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** Default constructor.
     * @param optCompareCookie the value that will be passed to our compare functor.  Defaults to NULL.
     */
   OrderedKeysHashtable(void * optCompareCookie = NULL) : OrderedHashtableType(MUSCLE_HASHTABLE_DEFAULT_CAPACITY, ByKeyEntryCompareFunctorType(_keyCompareFunctor), optCompareCookie), _keyCompareFunctor() {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   OrderedKeysHashtable(const OrderedKeysHashtable & rhs) : OrderedHashtableType(rhs.GetNumAllocatedItemSlots(), ByKeyEntryCompareFunctorType(_keyCompareFunctor), rhs.GetCompareCookie()), _keyCompareFunctor() {(void) this->CopyFrom(rhs);}

   /** Templated pseudo-copy-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types. 
     * @param rhs the hash table to make this hash table a duplicate of
     */
   template<class RHSKeyCompareFunctorType, class RHSHashFunctorType> OrderedKeysHashtable(const HashtableMid<KeyType,ValueType,RHSKeyCompareFunctorType,RHSHashFunctorType> & rhs) : OrderedHashtableType(rhs.GetNumAllocatedItemSlots(), ByKeyEntryCompareFunctorType(_keyCompareFunctor), NULL), _keyCompareFunctor() {(void) this->CopyFrom(rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   OrderedKeysHashtable & operator=(const OrderedKeysHashtable & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** Templated pseudo-assignment-operator:  Allows us to be set from similar table with different functor types. 
     * @param rhs the hash table to make this hash table a duplicate of
     */
   template<class RHSHashFunctorType, class RHSSubclassType> OrderedKeysHashtable & operator=(const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** This method does an efficient zero-copy swap of this hash table's contents with those of (swapMe).  
    *  That is to say, when this method returns, (swapMe) will be identical to the old state of this 
    *  Hashtable, and this Hashtable will be identical to the old state of (swapMe). 
    *  Any active iterators present for either table will swap owners also, becoming associated with the other table.
    *  @param swapMe The table whose contents and iterators are to be swapped with this table's.
    *  @note This method is redeclared here solely to make sure that the muscleSwap() SFINAE magic sees it.
    *  @note This table's compare-functor object and compare-cookie value will NOT be swapped as part of this
    *        operation.  That means that if (swapMe) was sorted using a different sorting mechanism than the
    *        one in use by this table's auto-sort, you may want to call Sort() on this table after the swap.
    */
   void SwapContents(OrderedKeysHashtable & swapMe) {HashtableMid<KeyType,ValueType,HashFunctorType,OrderedKeysHashtable<KeyType,ValueType,KeyCompareFunctorType,HashFunctorType> >::SwapContents(swapMe);}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   OrderedKeysHashtable(OrderedKeysHashtable && rhs) : OrderedHashtableType(0, ByKeyEntryCompareFunctorType(_keyCompareFunctor), NULL), _keyCompareFunctor() {this->SwapContents(rhs);}

   /** Templated pseudo-move-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types.
     * @param rhs the hash table that we are to steal the contents of, to make them our own
     */
   template<class RHSKeyCompareFunctorType, class RHSHashFunctorType> OrderedKeysHashtable(HashtableMid<KeyType,ValueType,RHSKeyCompareFunctorType,RHSHashFunctorType> && rhs) : OrderedHashtableType(0, ByKeyEntryCompareFunctorType(_keyCompareFunctor), NULL), _keyCompareFunctor() {this->SwapContents(rhs);}

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   OrderedKeysHashtable & operator=(OrderedKeysHashtable && rhs) {this->SwapContents(rhs); return *this;}

   /** Templated pseudo-move-assignment-operator:  Allows us to be set from similar table with different functor types.
     * @param rhs the hash table that we are to steal the contents of, to make them our own
     */
   template<class RHSHashFunctorType, class RHSSubclassType> OrderedKeysHashtable & operator=(HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> && rhs) {(void) this->SwapContents(rhs); return *this;}
#endif

   typedef typename HashtableBase<KeyType,ValueType,HashFunctorType>::template ByKeyEntryCompareFunctor<KeyCompareFunctorType> ByKeyEntryCompareFunctorType;
   typedef OrderedKeysHashtable<KeyType, ValueType, KeyCompareFunctorType, HashFunctorType> OrderedKeysHashtableType;
   typedef OrderedHashtable<KeyType, ValueType, HashFunctorType, ByKeyEntryCompareFunctorType, OrderedKeysHashtableType> OrderedHashtableType;

   const KeyCompareFunctorType _keyCompareFunctor;
};

/** This is a specialized Hashtable that keeps its iteration entries sorted-by-value at all times (unless you specifically call SetAutoSortEnabled(false)) */
template <class KeyType, class ValueType, class ValueCompareFunctorType=CompareFunctor<ValueType>, class HashFunctorType=typename DEFAULT_HASH_FUNCTOR(KeyType)> class OrderedValuesHashtable MUSCLE_FINAL_CLASS : public OrderedHashtable<KeyType, ValueType, HashFunctorType, typename HashtableBase<KeyType,ValueType,HashFunctorType>::template ByValueEntryCompareFunctor<ValueCompareFunctorType>, OrderedValuesHashtable<KeyType, ValueType, ValueCompareFunctorType, HashFunctorType> >
{
public:
   // Some convenient typedefs, for brevity's sake
   typedef HashtableIterator<KeyType,ValueType,HashFunctorType> IteratorType;

   /** Default constructor.
     * @param optCompareCookie the value that will be passed to our compare functor.  Defaults to NULL.
     */
   OrderedValuesHashtable(void * optCompareCookie = NULL) : OrderedHashtableType(MUSCLE_HASHTABLE_DEFAULT_CAPACITY, ByValueEntryCompareFunctorType(_valueCompareFunctor), optCompareCookie), _valueCompareFunctor() {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   OrderedValuesHashtable(const OrderedValuesHashtable & rhs) : OrderedHashtableType(rhs.GetNumAllocatedItemSlots(), ByValueEntryCompareFunctorType(_valueCompareFunctor), rhs.GetCompareCookie()), _valueCompareFunctor() {(void) this->CopyFrom(rhs);}

   /** Templated pseudo-copy-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types. 
     * @param rhs the hash table to make this hash table a duplicate of
     */
   template<class RHSValueCompareFunctorType, class RHSHashFunctorType> OrderedValuesHashtable(const HashtableMid<KeyType,ValueType,RHSValueCompareFunctorType,RHSHashFunctorType> & rhs) : OrderedHashtableType(rhs.GetNumAllocatedItemSlots(), ByValueEntryCompareFunctorType(_valueCompareFunctor), NULL), _valueCompareFunctor() {(void) this->CopyFrom(rhs);}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   OrderedValuesHashtable & operator=(const OrderedValuesHashtable & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** Templated pseudo-assignment-operator:  Allows us to be set from similar table with different functor types. 
     * @param rhs the hash table to make this hash table a duplicate of
     */
   template<class RHSHashFunctorType, class RHSSubclassType> OrderedValuesHashtable & operator=(const HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> & rhs) {(void) this->CopyFrom(rhs); return *this;}

   /** This method does an efficient zero-copy swap of this hash table's contents with those of (swapMe).  
    *  That is to say, when this method returns, (swapMe) will be identical to the old state of this 
    *  Hashtable, and this Hashtable will be identical to the old state of (swapMe). 
    *  Any active iterators present for either table will swap owners also, becoming associated with the other table.
    *  @param swapMe The table whose contents and iterators are to be swapped with this table's.
    *  @note This method is redeclared here solely to make sure that the muscleSwap() SFINAE magic sees it.
    *  @note This table's compare-functor object and compare-cookie value will NOT be swapped as part of this
    *        operation.  That means that if (swapMe) was sorted using a different sorting mechanism than the
    *        one in use by this table's auto-sort, you may want to call Sort() on this table after the swap.
    */
   void SwapContents(OrderedValuesHashtable & swapMe) {HashtableMid<KeyType,ValueType,HashFunctorType,OrderedValuesHashtable<KeyType,ValueType,ValueCompareFunctorType,HashFunctorType> >::SwapContents(swapMe);}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   OrderedValuesHashtable(OrderedValuesHashtable && rhs) : OrderedHashtableType(0, ByValueEntryCompareFunctorType(_valueCompareFunctor), NULL), _valueCompareFunctor() {this->SwapContents(rhs);}

   /** Templated pseudo-move-constructor:  Allows us to be instantiated as a copy of a similar table with different functor types.
     * @param rhs the hash table that we are to steal the contents of, to make them our own
     */
   template<class RHSValueCompareFunctorType, class RHSHashFunctorType> OrderedValuesHashtable(HashtableMid<KeyType,ValueType,RHSValueCompareFunctorType,RHSHashFunctorType> && rhs) : OrderedHashtableType(0, ByValueEntryCompareFunctorType(_valueCompareFunctor), NULL), _valueCompareFunctor() {this->SwapContents(rhs);}

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   OrderedValuesHashtable & operator=(OrderedValuesHashtable && rhs) {this->SwapContents(rhs); return *this;}

   /** Templated pseudo-move-assignment-operator:  Allows us to be set from similar table with different functor types.
     * @param rhs the hash table that we are to steal the contents of, to make them our own
     */
   template<class RHSHashFunctorType, class RHSSubclassType> OrderedValuesHashtable & operator=(HashtableMid<KeyType,ValueType,RHSHashFunctorType,RHSSubclassType> && rhs) {(void) this->SwapContents(rhs); return *this;}
#endif

   typedef typename HashtableBase<KeyType,ValueType,HashFunctorType>::template ByValueEntryCompareFunctor<ValueCompareFunctorType> ByValueEntryCompareFunctorType;
   typedef OrderedValuesHashtable<KeyType, ValueType, ValueCompareFunctorType, HashFunctorType> OrderedValuesHashtableType;
   typedef OrderedHashtable<KeyType, ValueType, HashFunctorType, ByValueEntryCompareFunctorType, OrderedValuesHashtableType> OrderedHashtableType;

   const ValueCompareFunctorType _valueCompareFunctor;
};

//===============================================================
// Implementation of HashtableBase
//===============================================================

// Similar to the equality operator
template <class KeyType, class ValueType, class HashFunctorType>
bool
HashtableBase<KeyType, ValueType, HashFunctorType> ::
IsEqualTo(const HashtableBase<KeyType, ValueType, HashFunctorType> & rhs, bool considerOrdering) const
{
   if (this == &rhs) return true;
   if (GetNumItems() != rhs.GetNumItems()) return false;

   const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
   if (considerOrdering)
   {
      const HashtableEntryBase * hisE = rhs.IndexToEntryChecked(rhs._iterHeadIdx);
      while(e)
      {
         if (!(hisE->_value == e->_value)) return false;
         e    = this->GetEntryIterNextChecked(e);
         hisE = rhs.GetEntryIterNextChecked(hisE);
      }
   }
   else
   {
      while(e)
      {
         const HashtableEntryBase * hisE = rhs.GetEntry(e->_hash, e->_key);
         if ((hisE == NULL)||(!(hisE->_value == e->_value))) return false;
         e = this->GetEntryIterNextChecked(e);
      }
   }
   return true;
}

template <class KeyType, class ValueType, class HashFunctorType>
bool
HashtableBase<KeyType,ValueType,HashFunctorType>::ContainsValue(const ValueType & value) const
{
   const HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
   while(e)
   {
      if (e->_value == value) return true;
      e = GetEntryIterNextChecked(e);
   }
   return false;
}

template <class KeyType, class ValueType, class HashFunctorType>
int32
HashtableBase<KeyType,ValueType,HashFunctorType>::IndexOfKey(const KeyType & key) const
{
   const HashtableEntryBase * entry = this->GetEntry(this->ComputeHash(key), key);
   int32 count = -1;
   if (entry)
   {
      if (entry == this->IndexToEntryChecked(_iterTailIdx)) count = GetNumItems()-1;
      else
      {
         while(entry)
         {
            entry = this->GetEntryIterPrevChecked(entry);
            count++;
         }
      }
   }
   return count;
}

template <class KeyType, class ValueType, class HashFunctorType>
int32
HashtableBase<KeyType,ValueType,HashFunctorType>::IndexOfValue(const ValueType & value, bool searchBackwards) const
{
   if (searchBackwards)
   {
      int32 idx = GetNumItems();
      const HashtableEntryBase * entry = this->IndexToEntryChecked(_iterTailIdx);
      while(entry)
      {
         --idx;
         if (entry->_value == value) return idx;
         entry = this->GetEntryIterPrevChecked(entry);
      }
   }
   else
   {
      int32 idx = 0;
      const HashtableEntryBase * entry = this->IndexToEntryChecked(_iterHeadIdx);
      while(entry)
      {
         if (entry->_value == value) return idx;
         entry = this->GetEntryIterNextChecked(entry);
         idx++;
      }
   }
   return -1;
}

template <class KeyType, class ValueType, class HashFunctorType>
const KeyType * 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKeyAt(uint32 index) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? &e->_key : NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKeyAt(uint32 index, KeyType & retKey) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   if (e)
   {
      retKey = e->_key;
      return B_NO_ERROR;
   }
   return B_BAD_ARGUMENT;
}

template <class KeyType, class ValueType, class HashFunctorType>
const KeyType & 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKeyAtWithDefault(uint32 index) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? e->_key : GetDefaultKey();
}

template <class KeyType, class ValueType, class HashFunctorType>
KeyType
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKeyAtWithDefault(uint32 index, const KeyType & defaultKey) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? e->_key : defaultKey;
}

template <class KeyType, class ValueType, class HashFunctorType>
const ValueType * 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValueAt(uint32 index) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? &e->_value : NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValueAt(uint32 index, ValueType & retValue) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   if (e)
   {
      retValue = e->_value;
      return B_NO_ERROR;
   }
   return B_BAD_ARGUMENT;
}

template <class KeyType, class ValueType, class HashFunctorType>
const ValueType & 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValueAtWithDefault(uint32 index) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? e->_value : GetDefaultValue();
}

template <class KeyType, class ValueType, class HashFunctorType>
ValueType
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValueAtWithDefault(uint32 index, const ValueType & defaultValue) const
{
   const HashtableEntryBase * e = GetEntryAt(index);
   return e ? e->_value : defaultValue;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValue(const KeyType & key, ValueType & setValue) const
{
   const ValueType * ptr = GetValue(key);
   if (ptr)
   {
      setValue = *ptr;
      return B_NO_ERROR;
   }
   else return B_DATA_NOT_FOUND;
}

template <class KeyType, class ValueType, class HashFunctorType>
const ValueType *
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValue(const KeyType & key) const
{
   const HashtableEntryBase * e = this->GetEntry(this->ComputeHash(key), key);
   return e ? &e->_value : NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
ValueType *
HashtableBase<KeyType,ValueType,HashFunctorType>::GetValue(const KeyType & key)
{
   HashtableEntryBase * e = this->GetEntry(this->ComputeHash(key), key);
   return e ? &e->_value : NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
const KeyType * 
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKey(const KeyType & lookupKey) const
{
   const HashtableEntryBase * e = this->GetEntry(this->ComputeHash(lookupKey), lookupKey);
   return e ? &e->_key : NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t
HashtableBase<KeyType,ValueType,HashFunctorType>::GetKey(const KeyType & lookupKey, KeyType & setKey) const
{
   const KeyType * ptr = GetKey(lookupKey);
   if (ptr)
   {
      setKey = *ptr;
      return B_NO_ERROR;
   }
   else return B_DATA_NOT_FOUND;
}

/// @cond HIDDEN_SYMBOLS

template <class KeyType, class ValueType, class HashFunctorType>
typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase *
HashtableBase<KeyType,ValueType,HashFunctorType>::GetEntry(uint32 hash, const KeyType & key) const
{
   if (HasItems()) 
   {
      HashtableEntryBase * e = this->GetEntryMapToUnchecked(this->IndexToEntryUnchecked(hash%_tableSize));
      if (IsBucketHead(e))  // if the e isn't the start of a bucket, then we know our entry doesn't exist
      {
         while(e)
         {
            if ((e->_hash == hash)&&(AreKeysEqual(e->_key, key))) return e;
            e = this->GetEntryBucketNextChecked(e);
         }
      }
   }
   return NULL;
}

template <class KeyType, class ValueType, class HashFunctorType>
typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase *
HashtableBase<KeyType,ValueType,HashFunctorType>::GetEntryAt(uint32 idx) const
{
   HashtableEntryBase * e = NULL;
   if (idx < _numItems)
   {
      if (idx < _numItems/2)
      {
         e = this->IndexToEntryChecked(_iterHeadIdx);
         while((e)&&(idx--)) e = this->GetEntryIterNextChecked(e);
      }
      else
      {
         idx = _numItems-(idx+1);
         e = this->IndexToEntryChecked(_iterTailIdx);
         while((e)&&(idx--)) e = this->GetEntryIterPrevChecked(e);
      }
   }
   return e;
}

template <class KeyType, class ValueType, class HashFunctorType>
template <class HisValueType, class HisHashFunctorType> 
bool 
HashtableBase<KeyType,ValueType,HashFunctorType>::AreKeySetsEqual(const HashtableBase<KeyType, HisValueType, HisHashFunctorType> & rhs) const
{
   if (GetNumItems() != rhs.GetNumItems()) return false;
   for (HashtableIterator<KeyType, ValueType, HashFunctorType> iter(*this); iter.HasData(); iter++) if (rhs.ContainsKey(iter.GetKey()) == false) return false;
   return true;
}

template <class KeyType, class ValueType, class HashFunctorType>
template <class HisValueType, class HisHashFunctorType>
bool
HashtableBase<KeyType,ValueType,HashFunctorType>::AreKeysASubsetOf(const HashtableBase<KeyType, HisValueType, HisHashFunctorType> & rhs) const
{
   if (GetNumItems() > rhs.GetNumItems()) return false;  // pigeonhole principle!
   for (HashtableIterator<KeyType, ValueType, HashFunctorType> iter(*this); iter.HasData(); iter++) if (rhs.ContainsKey(iter.GetKey()) == false) return false;
   return true;
}

// Linked-list MergeSort adapted from Simon Tatham's C code at http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.c
template <class KeyType, class ValueType, class HashFunctorType>
template <class EntryCompareFunctorType>
void 
HashtableBase<KeyType,ValueType,HashFunctorType>::SortByEntry(const EntryCompareFunctorType & ecf, void * cookie)
{
   if (this->_iterHeadIdx == MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) return;

   for (uint32 mergeSize = 1; /* empty */; mergeSize *= 2)
   {
      HashtableEntryBase * p = this->IndexToEntryChecked(_iterHeadIdx);
      this->_iterHeadIdx = this->_iterTailIdx = MUSCLE_HASHTABLE_INVALID_SLOT_INDEX;

      uint32 numMerges = 0;  /* count number of merges we do in this pass */
      while(p) 
      {
         numMerges++;  /* there exists a merge to be done */

         /* step `mergeSize' places along from p */
         HashtableEntryBase * q = p;
         uint32 psize = 0;
         for (uint32 i=0; i<mergeSize; i++) 
         {
             psize++;
             q = this->GetEntryIterNextChecked(q);
             if (!q) break;
         }

         /* now we have two lists; merge them */
         for (uint32 qsize=mergeSize; ((psize > 0)||((qsize > 0)&&(q))); /* empty */) 
         {
            HashtableEntryBase * e;

            /* decide whether next element of the merge comes from p or q */
                 if (psize == 0)                     {e = q; q = this->GetEntryIterNextChecked(q); qsize--;}
            else if ((qsize == 0)||(q == NULL))      {e = p; p = this->GetEntryIterNextChecked(p); psize--;}
            else if (ecf.Compare(*p,*q,cookie) <= 0) {e = p; p = this->GetEntryIterNextChecked(p); psize--;}
            else                                     {e = q; q = this->GetEntryIterNextChecked(q); qsize--;}

            /* append to our new more-sorted list */
            HashtableEntryBase * tail = this->IndexToEntryChecked(_iterTailIdx);
            if (tail) this->SetEntryIterNextChecked(tail, e);
                 else this->_iterHeadIdx = this->EntryToIndexChecked(e);
            this->SetEntryIterPrevChecked(e, tail);
            this->_iterTailIdx = this->EntryToIndexChecked(e);
         }

         p = q; /* now p has stepped `mergeSize' places along, and q has too */
      }
      this->SetEntryIterNext(this->IndexToEntryChecked(_iterTailIdx), MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
      if (numMerges <= 1) return;
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::SwapContentsAux(HashtableBase<KeyType,ValueType,HashFunctorType> & swapMe, bool swapIterators)
{
   muscleSwap(_numItems,       swapMe._numItems);
   muscleSwap(_tableSize,      swapMe._tableSize);
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
   muscleSwap(_tableIndexType, swapMe._tableIndexType);
#endif
   muscleSwap(_table,          swapMe._table);
   muscleSwap(_iterHeadIdx,    swapMe._iterHeadIdx);
   muscleSwap(_iterTailIdx,    swapMe._iterTailIdx);
   muscleSwap(_freeHeadIdx,    swapMe._freeHeadIdx);
   if (swapIterators)
   {
      muscleSwap(_iterList,         swapMe._iterList);
#ifndef MUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
      muscleSwap(_iteratorCount,    swapMe._iteratorCount);
      muscleSwap(_iteratorThreadID, swapMe._iteratorThreadID);
#endif

      // Lastly, swap the owners of all iterators, so that they will unregister from the correct table when they die
      {
         IteratorType * next = _iterList;
         while(next)
         {
            next->_owner = &swapMe;
            next = next->_nextIter;
         }
      }
      {
         IteratorType * next = swapMe._iterList;
         while(next)
         {
            next->_owner = this;
            next = next->_nextIter;
         }
      }
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t
HashtableBase<KeyType,ValueType,HashFunctorType>::EnsureTableAllocated()
{
   if (this->_table == NULL) 
   {
      switch(this->GetTableIndexType())
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            this->_table = HashtableEntry<uint8> ::CreateEntriesArray(this->_tableSize); 
         break;
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            this->_table = HashtableEntry<uint16>::CreateEntriesArray(this->_tableSize); 
         break;
#endif

         default:
            this->_table = HashtableEntry<uint32>::CreateEntriesArray(this->_tableSize); 
         break;
      }
      this->_freeHeadIdx = 0;
   }
   return this->_table ? B_NO_ERROR : B_OUT_OF_MEMORY;
}

// This is the part of the insertion that is CompareFunctor-neutral.
template <class KeyType, class ValueType, class HashFunctorType>
HT_UniversalSinkKeyValueRef
typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase *
HashtableBase<KeyType,ValueType,HashFunctorType>::PutAuxAux(uint32 hash, HT_SinkKeyParam key, HT_SinkValueParam value)
{
   HashtableEntryBase * tableSlot = this->GetEntryMapToUnchecked(this->IndexToEntryUnchecked(hash%_tableSize));
   if (IsBucketHead(tableSlot))
   {
      // This slot's chain is already present -- so just create a new entry in the chain's linked list to hold our item
      HashtableEntryBase * e = this->IndexToEntryUnchecked(_freeHeadIdx);
      _freeHeadIdx = this->PopFromFreeList(e, _freeHeadIdx);
      e->_hash  = hash;
      e->_key   = HT_ForwardKey(key);
      e->_value = HT_ForwardValue(value);

      // insert e into the list immediately after (tableSlot)
      this->SetEntryBucketPrevUnchecked(e, tableSlot);
      const uint32 eBucketNext = this->GetEntryBucketNext(tableSlot);
      this->SetEntryBucketNext(e, eBucketNext);

      const uint32 eIdx = this->EntryToIndexUnchecked(e);
      if (eBucketNext != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) this->SetEntryBucketPrev(this->IndexToEntryUnchecked(eBucketNext), eIdx);
      this->SetEntryBucketNext(tableSlot, eIdx);
      return e;
   }
   else 
   {
      if (tableSlot->_hash != MUSCLE_HASHTABLE_INVALID_HASH_CODE)
      {
         // Hey, some other bucket is using my starter-slot!
         // To get around this, we'll swap my starter-slot for an empty one and use that instead.
         SwapEntryMaps(this->GetEntryMappedFrom(tableSlot), this->GetEntryMappedFrom(this->IndexToEntryChecked(_freeHeadIdx)));
         tableSlot = this->IndexToEntryChecked(_freeHeadIdx);
      }
      _freeHeadIdx = this->PopFromFreeList(tableSlot, _freeHeadIdx);

      // First entry in tableSlot; just copy data over
      tableSlot->_hash  = hash;
      tableSlot->_key   = HT_ForwardKey(key);
      tableSlot->_value = HT_ForwardValue(value);
 
      this->SetEntryBucketPrev(tableSlot, MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
      this->SetEntryBucketNext(tableSlot, MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
      return tableSlot;
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::SwapEntryMaps(uint32 idx1, uint32 idx2)
{
   HashtableEntryBase * e1 = this->IndexToEntryUnchecked(idx1);
   HashtableEntryBase * e2 = this->IndexToEntryUnchecked(idx2);

   // was: muscleSwap(e1->_mapTo, e2->_mapTo);
   {
      const uint32 e1MapTo = this->GetEntryMapTo(e1);
      const uint32 e2MapTo = this->GetEntryMapTo(e2);
      this->SetEntryMapTo(e1, e2MapTo);
      this->SetEntryMapTo(e2, e1MapTo);
   }

   this->SetEntryMappedFrom(this->GetEntryMapToUnchecked(e1), idx1); // was: _table[e1->_mapTo]._mappedFrom = idx1;
   this->SetEntryMappedFrom(this->GetEntryMapToUnchecked(e2), idx2); // was: _table[e2->_mapTo]._mappedFrom = idx2;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t
HashtableBase<KeyType,ValueType,HashFunctorType>::RemoveEntryByIndex(uint32 idx, ValueType * optSetValue)
{
   HashtableEntryBase * entry = this->IndexToEntryChecked(idx);
   return entry ? RemoveEntry(entry, optSetValue) : B_BAD_ARGUMENT;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t
HashtableBase<KeyType,ValueType,HashFunctorType>::RemoveEntry(HashtableEntryBase * e, ValueType * optSetValue)
{
   RemoveIterationEntry(e);
   if (optSetValue) *optSetValue = e->_value;

   HashtableEntryBase * prev = this->GetEntryBucketPrevChecked(e);
   HashtableEntryBase * next = this->GetEntryBucketNextChecked(e);
   if (prev)
   {
      this->SetEntryBucketNextChecked(prev, next);
      if (next) this->SetEntryBucketPrevUnchecked(next, prev);
   }
   else if (next) 
   {
      this->SetEntryBucketPrev(next, MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
      SwapEntryMaps(this->GetEntryMappedFrom(e), this->GetEntryMappedFrom(next));
   }

   _numItems--;
   switch(this->GetTableIndexType())
   {
      case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         static_cast<HashtableEntry<uint8> *>(e)->PushToFreeList(GetDefaultKey(), GetDefaultValue(), _freeHeadIdx, this); 
      break;
#endif

      case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         static_cast<HashtableEntry<uint16>*>(e)->PushToFreeList(GetDefaultKey(), GetDefaultValue(), _freeHeadIdx, this); 
      break;
#endif

      default:
         static_cast<HashtableEntry<uint32>*>(e)->PushToFreeList(GetDefaultKey(), GetDefaultValue(), _freeHeadIdx, this); 
      break;
   }
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType>
uint32
HashtableBase<KeyType,ValueType,HashFunctorType>::PopFromFreeList(HashtableEntryBase * e, uint32 freeHeadIdx) 
{
   switch(this->GetTableIndexType())
   {
      case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         return static_cast<HashtableEntry<uint8> *>(e)->PopFromFreeList(freeHeadIdx, this);
#endif

      case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         return static_cast<HashtableEntry<uint16>*>(e)->PopFromFreeList(freeHeadIdx, this); 
#endif

      default:
         return static_cast<HashtableEntry<uint32>*>(e)->PopFromFreeList(freeHeadIdx, this);
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
bool
HashtableBase<KeyType,ValueType,HashFunctorType>::IsPointerPointingIntoDataTable(const void * ptr) const 
{
   switch(this->GetTableIndexType())
   {
      case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         return HashtableEntry<uint8> ::IsPointerPointingIntoDataTable(this, ptr);
#endif

      case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
         return HashtableEntry<uint16>::IsPointerPointingIntoDataTable(this, ptr); 
#endif

      default:
         return HashtableEntry<uint32>::IsPointerPointingIntoDataTable(this, ptr);
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::Clear(bool releaseCachedBuffers)
{
   // First go through our list of active iterators, and let them all know they are now invalid
   while(_iterList)
   {
      IteratorType * next = _iterList->_nextIter;
      _iterList->_owner = NULL;
      _iterList->_iterCookie = _iterList->_prevIter = _iterList->_nextIter = NULL;
      _iterList = next;
   }

   // It's important to set each in-use HashtableEntryBase to its default state so
   // that any held memory (e.g. RefCountables) will be freed, etc.
   // Calling RemoveEntry() on each item is necessary to ensure correct behavior
   // even when the templatized classes' assignment operations cause re-entrancies, etc.
   while(_iterHeadIdx != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX) (void) RemoveEntryByIndex(_iterHeadIdx, NULL);

   if (releaseCachedBuffers)
   {
      HashtableEntryBase * oldTable = _table;
      const uint8 oldTableIndexType = this->GetTableIndexType();

      _table          = NULL;
      _freeHeadIdx    = MUSCLE_HASHTABLE_INVALID_SLOT_INDEX;
      _tableSize      = MUSCLE_HASHTABLE_DEFAULT_CAPACITY;
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
      _tableIndexType = this->ComputeTableIndexTypeForTableSize(_tableSize);
#endif

      // done after state is updated, in case of re-entrancies in the dtors
      switch(oldTableIndexType)
      {
         case TABLE_INDEX_TYPE_UINT8:  
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            delete [] (static_cast<HashtableEntry<uint8> *>(oldTable)); 
         break;
#endif

         case TABLE_INDEX_TYPE_UINT16: 
#ifndef MUSCLE_AVOID_MINIMIZED_HASHTABLES
            delete [] (static_cast<HashtableEntry<uint16>*>(oldTable)); 
         break;
#endif

         default:
            delete [] (static_cast<HashtableEntry<uint32>*>(oldTable)); 
         break;
      }
   }
}

template <class KeyType, class ValueType, class HashFunctorType>
float
HashtableBase<KeyType,ValueType,HashFunctorType>::CountAverageLookupComparisons(bool printStatistics) const
{
   Hashtable<uint32,uint32> histogram;
   uint32 chainCount = 0;
   if (_table)
   {
      for (uint32 i=0; i<_tableSize; i++)
      {
         const HashtableEntryBase * e = this->IndexToEntryUnchecked(i);
         if (IsBucketHead(e))
         {
            chainCount++;

            uint32 chainSize = 0;
            while(e)
            {
               chainSize++;
               e = this->GetEntryBucketNextChecked(e);
            }
            uint32 * count = histogram.GetOrPut(chainSize);
            if (count) (*count)++;
         }
      }
   }
   histogram.SortByKey();

   const uint32 totalNumItems = GetNumItems();
   if (printStatistics) printf("Hashtable statistics:  " UINT32_FORMAT_SPEC " items in table, " UINT32_FORMAT_SPEC " slots allocated, " UINT32_FORMAT_SPEC " chains.\n", totalNumItems, _tableSize, chainCount);
   if (totalNumItems > 0)
   {
      uint64 totalCounts = 0, totalExtras = 0;
      for (HashtableIterator<uint32, uint32> iter(histogram); iter.HasData(); iter++)
      {
         const uint32 curChainSize           = iter.GetKey();
         const uint32 numChainsOfThisSize    = iter.GetValue();
         const uint32 numItemsInCurChainSize = numChainsOfThisSize*curChainSize;
         if (printStatistics) printf("  " UINT32_FORMAT_SPEC " chains of size " UINT32_FORMAT_SPEC " (aka %.3f%% of items)\n", numChainsOfThisSize, curChainSize, (100.0f*numItemsInCurChainSize)/totalNumItems);
         totalCounts += ((uint64)numItemsInCurChainSize)*curChainSize;
         totalExtras += ((uint64)numItemsInCurChainSize)*(curChainSize-1);
      }
      const float ret = (((float)totalExtras)/(2.0f*totalNumItems))+1.0f;
      if (printStatistics) printf("Average chain length is %.3f.  Average lookup requires %.3f key-comparisons.\n", ((float)totalCounts)/totalNumItems, ret);
      return ret;
   }
   else return 0.0f;
}

template <class KeyType, class ValueType, class HashFunctorType>
uint32 
HashtableBase<KeyType,ValueType,HashFunctorType>::Remove(const HashtableBase & pairs)
{
   uint32 removeCount = 0;
   if (&pairs == this)
   {
      removeCount = GetNumItems();
      Clear();
   }
   else
   {
      HashtableEntryBase * e = pairs.IndexToEntryChecked(pairs._iterHeadIdx);
      while(e)
      {
         if (RemoveAux(e->_hash, e->_key, NULL).IsOK()) removeCount++;
         e = pairs.GetEntryIterNextChecked(e);
      }
   }
   return removeCount;
}

template <class KeyType, class ValueType, class HashFunctorType>
uint32 
HashtableBase<KeyType,ValueType,HashFunctorType>::Intersect(const HashtableBase & pairs)
{
   uint32 removeCount = 0;
   if (&pairs != this)
   {
      HashtableEntryBase * e = this->IndexToEntryChecked(_iterHeadIdx);
      while(e)
      {
         HashtableEntryBase * next = this->GetEntryIterNextChecked(e); // save this first, since we might be erasing (e)
         if ((pairs.GetEntry(e->_hash, e->_key) == NULL)&&(this->RemoveAux(e->_hash, e->_key, NULL).IsOK())) removeCount++;
         e = next;
      }
   }
   return removeCount;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveToFront(const KeyType & moveMe)
{
   HashtableEntryBase * e = this->GetEntry(this->ComputeHash(moveMe), moveMe);
   if (e == NULL) return B_DATA_NOT_FOUND;
   this->MoveToFrontAux(e);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveToBack(const KeyType & moveMe)
{
   HashtableEntryBase * e = this->GetEntry(this->ComputeHash(moveMe), moveMe);
   if (e == NULL) return B_DATA_NOT_FOUND;
   this->MoveToBackAux(e);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveToBefore(const KeyType & moveMe, const KeyType & toBeforeMe)
{
   if (HasItems())
   {
      HashtableEntryBase * e = this->GetEntry(this->ComputeHash(moveMe),     moveMe);
      HashtableEntryBase * f = this->GetEntry(this->ComputeHash(toBeforeMe), toBeforeMe);
      if ((e == NULL)||(f == NULL)) return B_DATA_NOT_FOUND;
      if (e == f)                   return B_BAD_ARGUMENT;
      this->MoveToBeforeAux(e, f);
      return B_NO_ERROR;
   }
   else return B_DATA_NOT_FOUND;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveToBehind(const KeyType & moveMe, const KeyType & toBehindMe)
{
   if (HasItems())
   {
      HashtableEntryBase * d = this->GetEntry(this->ComputeHash(toBehindMe), toBehindMe);
      HashtableEntryBase * e = this->GetEntry(this->ComputeHash(moveMe),     moveMe);
      if ((d == NULL)||(e == NULL)) return B_DATA_NOT_FOUND;
      if (d == e)                   return B_BAD_ARGUMENT;
      this->MoveToBehindAux(e, d);
      return B_NO_ERROR;
   }
   else return B_DATA_NOT_FOUND;
}

template <class KeyType, class ValueType, class HashFunctorType>
status_t 
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveToPosition(const KeyType & moveMe, uint32 idx)
{
   HashtableEntryBase * e = this->GetEntry(this->ComputeHash(moveMe), moveMe);
   if (e == NULL) return B_DATA_NOT_FOUND;
   this->MoveToPositionAux(e, idx);
   return B_NO_ERROR;
}

// Adds (e) to the our iteration linked list, behind (optBehindThis), or at the head if (optBehindThis) is NULL.
template <class KeyType, class ValueType, class HashFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::InsertIterationEntry(HashtableEntryBase * e, HashtableEntryBase * optBehindThis)
{
   this->SetEntryIterPrevChecked(e, optBehindThis);
   this->SetEntryIterNext(e, optBehindThis ? this->GetEntryIterNext(optBehindThis) : this->_iterHeadIdx);

   HashtableEntryBase * prev = this->GetEntryIterPrevChecked(e);
   if (prev) this->SetEntryIterNextUnchecked(prev, e);
        else this->_iterHeadIdx = this->EntryToIndexUnchecked(e);

   HashtableEntryBase * next = this->GetEntryIterNextChecked(e);
   if (next) this->SetEntryIterPrevUnchecked(next, e);
        else this->_iterTailIdx = this->EntryToIndexUnchecked(e);
}

// Remove (e) from our iteration linked list
template <class KeyType, class ValueType, class HashFunctorType>
void 
HashtableBase<KeyType,ValueType,HashFunctorType>::RemoveIterationEntry(HashtableEntryBase * e)
{
   // Update any iterators that were pointing at (e), so that they now point to the entry after e.
   // That way, on the next (iter++) they will move to the entry after the now-removed e, as God intended.
   IteratorType * next = this->_iterList;
   while(next)
   {
      if (next->_iterCookie == e) 
      {
         if (next->_scratchKeyAndValue.IsObjectConstructed() == false) next->SetScratchValues(e->_key, e->_value);
         next->_iterCookie = GetSubsequentEntry(next->_iterCookie, next->_flags);
         next->UpdateKeyAndValuePointers();
      }
      next = next->_nextIter;
   }

   HashtableEntryBase * prevNode = this->GetEntryIterPrevChecked(e);
   HashtableEntryBase * nextNode = this->GetEntryIterNextChecked(e);
   if (this->IndexToEntryChecked(_iterHeadIdx) == e) this->_iterHeadIdx = this->EntryToIndexChecked(nextNode);
   if (this->IndexToEntryChecked(_iterTailIdx) == e) this->_iterTailIdx = this->EntryToIndexChecked(prevNode);
   if (prevNode) this->SetEntryIterNextChecked(prevNode, nextNode);
   if (nextNode) this->SetEntryIterPrevChecked(nextNode, prevNode);
   this->SetEntryIterPrev(e, MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
   this->SetEntryIterNext(e, MUSCLE_HASHTABLE_INVALID_SLOT_INDEX);
}

template <class KeyType, class ValueType, class HashFunctorType>
template <class EntryCompareFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::InsertIterationEntryInOrder(const EntryCompareFunctorType & ecf, HashtableEntryBase * e, bool isAutoSortEnabled, void * compareCookie)
{
   HashtableEntryBase * insertAfter = this->IndexToEntryChecked(this->_iterTailIdx);  // default to appending to the end of the list
   if ((isAutoSortEnabled)&&(this->_iterHeadIdx != MUSCLE_HASHTABLE_INVALID_SLOT_INDEX))
   {
      // We're in sorted mode, so we'll try to place this guy in the correct position.
           if (ecf.Compare(*e, *this->IndexToEntryUnchecked(this->_iterHeadIdx), compareCookie) < 0) insertAfter = NULL;  // easy; append to the head of the list
      else if (ecf.Compare(*e, *this->IndexToEntryUnchecked(this->_iterTailIdx), compareCookie) < 0)  // only iterate through if we're before the tail, otherwise the tail is fine
      {
         HashtableEntryBase * prev = this->IndexToEntryUnchecked(this->_iterHeadIdx);
         HashtableEntryBase * next = this->GetEntryIterNextChecked(prev);  // more difficult;  find where to insert into the middle
         while(next)
         {
            if (ecf.Compare(*e, *next, compareCookie) < 0)
            {
               insertAfter = prev;
               break;
            }
            else 
            {
               prev = next;
               next = this->GetEntryIterNextChecked(next);
            }
         }   
      }
   }
   this->InsertIterationEntry(e, insertAfter);
}

template <class KeyType, class ValueType, class HashFunctorType>
template <class EntryCompareFunctorType>
void
HashtableBase<KeyType,ValueType,HashFunctorType>::MoveIterationEntryToCorrectPosition(const EntryCompareFunctorType & ecf, HashtableEntryBase * e, void * compareCookie)
{
   HashtableEntryBase * b;
   if (((b = this->GetEntryIterPrevChecked(e)) != NULL)&&(ecf.Compare(*e, *b, compareCookie) < 0))
   {
      if (ecf.Compare(*e, *this->IndexToEntryUnchecked(this->_iterHeadIdx), compareCookie) < 0) this->MoveToFrontAux(e);
      else
      {
         HashtableEntryBase * prev;
         while(((prev = this->GetEntryIterPrevChecked(b)) != NULL)&&(ecf.Compare(*e, *prev, compareCookie) < 0)) b = prev;
         this->MoveToBeforeAux(e, b);
      }
   }
   else if (((b = this->GetEntryIterNextChecked(e)) != NULL)&&(ecf.Compare(*e, *b, compareCookie) > 0))
   {
      if (ecf.Compare(*e, *this->IndexToEntryUnchecked(this->_iterTailIdx), compareCookie) > 0) this->MoveToBackAux(e);
      else
      {
         HashtableEntryBase * next;
         while(((next = this->GetEntryIterNextChecked(b)) != NULL)&&(ecf.Compare(*e, *next, compareCookie) > 0)) b = next;
         this->MoveToBehindAux(e, b);
      }
   }
}

/// @endcond

//===============================================================
// Implementation of HashtableMid
//===============================================================

// CopyFrom() method is similar to the assignment operator, but gives a return value.
template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
template<class RHSHashFunctorType>
status_t
HashtableMid<KeyType, ValueType, HashFunctorType, SubclassType> ::
CopyFrom(const HashtableBase<KeyType, ValueType, RHSHashFunctorType> & rhs, bool clearFirst)
{
   if (((const void *)(this)) == ((const void *)(&rhs))) return B_NO_ERROR;

   if (clearFirst) this->Clear((rhs.IsEmpty())&&(this->_tableSize>MUSCLE_HASHTABLE_DEFAULT_CAPACITY));  // FogBugz #10274
   if (rhs.HasItems())
   {
      MRETURN_ON_ERROR(EnsureSize(this->GetNumItems()+rhs.GetNumItems()));
      MRETURN_ON_ERROR(this->EnsureTableAllocated());

      this->CopyFromAux(rhs);
      static_cast<SubclassType *>(this)->SortAux();  // We do the sort (if any) at the end, since that is more efficient than traversing the list after every insert
   }
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
status_t
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::EnsureSize(uint32 requestedSize, bool allowShrink)
{
   const uint32 biggerTableSize = muscleMax(this->_numItems, allowShrink?requestedSize:muscleMax(requestedSize,this->_tableSize));
   if (biggerTableSize == this->_tableSize) return B_NO_ERROR;      // no point in continuing if the new table's size will equal what we have now
   if (biggerTableSize == 0)  // in the case where the user wants to shrink an empty table's array to zero, we can handle that via Clear(true)
   {
      this->Clear(true);
      return B_NO_ERROR;
   }

   // 1. Initialize the scratch space for our active iterators.
   {
      IteratorType * nextIter = this->_iterList;
      while(nextIter)
      {
         this->SetIteratorScratchSpace(*nextIter, NULL);
         nextIter = this->GetIteratorNextIterator(*nextIter);
      }
   }
    
   // 2. Create a new, bigger table, to hold a copy of our data.
   SubclassType biggerTable;
   biggerTable._tableSize      = biggerTableSize;
#ifndef MUSCLE_HASHTABLE_EXCLUDE_TABLE_INDEX_TYPE_FIELD
   biggerTable._tableIndexType = this->ComputeTableIndexTypeForTableSize(biggerTable._tableSize);
#endif
   biggerTable.DisableAutoSort();  // he doesn't need to auto-sort as our entries will already be in the correct order

   // 3. Place all of our data into (biggerTable)
   {
      HashtableEntryBaseType * next = this->IndexToEntryChecked(this->_iterHeadIdx);
      while(next)
      {
         HashtableEntryBaseType * hisClone = biggerTable.PutAux(next->_hash, HT_PlunderKey(next->_key), HT_PlunderValue(next->_value), NULL, NULL);
         if (hisClone)
         {
            // Mark any iterators that will need to be redirected to point to the new nodes.
            IteratorType * nextIter = this->_iterList;
            while(nextIter)
            {
               if (this->GetIteratorNextCookie(*nextIter) == next) this->SetIteratorScratchSpace(*nextIter, hisClone);
               nextIter = this->GetIteratorNextIterator(*nextIter);
            }
         }
         else return B_OUT_OF_MEMORY;

         next = this->GetEntryIterNextChecked(next);
      }
   }

   // 4. Swap contents with the bigger table, but don't swap iterator lists (we want to keep ours!)
   this->SwapContentsAux(biggerTable, false);

   // 5. Lastly, fix up our iterators to point to their new entries.
   {
      IteratorType * nextIter = this->_iterList;
      while(nextIter)
      {
         IteratorType & ni = *nextIter;
         this->SetIteratorNextCookie(ni, this->GetIteratorScratchSpace(ni));
         this->UpdateIteratorKeyAndValuePointers(ni);
         nextIter = this->GetIteratorNextIterator(ni);
      }
   }

#ifdef MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS
   if (this->GetNumItems() > 16)
   {
      const float av = this->CountAverageLookupComparisons();
# if MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS >= 100
      if (av >= ((MUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS)/100.0f))
# else
      if (av >= 2.0f)
# endif
      {
         LogTime(MUSCLE_LOG_WARNING, "Hashtable had average lookup comparison count of %f.  Printing statistics and stack trace to stdout.\n", av);
         (void) this->CountAverageLookupComparisons(true); 
         PrintStackTrace();
      }
   }
#endif
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
template <class RHSHashFunctorType, class RHSSubclassType>
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::CopyToTable(const KeyType & copyMe, HashtableMid<KeyType, ValueType, RHSHashFunctorType, RHSSubclassType> & toTable) const
{
   const uint32 hash = this->ComputeHash(copyMe);
   HashtableEntryBaseType * e = this->GetEntry(hash, copyMe);
   if (e)
   { 
      if (this == &toTable) return B_NO_ERROR;  // it's already here!
      if (toTable.PutAux(hash, copyMe, e->_value, NULL, NULL) != NULL) return B_NO_ERROR;
   }
   return B_BAD_ARGUMENT;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
template <class RHSHashFunctorType, class RHSSubclassType>
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::MoveToTable(const KeyType & moveMe, HashtableMid<KeyType, ValueType, RHSHashFunctorType, RHSSubclassType> & toTable)
{
   const uint32 hash = this->ComputeHash(moveMe);
   HashtableEntryBaseType * e = this->GetEntry(hash, moveMe);
   if (e)
   {
      if (this == &toTable) return B_NO_ERROR;  // it's already here!
      if (toTable.PutAux(hash, moveMe, HT_PlunderValue(e->_value), NULL, NULL) != NULL) return this->RemoveAux(e->_hash, moveMe, NULL);
   }
   return B_BAD_ARGUMENT;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
typename HashtableBase<KeyType,ValueType,HashFunctorType>::HashtableEntryBase *
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutAux(uint32 hash, HT_SinkKeyParam key, HT_SinkValueParam value, ValueType * optSetPreviousValue, bool * optReplacedFlag)
{
   if (optReplacedFlag) *optReplacedFlag = false;
   if (this->EnsureTableAllocated().IsError()) return NULL;

   // If we already have an entry for this key in the table, we can just replace its contents
   HashtableEntryBaseType * e = this->GetEntry(hash, key);
   if (e)
   {
      if (optSetPreviousValue) *optSetPreviousValue = e->_value;
      if (optReplacedFlag)     *optReplacedFlag     = true;
      e->_value = HT_ForwardValue(value);
      static_cast<SubclassType*>(this)->MoveIterationEntryToCorrectPositionAux(e);
      return e;
   }

   // Rehash the table if the threshold is exceeded
   if (this->_numItems == this->_tableSize) 
   {
      // Avoid dangling-pointer-issues in the case where we need to realloc the array, 
      // but our arguments are pointing into the old array that is about to be freed inside EnsureSize()
      if ((this->IsKeyLocatedInThisContainer(key))||(this->IsValueLocatedInThisContainer(value)))
      {
         const KeyType   tempKey = key;
         const ValueType tempVal = value;
         return PutAux(hash, tempKey, tempVal, optSetPreviousValue, optReplacedFlag);  // go again
      }
      return (EnsureSize(this->_tableSize*2).IsOK()) ? PutAux(hash, HT_ForwardKey(key), HT_ForwardValue(value), optSetPreviousValue, optReplacedFlag) : NULL;
   }

   e = this->PutAuxAux(hash, HT_ForwardKey(key), HT_ForwardValue(value));
   static_cast<SubclassType*>(this)->InsertIterationEntryAux(e);

   this->_numItems++;
   return e; 
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutAtFront(HT_SinkKeyParam key, HT_SinkValueParam v)
{
   HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(v), NULL, NULL);
   if (e == NULL) return B_OUT_OF_MEMORY;
   this->MoveToFrontAux(e);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutAtBack(HT_SinkKeyParam key, HT_SinkValueParam v)
{
   HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(v), NULL, NULL);
   if (e == NULL) return B_OUT_OF_MEMORY;
   this->MoveToBackAux(e);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutBefore(HT_SinkKeyParam key, HT_SinkKeyParam placeBeforeMe, HT_SinkValueParam v)
{
   HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(v), NULL, NULL);
   if (e == NULL) return B_OUT_OF_MEMORY;
   HashtableEntryBaseType * f = this->GetEntry(this->ComputeHash(placeBeforeMe), placeBeforeMe);
   if ((f)&&(e != f)) this->MoveToBeforeAux(e, f);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutBehind(HT_SinkKeyParam key, HT_SinkKeyParam placeBehindMe, HT_SinkValueParam v)
{
   HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(v), NULL, NULL);
   if (e == NULL) return B_OUT_OF_MEMORY;
   HashtableEntryBaseType * d = this->GetEntry(this->ComputeHash(placeBehindMe), placeBehindMe);
   if ((d)&&(e != d)) this->MoveToBehindAux(e, d);
   return B_NO_ERROR;
}

template <class KeyType, class ValueType, class HashFunctorType, class SubclassType>
HT_UniversalSinkKeyValueRef
status_t 
HashtableMid<KeyType,ValueType,HashFunctorType,SubclassType>::PutAtPosition(HT_SinkKeyParam key, uint32 atPosition, HT_SinkValueParam v)
{
   HashtableEntryBaseType * e = PutAux(this->ComputeHash(key), HT_ForwardKey(key), HT_ForwardValue(v), NULL, NULL);
   if (e == NULL) return B_OUT_OF_MEMORY;
   this->MoveToPositionAux(e, atPosition);
   return B_NO_ERROR;
}

//===============================================================
// Implementation of HashtableIterator
//===============================================================

template <class KeyType, class ValueType, class HashFunctorType>
HashtableIterator<KeyType, ValueType, HashFunctorType>::HashtableIterator() 
   : _iterCookie(NULL)
   , _currentKey(NULL)
   , _currentVal(NULL)
   , _flags(0)
   , _owner(NULL)
   , _okayToUnsetThreadID(false)
{
   // empty
}

template <class KeyType, class ValueType, class HashFunctorType>
HashtableIterator<KeyType, ValueType, HashFunctorType>::HashtableIterator(const HashtableIterator & rhs) 
   : _flags(0)
   , _owner(NULL)
   , _okayToUnsetThreadID(false)
{
   *this = rhs;
}

template <class KeyType, class ValueType, class HashFunctorType>
HashtableIterator<KeyType, ValueType, HashFunctorType>::HashtableIterator(const HashtableBase<KeyType, ValueType, HashFunctorType> & table, uint32 flags) 
   : _flags(flags)
   , _owner(&table)
   , _okayToUnsetThreadID(false)
{
   table.InitializeIterator(*this);
}

template <class KeyType, class ValueType, class HashFunctorType>
HT_UniversalSinkKeyRef
HashtableIterator<KeyType, ValueType, HashFunctorType>::HashtableIterator(const HashtableBase<KeyType, ValueType, HashFunctorType> & table, HT_SinkKeyParam startAt, uint32 flags) 
   : _flags(flags)
   , _owner(&table)
   , _okayToUnsetThreadID(false)
{
   table.InitializeIteratorAt(*this, HT_ForwardKey(startAt));
}

template <class KeyType, class ValueType, class HashFunctorType>
HashtableIterator<KeyType, ValueType, HashFunctorType>::~HashtableIterator()
{
   if (_owner) _owner->UnregisterIterator(this);
}

template <class KeyType, class ValueType, class HashFunctorType>
HashtableIterator<KeyType,ValueType,HashFunctorType> &
HashtableIterator<KeyType,ValueType,HashFunctorType>:: operator=(const HashtableIterator<KeyType,ValueType,HashFunctorType> & rhs)
{
   if (this != &rhs)
   {
      if (_owner) _owner->UnregisterIterator(this);
      _flags = rhs._flags;   // must be done while unregistered, in case NOREGISTER flag changes state
      _owner = rhs._owner;
      _scratchKeyAndValue = rhs._scratchKeyAndValue;
      if (_owner) _owner->RegisterIterator(this);

      _iterCookie = rhs._iterCookie;
      UpdateKeyAndValuePointers();
   }
   return *this;
}

} // end namespace muscle

#endif
