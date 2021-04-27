#include "util/SharedUsageLimitProxyMemoryAllocator.h"

namespace muscle {

static const int32 CACHE_BYTES = 100*1024;  // 100KB local cache size seems reasonable, no?

SharedUsageLimitProxyMemoryAllocator :: SharedUsageLimitProxyMemoryAllocator(const char * sharedAreaKey, int32 memberID, uint32 groupSize, const MemoryAllocatorRef & slaveRef, size_t maxBytes)
   : ProxyMemoryAllocator(slaveRef)
   , _localAllocated(0)
   , _maxBytes(maxBytes)
   , _memberID(memberID)
   , _groupSize(groupSize)
   , _localCachedBytes(0)
{
   status_t ret;
   if (_shared.SetArea(sharedAreaKey, groupSize*sizeof(size_t), true).IsOK(ret))
   {
      if (_shared.IsCreatedLocally()) 
      {
         size_t * sa = GetArrayPointer();
         if (sa)
         {
            uint32 numSlots = GetNumSlots();
            for (uint32 i=0; i<numSlots; i++) sa[i] = 0;
         }
      }
      else ResetDaemonCounter();  // Clean up after previous daemon, just in case

      _shared.UnlockArea();  // because it was locked for us by SetArea()
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "SharedUsageLimitProxyMemoryAllocator:  Could not initialize shared memory area [%s]! [%s]\n", sharedAreaKey, ret());
}

SharedUsageLimitProxyMemoryAllocator :: ~SharedUsageLimitProxyMemoryAllocator()
{
   if (_shared.LockAreaReadWrite().IsOK())
   {
      ResetDaemonCounter();
      _shared.UnlockArea();
   }
}

void SharedUsageLimitProxyMemoryAllocator :: ResetDaemonCounter()
{
   const int32 numSlots = _shared.GetAreaSize() / sizeof(size_t);
   size_t * sa = GetArrayPointer();
   if ((sa)&&(_memberID >= 0)&&(_memberID < numSlots)) sa[_memberID] = 0;
   _localCachedBytes = _localAllocated = 0;
}

status_t SharedUsageLimitProxyMemoryAllocator :: ChangeDaemonCounter(int32 byteDelta)
{
   if (byteDelta > 0)
   {
      if ((size_t)byteDelta > _localCachedBytes)
      {
         // Hmm, we don't have enough bytes locally, better ask for some from the shared region
         const int32 wantBytes = ((byteDelta/CACHE_BYTES)+1)*CACHE_BYTES; // round up to nearest multiple
         if (ChangeDaemonCounterAux(wantBytes).IsOK()) _localCachedBytes += wantBytes;
         if ((size_t)byteDelta > _localCachedBytes) return B_ACCESS_DENIED;  // still not enough!?
      }
      _localCachedBytes -= byteDelta;
   }
   else
   {
      _localCachedBytes -= byteDelta;  // actually adds, since byteDelta is negative
      if (_localCachedBytes > 2*CACHE_BYTES)
      {
         int32 diffBytes = (int32)(_localCachedBytes-CACHE_BYTES);  // FogBugz #4569 -- reduce cache to our standard cache size
         if (ChangeDaemonCounterAux(-diffBytes).IsOK()) _localCachedBytes -= diffBytes;
      }
   }
   return B_NO_ERROR;
}

// Locks the shared memory region and adjusts our counter there.  This is a bit expensive, so we try to minimize the number of times we do it.
status_t SharedUsageLimitProxyMemoryAllocator :: ChangeDaemonCounterAux(int32 byteDelta)
{
   if (_memberID < 0) return B_BAD_OBJECT;

   status_t ret ;
   if (_shared.LockAreaReadWrite().IsOK(ret))
   {
      const int32 numSlots = _shared.GetAreaSize() / sizeof(size_t);
      size_t * sa = GetArrayPointer();
      if ((sa)&&(_memberID < numSlots))
      {
         if (byteDelta <= 0)
         {
            const size_t reduceBy = -byteDelta;
            if (reduceBy > _localAllocated)
            {
               // This should never happen, but just in case it does...
               printf("Error, Attempted to reduce slot " INT32_FORMAT_SPEC "'s counter (currently " UINT64_FORMAT_SPEC ") by " UINT64_FORMAT_SPEC "!  Setting counter at zero instead.\n", (int32)_memberID, (uint64)_localAllocated, (uint64)reduceBy);
               _localAllocated = 0; 
            }
            else _localAllocated -= reduceBy;

            ret = B_NO_ERROR;
         }
         else if ((CalculateTotalAllocationSum()+byteDelta) <= _maxBytes)
         {
            _localAllocated += byteDelta;
            ret = B_NO_ERROR;
         }

         sa[_memberID] = _localAllocated;  // write our current allocation out to the shared memory region
      }

      _shared.UnlockArea();
   }
   return ret;
}

size_t SharedUsageLimitProxyMemoryAllocator :: CalculateTotalAllocationSum() const
{
   size_t total = 0;
   const size_t * sa = GetArrayPointer();
   if (sa)
   {
      const uint32 numSlots = GetNumSlots();
      for (uint32 i=0; i<numSlots; i++) total += sa[i];
   }
   return total;
}

status_t SharedUsageLimitProxyMemoryAllocator :: AboutToAllocate(size_t cab, size_t arb)
{
   status_t ret;
   if (ChangeDaemonCounter((int32)arb).IsOK(ret))
   {
      ret = ProxyMemoryAllocator::AboutToAllocate(cab, arb);
      if (ret.IsError()) (void) ChangeDaemonCounter(-((int32)arb)); // roll back!
   }
   return ret;
}

void SharedUsageLimitProxyMemoryAllocator :: AboutToFree(size_t cab, size_t arb)
{
   (void) ChangeDaemonCounter(-((int32)arb));
   ProxyMemoryAllocator::AboutToFree(cab, arb);
}

size_t SharedUsageLimitProxyMemoryAllocator :: GetNumAvailableBytes(size_t allocated) const
{
   size_t totalUsed = ((size_t)-1);
   if (_shared.LockAreaReadOnly().IsOK())
   {
      totalUsed = CalculateTotalAllocationSum();
      _shared.UnlockArea();
   }
   return muscleMin((_maxBytes>totalUsed)?(_maxBytes-totalUsed):0, ProxyMemoryAllocator::GetNumAvailableBytes(allocated));
}

status_t SharedUsageLimitProxyMemoryAllocator :: GetCurrentMemoryUsage(size_t * retCounts, size_t * optRetTotal) const
{
   status_t ret;
   if (_shared.LockAreaReadOnly().IsOK(ret))
   {
      const size_t * sa = GetArrayPointer();
      if ((sa)&&(retCounts)) for (int32 i=GetNumSlots()-1; i>=0; i--) retCounts[i] = sa[i];
      if (optRetTotal) *optRetTotal = CalculateTotalAllocationSum();
      _shared.UnlockArea();
      return B_NO_ERROR;
   }
   else return ret;
}

} // end namespace muscle
