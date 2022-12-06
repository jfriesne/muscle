/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/ProxyIOGateway.h"

namespace muscle {

ProxyIOGateway :: ProxyIOGateway(const AbstractMessageIOGatewayRef & slaveGateway)
   : _slaveGateway(slaveGateway)
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
      if ((inMsg())&&(inMsg()->UnflattenFromByteBuffer(buf).IsOK())) receiver.CallMessageReceivedFromGateway(inMsg, (void *) &fromIAP);
   }
}

void ProxyIOGateway :: GenerateOutgoingByteBuffers(Queue<ByteBufferRef> & outQ)
{
   MessageRef msg;
   if (GetOutgoingMessageQueue().RemoveHead(msg).IsError()) return;

   if (_slaveGateway())
   {
      // Get the slave gateway to generate its output into our ByteBuffer
      _slaveGateway()->AddOutgoingMessage(msg);

      DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

      if (GetMaximumPacketSize() > 0)
      {
         _slaveGateway()->SetDataIO(DummyDataIORef(_fakePacketSendIO));
         while(_slaveGateway()->DoOutput().GetByteCount() > 0) {/* empty */}

         Hashtable<ByteBufferRef, IPAddressAndPort> & b = _fakePacketSendIO.GetWrittenBuffers();
         if (b.HasItems())
         {
            for (HashtableIterator<ByteBufferRef, IPAddressAndPort> iter(b); iter.HasData(); iter++) (void) outQ.AddTail(iter.GetKey());
            b.Clear();
         }
      }
      else
      {
         _fakeStreamSendIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
         _fakeStreamSendBuffer.SetNumBytes(0, false);
         _slaveGateway()->SetDataIO(DummyDataIORef(_fakeStreamSendIO));
         while(_slaveGateway()->DoOutput().GetByteCount() > 0) {/* empty */}
         (void) outQ.AddTail(DummyByteBufferRef(_fakeStreamSendBuffer));
      }

      _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
   }
   else
   {
      const uint32 msgFlatSize = msg()->FlattenedSize();
      if (_fakeStreamSendBuffer.SetNumBytes(msgFlatSize, false).IsOK())
      {
         // Default algorithm:  Just Flatten() the Message directly into a buffer
         msg()->FlattenToBytes(_fakeStreamSendBuffer.GetBuffer(), msgFlatSize);
         (void) outQ.AddTail(DummyByteBufferRef(_fakeStreamSendBuffer));
      }
   }
}

} // end namespace muscle
