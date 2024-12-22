#include "iogateway/WebSocketIOGateway.h"
#include "iogateway/PlainTextMessageIOGateway.h"  // for PR_COMMAND_TEXT_STRINGS
#include "iogateway/RawDataMessageIOGateway.h"    // for PR_COMMAND_RAW_DATA
#include "support/DataFlattener.h"
#include "util/IncrementalHashCalculator.h"
#include "util/MiscUtilityFunctions.h"  // for Base64Encode()
#include "util/StringTokenizer.h"

namespace muscle {

enum {
   WS_OPCODE_CONTINUATION = 0,
   WS_OPCODE_TEXT,
   WS_OPCODE_BINARY,
   WS_OPCODE_RESERVED_3,
   WS_OPCODE_RESERVED_4,
   WS_OPCODE_RESERVED_5,
   WS_OPCODE_RESERVED_6,
   WS_OPCODE_RESERVED_7,
   WS_OPCODE_CLOSE,
   WS_OPCODE_PING,
   WS_OPCODE_PONG,
   WS_OPCODE_RESERVED_B,
   WS_OPCODE_RESERVED_C,
   WS_OPCODE_RESERVED_D,
   WS_OPCODE_RESERVED_E,
   WS_OPCODE_RESERVED_F,
   NUM_WS_OPCODES
};

static String GetWebSocketHashKeyString(const String & orig)
{
   const String fullKey          = orig.WithAppend("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");  // per RFC 6455
   const IncrementalHash shaHash = IncrementalHashCalculator::CalculateHashSingleShot(HASH_ALGORITHM_SHA1, (const uint8 *) fullKey(), fullKey.Length());
   return Base64Encode(shaHash.GetBytes(), IncrementalHashCalculator::GetNumResultBytesUsedByAlgorithm(HASH_ALGORITHM_SHA1));
}

WebSocketMessageIOGateway :: WebSocketMessageIOGateway(bool expectHTTPHeader, const StringMatcher & protocolNameMatcher, const StringMatcher & pathMatcher)
   : _needsProtocolUpgrade(expectHTTPHeader)
   , _protocolNameMatcher(protocolNameMatcher)
   , _pathMatcher(pathMatcher)
   , _headerBytesReceived(0)
   , _headerSize(2)
   , _firstByteToMask(0)
   , _payloadBytesRead(0)
   , _maskOffset(0)
   , _opCode(0)
   , _inputClosed(false)
   , _outputBytesWritten(0)
{
   // empty
}

void WebSocketMessageIOGateway :: ResetHeaderReceiveState()
{
   /** Reset our state to receive the next frame's header */
   _headerBytesReceived = 0;
   _headerSize          = 2;
   _maskOffset          = 0;
}

status_t WebSocketMessageIOGateway :: PerformProtocolUpgrade()
{
   bool hasGet = false;
   Hashtable<String, String> args;
   {
      StringTokenizer tok(_httpPreamble(), "\r\n");
      const char * t;
      while((t = tok()) != NULL)
      {
         String s = t;
         s = s.Trimmed();
         if ((s.StartsWithIgnoreCase("GET "))&&(s.EndsWithIgnoreCase(" HTTP/1.1")))
         {
            hasGet = true;
            s = s.Substring(4);
            s = s.Substring(0, s.Length()-9);
            s = s.Trimmed();
            if (_pathMatcher.Match(s) == false)
            {
               LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  GET path [%s] doesn't match pattern [%s]\n", s(), _pathMatcher.GetPattern()());
               return B_ACCESS_DENIED;
            }
         }
         else
         {
            const int32 colIdx = s.IndexOf(':');
            if (colIdx > 0) (void) args.Put(s.Substring(0, colIdx).Trimmed(), s.Substring(colIdx+1).Trimmed());
         }
      }
   }

   if (hasGet == false)
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  No GET command found!\n");
      return B_BAD_DATA;
   }

   const String & upgradeTo = args["Upgrade"];
   if (!upgradeTo.EqualsIgnoreCase("websocket"))
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  Upgrade to websocket not found!  [%s]\n", upgradeTo());
      return B_BAD_DATA;
   }

   const String & conn = args["Connection"];
   if (!conn.EqualsIgnoreCase("Upgrade"))
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  Connection upgrade directive not found!  [%s]\n", conn());
      return B_BAD_DATA;
   }

   String foundProto;
   const String & proto = args["Sec-WebSocket-Protocol"];
   {
      StringTokenizer tok(proto(), ", ");
      const char * t;
      while((t = tok()) != NULL)
      {
         if (_protocolNameMatcher.Match(t))
         {
            foundProto = t;
            break;
         }
      }
      if (foundProto.IsEmpty())
      {
         LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  No protocol found in [%s] that matches [%s]\n", proto(), _protocolNameMatcher.GetPattern()());
         return B_ACCESS_DENIED;
      }
   }

   const String & key = args["Sec-WebSocket-Key"];
   if (key.HasChars())
   {
      // FogBugz #8945:  go into web-socket mode if that was required
      String us = "HTTP/1.1 101 Switching Protocols\r\n"
                  "Upgrade: websocket\r\n"
                  "Connection: Upgrade\r\n";
      us += String("Sec-WebSocket-Accept: %1\r\n").Arg(GetWebSocketHashKeyString(key));
      if (foundProto.HasChars()) us += String("Sec-WebSocket-Protocol: %1\r\n").Arg(foundProto);
      us += "\r\n";

      const io_status_t ret = GetDataIO()()->Write(us(), us.Length());  // I'm assuming this will always fit into the TCP socket's outgoing buffer immediately, since it's small
      return (ret.GetByteCount() == (int32)us.Length()) ? B_NO_ERROR : B_IO_ERROR;
   }
   else
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::PerformProtocolUpgrade():  Sec-WebSocket-Key found\n");
      return B_BAD_DATA;
   }
}

io_status_t WebSocketMessageIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   status_t ret;

   bool firstTime = true;  // always go at least once, to avoid live-lock
   int32 readBytes = 0;
   while((maxBytes > 0)&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      if (_needsProtocolUpgrade)
      {
         char temp[1024];

         const uint32 numBytesToReceive = muscleMin(maxBytes, (_payload()->GetNumBytes()-_payloadBytesRead));
         const io_status_t readRet      = GetDataIO()()->Read(_payload()->GetBuffer()+_payloadBytesRead, numBytesToReceive);
         if (readRet.IsError()) {ret = readRet.GetStatus(); break;}

         _httpPreamble += String(temp, readRet.GetByteCount());
         if (_httpPreamble.Contains("\r\n\r\n"))
         {
            ret = PerformProtocolUpgrade();
            if (ret.IsError()) {LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Protocol upgrade failed [%s]\n", ret()); break;}
            _needsProtocolUpgrade = false;
            _httpPreamble.Clear();
         }
         else if (_httpPreamble.Length() > (50*1024))
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  HTTP preamble is too long, aborting\n");
            return B_BAD_DATA;
         }
      }
      else if (_headerBytesReceived == _headerSize)
      {
         // download payload bytes
         if (_payload() == NULL) {LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Can't receive payload, no _payload buffer is present!\n"); return B_LOGIC_ERROR;}

         const uint32 numBytesToReceive = muscleMin(maxBytes, (_payload()->GetNumBytes()-_payloadBytesRead));
         const io_status_t readRet      = GetDataIO()()->Read(_payload()->GetBuffer()+_payloadBytesRead, numBytesToReceive);
         if (readRet.IsError()) {ret = readRet.GetStatus(); break;}

         const uint32 numBytesRead = readRet.GetByteCount();
         if (numBytesRead > 0)
         {
            readBytes         += numBytesRead;
            _payloadBytesRead += numBytesRead;
            maxBytes          -= numBytesRead;
            if (_payloadBytesRead == _payload()->GetNumBytes())
            {
                // If FIN is set, we'll execute this frame and clear it; otherwise we'll append the next frame's data to this one's.
                if ((_inputClosed)||(_headerBytes[0] & 0x80)) ExecuteReceivedFrame(receiver);
                ResetHeaderReceiveState();
            }
         }
         else break;
      }
      else
      {
         const uint32 headerBytesToReceive = muscleMin(maxBytes, _headerSize-_headerBytesReceived);
         const io_status_t readRet         = GetDataIO()()->Read(&_headerBytes[_headerBytesReceived], headerBytesToReceive);
         if (readRet.IsError()) {ret = readRet.GetStatus(); break;}

         const uint32 numBytesRead = readRet.GetByteCount();
         if (numBytesRead > 0)
         {
            readBytes            += numBytesRead;
            _headerBytesReceived += numBytesRead;
            maxBytes             -= numBytesRead;
            if (_headerBytesReceived == _headerSize)
            {
               switch(_headerSize)
               {
                  case 2:
                  {
                     if (_headerBytes[0] & 0x70) {LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateay:  Frame from client had reserved bits set!  %x\n", _headerBytes[0]); return B_BAD_DATA;}

                     const bool maskBit = ((_headerBytes[1] & 0x80) != 0);
                     if (maskBit == 0) {LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Frame from client didn't have its mask bit set!\n"); return B_BAD_DATA;}
                     switch(_headerBytes[1] & 0x7F)
                     {
                        case 126: _headerSize += 2+4; break; /* need to read two more bytes to find out the payload length, plus mask */
                        case 127: _headerSize += 8+4; break; /* need to read eight more bytes to find out the payload length, plus mask */
                        default:  _headerSize += 0+4; break; /* need mask only */
                     }
                     break;
                  }
                  break;

                  case 6:
                     MRETURN_ON_ERROR(InitializeIncomingPayload(_headerBytes[1]&0x7F, 2, receiver));
                  break;

                  case 8:
                     MRETURN_ON_ERROR(InitializeIncomingPayload(B_BENDIAN_TO_HOST_INT16(muscleCopyIn<int16>(&_headerBytes[2])), 4, receiver));
                  break;

                  case 14:
                  {
                     const uint64 payloadSize = B_BENDIAN_TO_HOST_INT64(muscleCopyIn<int64>(&_headerBytes[2]));
                     if (payloadSize > (10*1024*1024)) {LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Payload size " UINT64_FORMAT_SPEC " is too large!\n", payloadSize); return B_ACCESS_DENIED;}
                     MRETURN_ON_ERROR(InitializeIncomingPayload(payloadSize, 10, receiver));
                  }
                  break;

                  default:
                     LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Unexpected header size " UINT32_FORMAT_SPEC "!\n", _headerSize);
                  return B_BAD_DATA;
               }
            }
         }
         else break;
      }
   }

   FlushReceivedMessage(receiver);
   return ((ret.IsError())&&(readBytes == 0)) ? io_status_t(ret) : io_status_t(readBytes);
}

void WebSocketMessageIOGateway :: FlushReceivedMessage(AbstractGatewayMessageReceiver & receiver)
{
   if (_receivedMsg())
   {
      MessageRef temp; temp.SwapContents(_receivedMsg);  // paranoia about re-entrancy issues here
      receiver.CallMessageReceivedFromGateway(temp, NULL);
   }
}

bool WebSocketMessageIOGateway :: HasBytesToOutput() const
{
   return ((_needsProtocolUpgrade == false)&&((_outputBytesWritten < _outputBuf.GetNumBytes())||(GetOutgoingMessageQueue().HasItems())));
}

io_status_t WebSocketMessageIOGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if (_needsProtocolUpgrade) return io_status_t(0);  // semi-paranoia: no generating any data until after we've sent our upgrade-response HTTP!

   int32 bytesWritten = 0;
   while(maxBytes > 0)
   {
      if (_outputBytesWritten < _outputBuf.GetNumBytes())
      {
         const io_status_t ret = GetDataIO()()->Write(_outputBuf.GetBuffer()+_outputBytesWritten, muscleMin(_outputBuf.GetNumBytes()-_outputBytesWritten, maxBytes));
         MRETURN_ON_ERROR(ret);

         const uint32 numBytesWritten = ret.GetByteCount();
         if (numBytesWritten > 0)
         {
            bytesWritten        += numBytesWritten;
            _outputBytesWritten += bytesWritten;
            maxBytes            -= bytesWritten;
         }
         else break;
      }
      else
      {
         Queue<MessageRef> & q = GetOutgoingMessageQueue();
         if (q.HasItems())
         {
            Message * m = q.Head()();
            if (m->what == WS_OPCODE_PONG)
            {
               // form Pong reply
               ByteBufferRef bufRef;
               if (m->FindFlat("data", bufRef) == B_NO_ERROR)
               {
                  MRETURN_ON_ERROR(CreateReplyFrame(bufRef()->GetBuffer(), bufRef()->GetNumBytes(), WS_OPCODE_PONG));
               }
               (void) q.RemoveHead();
            }
            else
            {
               // form Text reply
               const String * s = m->GetStringPointer(PR_NAME_TEXT_LINE);
               if (s)
               {
                  MRETURN_ON_ERROR(CreateReplyFrame((const uint8 *) s->Cstr(), s->Length(), WS_OPCODE_TEXT));  // note that we don't send a NUL terminator byte!
                  (void) m->RemoveData(PR_NAME_TEXT_LINE);
               }
               else (void) q.RemoveHead();
            }
         }
         else break;
      }
   }

   return io_status_t(bytesWritten);
}

status_t WebSocketMessageIOGateway :: CreateReplyFrame(const uint8 * data, uint32 numBytes, uint8 opCode)
{
   uint32 frameSize = 2+numBytes;

        if (numBytes > 65535) frameSize += 8;  // oops, we need the 8-byte-payload field
   else if (numBytes > 125)   frameSize += 2;  // oops, we need the 2-byte-payload field

   MRETURN_ON_ERROR(_outputBuf.SetNumBytes(frameSize, false));

   _outputBytesWritten = 0;

   BigEndianDataFlattener flat(_outputBuf);

   flat.WriteInt8(0x80 | opCode);  // 0x80 == FIN bit
   if (numBytes > 65535)
   {
      flat.WriteInt8(127); // magic number for 8-byte payload
      flat.WriteInt64(numBytes);
   }
   else if (numBytes > 125)
   {
      flat.WriteInt8(126);   // magic number for 2-byte payload
      flat.WriteInt16(numBytes);
   }
   else flat.WriteInt8((uint8) numBytes);

   flat.WriteBytes(data, numBytes);
   return B_NO_ERROR;
}

status_t WebSocketMessageIOGateway :: InitializeIncomingPayload(uint32 payloadSizeBytes, uint32 maskOffset, AbstractGatewayMessageReceiver & receiver)
{
   _maskOffset = maskOffset;

   if (payloadSizeBytes == 0)
   {
      // Special case for when there is no payload to receive
      if (_payload() == NULL) _opCode  = _headerBytes[0] & 0x0F;
      if (_headerBytes[0] & 0x80) ExecuteReceivedFrame(receiver);
      ResetHeaderReceiveState();
      return B_NO_ERROR;
   }
   else
   {
      if (_payload())
      {
         // don't change _opCode if we are merely extending a fragment...
         (void) UnmaskPayloadSegment(NULL);  // Gotta unmask the previous segment while we still have its mask
         if (_payload()->AppendBytes(NULL, payloadSizeBytes, false) != B_NO_ERROR) _payload.Reset();
      }
      else
      {
         _opCode  = _headerBytes[0] & 0x0F;
         _payload = GetByteBufferFromPool(payloadSizeBytes);
      }
      return _payload() ? B_NO_ERROR : B_ERROR;
   }
}

const uint8 * WebSocketMessageIOGateway :: UnmaskPayloadSegment(uint32 * optRetSize)
{
   // Time to unmask our payload!
   uint8 * payloadBytes = _payload() ? _payload()->GetBuffer() : NULL;
   if (payloadBytes)
   {
      uint32 numPayloadBytes = _payload()->GetNumBytes();
      const uint8 * mask = &_headerBytes[_maskOffset];
      for (uint32 i=_firstByteToMask; i<numPayloadBytes; i++) payloadBytes[i] ^= mask[i%4];
      if (optRetSize) *optRetSize = numPayloadBytes;
      _firstByteToMask = numPayloadBytes;
   }
   else
   {
      _firstByteToMask = 0;
      if (optRetSize) *optRetSize = 0;
   }

   return payloadBytes;
}

void WebSocketMessageIOGateway :: ExecuteReceivedFrame(AbstractGatewayMessageReceiver & receiver)
{
   if (_inputClosed == false)
   {
      uint32 payloadSize;
      const uint8 * payloadBytes = UnmaskPayloadSegment(&payloadSize);

      switch(_opCode)
      {
         case WS_OPCODE_CONTINUATION:
            if (_inputClosed == false) LogTime(MUSCLE_LOG_WARNING, "WebSocketMessageIOGateway:  Executing a continuation opcode?!?\n");
         break;

         case WS_OPCODE_TEXT:
         {
            if ((_receivedMsg())&&(_receivedMsg()->what != PR_COMMAND_TEXT_STRINGS)) FlushReceivedMessage(receiver);

            const String text((const char *) payloadBytes, payloadSize);
            LogTime(MUSCLE_LOG_DEBUG, "Received text from client:  [%s]\n", text());
            if (_receivedMsg() == NULL) _receivedMsg = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
            if (_receivedMsg())
            {
               StringTokenizer tok(text(), "\r\n");  // GXFW-84
               const char * t;
               while((t=tok()) != NULL) (void) _receivedMsg()->AddString(PR_NAME_TEXT_LINE, t);
            }
         }
         break;

         case WS_OPCODE_BINARY:
         {
            if ((_receivedMsg())&&(_receivedMsg()->what != PR_COMMAND_RAW_DATA)) FlushReceivedMessage(receiver);

            ByteBufferRef buf = GetByteBufferFromPool(payloadSize, payloadBytes);
            if (buf())
            {
               LogTime(MUSCLE_LOG_DEBUG, "Received blob from client: " UINT32_FORMAT_SPEC "bytes\n", payloadSize);
               if (_receivedMsg() == NULL) _receivedMsg = GetMessageFromPool(PR_COMMAND_RAW_DATA);
               if (_receivedMsg()) (void) _receivedMsg()->AddFlat(PR_NAME_DATA_CHUNKS, buf);
            }
         }
         break;

         case WS_OPCODE_CLOSE:
            _inputClosed = true;
            LogTime(MUSCLE_LOG_DEBUG, "Got WS_OPCODE_CLOSE!\n");
         break;

         case WS_OPCODE_PING:
         {
            FlushReceivedMessage(receiver);  // necessary because the receiver might queue some outgoing-data that we want to appear before the pong we send below

            MessageRef pongMsg = GetMessageFromPool(WS_OPCODE_PONG);
            if ((pongMsg())&&((_payload() == NULL)||(pongMsg()->AddFlat("data", _payload) == B_NO_ERROR))) (void) AddOutgoingMessage(pongMsg);
         }
         break;

         case WS_OPCODE_PONG:
            // Not sure what to do with pongs, so I'm just going to ignore them for now
         break;

         default:
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Received unsupported opcode 0x%x!\n", _opCode);
         break;
      }
   }

   _opCode = 0;
   _payload.Reset();
   _payloadBytesRead = 0;
   _firstByteToMask  = 0;
}

};  // end namespace muscle
