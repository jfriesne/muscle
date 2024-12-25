/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "platform/emscripten/EmscriptenWebSocketDataIO.h"
#include "util/String.h"

namespace muscle {

static void CheckPointer(const char * desc, const Hashtable<void *, Void> & table, void * ptr, int line)
{
   if (table.ContainsKey(ptr) == false)
   {
      printf("ERROR @ [%s]:%i:  ptr %p isn't in the table!\n", desc, line, ptr);
      MCRASH("WTF Z");
   }
}

EmscriptenWebSocket :: EmscriptenWebSocket()
   : _sub(NULL)
   , _emSock(-1)
{
   // empty
}

EmscriptenWebSocket :: EmscriptenWebSocket(EmscriptenWebSocketSubscriber * sub, int emSock)
   : _sub(sub)
   , _emSock(emSock)
   , _sockRef(new Socket(_emSock, false))
{
   // empty
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
static EM_BOOL em_websocket_onmessage_callback(int eventType, const EmscriptenWebSocketMessageEvent * evt, void * userData)
{
   return reinterpret_cast<EmscriptenWebSocket *>(userData)->EmScriptenWebSocketMessageReceived(eventType, *evt) ? EM_TRUE : EM_FALSE;
}

static EM_BOOL em_websocket_onopen_callback(int eventType, const EmscriptenWebSocketOpenEvent * evt, void * userData)
{
   return reinterpret_cast<EmscriptenWebSocket *>(userData)->EmScriptenWebSocketConnectionOpened(eventType, *evt) ? EM_TRUE : EM_FALSE;
}

static EM_BOOL em_websocket_onerror_callback(int eventType, const EmscriptenWebSocketErrorEvent * evt, void * userData)
{
   return reinterpret_cast<EmscriptenWebSocket *>(userData)->EmScriptenWebSocketErrorOccurred(eventType, *evt) ? EM_TRUE : EM_FALSE;
}

static EM_BOOL em_websocket_onclose_callback(int eventType, const EmscriptenWebSocketCloseEvent * evt, void * userData)
{
   return reinterpret_cast<EmscriptenWebSocket *>(userData)->EmScriptenWebSocketConnectionClosed(eventType, *evt) ? EM_TRUE : EM_FALSE;
}
#endif

io_status_t EmscriptenWebSocket :: Write(const void * data, uint32 numBytes)
{
#if defined(__EMSCRIPTEN__)
   if (_emSock > 0)
   {
      const EMSCRIPTEN_RESULT ret = emscripten_websocket_send_binary(_emSock, const_cast<void *>(data), numBytes);
      return (ret == EMSCRIPTEN_RESULT_SUCCESS) ? io_status_t(numBytes) : io_status_t(B_IO_ERROR);
   }
   else return B_BAD_OBJECT;
#else
   (void) data;
   (void) numBytes;
   return B_UNIMPLEMENTED;
#endif
}


#if defined(__EMSCRIPTEN__)
bool EmscriptenWebSocket :: EmScriptenWebSocketConnectionOpened(int eventType, const EmscriptenWebSocketOpenEvent    & evt) {return _sub ? _sub->EmScriptenWebSocketConnectionOpened(*this, eventType, evt) : false;}
bool EmscriptenWebSocket :: EmScriptenWebSocketMessageReceived( int eventType, const EmscriptenWebSocketMessageEvent & evt) {return _sub ? _sub->EmScriptenWebSocketMessageReceived( *this, eventType, evt) : false;}
bool EmscriptenWebSocket :: EmScriptenWebSocketErrorOccurred(   int eventType, const EmscriptenWebSocketErrorEvent   & evt) {return _sub ? _sub->EmScriptenWebSocketErrorOccurred   (*this, eventType, evt) : false;}
bool EmscriptenWebSocket :: EmScriptenWebSocketConnectionClosed(int eventType, const EmscriptenWebSocketCloseEvent   & evt) {return _sub ? _sub->EmScriptenWebSocketConnectionClosed(*this, eventType, evt) : false;}
#endif

EmscriptenWebSocketDataIO :: EmscriptenWebSocketDataIO(const String & host, uint16 port, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback)
   : _sock(CreateClientWebSocket(host, port))
   , _optSession(optSession)
   , _optAsyncCallback(optAsyncCallback)
   , _asyncConnectedFlag(false)
   , _asyncDisconnectedFlag(false)
{
   // empty
}

EmscriptenWebSocketDataIO :: ~EmscriptenWebSocketDataIO()
{
   // empty
}

io_status_t EmscriptenWebSocketDataIO :: Read(void * buffer, uint32 size)
{
   DECLARE_MUTEXGUARD(_mutex);

   if (_asyncConnectedFlag)
   {
      _asyncConnectedFlag = false;
      if (_optSession) _optSession->AsyncConnectCompleted();
   }

   const ByteBufferRef * lastBufRef = _receivedData.GetFirstKey();
   const ByteBuffer    * lastBuf    = lastBufRef ? lastBufRef->GetItemPointer() : NULL;
   if (lastBuf == NULL)
   {
      if (_asyncDisconnectedFlag)
      {
         _asyncDisconnectedFlag = false;
         if (_optSession) (void) _optSession->ClientConnectionClosed();
      }
      return io_status_t(0);  // nothing more to read!
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
   DECLARE_MUTEXGUARD(_mutex);
   return _sock() ? _sock()->Write(buffer, size) : B_BAD_OBJECT;
}

void EmscriptenWebSocketDataIO :: Shutdown()
{
   DECLARE_MUTEXGUARD(_mutex);
   _sock.Reset();
}

#if defined(__EMSCRIPTEN__)
bool EmscriptenWebSocketDataIO :: EmScriptenWebSocketConnectionOpened(EmscriptenWebSocket & /*webSock*/, int /*eventType*/, const EmscriptenWebSocketOpenEvent & /*evt*/)
{
   DECLARE_MUTEXGUARD(_mutex);
   if (_optAsyncCallback)
   {
      _asyncConnectedFlag = true;
      (void) _optAsyncCallback->SetAsyncCallbackTime(0);
   }
   else if (_optSession) _optSession->AsyncConnectCompleted();

   return true;
}

bool EmscriptenWebSocketDataIO :: EmScriptenWebSocketMessageReceived(EmscriptenWebSocket & /*webSock*/, int /*eventType*/, const EmscriptenWebSocketMessageEvent & evt)
{
   DECLARE_MUTEXGUARD(_mutex);
   if ((_optSession)&&(evt.numBytes > 0)&&(evt.isText == false))
   {
      const uint8 * newData = evt.data;
      const uint32 newNumBytes = evt.numBytes;

      if (_optAsyncCallback)
      {
         ByteBufferRef newBuf = GetByteBufferFromPool(newNumBytes, newData);
         if ((newBuf())&&(_receivedData.Put(newBuf, 0).IsOK())) (void) _optAsyncCallback->SetAsyncCallbackTime(0);  // wanna handle the data ASAP!
      }
      else
      {
         // Hopefully all the new data will be read synchronously, and we can avoid a data-copy here
         ByteBuffer tempBuf; tempBuf.AdoptBuffer(evt.numBytes, evt.data);
         DummyByteBufferRef dummyBuffer(tempBuf);
         if (_receivedData.Put(dummyBuffer, 0).IsOK())
         {
            io_status_t ret;
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

            tempBuf.ReleaseBuffer();
            return ret.IsOK();
         }
         else
         {
            tempBuf.ReleaseBuffer();
            return false;
         }
      }
   }

   return true;
}

bool EmscriptenWebSocketDataIO :: EmScriptenWebSocketErrorOccurred(EmscriptenWebSocket & /*webSock*/, int eventType, const EmscriptenWebSocketErrorEvent & /*evt*/)
{
   DECLARE_MUTEXGUARD(_mutex);
   LogTime(MUSCLE_LOG_ERROR, "EmScriptenWebSocketErrorOccurred:  Error %i on web socket!\n", eventType);
   return true;
}

bool EmscriptenWebSocketDataIO :: EmScriptenWebSocketConnectionClosed(EmscriptenWebSocket & /*webSock*/, int /*eventType*/, const EmscriptenWebSocketCloseEvent & /*evt*/)
{
   DECLARE_MUTEXGUARD(_mutex);
   if (_optAsyncCallback)
   {
      _asyncDisconnectedFlag = true;
      (void) _optAsyncCallback->SetAsyncCallbackTime(0);
   }
   else if (_optSession) (void) _optSession->ClientConnectionClosed();

   return true;
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
