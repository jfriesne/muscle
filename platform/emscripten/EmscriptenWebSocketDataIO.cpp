/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "platform/emscripten/EmscriptenWebSocketDataIO.h"
#include "reflector/AbstractReflectSession.h"
#include "util/String.h"

namespace muscle {

EmscriptenWebSocketDataIO :: EmscriptenWebSocketDataIO(const String & host, uint16 port, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback)
   : _emSockRef(CreateClientWebSocket(host, port))
   , _sockRef(_emSockRef())
   , _optSession(optSession)
   , _optAsyncCallback(optAsyncCallback)
{
#if !defined(__EMSCRIPTEN__)
   (void) _optAsyncCallback; // suppress compiler warning about member-variable not being used
   (void) _optSession;       // ditto
#endif
}

EmscriptenWebSocketDataIO :: ~EmscriptenWebSocketDataIO()
{
   // empty
}

io_status_t EmscriptenWebSocketDataIO :: Read(void * buffer, uint32 size)
{
   const ByteBufferRef * lastBufRef = _receivedData.GetFirstKey();
   const ByteBuffer    * lastBuf    = lastBufRef ? lastBufRef->GetItemPointer() : NULL;
   if (lastBuf == NULL)
   {
      if (_emSockRef() == NULL) return B_BAD_OBJECT;  // dude, where's my socket?
      switch(_emSockRef()->GetState())
      {
         case EmscriptenWebSocket::STATE_INVALID:      return B_BAD_OBJECT;
         case EmscriptenWebSocket::STATE_INITIALIZING: return io_status_t(0);  // not ready to receive yet, but we should become ready shortly!
         case EmscriptenWebSocket::STATE_OPEN:         return io_status_t(0);  // no data ready right now, come back later
         case EmscriptenWebSocket::STATE_CLOSED:       return B_END_OF_STREAM;
         case EmscriptenWebSocket::STATE_ERROR:        return B_IO_ERROR;
         default:                                      return B_LOGIC_ERROR;
      }
   }

   uint32 * lastNumBytesRead   = _receivedData.GetFirstValue();  // guaranteed not to be NULL
   const uint32 bufSize        = lastBuf->GetNumBytes();
   const uint32 numBytesToCopy = muscleMin(size, bufSize-(*lastNumBytesRead));
   memcpy(buffer, lastBuf->GetBuffer()+(*lastNumBytesRead), numBytesToCopy);

   *lastNumBytesRead += numBytesToCopy;
   if (*lastNumBytesRead == bufSize) (void) _receivedData.RemoveFirst();

   return io_status_t(numBytesToCopy);
}

io_status_t EmscriptenWebSocketDataIO :: Write(const void * buffer, uint32 size)
{
   if (_emSockRef() == NULL) return B_BAD_OBJECT;

   switch(_emSockRef()->GetState())
   {
      case EmscriptenWebSocket::STATE_INVALID:      return B_BAD_OBJECT;
      case EmscriptenWebSocket::STATE_INITIALIZING: return io_status_t(0);                     // not ready to send yet, but we should be shortly!
      case EmscriptenWebSocket::STATE_OPEN:         return _emSockRef()->Write(buffer, size);  // go for it!
      case EmscriptenWebSocket::STATE_CLOSED:       return B_END_OF_STREAM;
      case EmscriptenWebSocket::STATE_ERROR:        return B_IO_ERROR;
      default:                                      return B_LOGIC_ERROR;
   }
}

void EmscriptenWebSocketDataIO :: Shutdown()
{
   _emSockRef.Reset();
}

#if defined(__EMSCRIPTEN__)
void EmscriptenWebSocketDataIO :: EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock)
{
   LogTime(MUSCLE_LOG_DEBUG, "EmscriptenWebSocketConnectionOpened:  web socket %i session opened!\n", webSock.GetFileDescriptor());

   if (_optSession) _optSession->AsyncConnectCompleted();
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}

void EmscriptenWebSocketDataIO :: EmscriptenWebSocketMessageReceived(EmscriptenWebSocket & /*webSock*/, const uint8 * dataBytes, uint32 numDataBytes, bool isText)
{
   io_status_t ret;

   if ((_optSession)&&(numDataBytes > 0)&&(isText == false))
   {
      // Hopefully all the new data will be read synchronously, and we can avoid a data-copy here
      ByteBuffer tempBuf; tempBuf.AdoptBuffer(numDataBytes, const_cast<uint8 *>(dataBytes));
      DummyByteBufferRef dummyBuffer(tempBuf);
      if (_receivedData.Put(dummyBuffer, 0).IsOK())
      {
         while((_receivedData.HasItems())&&(ret.IsOK())&&(_optSession->IsReadyForInput()))
         {
            const io_status_t subRet = _optSession->DoInput(*_optSession, MUSCLE_NO_LIMIT);

                 if (subRet.GetByteCount() > 0) ret += subRet;
            else if (subRet.GetByteCount() < 0) ret  = subRet;
            else                                break;    // avoid busy-looping if DoInput() isn't reading anything
         }

         // If there is still data in _receivedData, then the last entry will be some or all of our dummy buffer.
         // We need to replace it with one where we control the lifetime of the buffer.
         const ByteBufferRef * lastBufRef = _receivedData.GetLastKey();
         if (lastBufRef)
         {
            const ByteBuffer * lastBuf = lastBufRef->GetItemPointer();

            const uint32 numBytesRead = *_receivedData.GetLastValue();
            ByteBufferRef newBuf = GetByteBufferFromPool(lastBuf->GetNumBytes()-numBytesRead, lastBuf->GetBuffer()+numBytesRead);

            (void) _receivedData.RemoveLast();

            if ((newBuf() == NULL)||(_receivedData.Put(newBuf, 0).IsError())) ret = B_OUT_OF_MEMORY;
         }
      }
      else ret = B_OUT_OF_MEMORY;

      tempBuf.ReleaseBuffer();
   }

   // now that we've done our reading, schedule an iteration of the ReflectServer's
   // event loop in case any Pulse() calls need to be computed and scheduled as well
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}

void EmscriptenWebSocketDataIO :: EmscriptenWebSocketErrorOccurred(EmscriptenWebSocket & webSock)
{
   LogTime(MUSCLE_LOG_ERROR, "EmscriptenWebSocketErrorOccurred:  Error reported on web socket %i!\n", webSock.GetFileDescriptor());
   if (_optSession)       (void) _optSession->ClientConnectionClosed();
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}

void EmscriptenWebSocketDataIO :: EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock)
{
   LogTime(MUSCLE_LOG_DEBUG, "EmscriptenWebSocketConnectionClosed:  web socket %i session closed!\n", webSock.GetFileDescriptor());
   if (_optSession)       (void) _optSession->ClientConnectionClosed();
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}
#endif

};  // end namespace muscle
