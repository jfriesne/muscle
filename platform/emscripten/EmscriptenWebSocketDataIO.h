/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_WEBSOCKET_DATAIO_H
#define EMSCRIPTEN_WEBSOCKET_DATAIO_H

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
   void EmscriptenWebSocketConnectionOpened();
   void EmscriptenWebSocketMessageReceived(const uint8 * dataBytes, uint32 numDataBytes, bool isText);
   void EmscriptenWebSocketErrorOccurred();
   void EmscriptenWebSocketConnectionClosed();
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
     */
   virtual void EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock) = 0;

   /** This callback is called when a websocket receives some data from the server.
     * @param webSock the socket that the data was received on
     * @param data pointer to the received data bytes
     * @param numDataBytes how many data-bytes (data) points to
     * @param isText true if the received data is text, or false if it's binary data.
     */
   virtual void EmscriptenWebSocketMessageReceived(EmscriptenWebSocket & webSock, const uint8 * data, uint32 numDataBytes, bool isText) = 0;

   /** This callback is called when a websocket reports an error condition.
     * @param webSock the socket that the error was received on
     */
   virtual void EmscriptenWebSocketErrorOccurred(EmscriptenWebSocket & webSock) = 0;

   /** This callback is called when a websocket becomes disconnected from the server.
     * @param webSock the socket that is no longer connected to the server
     */
   virtual void EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock) = 0;
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
   virtual void EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock);
   virtual void EmscriptenWebSocketMessageReceived( EmscriptenWebSocket & webSock, const uint8 * data, uint32 numDataBytes, bool isText);
   virtual void EmscriptenWebSocketErrorOccurred(   EmscriptenWebSocket & webSock);
   virtual void EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock);
#endif
};

};  // end namespace muscle

#endif
