/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "platform/emscripten/EmscriptenAsyncCallback.h"
#include "platform/emscripten/EmscriptenWebSocket.h"
#include "util/String.h"

namespace muscle {

EmscriptenWebSocket :: EmscriptenWebSocket()
   : _watcher(NULL)
   , _state(STATE_INVALID)
{
   // empty
}

EmscriptenWebSocket :: EmscriptenWebSocket(EmscriptenWebSocketWatcher * watcher, int emSock)
   : Socket(emSock, false)
   , _watcher(watcher)
   , _state(STATE_INITIALIZING)
{
#if !defined(__EMSCRIPTEN__)
   (void) _watcher; // suppress compiler warning about member-variable not being used
#endif
}

EmscriptenWebSocket :: ~EmscriptenWebSocket()
{
#if defined(__EMSCRIPTEN__)
   const int emSock = GetFileDescriptor();
   if (emSock > 0)
   {
      const EMSCRIPTEN_RESULT cr = emscripten_websocket_close(emSock, 1000, "EmscriptenWebSocket Dtor");
      if (cr < 0) printf("emscripten_websocket_close(%i) failed (%i)\n", emSock, cr);

      const EMSCRIPTEN_RESULT dr = emscripten_websocket_delete(emSock);
      if (dr < 0) printf("emscripten_websocket_delete(%i) failed (%i)\n", emSock, dr);
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
   const int emSock = GetFileDescriptor();
   if (emSock > 0)
   {
      const EMSCRIPTEN_RESULT ret = emscripten_websocket_send_binary(emSock, const_cast<void *>(data), numBytes);
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
void EmscriptenWebSocket :: EmscriptenWebSocketConnectionOpened() {_state = STATE_OPEN;   if (_watcher) _watcher->EmscriptenWebSocketConnectionOpened(*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketErrorOccurred(   ) {_state = STATE_ERROR;  if (_watcher) _watcher->EmscriptenWebSocketErrorOccurred   (*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketConnectionClosed() {_state = STATE_CLOSED; if (_watcher) _watcher->EmscriptenWebSocketConnectionClosed(*this);}
void EmscriptenWebSocket :: EmscriptenWebSocketMessageReceived(const uint8 * dataBytes, uint32 numDataBytes, bool isText) {if (_watcher) _watcher->EmscriptenWebSocketMessageReceived(*this, dataBytes, numDataBytes, isText);}
#endif

EmscriptenWebSocketRef EmscriptenWebSocketWatcher :: CreateClientWebSocket(const String & host, uint16 port)
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
