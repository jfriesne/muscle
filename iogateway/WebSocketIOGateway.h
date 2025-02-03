#include "iogateway/AbstractMessageIOGateway.h"
#include "regex/StringMatcher.h"
#include "util/ByteBuffer.h"

namespace muscle {

/** This gateway can be used to communicate over WebSocket connections. */
class WebSocketMessageIOGateway : public AbstractMessageIOGateway
{
public:
   /** Default constructor
     * This contructor sets up a WebSocketMessageIOGateway with no HTTP handshaking phase.  It's assumed that any necessary
     * HTTP->WebSockets upgrade handshaking has already been handled via some other mechanism.
     */
   WebSocketMessageIOGateway();

   /** Server-side constructor
     * @param protocolNameMatcher this should match any dotted-protocol-names that we want to accept, and not match any
     *                            dotted-protocol-names that we don't want to accept.  Defaults to "*" (a.k.a. accept any protocol name)
     * @param pathMatcher this should match any file-path names (e.g. as appear immediately after the GET command) that we want to accept,
     *                            and not match any file-paths we don't want to accept.  Defaults to "*" (a.k.a. accept any file-path)
     */
   WebSocketMessageIOGateway(const StringMatcher & protocolNameMatcher = StringMatcher("*"), const StringMatcher & pathMatcher = StringMatcher("*"));

   /** Client-side constructor
     * @param getPath the filepath to request in the HTTP GET command (e.g. "/chat")
     * @param host the hostname to specify in the "Host:" header.
     * @param protocolsStr The dotted websocket-sub-protocol specifier to request.  (Multiple protocols may be specified, comma-separated)
     * @param origin optional hostname to place into the "Origin:" field
     */
   WebSocketMessageIOGateway(const String & getPath, const String & host, const String & protocolsStr, const String & origin);

   virtual bool HasBytesToOutput() const;

   /** Returns true iff our HTTP->WebSocket upgrade handshake is still in progress. */
   bool IsHandshakeInProgress() const {return ((_handshakeState == WEBSOCKET_HANDSHAKE_AS_SERVER)||(_handshakeState == WEBSOCKET_HANDSHAKE_AS_CLIENT));}

   /** If we are configured as a client, then after the handshake terminates, this method will return the protcool the server
     * indicated to us that he will be using (i.e. one of the protocols we proposed)
     */
   const String & GetAcceptedProtocol() const {return _acceptedProtocol;}

   /** If you want this gateway to tunnel the protocol of another gateway over the WebSocket connection, you
     * can install that other gateway here and this gateway will use it to convert incoming WebSocket binary buffers
     * into Messages, and to convert outgoing Messages into binary buffers to send as WebSocket buffers.
     * @param slaveGateway gateway to use as a data-converter
     */
   void SetSlaveGateway(const AbstractMessageIOGatewayRef & slaveGateway) {_slaveGateway = slaveGateway;}

   /** Returns a reference to our currently-held slave gateway.  If NULL (the default) then we'll use our built-ing
     * algorithm to turn PR_COMMAND_TEXT_STRINGS Messages into ASCII WebSocket buffers, and PR_COMMAND_RAW_DATA Messages
     * into binary WebSocket buffers.
     */
   const AbstractMessageIOGatewayRef & GetSlaveGateway() const {return _slaveGateway;}

protected:
   virtual io_status_t DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);
   virtual io_status_t DoOutputImplementation(uint32 maxBytes);

private:
   enum {
      WEBSOCKET_HANDSHAKE_AS_SERVER = 0, ///< Respond to initial incoming HTTP GET request with an upgrade proposal
      WEBSOCKET_HANDSHAKE_AS_CLIENT,     ///< Send an HTTP GET request on startup
      WEBSOCKET_HANDSHAKE_NONE,          ///< Don't do any HTTP handshaking; assume that has already been handled elsewhere
      NUM_WEBSOCKET_HANDSHAKES           ///< Guard value
   };

   status_t HandleReceivedHTTPText();
   status_t CreateReplyFrame(const uint8 * data, uint32 numBytes, uint8 opCode);
   status_t InitializeIncomingPayload(uint32 payloadSizeBytes, uint32 maskOffset, AbstractGatewayMessageReceiver & receiver);
   const uint8 * UnmaskPayloadSegment(uint32 * optRetSize);
   void ExecuteReceivedFrame(AbstractGatewayMessageReceiver & receiver);
   void FlushReceivedMessage(AbstractGatewayMessageReceiver & receiver);
   void ResetHeaderReceiveState();

   uint32 _handshakeState;
   StringMatcher _protocolNameMatcher;
   StringMatcher _pathMatcher;

   String _clientGeneratedKey;
   String _receivedHTTPText;

   String _httpTextToWrite;
   uint32 _numHTTPBytesWritten;

   String _acceptedProtocol; // the protocol the server told us he has chosen to use

   uint8 _headerBytes[14];   // maximum possible header size is 14 bytes
   uint32 _headerBytesReceived;
   uint32 _headerSize;

   ByteBufferRef _payload;
   uint32 _firstByteToMask;
   uint32 _payloadBytesRead;
   uint32 _maskOffset;
   uint8 _opCode;
   bool _inputClosed;
   MessageRef _receivedMsg;

   ByteBuffer _outputBuf;
   uint32 _outputBytesWritten;

   AbstractMessageIOGatewayRef _slaveGateway;
   ByteBuffer _scratchSlaveBuf;
};
DECLARE_REFTYPES(WebSocketMessageIOGateway);

};  // end namespace muscle
