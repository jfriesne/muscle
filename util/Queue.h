/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQueue_h
#define MuscleQueue_h

#include "support/MuscleSupport.h"

#ifdef MUSCLE_USE_CPLUSPLUS11
# include<initializer_list>
#endif

namespace muscle {

#ifndef SMALL_QUEUE_SIZE
# define SMALL_QUEUE_SIZE 3
#endif

#ifdef MUSCLE_USE_CPLUSPLUS11
// Enable move semantics (when possible) for C++11
# define QQ_UniversalSinkItemRef template<typename ItemParam>
# define QQ_SinkItemParam  ItemParam &&
# define QQ_PlunderItem(item) std::move(item)
# define QQ_ForwardItem(item) std::forward<ItemParam>(item)
#else
// For earlier versions of C++, use the traditional copy/ref semantics
# define QQ_UniversalSinkItemRef
# define QQ_SinkItemParam const ItemType &
# define QQ_PlunderItem(item) (item)
# define QQ_ForwardItem(item) (item)
#endif

/** This class implements a templated double-ended-queue data structure.
 *  Adding or removing items from the head or tail of a Queue is (on average)
 *  an O(1) operation.  A Queue can also serve as a reasonably efficient resizable-array
 *  class (aka Vector) if that is all you need.
 */
template <class ItemType> class Queue MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor.  */
   Queue();

   /** Copy constructor. */
   Queue(const Queue& copyMe);

   /** Destructor. */
   virtual ~Queue();

   /** Assigment operator. */
   Queue & operator=(const Queue & from);

#ifdef MUSCLE_USE_CPLUSPLUS11
   /** Initialize list Constructor */
   Queue(const std::initializer_list<ItemType> & list) : _queue(NULL), _queueSize(0), _itemCount(0) {(void) AddTailMulti(list);}

   /** C++11 Move Constructor */
   Queue(Queue && rhs) : _queue(NULL), _queueSize(0), _itemCount(0) {if (rhs._queue == rhs._smallQueue) *this = rhs; else SwapContents(rhs);}

   /** C++11 Move Assignment Operator */
   Queue & operator =(Queue && rhs) {if (rhs._queue == rhs._smallQueue) *this = rhs; else SwapContents(rhs); return *this;}
#endif

   /** Equality operator.  Queues are equal if they are the same length, and
     * every nth item in this queue is == to the corresponding item in (rhs). */
   bool operator==(const Queue & rhs) const;

   /** Returns the negation of the equality operator */
   bool operator!=(const Queue & rhs) const {return !(*this == rhs);}

   /** Similar to the assignment operator, except this method returns a status code.
     * @param rhs This Queue's contents will become a copy of (rhs)'s items.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?).
     */
   status_t CopyFrom(const Queue<ItemType> & rhs);

   /** Appends (item) to the end of the queue.  Queue size grows by one.
    *  @param item The item to append.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   QQ_UniversalSinkItemRef status_t AddTail(QQ_SinkItemParam item) {return (AddTailAndGet(QQ_ForwardItem(item)) != NULL) ? B_NO_ERROR : B_ERROR;}

   /** As above, except that a default item is appended.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t AddTail() {return (AddTailAndGet() != NULL) ? B_NO_ERROR : B_ERROR;}

   /** Appends some or all items in (queue) to the end of our queue.  Queue size
    *  grows by at most (queue.GetNumItems()).
    *  For example:
    *    Queue<int> a;  // contains 1, 2, 3, 4
    *    Queue<int> b;  // contains 5, 6, 7, 8
    *    a.AddTail(b);  // a now contains 1, 2, 3, 4, 5, 6, 7, 8
    *  @param queue The queue to append to our queue.
    *  @param startIndex Index in (queue) to start adding at.  Default to zero.
    *  @param numItems Number of items to add.  If this number is too large, it will be capped appropriately.  Default is to add all items.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t AddTailMulti(const Queue<ItemType> & queue, uint32 startIndex = 0, uint32 numItems = MUSCLE_NO_LIMIT);

   /** Adds the given array of items to the tail of the Queue.  Equivalent
    *  to adding them to the tail of the Queue one at a time, but somewhat
    *  more efficient.  On success, the queue size grows by (numItems).
    *  @param items Pointer to an array of items to add to the Queue.
    *  @param numItems Number of items in the array
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out of memory)
    */
   status_t AddTailMulti(const ItemType * items, uint32 numItems);

#ifdef MUSCLE_USE_CPLUSPLUS11
   /** Available in C++11 only:  Appends the items specified in the initializer
    *  list to this Queue.
    *  @param list The C++11 initializer list of items (e.g. {1,2,3,4,5} to add.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
    */
   status_t AddTailMulti(const std::initializer_list<ItemType> & list)
   {
      if (EnsureCanAdd(list.size()) != B_NO_ERROR) return B_ERROR;
      for (auto i : list) (void) AddTail(i);
      return B_NO_ERROR;
   }
#endif

   /** Appends (item) to the end of the queue.  Queue size grows by one.
    *  @param item The item to append.
    *  @return A pointer to the appended item on success, or a NULL pointer on failure.
    */
   QQ_UniversalSinkItemRef ItemType * AddTailAndGet(QQ_SinkItemParam item);

   /** As above, except that a default item is appended.
    *  @return A pointer to the appended item on success, or a NULL pointer on failure.
    */
   ItemType * AddTailAndGet();

   /** Prepends (item) to the head of the queue.  Queue size grows by one.
    *  @param item The item to prepend.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   QQ_UniversalSinkItemRef status_t AddHead(QQ_SinkItemParam item) {return (AddHeadAndGet(QQ_ForwardItem(item)) != NULL) ? B_NO_ERROR : B_ERROR;}

   /** As above, except a default item is prepended.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t AddHead() {return (AddHeadAndGet() != NULL) ? B_NO_ERROR : B_ERROR;}

   /** Concatenates (queue) to the head of our queue.
    *  Our queue size grows by at most (queue.GetNumItems()).
    *  For example:
    *    Queue<int> a;  // contains 1, 2, 3, 4
    *    Queue<int> b;  // contains 5, 6, 7, 8
    *    a.AddHead(b);  // a now contains 5, 6, 7, 8, 1, 2, 3, 4
    *  @param queue The queue to prepend to our queue.
    *  @param startIndex Index in (queue) to start adding at.  Default to zero.
    *  @param numItems Number of items to add.  If this number is too large, it will be capped appropriately.  Default is to add all items.
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t AddHeadMulti(const Queue<ItemType> & queue, uint32 startIndex = 0, uint32 numItems = MUSCLE_NO_LIMIT);

   /** Concatenates the given array of items to the head of the Queue.
    *  The semantics are the same as for AddHeadMulti(const Queue<ItemType> &).
    *  @param items Pointer to an array of items to add to the Queue.
    *  @param numItems Number of items in the array
    *  @return B_NO_ERROR on success, or B_ERROR on failure (out of memory)
    */
   status_t AddHeadMulti(const ItemType * items, uint32 numItems);

   /** Prepends (item) to the beginning of the queue.  Queue size grows by one.
    *  @param item The item to prepend.
    *  @return A pointer to the prepend item on success, or a NULL pointer on failure.
    */
   QQ_UniversalSinkItemRef ItemType * AddHeadAndGet(QQ_SinkItemParam item);

   /** As above, except that a default item is prepended.
    *  @return A pointer to the prepend item on success, or a NULL pointer on failure.
    */
   ItemType * AddHeadAndGet();

   /** Removes the item at the head of the queue.
    *  @return B_NO_ERROR on success, B_ERROR if the queue was empty.
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
    *  @return B_NO_ERROR on success, B_ERROR if the queue was empty
    */
   status_t RemoveHead(ItemType & returnItem);

   /** Removes the item at the tail of the queue.
    *  @return B_NO_ERROR on success, B_ERROR if the queue was empty.
    */
   status_t RemoveTail();

   /** Removes the item at the tail of the queue and places it into (returnItem).
    *  @param returnItem On success, the removed item is copied into this object.
    *  @return B_NO_ERROR on success, B_ERROR if the queue was empty
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
    *  @return B_NO_ERROR on success, B_ERROR on failure (i.e. bad index)
    *  Note that this method is somewhat inefficient for indices that
    *  aren't at the head or tail of the queue (i.e. O(n) time)
    */
   status_t RemoveItemAt(uint32 index);

   /** Removes the item at the (index)'th position in the queue, and copies it into (returnItem).
    *  @param index Which item to remove--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @param returnItem On success, the removed item is copied into this object.
    *  @return B_NO_ERROR on success, B_ERROR on failure (i.e. bad index)
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
    *  @return B_NO_ERROR on success, B_ERROR on failure (e.g. bad index)
    */
   status_t GetItemAt(uint32 index, ItemType & returnItem) const;

   /** Returns a pointer to an item in the array (i.e. no copying of the item is done).
    *  Included for efficiency; be careful with this: modifying the queue can invalidate
    *  the returned pointer!
    *  @param index Index of the item to return a pointer to.
    *  @return a pointer to the internally held item, or NULL if (index) was invalid.
    */
   ItemType * GetItemAt(uint32 index) const {return (index<_itemCount)?GetItemAtUnchecked(index):NULL;}

   /** The same as GetItemAt(), except this version doesn't check to make sure
    *  (index) is valid.
    *  @param index Index of the item to return a pointer to.  Must be a valid index!
    *  @return a pointer to the internally held item.  The returned value is undefined
    *          if the index isn't valid, so be careful!
    */
   ItemType * GetItemAtUnchecked(uint32 index) const {return &_queue[InternalizeIndex(index)];}

   /** Returns a reference to the (index)'th item in the Queue, if such an item exists,
     * or a reference to a default item if it doesn't.  Unlike the [] operator,
     * it is okay to call this method with any value of (index).
     * @param index Which item to return.
     */
   const ItemType & GetWithDefault(uint32 index) const {return (index<_itemCount)?(*this)[index]:GetDefaultItem();}

   /** Returns a reference to the (index)'th item in the Queue, if such an item exists,
     * or the supplied default item if it doesn't.  Unlike the [] operator,
     * it is okay to call this method with any value of (index).
     * @param index Which item to return.
     * @param defItem An item to return if (index) isn't a valid value.
     */
   const ItemType & GetWithDefault(uint32 index, const ItemType & defItem) const {return (index<_itemCount)?(*this)[index]:defItem;}

   /** Replaces the (index)'th item in the queue with (newItem).
    *  @param index Which item to replace--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @param newItem The item to place into the queue at the (index)'th position.
    *  @return B_NO_ERROR on success, B_ERROR on failure (e.g. bad index)
    */
   QQ_UniversalSinkItemRef status_t ReplaceItemAt(uint32 index, QQ_SinkItemParam newItem);

   /** As above, except the specified item is replaced with a default item.
    *  @param index Which item to replace--can range from zero
    *               (head of the queue) to (GetNumItems()-1) (tail of the queue).
    *  @return B_NO_ERROR on success, B_ERROR on failure (e.g. bad index)
    */
   status_t ReplaceItemAt(uint32 index) {return ReplaceItemAt(index, GetDefaultItem());}

   /** Inserts (item) into the (nth) slot in the array.  InsertItemAt(0)
    *  is the same as AddHead(item), InsertItemAt(GetNumItems()) is the same
    *  as AddTail(item).  Other positions will involve an O(n) shifting of contents.
    *  @param index The position at which to insert the new item.
    *  @param newItem The item to insert into the queue.
    *  @return B_NO_ERROR on success, B_ERROR on failure (i.e. bad index).
    */
   QQ_UniversalSinkItemRef status_t InsertItemAt(uint32 index, QQ_SinkItemParam newItem);

   /** As above, except that a default item is inserted.
    *  @param index The position at which to insert the new item.
    *  @return B_NO_ERROR on success, B_ERROR on failure (i.e. bad index).
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
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
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
    *  @return B_NO_ERROR on success, B_ERROR on failure (out of memory)
    */
   status_t InsertItemsAt(uint32 index, const ItemType * items, uint32 numNewItems);

   /** Removes all items from the queue.
    *  @param releaseCachedBuffers If true, we will immediately free any buffers that we may be holding.  Otherwise
    *                              we will keep them around so that they can be re-used in the future.
    */
   void Clear(bool releaseCachedBuffers = false);

   /** This version of Clear() merely sets our held item-count to zero; it doesn't actually modify the
     * state of any items in the Queue.  This is very efficient, but be careful when using it with non-POD
     * types; for example, if you have a Queue<MessageRef> and you call FastClear() on it, the Messages
     * referenced by the MessageRef objects will not get recycled during the Clear() call because the
     * MessageRefs still exist in the Queue's internal array, even though they aren't readily accessible
     * anymore.  Only call this method if you know what you are doing!
     */
   void FastClear() {_itemCount = _headIndex = _tailIndex = 0;}

   /** Returns the number of items in the queue.  (This number does not include pre-allocated space) */
   uint32 GetNumItems() const {return _itemCount;}

   /** Returns the total number of item-slots we have allocated space for.  Note that this is NOT
    *  the same as the value returned by GetNumItems() -- it is generally larger, since we often
    *  pre-allocate additional slots in advance, in order to cut down on the number of re-allocations
    *  we need to peform.
    */
   uint32 GetNumAllocatedItemSlots() const {return _queueSize;}

   /** Returns the number of "extra" (i.e. currently unoccupied) array slots we currently have allocated.
     * Attempting to add more than (this many) additional items to this Queue will cause a memory reallocation.
     */
   uint32 GetNumUnusedItemSlots() const {return _queueSize-_itemCount;}

   /** Returns the number of bytes of memory taken up by this Queue's data */
   uint32 GetTotalDataSize() const {return sizeof(*this)+(GetNumAllocatedItemSlots()*sizeof(ItemType));}

   /** Convenience method:  Returns true iff there are no items in the queue. */
   bool IsEmpty() const {return (_itemCount == 0);}

   /** Convenience method:  Returns true iff there is at least one item in the queue. */
   bool HasItems() const {return (_itemCount > 0);}

   /** Returns a read-only reference the head item in the queue.  You must not call this when the queue is empty! */
   const ItemType & Head() const {return *GetItemAtUnchecked(0);}

   /** Returns a read-only reference the tail item in the queue.  You must not call this when the queue is empty! */
   const ItemType & Tail() const {return *GetItemAtUnchecked(_itemCount-1);}

   /** Returns a read-only reference the head item in the queue, or a default item if the Queue is empty. */
   const ItemType & HeadWithDefault() const {return HasItems() ? Head() : GetDefaultItem();}

   /** Returns a read-only reference the head item in the queue, or to the supplied default item if the Queue is empty.
     * @param defaultItem An item to return if the Queue is empty.
     */
   const ItemType & HeadWithDefault(const ItemType & defaultItem) const {return HasItems() ? Head() : defaultItem;}

   /** Returns a read-only reference the tail item in the queue, or a default item if the Queue is empty. */
   const ItemType & TailWithDefault() const {return HasItems() ? Tail() : GetDefaultItem();}

   /** Returns a read-only reference the tail item in the queue, or to the supplied default item if the Queue is empty.
     * @param defaultItem An item to return if the Queue is empty.
     */
   const ItemType & TailWithDefault(const ItemType & defaultItem) const {return HasItems() ? Tail() : defaultItem;}

   /** Returns a writable reference the head item in the queue.  You must not call this when the queue is empty! */
   ItemType & Head() {return *GetItemAtUnchecked(0);}

   /** Returns a writable reference the tail item in the queue.  You must not call this when the queue is empty! */
   ItemType & Tail() {return *GetItemAtUnchecked(_itemCount-1);}

   /** Returns a pointer to the first item in the queue, or NULL if the queue is empty */
   ItemType * HeadPointer() const {return GetItemAt(0);}

   /** Returns a pointer to the last item in the queue, or NULL if the queue is empty */
   ItemType * TailPointer() const {return GetItemAt(_itemCount-1);}

   /** Convenient read-only array-style operator (be sure to only use valid indices!) */
   const ItemType & operator [](uint32 Index) const;

   /** Convenient read-write array-style operator (be sure to only use valid indices!) */
   ItemType & operator [](uint32 Index);

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
    *                     existing size.  Defaults to false (i.e. only reallocate if the desired size is greater than
    *                     the existing size).
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory)
    */
   status_t EnsureSize(uint32 numSlots, bool setNumItems = false, uint32 extraReallocItems = 0, bool allowShrink = false) {return EnsureSizeAux(numSlots, setNumItems, extraReallocItems, NULL, allowShrink);}

   /** Convenience wrapper around EnsureSize():  This method ensures that this Queue has enough
     * extra space allocated to fit another (numExtraSlots) items without having to do a reallocation.
     * If it doesn't, it will do a reallocation so that it does have at least that much extra space.
     * @param numExtraSlots How many extra items we want to ensure room for.  Defaults to 1.
     * @returns B_NO_ERROR if the extra space now exists, or B_ERROR on failure (out of memory?)
     */
   status_t EnsureCanAdd(uint32 numExtraSlots = 1) {return EnsureSize(GetNumItems()+numExtraSlots);}

   /** Convenience wrapper around EnsureSize():  This method shrinks the Queue so that its size is
     * equal to the size of the data it contains, plus (numExtraSlots).
     * @param numExtraSlots the number of extra empty slots the Queue should contains after the shrink.
     *                      Defaults to zero.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
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
   bool Contains(const ItemType & item, uint32 startAt = 0, uint32 endAtPlusOne = MUSCLE_NO_LIMIT) const {return (IndexOf(item, startAt, endAtPlusOne) >= 0);}

   /** Returns the first index of the given (item), or -1 if (item) is not found in the list.  O(n) search time.
    *  @param item The item to look for.
    *  @param startAt The first index in the list to look at.  Defaults to zero.
    *  @param endAtPlusOne One more than the final index to look at.  If this value is greater than
    *               the number of items in the list, it will be clamped internally to be equal
    *               to the number of items in the list.  Defaults to MUSCLE_NO_LIMIT.
    *  @return The index of the first item found, or -1 if no such item was found in the specified range.
    */
   int32 IndexOf(const ItemType & item, uint32 startAt = 0, uint32 endAtPlusOne = MUSCLE_NO_LIMIT) const;

   /** Returns the last index of the given (item), or -1 if (item) is not found in the list.  O(n) search time.
    *  This method is different from IndexOf() in that this method searches backwards in the list.
    *  @param item The item to look for.
    *  @param startAt The initial index in the list to look at.  If this value is greater than or equal to
    *                 the size of the list, it will be clamped down to (numItems-1).   Defaults to MUSCLE_NO_LIMIT.
    *  @param endAt The final index in the list to look at.  Defaults to zero, which means
    *               to search back to the beginning of the list, if necessary.
    *  @return The index of the first item found in the reverse search, or -1 if no such item was found in the specified range.
    */
   int32 LastIndexOf(const ItemType & item, uint32 startAt = MUSCLE_NO_LIMIT, uint32 endAt = 0) const;

   /**
    *  Swaps the values of the two items at the given indices.  This operation
    *  will involve three copies of the held items.
    *  @param fromIndex First index to swap.
    *  @param toIndex   Second index to swap.
    */
   void Swap(uint32 fromIndex, uint32 toIndex);

   /**
    *  Reverses the ordering of the items in the given subrange.
    *  (e.g. if the items were A,B,C,D,E, this would change them to E,D,C,B,A)
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
   void SwapContents(Queue<ItemType> & that);

   /**
    *  Goes through the array and removes every item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return The number of instances of (val) that were found and removed during this operation.
    */
   uint32 RemoveAllInstancesOf(const ItemType & val);

   /**
    *  Goes through the array and removes all duplicate items.
    *  @param assumeAlreadySorted If set to true, the algorithm will assume the array is
    *                             already sorted by value.  Otherwise it will sort the array
    *                             (so that duplicates are next to each other) before doing the
    *                             duplicate-removal step.  Defaults to false.
    *  @return The number of duplicate items that were found and removed during this operation.
    */
   uint32 RemoveDuplicateItems(bool assumeAlreadySorted = false);

   /**
    *  Goes through the array and removes the first item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return B_NO_ERROR if a matching item was found and removed, or B_ERROR if it wasn't found.
    */
   status_t RemoveFirstInstanceOf(const ItemType & val);

   /**
    *  Goes through the array and removes the last item that is equal to (val).
    *  @param val the item to look for and remove
    *  @return B_NO_ERROR if a matching item was found and removed, or B_ERROR if it wasn't found.
    */
   status_t RemoveLastInstanceOf(const ItemType & val);

   /** Returns true iff the first item in our queue is equal to (prefix). */
   bool StartsWith(const ItemType & prefix) const {return ((HasItems())&&(Head() == prefix));}

   /** Returns true iff the (prefixQueue) is a prefix of this queue. */
   bool StartsWith(const Queue<ItemType> & prefixQueue) const;

   /** Returns true iff the last item in our queue is equal to (suffix). */
   bool EndsWith(const ItemType & suffix) const {return ((HasItems())&&(Tail() == suffix));}

   /** Returns true iff the (suffixQueue) is a suffix of this queue. */
   bool EndsWith(const Queue<ItemType> & suffixQueue) const;

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
   ItemType * GetArrayPointer(uint32 whichArray, uint32 & retLength) {return const_cast<ItemType *>(GetArrayPointerAux(whichArray, retLength));}

   /** Read-only version of the above */
   const ItemType * GetArrayPointer(uint32 whichArray, uint32 & retLength) const {return GetArrayPointerAux(whichArray, retLength);}

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
   bool IsNormalized() const {return ((_itemCount == 0)||(_headIndex <= _tailIndex));}

   /** Returns true iff (val) is physically located in this container's internal items array.
     * @param val Reference to an item.
     */
   bool IsItemLocatedInThisContainer(const ItemType & val) const {return ((HasItems())&&((uintptr)((&val)-_queue) < (uintptr)GetNumItems()));}

   /** Returns a read-only reference to a default-constructed item of this Queue's type.  This item will be valid as long as this Queue is valid. */
   const ItemType & GetDefaultItem() const {return GetDefaultObjectForType<ItemType>();}

   /** Returns a pointer to our internally held array of items.  Note that this array's items are not guaranteed
     * to be stored in order -- in particular, the items may be "wrapped around" the end of the array, with the
     * first items in the sequence appearing near the end of the array.  Only use this function if you know exactly
     * what you are doing!
     */
   ItemType * GetRawArrayPointer() {return _queue;}

   /** As above, but provides read-only access */
   const ItemType * GetRawArrayPointer() const {return _queue;}

private:
   status_t EnsureSizeAux(uint32 numSlots, ItemType ** optRetOldArray) {return EnsureSizeAux(numSlots, false, 0, optRetOldArray, false);}
   status_t EnsureSizeAux(uint32 numSlots, bool setNumItems, uint32 extraReallocItems, ItemType ** optRetOldArray, bool allowShrink);
   const ItemType * GetArrayPointerAux(uint32 whichArray, uint32 & retLength) const;
   void SwapContentsAux(Queue<ItemType> & that);

   inline uint32 NextIndex(uint32 idx) const {return (idx >= _queueSize-1) ? 0 : idx+1;}
   inline uint32 PrevIndex(uint32 idx) const {return (idx == 0) ? _queueSize-1 : idx-1;}

   // Translates a user-index into an index into the _queue array.
   inline uint32 InternalizeIndex(uint32 idx) const {return (_headIndex + idx) % _queueSize;}

   // Helper methods, used for sorting (stolen from http://www-ihm.lri.fr/~thomas/VisuTri/inplacestablesort.html)
   template<class CompareFunctorType> void   Merge(const CompareFunctorType & compareFunctor, uint32 from, uint32 pivot, uint32 to, uint32 len1, uint32 len2, void * optCookie);
   template<class CompareFunctorType> uint32 Lower(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const;
   template<class CompareFunctorType> uint32 Upper(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, const ItemType & val, void * optCookie) const;

   ItemType _smallQueue[SMALL_QUEUE_SIZE];  // small queues can be stored inline in this array
   ItemType * _queue;                       // points to _smallQueue, or to a dynamically alloc'd array
   uint32 _queueSize;  // number of slots in the _queue array
   uint32 _itemCount;  // number of valid items in the array
   uint32 _headIndex;  // index of the first filled slot (meaningless if _itemCount is zero)
   uint32 _tailIndex;  // index of the last filled slot (meaningless if _itemCount is zero)
};

template <class ItemType>
Queue<ItemType>::Queue()
   : _queue(NULL), _queueSize(0), _itemCount(0)
{
   // empty
}

template <class ItemType>
Queue<ItemType>::Queue(const Queue& rhs)
   : _queue(NULL), _queueSize(0), _itemCount(0)
{
   *this = rhs;
}

template <class ItemType>
bool
Queue<ItemType>::operator ==(const Queue& rhs) const
{
   if (this == &rhs) return true;
   if (GetNumItems() != rhs.GetNumItems()) return false;

   for (int i = GetNumItems()-1; i>=0; i--) if (((*this)[i] == rhs[i]) == false) return false;

   return true;
}

template <class ItemType>
Queue<ItemType> &
Queue<ItemType>::operator =(const Queue& rhs)
{
   if (this != &rhs)
   {
      uint32 hisNumItems = rhs.GetNumItems();
           if (hisNumItems == 0) Clear(true);  // FogBugz #10274
      else if (EnsureSize(hisNumItems, true) == B_NO_ERROR) for (uint32 i=0; i<hisNumItems; i++) (*this)[i] = rhs[i];
   }
   return *this;
}

template <class ItemType>
status_t
Queue<ItemType>::CopyFrom(const Queue & rhs)
{
   if (this == &rhs) return B_NO_ERROR;

   uint32 numItems = rhs.GetNumItems();
   if (EnsureSize(numItems, true) == B_NO_ERROR)
   {
      for (uint32 i=0; i<numItems; i++) (*this)[i] = rhs[i];
      return B_NO_ERROR;
   }
   return B_ERROR;
}

template <class ItemType>
ItemType &
Queue<ItemType>::operator[](uint32 i)
{
   MASSERT(i<_itemCount, "Invalid index to Queue::[]");
   return _queue[InternalizeIndex(i)];
}

template <class ItemType>
const ItemType &
Queue<ItemType>::operator[](uint32 i) const
{
   MASSERT(i<_itemCount, "Invalid index to Queue::[]");
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
   ItemType * oldArray;
   if (EnsureSizeAux(_itemCount+1, false, _itemCount+1, &oldArray, false) != B_NO_ERROR) return NULL;

   if (_itemCount == 0) _headIndex = _tailIndex = 0;
                   else _tailIndex = NextIndex(_tailIndex);
   _itemCount++;
   ItemType * ret = &_queue[_tailIndex];
   *ret = item;
   delete [] oldArray;  // must do this AFTER the last reference to (item), in case (item) was part of (oldArray)
   return ret;
}

template <class ItemType>
ItemType *
Queue<ItemType>::
AddTailAndGet()
{
   if (EnsureSize(_itemCount+1, false, _itemCount+1) != B_NO_ERROR) return NULL;
   if (_itemCount == 0) _headIndex = _tailIndex = 0;
                   else _tailIndex = NextIndex(_tailIndex);
   _itemCount++;
   return &_queue[_tailIndex];
}

template <class ItemType>
status_t
Queue<ItemType>::
AddTailMulti(const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);

   uint32 mySize = GetNumItems();
   uint32 newSize = mySize+numNewItems;
   if (EnsureSize(newSize, true) != B_NO_ERROR) return B_ERROR;
   for (uint32 i=mySize; i<newSize; i++) (*this)[i] = queue[startIndex++];

   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
AddTailMulti(const ItemType * items, uint32 numItems)
{
   uint32 mySize = GetNumItems();
   uint32 newSize = mySize+numItems;
   uint32 rhs = 0;

   ItemType * oldArray;
   if (EnsureSizeAux(newSize, true, 0, &oldArray, false) != B_NO_ERROR) return B_ERROR;
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
   ItemType * oldArray;
   if (EnsureSizeAux(_itemCount+1, false, _itemCount+1, &oldArray, false) != B_NO_ERROR) return NULL;
   if (_itemCount == 0) _headIndex = _tailIndex = 0;
                   else _headIndex = PrevIndex(_headIndex);
   _itemCount++;
   ItemType * ret = &_queue[_headIndex];
   *ret = QQ_ForwardItem(item);
   delete [] oldArray;  // must be done after all references to (item)!
   return ret;
}

template <class ItemType>
ItemType *
Queue<ItemType>::
AddHeadAndGet()
{
   if (EnsureSize(_itemCount+1, false, _itemCount+1) != B_NO_ERROR) return NULL;
   if (_itemCount == 0) _headIndex = _tailIndex = 0;
                   else _headIndex = PrevIndex(_headIndex);
   _itemCount++;
   return &_queue[_headIndex];
}

template <class ItemType>
status_t
Queue<ItemType>::
AddHeadMulti(const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);

   if (EnsureSize(numNewItems+GetNumItems()) != B_NO_ERROR) return B_ERROR;
   for (int i=((int)startIndex+numNewItems)-1; i>=(int32)startIndex; i--) (void) AddHead(queue[i]);  // guaranteed not to fail
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
AddHeadMulti(const ItemType * items, uint32 numItems)
{
   ItemType * oldArray;
   if (EnsureSizeAux(_itemCount+numItems, &oldArray) != B_NO_ERROR) return B_ERROR;
   for (int i=((int)numItems)-1; i>=0; i--) (void) AddHead(items[i]);  // guaranteed not to fail
   delete [] oldArray;  // must be done last!
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveHead(ItemType & returnItem)
{
   if (_itemCount == 0) return B_ERROR;
   returnItem = _queue[_headIndex];
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
   if (_itemCount == 0) return B_ERROR;
   int oldHeadIndex = _headIndex;
   _headIndex = NextIndex(_headIndex);
   _itemCount--;
   _queue[oldHeadIndex] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
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
      ItemType ret = Head();
      (void) RemoveHead();
      return ret;
   }
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveTail(ItemType & returnItem)
{
   if (_itemCount == 0) return B_ERROR;
   returnItem = _queue[_tailIndex];
   return RemoveTail();
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveTail()
{
   if (_itemCount == 0) return B_ERROR;
   int removedItemIndex = _tailIndex;
   _tailIndex = PrevIndex(_tailIndex);
   _itemCount--;
   _queue[removedItemIndex] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
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
      ItemType ret = Tail();
      (void) RemoveTail();
      return ret;
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
   else return B_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveItemAt(uint32 index, ItemType & returnItem)
{
   if (index >= _itemCount) return B_ERROR;
   returnItem = _queue[InternalizeIndex(index)];
   return RemoveItemAt(index);
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveItemAt(uint32 index)
{
   if (index >= _itemCount) return B_ERROR;

   uint32 internalizedIndex = InternalizeIndex(index);
   uint32 indexToClear;

   if (index < _itemCount/2)
   {
      // item is closer to the head:  shift everything forward one, ending at the head
      while(internalizedIndex != _headIndex)
      {
         uint32 prev = PrevIndex(internalizedIndex);
         _queue[internalizedIndex] = _queue[prev];
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
         uint32 next = NextIndex(internalizedIndex);
         _queue[internalizedIndex] = _queue[next];
         internalizedIndex = next;
      }
      indexToClear = _tailIndex;
      _tailIndex = PrevIndex(_tailIndex);
   }

   _itemCount--;
   _queue[indexToClear] = GetDefaultItem();  // this must be done last, as queue state must be coherent when we do this
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
      ItemType ret = (*this)[index];
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
   if (index >= _itemCount) return B_ERROR;
   _queue[InternalizeIndex(index)] = QQ_ForwardItem(newItem);
   return B_NO_ERROR;
}

template <class ItemType>
QQ_UniversalSinkItemRef
status_t
Queue<ItemType>::
InsertItemAt(uint32 index, QQ_SinkItemParam newItem)
{
   if ((GetNumUnusedItemSlots() < 1)&&(IsItemLocatedInThisContainer(newItem)))
   {
      ItemType temp = QQ_ForwardItem(newItem); // avoid dangling pointer issue by copying the item to a temporary location
      return InsertItemAt(index, temp);
   }

   // Simple cases
   if (index >  _itemCount) return B_ERROR;
   if (index == _itemCount) return AddTail(QQ_ForwardItem(newItem));
   if (index == 0)          return AddHead(QQ_ForwardItem(newItem));

   // Harder case:  inserting into the middle of the array
   if (index < _itemCount/2)
   {
      // Add a space at the front, and shift things back
      if (AddHead() != B_NO_ERROR) return B_ERROR;  // allocate an extra slot
      for (uint32 i=0; i<index; i++) ReplaceItemAt(i, QQ_ForwardItem(*GetItemAtUnchecked(i+1)));
   }
   else
   {
      // Add a space at the rear, and shift things forward
      if (AddTail() != B_NO_ERROR) return B_ERROR;  // allocate an extra slot
      for (int32 i=((int32)_itemCount)-1; i>((int32)index); i--) ReplaceItemAt(i, QQ_ForwardItem(*GetItemAtUnchecked(i-1)));
   }
   return ReplaceItemAt(index, QQ_ForwardItem(newItem));
}

template <class ItemType>
status_t
Queue<ItemType>::
InsertItemsAt(uint32 index, const Queue<ItemType> & queue, uint32 startIndex, uint32 numNewItems)
{
   uint32 hisSize = queue.GetNumItems();
   numNewItems = muscleMin(numNewItems, (startIndex < hisSize) ? (hisSize-startIndex) : 0);
   if (numNewItems == 0) return B_NO_ERROR;
   if (index > _itemCount) return B_ERROR;
   if (numNewItems == 1)
   {
      if (index == 0)          return AddHead(queue.Head());
      if (index == _itemCount) return AddTail(queue.Head());
   }

   uint32 oldSize = GetNumItems();
   uint32 newSize = oldSize+numNewItems;

   if (EnsureSize(newSize, true) != B_NO_ERROR) return B_ERROR;
   for (uint32 i=index; i<oldSize; i++)           (*this)[i+numNewItems] = (*this)[i];
   for (uint32 i=index; i<index+numNewItems; i++) (*this)[i]             = queue[startIndex++];
   return B_NO_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
InsertItemsAt(uint32 index, const ItemType * items, uint32 numNewItems)
{
   if (numNewItems == 0) return B_NO_ERROR;
   if (index > _itemCount) return B_ERROR;
   if (numNewItems == 1)
   {
      if (index == 0)          return AddHead(*items);
      if (index == _itemCount) return AddTail(*items);
   }

   uint32 oldSize = GetNumItems();
   uint32 newSize = oldSize+numNewItems;

   ItemType * oldItems;
   if (EnsureSizeAux(newSize, true, &oldItems, NULL, false) != B_NO_ERROR) return B_ERROR;
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
      _itemCount = 0;
   }
   else if (HasItems())
   {
      for (uint32 i=0; i<2; i++)
      {
         uint32 arrayLen = 0;  // set to zero just to shut the compiler up
         const ItemType & defaultItem = GetDefaultItem();
         ItemType * p = GetArrayPointer(i, arrayLen);
         if (p) {for (uint32 j=0; j<arrayLen; j++) p[j] = defaultItem;}
      }
      FastClear();
   }
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
      uint32 temp    = size + extraPreallocs;
      uint32 newQLen = muscleMax((uint32)SMALL_QUEUE_SIZE, ((setNumItems)||(temp <= sqLen)) ? muscleMax(sqLen,temp) : temp);

      ItemType * newQueue = ((_queue == _smallQueue)||(newQLen > sqLen)) ? newnothrow_array(ItemType,newQLen) : _smallQueue;

      if (newQueue == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
      if (newQueue == _smallQueue) newQLen = sqLen;

      // The (_queueSize > 0) check below isn't strictly necessary, but it makes clang++ feel better
      if (_queueSize > 0) for (uint32 i=0; i<_itemCount; i++) newQueue[i] = QQ_PlunderItem(*GetItemAtUnchecked(i));  // we know that (_itemCount < size)

      if (setNumItems) _itemCount = size;
      _headIndex = 0;
      _tailIndex = _itemCount-1;

           if (_queue == _smallQueue) for (uint32 i=0; i<sqLen; i++) _smallQueue[i] = GetDefaultItem();
      else if (retOldArray) *retOldArray = _queue;
      else delete [] _queue;

      _queue = newQueue;
      _queueSize = newQLen;
   }

   if (setNumItems)
   {
      // Force ourselves to contain exactly the required number of items
      if (size > _itemCount)
      {
         // We can do this quickly because the "new" items are already initialized properly
         _tailIndex = PrevIndex((_headIndex+size)%_queueSize);
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
            return (InsertItemAt(insertAfter+1, QQ_ForwardItem(item)) == B_NO_ERROR) ? (insertAfter+1) : -1;
   return (AddHead(QQ_ForwardItem(item)) == B_NO_ERROR) ? 0 : -1;
}

template <class ItemType>
template <class CompareFunctorType>
void
Queue<ItemType>::
Sort(const CompareFunctorType & compareFunctor, uint32 from, uint32 to, void * optCookie)
{
   uint32 size = GetNumItems();
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
                  int ret = compareFunctor.Compare(*(GetItemAtUnchecked(j)), *(GetItemAtUnchecked(j-1)), optCookie);
                  if (ret < 0) Swap(j, j-1);
                          else break;
               }
            }
         }
      }
      else
      {
         // Okay, do the real thing
         uint32 middle = (from + to)/2;
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
   uint32 size = GetNumItems();
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
   uint32 ni = GetNumItems();
   for (uint32 i=0; i<ni; i++) if ((*this)[i] == val) return RemoveItemAt(i);
   return B_ERROR;
}

template <class ItemType>
status_t
Queue<ItemType>::
RemoveLastInstanceOf(const ItemType & val)
{
   for (int32 i=((int32)GetNumItems())-1; i>=0; i--) if ((*this)[i] == val) return RemoveItemAt(i);
   return B_ERROR;
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
   uint32 ret      = 0;
   uint32 writeTo  = 0;
   uint32 origSize = GetNumItems();
   for(uint32 readFrom=0; readFrom<origSize; readFrom++)
   {
      const ItemType & nextRead = (*this)[readFrom];
      if (nextRead == val) ret++;
      else
      {
         if (readFrom > writeTo) (*this)[writeTo] = nextRead;
         writeTo++;
      }
   }

   // Now get rid of any now-surplus slots
   for (; writeTo<origSize; writeTo++) RemoveTail();

   return ret;
}

template <class ItemType>
uint32
Queue<ItemType>::
RemoveDuplicateItems(bool assumeAlreadySorted)
{
   if (IsEmpty()) return 0;  // nothing to do!
   if (assumeAlreadySorted == false) Sort();

   uint32 numWrittenItems = 1;  // we'll always keep the first item
   uint32 totalItems = GetNumItems();
   for (uint32 i=0; i<totalItems; i++)
   {
      const ItemType & nextItem = (*this)[i];
      const ItemType & wItem    = (*this)[numWrittenItems-1];
      if (nextItem != wItem) (*this)[numWrittenItems++] = nextItem;
   }

   uint32 ret = GetNumItems()-numWrittenItems;
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
               uint32 shift = pivot - first_cut;
               uint32 p1 = first_cut+n;
               uint32 p2 = p1+shift;
               while (p2 != first_cut + n)
               {
                  ReplaceItemAt(p1, QQ_PlunderItem(*GetItemAtUnchecked(p2)));
                  p1 = p2;
                  if (second_cut - p2 > shift) p2 += shift;
                                          else p2  = first_cut + (shift - (second_cut - p2));
               }
               ReplaceItemAt(p1, QQ_PlunderItem(val));
            }
         }

         uint32 new_mid = first_cut+len22;
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
      while (len > 0)
      {
         uint32 half = len/2;
         uint32 mid  = from + half;
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
      while (len > 0)
      {
         uint32 half = len/2;
         uint32 mid  = from + half;
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
   if (_itemCount > 0)
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
Queue<ItemType>::SwapContents(Queue<ItemType> & that)
{
   if (&that == this) return;  // no point trying to swap with myself

   bool thisSmall = (_queue == _smallQueue);
   bool thatSmall = (that._queue == that._smallQueue);

   if ((thisSmall)&&(thatSmall))
   {
      // First, move any extra items from the longer queue to the shorter one...
      uint32 commonSize = muscleMin(GetNumItems(), that.GetNumItems());
      int32 sizeDiff    = ((int32)GetNumItems())-((int32)that.GetNumItems());
      Queue<ItemType> & copyTo   = (sizeDiff > 0) ? that : *this;
      Queue<ItemType> & copyFrom = (sizeDiff > 0) ? *this : that;
      (void) copyTo.AddTailMulti(copyFrom, commonSize);   // guaranteed not to fail
      (void) copyFrom.EnsureSize(commonSize, true);  // remove the copied items from (copyFrom)

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
Queue<ItemType>::SwapContentsAux(Queue<ItemType> & largeThat)
{
   // First, copy over our (small) contents to his small-buffer
   uint32 ni = GetNumItems();
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
   uint32 prefixQueueLen = prefixQueue.GetNumItems();
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
         uint32 startAt = _tailIndex+1;
         for (uint32 i=0; i<_itemCount; i++)
         {
            ItemType & from = (*this)[i];
            _queue[startAt+i] = from;
            from = GetDefaultItem();  // clear the old slot to avoid leaving extra Refs, etc
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
             ItemType tmp = _queue[v];
             c++;
             while(tp != v)
             {
                 _queue[t] = _queue[tp];
                 t = tp;
                 tp += _headIndex;
                 if (tp >= _queueSize) tp -= _queueSize;
                 c++;
             }
             _queue[t] = tmp;
         }
         _headIndex = 0;
         _tailIndex = _itemCount-1;
      }
   }
}

}; // end namespace muscle

#endif
