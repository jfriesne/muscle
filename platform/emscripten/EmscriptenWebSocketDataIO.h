/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef EmscriptenWebSocketDataIO_h
#define EmscriptenWebSocketDataIO_h

#include "dataio/DataIO.h"
#include "platform/emscripten/EmscriptenWebSocket.h"  // for EmscriptenWebSocketWatcher
#include "util/ByteBuffer.h"
#include "util/Hashtable.h"

namespace muscle {

class AbstractReflectSession;
class EmscriptenAsyncCallback;
class EmscriptenWebSocketWatcher;
class IPAddressAndPort;

/** This DataIO knows how to read and write bytes via an EmscriptenWebSocket */
class EmscriptenWebSocketDataIO : public DataIO, private EmscriptenWebSocketWatcher
{
public:
   /** Convenience Constructor
     * @param destURL the destination URL (e.g. "ws://localhost:8080") that we should pass to the WebSockets API to tell it where to
     *                connect our WebSocket to.  You may want to call IPAddress::ToURL("ws") or IPAddressAndPort::ToURL("ws") to
     *                generate this String.
     * @param optSession if non-NULL, we will make callbacks on this session when we receive callbacks from the EmscriptenWebSocket.
     * @param optAsyncCallback if non-NULL, we will schedule callbacks on this to handle the I/O for us
     */
   EmscriptenWebSocketDataIO(const String & destURL, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback);

   /** Constructor
     * @param emSockRef Reference to an EmscriptenWebSocket (e.g. as previously created by CreateClientWebSocket()) for this DataIO to use.
     * @param optSession if non-NULL, we will make callbacks on this session when we receive callbacks from the EmscriptenWebSocket.
     * @param optAsyncCallback if non-NULL, we will schedule callbacks on this to handle the I/O for us
     */
   EmscriptenWebSocketDataIO(const EmscriptenWebSocketRef & emSockRef, AbstractReflectSession * optSession, EmscriptenAsyncCallback * optAsyncCallback);

   /** Destructor */
   virtual ~EmscriptenWebSocketDataIO();

   virtual io_status_t Read(void * buffer, uint32 size);
   virtual io_status_t Write(const void * buffer, uint32 size);

   virtual void FlushOutput() {/* empty */}
   virtual void Shutdown();

   MUSCLE_NODISCARD virtual const ConstSocketRef &  GetReadSelectSocket() const {return _sockRef;}
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _sockRef;}

   /** Returns a reference to the EmscriptenWebSocket we are holding internally */
   const EmscriptenWebSocketRef & GetEmscriptenSocket() const {return _emSockRef;}

private:
   EmscriptenWebSocketRef _emSockRef;
   ConstSocketRef _sockRef;  // points to the same object as (_emSockRef); here solely for convenience

   AbstractReflectSession  * _optSession;
   EmscriptenAsyncCallback * _optAsyncCallback;

   Hashtable<ByteBufferRef, uint32> _receivedData;  // buffer -> bytes already read

#if defined(__EMSCRIPTEN__)
   virtual void EmscriptenWebSocketConnectionOpened(EmscriptenWebSocket & webSock);
   virtual void EmscriptenWebSocketMessageReceived( EmscriptenWebSocket & webSock, const uint8 * data, uint32 numDataBytes, bool isText);
   virtual void EmscriptenWebSocketErrorOccurred(   EmscriptenWebSocket & webSock);
   virtual void EmscriptenWebSocketConnectionClosed(EmscriptenWebSocket & webSock);
#endif
};
DECLARE_REFTYPES(EmscriptenWebSocketDataIO);

};  // end namespace muscle

#endif
