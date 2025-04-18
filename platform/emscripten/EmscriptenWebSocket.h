/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EMSCRIPTEN_WEBSOCKET_H
#define EMSCRIPTEN_WEBSOCKET_H

#include "util/Socket.h"

namespace muscle {

class EmscriptenAsyncCallback;
class EmscriptenWebSocketSubscriber;
class String;

/** Special Socket class that holds an Emscripten websocket and makes sure it gets cleaned up properly when no longer in use */
class EmscriptenWebSocket : public Socket
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

   enum {
      STATE_INVALID = 0,  ///< Not actually associated with a valid web socket descriptor
      STATE_INITIALIZING, ///< Connection isn't open yet
      STATE_OPEN,         ///< Open and ready to conduct business
      STATE_CLOSED,       ///< No longer available (closed by remote peer?)
      STATE_ERROR,        ///< Something went wrong
      NUM_STATES          ///< Guard value
   };

   /** Returns a STATE_* values indicating the current state of this WebSocket */
   uint32 GetState() const {return _state;}

private:
   friend class EmscriptenWebSocketSubscriber;
   EmscriptenWebSocket(EmscriptenWebSocketSubscriber * sub, int emSock);

   EmscriptenWebSocketSubscriber * _sub;
   uint32 _state;

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

};  // end namespace muscle

#endif
