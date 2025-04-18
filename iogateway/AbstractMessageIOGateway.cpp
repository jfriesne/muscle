/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/AbstractMessageIOGateway.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

namespace muscle {

AbstractMessageIOGateway :: AbstractMessageIOGateway()
   : _packetDataIO(NULL)
   , _mtuSize(0)
   , _flushOnEmpty(true)
   , _packetRemoteLocationTaggingEnabled(true)
{
   // empty
}

AbstractMessageIOGateway :: ~AbstractMessageIOGateway()
{
   // empty
}

void
AbstractMessageIOGateway ::
SetFlushOnEmpty(bool f)
{
   _flushOnEmpty = f;
}

bool
AbstractMessageIOGateway ::
IsReadyForInput() const
{
   return true;
}

uint64
AbstractMessageIOGateway ::
GetOutputStallLimit() const
{
   return _ioRef() ? _ioRef()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;
}

void
AbstractMessageIOGateway ::
Shutdown()
{
   if (_ioRef()) _ioRef()->Shutdown();
}

void
AbstractMessageIOGateway ::
Reset()
{
   _outgoingMessages.Clear();
   _unrecoverableErrorStatus = B_NO_ERROR;
}

void
AbstractMessageIOGateway ::
FlushOutput()
{
   DataIO * dio = GetDataIO()();
   if (dio) dio->FlushOutput();
}

/** Used to funnel callbacks from DoInput() back through the AbstractMessageIOGateway's own API, so that subclasses can insert their logic as necessary */
class ScratchProxyReceiver : public AbstractGatewayMessageReceiver
{
public:
   ScratchProxyReceiver(AbstractMessageIOGateway * gw, AbstractGatewayMessageReceiver * r)
      : _gw(gw)
      , _r(r)
   {
      // empty
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData) {_gw->SynchronousMessageReceivedFromGateway(msg, userData, *_r);}
   virtual void AfterMessageReceivedFromGateway(const MessageRef & msg, void * userData) {_gw->SynchronousAfterMessageReceivedFromGateway(msg, userData, *_r);}
   virtual void BeginMessageReceivedFromGatewayBatch() {_gw->SynchronousBeginMessageReceivedFromGatewayBatch(*_r);}
   virtual void EndMessageReceivedFromGatewayBatch() {_gw->SynchronousEndMessageReceivedFromGatewayBatch(*_r);}

private:
   AbstractMessageIOGateway * _gw;
   AbstractGatewayMessageReceiver * _r;
};

status_t
AbstractMessageIOGateway ::
ExecuteSynchronousMessaging(AbstractGatewayMessageReceiver * optReceiver, uint64 timeoutPeriod)
{
   const int readFD  = GetDataIO()() ? GetDataIO()()->GetReadSelectSocket().GetFileDescriptor()  : -1;
   const int writeFD = GetDataIO()() ? GetDataIO()()->GetWriteSelectSocket().GetFileDescriptor() : -1;
   if ((readFD < 0)||(writeFD < 0)) return B_BAD_OBJECT;  // no socket to transmit or receive on!

   ScratchProxyReceiver scratchReceiver(this, optReceiver);
   const uint64 endTime = (timeoutPeriod == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+timeoutPeriod);
   SocketMultiplexer multiplexer;
   while(IsStillAwaitingSynchronousMessagingReply())
   {
      if (GetRunTime64() >= endTime) return B_TIMED_OUT;
      if (optReceiver)        MRETURN_ON_ERROR(multiplexer.RegisterSocketForReadReady(readFD));
      if (HasBytesToOutput()) MRETURN_ON_ERROR(multiplexer.RegisterSocketForWriteReady(writeFD));

      MRETURN_ON_ERROR(multiplexer.WaitForEvents(endTime));
      if (multiplexer.IsSocketReadyForWrite(writeFD)) MRETURN_ON_ERROR(DoOutput().GetStatus());
      if (multiplexer.IsSocketReadyForRead(readFD))   MRETURN_ON_ERROR(DoInput(scratchReceiver).GetStatus());
   }
   return B_NO_ERROR;
}

void AbstractMessageIOGateway :: SetDataIO(const DataIORef & ref)
{
   _ioRef = ref;

   _packetDataIO = dynamic_cast<PacketDataIO *>(_ioRef());
   _mtuSize = _packetDataIO ? _packetDataIO->GetMaximumPacketSize() : 0;
}

io_status_t AbstractMessageIOGateway :: DoOutput(uint32 maxBytes)
{
   const io_status_t ret = DoOutputImplementation(maxBytes);
   if ((ret.GetByteCount() > 0)&&(_flushOnEmpty)&&(HasBytesToOutput() == false)) FlushOutput();
   return ret;
}

status_t AbstractMessageIOGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   const status_t ret = _unrecoverableErrorStatus.IsError() ? B_BAD_OBJECT : _outgoingMessages.AddTail(messageRef);
#if defined(__EMSCRIPTEN__)
   // A cheap hack to keep Emscripten responsive, because otherwise
   // there's no easy way to trigger the ServerEventLoop to be executed
   // again later on to flush our outgoing-message-queue.
   while(DoOutput().GetByteCount() > 0) {/* empty */}
#endif
   return ret;
}

} // end namespace muscle
