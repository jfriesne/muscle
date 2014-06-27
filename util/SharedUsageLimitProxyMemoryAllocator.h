/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef SharedUsageLimitProxyMemoryAllocator_h
#define SharedUsageLimitProxyMemoryAllocator_h

#include "util/MemoryAllocator.h"
#include "system/SharedMemory.h"

namespace muscle {

/** This MemoryAllocator decorates its slave MemoryAllocator to 
  * enforce a user-defined per-process-group limit on how much 
  * memory may be allocated at any given time.  Note that this
  * class depends on the SharedMemory class to operate, so this
  * class will only work on operating systems for which SharedMemory
  * is implemented (read: Unix, currently).
  */
class SharedUsageLimitProxyMemoryAllocator : public ProxyMemoryAllocator
{
public:
   /** Constructor.  
     * @param groupKey A string that is used as a key by all processes in the group.
     *                 Any process that uses the same key will use the same shared memory limit.
     * @param memberID Our member ID in the group.  This number should be between zero and (groupSize-1) inclusive,
     *                 and should be different for each process in the process group.  It is used to keep track
     *                 of how much memory each process in the group is using.  If the memberID is less than zero,
     *                 this allocator will not track memory used by this process, and is then useful only to
     *                 watch memory used by other processes.
     * @param groupSize The maximum number of process that may be in the group.  
     *                  This value should be the same for all process in a given group.
     * @param slaveRef Reference to a sub-MemoryAllocator whose methods we will call through to.
     * @param maxBytes The maximum number of bytes that may be allocated, in aggregate, by all the process
     *                 in the group.  This value should be the same for all process in a given group.
     */
   SharedUsageLimitProxyMemoryAllocator(const char * groupKey, int32 memberID, uint32 groupSize, const MemoryAllocatorRef & slaveRef, size_t maxBytes);

   /** Destructor.  */
   virtual ~SharedUsageLimitProxyMemoryAllocator();

   /** Overridden to return B_ERROR if memory usage would exceed the aggregate maximum due to this allocation. */
   virtual status_t AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes);

   /** Overridden to record this amount of memory being freed */
   virtual void AboutToFree(size_t currentlyAllocatedBytes, size_t allocRequestBytes);

   /** Overridden to return our total memory size as passed in to our ctor. */
   virtual size_t GetMaxNumBytes() const {return _maxBytes;}

   /** Overridden to return the number of bytes actually available to us. */
   virtual size_t GetNumAvailableBytes(size_t currentlyAllocated) const;

   /** Returns our own process's member ID value, as passed in to the constructor */
   int32 GetMemberID() const {return _memberID;}

   /** Returns the number of processes in our process group, as specified in the constructor. */
   uint32 GetGroupSize() const {return _groupSize;}

   /** Returns the current memory used, in bytes, for each member of our group.
    *  @param retCounts on success, the bytes used by each member of our group will
    *                   be written into this array.  This array must have enough space
    *                   for the number of items specified by (groupSize) in our constructor.
    *  @param optRetTotal If non-NULL, then on success the shared memory area's current total-bytes-allocated value
    *                     will be placed here.  This value should always be the sum of the values returned in (retCounts).
    *                     (although it is possible it might not be if the shared memory area becomes inconsistent).
    *                     Defaults to NULL.
    *  @returns B_NO_ERROR on success, or B_ERROR if the information could not be accessed.
    */
   status_t GetCurrentMemoryUsage(size_t * retCounts, size_t * optRetTotal = NULL) const;

   /** Returns true iff our shared memory area setup worked and we are ready for use.
     * Returns false if there was a problem setting up and we aren't usable.
     */
   bool IsValid() const {return (_shared.GetAreaSize() > 0);}

private:
   void ResetDaemonCounter(); // Note: this assumes the SharedMemory is already locked for read/write!
   status_t ChangeDaemonCounter(int32 byteDelta);  // Note: this assumes the SharedMemory is not locked yet
   status_t ChangeDaemonCounterAux(int32 byteDelta);  // Note: this assumes the SharedMemory is not locked yet
   size_t CalculateTotalAllocationSum() const;
   uint32 GetNumSlots() const {return _shared.GetAreaSize()/sizeof(size_t);}

   size_t _localAllocated;
   size_t _maxBytes;
   mutable SharedMemory _shared;
   int32 _memberID;
   uint32 _groupSize;
   int32 _localCachedBytes;
};

}; // end namespace muscle

#endif
