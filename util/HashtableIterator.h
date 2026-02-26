/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleHashtableIterator_h
#define MuscleHashtableIterator_h

#include "support/MuscleSupport.h"
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

// Forward declarations
template <class KeyType, class ValueType, class HashFunctorType>                     class HashtableIterator;
template <class KeyType, class ValueType, class HashFunctorType>                     class ConstHashtableIterator;
template <class KeyType, class ValueType, class HashFunctorType>                     class HashtableBase;
template <class KeyType, class ValueType, class HashFunctorType, class SubclassType> class HashtableMid;

/** These flags can be passed to the HashtableIterator constructor (or to the GetIterator()/GetIteratorAt()
  * functions in the Hashtable class) to modify the iterator's behaviour.
  */
enum {
   HTIT_FLAG_BACKWARDS  = (1<<0), /**< iterate backwards. */
   HTIT_FLAG_NOREGISTER = (1<<1), /**< don't register the iterator object with Hashtable object */
};

namespace muscle_private {

template <class KeyType, class ValueType, class HashFunctorType> class HashtableIteratorImp;

// Internal/private implementation of the HashtableIterator class
template <class KeyType, class ValueType, class HashFunctorType> class MUSCLE_NODISCARD HashtableIteratorImp MUSCLE_FINAL_CLASS
{
public:
   /** Convenience typedef for the type of Hashtable this HashtableIteratorImp is associated with. */
   typedef HashtableBase<KeyType, ValueType, HashFunctorType> HashtableType;

   HashtableIteratorImp();
   HashtableIteratorImp(const HashtableIteratorImp & rhs);
   HashtableIteratorImp(const HashtableType & table, uint32 flags = 0);

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   HashtableIteratorImp(HashtableIteratorImp && rhs) MUSCLE_NOEXCEPT;
   HashtableIteratorImp(HashtableType && table, uint32 flags = 0) = delete;
   HashtableIteratorImp & operator=(HashtableIteratorImp && rhs) {SwapContentsAux(rhs, true); return *this;}
#endif

   HT_UniversalSinkKeyRef HashtableIteratorImp(const HashtableType & table, HT_SinkKeyParam startAt, uint32 flags);
   ~HashtableIteratorImp();

   HashtableIteratorImp & operator=(const HashtableIteratorImp & rhs);

   void operator++(int)
   {
      if (_scratchKeyAndValue.EnsureObjectDestructed() == false) _iterCookie = _owner ? _owner->GetSubsequentEntry(_iterCookie, _flags) : NULL;
      UpdateKeyAndValuePointers();
   }

   void operator--(int) {const bool b = IsBackwards(); SetBackwards(!b); (*this)++; SetBackwards(b);}

   MUSCLE_NODISCARD bool HasData() const {return (_currentKey != NULL);}

   MUSCLE_NODISCARD const KeyType & GetKey() const
   {
#ifdef __clang_analyzer__
      assert(_currentKey != NULL);
#endif
      return *_currentKey;
   }

   MUSCLE_NODISCARD ValueType & GetValue() const
   {
#ifdef __clang_analyzer__
      assert(_currentVal != NULL);
#endif
      return *_currentVal;
   }

   MUSCLE_NODISCARD const ValueType & GetValueConst() const
   {
#ifdef __clang_analyzer__
      assert(_currentVal != NULL);
#endif
      return *_currentVal;
   }

   MUSCLE_NODISCARD uint32 GetFlags() const {return _flags;}
   void SetBackwards(bool backwards) {if (backwards) _flags |= HTIT_FLAG_BACKWARDS; else _flags &= ~HTIT_FLAG_BACKWARDS;}
   MUSCLE_NODISCARD bool IsBackwards() const {return ((_flags & HTIT_FLAG_BACKWARDS) != 0);}
   MUSCLE_NODISCARD bool IsAtStart() const {return ((HasData())&&(_currentKey == (IsBackwards() ? _owner->GetLastKey() : _owner->GetFirstKey())));}
   MUSCLE_NODISCARD bool IsAtEnd() const {return ((HasData())&&(_currentKey == (IsBackwards() ? _owner->GetFirstKey() : _owner->GetLastKey())));}
   void SwapContents(HashtableIteratorImp & swapMe) MUSCLE_NOEXCEPT {SwapContentsAux(swapMe, false);}
   void SwapContentsAux(HashtableIteratorImp & swapMe, bool swapMeIsGoingAway) MUSCLE_NOEXCEPT;

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

   void * _iterCookie;           // points to the HashtableEntryBase object that we are currently associated with
   const KeyType * _currentKey;  // cached result, so that GetKey() can be a branch-free inline method
   ValueType * _currentVal;      // cached result, so that GetValue() can be a branch-free inline method

   uint32 _flags;
   HashtableIteratorImp * _prevIter; // for the doubly linked list so that the table can notify us if it is modified
   HashtableIteratorImp * _nextIter; // for the doubly linked list so that the table can notify us if it is modified
   const HashtableType * _owner;  // table that we are associated with
   void * _scratchSpace;         // ignore this; it's temp scratch space used by EnsureSize().

   // Used for emergency storage of scratch values
   class KeyAndValue
   {
   public:
      KeyAndValue() : _key(), _value() {/* empty */}
      HT_UniversalSinkKeyValueRef KeyAndValue(HT_SinkKeyParam key, HT_SinkValueParam value) : _key(HT_ForwardKey(key)), _value(HT_ForwardValue(value)) {/* empty */}

      KeyType _key;
      ValueType _value;
   };
   DemandConstructedObject<KeyAndValue> _scratchKeyAndValue;
   bool _okayToUnsetThreadID;
};

}  // end namespace muscle_private

/**
 * This class is a read/write iterator object, used for iterating over the set
 * of keys or values in a non-const Hashtable.  Note that the Hashtable class
 * maintains the ordering of its keys and values, unlike many hash table
 * implementations.
 *
 * Given a Hashtable object, you can obtain one or more of these
 * iterator objects by calling the Hashtable's GetIterator() method;
 * or you can just specify the Hashtable you want to iterate over as
 * an argument to the HashtableIterator constructor.
 *
 * The most common form for a Hashtable iteration is this:<pre>
 * for (auto iter = table.GetIterator(); iter.HasData(); iter++)
 * {
 *    const String & nextKey = iter.GetKey();
 *    int nextValue = iter.GetValue();
 *    [...]
 * }</pre>
 *
 * It is safe to modify or delete a Hashtable during an iteration-traversal (from the same
 * thread only); any HashtableIterators that are in the middle of iterating over the Hashtable
 * will be automatically notified about the modification, so that they can do the right thing
 * (and in particular, not continue to point to any no-longer-existing hashtable entries).
 * @tparam KeyType the type of the keys of the key-value pairs in the Hashtable that this object will iterate over.
 * @tparam ValueType the type of the values of the key-value pairs in the Hashtable that this object will iterate over.
 * @tparam HashFunctorType the type of the hash functor to use to calculate hashes of keys in the hash table.  If not specified, an appropriate type will be chosen via SFINAE magic.
 */
template <class KeyType, class ValueType, class HashFunctorType = typename AutoChooseHashFunctorHelper<KeyType>::Type > class MUSCLE_NODISCARD HashtableIterator MUSCLE_FINAL_CLASS
{
public:
   /** Convenience typedef for the type of Hashtable this HashtableIterator is associated with. */
   typedef HashtableBase<KeyType, ValueType, HashFunctorType> HashtableType;

   /** Default constructor.  */
   HashtableIterator() {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   HashtableIterator(const HashtableIterator & rhs) : _imp(rhs._imp) {/* empty */}

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIterator().
     * @param table the Hashtable to iterate over.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Defaults to zero for default behaviour.
     */
   HashtableIterator(HashtableType & table, uint32 flags = 0) : _imp(table, flags) {/* empty */}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   HashtableIterator(HashtableIterator && rhs) MUSCLE_NOEXCEPT : _imp(std_move_if_available(rhs._imp)) {/* empty */}

   /** This constructor is declared deleted to keep HashtableIterators from being accidentally associated with temporary objects */
   HashtableIterator(HashtableType && table, uint32 flags = 0) = delete;

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   HashtableIterator & operator=(HashtableIterator && rhs) {_imp.SwapContentsAux(rhs._imp, true); return *this;}
#endif

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIteratorAt().
     * @param table the Hashtable to iterate over.
     * @param startAt the first key that should be returned by the iteration.  If (startAt) is not in the table,
     *                the iterator will not return any results.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Set to zero to get the default behaviour.
     */
   HT_UniversalSinkKeyRef HashtableIterator(HashtableType & table, HT_SinkKeyParam startAt, uint32 flags) : _imp(table, startAt, flags) {/* empty */}

   /** Destructor */
   ~HashtableIterator() {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   HashtableIterator & operator=(const HashtableIterator & rhs) {_imp = rhs._imp; return *this;}

   /** Advances this iterator by one entry in the table.  */
   void operator++(int) {_imp++;}

   /** Retracts this iterator by one entry in the table.  The opposite of the ++ operator. */
   void operator--(int) {_imp--;}

   /** Returns true iff this iterator is pointing to valid key/value data.  Do not call GetKey() or GetValue()
     * unless this method returns true!  Note that the value returned by this method can change if the
     * Hashtable is modified.
     */
   MUSCLE_NODISCARD bool HasData() const {return _imp.HasData();}

   /**
    * Returns a reference to the key this iterator is currently pointing at.  This method does not change the state of the iterator.
    * @note This method may only be called if the iterator is pointing to a valid entry (i.e. if HasData() returns true).
    *       Calling it when HasData()==false will invoke undefined behavior!
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   MUSCLE_NODISCARD const KeyType & GetKey() const {return _imp.GetKey();}

   /**
    * Returns a reference to the value this iterator is currently pointing at.
    * @note This method may only be called if the iterator is pointing to a valid entry (i.e. if HasData() returns true).
    *       Calling it when HasData()==false will invoke undefined behavior!
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   MUSCLE_NODISCARD ValueType & GetValue() const {return _imp.GetValue();}

   /** Returns this iterator's HTIT_FLAG_* bit-chord value. */
   MUSCLE_NODISCARD uint32 GetFlags() const {return _imp.GetFlags();}

   /** Sets or unsets the HTIT_FLAG_BACKWARDS flag on this iterator.
     * @param backwards If true, this iterator will be set to iterate backwards from wherever it is currently;
     *                  if false, this iterator will be set to iterate forwards from wherever it is currently.
     */
   void SetBackwards(bool backwards) {_imp.SetBackwards(backwards);}

   /** Returns true iff this iterator is set to iterate in reverse order -- ie if HTIT_FLAG_BACKWARDS
     * was passed in to the constructor, or if SetBackwards(true) was called.
     */
   MUSCLE_NODISCARD bool IsBackwards() const {return _imp.IsBackwards();}

   /** Convenience method.  Returns true iff we are currently referencing the first key/value pair in our iteration-sequence */
   MUSCLE_NODISCARD bool IsAtStart() const {return _imp.IsAtStart();}

   /** Convenience method.  Returns true iff we are currently referencing the final key/value pair in our iteration-sequence */
   MUSCLE_NODISCARD bool IsAtEnd() const {return _imp.IsAtEnd();}

   /** This method swaps the state of this iterator with the iterator in the argument.
    *  @param swapMe The iterator whose state we are to swap with
    */
   void SwapContents(HashtableIterator & swapMe) MUSCLE_NOEXCEPT {_imp.SwapContents(swapMe._imp);}

private:
   friend class HashtableBase<KeyType, ValueType, HashFunctorType>;
   friend class ConstHashtableIterator<KeyType, ValueType, HashFunctorType>;

   muscle_private::HashtableIteratorImp<KeyType, ValueType, HashFunctorType> _imp;
};

/**
 * This class is a read-only iterator object, used for iterating over the set
 * of keys or values in a const Hashtable.  Note that the Hashtable class
 * maintains the ordering of its keys and values, unlike many hash table
 * implementations.
 *
 * Given a Hashtable object, you can obtain one or more of these
 * iterator objects by calling the Hashtable's GetIterator() method;
 * or you can just specify the Hashtable you want to iterate over as
 * an argument to the HashtableIterator constructor.
 *
 * The most common form for a Hashtable iteration is this:<pre>
 * for (auto iter = table.GetIterator(); iter.HasData(); iter++)
 * {
 *    const String & nextKey = iter.GetKey();
 *    int nextValue = iter.GetValue();
 *    [...]
 * }</pre>
 *
 * It is safe to modify or delete a Hashtable during an iteration-traversal (from the same
 * thread only); any HashtableIterators that are in the middle of iterating over the Hashtable
 * will be automatically notified about the modification, so that they can do the right thing
 * (and in particular, not continue to point to any no-longer-existing hashtable entries).
 * @tparam KeyType the type of the keys of the key-value pairs in the Hashtable that this object will iterate over.
 * @tparam ValueType the type of the values of the key-value pairs in the Hashtable that this object will iterate over.
 * @tparam HashFunctorType the type of the hash functor to use to calculate hashes of keys in the hash table.  If not specified, an appropriate type will be chosen via SFINAE magic.
 */
template <class KeyType, class ValueType, class HashFunctorType = typename AutoChooseHashFunctorHelper<KeyType>::Type > class MUSCLE_NODISCARD ConstHashtableIterator MUSCLE_FINAL_CLASS
{
public:
   /** Convenience typedef for the type of Hashtable this ConstHashtableIterator is associated with. */
   typedef HashtableBase<KeyType, ValueType, HashFunctorType> HashtableType;

   /** Default constructor. */
   ConstHashtableIterator() {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   ConstHashtableIterator(const ConstHashtableIterator & rhs) : _imp(rhs._imp) {/* empty */}

   /** Pseudo-copy-constructor for creating a ConstHashtableIterator from a HashtableIterator
     * @param rhs the HashtableIterator to make this a read-only copy of
     */
   ConstHashtableIterator(const HashtableIterator<KeyType, ValueType, HashFunctorType> & rhs) : _imp(rhs._imp) {/* empty */}

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIterator().
     * @param table the Hashtable to iterate over.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Defaults to zero for default behaviour.
     */
   ConstHashtableIterator(const HashtableType & table, uint32 flags = 0) : _imp(table, flags) {/* empty */}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   ConstHashtableIterator(ConstHashtableIterator && rhs) MUSCLE_NOEXCEPT : _imp(std_move_if_available(rhs._imp)) {/* empty */}

   /** This constructor is declared deleted to keep ConstHashtableIterators from being accidentally associated with temporary objects */
   ConstHashtableIterator(HashtableType && table, uint32 flags = 0) = delete;

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   ConstHashtableIterator & operator=(ConstHashtableIterator && rhs) {_imp.SwapContentsAux(rhs._imp, true); return *this;}
#endif

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by table.GetIteratorAt().
     * @param table the Hashtable to iterate over.
     * @param startAt the first key that should be returned by the iteration.  If (startAt) is not in the table,
     *                the iterator will not return any results.
     * @param flags A bit-chord of HTIT_FLAG_* constants (see above).  Set to zero to get the default behaviour.
     */
   HT_UniversalSinkKeyRef ConstHashtableIterator(const HashtableType & table, HT_SinkKeyParam startAt, uint32 flags) : _imp(table, startAt, flags) {/* empty */}

   /** Destructor */
   ~ConstHashtableIterator() {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   ConstHashtableIterator & operator=(const ConstHashtableIterator & rhs) {_imp = rhs._imp; return *this;}

   /** Pseudo-assignment operator for setting a ConstHashtableIterator equal to a HashtableIterator.
     * @param rhs the HashtableIterator to make this a read-only copy of
     */
   ConstHashtableIterator & operator=(const HashtableIterator<KeyType, ValueType, HashFunctorType> & rhs) {_imp = rhs._imp; return *this;}

   /** Advances this iterator by one entry in the table.  */
   void operator++(int) {_imp++;}

   /** Retracts this iterator by one entry in the table.  The opposite of the ++ operator. */
   void operator--(int) {_imp--;}

   /** Returns true iff this iterator is pointing to valid key/value data.  Do not call GetKey() or GetValue()
     * unless this method returns true!  Note that the value returned by this method can change if the
     * Hashtable is modified.
     */
   MUSCLE_NODISCARD bool HasData() const {return _imp.HasData();}

   /**
    * Returns a reference to the key this iterator is currently pointing at.  This method does not change the state of the iterator.
    * @note This method may only be called if the iterator is pointing to a valid entry (i.e. if HasData() returns true).
    *       Calling it when HasData()==false will invoke undefined behavior!
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   MUSCLE_NODISCARD const KeyType & GetKey() const {return _imp.GetKey();}

   /**
    * Returns a read-only reference to the value this iterator is currently pointing at.
    * @note This method may only be called if the iterator is pointing to a valid entry (i.e. if HasData() returns true).
    *       Calling it when HasData()==false will invoke undefined behavior!
    * @note The returned reference is only guaranteed to remain valid for as long as the Hashtable remains unchanged.
    */
   MUSCLE_NODISCARD const ValueType & GetValue() const {return _imp.GetValueConst();}

   /** Returns this iterator's HTIT_FLAG_* bit-chord value. */
   MUSCLE_NODISCARD uint32 GetFlags() const {return _imp.GetFlags();}

   /** Sets or unsets the HTIT_FLAG_BACKWARDS flag on this iterator.
     * @param backwards If true, this iterator will be set to iterate backwards from wherever it is currently;
     *                  if false, this iterator will be set to iterate forwards from wherever it is currently.
     */
   void SetBackwards(bool backwards) {_imp.SetBackwards(backwards);}

   /** Returns true iff this iterator is set to iterate in reverse order -- ie if HTIT_FLAG_BACKWARDS
     * was passed in to the constructor, or if SetBackwards(true) was called.
     */
   MUSCLE_NODISCARD bool IsBackwards() const {return _imp.IsBackwards();}

   /** Convenience method.  Returns true iff we are currently referencing the first key/value pair in our iteration-sequence */
   MUSCLE_NODISCARD bool IsAtStart() const {return _imp.IsAtStart();}

   /** Convenience method.  Returns true iff we are currently referencing the final key/value pair in our iteration-sequence */
   MUSCLE_NODISCARD bool IsAtEnd() const {return _imp.IsAtEnd();}

   /** This method swaps the state of this iterator with the iterator in the argument.
    *  @param swapMe The iterator whose state we are to swap with
    */
   void SwapContents(ConstHashtableIterator & swapMe) MUSCLE_NOEXCEPT {_imp.SwapContents(swapMe._imp);}

private:
   friend class HashtableBase<KeyType, ValueType, HashFunctorType>;

   muscle_private::HashtableIteratorImp<KeyType, ValueType, HashFunctorType> _imp;
};

} // end namespace muscle

#endif
