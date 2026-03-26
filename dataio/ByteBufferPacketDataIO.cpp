#include "dataio/ByteBufferPacketDataIO.h"

namespace muscle {

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(uint32 maxPacketSize, bool okayToReturnEndOfStream)
   : _maxPacketSize(maxPacketSize)
   , _okayToReturnEndOfStream(okayToReturnEndOfStream)
{
   // empty
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const ByteBufferRef & buf, const IPAddressAndPort & fromIAP, uint32 maxPacketSize, bool okayToReturnEndOfStream)
   : _maxPacketSize(maxPacketSize)
   , _okayToReturnEndOfStream(okayToReturnEndOfStream)
{
   SetBuffersToRead(buf, fromIAP);
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const Queue<ByteBufferRef> & bufs, const IPAddressAndPort & fromIAP, uint32 maxPacketSize, bool okayToReturnEndOfStream)
   : _maxPacketSize(maxPacketSize)
   , _okayToReturnEndOfStream(okayToReturnEndOfStream)
{
   SetBuffersToRead(bufs, fromIAP);
}

ByteBufferPacketDataIO :: ByteBufferPacketDataIO(const Hashtable<ByteBufferRef, IPAddressAndPort> & bufs, uint32 maxPacketSize, bool okayToReturnEndOfStream)
   : _maxPacketSize(maxPacketSize)
   , _okayToReturnEndOfStream(okayToReturnEndOfStream)
{
   SetBuffersToRead(bufs);
}

ByteBufferPacketDataIO ::~ByteBufferPacketDataIO()
{
   // empty
}

void ByteBufferPacketDataIO :: SetBuffersToRead(const Queue<ByteBufferRef> & bufs, const IPAddressAndPort & fromIAP)
{
   _bufsToRead.Clear();
   (void) _bufsToRead.EnsureSize(bufs.GetNumItems());
   for (uint32 i=0; i<bufs.GetNumItems(); i++) (void) _bufsToRead.Put(bufs[i], fromIAP);
}

io_status_t ByteBufferPacketDataIO :: ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource)
{
   if (_bufsToRead.IsEmpty()) return _okayToReturnEndOfStream ? B_END_OF_STREAM : io_status_t();

   retPacketSource = *_bufsToRead.GetFirstValue();

   const ByteBufferRef & bb = *_bufsToRead.GetFirstKey();
   if (bb())
   {
      const uint32 ret = muscleMin(bb()->GetNumBytes(), size);
      memcpy(buffer, bb()->GetBuffer(), ret);
      (void) _bufsToRead.RemoveFirst();
      return io_status_t(ret);
   }
   else
   {
      (void) _bufsToRead.RemoveFirst();
      return io_status_t();
   }
}

io_status_t ByteBufferPacketDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
{
   ByteBufferRef buf = GetByteBufferFromPool(size, (const uint8 *) buffer);
   MRETURN_ON_ERROR(buf);
   MRETURN_ON_ERROR(_writtenBufs.Put(buf, packetDest));
   return io_status_t((int32)size);
}

} // end namespace muscle
