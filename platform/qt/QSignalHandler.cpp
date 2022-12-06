/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <QCoreApplication>

#include "platform/qt/QSignalHandler.h"

namespace muscle {

QSignalHandler :: QSignalHandler(QObject * parent, const char * name)
   : QObject(parent)
   , _socketNotifier(NULL)
{
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
      char buf[64];
      const int32 bytesReceived = ReceiveData(_mainThreadSocket, buf, sizeof(buf), false).GetByteCount();
      if (bytesReceived <= 0) break;
      for (int32 i=0; i<bytesReceived; i++) emit SignalReceived(buf[i]);
   }
}

void QSignalHandler :: SignalHandlerFunc(int sigNum)
{
   // Note that this method is called within the context of the POSIX/Win32 signal handler and thus we have to
   // be very careful about what we do here!   Sending a byte on a socket should be okay though.  (there is the
   // worry that the SignalHandlerSession object might have been deleted by the time we get here, but I don't
   // think there is much I can do about that)
   int nextSigNum;
   for (uint32 i=0; GetNthSignalNumber(i, nextSigNum).IsOK(); i++)
   {
      if (sigNum == nextSigNum)
      {
         const char c = (char) sigNum;
         (void) SendData(_handlerFuncSocket, &c, 1, false);  // send the signal value to the main thread so it can handle it later
         break;
      }
   }
}

} // end namespace muscle
