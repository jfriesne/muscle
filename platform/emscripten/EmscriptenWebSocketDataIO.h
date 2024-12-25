/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_WEBSOCKET_DATAIO_H
#define EMSCRIPTEN_WEBSOCKET_DATAIO_H

#if defined(__EMSCRIPTEN__)
# include <emscripten/websocket.h>
#endif

#include "dataio/DataIO.h"
#include "reflector/AbstractReflectSession.h"
#include "system/Mutex.h"
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
   bool EmScriptenWebSocketConnectionOpened(int eventType, const EmscriptenWebSocketOpenEvent    & evt);
   bool EmScriptenWebSocketMessageReceived( int eventType, const EmscriptenWebSocketMessageEvent & evt);
   bool EmScriptenWebSocketErrorOccurred(   int eventType, const EmscriptenWebSocketErrorEvent   & evt);
   bool EmScriptenWebSocketConnectionClosed(int eventType, const EmscriptenWebSocketCloseEvent   & evt);
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
#if defined(__EMSCRIPTEN__)
   virtual bool EmScriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketOpenEvent    & evt) = 0;
   virtual bool EmScriptenWebSocketMessageReceived( EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketMessageEvent & evt) = 0;
   virtual bool EmScriptenWebSocketErrorOccurred(   EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketErrorEvent   & evt) = 0;
   virtual bool EmScriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketCloseEvent   & evt) = 0;
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

   bool _asyncConnectedFlag;
   bool _asyncDisconnectedFlag;

   Mutex _mutex;
   Hashtable<ByteBufferRef, uint32> _receivedData;  // buffer -> bytes already read

#if defined(__EMSCRIPTEN__)
   virtual bool EmScriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketOpenEvent    & evt);
   virtual bool EmScriptenWebSocketMessageReceived( EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketMessageEvent & evt);
   virtual bool EmScriptenWebSocketErrorOccurred(   EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketErrorEvent   & evt);
   virtual bool EmScriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock, int eventType, const EmscriptenWebSocketCloseEvent   & evt);
#endif
};

};  // end namespace muscle

#endif
