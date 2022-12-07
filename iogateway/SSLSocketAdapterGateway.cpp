/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/SSLSocketDataIO.h"
#include "iogateway/SSLSocketAdapterGateway.h"

namespace muscle {

SSLSocketAdapterGateway :: SSLSocketAdapterGateway(const AbstractMessageIOGatewayRef & slaveGateway)
{
   SetSlaveGateway(slaveGateway);
}

SSLSocketAdapterGateway :: ~SSLSocketAdapterGateway()
{
   // empty
}

void SSLSocketAdapterGateway :: SetDataIO(const DataIORef & ref)
{
   AbstractMessageIOGateway::SetDataIO(ref);
   if (_slaveGateway()) _slaveGateway()->SetDataIO(ref);
}

void SSLSocketAdapterGateway :: SetSlaveGateway(const AbstractMessageIOGatewayRef & slaveGateway)
{
   if (_slaveGateway()) _slaveGateway()->SetDataIO(DataIORef());
   _slaveGateway = slaveGateway;
   if (_slaveGateway()) _slaveGateway()->SetDataIO(GetDataIO());
}

status_t SSLSocketAdapterGateway :: AddOutgoingMessage(const MessageRef & messageRef)
{
   return _slaveGateway()->AddOutgoingMessage(messageRef);
}

bool SSLSocketAdapterGateway :: IsReadyForInput() const
{
   return ((_sslMessages.HasItems()) || ((GetSSLState() & (SSLSocketDataIO::SSL_STATE_READ_WANTS_READABLE_SOCKET | SSLSocketDataIO::SSL_STATE_WRITE_WANTS_READABLE_SOCKET)) != 0) || ((_slaveGateway())&&(_slaveGateway()->IsReadyForInput())));
}

bool SSLSocketAdapterGateway :: HasBytesToOutput() const
{
   return (((GetSSLState() & (SSLSocketDataIO::SSL_STATE_READ_WANTS_WRITEABLE_SOCKET | SSLSocketDataIO::SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET)) != 0) || ((_slaveGateway())&&(_slaveGateway()->HasBytesToOutput())));
}

uint64 SSLSocketAdapterGateway :: GetOutputStallLimit() const
{
   return _slaveGateway() ? _slaveGateway()->GetOutputStallLimit() : MUSCLE_TIME_NEVER;
}

void SSLSocketAdapterGateway :: Shutdown()
{
   if (_slaveGateway()) _slaveGateway()->Shutdown();
}

void SSLSocketAdapterGateway :: Reset()
{
   if (_slaveGateway()) _slaveGateway()->Reset();
}

io_status_t SSLSocketAdapterGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if ((GetSSLState() & SSLSocketDataIO::SSL_STATE_READ_WANTS_WRITEABLE_SOCKET) != 0)
   {
      if (_slaveGateway() == NULL) return B_BAD_OBJECT;
      MRETURN_ON_ERROR(_slaveGateway()->DoInput(_sslMessages));
      if (_sslMessages.HasItems()) SetSSLForceReadReady(true);  // to make sure that our DoInput() method gets called ASAP
   }
   return _slaveGateway() ? _slaveGateway()->DoOutput(maxBytes) : io_status_t(B_BAD_OBJECT);
}

io_status_t SSLSocketAdapterGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   if (_sslMessages.HasItems())
   {
      SetSSLForceReadReady(false);
      MessageRef msg; while(_sslMessages.RemoveHead(msg).IsOK()) receiver.CallMessageReceivedFromGateway(msg);
   }

   status_t ret;
   if (((GetSSLState() & SSLSocketDataIO::SSL_STATE_WRITE_WANTS_READABLE_SOCKET) != 0)&&((_slaveGateway()==NULL)||(_slaveGateway()->DoOutput().IsError(ret)))) return ret | B_BAD_OBJECT;
   return _slaveGateway() ? _slaveGateway()->DoInput(receiver, maxBytes) : io_status_t(B_BAD_OBJECT);
}

uint32 SSLSocketAdapterGateway :: GetSSLState() const
{
   const SSLSocketDataIO * dio = dynamic_cast<SSLSocketDataIO *>(GetDataIO()());
   return dio ? dio->_sslState : 0;
}

void SSLSocketAdapterGateway :: SetSSLForceReadReady(bool forceReadReady)
{
   SSLSocketDataIO * dio = dynamic_cast<SSLSocketDataIO *>(GetDataIO()());
   if (dio) dio->_forceReadReady = forceReadReady;
}

}  // end namespace muscle
