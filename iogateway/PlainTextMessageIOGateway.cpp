/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/PlainTextMessageIOGateway.h"

namespace muscle {

PlainTextMessageIOGateway ::
PlainTextMessageIOGateway()
   : _eolString("\r\n")
   , _currentSendLineIndex(-1)
   , _currentSendOffset(-1)
   , _prevCharWasCarriageReturn(false)
   , _flushPartialIncomingLines(false)
{
   // empty
}

PlainTextMessageIOGateway ::
~PlainTextMessageIOGateway()
{
   // empty
}

io_status_t
PlainTextMessageIOGateway ::
DoOutputImplementation(uint32 maxBytes)
{
   if (GetMaximumPacketSize() > 0)
   {
      // For the packet-based implementation, we will send one packet per outgoing Message
      // It'll be up to the user to make sure not to put too much text in one Message
      io_status_t totalNumBytesSent;
      MessageRef nextMsg;
      while(((uint32)totalNumBytesSent.GetByteCount() < maxBytes)&&(GetOutgoingMessageQueue().RemoveHead(nextMsg).IsOK()))
      {
         uint32 outBufLen = 1; // 1 for the one extra NUL byte at the end of all the strings (per String::Flatten(), below)
         const String * nextStr;
         for (uint32 i=0; nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &nextStr).IsOK(); i++) outBufLen += (nextStr->Length() + _eolString.Length());

         ByteBufferRef outBuf = GetByteBufferFromPool(outBufLen);
         if (outBuf())
         {
            DataFlattener flat(*outBuf());
            for (uint32 i=0; nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &nextStr).IsOK(); i++)
            {
               flat.WriteBytes(reinterpret_cast<const uint8 *>(nextStr->Cstr()), nextStr->Length());   // Note that we write Length() bytes, NOT FlattenedSize()
               flat.WriteBytes(reinterpret_cast<const uint8 *>(_eolString()),    _eolString.Length()); // bytes (i.e. we don't write NUL terminator byte)
            }

            const uint8 * outBytes      = outBuf()->GetBuffer();
            const uint32 numBytesToSend = outBuf()->GetNumBytes()-1;  // don't sent the NUL terminator byte; receivers shouldn't rely on it anyway

            PacketDataIO * pdio = GetPacketDataIO();  // guaranteed non-NULL
            IPAddressAndPort packetDest;
            const io_status_t subRet = nextMsg()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, packetDest).IsOK()
                                     ? pdio->WriteTo(outBytes, numBytesToSend, packetDest)
                                     : pdio->Write(  outBytes, numBytesToSend);
            MTALLY_BYTES_OR_RETURN_ON_ERROR(totalNumBytesSent, subRet);

            if (subRet.GetByteCount() == 0)
            {
               (void) GetOutgoingMessageQueue().AddHead(nextMsg);  // roll back -- no more buffer space to output to.  We'll try again later to send it, maybe
               break;
            }
         }
         else break;
      }
      return totalNumBytesSent;
   }
   else return DoOutputImplementationAux(maxBytes, 0);  // stream-based implementation is here
}

io_status_t
PlainTextMessageIOGateway ::
DoOutputImplementationAux(uint32 maxBytes, uint32 recurseDepth)
{
   if (recurseDepth >= 1024) return io_status_t();  // We don't want to recurse so deeply that we overflow the stack!

   const Message * msg = _currentSendingMessage();
   if (msg == NULL)
   {
      // try to get the next message from our queue
      (void) GetOutgoingMessageQueue().RemoveHead(_currentSendingMessage);
      msg = _currentSendingMessage();
      _currentSendLineIndex = _currentSendOffset = -1;
   }

   if (msg)
   {
      if ((_currentSendOffset < 0)||(_currentSendOffset >= (int32)_currentSendText.Length()))
      {
         // Try to get the next line of text from our message
         if (msg->FindString(PR_NAME_TEXT_LINE, ++_currentSendLineIndex, _currentSendText).IsOK())
         {
            _currentSendOffset = 0;
            _currentSendText += _eolString;
         }
         else
         {
            _currentSendingMessage.Reset();  // no more text available?  Go to the next message then.
            return DoOutputImplementationAux(maxBytes, recurseDepth+1);
         }
      }

      // At this point it is guaranteed that (_currentSendOffset >= 0) or else we would have returned, above
      if (_currentSendOffset < (int32)_currentSendText.Length())
      {
         // Send as much as we can of the current text line
         const char * bytes = _currentSendText.Cstr() + _currentSendOffset;
         const io_status_t bytesWritten = GetDataIO()() ? GetDataIO()()->Write(bytes, muscleMin(_currentSendText.Length()-_currentSendOffset, maxBytes)) : io_status_t(B_BAD_OBJECT);
         MRETURN_ON_ERROR(bytesWritten);

         if (bytesWritten.GetByteCount() > 0)
         {
            _currentSendOffset += bytesWritten.GetByteCount();
            return bytesWritten + DoOutputImplementationAux(maxBytes-bytesWritten.GetByteCount(), recurseDepth+1);
         }
      }
   }
   return io_status_t();
}

MessageRef
PlainTextMessageIOGateway ::
AddIncomingText(const MessageRef & inMsg, const char * s)
{
   MessageRef ret = inMsg;
   if (ret() == NULL) ret = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
   MRETURN_ON_ERROR(ret);

   if (_incomingText.HasChars())
   {
      MRETURN_ON_ERROR(ret()->AddString(PR_NAME_TEXT_LINE, _incomingText.WithAppend(s)));
      _incomingText.Clear();
   }
   else MRETURN_ON_ERROR(ret()->AddString(PR_NAME_TEXT_LINE, s));

   return ret;
}

io_status_t
PlainTextMessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   const uint32 tempBufSize = 2048;
   char buf[tempBufSize];

   const uint32 mtuSize = GetMaximumPacketSize();
   if (mtuSize > 0)
   {
      // Packet-IO implementation
      char * pbuf  = buf;
      int pbufSize = tempBufSize;

      ByteBufferRef bigBuf;
      if (mtuSize > tempBufSize)
      {
         // Just in case our MTU size is too big for our on-stack buffer
         bigBuf = GetByteBufferFromPool(mtuSize);
         if (bigBuf())
         {
            pbuf     = (char *) bigBuf()->GetBuffer();
            pbufSize = bigBuf()->GetNumBytes();
         }
      }

      io_status_t totalBytesRead;
      while(true)
      {
         IPAddressAndPort sourceIAP;
         const io_status_t bytesRead = GetPacketDataIO()->ReadFrom(pbuf, muscleMin(maxBytes, (uint32)(pbufSize-1)), sourceIAP);
         MTALLY_BYTES_OR_RETURN_ON_ERROR(totalBytesRead, bytesRead);

         if (bytesRead.GetByteCount() > 0)
         {
            const uint32 origBytesRead = bytesRead.GetByteCount();
            uint32 filteredBytesRead   = origBytesRead;
            FilterInputBuffer(pbuf, filteredBytesRead, pbufSize-1);
            totalBytesRead = io_status_t(totalBytesRead.GetByteCount()+((int32)(filteredBytesRead-origBytesRead)));  // adjust totalBytesRead to reflect our filtering, if necessary
            pbuf[filteredBytesRead] = '\0';

            bool prevCharWasCarriageReturn = false;  // deliberately a local var, since UDP packets should be independent of each other
            MessageRef inMsg;  // demand-allocated
            int32 beginAt = 0;
            for (uint32 i=0; i<filteredBytesRead; i++)
            {
               const char nextChar = pbuf[i];
               if ((nextChar == '\r')||(nextChar == '\n'))
               {
                  pbuf[i] = '\0';  // terminate the string here
                  if ((nextChar == '\r')||(prevCharWasCarriageReturn == false)) inMsg = AddIncomingText(inMsg, &pbuf[beginAt]);
                  beginAt = i+1;
               }
               prevCharWasCarriageReturn = (nextChar == '\r');
            }
            if (beginAt < (int32)filteredBytesRead) inMsg = AddIncomingText(inMsg, &pbuf[beginAt]);
            if (inMsg())
            {
               if (GetPacketRemoteLocationTaggingEnabled()) (void) inMsg()->AddFlat(PR_NAME_PACKET_REMOTE_LOCATION, sourceIAP);
               receiver.CallMessageReceivedFromGateway(inMsg);
               inMsg.Reset();
            }
         }
         else
         {
            if ((_flushPartialIncomingLines)&&(HasBufferedIncomingText())) FlushInput(receiver);
            break;
         }
      }
      return totalBytesRead;
   }
   else
   {
      // Stream-IO implementation
      const io_status_t bytesRead = GetDataIO()() ? GetDataIO()()->Read(buf, muscleMin(maxBytes, (uint32)(sizeof(buf)-1))) : io_status_t(B_BAD_OBJECT);
           if (bytesRead.IsError()) FlushInput(receiver);
      else if (bytesRead.GetByteCount() > 0)
      {
         uint32 filteredBytesRead = bytesRead.GetByteCount();
         FilterInputBuffer(buf, filteredBytesRead, sizeof(buf)-1);
         buf[filteredBytesRead] = '\0';

         MessageRef inMsg;  // demand-allocated
         int32 beginAt = 0;
         for (uint32 i=0; i<filteredBytesRead; i++)
         {
            const char nextChar = buf[i];
            if ((nextChar == '\r')||(nextChar == '\n'))
            {
               buf[i] = '\0';  // terminate the string here
               if ((nextChar == '\r')||(_prevCharWasCarriageReturn == false)) inMsg = AddIncomingText(inMsg, &buf[beginAt]);
               beginAt = i+1;
            }
            _prevCharWasCarriageReturn = (nextChar == '\r');
         }
         if (beginAt < (int32)filteredBytesRead)
         {
            if (_flushPartialIncomingLines) inMsg = AddIncomingText(inMsg, &buf[beginAt]);
                                       else _incomingText += &buf[beginAt];
         }
         if (inMsg()) receiver.CallMessageReceivedFromGateway(inMsg);
      }
      else if ((_flushPartialIncomingLines)&&(HasBufferedIncomingText())) FlushInput(receiver);

      return bytesRead;
   }
}

void
PlainTextMessageIOGateway ::
FilterInputBuffer(char * /*buf*/, uint32 & /*bufLen*/, uint32 /*maxLen*/)
{
   // empty
}

void
PlainTextMessageIOGateway :: FlushInput(AbstractGatewayMessageReceiver & receiver)
{
   if (_incomingText.HasChars())
   {
      MessageRef inMsg = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
      if ((inMsg())&&(inMsg()->AddString(PR_NAME_TEXT_LINE, _incomingText).IsOK()))
      {
         _incomingText.Clear();
         receiver.CallMessageReceivedFromGateway(inMsg);
      }
   }
}

bool
PlainTextMessageIOGateway ::
HasBytesToOutput() const
{
   return ((_currentSendingMessage() != NULL)||(GetOutgoingMessageQueue().HasItems()));
}

void
PlainTextMessageIOGateway ::
Reset()
{
   TCHECKPOINT;

   AbstractMessageIOGateway::Reset();
   _currentSendingMessage.Reset();
   _currentSendText.Clear();
   _prevCharWasCarriageReturn = false;
   _incomingText.Clear();
}

TelnetPlainTextMessageIOGateway :: TelnetPlainTextMessageIOGateway()
   : _inSubnegotiation(false)
   , _commandBytesLeft(0)
{
   // empty
}

TelnetPlainTextMessageIOGateway :: ~TelnetPlainTextMessageIOGateway()
{
   // empty
}

void
TelnetPlainTextMessageIOGateway ::
FilterInputBuffer(char * buf, uint32 & bufLen, uint32 /*maxLen*/)
{
   // Based on the document at http://support.microsoft.com/kb/231866
   static const unsigned char IAC = 255;
   static const unsigned char SB  = 250;
   static const unsigned char SE  = 240;

   char * output = buf;
   for (uint32 i=0; i<bufLen; i++)
   {
      unsigned char c = buf[i];
      bool keepChar = ((c & 0x80) == 0);
      switch(c)
      {
         case IAC: _commandBytesLeft = 3;                            break;
         case SB:  _inSubnegotiation = true;                         break;
         case SE:  _inSubnegotiation = false; _commandBytesLeft = 0; break;
         default:  /* empty */                                       break;
      }
      if (_commandBytesLeft > 0) {--_commandBytesLeft; keepChar = false;}
      if (_inSubnegotiation) keepChar = false;
      if (keepChar) *output++ = c;  // strip out any telnet control/escape codes
   }
   bufLen = (uint32) (output-buf);
}

} // end namespace muscle
