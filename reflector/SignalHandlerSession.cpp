/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "reflector/SignalHandlerSession.h"
#include "system/SignalMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

SignalHandlerSession :: SignalHandlerSession() : _numValidRecvBytes(0)
{
#ifdef MUSCLE_AVOID_CPLUSPLUS11
   assert(sizeof(_recvBuf)==SignalEventInfo::FlattenedSize());
#endif
}

ConstSocketRef SignalHandlerSession :: CreateDefaultSocket()
{
   ConstSocketRef sock;
   (void) CreateConnectedSocketPair(sock, _handlerSocket);
   return sock;
}

io_status_t SignalHandlerSession :: DoInput(AbstractGatewayMessageReceiver &, uint32)
{
   io_status_t totalByteCount;
   while(1)
   {
      const io_status_t bytesReceived = ReceiveData(GetSessionReadSelectSocket(), _recvBuf+_numValidRecvBytes, sizeof(_recvBuf)-_numValidRecvBytes, false);
      MTALLY_BYTES_OR_RETURN_ON_ERROR_OR_BREAK(totalByteCount, bytesReceived);

      _numValidRecvBytes += bytesReceived.GetByteCount();
      if (_numValidRecvBytes >= sizeof(_recvBuf))
      {
         _numValidRecvBytes = 0;

         SignalEventInfo sei;
         DataUnflattener unflat(_recvBuf, sizeof(_recvBuf));
         if (sei.Unflatten(unflat).IsOK()) SignalReceived(sei);
      }
   }
   return totalByteCount;
}

status_t SignalHandlerSession :: AttachedToServer()
{
   MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());
   return SignalMultiplexer::GetSignalMultiplexer().AddHandler(this);
}

void SignalHandlerSession :: AboutToDetachFromServer()
{
   SignalMultiplexer::GetSignalMultiplexer().RemoveHandler(this);
   AbstractReflectSession::AboutToDetachFromServer();
}

void SignalHandlerSession :: SignalReceived(const SignalEventInfo & sei)
{
   LogTime(MUSCLE_LOG_CRITICALERROR, "Signal #%i received from process #" UINT64_FORMAT_SPEC ", ending event loop!\n", sei.GetSignalNumber(), (uint64) sei.GetFromProcessID());
   EndServer();
}

static bool _wasSignalCaught = false;
bool WasSignalCaught() {return _wasSignalCaught;}

void SignalHandlerSession :: SignalHandlerFunc(const SignalEventInfo & sei)
{
   // Note that this method is called within the context of the POSIX/Win32 signal handler and thus we have to
   // be very careful about what we do here!   Sending a few bytes on a socket should be okay though.  (there is
   // the worry that the SignalHandlerSession object might have been deleted by the time we get here, but I don't
   // think there is much I can do about that)
   if (IsAttachedToServer())
   {
      int nextSigNum;
      for (uint32 i=0; GetNthSignalNumber(i, nextSigNum).IsOK(); i++)
      {
         if (sei.GetSignalNumber() == nextSigNum)
         {
            _wasSignalCaught = true;

#ifdef MUSCLE_AVOID_CPLUSPLUS11
            uint8 buf[sizeof(int32)+sizeof(uint64)];  // ugly hack because C++03 doesn't know about constexpr methods
            assert(sizeof(buf)==SignalEventInfo::FlattenedSize());
#else
            uint8 buf[SignalEventInfo::FlattenedSize()];
#endif
            sei.Flatten(DataFlattener(buf, sizeof(buf)));
            (void) SendData(_handlerSocket, buf, sizeof(buf), false);  // send the signal value to the main thread so it can handle it later
            break;
         }
      }
   }
}

} // end namespace muscle
