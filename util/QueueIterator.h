/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQueueIterator_h
#define MuscleQueueIterator_h

#include "support/MuscleSupport.h"

namespace muscle {

template <class ItemType> class Queue; // forward declaration

/**
 * This class is an iterator object, useful for iterating over the sequence
 * of items in a Queue.  You can use this if you prefer syntax that is a little
 * safer than the traditional <pre>for (uint32 i=0; i<q.GetNumItems(); i++) {...}</pre> loop.
 *
 * Given a Queue object, you can obtain one or more of these
 * iterator objects by calling the Queue's GetIterator() method.
 *
 * The most common form for a Queue iteration is this:<pre>
 *
 * const Queue<int> q = {1,2,3};
 * for (auto iter = q.GetIterator(); iter.HasData(); iter++)
 * {
 *    const uint32 idx = iter.GetIndex();
 *    const int & nextValue = iter.GetValue();
 *    [...]
 * }</pre>
 *
 * @tparam ItemType the type of the items in the Queue that we will iterate over.
 * @note to iterate over a const Queue, use the ConstQueueIterator class instead.
 */
template <class ItemType> class MUSCLE_NODISCARD QueueIterator MUSCLE_FINAL_CLASS
{
public:
   /**
    * Default constructor.  It's here only so that you can include QueueIterators
    * as member variables, in arrays, etc.  QueueIterators created with this
    * constructor are "empty", so they won't be very useful until you set them equal
    * to a valid/non-empty QueueIterator.
    */
   QueueIterator() : _queue(const_cast< Queue<ItemType *> * >(&GetDefaultObjectForType< Queue<ItemType> >())), _currentIndex(0), _stride(1) {/* empty */}

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by queue.GetIteratorAt().
     * @param queue the Queue to iterate over.
     * @param startIndex index of the first value that should be returned by the iteration.  If (startIndex) is not a valid index into
     *                   the Queue, this iterator will not return any results.  Defaults to zero.
     * @param stride the number our ++() operator should add to our current-index state.  Defaults to 1 (i.e. forward-iteration)
     *               but you could pass -1 for backward-iteration, or even other values if you wanted to skip past items during the iteration.
     * @note the QueueIterator object retains a pointer to (queue) and requires the Queue to remain valid for the duration of the iteration.
     */
   QueueIterator(Queue<ItemType> & queue, uint32 startIndex = 0, int32 stride = 1) : _queue(&queue), _currentIndex(startIndex), _stride(stride) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   QueueIterator(const QueueIterator & rhs) : _queue(rhs._queue), _currentIndex(rhs._currentIndex), _stride(rhs._stride) {/* empty */}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** This constructor is declared deleted to keep QueueIterators from being accidentally associated with temporary Queue objects */
   QueueIterator(Queue<ItemType> &&) = delete;

   /** This constructor is declared deleted to keep QueueIterators from being accidentally associated with temporary Queue objects */
   QueueIterator(Queue<ItemType> &&, uint32) = delete;

   /** This constructor is declared deleted to keep QueueIterators from being accidentally associated with temporary Queue objects */
   QueueIterator(Queue<ItemType> &&, uint32, int32) = delete;
#endif

   /** Destructor */
   ~QueueIterator() {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   QueueIterator & operator=(const QueueIterator & rhs) {_queue = rhs._queue; _currentIndex = rhs._currentIndex; _stride = rhs._stride; return *this;}

   /** Advances this iterator by one step in the queue.
     * @note the direction and stride of the step is determined by the (stride) argument to our constructor
     */
   void operator++(int) {_currentIndex += _stride;}

   /** Retracts this iterator by one step in the queue.  The opposite of the ++ operator.
     * @note the direction and stride of the step is determined by the (stride) argument to our constructor
     */
   void operator--(int) {_currentIndex -= _stride;}

   /** Returns true iff this iterator is pointing to valid key/value data. */
   MUSCLE_NODISCARD bool HasData() const {return _queue->IsIndexValid(_currentIndex);}

   /** Returns the index into the Queue that iterator is currently pointing at (0=first item, 1=second item, etc). */
   MUSCLE_NODISCARD uint32 GetIndex() const {return _currentIndex;}

   /**
    * Returns a reference to the value in the Queue that this iterator is currently pointing at.
    * @note this method's implementation does no error-checking at all, so you need to be entirely certain that this iterator
    *       is pointing to a valid item (i.e. HasData() returns true) before calling it, or you'll invoke undefined behavior.
    */
   MUSCLE_NODISCARD ItemType & GetValue() const {return _queue->GetItemAtUnchecked(_currentIndex);}

   /** Synonym for GetValue(), so that you can write *qIter instead of qIter.GetValue() if you want */
   MUSCLE_NODISCARD ItemType & operator *() const {return GetValue();}

   /** Returns this iterator's stride (typically 1 for a forward-iterator or -1 for a backward-iterator) */
   MUSCLE_NODISCARD int32 GetStride() const {return _stride;}

   /** Returns a reference to the Queue we are iterating over */
   MUSCLE_NODISCARD Queue<ItemType> & GetQueue() const {return *_queue;}

   /** This method swaps the state of this iterator with the iterator in the argument.
    *  @param swapMe The iterator whose state we are to swap with
    */
   void SwapContents(QueueIterator & swapMe) MUSCLE_NOEXCEPT {muscleSwap(_queue, swapMe._queue); muscleSwap(_currentIndex, swapMe._currentIndex); muscleSwap(_stride, swapMe._stride);}

private:
   Queue<ItemType> * _queue;  // queue that we are associated with (guaranteed never to be NULL)
   uint32 _currentIndex;
   int32 _stride;  // e.g. 1 for forward-iteration, or -1 for reverse-iteration
};

/**
 * This class is a read-only iterator object, useful for iterating over the sequence
 * of items in a const Queue.  You can use this if you prefer syntax that is a little
 * safer than the traditional <pre>for (uint32 i=0; i<q.GetNumItems(); i++) {...}</pre> loop.
 *
 * Given a Queue object, you can obtain one or more of these
 * iterator objects by calling the Queue's GetIterator() method.
 *
 * The most common form for a Queue iteration is this:<pre>
 *
 * const Queue<int> q = {1,2,3};
 * for (auto iter = q.GetIterator(); iter.HasData(); iter++)
 * {
 *    const uint32 idx = iter.GetIndex();
 *    const int & nextValue = iter.GetValue();
 *    [...]
 * }</pre>
 *
 * @tparam ItemType the type of the items in the Queue that we will iterate over.
 * @note if you need to be able to modify the items as your iterate over them, use the QueueIterator class instead.
 */
template <class ItemType> class MUSCLE_NODISCARD ConstQueueIterator MUSCLE_FINAL_CLASS
{
public:
   /**
    * Default constructor.  It's here only so that you can include ConstQueueIterators
    * as member variables, in arrays, etc.  ConstQueueIterators created with this
    * constructor are "empty", so they won't be very useful until you set them equal
    * to a valid/non-empty ConstQueueIterator.
    */
   ConstQueueIterator() : _queue(&GetDefaultObjectForType< Queue<ItemType> >()), _currentIndex(0), _stride(1) {/* empty */}

   /** Convenience Constructor -- makes an iterator equivalent to the value returned by queue.GetIteratorAt().
     * @param queue the Queue to iterate over.
     * @param startIndex index of the first value that should be returned by the iteration.  If (startIndex) is not a valid index into
     *                   the Queue, this iterator will not return any results.  Defaults to zero.
     * @param stride the number our ++() operator should add to our current-index state.  Defaults to 1 (i.e. forward-iteration)
     *               but you could pass -1 for backward-iteration, or even other values if you wanted to skip past items during the iteration.
     * @note the ConstQueueIterator object retains a pointer to (queue) and requires the Queue to remain valid for the duration of the iteration.
     */
   ConstQueueIterator(const Queue<ItemType> & queue, uint32 startIndex = 0, int32 stride = 1) : _queue(&queue), _currentIndex(startIndex), _stride(stride) {/* empty */}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   ConstQueueIterator(const ConstQueueIterator & rhs) : _queue(rhs._queue), _currentIndex(rhs._currentIndex), _stride(rhs._stride) {/* empty */}

   /** Destructor */
   ~ConstQueueIterator() {/* empty */}

   /** Pseudo-copy-constructor
     * @param rhs the QueueIterator to make us a read-only copy of
     */
   ConstQueueIterator(const QueueIterator<ItemType> & rhs) {_queue = &rhs.GetQueue(); _currentIndex = rhs.GetIndex(); _stride = rhs.GetStride();}

#ifndef MUSCLE_AVOID_CPLUSPLUS11
   /** This constructor is declared deleted to keep ConstQueueIterators from being accidentally associated with temporary Queue objects */
   ConstQueueIterator(Queue<ItemType> &&) = delete;

   /** This constructor is declared deleted to keep ConstQueueIterators from being accidentally associated with temporary Queue objects */
   ConstQueueIterator(Queue<ItemType> &&, uint32) = delete;

   /** This constructor is declared deleted to keep ConstQueueIterators from being accidentally associated with temporary Queue objects */
   ConstQueueIterator(Queue<ItemType> &&, uint32, int32) = delete;
#endif

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   ConstQueueIterator & operator=(const ConstQueueIterator & rhs) {_queue = rhs._queue; _currentIndex = rhs._currentIndex; _stride = rhs._stride; return *this;}

   /** Pseudo-assignment-operator
     * @param rhs the QueueIterator to make us a read-only copy of
     */
   ConstQueueIterator & operator=(const QueueIterator<ItemType> & rhs) {_queue = &rhs.GetQueue(); _currentIndex = rhs.GetIndex(); _stride = rhs.GetStride(); return *this;}

   /** Advances this iterator by one step in the queue.
     * @note the direction and stride of the step is determined by the (stride) argument to our constructor
     */
   void operator++(int) {_currentIndex += _stride;}

   /** Retracts this iterator by one step in the queue.  The opposite of the ++ operator.
     * @note the direction and stride of the step is determined by the (stride) argument to our constructor
     */
   void operator--(int) {_currentIndex -= _stride;}

   /** Returns true iff this iterator is pointing to valid key/value data. */
   MUSCLE_NODISCARD bool HasData() const {return _queue->IsIndexValid(_currentIndex);}

   /** Returns the index into the Queue that iterator is currently pointing at (0=first item, 1=second item, etc). */
   MUSCLE_NODISCARD uint32 GetIndex() const {return _currentIndex;}

   /**
    * Returns a reference to the value in the Queue that this iterator is currently pointing at.
    * @note this method's implementation does no error-checking at all, so you need to be entirely certain that this iterator
    *       is pointing to a valid item (i.e. HasData() returns true) before calling it, or you'll invoke undefined behavior.
    */
   MUSCLE_NODISCARD const ItemType & GetValue() const {return _queue->GetItemAtUnchecked(_currentIndex);}

   /** Synonym for GetValue(), so that you can write *qIter instead of qIter.GetValue() if you want */
   MUSCLE_NODISCARD const ItemType & operator *() const {return GetValue();}

   /** Returns this iterator's stride (typically 1 for a forward-iterator or -1 for a backward-iterator) */
   MUSCLE_NODISCARD int32 GetStride() const {return _stride;}

   /** Returns a reference to the Queue we are iterating over */
   MUSCLE_NODISCARD const Queue<ItemType> & GetQueue() const {return *_queue;}

   /** This method swaps the state of this iterator with the iterator in the argument.
    *  @param swapMe The iterator whose state we are to swap with
    */
   void SwapContents(ConstQueueIterator & swapMe) MUSCLE_NOEXCEPT {muscleSwap(_queue, swapMe._queue); muscleSwap(_currentIndex, swapMe._currentIndex); muscleSwap(_stride, swapMe._stride);}

private:
   const Queue<ItemType> * _queue;  // queue that we are associated with (guaranteed never to be NULL)
   uint32 _currentIndex;
   int32 _stride;  // e.g. 1 for forward-iteration, or -1 for reverse-iteration
};

} // end namespace muscle

#endif
