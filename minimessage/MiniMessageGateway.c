#include <string.h>  // for memcpy()
#include "minimessage/MiniMessageGateway.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _MMessageGateway {
   MByteBuffer * _curInput;   /* We receive data into this buffer           */
   MByteBuffer * _curOutput;  /* The head of our output-buffers linked list */
   MByteBuffer * _outputTail; /* The tail of our output-buffers linked list */ 
   uint32 _curInputPos;       /* Index of next byte to receive in _curInput */
   uint32 _maxInputPos;       /* Index of last byte to receive in _curInput */
   uint32 _curOutputPos;      /* Index of next byte to send in _curOutput   */
};

static inline MByteBuffer * GetNextPointer(const MByteBuffer * buf)
{
   return *((MByteBuffer **)(&buf->bytes));
}

static inline void SetNextPointer(MByteBuffer * buf, const MByteBuffer * next)
{
   *((const MByteBuffer **)(&buf->bytes)) = next;
}

MMessageGateway * MGAllocMessageGateway()
{
   MMessageGateway * ret = MMalloc(sizeof(MMessageGateway));
   if (ret)
   {
      ret->_outputTail   = NULL;
      ret->_curInputPos  = 0;                        /* input buffer doesn't have a next pointer */
      ret->_curOutputPos = sizeof(MByteBuffer *); /* start sending after the next pointer */
      ret->_maxInputPos  = (2*sizeof(uint32));       /* start by reading the fixed-size header fields */
      ret->_curOutput    = NULL;
      ret->_curInput     = MBAllocByteBuffer(ret->_maxInputPos, false);
      if (ret->_curInput == NULL)
      {
         MFree(ret);
         ret = NULL;
      }
   }
   return ret;
}

void MGFreeMessageGateway(MMessageGateway * gw)
{
   if (gw)
   {
      MBFreeByteBuffer(gw->_curInput);
      while(gw->_curOutput)
      {
         MByteBuffer * next = GetNextPointer(gw->_curOutput);
         MBFreeByteBuffer(gw->_curOutput);
         gw->_curOutput = next;
      }
      MFree(gw);
   }
}

static const uint32 _MUSCLE_MESSAGE_ENCODING_DEFAULT = 1164862256; /* 'Enc0' -- vanilla encoding */

status_t MGAddOutgoingMessage(MMessageGateway * gw, const MMessage * msg)
{
   uint32 flatSize = MMGetFlattenedSize(msg);
   MByteBuffer * buf = MBAllocByteBuffer(sizeof(MMessage *) + (2*sizeof(uint32)) + flatSize, false);
   if (buf) 
   {
      uint32 * h = (uint32 *)(((MMessage **)(&buf->bytes))+1);

      SetNextPointer(buf, NULL);
      h[0] = B_HOST_TO_LENDIAN_INT32(flatSize);
      h[1] = B_HOST_TO_LENDIAN_INT32(_MUSCLE_MESSAGE_ENCODING_DEFAULT);
      MMFlattenMessage(msg, (uint8 *) &h[2]);

      if (gw->_outputTail)
      {
         SetNextPointer(gw->_outputTail, buf);
         gw->_outputTail = buf;
      }
      else gw->_curOutput = gw->_outputTail = buf;
       
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

MBool MGHasBytesToOutput(const MMessageGateway * gw)
{
   return (gw->_curOutput != NULL);
}

int32 MGDoOutput(MMessageGateway * gw, uint32 maxBytes, MGSendFunc sendFunc, void * arg)
{
   int32 totalSent = 0;
   while(gw->_curOutput != NULL)
   {
      int32 bytesSent;

      uint32 bytesToSend = gw->_curOutput->numBytes - gw->_curOutputPos;
      if (bytesToSend > maxBytes) bytesToSend = maxBytes;
      
      bytesSent = sendFunc((&gw->_curOutput->bytes)+(gw->_curOutputPos), bytesToSend, arg);
      if (bytesSent < 0) return -1;  /* error! */
      else
      {
         totalSent += bytesSent;
         maxBytes  -= bytesSent;
         gw->_curOutputPos += bytesSent;
         if (gw->_curOutputPos == gw->_curOutput->numBytes)
         {
            MByteBuffer * next = GetNextPointer(gw->_curOutput);
            if (next == NULL) gw->_outputTail = NULL;
            MBFreeByteBuffer(gw->_curOutput);
            gw->_curOutput = next;
            gw->_curOutputPos = sizeof(MByteBuffer *);  /* send all data after the next-pointer */
         }
      }
      if (bytesSent < bytesToSend) break;  /* short write indicates that the output buffer is full for now */
   }
   return totalSent;
}

int32 MGDoInput(MMessageGateway * gw, uint32 maxBytes, MGReceiveFunc recvFunc, void * arg, MMessage ** optRetMsg)
{
   int32 totalRecvd = 0;

   if (optRetMsg) *optRetMsg = NULL;
   while(true) 
   {
      int32 bytesReceived;
      uint32 bytesToRecv = gw->_maxInputPos - gw->_curInputPos;
      if (bytesToRecv > maxBytes) bytesToRecv = maxBytes;

      bytesReceived = recvFunc((&gw->_curInput->bytes)+gw->_curInputPos, bytesToRecv, arg);
      if (bytesReceived < 0) return -1;  /* error */
      else
      {
         totalRecvd += bytesReceived;
         maxBytes   -= bytesReceived;
         gw->_curInputPos += bytesReceived;
         if (gw->_curInputPos == gw->_maxInputPos)
         {
            if (gw->_curInputPos > (2*sizeof(uint32)))
            {
               /* We have received the Message body and we are ready to unflatten! */
               MMessage * msg = MMAllocMessage(0);
               if (msg == NULL) return -1;  /* out of memory */

               if (MMUnflattenMessage(msg, (&gw->_curInput->bytes)+(2*sizeof(uint32)), gw->_maxInputPos-(2*sizeof(uint32))) == B_NO_ERROR)
               {
                  /* set parser up for the next go-round */
                  gw->_curInputPos = 0;
                  gw->_maxInputPos = 2*(sizeof(uint32));

                  /* For input buffers over 64KB, it's better to reclaim the extra space than avoid reallocating */
                  if (gw->_curInput->numBytes > 64*1024)
                  {
                     MByteBuffer * smallBuf = MBAllocByteBuffer(64*1024, false);
                     if (smallBuf == NULL) 
                     {
                        MMFreeMessage(msg);
                        return -1;
                     }

                     MBFreeByteBuffer(gw->_curInput);
                     gw->_curInput = smallBuf;
                  }

                  if (optRetMsg)
                  {
                     *optRetMsg = msg;
                     break;  /* If we have a Message for the user, return it to him now */
                  }
                  else MMFreeMessage(msg);  /* User doesn't want it, so just throw it away */
               }
               else
               {
                  MMFreeMessage(msg);
                  return -1;   /* parse error! */
               }
            }
            else
            {
               /* We have our fixed-size headers, now prepare to receive the actual Message body! */
               const uint32 * h = (const uint32 *)(&gw->_curInput->bytes);
               uint32 bodySize = B_LENDIAN_TO_HOST_INT32(h[0]);
               if ((bodySize == 0)||(B_LENDIAN_TO_HOST_INT32(h[1]) != _MUSCLE_MESSAGE_ENCODING_DEFAULT)) return -1;  /* unsupported encoding! */

               bodySize += (2*sizeof(uint32));  /* we'll include the fixed headers in our body buffer too */
               if (bodySize > gw->_curInput->numBytes)
               {
                  /** Gotta trade up for a larger buffer, so that all our data will fit! */
                  MByteBuffer * bigBuf = MBAllocByteBuffer(2*bodySize, false);  /* might as well alloc some extra space too */
                  if (bigBuf == NULL) return -1;  /* out of memory! */

                  memcpy(&bigBuf->bytes, &gw->_curInput->bytes, gw->_curInputPos);  /* not strictly necessary, but for form's sake... */
                  MBFreeByteBuffer(gw->_curInput);  /* dispose of the too-small old input buffer */
                  gw->_curInput = bigBuf;
               }
               gw->_maxInputPos = bodySize;
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
