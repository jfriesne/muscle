/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "iogateway/SLIPFramedDataMessageIOGateway.h"

namespace muscle {

SLIPFramedDataMessageIOGateway :: SLIPFramedDataMessageIOGateway() : _lastReceivedCharWasEscape(false)
{
   // empty
}

SLIPFramedDataMessageIOGateway :: ~SLIPFramedDataMessageIOGateway()
{
   // empty
}

int32 SLIPFramedDataMessageIOGateway :: DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes)
{
   int32 ret = RawDataMessageIOGateway::DoInputImplementation(*this, maxBytes);
   if (_pendingMessage()) 
   {
      MessageRef msg; 
      muscleSwap(_pendingMessage, msg);  // paranoia wrt re-entrancy
      receiver.CallMessageReceivedFromGateway(msg); 
   }
   return ret;
}

void SLIPFramedDataMessageIOGateway :: Reset()
{
   AbstractMessageIOGateway::Reset();
   _lastReceivedCharWasEscape = false;
   _pendingBuffer.Reset();
   _pendingMessage.Reset();
}

static const uint8 SLIP_END        = 0300;  // yes, octal constants
static const uint8 SLIP_ESC        = 0333;  // straight out of the RFC
static const uint8 SLIP_ESCAPE_END = 0334;
static const uint8 SLIP_ESCAPE_ESC = 0335;

static ByteBufferRef SLIPEncodeBytes(const uint8 * bytes, uint32 numBytes)
{
   // First, calculate how many bytes the SLIP'd buffer will need to hold
   uint32 numSLIPBytes = 2;  // 1 for the SLIP_END byte at the beginning, and one for the SLIP_END at the end.
   for (uint32 i=0; i<numBytes; i++)
   {
      switch(bytes[i])
      {
         case SLIP_END: case SLIP_ESC: numSLIPBytes += 2; break;  // these get escaped as two characters
         default:                      numSLIPBytes += 1; break;
      }
   }

   ByteBufferRef bufRef = GetByteBufferFromPool(numSLIPBytes);
   if (bufRef())
   {
      uint8 * out = bufRef()->GetBuffer();
      *out++ = SLIP_END;
      for (uint32 i=0; i<numBytes; i++)
      {
         switch(bytes[i])
         {
            case SLIP_END: 
               *out++ = SLIP_ESC;
               *out++ = SLIP_ESCAPE_END;
            break;

            case SLIP_ESC:
               *out++ = SLIP_ESC;
               *out++ = SLIP_ESCAPE_ESC;
            break;

            default:
               *out++ = bytes[i];
            break;
         }
      }
      *out++ = SLIP_END;
   }
   return bufRef;
}

MessageRef SLIPFramedDataMessageIOGateway :: PopNextOutgoingMessage()
{
   MessageRef msg = RawDataMessageIOGateway::PopNextOutgoingMessage();
   if (msg() == NULL) return MessageRef();

   MessageRef slipMsg = GetLightweightCopyOfMessageFromPool(*msg());
   if (slipMsg() == NULL) return MessageRef();

   (void) slipMsg()->RemoveName(PR_NAME_DATA_CHUNKS);  // make sure we don't modify the field object in (msg)

   // slipMsg will be like (msg), except that we've slip-encoded each data item
   const uint8 * buf;
   uint32 numBytes;
   for (int32 i=0; msg()->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, i, (const void **) &buf, &numBytes) == B_NO_ERROR; i++)
   {
      ByteBufferRef slipData = SLIPEncodeBytes(buf, numBytes);
      if ((slipData()==NULL)||(slipMsg()->AddFlat(PR_NAME_DATA_CHUNKS, slipData) != B_NO_ERROR)) return MessageRef();
   }

   return slipMsg;
}

void SLIPFramedDataMessageIOGateway :: AddPendingByte(uint8 b)
{
   if (_pendingBuffer() == NULL) 
   {
      _pendingBuffer = GetByteBufferFromPool(256);  // An arbitrary initial size, since I can't think of any good heuristic
      if (_pendingBuffer()) _pendingBuffer()->Clear(false);  // but make it look empty
                       else return;  // out of memory?
   }
   (void) _pendingBuffer()->AppendByte(b);
}

// This proxy implementation receives raw data from the superclass and SLIP-decodes it, building up a Message full of decoded data to send to our own caller later.
void SLIPFramedDataMessageIOGateway :: MessageReceivedFromGateway(const MessageRef & msg, void * /*userData*/)
{
   const uint8 * buf;
   uint32 numBytes;
   for (int32 x=0; msg()->FindData(PR_NAME_DATA_CHUNKS, B_ANY_TYPE, x, (const void **) &buf, &numBytes) == B_NO_ERROR; x++)
   {
      for (uint32 i=0; i<numBytes; i++)
      {
         uint8 b = buf[i];
         if (_lastReceivedCharWasEscape)
         {
            switch(b)
            {
               case SLIP_ESCAPE_END:  AddPendingByte(SLIP_END); break;
               case SLIP_ESCAPE_ESC:  AddPendingByte(SLIP_ESC); break;
               default:               AddPendingByte(b);        break;  // protocol violation, but we'll just let the byte through since that is what the reference implementation does
            }
            _lastReceivedCharWasEscape = false;
         }
         else
         { 
            switch(b)
            {
               case SLIP_END:
                  if ((_pendingBuffer())&&(_pendingBuffer()->GetNumBytes() > 0))
                  {
                     if (_pendingMessage() == NULL) _pendingMessage = GetMessageFromPool(msg()->what);
                     if (_pendingMessage()) (void) _pendingMessage()->AddFlat(PR_NAME_DATA_CHUNKS, _pendingBuffer);
                     _pendingBuffer.Reset();
                  }
               break;

               case SLIP_ESC:
                  // do nothing
               break;

               default:
                  AddPendingByte(b);
               break;
            } 
            _lastReceivedCharWasEscape = (b==SLIP_ESC);
         }
      }
   }
}

}; // end namespace muscle
