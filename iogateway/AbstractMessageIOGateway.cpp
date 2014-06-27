/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "iogateway/AbstractMessageIOGateway.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

namespace muscle {

AbstractMessageIOGateway :: AbstractMessageIOGateway() : _hosed(false), _flushOnEmpty(true)
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
   _hosed = false;
}

/** Used to funnel callbacks from DoInput() back through the AbstractMessageIOGateway's own API, so that subclasses can insert their logic as necessary */
class ScratchProxyReceiver : public AbstractGatewayMessageReceiver
{
public:
   ScratchProxyReceiver(AbstractMessageIOGateway * gw, AbstractGatewayMessageReceiver * r) : _gw(gw), _r(r) {/* empty */}

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
   int readFD  = GetDataIO()() ? GetDataIO()()->GetReadSelectSocket().GetFileDescriptor()  : -1;
   int writeFD = GetDataIO()() ? GetDataIO()()->GetWriteSelectSocket().GetFileDescriptor() : -1;
   if ((readFD < 0)||(writeFD < 0)) return B_ERROR;  // no socket to transmit or receive on!

   ScratchProxyReceiver scratchReceiver(this, optReceiver);
   uint64 endTime = (timeoutPeriod == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+timeoutPeriod);
   SocketMultiplexer multiplexer;
   while(IsStillAwaitingSynchronousMessagingReply())
   {
      if (GetRunTime64() >= endTime) return B_ERROR;
      if (optReceiver)        multiplexer.RegisterSocketForReadReady(readFD);
      if (HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(writeFD);
      if ((multiplexer.WaitForEvents(endTime) < 0)                        ||
         ((multiplexer.IsSocketReadyForWrite(writeFD))&&(DoOutput() < 0)) ||
         ((multiplexer.IsSocketReadyForRead(readFD))&&(DoInput(scratchReceiver) < 0))) return IsStillAwaitingSynchronousMessagingReply() ? B_ERROR : B_NO_ERROR;
   }
   return B_NO_ERROR;
}

}; // end namespace muscle
