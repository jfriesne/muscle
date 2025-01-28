/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <QCoreApplication>

#include "platform/qt/QSignalHandler.h"

namespace muscle {

QSignalHandler :: QSignalHandler(QObject * parent, const char * name)
   : QObject(parent)
   , _socketNotifier(NULL)
   , _numValidRecvBytes(0)
{
#ifdef MUSCLE_AVOID_CPLUSPLUS11
   assert(sizeof(_recvBuf)==SignalEventInfo::FlattenedSize(), "C++03 compatibility hack is using the wrong buffer size");
#endif

   if (name) setObjectName(name);

   status_t ret;
   if ((CreateConnectedSocketPair(_mainThreadSocket, _handlerFuncSocket).IsOK(ret))&&(SignalMultiplexer::GetSignalMultiplexer().AddHandler(this).IsOK(ret)))
   {
      _socketNotifier = new QSocketNotifier(_mainThreadSocket.GetFileDescriptor(), QSocketNotifier::Read, this);
      connect(_socketNotifier, SIGNAL(activated(int)), this, SLOT(SocketDataReady()));
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "QSignalHandler %p could not register with the SignalMultiplexer! [%s]\n", this, ret());
}

QSignalHandler :: ~QSignalHandler()
{
   if (_socketNotifier) _socketNotifier->setEnabled(false);  // prevent occasional CPU-spins under Lion
   SignalMultiplexer::GetSignalMultiplexer().RemoveHandler(this);
}

void QSignalHandler :: SocketDataReady()
{
   while(1)
   {
      const int32 bytesReceived = ReceiveData(_mainThreadSocket, _recvBuf+_numValidRecvBytes, sizeof(_recvBuf)-_numValidRecvBytes, false).GetByteCount();
      if (bytesReceived <= 0) break;

      _numValidRecvBytes += bytesReceived;
      if (_numValidRecvBytes >= sizeof(_recvBuf))
      {
         _numValidRecvBytes = 0;

         SignalEventInfo sei;
         DataUnflattener unflat(_recvBuf, sizeof(_recvBuf));
         if (sei.Unflatten(unflat).IsOK()) emit SignalReceived(sei);
      }
   }
}

void QSignalHandler :: SignalHandlerFunc(const SignalEventInfo & sei)
{
   // Note that this method is called within the context of the POSIX/Win32 signal handler and thus we have to
   // be very careful about what we do here!   Sending a few bytes on a socket should be okay though.  (there is
   // the worry that the SignalHandlerSession object might have been deleted by the time we get here, but I don't
   // think there is much I can do about that)
   int nextSigNum;
   for (uint32 i=0; GetNthSignalNumber(i, nextSigNum).IsOK(); i++)
   {
      if (sei.GetSignalNumber() == nextSigNum)
      {
#ifdef MUSCLE_AVOID_CPLUSPLUS11
         uint8 buf[sizeof(int32)+sizeof(uint64)];  // ugly hack because C++03 doesn't know about constexpr methods
         assert(sizeof(buf)==SignalEventInfo::FlattenedSize(), "C++03 compatibility hack is using the wrong buffer size");
#else
         uint8 buf[SignalEventInfo::FlattenedSize()];
#endif
         sei.Flatten(DataFlattener(buf, sizeof(buf)));
         (void) SendData(_handlerFuncSocket, buf, sizeof(buf), false);  // send the signal value to the main thread so it can handle it later
         break;
      }
   }
}

} // end namespace muscle
