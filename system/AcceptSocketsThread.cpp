/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "system/AcceptSocketsThread.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

namespace muscle {

AcceptSocketsThread :: AcceptSocketsThread()
{
   // empty
}

AcceptSocketsThread :: AcceptSocketsThread(uint16 port, const IPAddress & optInterfaceIP)
{
   (void) SetPort(port, optInterfaceIP);
}

AcceptSocketsThread :: ~AcceptSocketsThread()
{
   // empty
}

status_t AcceptSocketsThread :: SetPort(uint16 port, const IPAddress & optInterfaceIP)
{
   if (IsInternalThreadRunning() == false)
   {
      _port = 0;
      _acceptSocket = CreateAcceptingSocket(port, 20, &port, optInterfaceIP);
      if (_acceptSocket())
      {
         _port = port;
         return B_NO_ERROR;
      }
      else return B_ERROR("CreateAcceptingSocket() failed");
   }
   return B_BAD_OBJECT;
}

status_t AcceptSocketsThread :: StartInternalThread()
{
   if ((IsInternalThreadRunning() == false)&&(_acceptSocket()))
   {
      _notifySocket = GetInternalThreadWakeupSocket();
      return (_notifySocket.GetFileDescriptor() >= 0) ? Thread::StartInternalThread() : B_BAD_OBJECT;
   } 
   return B_BAD_OBJECT;
}

void AcceptSocketsThread :: InternalThreadEntry()
{
   SocketMultiplexer multiplexer;
   bool keepGoing = true;
   while(keepGoing)
   {
      const int afd = _acceptSocket.GetFileDescriptor();
      const int nfd = _notifySocket.GetFileDescriptor();

      multiplexer.RegisterSocketForReadReady(afd);
      multiplexer.RegisterSocketForReadReady(nfd);
      if (multiplexer.WaitForEvents() < 0) break;
      if (multiplexer.IsSocketReadyForRead(nfd))
      {
         MessageRef msgRef;
         int32 numLeft;
         while((numLeft = WaitForNextMessageFromOwner(msgRef, 0)) >= 0)
         {
            if (MessageReceivedFromOwner(msgRef, numLeft).IsError())
            { 
               keepGoing = false;
               break;
            }
         }
      }
      if (multiplexer.IsSocketReadyForRead(afd))
      {
         ConstSocketRef newSocket = Accept(_acceptSocket);
         if (newSocket())
         {
            MessageRef msg(GetMessageFromPool(AST_EVENT_NEW_SOCKET_ACCEPTED));
            msg()->AddTag(AST_NAME_SOCKET, CastAwayConstFromRef(newSocket.GetRefCountableRef()));
            (void) SendMessageToOwner(msg);
         }
      }
   }
}

} // end namespace muscle
