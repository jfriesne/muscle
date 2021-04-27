/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "iogateway/PlainTextMessageIOGateway.h"

namespace muscle {

PlainTextMessageIOGateway ::
PlainTextMessageIOGateway() : _eolString("\r\n"), _prevCharWasCarriageReturn(false), _flushPartialIncomingLines(false)
{
   // empty
}

PlainTextMessageIOGateway ::
~PlainTextMessageIOGateway()
{
   // empty
}

int32
PlainTextMessageIOGateway ::
DoOutputImplementation(uint32 maxBytes)
{
   if (GetMaximumPacketSize() > 0)
   {
      // For the packet-based implementation, we will send one packet per outgoing Message
      // It'll be up to the user to make sure not to put too much text in one Message
      uint32 totalNumBytesSent = 0;
      MessageRef nextMsg;
      while((totalNumBytesSent < maxBytes)&&(GetOutgoingMessageQueue().RemoveHead(nextMsg).IsOK()))
      {
         uint32 outBufLen = 1; // 1 for the one extra NUL byte at the end of all the strings (per String::Flatten(), below)
         const String * nextStr;
         for (uint32 i=0; nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &nextStr).IsOK(); i++) outBufLen += (nextStr->Length() + _eolString.Length());

         ByteBufferRef outBuf = GetByteBufferFromPool(outBufLen);
         if (outBuf())
         {
            uint8 * b = outBuf()->GetBuffer();
            for (uint32 i=0; nextMsg()->FindString(PR_NAME_TEXT_LINE, i, &nextStr).IsOK(); i++) 
            {
               nextStr->Flatten(b);   b += nextStr->Length();   // Advance by Length() instead of FlattenedSize()
               _eolString.Flatten(b); b += _eolString.Length(); // to avoid NUL bytes inside our outBuf
            }

            const uint8 * outBytes      = outBuf()->GetBuffer();
            const uint32 numBytesToSend = outBuf()->GetNumBytes()-1;  // don't sent the NUL terminator byte; receivers shouldn't rely on it anyway

            PacketDataIO * pdio = GetPacketDataIO();  // guaranteed non-NULL
            IPAddressAndPort packetDest;
            const int32 numBytesSent = (nextMsg()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, packetDest).IsOK())
                                     ? pdio->WriteTo(outBytes, numBytesToSend, packetDest)
                                     : pdio->Write(  outBytes, numBytesToSend);
                 if (numBytesSent > 0) totalNumBytesSent += numBytesSent;
            else if (numBytesSent < 0) return (totalNumBytesSent > 0) ? totalNumBytesSent : -1;
            else
            {
               (void) GetOutgoingMessageQueue().AddHead(nextMsg);  // roll back -- we'll try again later to send it, maybe
               break;
            }
         }
         else break;
      }
      return totalNumBytesSent;
   }
   else return DoOutputImplementationAux(maxBytes, 0);  // stream-based implementation is here
}

int32
PlainTextMessageIOGateway ::
DoOutputImplementationAux(uint32 maxBytes, uint32 recurseDepth)
{
   if (recurseDepth >= 1024) return 0;  // We don't want to recurse so deeply that we overflow the stack!

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
      if ((_currentSendOffset >= 0)&&(_currentSendOffset < (int32)_currentSendText.Length()))
      {
         // Send as much as we can of the current text line
         const char * bytes = _currentSendText.Cstr() + _currentSendOffset;
         const int32 bytesWritten = GetDataIO()() ? GetDataIO()()->Write(bytes, muscleMin(_currentSendText.Length()-_currentSendOffset, maxBytes)) : -1;
              if (bytesWritten < 0) return -1;
         else if (bytesWritten > 0)
         {
            _currentSendOffset += bytesWritten;
            const int32 subRet = DoOutputImplementationAux(maxBytes-bytesWritten, recurseDepth+1);
            return (subRet >= 0) ? subRet+bytesWritten : -1;
         }
      }
   }
   return 0;
}

MessageRef
PlainTextMessageIOGateway ::
AddIncomingText(const MessageRef & inMsg, const char * s)
{
   MessageRef ret = inMsg;
   if (ret() == NULL) ret = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
   if (ret())
   {
      if (_incomingText.HasChars())
      {
         (void) ret()->AddString(PR_NAME_TEXT_LINE, _incomingText.Append(s));
         _incomingText.Clear();
      }
      else (void) ret()->AddString(PR_NAME_TEXT_LINE, s);
   }
   return ret;
}

int32
PlainTextMessageIOGateway ::
DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   TCHECKPOINT;

   int32 ret = 0;
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

      while(true)
      {
         IPAddressAndPort sourceIAP;
         const int32 bytesRead = GetPacketDataIO()->ReadFrom(pbuf, muscleMin(maxBytes, (uint32)(pbufSize-1)), sourceIAP);
              if (bytesRead < 0) return (ret > 0) ? ret : -1;
         else if (bytesRead > 0)
         {
            uint32 filteredBytesRead = bytesRead;
            FilterInputBuffer(pbuf, filteredBytesRead, pbufSize-1);
            ret += filteredBytesRead;
            pbuf[filteredBytesRead] = '\0';

            bool prevCharWasCarriageReturn = false;  // deliberately a local var, since UDP packets should be independent of each other
            MessageRef inMsg;  // demand-allocated
            int32 beginAt = 0;
            for (uint32 i=0; i<filteredBytesRead; i++)
            {
               char nextChar = pbuf[i];
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
            ret += bytesRead;
         }
         else return ret;
      }
   }
   else
   {
      // Stream-IO implementation
      const int32 bytesRead = GetDataIO()() ? GetDataIO()()->Read(buf, muscleMin(maxBytes, (uint32)(sizeof(buf)-1))) : -1;
      if (bytesRead < 0)
      {
         FlushInput(receiver);
         return -1;
      }
      if (bytesRead > 0)
      {
         uint32 filteredBytesRead = bytesRead;
         FilterInputBuffer(buf, filteredBytesRead, sizeof(buf)-1);
         ret += filteredBytesRead;
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
   }
   return ret;
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
      }
      if (_commandBytesLeft > 0) {--_commandBytesLeft; keepChar = false;}
      if (_inSubnegotiation) keepChar = false; 
      if (keepChar) *output++ = c;  // strip out any telnet control/escape codes
   }
   bufLen = (uint32) (output-buf);
}

} // end namespace muscle
