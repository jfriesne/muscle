/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/ProxyIOGateway.h"

namespace muscle {

ProxyIOGateway :: ProxyIOGateway(const AbstractMessageIOGatewayRef & slaveGateway)
   : _slaveGateway(slaveGateway)
{
   _fakeStreamSendIO.SetBuffer(ByteBufferRef(&_fakeSendBuffer, false));
   // _fakeStreamReceiveIO's buffer will be set just before it is used
}

void ProxyIOGateway :: HandleIncomingByteBuffer(AbstractGatewayMessageReceiver & receiver, const uint8 * p, uint32 bytesRead, const IPAddressAndPort & fromIAP)
{
   ByteBuffer temp;
   temp.AdoptBuffer(bytesRead, const_cast<uint8 *>(p));
   HandleIncomingByteBuffer(receiver, ByteBufferRef(&temp, false), fromIAP);
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
         _slaveGateway()->SetDataIO(DataIORef(&_fakePacketReceiveIO, false));
         _scratchReceiver    = &receiver;
         _scratchReceiverArg = (void *) &fromIAP;
         (void) _slaveGateway()->DoInput(*this, buf()->GetNumBytes());
         _fakePacketReceiveIO.ClearBuffersToRead();
      }
      else
      {
         // stream-IO implementation
         _fakeStreamReceiveIO.SetBuffer(buf); (void) _fakeStreamReceiveIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
         _slaveGateway()->SetDataIO(DataIORef(&_fakeStreamReceiveIO, false));

         uint32 slaveBytesRead = 0;
         while(slaveBytesRead < buf()->GetNumBytes())
         {
            _scratchReceiver    = &receiver;
            _scratchReceiverArg = (void *) &fromIAP;
            const int32 nextBytesRead = _slaveGateway()->DoInput(*this, buf()->GetNumBytes()-slaveBytesRead);
            if (nextBytesRead > 0) slaveBytesRead += nextBytesRead;
                              else break;
         }
         _fakeStreamReceiveIO.SetBuffer(ByteBufferRef());
      }

      _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
   }
   else
   {
      MessageRef inMsg = GetMessageFromPool();
      if ((inMsg())&&(inMsg()->UnflattenFromByteBuffer(*buf()).IsOK())) receiver.CallMessageReceivedFromGateway(inMsg, (void *) &fromIAP);
   }
}

ByteBufferRef ProxyIOGateway :: CreateNextOutgoingByteBuffer()
{
   ByteBufferRef ret;

   MessageRef msg;
   if (GetOutgoingMessageQueue().RemoveHead(msg).IsOK())
   {
      if (_slaveGateway())
      {
         // Get the slave gateway to generate its output into our ByteBuffer
         _slaveGateway()->AddOutgoingMessage(msg);

         DataIORef oldIO = _slaveGateway()->GetDataIO(); // save slave gateway's old state

         if (GetMaximumPacketSize() > 0)
         {
            _slaveGateway()->SetDataIO(DataIORef(&_fakePacketSendIO, false));
            while(_slaveGateway()->DoOutput() > 0) {/* empty */}

            Hashtable<ByteBufferRef, IPAddressAndPort> & b = _fakePacketSendIO.GetWrittenBuffers();
            if (b.HasItems())
            {
               if (b.GetNumItems() > 1) LogTime(MUSCLE_LOG_WARNING, "ProxyIOGateway:  child gateway's DoOutput() produced " UINT32_FORMAT_SPEC " packet-buffers, discarding all but the first one!\n", b.GetNumItems());
               ret = *b.GetFirstKey();
               b.Clear();
            }
         }
         else
         {
            _fakeStreamSendIO.Seek(0, SeekableDataIO::IO_SEEK_SET);
            _fakeSendBuffer.SetNumBytes(0, false);
            _slaveGateway()->SetDataIO(DataIORef(&_fakeStreamSendIO, false));
            while(_slaveGateway()->DoOutput() > 0) {/* empty */}
            ret.SetRef(&_fakeSendBuffer, false);
         }
         _slaveGateway()->SetDataIO(oldIO);  // restore slave gateway's old state
      }
      else if (_fakeSendBuffer.SetNumBytes(msg()->FlattenedSize(), false).IsOK())
      {
         // Default algorithm:  Just flatten the Message into the buffer
         msg()->Flatten(_fakeSendBuffer.GetBuffer());
         ret.SetRef(&_fakeSendBuffer, false);
      }
   }

   return ret;
}

} // end namespace muscle
