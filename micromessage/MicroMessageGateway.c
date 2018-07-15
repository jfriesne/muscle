#include <string.h>  // for memmove()
#include "micromessage/MicroMessageGateway.h"

#ifdef __cplusplus
extern "C" {
#endif

static const uint32 _MUSCLE_MESSAGE_ENCODING_DEFAULT = 1164862256; /* 'Enc0' -- vanilla encoding */
static const uint32 GATEWAY_HEADER_SIZE = 2*sizeof(uint32);
static const uint32 MESSAGE_HEADER_SIZE = 3*sizeof(uint32);

void UGGatewayInitialize(UMessageGateway * gw, uint8 * inputBuffer, uint32 numInputBufferBytes, uint8 * outputBuffer, uint32 numOutputBufferBytes)
{
   gw->_inputBuffer          = inputBuffer;
   gw->_inputBufferSize      = numInputBufferBytes;
   gw->_numValidInputBytes   = 0;
   gw->_numInputBytesToRead  = GATEWAY_HEADER_SIZE;
   gw->_outputBuffer         = outputBuffer;
   gw->_outputBufferSize     = numOutputBufferBytes;
   gw->_firstValidOutputByte = gw->_outputBuffer;
   gw->_numValidOutputBytes  = 0;
   gw->_preparingOutgoingMessage = UFalse;
}

static uint32 UGGetAvailableBytesCount(const UMessageGateway * gw)
{
   return (gw->_outputBuffer+gw->_outputBufferSize)-(gw->_firstValidOutputByte+gw->_numValidOutputBytes);
}

UMessage UGGetOutgoingMessage(UMessageGateway * gw, uint32 whatCode)
{
   UMessage ret;
   if (gw->_preparingOutgoingMessage)
   {
      printf("UGGetOutgoingMessage:  Error, can't return a second UGGetOutgoingMessage() when a first one is still in use!  Did you forget to call UMOutgoingMessagePrepared() on the previous UMessage?\n");
      (void) UMInitializeToInvalid(&ret);
      return ret;
   }

   /* First, see if it's worth moving the outgoing buffer data to the front, in order to gain space */
   uint32 bytesAvail = UGGetAvailableBytesCount(gw);
   if (bytesAvail < (gw->_outputBufferSize/4))
   {
      /** Move the already-buffered outgoing-data up to the top of the buffer, to free up more space for our new UMessage */
      memmove(gw->_outputBuffer, gw->_firstValidOutputByte, gw->_numValidOutputBytes);
      gw->_firstValidOutputByte = gw->_outputBuffer;
      bytesAvail = UGGetAvailableBytesCount(gw);
   }

   uint8 * nextAvailByte = gw->_firstValidOutputByte+gw->_numValidOutputBytes;
   if (bytesAvail >= (GATEWAY_HEADER_SIZE+MESSAGE_HEADER_SIZE))
   {
      (void) UMInitializeToEmptyMessage(&ret, nextAvailByte+GATEWAY_HEADER_SIZE, bytesAvail-GATEWAY_HEADER_SIZE, whatCode);
      gw->_preparingOutgoingMessage = UTrue;
   }
   else (void) UMInitializeToInvalid(&ret);

   return ret;
}

static inline void UMWriteInt32(void * ptr, uint32 val)
{
   const uint32 le32 = B_HOST_TO_LENDIAN_INT32(val);
   memcpy(ptr, &le32, sizeof(le32));
}

static inline uint32 UMReadInt32(const void * ptr)
{
   uint32 le32; memcpy(&le32, ptr, sizeof(le32));
   return B_LENDIAN_TO_HOST_INT32(le32);
}

void UGOutgoingMessagePrepared(UMessageGateway * gw, const UMessage * msg)
{
   if (gw->_preparingOutgoingMessage)
   {
      uint8 * nextAvailByte = gw->_firstValidOutputByte+gw->_numValidOutputBytes;
      uint8 * expected      = msg->_buffer-(2*sizeof(uint32));
      if (nextAvailByte == expected)
      {
         const uint32 msgSize = UMGetFlattenedSize(msg);

         UMWriteInt32(nextAvailByte, msgSize);                          
         nextAvailByte += sizeof(uint32);

         UMWriteInt32(nextAvailByte, _MUSCLE_MESSAGE_ENCODING_DEFAULT); 
#ifdef DISABLED_TO_KEEP_CLANG_STATIC_ANALYZER_HAPPY
         nextAvailByte += sizeof(uint32);
#endif

         /* The UMessage data is already in the right place, as the UMessage wrote it there directly */
         gw->_numValidOutputBytes += (GATEWAY_HEADER_SIZE+msgSize);
         gw->_preparingOutgoingMessage = UFalse;
      }
      else printf("UGOutgoingMessagePrepared():  The UMessage passed in is not the expected one!  %p vs %p\n", nextAvailByte, expected);
   }
   else printf("UGOutgoingMessagePrepared():  Error, called when there was not any UMessage being prepared!?\n");
}

void UGOutgoingMessageCancelled(UMessageGateway * gw, const UMessage * msg)
{
   gw->_preparingOutgoingMessage = UFalse;
   (void) msg;  /* avoid compiler warning */
}

UBool UGHasBytesToOutput(const UMessageGateway * gw)
{
   return (gw->_numValidOutputBytes > 0);
}

int32 UGDoOutput(UMessageGateway * gw, uint32 maxBytes, UGSendFunc sendFunc, void * arg)
{
   int32 totalSent = 0;
   while((gw->_numValidOutputBytes > 0)&&((uint32)totalSent < maxBytes))
   {
      int32 bytesToSend = gw->_numValidOutputBytes;
      if (bytesToSend > maxBytes) bytesToSend = maxBytes;
      
      const int32 bytesSent = sendFunc(gw->_firstValidOutputByte, bytesToSend, arg);
      if (bytesSent < 0) return -1;  /* error! */
      else
      {
         totalSent += bytesSent;
         gw->_numValidOutputBytes -= bytesSent;
         gw->_firstValidOutputByte = (gw->_numValidOutputBytes == 0) ? gw->_outputBuffer : (gw->_firstValidOutputByte+bytesSent);
      }
      if (bytesSent < bytesToSend) break;  /* short write indicates that the output buffer is full for now */
   }
   return totalSent;
}

int32 UGDoInput(UMessageGateway * gw, uint32 maxBytes, UGReceiveFunc recvFunc, void * arg, UMessage * optRetMsg)
{
   if (optRetMsg) UMInitializeToInvalid(optRetMsg);

   int32 totalRecvd = 0;
   while(true) 
   {
      int32 bytesReceived;
      uint32 bytesToRecv = (gw->_numInputBytesToRead-gw->_numValidInputBytes);
      if (bytesToRecv > maxBytes) bytesToRecv = maxBytes;

      bytesReceived = recvFunc(gw->_inputBuffer+gw->_numValidInputBytes, bytesToRecv, arg);
      if (bytesReceived < 0) return -1;  /* error */
      else
      {
         totalRecvd += bytesReceived;
         gw->_numValidInputBytes += bytesReceived;
         if (gw->_numValidInputBytes == gw->_numInputBytesToRead)
         {
            if (gw->_numValidInputBytes == GATEWAY_HEADER_SIZE)  /* guaranteed never to be a real UMessage, since the gateway header size is less than the legal minimum UMessage size */
            {
               /* We have our fixed-size headers, now prepare to receive the actual Message body! */
               const uint32 bodySize = UMReadInt32(gw->_inputBuffer);
               const uint32 magic    = UMReadInt32(gw->_inputBuffer+sizeof(uint32));
               if ((bodySize < MESSAGE_HEADER_SIZE)||(bodySize > gw->_inputBufferSize)||(magic != _MUSCLE_MESSAGE_ENCODING_DEFAULT)) return -1;
               gw->_numValidInputBytes  = 0;
               gw->_numInputBytesToRead = bodySize;
            }
            else
            {
               if (optRetMsg) (void) UMInitializeWithExistingData(optRetMsg, gw->_inputBuffer, gw->_numValidInputBytes);
               gw->_numValidInputBytes = 0;
               gw->_numInputBytesToRead = GATEWAY_HEADER_SIZE;   /* get ready for the next header */
               break;  /* Otherwise the caller would never see his UMessage */
            }
         }
      }
      if (bytesReceived < bytesToRecv) break;  /* short read indicates that the input buffer is empty for now */
   }
   return totalRecvd;
}

#ifdef __cplusplus
};
#endif
