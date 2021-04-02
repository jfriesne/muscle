#include "dataio/ByteBufferPacketDataIO.h"

namespace muscle {

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(uint32 maxPacketSize)
   : _maxPacketSize(maxPacketSize)
{
   // empty
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const ByteBufferRef & buf, const IPAddressAndPort & fromIAP, uint32 maxPacketSize)
   : _maxPacketSize(maxPacketSize)
{
   SetBuffersToRead(buf, fromIAP);
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const Queue<ByteBufferRef> & bufs, uint32 maxPacketSize)
   : _maxPacketSize(maxPacketSize)
{
   SetBuffersToRead(bufs);
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const Hashtable<ByteBufferRef, IPAddressAndPort> & bufs, uint32 maxPacketSize)
   : _maxPacketSize(maxPacketSize)
{
   SetBuffersToRead(bufs);
}

ByteBufferPacketDataIO ::~ByteBufferPacketDataIO()
{
   // empty
}

void ByteBufferPacketDataIO :: SetBuffersToRead(const Queue<ByteBufferRef> & bufs)
{
   _bufsToRead.Clear();
   (void) _bufsToRead.EnsureSize(bufs.GetNumItems());
   for (uint32 i=0; i<bufs.GetNumItems(); i++) _bufsToRead.Put(bufs[i], IPAddressAndPort());
}

int32 ByteBufferPacketDataIO :: ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource)
{
   if (_bufsToRead.IsEmpty()) return -1;

   retPacketSource = *_bufsToRead.GetFirstValue();

   const ByteBufferRef & bb = *_bufsToRead.GetFirstKey();
   if (bb() == NULL) return 0;

   const int32 ret = muscleMin(bb()->GetNumBytes(), size);
   memcpy(buffer, bb()->GetBuffer(), ret);
   return ret;
}

int32 ByteBufferPacketDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
{
   ByteBufferRef buf = GetByteBufferFromPool(size, (const uint8 *) buffer);
   return ((buf())&&(_writtenBufs.Put(buf, packetDest).IsOK())) ? (int32)size : -1;
}

} // end namespace muscle
