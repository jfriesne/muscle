/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQueue_h
#define MuscleQueue_h

#ifndef MUSCLE_AVOID_CPLUSPLUS11
# include <initializer_list>
#endif

#include "support/MuscleSupport.h"
#include "support/NotCopyable.h"

namespace muscle {

#ifndef SMALL_QUEUE_SIZE
/** Specifies the number of items that should be held "inline" inside a Queue object.  Any Queue will be able to hold up to this many items without having to do a separate heap allocation.  Defaults to 3 if not specified otherwise by the compiler (eg via -DSMALL_QUEUE_SIZE=5) */
# define SMALL_QUEUE_SIZE 3
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
# ifndef MUSCLE_AVOID_CPLUSPLUS11
// Enable move semantics (when possible) for C++11
#  define QQ_UniversalSinkItemRef template<typename ItemParam>
#  define QQ_SinkItemParam  ItemParam &&
#  define QQ_PlunderItem(item) std::move(item)
#  define QQ_ForwardItem(item) std::forward<ItemParam>(item)
# else
// For earlier versions of C++, use the traditional copy/ref semantics
#  define QQ_UniversalSinkItemRef
#  define QQ_SinkItemParam const ItemType &
#  define QQ_PlunderItem(item) (item)
#  define QQ_ForwardItem(item) (item)
# endif
#endif

/** This class implements a templated double-ended-queue data structure.
 *  Adding or removing items from the head or tail of a Queue is (on average)
 *  an O(1) operation.  A Queue can also serve as a reasonably efficient resizable-array
 *  class (aka Vector) if that is all you need.
 *  @tparam ItemType the type of object to be stored in this Queue.
 */
template <class ItemType> class MUSCLE_NODISCARD Queue MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  */
   Queue();

   /** Explicitly-sized constructor
     * @param preallocatedItemSlotsCount how many slots should be preallocated for this table
     */
   explicit Queue(PreallocatedItemSlotsCount preallocatedItemSlotsCount);

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   Queue(const Queue& rhs);

   /** Destructor. */
   ~Queue();

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   Queue & operator=(const Queue & rhs);

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** Initializer-list Constructor (C++11 only)
     * @param list the initializer-list of items to add to this Queue
     */
   Queue(const std::initializer_list<ItemType> & list)
      : _queue(NULL)
      , _queueSize(0)
      , _itemCount(0)
      , _headIndex(0)  // initialization not strictly necessary, but
      , _tailIndex(0)  // here anyway to keep the static analyzers happy
   {
      (void) AddTailMulti(list);
   }

   /** @copydoc DoxyTemplate::DoxyTemplate(DoxyTemplate &&) */
   Queue(Queue && rhs) MUSCLE_NOEXCEPT
      : _queue(NULL)
      , _queueSize(0)
      , _itemCount(0)
      , _headIndex(0)  // initialization not strictly necessary, but
      , _tailIndex(0)  // here anyway to keep the static analyzers happy
   {
      Plunder(rhs);
   }

   /** @copydoc DoxyTemplate::operator=(DoxyTemplate &&) */
   Queue & operator =(Queue && rhs) MUSCLE_NOEXCEPT
   {
      Plunder(rhs);
      return *this;
   }
#endif

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator==(const Queue & rhs) const;

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator!=(const Queue & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator<(const DoxyTemplate &) const */
   bool operator<(const Queue & rhs) const {return (lexicographicalCompare(rhs) < 0);}

   /** @copydoc DoxyTemplate::operator<=(const DoxyTemplate &) const */
   bool operator<=(const Queue & rhs) const {return (lexicographicalCompare(rhs) <= 0);}

   /** @copydoc DoxyTemplate::operator>(const DoxyTemplate &) const */
   bool operator>(const Queue & rhs) const {return (lexicographicalCompare(rhs) > 0);}

   /** @copydoc DoxyTemplate::operator>=(const DoxyTemplate &) const */
   bool operator>=(const Queue & rhs) const {return (lexicographicalCompare(rhs) >= 0);}

   /** Calculates and returns a hash code for this Queue.  The value returned by this
     * method depends on both the ordering and the contents of the items in the Queue.
     * @param itemHashFunctor reference to a hash-functor object to use to calculate hash values for our items.
     * @note This method is only callable if our item-type is hashable
     */
   template<class ItemHashFunctorType> MUSCLE_NODISCARD uint32 HashCode(const ItemHashFunctorType & itemHashFunctor) const;

   /** Calculates and returns a hash code for this Queue using the default hash-functor for our item type.
     * The value returned by this method depends on both the ordering and the contents of the items in this Queue.
     * @note This method is only callable if our item-type is hashable
     */
   MUSCLE_NODISCARD uint32 HashCode() const
   {
      typename DEFAULT_HASH_FUNCTOR(ItemType) hashFunctor;
      return HashCode<>(hashFunctor);
   }

   /** Calculates and returns a checksum for this Queue by calling CalculatePODHashCode() on each item.
     * @note the ordering of the items in the Queue is significant in the computation of the checksum
     */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const
   {
      uint32 ret = 0;
      for (uint32 i=0; i<GetNumItems(); i++) ret += ((i+1)*CalculatePODChecksum((*this)[i]));
      return ret;
   }

   /** Similar to the assignment operator, except this method returns a status code.
     * @param rhs This Queue's contents will become a copy of (rhs)'s items.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t CopyFrom(const Queue<ItemType> & rhs);

   /** Appends (item) to the end of the queue.  Queue size grows by one.
    *  @param item The item to append.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   QQ_UniversalSinkItemRef status_t AddTail(QQ_SinkItemParam item) {return (AddTailAndGet(QQ_ForwardItem(item)) != NULL) ? B_NO_ERROR : B_OUT_OF_MEMORY;}

   /** As above, except that a default-initialized item is appended.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddTail() {return AddTail(GetDefaultItem());}

   /** Appends some or all items in (queue) to the end of our queue.  Queue size
    *  grows by at most (queue.GetNumItems()).
    *  For example:
    *    Queue<int> a;  // contains 1, 2, 3, 4
    *    Queue<int> b;  // contains 5, 6, 7, 8
    *    a.AddTail(b);  // a now contains 1, 2, 3, 4, 5, 6, 7, 8
    *  @param queue The queue to append to our queue.
    *  @param startIndex Index in (queue) to start adding at.  Default to zero.
    *  @param numItems Number of items to add.  If this number is too large, it will be capped appropriately.  Default is to add all items.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddTailMulti(const Queue<ItemType> & queue, uint32 startIndex = 0, uint32 numItems = MUSCLE_NO_LIMIT);

   /** Adds the given array of items to the tail of the Queue.  Equivalent
    *  to adding them to the tail of the Queue one at a time, but somewhat
    *  more efficient.  On success, the queue size grows by (numItems).
    *  @param items Pointer to an array of items to add to the Queue.
    *  @param numItems Number of items in the array
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddTailMulti(const ItemType * items, uint32 numItems);

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** Available in C++11 only:  Appends the items specified in the initializer
    *  list to this Queue.
    *  @param list The C++11 initializer list of items (eg {1,2,3,4,5} to add.
    *  @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddTailMulti(const std::initializer_list<ItemType> & list)
   {
      MRETURN_ON_ERROR(EnsureCanAdd((uint32) list.size()));
      for (auto i : list) (void) AddTail(i);  // can't fail, because we allocated the necessary space on the previous line
      return B_NO_ERROR;
   }
#endif

   /** Convenience method:  Appends (item) to the end of the queue, if no object equal to (item) is already present in the Queue.
    *  @param item The item to append.
    *  @note this method runs in O(N) time, so be careful about using it on large Queues (maybe use a Hashtable instead?)
    *  @return B_NO_ERROR on success (or if an equivalent item was already present), or B_OUT_OF_MEMORY on failure.
    */
   QQ_UniversalSinkItemRef status_t AddTailIfNotAlreadyPresent(QQ_SinkItemParam item) {return Contains(item) ? B_NO_ERROR : AddTail(item);}

   /** Appends (item) to the end of the queue.  Queue size grows by one.
    *  @param item The item to append.
    *  @return A pointer to the appended item on success, or a NULL pointer on failure.
    */
   QQ_UniversalSinkItemRef MUSCLE_NODISCARD ItemType * AddTailAndGet(QQ_SinkItemParam item);

   /** As above, except that an item is appended.
    *  @return A pointer to the appended item on success, or a NULL pointer on failure.
    *  @note that for POD ItemTypes, the appended item will be in an unitialized state.
    */
   MUSCLE_NODISCARD ItemType * AddTailAndGet();

   /** Prepends (item) to the head of the queue.  Queue size grows by one.
    *  @param item The item to prepend.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   QQ_UniversalSinkItemRef status_t AddHead(QQ_SinkItemParam item) {return (AddHeadAndGet(QQ_ForwardItem(item)) != NULL) ? B_NO_ERROR : B_OUT_OF_MEMORY;}

   /** As above, except that a default-initialized item is prepended.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddHead() {return AddHead(GetDefaultItem());}

   /** Concatenates (queue) to the head of our queue.
    *  Our queue size grows by at most (queue.GetNumItems()).
    *  For example:
    *    Queue<int> a;  // contains 1, 2, 3, 4
    *    Queue<int> b;  // contains 5, 6, 7, 8
    *    a.AddHead(b);  // a now contains 5, 6, 7, 8, 1, 2, 3, 4
    *  @param queue The queue to prepend to our queue.
    *  @param startIndex Index in (queue) to start adding at.  Default to zero.
    *  @param numItems Number of items to add.  If this number is too large, it will be capped appropriately.  Default is to add all items.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddHeadMulti(const Queue<ItemType> & queue, uint32 startIndex = 0, uint32 numItems = MUSCLE_NO_LIMIT);

   /** Concatenates the given array of items to the head of the Queue.
    *  The semantics are the same as for AddHeadMulti(const Queue<ItemType> &).
    *  @param items Pointer to an array of items to add to the Queue.
    *  @param numItems Number of items in the array
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t AddHeadMulti(const ItemType * items, uint32 numItems);

   /** Convenience method:  Prepends (item) to the head of the queue, if no object equal to (item) is already present in the Queue.
    *  @param item The item to prepend.
    *  @note this method runs in O(N) time, so be careful about using it on large Queues (maybe use a Hashtable instead?)
    *  @return B_NO_ERROR on success (or if an equivalent item was already present), or B_OUT_OF_MEMORY on failure.
    */
   QQ_UniversalSinkItemRef status_t AddHeadIfNotAlreadyPresent(QQ_SinkItemParam item) {return Contains(item) ? B_NO_ERROR : AddHead(item);}

   /** Prepends (item) to the beginning of the queue.  Queue size grows by one.
    *  @param item The item to prepend.
    *  @return A pointer to the prepend item on success, or a NULL pointer on failure.
    */
   QQ_UniversalSinkItemRef MUSCLE_NODISCARD ItemType * AddHeadAndGet(QQ_SinkItemParam item);

   /** As above, except that an item is prepended.
    *  @return A pointer to the prepended item on success, or a NULL pointer on failure.
    *  @note that for POD ItemTypes, the prepended item will be in an unitialized state.
    */
   MUSCLE_NODISCARD ItemType * AddHeadAndGet();

   /** Removes the item at the head of the queue.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the queue was already empty.
    */
   status_t RemoveHead();

   /** Removes the item at the head of the queue and returns it.
     * If the Queue was empty, a default item is returned.
     */
   ItemType RemoveHeadWithDefault();

   /** Removes up to (numItems) items from the head of the queue.
     * @param numItems The desired number of items to remove
     * @returns the actual number of items removed (may be less than (numItems) if the Queue was too short)
     */
   uint32 RemoveHeadMulti(uint32 numItems);

   /** Removes the item at the head of the queue and places it into (returnItem).
    *  @param returnItem On success, the removed item is copied into this object.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the queue was empty
    */
   status_t RemoveHead(ItemType & returnItem);

   /** Removes the item at the tail of the queue.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the queue was empty.
    */
   status_t RemoveTail();

   /** Removes the item at the tail of the queue and places it into (returnItem).
    *  @param returnItem On success, the removed item is copied into this object.
    *  @return B_NO_ERROR on success, or B_DATA_NOT_FOUND if the queue was empty
    */
   status_t RemoveTail(ItemType & returnItem);

   /** Removes the item at the tail of the queue and returns it.
     * If the Queue was empty, a default item is returned.
     */
   ItemType RemoveTailWithDefault();

   /** Removes up to (numItems) items from the tail of the queue.
     * @param numItems The desired number of items to remove
     * @returns the actual number of items removed (may be less than (numItems) if the Queue was too short)
     */
   uint32 RemoveTailMulti(uint32 numItems);

   /** Removes the item at the (index)'th position in the queue.
    *  @param index Which item to remove--can range from zero
    *               (head of the queue) to GetNumItems()-1 (tail of the queue).
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure (ie bad index)
    *  Note that this method is somewhat inefficient for indices that
    *  aren't at the head or tail of the queue (ie O(n) time)
    */
   status_t RemoveItemAt(uint32 index);

   /** Removes the item at the (index)'th position in the queue, and copies it into (returnItem).
    *  @param index Which item to remove--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @param returnItem On success, the removed item is copied into this object.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure (ie bad index)
    */
   status_t RemoveItemAt(uint32 index, ItemType & returnItem);

   /** Removes the nth item in the Queue and returns it.
    *  If there was no nth item in the Queue, a default item is returned.
    *  @param index Index of the item to remove and return.
    */
   ItemType RemoveItemAtWithDefault(uint32 index);

   /** Copies the (index)'th item into (returnItem).
    *  @param index Which item to get--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @param returnItem On success, the retrieved item is copied into this object.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure (eg bad index)
    */
   status_t GetItemAt(uint32 index, ItemType & returnItem) const;

   /** Returns a pointer to an item in the array (ie no copying of the item is done).
    *  Included for efficiency; be careful with this: modifying the queue can invalidate
    *  the returned pointer!
    *  @param index Index of the item to return a pointer to.
    *  @return a pointer to the internally held item, or NULL if (index) was invalid.
    */
   MUSCLE_NODISCARD ItemType * GetItemAt(uint32 index) const {return IsIndexValid(index)?GetItemAtUnchecked(index):NULL;}

   /** The same as GetItemAt(), except this version doesn't check to make sure
    *  (index) is valid.
    *  @param index Index of the item to return a pointer to.  Must be a valid index!
    *  @return a pointer to the internally held item.  The returned value is undefined
    *          if the index isn't valid, so be careful!
    */
   MUSCLE_NODISCARD MUSCLE_NEVER_RETURNS_NULL ItemType * GetItemAtUnchecked(uint32 index) const {return &_queue[InternalizeIndex(index)];}

   /** Returns a reference to the (index)'th item in the Queue, if such an item exists,
     * or a reference to a default item if it doesn't.  Unlike the [] operator,
     * it is okay to call this method with any value of (index).
     * @param index Which item to return.
     */
   MUSCLE_NODISCARD const ItemType & GetWithDefault(uint32 index) const {return IsIndexValid(index)?(*this)[index]:GetDefaultItem();}

   /** Returns a copy of the (index)'th item in the Queue, if such an item exists,
     * or the supplied default item if it doesn't.  Unlike the [] operator,
     * it is okay to call this method with any value of (index).
     * @param index Which item to return.
     * @param defItem An item to return if (index) isn't a valid value.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defItem) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   MUSCLE_NODISCARD ItemType GetWithDefault(uint32 index, const ItemType & defItem) const {return IsIndexValid(index)?(*this)[index]:defItem;}

   /** Sets all items in this Queue to be equal to the argument
     * @param newItem The item to set everything in this Queue equal to
     */
   void ReplaceAllItems(const ItemType & newItem) {for (uint32 i=0; i<GetNumItems(); i++) (*this)[i] = newItem;}

   /** Replaces the (index)'th item in the queue with (newItem).
    *  @param index Which item to replace--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @param newItem The item to place into the queue at the (index)'th position.
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure (eg bad index)
    */
   QQ_UniversalSinkItemRef status_t ReplaceItemAt(uint32 index, QQ_SinkItemParam newItem);

   /** As above, except the specified item is replaced with a default-initialized item.
    *  @param index Which item to replace--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @return B_NO_ERROR on success, or B_BAD_ARGUMENT on failure (eg bad index)
    */
   status_t ReplaceItemAt(uint32 index) {return ReplaceItemAt(index, GetDefaultItem());}

   /** Inserts (item) into the (nth) slot in the array.  InsertItemAt(0)
    *  is the same as AddHead(item), InsertItemAt(GetNumItems()) (or larger) is the same
    *  as AddTail(item).  Other positions will involve an O(n) shifting of contents.
    *  @param index The position at which to insert the new item.
    *  @param newItem The item to insert into the queue.
    *  @return B_NO_ERROR on success, or an error value on failure (out of memory?)
    */
   QQ_UniversalSinkItemRef status_t InsertItemAt(uint32 index, QQ_SinkItemParam newItem);

   /** As above, except that a default-initialized item is inserted.
    *  @param index The position at which to insert the new item.
    *  @return B_NO_ERROR on success, or an error value on failure (out of memory?)
    */
   status_t InsertItemAt(uint32 index) {return InsertItemAt(index, GetDefaultItem());}

   /** Inserts some or all of the items in (queue) at the specified position in our queue.
    *  Queue size grows by at most (queue.GetNumItems()).
    *  For example:
    *    Queue<int> a;   // contains 1, 2, 3, 4
    *    Queue<int> b;   // contains 5, 6, 7, 8
    *    a.InsertItemsAt(2, b); // a now contains 1, 2, 5, 6, 7, 8, 3, 4
    *  @param index The index into this Queue that items from (queue) should be inserted at.
    *  @param queue The queue whose items are to be inserted into this queue.
    *  @param startIndex Index in (queue) to start reading items from.  Defaults to zero.
    *  @param numNewItems Number of items to insert.  If this number is too large, it will be capped appropriately.  Default is to add all items.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t InsertItemsAt(uint32 index, const Queue<ItemType> & queue, uint32 startIndex = 0, uint32 numNewItems = MUSCLE_NO_LIMIT);

   /** Inserts the items pointed at by (items) at the specified position in our queue.
    *  Queue size grows (numNewItems).
    *  For example:
    *    Queue<int> a;       // contains 1, 2, 3, 4
    *    const int b[] = {5, 6, 7};
    *    a.InsertItemsAt(2, b, ARRAYITEMS(b));  // a now contains 1, 2, 5, 6, 7, 3, 4
    *  @param index The index into this Queue that items from (queue) should be inserted at.
    *  @param items Pointer to the items to insert into this queue.
    *  @param numNewItems Number of items to insert.
    *  @return B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t InsertItemsAt(uint32 index, const ItemType * items, uint32 numNewItems);

   /** Removes all items from the queue.
    *  @param releaseCachedBuffers If true, we will immediately free any buffers that we may be holding.  Otherwise
    *                              we will keep them around so that they can be re-used in the future.
    *  @note on return, the Queue is guaranteed to be empty and normalized.
    */
   void Clear(bool releaseCachedBuffers = false);

   /** This version of Clear() merely sets our held item-count to zero; it doesn't actually modify the
     * state of any items in the Queue.  This is very efficient, but be careful when using it with non-POD
     * types; for example, if you have a Queue<MessageRef> and you call FastClear() on it, the Messages
     * referenced by the MessageRef objects will not get recycled during the Clear() call because the
     * MessageRefs still exist in the Queue's internal array, even though they aren't readily accessible
     * anymore.  Only call this method if you know what you are doing!
     * @note on return, the Queue is guaranteed to be empty and normalized.
     */
   void FastClear() {_itemCount = _headIndex = _tailIndex = 0;}

   /** Returns the number of items in the queue.  (This number does not include pre-allocated space) */
   MUSCLE_NODISCARD uint32 GetNumItems() const {return _itemCount;}

   /** Returns the total number of item-slots we have allocated space for.  Note that this is NOT
    *  the same as the value returned by GetNumItems() -- it is generally larger, since we often
    *  pre-allocate additional slots in advance, in order to cut down on the number of re-allocations
    *  we need to peform.
    */
   MUSCLE_NODISCARD uint32 GetNumAllocatedItemSlots() const {return _queueSize;}

   /** Returns the number of "extra" (ie currently unoccupied) array slots we currently have allocated.
     * Attempting to add more than (this many) additional items to this Queue will cause a memory reallocation.
     */
   MUSCLE_NODISCARD uint32 GetNumUnusedItemSlots() const {return _queueSize-_itemCount;}

   /** Returns the number of bytes of memory taken up by this Queue's data */
   MUSCLE_NODISCARD uint32 GetTotalDataSize() const {return sizeof(*this)+(GetNumAllocatedItemSlots()*sizeof(ItemType));}

   /** Convenience method:  Returns true iff there are no items in the queue. */
   MUSCLE_NODISCARD bool IsEmpty() const {return (_itemCount == 0);}

   /** Convenience method:  Returns true iff there is at least one item in the queue. */
   MUSCLE_NODISCARD bool HasItems() const {return (_itemCount > 0);}

   /** Convenience method:  Returns true iff (index) is less than the number of valid items currently in the Queue.
     * @param index an index to test to see if it's valid
     */
   MUSCLE_NODISCARD bool IsIndexValid(uint32 index) const {return (index < _itemCount);}

   /** Returns a read-only reference the head item in the queue.  You must not call this when the queue is empty! */
   MUSCLE_NODISCARD const ItemType & Head() const {return *GetItemAtUnchecked(0);}

   /** Returns a read-only reference the tail item in the queue.  You must not call this when the queue is empty! */
   MUSCLE_NODISCARD const ItemType & Tail() const {return *GetItemAtUnchecked(_itemCount-1);}

   /** Returns a read-only reference the head item in the queue, or a default item if the Queue is empty. */
   MUSCLE_NODISCARD const ItemType & HeadWithDefault() const {return HasItems() ? Head() : GetDefaultItem();}

   /** Returns a read-only reference the head item in the queue, or to the supplied default item if the Queue is empty.
     * @param defaultItem An item to return if the Queue is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defItem) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   MUSCLE_NODISCARD ItemType HeadWithDefault(const ItemType & defaultItem) const {return HasItems() ? Head() : defaultItem;}

   /** Returns a read-only reference the tail item in the queue, or a default item if the Queue is empty. */
   MUSCLE_NODISCARD const ItemType & TailWithDefault() const {return HasItems() ? Tail() : GetDefaultItem();}

   /** Returns a read-only reference the tail item in the queue, or to the supplied default item if the Queue is empty.
     * @param defaultItem An item to return if the Queue is empty.
     * @note that this method returns by-value rather than by-reference, because
     *       returning (defItem) by-reference makes it too easy to call this method
     *       in a way that would cause it to return a dangling-reference-to-a-temporary-object.
     */
   MUSCLE_NODISCARD ItemType TailWithDefault(const ItemType & defaultItem) const {return HasItems() ? Tail() : defaultItem;}

   /** Returns a writable reference the head item in the queue.  You must not call this when the queue is empty! */
   MUSCLE_NODISCARD ItemType & Head() {return *GetItemAtUnchecked(0);}

   /** Returns a writable reference the tail item in the queue.  You must not call this when the queue is empty! */
   MUSCLE_NODISCARD ItemType & Tail() {return *GetItemAtUnchecked(_itemCount-1);}

   /** Returns a pointer to the first item in the queue, or NULL if the queue is empty */
   MUSCLE_NODISCARD ItemType * HeadPointer() const {return GetItemAt(0);}

   /** Returns a pointer to the last item in the queue, or NULL if the queue is empty */
   MUSCLE_NODISCARD ItemType * TailPointer() const {return GetItemAt(_itemCount-1);}

   /** Convenient read-only array-style operator (be sure to only use valid indices!)
     * @param index the index of the item to get (between 0 and (GetNumItems()-1), inclusive)
     */
   const ItemType & operator [](uint32 index) const;

   /** Convenient read-write array-style operator (be sure to only use valid indices!)
     * @param index the index of the item to get (between 0 and (GetNumItems()-1), inclusive)
     */
   ItemType & operator [](uint32 index);

   /** Makes sure there is enough space allocated for at least (numSlots) items.
    *  You only need to call this if you wish to minimize the number of data re-allocations done,
    *  or wish to add or remove a large number of default items at once (by specifying setNumItems=true).
    *  @param numSlots the minimum amount of items to pre-allocate space for in the Queue.
    *  @param setNumItems If true, the length of the Queue will be altered by adding or removing
    *                     items to (from) the tail of the Queue until the Queue is the specified size.
    *                     If false (the default), more slots may be pre-allocated, but the
    *                     number of items officially in the Queue remains the same as before.
    *  @param extraReallocItems If we have to do an array reallocation, this many extra slots will be
    *                           added to the newly allocated array, above and beyond what is strictly
    *                           necessary.  That way the likelihood of another reallocation being necessary
    *                           in the near future is reduced.  Default value is zero, indicating that
    *                           no extra slots will be allocated.  This argument is ignored if (setNumItems) is true.
    *  @param allowShrink If set to true, the array will be reallocated even if the new array size is smaller than the
    *                     existing size.  Defaults to false (ie only reallocate if the desired size is greater than
    *                     the existing size).
    *  @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
    */
   status_t EnsureSize(uint32 numSlots, bool setNumItems = false, uint32 extraReallocItems = 0, bool allowShrink = false) {return EnsureSizeAux(numSlots, setNumItems, extraReallocItems, NULL, allowShrink);}

   /** Convenience wrapper around EnsureSize():  This method ensures that this Queue has enough
     * extra space allocated to fit another (numExtraSlots) items without having to do a reallocation.
     * If it doesn't, it will do a reallocation so that it does have at least that much extra space.
     * @param numExtraSlots How many extra items we want to ensure room for.  Defaults to 1.
     * @returns B_NO_ERROR if the extra space now exists, or B_OUT_OF_MEMORY on failure.
     */
   status_t EnsureCanAdd(uint32 numExtraSlots = 1) {return EnsureSize(GetNumItems()+numExtraSlots);}

   /** Convenience wrapper around EnsureSize():  This method shrinks the Queue so that its size is
     * equal to the size of the data it contains, plus (numExtraSlots).
     * @param numExtraSlots the number of extra empty slots the Queue should contains after the shrink.
     *                      Defaults to zero.
     * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure.
     */
   status_t ShrinkToFit(uint32 numExtraSlots = 0) {return EnsureSize(GetNumItems()+numExtraSlots, false, 0, true);}

   /** Convenience method -- works the same as IndexOf() but returns a boolean instead of an int32 index.
    *  @param item The item to look for.
    *  @param startAt The first index in the list to look at.  Defaults to zero.
    *  @param endAtPlusOne One more than the final index to look at.  If this value is greater than
    *               the number of items in the list, it will be clamped internally to be equal
    *               to the number of items in the list.  Defaults to MUSCLE_NO_LIMIT.
    *  @return True if the item was found in the specified range, or false otherwise.
    */
   MUSCLE_NODISCARD bool Contains(const ItemType & item, uint32 startAt = 0, uint32 endAtPlusOne = MUSCLE_NO_LIMIT) const {return (IndexOf(item, startAt, endAtPlusOne) >= 0);}

   /** Returns the first index of the given (item), or -1 if (item) is not found in the list.  O(n) search time.
    *  @param item The item to look for.
    *  @param startAt The first index in the list to look at.  Defaults to zero.
    *  @param endAtPlusOne One more than the final index to look at.  If this value is greater than
    *               the number of items in the list, it will be clamped internally to be equal
    *               to the number of items in the list.  Defaults to MUSCLE_NO_LIMIT.
    *  @return The index of the first item found, or -1 if no such item was found in the specified range.
    */
   MUSCLE_NODISCARD int32 IndexOf(const ItemType & item, uint32 startAt = 0, uint32 endAtPlusOne = MUSCLE_NO_LIMIT) const;

   /** Returns the last index of the given (item), or -1 if (item) is not found in the list.  O(n) search time.
    *  This method is different from IndexOf() in that this method searches backwards in the list.
    *  @param item The item to look for.
    *  @param startAt The initial index in the list to look at.  If this value is greater than or equal to
    *                 the size of the list, it will be clamped down to (numItems-1).   Defaults to MUSCLE_NO_LIMIT.
    *  @param endAt The final index in the list to look at.  Defaults to zero, which means
    *               to search back to the beginning of the list, if necessary.
    *  @return The index of the first item found in the reverse search, or -1 if no such item was found in the specified range.
    */
   MUSCLE_NODISCARD int32 LastIndexOf(const ItemType & item, uint32 startAt = MUSCLE_NO_LIMIT, uint32 endAt = 0) const;

   /**
    *  Swaps the values of the two items at the given indices.  This operation
    *  will involve three copies of the held items.
    *  @param fromIndex First index to swap.
    *  @param toIndex   Second index to swap.
    */
   void Swap(uint32 fromIndex, uint32 toIndex);

   /**
    *  Reverses the ordering of the items in the given subrange.
    *  (eg if the items were A,B,C,D,E, this would change them to E,D,C,B,A)
    *  @param from Index of the start of the subrange.  Defaults to zero.
    *  @param to Index of the next item after the end of the subrange.  If greater than
    *         the number of items currently in the queue, this value will be clipped
    *         to be equal to that number.  Defaults to the largest possible uint32.
    */
   void ReverseItemOrdering(uint32 from=0, uint32 to = MUSCLE_NO_LIMIT);

   /**
    *  Does an in-place, stable sort on a subrange of the contents of this Queue.  (The sort algorithm is a smart, in-place merge sort)
    *  @param compareFunctor the item-comparison functor to use when sorting the items in the Queue.
    *  @param from Index of the start of the subrange.  Defaults to zero.
    *  @param to Index of the next item after the end of the subrange.
    *         If greater than the number of items currently in the queue,
    *         the subrange will extend to the last item.  Defaults to the largest possible uint32.
    *  @param optCookie A user-defined value that will be passed to the comparison functor.
    */
   template <class CompareFunctorType> void Sort(const CompareFunctorType & compareFunctor, uint32 from=0, uint32 to = MUSCLE_NO_LIMIT, void * optCookie = NULL);

   /**
    *  Same as above, except that the default item-comparison functor for our ItemType is implicitly used.
    *  @param from Index of the start of the subrange.  Defaults to zero.
    *  @param to Index of the next item after the end of the subrange.
    *         If greater than the number of items currently in the queue,
    *         the subrange will extend to the last item.  Defaults to the largest possible uint32.
    *  @param optCookie A user-defined value that will be passed to the comparison functor.
    */
   void Sort(uint32 from=0, uint32 to = MUSCLE_NO_LIMIT, void * optCookie = NULL) {Sort(CompareFunctor<ItemType>(), from, to, optCookie);}

   /**
    * Inserts the specified item at the position necessary to keep the Queue in sorted order.
    * Note that this method assumes the Queue is already in sorted order before the insertion.
    * @param item The item to insert into the Queue.
    * @param optCookie optional value to pass to the comparison functor.  Defaults to NULL.
    * @returns The index at which the item was inserted on success, or -1 on failure (out of memory)?
    */
   QQ_UniversalSinkItemRef int32 InsertItemAtSortedPosition(QQ_SinkItemParam item, void * optCookie = NULL) {return InsertItemAtSortedPosition(CompareFunctor<ItemType>(), item, optCookie);}

   /**
    * Inserts the specified item at the position necessary to keep the Queue in sorted order.
    * Note that this method assumes the Queue is already in sorted order before the insertion.
    * @param compareFunctor a functor object whose Compare() method is called to do item comparisons.
    * @param item The item to insert into the Queue.
    * @param optCookie optional value to pass to the comparison functor.  Defaults to NULL.
    * @returns The index at which the item was inserted on success, or -1 on failure (out of memory)?
    */
   QQ_UniversalSinkItemRef int32 InsertItemAtSortedPosition(const CompareFunctor<ItemType> & compareFunctor, QQ_SinkItemParam item, void * optCookie = NULL);

   /**
    *  Swaps our contents with the contents of (that), in an efficient manner.
    *  @param that The queue whose contents are to be swapped with our own.
    */
   void SwapContents(Queue<ItemType> & that);  // note:  can't be MUSCLE_NOEXCEPT because we may copy items

   /**
    *  Goes through the array and removes every item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return The number of instances of (val) that were found and removed during this operation.
    */
   uint32 RemoveAllInstancesOf(const ItemType & val);

   /**
    *  Sorts the Queue, then iterates through the items and
    *  removes any duplicate items (ie any items that are == to each other)
    *  such that there is only at most a single instance of any given value
    *  left in the Queue.
    *  @return The number of duplicate items that were found and
    *          removed during this operation.
    */
   uint32 RemoveDuplicateItems();

   /** Same as RemoveDuplicateItems(), except this version assumes that
    *  the items are already in sorted order.
    *  @return The number of duplicate items that were found and
    *          removed during this operation.
    */
   uint32 RemoveSortedDuplicateItems();

   /**
    *  Goes through the array and removes the first item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return B_NO_ERROR if a matching item was found and removed, or B_DATA_NOT_FOUND if it wasn't found.
    */
   status_t RemoveFirstInstanceOf(const ItemType & val);

   /**
    *  Goes through the array and removes the last item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return B_NO_ERROR if a matching item was found and removed, or B_DATA_NOT_FOUND if it wasn't found.
    */
   status_t RemoveLastInstanceOf(const ItemType & val);

   /** Returns true iff the first item in our queue is equal to (prefix).
     * @param prefix the item to check for at the head of this Queue
     */
   MUSCLE_NODISCARD bool StartsWith(const ItemType & prefix) const {return ((HasItems())&&(Head() == prefix));}

   /** Returns true iff the (prefixQueue) is a prefix of this queue.
     * @param prefixQueue the items to check for at the head of this Queue
     */
   MUSCLE_NODISCARD bool StartsWith(const Queue<ItemType> & prefixQueue) const;

   /** Returns true iff the last item in our queue is equal to (suffix).
     * @param suffix the item to check for at the tail of this Queue
     */
   MUSCLE_NODISCARD bool EndsWith(const ItemType & suffix) const {return ((HasItems())&&(Tail() == suffix));}

   /** Returns true iff the (suffixQueue) is a suffix of this queue.
     * @param suffixQueue the list of items to check for at the tail of this Queue
     */
   MUSCLE_NODISCARD bool EndsWith(const Queue<ItemType> & suffixQueue) const;

   /**
    *  Returns a pointer to the nth internally-held contiguous-Item-sub-array, to allow efficient
    *  array-style access to groups of items in this Queue.  In the current implementation
    *  there may be as many as two such sub-arrays present, depending on the internal state of the Queue.
    *  @param whichArray Index of the internal array to return a pointer to.  Typically zero or one.
    *  @param retLength The number of items in the returned sub-array will be written here.
    *  @return Pointer to the first item in the sub-array on success, or NULL on failure.
    *          Note that this array is only guaranteed valid as long as no items are
    *          added or removed from the Queue.
    */
   MUSCLE_NODISCARD ItemType * GetArrayPointer(uint32 whichArray, uint32 & retLength) {return const_cast<ItemType *>(GetArrayPointerAux(whichArray, retLength));}

   /** Read-only version of the above
    *  @param whichArray Index of the internal array to return a pointer to.  Typically zero or one.
    *  @param retLength The number of items in the returned sub-array will be written here.
    *  @return Pointer to the first item in the sub-array on success, or NULL on failure.
    *          Note that this array is only guaranteed valid as long as no items are
    *          added or removed from the Queue.
    */
   MUSCLE_NODISCARD const ItemType * GetArrayPointer(uint32 whichArray, uint32 & retLength) const {return GetArrayPointerAux(whichArray, retLength);}

   /** Normalizes the layout of the items held in this Queue so that they are guaranteed to be contiguous
     * in memory.  This is useful if you want to pass pointers to items in this array in to functions
     * that expect C arrays.  Note that prepending items to this Queue may de-normalize it again.
     * Note also that this method is O(N) if the array needs normalizing.
     * (It's a no-op if the Queue is already normalized, of course)
     */
   void Normalize();

   /** Returns true iff this Queue is currently normalized -- that is, if its contents are arranged
     * in the normal C-array ordering.  Returns false otherwise.  Call Normalize() if you want to
     * make sure that the data is normalized.
     */
   MUSCLE_NODISCARD bool IsNormalized() const {return ((IsEmpty())||(_headIndex <= _tailIndex));}

   /** Returns true iff (val) is physically located in this container's internal items array.
     * @param val Reference to an item.
     */
   MUSCLE_NODISCARD bool IsItemLocatedInThisContainer(const ItemType & val) const {return ((HasItems())&&((uintptr)((&val)-_queue) < (uintptr)GetNumItems()));}

   /** Returns a read-only reference to a default-constructed item of this Queue's type.
     * This item is guaranteed to remain valid for the life of the process.
     */
   MUSCLE_NODISCARD const ItemType & GetDefaultItem() const {return GetDefaultObjectForType<ItemType>();}

   /** Returns a pointer to our internally held array of items.  Note that this array's items are not guaranteed
     * to be stored in order -- in particular, the items may be "wrapped around" the end of the array, with the
     * first items in the sequence appearing near the end of the array.  Only use this function if you know exactly
     * what you are doing!
     */
   MUSCLE_NODISCARD ItemType * GetRawArrayPointer() {return _queue;}

   /** As above, but provides read-only access */
   MUSCLE_NODISCARD const ItemType * GetRawArrayPointer() const {return _queue;}

   /** Causes this Queue to drop any data it is currently holding and use the specified array as its data-buffer instead.
     * @param numItemsInArray The number of items pointed to by (array).
     * @param array Pointer to an array full of data.  This Queue will take ownership of the array, and will call
     *              delete[] on it at some point (either as part of a resize, or as part of the Queue destructor)
     *              unless you call ReleaseDataArray() beforehand.
     * @param validItemCount how many of the items in (array) should be considered currently-valid.  If larger than (numItemsInArray)
     *                      this value will be treated as if it was equal to (numItemsInArray).  (Useful if you have eg an
     *                      array of 800 items but want the final 300 of them to be used as "room to grow" only).
     *                      Defaults to MUSCLE_NO_LIMIT.
     * @note Don't call this method unless you know what you are doing!
     */
   void AdoptRawDataArray(uint32 numItemsInArray, ItemType * array, uint32 validItemCount = MUSCLE_NO_LIMIT);

   /** Relinquishes ownership of our internally-held data array, and returns it to the caller.
     * Note that the caller becomes the owner of the returned data-array and should call delete[] on it
     * when he is done using it, otherwise there will be a memory leak.  This method has the side effect
     * of clearing this Queue.
     * @param optRetArrayLen if non-NULL, the number of items in the returned array will be written into the
     *                       uint32 this pointer points to.  You can also get this value by calling GetNumAllocatedItemSlots()
     *                       before calling ReleaseRawDataArray() (but not afterwards!).  Defaults to NULL.
     * @returns a pointer to an array of items for the caller to own, or NULL on failure.  Call delete[] on the returned value to avoid a memory leak.
     * @note Don't call this method unless you know what you are doing!
     */
   ItemType * ReleaseRawDataArray(uint32 * optRetArrayLen = NULL);

   /** Steals the contents of (rhs) for our own use.
     * @param rhs the Queue to plunder.  On return, (rhs) will be empty,
     *            and this Queue will contain the contents that (rhs) previously contained.
     */
   void Plunder(Queue & rhs) MUSCLE_NOEXCEPT
   {
      if (rhs._queue == rhs._smallQueue)
      {
         const uint32 rhsSize = rhs.GetNumItems();  // guaranteed to be small enough for the _smallQueue
         (void) EnsureSize(rhsSize, true);          // set our size to match (rhs)'s -- guaranteed not to fail
         for (uint32 i=0; i<rhsSize; i++) muscleSwap((*this)[i], rhs[i]);  // and then steal his goodies
      }
      else SwapContents(rhs);

      rhs.Clear();
   }

private:
   /** Returns true iff we need to set our ItemType objects to their default-constructed state when we're done using them */
   MUSCLE_NODISCARD inline bool IsPerItemClearNecessary() const
   {
#ifndef MUSCLE_AVOID_CPLUSPLUS11
      return !std::is_trivial<ItemType>::value;
#else
      return true;
#endif
   }

   // Like strcmp() except for vectors instead of strings
   MUSCLE_NODISCARD int lexicographicalCompare(const Queue & rhs) const
   {
      const uint32 commonRange = muscleMin(GetNumItems(), rhs.GetNumItems());
      for (uint32 i=0; i<commonRange; i++)
      {
         const ItemType & leftItem  = (*this)[i];
         const ItemType & rightItem = rhs[i];
         if (leftItem < rightItem) return -1;
         if (rightItem < leftItem) return +1;
      }

      if (GetNumItems() < rhs.GetNumItems()) return -1;
      if (GetNumItems() > rhs.GetNumItems()) return +1;
      return 0;
   }

   status_t EnsureSizeAux(uint32 numSlots, ItemType ** optRetOldArray) {return EnsureSizeAux(numSlots, false, 0, optRetOldArray, false);}
   status_t EnsureSizeAux(uint32 numSlots, bool setNumItems, uint32 extraReallocItems, ItemType ** optRetOldArray, bool allowShrink);
   MUSCLE_NODISCARD const ItemType * GetArrayPointerAux(uint32 whichArray, uint32 & retLength) const;
   void SwapContentsAux(Queue<ItemType> & that);  // note:  can't be MUSCLE_NOEXCEPT because we may copy items

   MUSCLE_NODISCARD inline uint32 NextIndex(uint32 idx) const {return (idx >= _queueSize-1) ? 0 : idx+1;}
   MUSCLE_NODISCARD inline uint32 PrevIndex(uint32 idx) const {return (idx == 0) ? _queueSize-1 : idx-1;}

   /** Translates a user-index into an index into the _queue array.
     * @param idx user-index between 0 and (_itemCount-1), inclusive
     * @returns an array-offset between 0 and (_queueSize-1), inclusive
     */
   MUSCLE_NODISCARD inline uint32 InternalizeIndex(uint32 idx) const
   {
#ifdef __clang_analyzer__
      assert(idx < _queueSize);
#endif
      const uint32 o = _headIndex + idx;
      return (o < _queueSize) ? o : (o-_queueSize);  // was (o % _queueSize), but this way is about 3 times faster
   }

   // Helper methods, used for sorting (stolen from http://www-ihm.lri.fr/~thomas/VisuTri/inplacestablesort.html)
   template<class CompareFunctorType> void   Merge(const CompareFunctorType & compareFunctor, uint32 from, uint32 pivot, uint32 to, uint32 len1, uint32 len2, void * optCookie);
   template<class CompareFunctorType> MUSCLE_NODISCARD uint32 Lower(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const;
   template<class CompareFunctorType> MUSCLE_NODISCARD uint32 Upper(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const;

   ItemType * _queue; // points to _smallQueue, or to a dynamically alloc'd array
   uint32 _queueSize; // number of slots in the _queue array
   uint32 _itemCount; // number of valid items in the array
   uint32 _headIndex; // index of the first filled slot (meaningless if _itemCount is zero)
   uint32 _tailIndex; // index of the last filled slot (meaningless if _itemCount is zero)

   enum {ACTUAL_SMALL_QUEUE_SIZE = ((sizeof(ItemType)*SMALL_QUEUE_SIZE) < sizeof(void*)) ? (sizeof(void*)/sizeof(ItemType)) : SMALL_QUEUE_SIZE};
   ItemType _smallQueue[ACTUAL_SMALL_QUEUE_SIZE];  // small queues can be stored inline in this array
};

/** A trivial RAII class -- its constructor pushes a specified value onto the end of the specified Queue,
  * and its destructor pops that value off of the end of that same Queue.  Useful for reliably pushing state
  * information for method-calls to read when passing the data as via method-arguments is too onerous.
  * @tparam ItemType the type of object held by the Queue that we will push/pop items to/from.
  */
template <typename ItemType> class MUSCLE_NODISCARD QueueStackGuard : private NotCopyable
{
public:
   /** Default constructor
     * @param q the Queue to add an item to
     * @param item the item to add to the tail of the Queue
     */
   QueueStackGuard(Queue<ItemType> & q, const ItemType & item) : _queue(q) {(void) _queue.AddTail(item);}

   /** Destructor -- pops the last item out of the Queue that was specified in the constructor. */
   ~QueueStackGuard() {(void) _queue.RemoveTail();}

private:
   Queue<ItemType> & _queue;
};

template <class ItemType>
Queue<ItemType>::Queue()
   : _queue(NULL)
   , _queueSize(0)
   , _itemCount(0)
   , _headIndex(0)
   , _tailIndex(0)
{
   // coverity[uninit_member] - _smallQueue doesn't need to be initialized
}

template <class ItemType>
Queue<ItemType>::Queue(PreallocatedItemSlotsCount preallocatedItemSlotsCount)
   : _queue(NULL)
   , _queueSize(0)
   , _itemCount(0)
   , _headIndex(0)
   , _tailIndex(0)
{
   // coverity[uninit_member] - _smallQueue doesn't need to be initialized
   (void) EnsureSize(preallocatedItemSlotsCount.GetNumItemSlotsToPreallocate());
}

template <class ItemType>
Queue<ItemType>::Queue(const Queue& rhs)
   : _queue(NULL)
   , _queueSize(0)
   , _itemCount(0)
   , _headIndex(0)
   , _tailIndex(0)
{
   *this = rhs;
   // coverity[uninit_member] - no need to initialize _smallQueue, because we promise to always assign to its item(s) before try to we read from them
}

template <class ItemType>
bool
Queue<ItemType>::operator ==(const Queue& rhs) const
{
   if (this == &rhs) return true;
   if (GetNumItems() != rhs.GetNumItems()) return false;
   for (int32 i = GetNumItems()-1; i>=0; i--) if (((*this)[i] == rhs[i]) == false) return false;
   return true;
}

template <class ItemType>
Queue<ItemType> &
Queue<ItemType>::operator =(const Queue& rhs)
{
   if (this != &rhs)
   {
      const uint32 hisNumItems = rhs.GetNumItems();
           if (hisNumItems == 0) Clear(true);  // FogBugz #10274
      else if (EnsureSize(hisNumItems, true).IsOK()) for (uint32 i=0; i<hisNumItems; i++) (*this)[i] = rhs[i];
   }
   return *this;
}

template<class ItemType>
template<class ItemHashFunctorType>
uint32
Queue<ItemType>::HashCode(const ItemHashFunctorType & itemHashFunctor) const
{
   const uint32 numItems = GetNumItems();

   uint32 ret = numItems;
   for (uint32 i=0; i<numItems; i++)
   {
      const uint32 itemHash = itemHashFunctor((*this)[i]);
      ret += ((i+1)*(itemHash?itemHash:1));  // Multiplying by (i+1) so that item-ordering will affect the result
   }
   return ret;
}

template <class ItemType>
status_t
Queue<ItemType>::CopyFrom(const Queue & rhs)
{
   if (this == &rhs) return B_NO_ERROR;

   const uint32 numItems = rhs.GetNumItems();
   MRETURN_ON_ERROR(EnsureSize(numItems, true));
   for (uint32 i=0; i<numItems; i++) (*this)[i] = rhs[i];
   return B_NO_ERROR;
}

template <class ItemType>
ItemType &
Queue<ItemType>::operator[](uint32 i)
{
   MASSERT(IsIndexValid(i), "Invalid index to Queue::[]");
   ItemType * ret = &_queue[InternalizeIndex(i)];
#ifdef __clang_analyzer__
   assert(ret != NULL);  // not sure why this is necessary
#endif
   return *ret;
}

template <class ItemType>
const ItemType &
Queue<ItemType>::operator[](uint32 i) const
{
   MASSERT(IsIndexValid(i), "Invalid index to Queue::[]");
   return *GetItemAtUnchecked(i);
}

template <class ItemType>
Queue<ItemType>::~Queue()
{
   if (_queue != _smallQueue) delete [] _queue;
}

template <class ItemType>
QQ_UniversalSinkItemRef
ItemType *
Queue<ItemType>::
AddTailAndGet(QQ_SinkItemParam item)
{
   if ((GetNumUnusedItemSlots() < 1)&&(IsItemLocatedInThisContainer(item)))
   {
      const ItemType temp = QQ_ForwardItem(item); // avoid dangling pointer issue by copying the item to a temporary location
      return AddTailAndGet(temp);
   }

   ItemType * oldArray;
   if (EnsureSizeAux(_itemCount+1, false, _itemCount+1, &oldArray, false).IsError()) return NULL;

   if (IsEmpty()) _headIndex = _tailIndex = 0;
             else _tailIndex = NextIndex(_tailIndex);
   _itemCount++;
   ItemType * ret = &_queue[_tailIndex];
#ifdef __clang_analyzer__
   assert(ret != NULL);
#endif
   *ret = QQ_ForwardItem(item);
   delete [] oldArray;  // must do this AFTER the last reference to (item), in case (item) was part of (oldArray)

   // coverity[use_after_free : FALSE] - (ret) is still a valid pointer because (ret) never points into (oldArray)
   return ret;
}

template <class ItemType>
ItemType *
Queue<ItemType>::
AddTailAndGet()
{
   if (EnsureSize(_itemCount+1, false, _itemCount+1).IsError()) return NULL;
   if (IsEmpty()) _headIndex = _tailIndex = 0;
             else _tailIndex = NextIndex(_tailIndex);
   _itemCount++;
   return &_queue[_tailIndex];
}

template <class ItemType>
status_t
Queue<ItemType>::
AddTailMulti(const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   const uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);

   const uint32 mySize  = GetNumItems();
   const uint32 newSize = mySize+numNewItems;

   if ((&queue == this)&&(newSize > GetNumAllocatedItemSlots()))
   {
      // Avoid re-entrancy problems by making a partial copy of myself to add back into myself
      Queue<ItemType> temp;
      status_t ret;
      return temp.AddTailMulti(queue, startIndex, numNewItems).IsOK(ret) ? AddTailMulti(temp) : ret;
   }

   MRETURN_ON_ERROR(EnsureSize(newSize, true));

   for (uint32 i=mySize; i<newSize; i++) (*this)[i] = queue[startIndex+(i-mySize)];
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
AddTailMulti(const ItemType * items, uint32 numItems)
{
   const uint32 mySize  = GetNumItems();
   const uint32 newSize = mySize+numItems;
   uint32 rhs = 0;

   if ((newSize > GetNumAllocatedItemSlots())&&(IsItemLocatedInThisContainer(*items)))
   {
      // Avoid re-entrancy problems by making a temporary copy of the items before adding them back to myself
      Queue<ItemType> temp;
      status_t ret;
      return temp.AddTailMulti(items, numItems).IsOK(ret) ? AddTailMulti(temp) : ret;
   }

   ItemType * oldArray;
   MRETURN_ON_ERROR(EnsureSizeAux(newSize, true, 0, &oldArray, false));

   for (uint32 i=mySize; i<newSize; i++) (*this)[i] = items[rhs++];
   delete [] oldArray;  // must be done after all references to (items)
   return B_NO_ERROR;
}

template <class ItemType>
QQ_UniversalSinkItemRef
ItemType *
Queue<ItemType>::
AddHeadAndGet(QQ_SinkItemParam item)
{
   if ((GetNumUnusedItemSlots() < 1)&&(IsItemLocatedInThisContainer(item)))
   {
      const ItemType temp = QQ_ForwardItem(item); // avoid dangling pointer issue by copying the item to a temporary location
      return AddTailAndGet(temp);
   }

   ItemType * oldArray;
   if (EnsureSizeAux(_itemCount+1, false, _itemCount+1, &oldArray, false).IsError()) return NULL;
   if (IsEmpty()) _headIndex = _tailIndex = 0;
             else _headIndex = PrevIndex(_headIndex);
   _itemCount++;
   ItemType * ret = &_queue[_headIndex];
   *ret = QQ_ForwardItem(item);
   delete [] oldArray;  // must be done after all references to (item)!

   // coverity[use_after_free : FALSE] - (ret) is still a valid pointer because (ret) never points into (oldArray)
   return ret;
}

template <class ItemType>
ItemType *
Queue<ItemType>::
AddHeadAndGet()
{
   if (EnsureSize(_itemCount+1, false, _itemCount+1).IsError()) return NULL;
   if (IsEmpty()) _headIndex = _tailIndex = 0;
             else _headIndex = PrevIndex(_headIndex);
   _itemCount++;
   return &_queue[_headIndex];
}

template <class ItemType>
status_t
Queue<ItemType>::
AddHeadMulti(const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   const uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);

   if ((&queue == this)&&(numNewItems > GetNumUnusedItemSlots()))
   {
      // Avoid re-entrancy problems by making a partial copy of myself to prepend back into myself
      Queue<ItemType> temp;
      status_t ret;
      return temp.AddTailMulti(queue, startIndex, numNewItems).IsOK(ret) ? AddHeadMulti(temp) : ret;  // yes, AddTailMulti() and then AddHeadMulti() is intentional
   }

   MRETURN_ON_ERROR(EnsureSize(numNewItems+GetNumItems()));

   for (int32 i=((int)startIndex+numNewItems)-1; i>=(int32)startIndex; i--) (void) AddHead(queue[i]);  // guaranteed not to fail
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
AddHeadMulti(const ItemType * items, uint32 numItems)
{
   if ((numItems > GetNumUnusedItemSlots())&&(IsItemLocatedInThisContainer(*items)))
   {
      // Avoid re-entrancy problems by making a temporary copy of the items before adding them back to myself
      Queue<ItemType> temp;
      status_t ret;
      return temp.AddTailMulti(items, numItems).IsOK(ret) ? AddHeadMulti(temp) : ret;  // Yes, AddTailMulti() and then AddHeadMulti() is intentional
   }

   ItemType * oldArray;
   MRETURN_ON_ERROR(EnsureSizeAux(_itemCount+numItems, &oldArray));
   for (int32 i=((int32)numItems)-1; i>=0; i--) (void) AddHead(items[i]);  // guaranteed not to fail
   delete [] oldArray;  // must be done last!
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveHead(ItemType & returnItem)
{
   if (IsEmpty()) return B_DATA_NOT_FOUND;
   returnItem = QQ_PlunderItem(_queue[_headIndex]);
   return RemoveHead();
}

template <class ItemType>
uint32
Queue<ItemType>::RemoveHeadMulti(uint32 numItems)
{
   numItems = muscleMin(numItems, _itemCount);
   if (numItems == _itemCount) Clear();
                          else for (uint32 i=0; i<numItems; i++) (void) RemoveHead();
   return numItems;
}

template <class ItemType>
uint32
Queue<ItemType>::RemoveTailMulti(uint32 numItems)
{
   numItems = muscleMin(numItems, _itemCount);
   if (numItems == _itemCount) Clear();
                          else for (uint32 i=0; i<numItems; i++) (void) RemoveTail();
   return numItems;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveHead()
{
   if (IsEmpty()) return B_DATA_NOT_FOUND;
   const uint32 oldHeadIndex = _headIndex;
   _headIndex = NextIndex(_headIndex);
   _itemCount--;
   if (IsPerItemClearNecessary()) _queue[oldHeadIndex] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
   return B_NO_ERROR;
}

template <class ItemType>
ItemType
Queue<ItemType>::
RemoveHeadWithDefault()
{
   if (IsEmpty()) return GetDefaultItem();
   else
   {
      const ItemType ret = QQ_PlunderItem(Head());
      (void) RemoveHead();
      return std_move_if_available(ret);
   }
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveTail(ItemType & returnItem)
{
   if (IsEmpty()) return B_DATA_NOT_FOUND;
   returnItem = QQ_PlunderItem(_queue[_tailIndex]);
   return RemoveTail();
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveTail()
{
   if (IsEmpty()) return B_DATA_NOT_FOUND;
   const uint32 removedItemIndex = _tailIndex;
   _tailIndex = PrevIndex(_tailIndex);
   _itemCount--;
   if (IsPerItemClearNecessary()) _queue[removedItemIndex] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
   return B_NO_ERROR;
}

template <class ItemType>
ItemType
Queue<ItemType>::
RemoveTailWithDefault()
{
   if (IsEmpty()) return GetDefaultItem();
   else
   {
      const ItemType ret = QQ_PlunderItem(Tail());
      (void) RemoveTail();
      return std_move_if_available(ret);
   }
}

template <class ItemType>
status_t
Queue<ItemType>::
GetItemAt(uint32 index, ItemType & returnItem) const
{
   const ItemType * p = GetItemAt(index);
   if (p)
   {
      returnItem = *p;
      return B_NO_ERROR;
   }
   else return B_BAD_ARGUMENT;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveItemAt(uint32 index, ItemType & returnItem)
{
   if (index >= _itemCount) return B_BAD_ARGUMENT;
   returnItem = QQ_PlunderItem(_queue[InternalizeIndex(index)]);
   return RemoveItemAt(index);
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveItemAt(uint32 index)
{
   if (index >= _itemCount) return B_BAD_ARGUMENT;

   uint32 internalizedIndex = InternalizeIndex(index);
   uint32 indexToClear;

   if (index < _itemCount/2)
   {
      // item is closer to the head:  shift everything forward one, ending at the head
      while(internalizedIndex != _headIndex)
      {
         const uint32 prev = PrevIndex(internalizedIndex);
         _queue[internalizedIndex] = QQ_PlunderItem(_queue[prev]);
         internalizedIndex = prev;
      }
      indexToClear = _headIndex;
      _headIndex = NextIndex(_headIndex);
   }
   else
   {
      // item is closer to the tail:  shift everything back one, ending at the tail
      while(internalizedIndex != _tailIndex)
      {
         const uint32 next = NextIndex(internalizedIndex);
         _queue[internalizedIndex] = QQ_PlunderItem(_queue[next]);
         internalizedIndex = next;
      }
      indexToClear = _tailIndex;
      _tailIndex = PrevIndex(_tailIndex);
   }

   _itemCount--;
   if (IsPerItemClearNecessary()) _queue[indexToClear] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
   return B_NO_ERROR;
}

template <class ItemType>
ItemType
Queue<ItemType>::
RemoveItemAtWithDefault(uint32 index)
{
   if (index >= GetNumItems()) return GetDefaultItem();
   else
   {
      const ItemType ret = QQ_PlunderItem((*this)[index]);
      (void) RemoveItemAt(index);
      return ret;
   }
}

template <class ItemType>
QQ_UniversalSinkItemRef
status_t
Queue<ItemType>::
ReplaceItemAt(uint32 index, QQ_SinkItemParam newItem)
{
   if (index >= _itemCount) return B_BAD_ARGUMENT;
   _queue[InternalizeIndex(index)] = QQ_ForwardItem(newItem);
   return B_NO_ERROR;
}

template <class ItemType>
QQ_UniversalSinkItemRef
status_t
Queue<ItemType>::
InsertItemAt(uint32 index, QQ_SinkItemParam item)
{
   if ((GetNumUnusedItemSlots() < 1)&&(IsItemLocatedInThisContainer(item)))
   {
      const ItemType temp = QQ_ForwardItem(item); // avoid dangling pointer issue by copying the item to a temporary location
      return InsertItemAt(index, temp);
   }

   // Simple cases
   if (index >= _itemCount) return AddTail(QQ_ForwardItem(item));
   if (index == 0)          return AddHead(QQ_ForwardItem(item));

   // Harder case:  inserting into the middle of the array
   if (index < _itemCount/2)
   {
      // Add a space at the front, and shift things back
      MRETURN_ON_ERROR(AddHead());  // allocate an extra slot
      for (uint32 i=0; i<index; i++) (void) ReplaceItemAt(i, QQ_ForwardItem(*GetItemAtUnchecked(i+1)));
   }
   else
   {
      // Add a space at the rear, and shift things forward
      MRETURN_ON_ERROR(AddTail());  // allocate an extra slot
      for (int32 i=((int32)_itemCount)-1; i>((int32)index); i--) (void) ReplaceItemAt(i, QQ_ForwardItem(*GetItemAtUnchecked(i-1)));
   }
   return ReplaceItemAt(index, QQ_ForwardItem(item));
}

template <class ItemType>
status_t
Queue<ItemType>::
InsertItemsAt(uint32 index, const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   index = muscleMin(index, _itemCount);

   const uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);
   if (numNewItems == 0) return B_NO_ERROR;
   if (numNewItems == 1)
   {
      if (index == 0)          return AddHead(queue.Head());
      if (index == _itemCount) return AddTail(queue.Head());
   }

   const uint32 oldSize = GetNumItems();
   const uint32 newSize = oldSize+numNewItems;

   MRETURN_ON_ERROR(EnsureSize(newSize, true));

   for (uint32 i=index; i<oldSize; i++)           (*this)[i+numNewItems] = (*this)[i];
   for (uint32 i=index; i<index+numNewItems; i++) (*this)[i]             = queue[startIndex++];
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
InsertItemsAt(uint32 index, const ItemType * items, uint32 numNewItems)
{
   index = muscleMin(index, _itemCount);

   if (numNewItems == 0) return B_NO_ERROR;
   if (numNewItems == 1)
   {
      if (index == 0)          return AddHead(*items);
      if (index == _itemCount) return AddTail(*items);
   }

   const uint32 oldSize = GetNumItems();
   const uint32 newSize = oldSize+numNewItems;

   ItemType * oldItems = NULL;  // set NULL just to keep the static analyzer happy
   MRETURN_ON_ERROR(EnsureSizeAux(newSize, true, &oldItems, NULL, false));

   int32 si = 0;
   for (uint32 i=index; i<oldSize; i++)           (*this)[i+numNewItems] = (*this)[i];
   for (uint32 i=index; i<index+numNewItems; i++) (*this)[i]             = items[si++];
   delete [] oldItems;
   return B_NO_ERROR;
}

template <class ItemType>
void
Queue<ItemType>::
Clear(bool releaseCachedBuffers)
{
   if ((releaseCachedBuffers)&&(_queue != _smallQueue))
   {
      delete [] _queue;
      _queue = NULL;
      _queueSize = 0;
   }
   else if ((HasItems())&&(IsPerItemClearNecessary()))
   {
      const ItemType & defaultItem = GetDefaultItem();
      for (uint32 i=0; i<2; i++)
      {
         uint32 arrayLen = 0;  // set to zero just to shut the compiler up
         ItemType * p = GetArrayPointer(i, arrayLen);
         if (p) {for (uint32 j=0; j<arrayLen; j++) p[j] = defaultItem;}
      }
   }

   FastClear();  // BAB-38:  Gotta call this in ALL cases, to make sure the Queue is normalized when we return
}

template <class ItemType>
status_t
Queue<ItemType>::
EnsureSizeAux(uint32 size, bool setNumItems, uint32 extraPreallocs, ItemType ** retOldArray, bool allowShrink)
{
   if (retOldArray) *retOldArray = NULL;  // default value, will be set non-NULL iff the old array needs deleting later

   if ((_queue == NULL)||(allowShrink ? (_queueSize != (size+extraPreallocs)) : (_queueSize < size)))
   {
      const uint32 sqLen = ARRAYITEMS(_smallQueue);
      const uint32 temp  = size + extraPreallocs;
      uint32 newQLen = muscleMax((uint32)ARRAYITEMS(_smallQueue), ((setNumItems)||(temp <= sqLen)) ? muscleMax(sqLen,temp) : temp);
      if (newQLen == MUSCLE_NO_LIMIT) return B_RESOURCE_LIMIT;  // avoid a stupidly-large allocation if someone makes this mistake

      ItemType * newQueue = ((_queue == _smallQueue)||(newQLen > sqLen)) ? newnothrow_array(ItemType,newQLen) : _smallQueue;

      MRETURN_OOM_ON_NULL(newQueue);
      if (newQueue == _smallQueue) newQLen = sqLen;

      if (_queue)  // just to make Coverity happy
      {
         for (uint32 i=0; i<_itemCount; i++)
            newQueue[i] = QQ_PlunderItem(*GetItemAtUnchecked(i));  // we know that (_itemCount < size)
      }

      if (setNumItems) _itemCount = size;
      _headIndex = 0;
      _tailIndex = _itemCount-1;

      if (_queue == _smallQueue)
      {
         if (IsPerItemClearNecessary())
         {
            const ItemType & defaultItem = GetDefaultItem();
            for (uint32 i=0; i<sqLen; i++) _smallQueue[i] = defaultItem;
         }
      }
      else if (retOldArray) *retOldArray = _queue;
      else delete [] _queue;

      _queue     = newQueue;
      _queueSize = newQLen;
   }

   if (setNumItems)
   {
      // Force ourselves to contain exactly the required number of items
      if (size > _itemCount)
      {
         // We can do this quickly because the "new" items are already initialized properly
         _tailIndex = PrevIndex(InternalizeIndex(size));
         _itemCount = size;
      }
      else (void) RemoveTailMulti(_itemCount-size);
   }

   return B_NO_ERROR;
}

template <class ItemType>
int32
Queue<ItemType>::
IndexOf(const ItemType & item, uint32 startAt, uint32 endAtPlusOne) const
{
   if (startAt >= GetNumItems()) return -1;

   endAtPlusOne = muscleMin(endAtPlusOne, GetNumItems());
   for (uint32 i=startAt; i<endAtPlusOne; i++) if (*GetItemAtUnchecked(i) == item) return i;
   return -1;
}

template <class ItemType>
int32
Queue<ItemType>::
LastIndexOf(const ItemType & item, uint32 startAt, uint32 endAt) const
{
   if (endAt >= GetNumItems()) return -1;

   startAt = muscleMin(startAt, GetNumItems()-1);
   for (int32 i=(int32)startAt; i>=((int32)endAt); i--) if (*GetItemAtUnchecked(i) == item) return i;
   return -1;
}

template <class ItemType>
void
Queue<ItemType>::
Swap(uint32 fromIndex, uint32 toIndex)
{
   muscleSwap((*this)[fromIndex], (*this)[toIndex]);
}

template <class ItemType>
QQ_UniversalSinkItemRef
int32
Queue<ItemType>::
InsertItemAtSortedPosition(const CompareFunctor<ItemType> & compareFunctor, QQ_SinkItemParam item, void * optCookie)
{
   int32 insertAfter = GetNumItems();
   if ((insertAfter > 0)&&(compareFunctor.Compare(item, Head(), optCookie) >= 0))
      while(--insertAfter >= 0)
         if (compareFunctor.Compare(item, (*this)[insertAfter], optCookie) >= 0)
            return (InsertItemAt(insertAfter+1, QQ_ForwardItem(item)).IsOK()) ? (insertAfter+1) : -1;
   return (AddHead(QQ_ForwardItem(item)).IsOK()) ? 0 : -1;
}

template <class ItemType>
template <class CompareFunctorType>
void
Queue<ItemType>::
Sort(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, void * optCookie)
{
   const uint32 size = GetNumItems();
   if (to > size) to = size;
   if (to > from)
   {
      if (to < from+12)
      {
         // too easy, just do a bubble sort (base case)
         if (to > from+1)
         {
            for (uint32 i=from+1; i<to; i++)
            {
               for (uint32 j=i; j>from; j--)
               {
                  const int ret = compareFunctor.Compare(*(GetItemAtUnchecked(j)), *(GetItemAtUnchecked(j-1)), optCookie);
                  if (ret < 0) Swap(j, j-1);
                          else break;
               }
            }
         }
      }
      else
      {
         // Okay, do the real thing
         const uint32 middle = (from + to)/2;
         Sort(compareFunctor, from, middle, optCookie);
         Sort(compareFunctor, middle, to, optCookie);
         Merge(compareFunctor, from, middle, to, middle-from, to-middle, optCookie);
      }
   }
}

template <class ItemType>
void
Queue<ItemType>::
ReverseItemOrdering(uint32 from, uint32 to)
{
   const uint32 size = GetNumItems();
   if (size > 0)
   {
      to--;  // make it inclusive
      if (to >= size) to = size-1;
      while (from < to) Swap(from++, to--);
   }
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveFirstInstanceOf(const ItemType & val)
{
   const uint32 ni = GetNumItems();
   for (uint32 i=0; i<ni; i++) if ((*this)[i] == val) return RemoveItemAt(i);
   return B_DATA_NOT_FOUND;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveLastInstanceOf(const ItemType & val)
{
   for (int32 i=((int32)GetNumItems())-1; i>=0; i--) if ((*this)[i] == val) return RemoveItemAt(i);
   return B_DATA_NOT_FOUND;
}

template <class ItemType>
uint32
Queue<ItemType>::
RemoveAllInstancesOf(const ItemType & val)
{
   if (IsItemLocatedInThisContainer(val))
   {
      // avoid having the item erased while we are still using it
      ItemType temp = val;
      return RemoveAllInstancesOf(temp);
   }

   // Efficiently collapse all non-matching slots up to the top of the list
   const uint32 origSize = GetNumItems();
   uint32 ret     = 0;
   uint32 writeTo = 0;
   for(uint32 readFrom=0; readFrom<origSize; readFrom++)
   {
      ItemType & nextRead = (*this)[readFrom];
      if (nextRead == val) ret++;
      else
      {
         if (readFrom > writeTo) (*this)[writeTo] = QQ_PlunderItem(nextRead);
         writeTo++;
      }
   }

   // Now get rid of any now-surplus slots
   for (; writeTo<origSize; writeTo++) (void) RemoveTail();

   return ret;
}

template <class ItemType>
uint32
Queue<ItemType>::
RemoveDuplicateItems()
{
   Sort();
   return RemoveSortedDuplicateItems();
}

template <class ItemType>
uint32
Queue<ItemType>::
RemoveSortedDuplicateItems()
{
   uint32 numWrittenItems  = 1;  // we'll always keep the first item
   const uint32 totalItems = GetNumItems();
   for (uint32 i=0; i<totalItems; i++)
   {
      ItemType & nextItem    = (*this)[i];
      const ItemType & wItem = (*this)[numWrittenItems-1];
      if (!(nextItem == wItem)) (*this)[numWrittenItems++] = QQ_PlunderItem(nextItem);
   }

   const uint32 ret = GetNumItems()-numWrittenItems;
   (void) EnsureSize(numWrittenItems, true);  // guaranteed to succeed
   return ret;
}

template <class ItemType>
template <class CompareFunctorType>
void
Queue<ItemType>::
Merge(const CompareFunctorType & compareFunctor, uint32 from, uint32 pivot, uint32 to, uint32 len1, uint32 len2, void * optCookie)
{
   if ((len1)&&(len2))
   {
      if (len1+len2 == 2)
      {
         if (compareFunctor.Compare(*(GetItemAtUnchecked(pivot)), *(GetItemAtUnchecked(from)), optCookie) < 0) Swap(pivot, from);
      }
      else
      {
         uint32 first_cut, second_cut;
         uint32 len11, len22;
         if (len1 > len2)
         {
            len11      = len1/2;
            first_cut  = from + len11;
            second_cut = Lower(compareFunctor, pivot, to, *GetItemAtUnchecked(first_cut), optCookie);
            len22      = second_cut - pivot;
         }
         else
         {
            len22      = len2/2;
            second_cut = pivot + len22;
            first_cut  = Upper(compareFunctor, from, pivot, *GetItemAtUnchecked(second_cut), optCookie);
            len11      = first_cut - from;
         }

         // do a rotation
         if ((pivot!=first_cut)&&(pivot!=second_cut))
         {
            // find the greatest common denominator of (pivot-first_cut) and (second_cut-first_cut)
            uint32 n = pivot-first_cut;
            {
               uint32 m = second_cut-first_cut;
               while(n!=0)
               {
                  uint32 t = m % n;
                  m=n;
                  n=t;
               }
               n = m;
            }

            while(n--)
            {
               ItemType val = QQ_PlunderItem(*GetItemAtUnchecked(first_cut+n));
               const uint32 shift = pivot - first_cut;
               uint32 p1 = first_cut+n;
               uint32 p2 = p1+shift;
               while (p2 != first_cut + n)
               {
                  (void) ReplaceItemAt(p1, QQ_PlunderItem(*GetItemAtUnchecked(p2)));
                  p1 = p2;
                  if (second_cut - p2 > shift) p2 += shift;
                                          else p2  = first_cut + (shift - (second_cut - p2));
               }
               (void) ReplaceItemAt(p1, QQ_PlunderItem(val));
            }
         }

         const uint32 new_mid = first_cut+len22;
         Merge(compareFunctor, from,    first_cut,  new_mid, len11,        len22,        optCookie);
         Merge(compareFunctor, new_mid, second_cut, to,      len1 - len11, len2 - len22, optCookie);
      }
   }
}


template <class ItemType>
template <class CompareFunctorType>
uint32
Queue<ItemType>::
Lower(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const
{
   if (to > from)
   {
      uint32 len = to - from;
      while(len > 0)
      {
         const uint32 half = len/2;
         const uint32 mid  = from + half;
         if (compareFunctor.Compare(*(GetItemAtUnchecked(mid)), val, optCookie) < 0)
         {
            from = mid+1;
            len  = len - half - 1;
         }
         else len = half;
      }
   }
   return from;
}

template <class ItemType>
template <class CompareFunctorType>
uint32
Queue<ItemType>::
Upper(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const
{
   if (to > from)
   {
      uint32 len = to - from;
      while(len > 0)
      {
         const uint32 half = len/2;
         const uint32 mid  = from + half;
         if (compareFunctor.Compare(val, *(GetItemAtUnchecked(mid)), optCookie) < 0) len = half;
         else
         {
            from = mid+1;
            len  = len - half -1;
         }
      }
   }
   return from;
}

template <class ItemType>
const ItemType *
Queue<ItemType> :: GetArrayPointerAux(uint32 whichArray, uint32 & retLength) const
{
   if (HasItems())
   {
      switch(whichArray)
      {
         case 0:
            retLength = (_headIndex <= _tailIndex) ? (_tailIndex-_headIndex)+1 : (_queueSize-_headIndex);
            return &_queue[_headIndex];
         break;

         case 1:
            if (_headIndex > _tailIndex)
            {
               retLength = _tailIndex+1;
               return &_queue[0];
            }
         break;
      }
   }

   retLength = 0;
   return NULL;
}

template <class ItemType>
void
Queue<ItemType>::SwapContents(Queue<ItemType> & that)  // note:  can't be MUSCLE_NOEXCEPT because we may copy items
{
   if (&that == this) return;  // no point trying to swap with myself

   const bool thisSmall = (this->_queue == this->_smallQueue);
   const bool thatSmall = (that._queue  == that._smallQueue);

   if ((thisSmall)&&(thatSmall))
   {
      // First, move any extra items from the longer queue to the shorter one...
      const uint32 commonSize    = muscleMin(GetNumItems(), that.GetNumItems());
      const int32 sizeDiff       = ((int32)GetNumItems())-((int32)that.GetNumItems());
      Queue<ItemType> & copyTo   = (sizeDiff > 0) ? that : *this;
      Queue<ItemType> & copyFrom = (sizeDiff > 0) ? *this : that;
      (void) copyTo.AddTailMulti(copyFrom, commonSize); // guaranteed not to fail
      (void) copyFrom.EnsureSize(commonSize, true);     // remove the copied items from (copyFrom)

      // Then just swap the elements that are present in both arrays
      for (uint32 i=0; i<commonSize; i++) muscleSwap((*this)[i], that[i]);
   }
   else if (thisSmall) SwapContentsAux(that);
   else if (thatSmall) that.SwapContentsAux(*this);
   else
   {
      // this case is easy, we can just swizzle the pointers around
      muscleSwap(_queue,     that._queue);
      muscleSwap(_queueSize, that._queueSize);
      muscleSwap(_headIndex, that._headIndex);
      muscleSwap(_tailIndex, that._tailIndex);
      muscleSwap(_itemCount, that._itemCount);
   }
}

template <class ItemType>
void
Queue<ItemType>::SwapContentsAux(Queue<ItemType> & largeThat)  // note:  can't be MUSCLE_NOEXCEPT because we may copy items
{
   // First, copy over our (small) contents to his small-buffer
   const uint32 ni = GetNumItems();
   for (uint32 i=0; i<ni; i++) largeThat._smallQueue[i] = QQ_PlunderItem((*this)[i]);

   // Now adopt his dynamic buffer
   _queue     = largeThat._queue;
   _queueSize = largeThat._queueSize;
   if (_queueSize > 0)
   {
      _headIndex = largeThat._headIndex;
      _tailIndex = largeThat._tailIndex;
   }
   else _headIndex = _tailIndex = 0;  // avoid static-analyzer warning in this case

   // And point him back at his small-buffer
   if (ni > 0)
   {
      largeThat._queue     = largeThat._smallQueue;
      largeThat._queueSize = ARRAYITEMS(largeThat._smallQueue);
      largeThat._headIndex = 0;
      largeThat._tailIndex = ni-1;
   }
   else
   {
      largeThat._queue     = NULL;
      largeThat._queueSize = 0;
      // headIndex and tailIndex are undefined in this case anyway
   }

   muscleSwap(_itemCount, largeThat._itemCount);
}

template <class ItemType>
bool
Queue<ItemType>::StartsWith(const Queue<ItemType> & prefixQueue) const
{
   if (prefixQueue.GetNumItems() > GetNumItems()) return false;
   const uint32 prefixQueueLen = prefixQueue.GetNumItems();
   for (uint32 i=0; i<prefixQueueLen; i++) if (!(prefixQueue[i] == (*this)[i])) return false;
   return true;
}

template <class ItemType>
bool
Queue<ItemType>::EndsWith(const Queue<ItemType> & suffixQueue) const
{
   if (suffixQueue.GetNumItems() > GetNumItems()) return false;
   int32 lastToCheck = GetNumItems()-suffixQueue.GetNumItems();
   for (int32 i=GetNumItems()-1; i>=lastToCheck; i--) if (!(suffixQueue[i] == (*this)[i])) return false;
   return true;
}

template <class ItemType>
void
Queue<ItemType>::Normalize()
{
   if (IsNormalized() == false)
   {
      if (_itemCount*2 <= _queueSize)
      {
         // There's enough space in the middle of the
         // array to just copy the items over, yay!  This
         // is more efficient when there are just a few
         // valid items in a large array.
         const bool isPerItemClearNecessary = IsPerItemClearNecessary();
         const ItemType & defaultItem       = GetDefaultItem();
         const uint32 startAt               = _tailIndex+1;
         for (uint32 i=0; i<_itemCount; i++)
         {
            ItemType & from = (*this)[i];
            _queue[startAt+i] = from;
            if (isPerItemClearNecessary) from = defaultItem;  // clear the old slot to avoid leaving extra Refs, etc
         }
         _headIndex = startAt;
         _tailIndex = startAt+_itemCount-1;
      }
      else
      {
         // There's not enough room to do a simple copy,
         // so we'll rotate the entire array using this
         // algorithm that was written by Paul Hsieh, taken
         // from http://www.azillionmonkeys.com/qed/case8.html
         uint32 c = 0;
         for (uint32 v = 0; c<_queueSize; v++)
         {
            uint32 t  = v;
            uint32 tp = v + _headIndex;
            const ItemType tmp = _queue[v];
            c++;
            while(tp != v)
            {
               _queue[t] = _queue[tp];
               t = tp;
               tp += _headIndex;
               if (tp >= _queueSize) tp -= _queueSize;
               c++;
            }
            _queue[t] = std_move_if_available(tmp);
         }
         _headIndex = 0;
         _tailIndex = _itemCount-1;
      }
   }
}

template <class ItemType>
ItemType *
Queue<ItemType>::ReleaseRawDataArray(uint32 * optRetArrayLen)
{
   ItemType * ret = NULL;

   if (optRetArrayLen) *optRetArrayLen = _queueSize;

   if (_queue == _smallQueue)
   {
      // Oops, we don't have a dynamically-created array to release!  So we'll create one
      // to return, just so the user doesn't have to worry about handling a special-case.
      ret = newnothrow_array(ItemType, _queueSize);
      if (ret)
      {
         for (uint32 i=0; i<_itemCount; i++) ret[i] = (*this)[i];
         Clear();
      }
      else MWARN_OUT_OF_MEMORY;
   }
   else
   {
      // In the non-small-Queue case we can just pass out the array-pointer
      ret        = _queue;
      _queue     = NULL;
      _queueSize = 0;
      _itemCount = 0;
   }
   return ret;
}

template <class ItemType>
void
Queue<ItemType>::AdoptRawDataArray(uint32 numItemsInArray, ItemType * array, uint32 validItemCount)
{
   Clear(true);
   if (array)
   {
      _queue     = array;
      _queueSize = numItemsInArray;
      _itemCount = muscleMin(numItemsInArray, validItemCount);
      _headIndex = 0;
      _tailIndex = 0;
   }
}

} // end namespace muscle

#endif
