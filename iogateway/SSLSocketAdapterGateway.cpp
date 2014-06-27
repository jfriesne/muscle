/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

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

int32 SSLSocketAdapterGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if ((GetSSLState() & SSLSocketDataIO::SSL_STATE_READ_WANTS_WRITEABLE_SOCKET) != 0)
   {
      if ((_slaveGateway()==NULL)||(_slaveGateway()->DoInput(_sslMessages) < 0)) return -1;
      if (_sslMessages.HasItems()) SetSSLForceReadReady(true);  // to make sure that our DoInput() method gets called ASAP
   }
   return _slaveGateway() ? _slaveGateway()->DoOutput(maxBytes) : -1;
}

int32 SSLSocketAdapterGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   if (_sslMessages.HasItems())
   {
      SetSSLForceReadReady(false);
      MessageRef msg; while(_sslMessages.RemoveHead(msg) == B_NO_ERROR) receiver.CallMessageReceivedFromGateway(msg);
   }

   if (((GetSSLState() & SSLSocketDataIO::SSL_STATE_WRITE_WANTS_READABLE_SOCKET) != 0)&&((_slaveGateway()==NULL)||(_slaveGateway()->DoOutput() < 0))) return -1;
   return _slaveGateway() ? _slaveGateway()->DoInput(receiver, maxBytes) : -1;
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

};  // end namespace muscle
