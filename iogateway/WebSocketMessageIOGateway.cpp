#include "dataio/ByteBufferDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"  // for PR_COMMAND_TEXT_STRINGS
#include "iogateway/RawDataMessageIOGateway.h"    // for PR_COMMAND_RAW_DATA
#include "iogateway/WebSocketMessageIOGateway.h"
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

static const String WS_GATEWAY_NAME_SPECIAL = "_wsgwy_";  // used to differentiate our own Messages from the user's Messages

static String GetWebSocketHashKeyString(const String & orig)
{
   const String fullKey          = orig.WithAppend("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");  // per RFC 6455
   const IncrementalHash shaHash = IncrementalHashCalculator::CalculateHashSingleShot(HASH_ALGORITHM_SHA1, (const uint8 *) fullKey(), fullKey.Length());
   return Base64Encode(shaHash.GetBytes(), IncrementalHashCalculator::GetNumResultBytesUsedByAlgorithm(HASH_ALGORITHM_SHA1));
}

WebSocketMessageIOGateway :: WebSocketMessageIOGateway()
   : _handshakeState(WEBSOCKET_HANDSHAKE_NONE)
   , _numHTTPBytesWritten(0)
   , _headerBytesReceived(0)
   , _headerSize(2)
   , _firstByteToMask(0)
   , _payloadBytesRead(0)
   , _opCode(0)
   , _inputClosed(false)
   , _outputBytesWritten(0)
{
   // empty
}

WebSocketMessageIOGateway :: WebSocketMessageIOGateway(const StringMatcher & protocolNameMatcher, const StringMatcher & pathMatcher)
   : _handshakeState(WEBSOCKET_HANDSHAKE_AS_SERVER)
   , _protocolNameMatcher(protocolNameMatcher)
   , _pathMatcher(pathMatcher)
   , _numHTTPBytesWritten(0)
   , _headerBytesReceived(0)
   , _headerSize(2)
   , _firstByteToMask(0)
   , _payloadBytesRead(0)
   , _opCode(0)
   , _inputClosed(false)
   , _outputBytesWritten(0)
{
   // empty
}

WebSocketMessageIOGateway :: WebSocketMessageIOGateway(const String & getPath, const String & host, const String & protocolsStr, const String & origin)
   : _handshakeState(WEBSOCKET_HANDSHAKE_AS_CLIENT)
   , _numHTTPBytesWritten(0)
   , _headerBytesReceived(0)
   , _headerSize(2)
   , _firstByteToMask(0)
   , _payloadBytesRead(0)
   , _opCode(0)
   , _inputClosed(false)
   , _outputBytesWritten(0)
{
   {
      uint8 randomBytes[16];
      const uint64 nonce = GetCurrentTime64() + GetRunTime64() + ((uintptr)this) + GetInsecurePseudoRandomNumber();
      memcpy(&randomBytes[0],             &nonce, sizeof(nonce));
      memcpy(&randomBytes[sizeof(nonce)], &nonce, sizeof(nonce));
      _clientGeneratedKey = Base64Encode(randomBytes, sizeof(randomBytes));
   }

   // Generate the HTTP request that we will send ASAP
   _httpTextToWrite  = String("GET %1 HTTP/1.1\r\n").Arg(getPath);
   _httpTextToWrite += String("Host: %1\n\n").Arg(host);
   _httpTextToWrite += "Upgrade: websocket\r\n";
   _httpTextToWrite += "Connection: Upgrade\r\n";
   _httpTextToWrite += String("Sec-WebSocket-Key: %1\r\n").Arg(_clientGeneratedKey);
   if (protocolsStr.HasChars()) _httpTextToWrite += String("Sec-WebSocket-Protocol: %4\r\n").Arg(protocolsStr);
   _httpTextToWrite += "Sec-WebSocket-Version: 13\r\n";
   if (origin.HasChars()) _httpTextToWrite += String("Origin: %1\r\n").Arg(origin);
}

void WebSocketMessageIOGateway :: ResetHeaderReceiveState()
{
   /** Reset our state to receive the next frame's header */
   _headerBytesReceived = 0;
   _headerSize          = 2;
}

status_t WebSocketMessageIOGateway :: HandleReceivedHTTPText()
{
   bool hasGet = false, hasSwitching = false;;
   Hashtable<String, String> args;
   {
      StringTokenizer tok(_receivedHTTPText(), "\r\n");
      const char * t;
      while((t = tok()) != NULL)
      {
         String s = t;
         s = s.Trimmed();

              if ((_handshakeState == WEBSOCKET_HANDSHAKE_AS_CLIENT)&&(s == "HTTP 1.1 101 Switching Protocols")) hasSwitching = true;
         else if ((_handshakeState == WEBSOCKET_HANDSHAKE_AS_SERVER)&&(s.StartsWithIgnoreCase("GET "))&&(s.EndsWithIgnoreCase(" HTTP/1.1")))
         {
            hasGet = true;
            s = s.Substring(4);
            s = s.Substring(0, s.Length()-9);
            s = s.Trimmed();
            if (_pathMatcher.Match(s) == false)
            {
               LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  GET path [%s] doesn't match pattern [%s]\n", s(), _pathMatcher.GetPattern()());
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

   // Should be present in both the proposal and the response
   const String & upgradeTo = args["Upgrade"];
   if (!upgradeTo.ContainsIgnoreCase("websocket"))
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  Upgrade to websocket not found!  [%s]\n", upgradeTo());
      return B_BAD_DATA;
   }

   // Should be present in both the proposal and the response
   const String & conn = args["Connection"];
   if (!conn.EqualsIgnoreCase("Upgrade"))
   {
      LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  Connection upgrade directive not found!  [%s]\n", conn());
      return B_BAD_DATA;
   }

   switch(_handshakeState)
   {
      case WEBSOCKET_HANDSHAKE_AS_SERVER:
      {
         if (hasGet == false)
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  No GET command found!\n");
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
               LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  No protocol found in [%s] that matches [%s]\n", proto(), _protocolNameMatcher.GetPattern()());
               return B_ACCESS_DENIED;
            }
         }

         const String & key = args["Sec-WebSocket-Key"];
         if (key.HasChars())
         {
            // FogBugz #8945:  go into web-socket mode if that was required
            _httpTextToWrite  = "HTTP/1.1 101 Switching Protocols\r\n";
            _httpTextToWrite += "Upgrade: websocket\r\n";
            _httpTextToWrite += "Connection: Upgrade\r\n";
            _httpTextToWrite += String("Sec-WebSocket-Accept: %1\r\n").Arg(GetWebSocketHashKeyString(key));
            if (foundProto.HasChars()) _httpTextToWrite += String("Sec-WebSocket-Protocol: %1\r\n").Arg(foundProto);
            _httpTextToWrite += "\r\n";

            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  Sec-WebSocket-Key not found\n");
            return B_BAD_DATA;
         }
      }
      break;

      case WEBSOCKET_HANDSHAKE_AS_CLIENT:
      {
         if (hasSwitching == false)
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  No Switching Protocols status found!\n");
            return B_BAD_DATA;
         }

         const String & key = args["Sec-WebSocket-Accept"];
         if (key == GetWebSocketHashKeyString(_clientGeneratedKey))
         {
            _acceptedProtocol = args["Sec-WebSocket-Protocol"];
            return B_NO_ERROR;
         }
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::HandleReceivedHTTPText():  Sec-WebSocket-Accept contained [%s], expected [%s]\n", key(), GetWebSocketHashKeyString(_clientGeneratedKey)());
            return B_BAD_DATA;
         }
      }
      break;

      default:
         // empty
      break;
   }

   return B_LOGIC_ERROR;  // we shouldn't get here under normal circumstances
}

io_status_t WebSocketMessageIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   status_t ret = GetUnrecoverableErrorStatus();

   bool firstTime = true;  // always go at least once, to avoid live-lock
   int32 readBytes = 0;
   while((maxBytes > 0)&&(ret.IsOK())&&((firstTime)||(IsSuggestedTimeSliceExpired() == false)))
   {
      firstTime = false;

      if ((_handshakeState == WEBSOCKET_HANDSHAKE_AS_CLIENT)||(_handshakeState == WEBSOCKET_HANDSHAKE_AS_SERVER))
      {
         char c;
         const io_status_t readRet = GetDataIO()()->Read(&c, 1);  // 1 byte at a time, to avoid any chance of reading past the end of the HTTP section
         if (readRet.IsError()) {ret = readRet.GetStatus(); break;}

         _receivedHTTPText += c;
         if (_receivedHTTPText.EndsWith("\r\n\r\n"))
         {
            ret = HandleReceivedHTTPText();
            if (ret.IsError())
            {
               LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Protocol upgrade failed [%s]\n", ret());
               SetUnrecoverableErrorStatus(ret);
               break;
            }

            _handshakeState = WEBSOCKET_HANDSHAKE_NONE;  // upgrade succeeded, now we can get to real business
            _receivedHTTPText.Clear();
         }
         else if (_receivedHTTPText.Length() > (25*1024))
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  HTTP preamble is too long, aborting\n");
            SetUnrecoverableErrorStatus(B_BAD_DATA);
            return B_BAD_DATA;
         }
      }
      else if (_headerBytesReceived == _headerSize)
      {
         // download payload bytes
         if (_payload() == NULL)
         {
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Can't receive payload, no _payload buffer is present!\n");
            SetUnrecoverableErrorStatus(B_LOGIC_ERROR);
            return B_LOGIC_ERROR;
         }

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
               // Unmask the payload segment
               uint8 * payloadBytes = _payload() ? _payload()->GetBuffer() : NULL;
               const uint32 numPayloadBytes = _payload() ? _payload()->GetNumBytes() : 0;
               if (payloadBytes) for (uint32 i=_firstByteToMask; i<numPayloadBytes; i++) payloadBytes[i] ^= _mask[(i-_firstByteToMask)%sizeof(_mask)];

               _firstByteToMask = numPayloadBytes;

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
                     if (_headerBytes[0] & 0x70)
                     {
                        LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Frame from client had reserved bits set!  %x\n", _headerBytes[0]);
                        SetUnrecoverableErrorStatus(B_BAD_DATA);
                        return B_BAD_DATA;
                     }

                     const bool maskBit = ((_headerBytes[1] & 0x80) != 0);
                     if (maskBit == 0)
                     {
                        LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Frame from client didn't have its mask bit set!\n");
                        SetUnrecoverableErrorStatus(B_BAD_DATA);
                        return B_BAD_DATA;
                     }

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
                     if (payloadSize > (10*1024*1024))
                     {
                        LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Payload size " UINT64_FORMAT_SPEC " is too large!\n", payloadSize);
                        SetUnrecoverableErrorStatus(B_RESOURCE_LIMIT);
                        return B_RESOURCE_LIMIT;
                     }
                     MRETURN_ON_ERROR(InitializeIncomingPayload((uint32) payloadSize, 10, receiver));
                  }
                  break;

                  default:
                     LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway:  Unexpected header size " UINT32_FORMAT_SPEC "!\n", _headerSize);
                     SetUnrecoverableErrorStatus(B_BAD_DATA);
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
   switch(_handshakeState)
   {
      case WEBSOCKET_HANDSHAKE_AS_SERVER:
      case WEBSOCKET_HANDSHAKE_AS_CLIENT:
         return (_numHTTPBytesWritten < _httpTextToWrite.Length());

      case WEBSOCKET_HANDSHAKE_NONE:
         return ((_numHTTPBytesWritten < _httpTextToWrite.Length())||(_outputBytesWritten < _outputBuf.GetNumBytes())||(GetOutgoingMessageQueue().HasItems()));

      default:
         return false;  // gateway is error'd out
   }
}

io_status_t WebSocketMessageIOGateway :: DoOutputImplementation(uint32 maxBytes)
{
   if (_numHTTPBytesWritten < _httpTextToWrite.Length())
   {
      const io_status_t ret = GetDataIO()()->Write(_httpTextToWrite()+_numHTTPBytesWritten, muscleMin((uint32)(_httpTextToWrite.Length()-_numHTTPBytesWritten), maxBytes));
      const int32 numBytesWritten = ret.GetByteCount();
      if (numBytesWritten > 0)
      {
         LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::DoOutputImplementation():  %p wrote " INT32_FORMAT_SPEC " bytes of outgoing HTTP data.\n", this, numBytesWritten);

         _numHTTPBytesWritten += ret.GetByteCount();
         if (_numHTTPBytesWritten >= _httpTextToWrite.Length())
         {
            _numHTTPBytesWritten = 0;
            _httpTextToWrite.Clear();
         }
      }
      return ret;
   }

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
            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::DoOutputImplementation():  %p wrote " INT32_FORMAT_SPEC " bytes of outgoing WebSocket data.\n", this, numBytesWritten);
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
            if ((m->what == WS_OPCODE_PONG)&&(m->HasName(WS_GATEWAY_NAME_SPECIAL)))
            {
               // form a WebSocket-Pong reply
               const ByteBufferRef optBufRef = m->GetFlat<ByteBufferRef>("data");
               const status_t ret = CreateReplyFrame(optBufRef()?optBufRef()->GetBuffer():NULL, optBufRef()?optBufRef()->GetNumBytes():0, WS_OPCODE_PONG);
               (void) q.RemoveHead();
               MRETURN_ON_ERROR(ret);
            }
            else if (_slaveGateway())
            {
               _scratchSlaveBuf.Clear();  // paranoia

               // Have the slave-gateway convert the outgoing Message into a binary-blob for us to send
               ByteBufferDataIO bbdio((DummyByteBufferRef(_scratchSlaveBuf)));  // extra parens to avoid most-vexing-parse
               _slaveGateway()->SetDataIO((DummyDataIORef(bbdio)));             // ditto
               (void) _slaveGateway()->AddOutgoingMessage(q.Head());
               while(_slaveGateway()->DoOutput().GetByteCount() > 0) {/* empty */}
               _slaveGateway()->SetDataIO(DataIORef());

               const status_t ret = CreateReplyFrame(_scratchSlaveBuf.GetBuffer(), _scratchSlaveBuf.GetNumBytes(), WS_OPCODE_BINARY);
               _scratchSlaveBuf.Clear();
               (void) q.RemoveHead();
               MRETURN_ON_ERROR(ret);
            }
            else
            {
               const String * s = m->GetStringPointer(PR_NAME_TEXT_LINE);
               if (s)
               {
                  // form Text reply
                  const status_t ret = CreateReplyFrame((const uint8 *) s->Cstr(), s->Length(), WS_OPCODE_TEXT);  // note that we don't send a NUL terminator byte!
                  (void) m->RemoveData(PR_NAME_TEXT_LINE);
                  MRETURN_ON_ERROR(ret);
               }
               else
               {
                  const void * d;
                  uint32 numBytes;
                  if (m->FindData(PR_NAME_DATA_CHUNKS, B_RAW_TYPE, &d, &numBytes).IsOK())
                  {
                     // form Binary reply
                     const status_t ret = CreateReplyFrame((const uint8 *)d, numBytes, WS_OPCODE_BINARY);
                     (void) m->RemoveData(PR_NAME_DATA_CHUNKS);
                     MRETURN_ON_ERROR(ret);
                  }
                  else (void) q.RemoveHead();
               }
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
   if (payloadSizeBytes == 0)
   {
      // Special case for when there is no payload to receive
      if (_payload() == NULL) _opCode = _headerBytes[0] & 0x0F;
      if (_headerBytes[0] & 0x80) ExecuteReceivedFrame(receiver);
      ResetHeaderReceiveState();
      return B_NO_ERROR;
   }
   else
   {
      memcpy(&_mask, &_headerBytes[maskOffset], sizeof(_mask));
      if (_payload())
      {
         // don't change _opCode if we are merely extending a fragment...
         if (_payload()->AppendBytes(NULL, payloadSizeBytes, false).IsError()) _payload.Reset();
      }
      else
      {
         _opCode  = _headerBytes[0] & 0x0F;
         _payload = GetByteBufferFromPool(payloadSizeBytes);
      }
      return _payload() ? B_NO_ERROR : B_ERROR;
   }
}

void WebSocketMessageIOGateway :: ExecuteReceivedFrame(AbstractGatewayMessageReceiver & receiver)
{
   if (_inputClosed == false)
   {
      const uint8 * payloadBytes = _payload() ? _payload()->GetBuffer()   : NULL;
      const uint32   payloadSize = _payload() ? _payload()->GetNumBytes() : 0;

      switch(_opCode)
      {
         case WS_OPCODE_CONTINUATION:
            if (_inputClosed == false) LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::ExecuteReceivedFrame():   %p received WS_OPCODE_CONTINUATION; continuation handling is not currently implemented\n", this);
         break;

         case WS_OPCODE_TEXT:
         {
            if ((_receivedMsg())&&(_receivedMsg()->what != PR_COMMAND_TEXT_STRINGS)) FlushReceivedMessage(receiver);

            const String text((const char *) payloadBytes, payloadSize);
            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p received text:  [%s]\n", this, text());

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

            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p Received " UINT32_FORMAT_SPEC "-byte binary blob.\n", this, payloadSize);

            if (_slaveGateway())
            {
               ByteBuffer temp; temp.AdoptBuffer(payloadSize, const_cast<uint8 *>(payloadBytes));
               ByteBufferDataIO bbdio((DummyByteBufferRef(temp)));  // extra parens to avoid most-vexing-parse
               _slaveGateway()->SetDataIO((DummyDataIORef(bbdio))); // ditto
               while(_slaveGateway()->DoInput(receiver).GetByteCount() > 0) {/* empty */}  // TODO: handle parse-errors here?
               _slaveGateway()->SetDataIO(DataIORef());
               temp.ReleaseBuffer();
            }
            else
            {
               ByteBufferRef buf = GetByteBufferFromPool(payloadSize, payloadBytes);
               if (buf())
               {
                  if (_receivedMsg() == NULL) _receivedMsg = GetMessageFromPool(PR_COMMAND_RAW_DATA);
                  if (_receivedMsg()) (void) _receivedMsg()->AddFlat(PR_NAME_DATA_CHUNKS, buf);
               }
            }
         }
         break;

         case WS_OPCODE_CLOSE:
            _inputClosed = true;
            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p got WS_OPCODE_CLOSE!\n", this);
         break;

         case WS_OPCODE_PING:
         {
            FlushReceivedMessage(receiver);  // necessary because the receiver might queue some outgoing-data that we want to appear before the pong we send below

            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p received WS_OPCODE_PING\n", this);

            MessageRef pongMsg = GetMessageFromPool(WS_OPCODE_PONG);
            if ((pongMsg())&&(pongMsg()->AddBool(WS_GATEWAY_NAME_SPECIAL, true).IsOK())&&((_payload() == NULL)||(pongMsg()->AddFlat("data", _payload).IsOK()))) (void) AddOutgoingMessage(pongMsg);
         }
         break;

         case WS_OPCODE_PONG:
            LogTime(MUSCLE_LOG_TRACE, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p received WS_OPCODE_PONG!\n", this);
         break;

         default:
            LogTime(MUSCLE_LOG_ERROR, "WebSocketMessageIOGateway::ExecuteReceivedFrame():  %p received unsupported opcode 0x%x!\n", this, _opCode);
         break;
      }
   }

   _payload.Reset();
   _opCode           = 0;
   _payloadBytesRead = 0;
   _firstByteToMask  = 0;
}

};  // end namespace muscle
