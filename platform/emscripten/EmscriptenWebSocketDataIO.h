/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_WEBSOCKET_DATAIO_H
#define EMSCRIPTEN_WEBSOCKET_DATAIO_H

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "dataio/DataIO.h"
#include "reflector/AbstractReflectSession.h"
#include "util/Hashtable.h"
#include "util/RefCount.h"

namespace muscle {

class EmscriptenAsyncCallback;
class EmscriptenWebSocketSubscriber;

/** RefCountable class to hold an Emscripten websocket and make sure it gets cleaned up when no longer in use */
class EmscriptenWebSocket : public RefCountable
{
public:
   /** Default constructor */
   EmscriptenWebSocket();

   /** Destructor -- closes and deletes our web socket, if we have one */
   virtual ~EmscriptenWebSocket();

   /** Send a data buffer via the WebSocket.
     * @param data the bytes to send
     * @param numBytes how many bytes (data) points to
     * @returns an io_status_t indicating how many bytes were sent, or an error
     */
   io_status_t Write(const void * data, uint32 numBytes);

   /** Returns a dummy ConstSocketRef that can (sort of) be used to represent our file descriptor */
   const ConstSocketRef & GetConstSocketRef() {return _sockRef;}

private:
   friend class EmscriptenWebSocketSubscriber;

   EmscriptenWebSocketSubscriber * _sub;
   int _emSock;

   ConstSocketRef _sockRef;

   EmscriptenWebSocket(EmscriptenWebSocketSubscriber * sub, int emSock);

public:
#if defined(__EMSCRIPTEN__)
   bool EmscriptenWebSocketConnectionOpened(int eventType, const EmscriptenWebSocketOpenEvent    & evt);
   bool EmscriptenWebSocketMessageReceived( int eventType, const EmscriptenWebSocketMessageEvent & evt);
   bool EmscriptenWebSocketErrorOccurred(   int eventType, const EmscriptenWebSocketErrorEvent   & evt);
   bool EmscriptenWebSocketConnectionClosed(int eventType, const EmscriptenWebSocketCloseEvent   & evt);
#endif
};
DECLARE_REFTYPES(EmscriptenWebSocket);

/** Interface class for an object that wants to receive WebSocket callbacks from Emscripten */
class EmscriptenWebSocketSubscriber
{
public:
   EmscriptenWebSocketSubscriber() {/* empty */}

   /** Utility method: Sets up and returns an outgoing WebSocket to the given host and port
     * @param host hostname to connect to
     * @param port port number to connect to
     * @returns a valid ref on success, or a NULL ref on failure.
     */
   EmscriptenWebSocketRef CreateClientWebSocket(const String & host, uint16 port);

protected:
#if defined(__EMSCRIPTEN__) || defined(DOXYGEN_SHOULD_IGNORE_THIS)
   /** This callback is called when a websocket connection becomes connected to a server.
     * @param webSock the socket that is now connected to the server
     * @param eventType the EMSCRIPTEN_EVENT_* value that represents this callback.  (Typically EMSCRIPTEN_EVENT_WEB_SOCKET_OPEN)
     * @param evt an EmscriptenWebSocketOpenEvent object containing more information about the event.
     * @return true to allow this event to be propagated to other listeners as well, or false to prevent further propagation of this event.
     */
   virtual bool EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketOpenEvent & evt) = 0;

   /** This callback is called when a websocket receives some data from the server.
     * @param webSock the socket that the data was received on
     * @param eventType the EMSCRIPTEN_EVENT_* value that represents this callback.  (Typically EMSCRIPTEN_EVENT_WEB_SOCKET_MESSAGE)
     * @param evt an EmscriptenWebSocketMessageEvent object containing more information about the event.
     * @return true to allow this event to be propagated to other listeners as well, or false to prevent further propagation of this event.
     */
   virtual bool EmscriptenWebSocketMessageReceived(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketMessageEvent & evt) = 0;

   /** This callback is called when a websocket reports an error condition.
     * @param webSock the socket that the error was received on
     * @param eventType the EMSCRIPTEN_EVENT_* value that represents this callback.  (Typically EMSCRIPTEN_EVENT_WEB_SOCKET_ERROR)
     * @param evt an EmscriptenWebSocketErrorEvent object containing more information about the error.
     * @return true to allow this event to be propagated to other listeners as well, or false to prevent further propagation of this event.
     */
   virtual bool EmscriptenWebSocketErrorOccurred(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketErrorEvent & evt) = 0;

   /** This callback is called when a websocket becomes disconnected from the server.
     * @param webSock the socket that is no longer connected to the server
     * @param eventType the EMSCRIPTEN_EVENT_* value that represents this callback.  (Typically EMSCRIPTEN_EVENT_WEB_SOCKET_CLOSE)
     * @param evt an EmscriptenWebSocketCloseEvent object containing more information about the event.
     * @return true to allow this event to be propagated to other listeners as well, or false to prevent further propagation of this event.
     */
   virtual bool EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketCloseEvent & evt) = 0;
#endif

private:
   friend class EmscriptenWebSocket;
};

/** This DataIO knows how to read and write bytes via an EmscriptenWebSocket */
class EmscriptenWebSocketDataIO : public DataIO, private EmscriptenWebSocketSubscriber
{
public:
   /** Constructor
     * @param host hostname to connect to
     * @param port port number to connect to
     * @param optSession if non-NULL, we will make callbacks on this session when we receive callbacks from the EmscriptenWebSocket.
     * @param optAsyncCallback if non-NULL, we will schedule callbacks on this to handle the I/O for us
     */
   EmscriptenWebSocketDataIO(const String & host, uint16 port, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback);

   /** Destructor */
   virtual ~EmscriptenWebSocketDataIO();

   virtual io_status_t Read(void * buffer, uint32 size);
   virtual io_status_t Write(const void * buffer, uint32 size);

   virtual void FlushOutput() {/* empty */}
   virtual void Shutdown();

   MUSCLE_NODISCARD virtual const ConstSocketRef &  GetReadSelectSocket() const {return _sock() ? _sock()->GetConstSocketRef() : GetInvalidSocket();}
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _sock() ? _sock()->GetConstSocketRef() : GetInvalidSocket();}

private:
   EmscriptenWebSocketRef _sock;
   AbstractReflectSession * _optSession;
   EmscriptenAsyncCallback * _optAsyncCallback;

   Hashtable<ByteBufferRef, uint32> _receivedData;  // buffer -> bytes already read

#if defined(__EMSCRIPTEN__)
   virtual bool EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketOpenEvent    & evt);
   virtual bool EmscriptenWebSocketMessageReceived( EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketMessageEvent & evt);
   virtual bool EmscriptenWebSocketErrorOccurred(   EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketErrorEvent   & evt);
   virtual bool EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketCloseEvent   & evt);
#endif
};

};  // end namespace muscle

#endif
