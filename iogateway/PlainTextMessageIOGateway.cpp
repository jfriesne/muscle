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
   TCHECKPOINT;

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
         if (msg->FindString(PR_NAME_TEXT_LINE, ++_currentSendLineIndex, _currentSendText) == B_NO_ERROR)
         {
            _currentSendOffset = 0;
            _currentSendText += _eolString;
         }
         else
         {
            _currentSendingMessage.Reset();  // no more text available?  Go to the next message then.
            return DoOutputImplementation(maxBytes);
         }
      }
      if ((msg)&&(_currentSendOffset >= 0)&&(_currentSendOffset < (int32)_currentSendText.Length()))
      {
         // Send as much as we can of the current text line
         const char * bytes = _currentSendText.Cstr() + _currentSendOffset;
         int32 bytesWritten = GetDataIO()()->Write(bytes, muscleMin(_currentSendText.Length()-_currentSendOffset, maxBytes));
              if (bytesWritten < 0) return -1;
         else if (bytesWritten > 0)
         {
            _currentSendOffset += bytesWritten;
            int32 subRet = DoOutputImplementation(maxBytes-bytesWritten);
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
   const int tempBufSize = 2048;
   char buf[tempBufSize];
   int32 bytesRead = GetDataIO()()->Read(buf, muscleMin(maxBytes, (uint32)(sizeof(buf)-1)));
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
         char nextChar = buf[i];
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
      if ((inMsg())&&(inMsg()->AddString(PR_NAME_TEXT_LINE, _incomingText) == B_NO_ERROR))
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

TelnetPlainTextMessageIOGateway :: TelnetPlainTextMessageIOGateway() : _inSubnegotiation(false), _commandBytesLeft(0)
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
   bufLen = output-buf;
}

}; // end namespace muscle
