/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/SignalHandlerSession.h"
#include "system/SignalMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

ConstSocketRef SignalHandlerSession :: CreateDefaultSocket()
{
   ConstSocketRef sock;
   (void) CreateConnectedSocketPair(sock, _handlerSocket);
   return sock; 
}

int32 SignalHandlerSession :: DoInput(AbstractGatewayMessageReceiver &, uint32)
{
   uint32 byteCount = 0;
   while(1)
   {
      char buf[64];
      int32 bytesReceived = ReceiveData(GetSessionReadSelectSocket(), buf, sizeof(buf), false);
      if (bytesReceived > 0)
      {
         byteCount += bytesReceived;
         for (int32 i=0; i<bytesReceived; i++) SignalReceived(buf[i]);
      }
      else if (bytesReceived < 0) return -1;
      else break;
   }
   return byteCount;
}

status_t SignalHandlerSession :: AttachedToServer()
{
   if (AbstractReflectSession::AttachedToServer() != B_NO_ERROR) return B_ERROR;
   return SignalMultiplexer::GetSignalMultiplexer().AddHandler(this);
}

void SignalHandlerSession :: AboutToDetachFromServer()
{
   SignalMultiplexer::GetSignalMultiplexer().RemoveHandler(this);
   AbstractReflectSession::AboutToDetachFromServer();
}

void SignalHandlerSession :: SignalReceived(int whichSignal)
{
   LogTime(MUSCLE_LOG_CRITICALERROR, "Signal %i received, ending event loop!\n", whichSignal);
   EndServer();
}

static bool _wasSignalCaught = false;
bool WasSignalCaught() {return _wasSignalCaught;}

void SignalHandlerSession :: SignalHandlerFunc(int sigNum)
{
   // Note that this method is called within the context of the POSIX/Win32 signal handler and thus we have to
   // be very careful about what we do here!   Sending a byte on a socket should be okay though.  (there is the
   // worry that the SignalHandlerSession object might have been deleted by the time we get here, but I don't
   // think there is much I can do about that)
   if (IsAttachedToServer())
   {
      int nextSigNum;
      for (uint32 i=0; GetNthSignalNumber(i, nextSigNum) == B_NO_ERROR; i++)
      {
         if (sigNum == nextSigNum)
         {
            _wasSignalCaught = true;
            char c = (char) sigNum;
            (void) SendData(_handlerSocket, &c, 1, false);  // send the signal value to the main thread so it can handle it later
            break;
         }
      }
   }
}

}; // end namespace muscle
