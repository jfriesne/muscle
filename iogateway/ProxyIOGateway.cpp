/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/ProxyIOGateway.h"

namespace muscle {

ProxyIOGateway :: ProxyIOGateway(const AbstractMessageIOGatewayRef & slaveGateway)
   : _slaveGateway(slaveGateway)
   , _scratchReceiver(NULL)     // not strictly necessary
   , _scratchReceiverArg(NULL)  // but just to be tidy
{
   _fakeStreamSendIO.SetBuffer(DummyByteBufferRef(_fakeStreamSendBuffer));
   // _fakeStreamReceiveIO's buffer will be set just before it is used
}

void ProxyIOGateway :: HandleIncomingByteBuffer(AbstractGatewayMessageReceiver & receiver, const uint8 * p, uint32 bytesRead, const IPAddressAndPort & fromIAP)
{
   ByteBuffer temp;
   temp.AdoptBuffer(bytesRead, const_cast<uint8 *>(p));
   HandleIncomingByteBuffer(receiver, DummyByteBufferRef(temp), fromIAP);
   (void) temp.ReleaseBuffer();
}

void ProxyIOGateway :: HandleIncomingByteBuffer(AbstractGatewayMessageReceiver & receiver, const ByteBufferRef & buf, const IPAddressAndPort & fromIAP)
{
   if (_slaveGateway())
   {
      DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

      if (GetMaximumPacketSize() > 0)
      {
         // packet-IO implementation
         _fakePacketReceiveIO.SetBuffersToRead(buf, fromIAP);
         _slaveGateway()->SetDataIO(DummyDataIORef(_fakePacketReceiveIO));
         _scratchReceiver    = &receiver;
         _scratchReceiverArg = (void *) &fromIAP;
         (void) _slaveGateway()->DoInput(*this, buf()->GetNumBytes());
         _fakePacketReceiveIO.ClearBuffersToRead();
      }
      else
      {
         // stream-IO implementation
         _fakeStreamReceiveIO.SetBuffer(buf); (void) _fakeStreamReceiveIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
         _slaveGateway()->SetDataIO(DummyDataIORef(_fakeStreamReceiveIO));

         uint32 slaveBytesRead = 0;
         while(slaveBytesRead < buf()->GetNumBytes())
         {
            _scratchReceiver    = &receiver;
            _scratchReceiverArg = (void *) &fromIAP;
            const io_status_t nextBytesRead = _slaveGateway()->DoInput(*this, buf()->GetNumBytes()-slaveBytesRead);
            if (nextBytesRead.GetByteCount() > 0) slaveBytesRead += nextBytesRead.GetByteCount();
                                             else break;
         }
         _fakeStreamReceiveIO.SetBuffer(ByteBufferRef());
      }

      _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
   }
   else
   {
      MessageRef inMsg = GetMessageFromPool();
      if ((inMsg())&&(inMsg()->UnflattenFromByteBuffer(buf).IsOK())) CallMessageReceivedFromGateway(receiver, inMsg, (void *) &fromIAP);
   }
}

status_t ProxyIOGateway :: GenerateOutgoingByteBuffers(Queue<ByteBufferRef> & outQ)
{
   MessageRef msg;
   if (PopNextOutgoingMessage(msg).IsError()) return B_NO_ERROR;  // yes, B_NO_ERROR is intentional -- having no outgoing Messages on-hand to process is not an error

   status_t ret;
   if (_slaveGateway())
   {
      // Get the slave gateway to generate its output into our ByteBuffer
      MRETURN_ON_ERROR(_slaveGateway()->AddOutgoingMessage(msg));

      DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state
      ret = GenerateOutgoingByteBuffersAux(outQ);     // can't return here as that would skip the gateway-restore on the next line
      _slaveGateway()->SetDataIO(oldIO);              // restore slave gateway's old state
   }
   else
   {
      // Default (slave-gateway-free) algorithm:  Just directly Flatten() the Message into a buffer and enqueue that
      const uint32 msgFlatSize = msg()->FlattenedSize();
      MRETURN_ON_ERROR(_fakeStreamSendBuffer.SetNumBytes(msgFlatSize, false));

      msg()->FlattenToBytes(_fakeStreamSendBuffer.GetBuffer(), msgFlatSize);
      MRETURN_ON_ERROR(outQ.AddTail(DummyByteBufferRef(_fakeStreamSendBuffer)));
   }

   return ret;
}

status_t ProxyIOGateway :: CallDoOutputOnSlaveGateway()
{
   while(1)
   {
      const io_status_t r = _slaveGateway()->DoOutput();
      if (r.GetByteCount() <= 0) return r.GetStatus();  // byteCount==0 means "no more data to process"; byteCount<0 means "an error occurred"
   }
}

status_t ProxyIOGateway :: GenerateOutgoingByteBuffersAux(Queue<ByteBufferRef> & outQ)
{
   if (GetMaximumPacketSize() > 0)
   {
      _slaveGateway()->SetDataIO(DummyDataIORef(_fakePacketSendIO));
      MRETURN_ON_ERROR(CallDoOutputOnSlaveGateway());

      status_t ret;
      Hashtable<ByteBufferRef, IPAddressAndPort> & b = _fakePacketSendIO.GetWrittenBuffers();
      for (ConstHashtableIterator<ByteBufferRef, IPAddressAndPort> iter(b); ((ret.IsOK())&&(iter.HasData())); iter++) ret = outQ.AddTail(iter.GetKey());
      b.Clear();
      return ret;
   }
   else
   {
      MRETURN_ON_ERROR(_fakeStreamSendIO.Seek(0, SeekableDataIO::IO_SEEK_SET));
      MRETURN_ON_ERROR(_fakeStreamSendBuffer.SetNumBytes(0, false));
      _slaveGateway()->SetDataIO(DummyDataIORef(_fakeStreamSendIO));
      MRETURN_ON_ERROR(CallDoOutputOnSlaveGateway());
      MRETURN_ON_ERROR(outQ.AddTail(DummyByteBufferRef(_fakeStreamSendBuffer)));
      return B_NO_ERROR;
   }

}

} // end namespace muscle
