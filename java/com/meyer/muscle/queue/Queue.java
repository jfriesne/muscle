/* This file is Copyright 2001 Level Control Systems.  See the included LICENSE.TXT file for details. */
package com.meyer.muscle.queue;

/* This file is Copyright 2000 Level Control Systems.  See the included LICENSE.txt file for details. */

/** This class implements a templated double-ended queue data structure.
 *  Adding or removing items from the head or tail of a Queue is (on average)
 *  an O(1) operation.
 */
public final class Queue
{
   /** Constructor.
    */
   public Queue()
   {
      // empty
   }

   /** Copy constructor. */
   public Queue(Queue copyMe)
   {
      setEqualTo(copyMe);
   }

   /** Make (this) a shallow copy of the passed-in queue. 
    *  @param setTo What to set this queue equal to.
    */
   public void setEqualTo(Queue setTo)
   {
      removeAllElements();
      int len = setTo.size();
      ensureSize(len);
      for (int i=0; i<len; i++) appendElement(setTo.elementAt(i));
   }

   /** Appends (obj) to the end of the queue.  Queue size grows by one.
    *  @param obj The element to append. 
    */
   public void appendElement(Object obj)
   {
      ensureSize(_elementCount+1);
      if (_elementCount == 0) _headIndex = _tailIndex = 0;
                         else _tailIndex = nextIndex(_tailIndex);
      _queue[_tailIndex] = obj;
      _elementCount++;
   }

   /** Prepends (obj) to the head of the queue.  Queue size grows by one.
    *  @param obj The Object to prepend. 
    */
   public void prependElement(Object obj)
   {
      ensureSize(_elementCount+1);
      if (_elementCount == 0) _headIndex = _tailIndex = 0;
                         else _headIndex = prevIndex(_headIndex);
      _queue[_headIndex] = obj;
      _elementCount++;
   }

   /** Removes and returns the element at the head of the queue.
    *  @return The removed Object on success, or null if the Queue was empty.
    */
   public Object removeFirstElement()
   {
      Object ret = null;
      if (_elementCount > 0)
      {
         ret = _queue[_headIndex];
         _queue[_headIndex] = null;   // allow garbage collection
         _headIndex = nextIndex(_headIndex);
         _elementCount--;
      }
      return ret;
   }
   
   /** Removes and returns the element at the tail of the Queue.
    *  @return The removed Object, or null if the Queue was empty.
    */
   public Object removeLastElement()
   {
      Object ret = null;
      if (_elementCount > 0)
      {       
         ret = _queue[_tailIndex];
         _queue[_tailIndex] = null;   // allow garbage collection
         _tailIndex = prevIndex(_tailIndex);
         _elementCount--;
      }
      return ret;
   }


   /** removes the element at the (index)'th position in the queue.
    *  @param idx Which element to remove--can range from zero 
    *               (head of the queue) to CountElements()-1 (tail of the queue).
    *  @return The removed Object, or null if the Queue was empty.
    *  Note that this method is somewhat inefficient for indices that
    *  aren't at the head or tail of the queue (i.e. O(n) time)
    *  @throws IndexOutOfBoundsException if (idx) isn't a valid index.
    */
   public Object removeElementAt(int idx)
   {
      if (idx == 0) return removeFirstElement();
      
      int index = internalizeIndex(idx);
      Object ret = _queue[index];   
      while(index != _tailIndex)
      {
         int next = nextIndex(index);
         _queue[index] = _queue[next];
         index = next;
      }
      _queue[_tailIndex] = null;    // allow garbage collection 
      _tailIndex = prevIndex(_tailIndex); 
      _elementCount--;
      return ret;
   }

   /** Copies the (index)'th element into (returnElement).
    *  @param index Which element to get--can range from zero 
    *               (head of the queue) to (CountElements()-1) (tail of the queue).
    *  @return The Object at the given index.
    *  @throws IndexOutOfBoundsException if (index) isn't a valid index.
    */
   public Object elementAt(int index) {return _queue[internalizeIndex(index)];}

   /** Replaces the (index)'th element in the queue with (newElement).
    *  @param index Which element to replace--can range from zero 
    *               (head of the queue) to (CountElements()-1) (tail of the queue).
    *  @param newElement The element to place into the queue at the (index)'th position.
    *  @return The Object that was previously in the (index)'th position.
    *  @throws IndexOutOfBoundsException if (index) isn't a valid index.
    */
   public Object setElementAt(int index, Object newElement)
   {
      int iidx = internalizeIndex(index);
      Object ret = _queue[iidx];
      _queue[iidx] = newElement;
      return ret;
   }
 
   /** Inserts (element) into the (nth) slot in the array.  InsertElementAt(0)
    *  is the same as addHead(element), InsertElementAt(CountElements()) is the same
    *  as addTail(element).  Other positions will involve an O(n) shifting of contents.
    *  @param index The position at which to insert the new element.
    *  @param newElement The element to insert into the queue.
    *  @throws IndexOutOfBoundsException if (index) isn't a valid index.
    */
   public void insertElementAt(int index, Object newElement)
   {
      if (index == 0) prependElement(newElement);
      else
      {
         // Harder case:  inserting into the middle of the array
         appendElement(null);  // just to allocate the extra slot
         int lo = (int) index;
         for (int i=_elementCount-1; i>lo; i--) _queue[internalizeIndex(i)] = _queue[internalizeIndex(i-1)];
         _queue[internalizeIndex(index)] = newElement;
      }
   }

   /** Removes all elements from the queue. */
   public void removeAllElements() 
   {
      if (_elementCount > 0)
      {
         // clear valid array elements to allow immediate garbage collection
         if (_tailIndex >= _headIndex) java.util.Arrays.fill(_queue, _headIndex, _tailIndex+1, null);
         else
         {
            java.util.Arrays.fill(_queue, 0,          _headIndex+1,  null);
            java.util.Arrays.fill(_queue, _tailIndex, _queue.length, null);
         }

         _elementCount = 0;
         _headIndex    = -1;
         _tailIndex    = -1; 
      }
   }

   /** Returns the number of elements in the queue.  (This number does not include pre-allocated space) */
   public int size() {return _elementCount;}

   /** Returns true iff their are no elements in the queue. */
   public boolean isEmpty() {return (_elementCount == 0);}

   /** Returns the head element in the queue.  You must not call this when the queue is empty! */
   public Object firstElement() {return elementAt(0);}

   /** Returns the tail element in the queue.  You must not call this when the queue is empty! */
   public Object lastElement() {return elementAt(_elementCount-1);}

   /** Makes sure there is enough space preallocated to hold at least
    *  (numElements) elements.  You only need to call this if
    *  you wish to minimize the number of array reallocations done.
    *  @param numElements the minimum amount of elements to pre-allocate space for in the Queue.
    */
   public void ensureSize(int numElements)
   {
      if ((_queue == null)||(_queue.length < numElements))
      {
         Object newQueue[] = new Object[(numElements < 5) ? 5 : numElements*2];    
         if (_elementCount > 0)
         {
            for (int i=0; i<_elementCount; i++) newQueue[i] = elementAt(i);
            _headIndex = 0;
            _tailIndex = _elementCount-1;
         }
         _queue = newQueue;
      }
   }

   /** Returns the last index of the given (element), or -1 if (element) is
    *  not found in the list.  O(n) search time.
    *  @param element The element to look for.
    *  @return The index of (element), or -1 if no such element is present.
    */
   public int indexOf(Object element)
   {
      if (_queue != null) for (int i=size()-1; i>=0; i--) if (elementAt(i) == element) return i;
      return -1;
   }
   
   private int nextIndex(int idx) {return (idx >= getArraySize()-1) ? 0 : idx+1;}
   private int prevIndex(int idx) {return (idx <= 0) ? getArraySize()-1 : idx-1;}

   // Translates a user-index into an index into the _queue array.
   private int internalizeIndex(int idx) 
   {
      if ((idx < 0)||(idx >= _elementCount)) throw new IndexOutOfBoundsException("bad index " + idx + " (queuelen=" + _elementCount + ")");
      return (_headIndex + idx) % getArraySize();
   }

   private int getArraySize() {return (_queue != null) ? _queue.length : 0;}
   
   private Object _queue[] = null;  // demand-allocated object array
   private int _elementCount = 0;  // number of valid elements in the array
   private int _headIndex = -1;   // index of the first filled slot, or -1 
   private int _tailIndex = -1;   // index of the last filled slot, or -1
}


