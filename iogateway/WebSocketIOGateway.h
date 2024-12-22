#include "iogateway/AbstractMessageIOGateway.h"
#include "regex/StringMatcher.h"
#include "util/ByteBuffer.h"

namespace muscle {

/** This gateway can be used to communicate with incoming WebSocket connections. */
class WebSocketMessageIOGateway : public AbstractMessageIOGateway
{
public:
   /** Constructor
     * @param expectHTTPHeader if true, we'll expect the incoming data to start with a WebSocket-standard
     *                         HTTP GET and Upgrade-request, and we'll handle that before tring to parse any WebSocket frames.
     *                         if false, then we'll assume the upgrade from HTTP has already been handled and just start parsing
     *                         WebSocket data immediately.
     * @param protocolNameMatcher this should match any dotted-protocol-names that we want to accept, and not match any
     *                            dotted-protocol-names that we don't want to accept.  Defaults to "*" (a.k.a. accept any protocol name)
     * @param pathMatcher this should match any file-path names (e.g. as appear immediately after the GET command) that we want to accept,
     *                            and not match any file-paths we don't want to accept.  Defaults to "*" (a.k.a. accept any file-path)
     * @note protocolNameMatcher and pathMatcher are only used if (expectHTTPHeader) is true
     */
   WebSocketMessageIOGateway(bool expectHTTPHeader, const StringMatcher & protocolNameMatcher = StringMatcher("*"), const StringMatcher & pathMatcher = StringMatcher("*"));

   virtual bool HasBytesToOutput() const;

protected:
   virtual io_status_t DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);
   virtual io_status_t DoOutputImplementation(uint32 maxBytes);

private:
   status_t PerformProtocolUpgrade();
   status_t CreateReplyFrame(const uint8 * data, uint32 numBytes, uint8 opCode);
   status_t InitializeIncomingPayload(uint32 payloadSizeBytes, uint32 maskOffset, AbstractGatewayMessageReceiver & receiver);
   const uint8 * UnmaskPayloadSegment(uint32 * optRetSize);
   void ExecuteReceivedFrame(AbstractGatewayMessageReceiver & receiver);
   void FlushReceivedMessage(AbstractGatewayMessageReceiver & receiver);
   void ResetHeaderReceiveState();

   bool _needsProtocolUpgrade;
   StringMatcher _protocolNameMatcher;
   StringMatcher _pathMatcher;
   String _httpPreamble;

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
};

};  // end namespace muscle
