/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleMemoryAllocator_h
#define MuscleMemoryAllocator_h

#include "support/MuscleSupport.h"
#include "util/CountedObject.h"
#include "util/GenericCallback.h"
#include "util/Queue.h"
#include "util/RefCount.h"

namespace muscle {

class MemoryAllocator;

/** Interface class representing an object that can allocate and free blocks of memory. */
class MemoryAllocator : public RefCountable
{
public:
   /** Default constructor; no-op */
   MemoryAllocator() : _hasAllocationFailed(false) {/* empty */}

   /** Virtual destructor, to keep C++ honest */
   virtual ~MemoryAllocator() {/* empty */}

   /** This method is called whenever we are about to allocate some memory.
    *  @param currentlyAllocatedBytes How many bytes the system has allocated currently
    *  @param allocRequestBytes How many bytes the system would like to allocate.
    *  @note Implementations of this method shall assume that calls to this method will
    *        be serialized, so they don't need to do any serialization themselves.
    *  @return Should return B_NO_ERROR if the allocation may proceed, or an error code if the allocation should fail.
    */
   virtual status_t AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes) = 0;

   /** This method is called whenever we are about to free some memory.
    *  @param currentlyAllocatedBytes How many bytes the system has allocated currently
    *  @param freeBytes How many bytes the system is about to free.
    *  @note Implementations of this method shall assume that calls to this method will
    *        be serialized, so they don't need to do any serialization themselves.
    */
   virtual void AboutToFree(size_t currentlyAllocatedBytes, size_t freeBytes) = 0;

   /** Called if an allocation fails (either because AboutToAllocate() returned other than B_NO_ERROR, or
    *  because malloc() returned NULL).   This method does not need to call SetMallocHasFailed(),
    *  because that will be called separately.
    *  @param currentlyAllocatedBytes How many bytes the system has allocated currently
    *  @param allocRequestBytes How many bytes the system wanted to allocate, but couldn't.
    *  @note Implementations of this method shall assume that calls to this method will
    *        be serialized, so they don't need to do any serialization themselves.
    *  @note This method should NOT undo any side effects that were incurred by
    *        a previous successful AboutToAllocate() method, as that will be done
    *        separately by a call to AboutToFree().
    */
   virtual void AllocationFailed(size_t currentlyAllocatedBytes, size_t allocRequestBytes) = 0;

   /** Sets the state of the "allocation has failed" flag.  Typically this is set true just
    *  before a call to AllocationFailed(), and set false again later on, after the flag has
    *  been noticed and dealt with.
    *  @param hasFailed true to set the allocation-has-failed flag; false to clear it
    */
   virtual void SetAllocationHasFailed(bool hasFailed) {_hasAllocationFailed = hasFailed;}

   /** Should be overridden to return the maximum amount of memory that may be allocated at once.
    *  If there is no set limit, this method should return MUSCLE_NO_LIMIT.
    */
   MUSCLE_NODISCARD virtual size_t GetMaxNumBytes() const = 0;

   /** Should be overridden to return the number of bytes still available for allocation,
    *  given that (currentlyAllocated) bytes have already been allocated.
    *  If there is no set limit, this method should return MUSCLE_NO_LIMIT.
    *  @param currentlyAllocated how many bytes are already allocated
    */
   MUSCLE_NODISCARD virtual size_t GetNumAvailableBytes(size_t currentlyAllocated) const = 0;

   /** Returns the current state of the "allocation has failed" flag. */
   MUSCLE_NODISCARD bool HasAllocationFailed() const {return _hasAllocationFailed;}

private:
   bool _hasAllocationFailed;

   DECLARE_COUNTED_OBJECT(MemoryAllocator);
};
DECLARE_REFTYPES(MemoryAllocator);

/** Convenience class, used for easy subclassing:  holds a slave MemoryAllocator and passes all method
  * calls on through to the slave.
  */
class ProxyMemoryAllocator : public MemoryAllocator
{
public:
   /** Constructor.
     * @param slaveRef Reference to another MemoryAllocator that we will pass all our method calls on through to.
     *                 If (slaveRef) is NULL, default behaviour (always allow, no-op failures) will be used.
     */
   ProxyMemoryAllocator(const MemoryAllocatorRef & slaveRef) : _slaveRef(slaveRef) {/* empty */}

   /** Destructor */
   virtual ~ProxyMemoryAllocator() {/* empty */}

   virtual status_t AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes);
   virtual void AboutToFree(size_t currentlyAllocatedBytes, size_t freeBytes);
   virtual void AllocationFailed(size_t currentlyAllocatedBytes, size_t allocRequestBytes);
   virtual void SetAllocationHasFailed(bool hasFailed);
   MUSCLE_NODISCARD virtual size_t GetMaxNumBytes() const;
   MUSCLE_NODISCARD virtual size_t GetNumAvailableBytes(size_t currentlyAllocated) const;

private:
   MemoryAllocatorRef _slaveRef;
};
DECLARE_REFTYPES(ProxyMemoryAllocator);

/** This MemoryAllocator decorates its slave MemoryAllocator to
  * enforce a user-defined per-process limit on how much memory may be allocated at any given time.
  */
class UsageLimitProxyMemoryAllocator : public ProxyMemoryAllocator
{
public:
   /** Constructor.
     * @param slaveRef Reference to a sub-MemoryAllocator whose methods we will call through to.
     * @param maxBytes The maximum number of bytes that we will allow (slave) to allocate.  Defaults to no limit.
     *                 This value may be reset later using SetMaxNumBytes().
     */
   UsageLimitProxyMemoryAllocator(const MemoryAllocatorRef & slaveRef, size_t maxBytes = MUSCLE_NO_LIMIT);

   /** Destructor.  */
   virtual ~UsageLimitProxyMemoryAllocator();

   /** Overridden to return false if memory usage would go over maximum due to this allocation.
    *  @param currentlyAllocatedBytes How many bytes the system has allocated currently
    *  @param allocRequestBytes How many bytes the system would like to allocate.
    */
   virtual status_t AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes);

   /** Set a new maximum number of allocatable bytes
     * @param maxBytes the new allocation limit, in bytes
     */
   void SetMaxNumBytes(size_t maxBytes) {_maxBytes = maxBytes;}

   /** Implemented to return our hard-coded size limit, same as GetMaxNumBytes() */
   MUSCLE_NODISCARD virtual size_t GetMaxNumBytes() const {return muscleMin(_maxBytes, ProxyMemoryAllocator::GetMaxNumBytes());}

   /** Implemented to return the difference between the maximum allocation and the current number allocated.
     * @param currentlyAllocated how many bytes are already allocated
     */
   MUSCLE_NODISCARD virtual size_t GetNumAvailableBytes(size_t currentlyAllocated) const {return muscleMin((_maxBytes>currentlyAllocated)?_maxBytes-currentlyAllocated:0, ProxyMemoryAllocator::GetNumAvailableBytes(currentlyAllocated));}

private:
   size_t _maxBytes;
};
DECLARE_REFTYPES(UsageLimitProxyMemoryAllocator);

/** This MemoryAllocator decorates its slave MemoryAllocator to call a list of
  * GenericCallback objects when the slave's memory allocation fails.  These
  * GenericCallback calls should try to free up some memory if possible;
  * then this MemoryAllocator will call the slave again and see if the memory
  * allocation can now succeed.
  */
class AutoCleanupProxyMemoryAllocator : public ProxyMemoryAllocator
{
public:
   /** Constructor.
     * @param slaveRef Reference to a sub-MemoryAllocator whose methods we will call through to.
     */
   AutoCleanupProxyMemoryAllocator(const MemoryAllocatorRef & slaveRef) : ProxyMemoryAllocator(slaveRef) {/* empty */}

   /** Destructor.  Calls ClearCallbacks(). */
   virtual ~AutoCleanupProxyMemoryAllocator() {/* empty */}

   /** Overridden to call our callbacks in event of a failure.
    *  @param currentlyAllocatedBytes How many bytes the system has allocated currently
    *  @param allocRequestBytes How many bytes the system would like to allocate.
    */
   virtual void AllocationFailed(size_t currentlyAllocatedBytes, size_t allocRequestBytes);

   /** Read-write access to our list of out-of-memory callbacks. */
   MUSCLE_NODISCARD Queue<GenericCallbackRef> & GetCallbacksQueue() {return _callbacks;}

   /** Write-only access to our list of out-of-memory callbacks. */
   MUSCLE_NODISCARD const Queue<GenericCallbackRef> & GetCallbacksQueue() const {return _callbacks;}

private:
   Queue<GenericCallbackRef> _callbacks;
};
DECLARE_REFTYPES(AutoCleanupProxyMemoryAllocator);

} // end namespace muscle

#endif
