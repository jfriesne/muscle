/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */ 

#include "util/MemoryAllocator.h"

namespace muscle {

status_t ProxyMemoryAllocator :: AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes)
{
   return _slaveRef() ? _slaveRef()->AboutToAllocate(currentlyAllocatedBytes, allocRequestBytes) : B_NO_ERROR;
}

void ProxyMemoryAllocator :: AboutToFree(size_t currentlyAllocatedBytes, size_t allocRequestBytes)
{
   if (_slaveRef()) _slaveRef()->AboutToFree(currentlyAllocatedBytes, allocRequestBytes);
}

void ProxyMemoryAllocator :: AllocationFailed(size_t currentlyAllocatedBytes, size_t allocRequestBytes)
{
   if (_slaveRef()) _slaveRef()->AllocationFailed(currentlyAllocatedBytes, allocRequestBytes);
}

void ProxyMemoryAllocator :: SetAllocationHasFailed(bool hasFailed)
{
   MemoryAllocator::SetAllocationHasFailed(hasFailed);
   if (_slaveRef()) _slaveRef()->SetAllocationHasFailed(hasFailed);
}

size_t ProxyMemoryAllocator :: GetMaxNumBytes() const
{
   return (_slaveRef()) ? _slaveRef()->GetMaxNumBytes() : MUSCLE_NO_LIMIT;
}

size_t ProxyMemoryAllocator :: GetNumAvailableBytes(size_t currentlyAllocated) const
{
   return (_slaveRef()) ? _slaveRef()->GetNumAvailableBytes(currentlyAllocated) : ((size_t)-1);
}

UsageLimitProxyMemoryAllocator :: UsageLimitProxyMemoryAllocator(const MemoryAllocatorRef & slaveRef, size_t maxBytes) : ProxyMemoryAllocator(slaveRef), _maxBytes(maxBytes)
{
   // empty
}
 
UsageLimitProxyMemoryAllocator :: ~UsageLimitProxyMemoryAllocator()
{
   // empty
}
 
status_t UsageLimitProxyMemoryAllocator :: AboutToAllocate(size_t currentlyAllocatedBytes, size_t allocRequestBytes)
{
   return ((allocRequestBytes <= _maxBytes)&&(currentlyAllocatedBytes + allocRequestBytes <= _maxBytes)) ? ProxyMemoryAllocator::AboutToAllocate(currentlyAllocatedBytes, allocRequestBytes) : B_ERROR;
}

void AutoCleanupProxyMemoryAllocator :: AllocationFailed(size_t currentlyAllocatedBytes, size_t allocRequestBytes)
{
   ProxyMemoryAllocator::AllocationFailed(currentlyAllocatedBytes, allocRequestBytes);
   uint32 nc = _callbacks.GetNumItems();
   for (uint32 i=0; i<nc; i++) if (_callbacks[i]()) (void) (_callbacks[i]())->Callback(NULL);
}

}; // end namespace muscle
