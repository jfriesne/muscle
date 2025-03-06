/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "platform/emscripten/EmscriptenWebSocketDataIO.h"
#include "util/String.h"

namespace muscle {

EmscriptenWebSocket :: EmscriptenWebSocket()
   : _sub(NULL)
   , _emSock(-1)
   , _state(STATE_INVALID)
{
   // empty
}

EmscriptenWebSocket :: EmscriptenWebSocket(EmscriptenWebSocketSubscriber * sub, int emSock)
   : _sub(sub)
   , _emSock(emSock)
   , _state(STATE_INITIALIZING)
   , _sockRef(new Socket(_emSock, false))
{
#if !defined(__EMSCRIPTEN__)
   (void) _sub; // suppress compiler warning about member-variable not being used
#endif
}

EmscriptenWebSocket :: ~EmscriptenWebSocket()
{
#if defined(__EMSCRIPTEN__)
   if (_emSock > 0)
   {
      const EMSCRIPTEN_RESULT cr = emscripten_websocket_close(_emSock, 1000, "EmscriptenWebSocket Dtor");
      if (cr < 0) printf("emscripten_websocket_close(%i) failed (%i)\n", _emSock, cr);

      const EMSCRIPTEN_RESULT dr = emscripten_websocket_delete(_emSock);
      if (dr < 0) printf("emscripten_websocket_delete(%i) failed (%i)\n", _emSock, dr);
   }
#endif
}

#if defined(__EMSCRIPTEN__)
static EM_BOOL em_websocket_onmessage_callback(int /*eventType*/, const EmscriptenWebSocketMessageEvent * evt, void * userData)
{
   reinterpret_cast<EmscriptenWebSocket *>(userData)->EmscriptenWebSocketMessageReceived(evt->data, evt->numBytes, evt->isText);
   return EM_TRUE;
}

static EM_BOOL em_websocket_onopen_callback(int /*eventType*/, const EmscriptenWebSocketOpenEvent * /*evt*/, void * userData)
{
   reinterpret_cast<EmscriptenWebSocket *>(userData)->EmscriptenWebSocketConnectionOpened();
   return EM_TRUE;
}

static EM_BOOL em_websocket_onerror_callback(int /*eventType*/, const EmscriptenWebSocketErrorEvent * /*evt*/, void * userData)
{
   reinterpret_cast<EmscriptenWebSocket *>(userData)->EmscriptenWebSocketErrorOccurred();
   return EM_TRUE;
}

static EM_BOOL em_websocket_onclose_callback(int /*eventType*/, const EmscriptenWebSocketCloseEvent * /*evt*/, void * userData)
{
   reinterpret_cast<EmscriptenWebSocket *>(userData)->EmscriptenWebSocketConnectionClosed();
   return EM_TRUE;
}

static status_t GetStatusForEmscriptenResult(EMSCRIPTEN_RESULT r)
{
   switch(r)
   {
      case EMSCRIPTEN_RESULT_SUCCESS:             return B_NO_ERROR;
      case EMSCRIPTEN_RESULT_DEFERRED:            return B_ERROR("Emscripten: Deferred");
      case EMSCRIPTEN_RESULT_NOT_SUPPORTED:       return B_ERROR("Emscripten: Not Supported");
      case EMSCRIPTEN_RESULT_FAILED_NOT_DEFERRED: return B_ERROR("Emscripten: Failed Not Deferred");
      case EMSCRIPTEN_RESULT_INVALID_TARGET:      return B_ERROR("Emscripten: Invalid Target");
      case EMSCRIPTEN_RESULT_UNKNOWN_TARGET:      return B_ERROR("Emscripten: Unknown Target");
      case EMSCRIPTEN_RESULT_INVALID_PARAM:       return B_ERROR("Emscripten: Invalid Param");
      case EMSCRIPTEN_RESULT_FAILED:              return B_ERROR("Emscripten: Failed");
      case EMSCRIPTEN_RESULT_NO_DATA:             return B_ERROR("Emscripten: No Data");
      case EMSCRIPTEN_RESULT_TIMED_OUT:           return B_ERROR("Emscripten: Timed Out");
      default:                                    return B_ERROR("Emscripten: Unknown");
   }
}
#endif

io_status_t EmscriptenWebSocket :: Write(const void * data, uint32 numBytes)
{
#if defined(__EMSCRIPTEN__)
   if (_emSock > 0)
   {
      const EMSCRIPTEN_RESULT ret = emscripten_websocket_send_binary(_emSock, const_cast<void *>(data), numBytes);
      return (ret == EMSCRIPTEN_RESULT_SUCCESS) ? io_status_t(numBytes) : GetStatusForEmscriptenResult(ret);
   }
   else return B_BAD_OBJECT;
#else
   (void) data;
   (void) numBytes;
   return B_UNIMPLEMENTED;
#endif
}

#if defined(__EMSCRIPTEN__)
void EmscriptenWebSocket :: EmscriptenWebSocketConnectionOpened() {_state = STATE_OPEN;   if (_sub) _sub->EmscriptenWebSocketConnectionOpened(*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketErrorOccurred(   ) {_state = STATE_ERROR;  if (_sub) _sub->EmscriptenWebSocketErrorOccurred   (*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketConnectionClosed() {_state = STATE_CLOSED; if (_sub) _sub->EmscriptenWebSocketConnectionClosed(*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketMessageReceived(const uint8 * dataBytes, uint32 numDataBytes, bool isText) {if (_sub) _sub->EmscriptenWebSocketMessageReceived(*this, dataBytes, numDataBytes, isText);}
#endif

EmscriptenWebSocketDataIO :: EmscriptenWebSocketDataIO(const String & host, uint16 port, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback)
   : _sock(CreateClientWebSocket(host, port))
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
      if (_sock() == NULL) return B_BAD_OBJECT;  // dude, where's my socket?
      switch(_sock()->GetState())
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
   if (_sock() == NULL) return B_BAD_OBJECT;

   switch(_sock()->GetState())
   {
      case EmscriptenWebSocket::STATE_INVALID:      return B_BAD_OBJECT;
      case EmscriptenWebSocket::STATE_INITIALIZING: return io_status_t(0);                // not ready to send yet, but we should be shortly!
      case EmscriptenWebSocket::STATE_OPEN:         return _sock()->Write(buffer, size);  // go for it!
      case EmscriptenWebSocket::STATE_CLOSED:       return B_END_OF_STREAM;
      case EmscriptenWebSocket::STATE_ERROR:        return B_IO_ERROR;
      default:                                      return B_LOGIC_ERROR;
   }
}

void EmscriptenWebSocketDataIO :: Shutdown()
{
   _sock.Reset();
}

#if defined(__EMSCRIPTEN__)
void EmscriptenWebSocketDataIO :: EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock)
{
   LogTime(MUSCLE_LOG_DEBUG, "EmscriptenWebSocketConnectionOpened:  web socket %i session opened!\n", webSock.GetConstSocketRef().GetFileDescriptor());

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
   LogTime(MUSCLE_LOG_ERROR, "EmscriptenWebSocketErrorOccurred:  Error reported on web socket %i!\n", webSock.GetConstSocketRef().GetFileDescriptor());
   if (_optSession)       (void) _optSession->ClientConnectionClosed();
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}

void EmscriptenWebSocketDataIO :: EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock)
{
   LogTime(MUSCLE_LOG_DEBUG, "EmscriptenWebSocketConnectionClosed:  web socket %i session closed!\n", webSock.GetConstSocketRef().GetFileDescriptor());
   if (_optSession)       (void) _optSession->ClientConnectionClosed();
   if (_optAsyncCallback) (void) _optAsyncCallback->SetAsyncCallbackTime(0);
}
#endif

EmscriptenWebSocketRef EmscriptenWebSocketSubscriber :: CreateClientWebSocket(const String & host, uint16 port)
{
#if defined(__EMSCRIPTEN__)
   // Docs: https://github.com/emscripten-core/emscripten/blob/main/system/include/emscripten/websocket.h
   const String wsURL = String("ws://%1:%2").Arg(host).Arg(port);
   EmscriptenWebSocketCreateAttributes ws_attrs = {wsURL(), "binary", false};

   EMSCRIPTEN_WEBSOCKET_T s = emscripten_websocket_new(&ws_attrs);
   if (s <= 0)
   {
      printf("emscripten_websocket_new failed (%i)\n", s);
      return EmscriptenWebSocketRef();
   }

   EmscriptenWebSocketRef ret(new EmscriptenWebSocket(this, s));
   (void) emscripten_websocket_set_onopen_callback(   s, ret(), em_websocket_onopen_callback);
   (void) emscripten_websocket_set_onmessage_callback(s, ret(), em_websocket_onmessage_callback);
   (void) emscripten_websocket_set_onerror_callback(  s, ret(), em_websocket_onerror_callback);
   (void) emscripten_websocket_set_onclose_callback(  s, ret(), em_websocket_onclose_callback);

   return ret;
#else
   (void) host;
   (void) port;
   return B_UNIMPLEMENTED;
#endif
}

};  // end namespace muscle
