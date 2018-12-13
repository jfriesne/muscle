#ifndef MicroMessageGateway_h
#define MicroMessageGateway_h

#include "micromessage/MicroMessage.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup micromessagegateway The MicroMessageGateway C function API 
 *  These functions are all defined in MicroMessageGateway(.c,.h), and are stand-alone
 *  C functions that provide functionality similar to that of the C++
 *  MessageIOGateway class.
 *  @{
 */

#ifndef DOXYGEN_SHOULD_IGNORE_THIS

/** This struct represents the state of the gateway we use to flatten/send and receive/unflatten messages.
  * Fields in this struct should be considered private; use the UG* functions in MicroMessageGateway.h
  * rather than accessing these fields directly.
  */
typedef struct _UMessageGateway {
   uint8 * _inputBuffer;
   uint32 _inputBufferSize;
   uint32 _numValidInputBytes;
   uint32 _numInputBytesToRead;
   uint8 * _outputBuffer;
   uint32 _outputBufferSize;
   uint8 * _firstValidOutputByte;
   uint32 _numValidOutputBytes;
   UBool _preparingOutgoingMessage;
} UMessageGateway;

#endif

/** Initializes the specified UMessageGateway struct to point to the specified memory buffers.
  * @param gateway the UMessageGateway to initialize.
  * @param inputBuffer An array of bytes to use for receiving incoming UMessage data.  This buffer should be at least 8 bytes larger than the
  *                    largest UMessage you expect to receive -- if a UMessage is received that is too large, the stream will be broken.
  *                    This array needs to remain valid and accessible for the lifetime of the UMessageGateway.
  * @param numInputBufferBytes The number of bytes pointed to by (inputBuffer).
  * @param outputBuffer An array of bytes to use for storing outgoing UMessage data.  This buffer should be at least 8 bytes larger than the
  *                     largest UMessage you expect to send, and larger still if you expect to queue up multiple outgoing UMessages at once.
  *                     The maximum size of outgoing UMessages will be limited to the amount of contiguous space available in this buffer.
  *                     This array needs to remain valid and accessible for the lifetime of the UMessageGateway.
  * @param numOutputBufferBytes The number of bytes pointed to by (outputBuffer).
  */
void UGGatewayInitialize(UMessageGateway * gateway, uint8 * inputBuffer, uint32 numInputBufferBytes, uint8 * outputBuffer, uint32 numOutputBufferBytes);

/** Typedef for a callback function that knows how to read data from a buffer and send it
  * out to (a file, the network, a serial line, wherever).
  * @param buf The buffer to read bytes from.
  * @param numBytes The number of bytes available for reading at (buf)
  * @param arg This is a user-specified value; it will be the same as the value passed in to UMDoOutput().
  * @returns The number of bytes actually read from (buf), or a negative value if there was a critical error (e.g. disconnected socket).
  */
typedef int32 (*UGSendFunc)(const uint8 * buf, uint32 numBytes, void * arg);

/** Typedef for a callback function that knows how to read data from 
  * (a file, the network, a serial line, wherever) and write it into a supplied buffer.
  * @param buf The buffer to write bytes to.
  * @param numBytes The number of bytes available for writing at (buf)
  * @param arg This is a user-specified value; it will be the same as the value passed in to UMDoInput().
  * @returns The number of bytes actually written into (buf), or a negative value if there was a critical error (e.g. disconnected socket).
  */
typedef int32 (*UGReceiveFunc)(uint8 * buf, uint32 numBytes, void * arg);

/** When you want to send a UMessage out to the network via your UMessageGatweay, you can request a UMessage to send
  * via this function.
  * @param gateway the UMessageGateway to request a UMessage from.
  * @param whatCode the uint32 'what code' you what the UMessage to have
  * @returns On success a valid UMessage object is returned that you can add data to.  When you are done adding data to the
  *          UMessage, be sure to call UGOutgoingMessagePrepared() to let the gateway know that it is now okay to start sending
  *          the UMessage bytes.  It's an error to call UGGetOutgoingMessage() again before calling UGOutgoingMessagePrepared().
  */
UMessage UGGetOutgoingMessage(UMessageGateway * gateway, uint32 whatCode);

/** Call this when you have finished adding data to a UMessage that was returned to you via UGGetOutgoingMessage().
  * This function tells the UMessageGateway that it is now okay to start sending the UMessage.
  * @param gateway The same gateway object that you previously passed to UGGetOutgoingMessage().
  * @param msg A pointer to the UMessage that you previously received from UGGetOutgoingMessage().
  */
void UGOutgoingMessagePrepared(UMessageGateway * gateway, const UMessage * msg);

/** You can call this instead of UGOutgoingMessagePrepared() if you decided you don't want to send a UMessage after all.
  * (e.g. because the UMessage returned by UGGetOutgoingMessage() was too small, or something).
  * This will set the gateway back into its pre-UGGetOutgoingMessage() state.
  * @param gateway The same gateway object that you previously passed to UGGetOutgoingMessage().
  * @param msg A pointer to the UMessage that you previously received from UGGetOutgoingMessage().
  */
void UGOutgoingMessageCancelled(UMessageGateway * gateway, const UMessage * msg);

/** Returns UTrue iff the given gateway has any output bytes queued up, that it wants to send.
  * @param gw The Gateway to query.
  * @returns UTrue if there are bytes queued up to send, or UFalse otherwise.
  */
UBool UGHasBytesToOutput(const UMessageGateway * gw);

/** Writes out as many queued bytes as possible (up to maxBytes).
  * @param gw The Gateway that should do the outputting.
  * @param maxBytes The maximum number of bytes that should be sent by this function call.  Pass in ~0 to write without limit.
  * @param sendFunc The function that the gateway way will call to actually do the write operation.
  * @param arg The argument to pass to the write function.
  * @returns The number of bytes written, or a negative number if there was an error.
  */
int32 UGDoOutput(UMessageGateway * gw, uint32 maxBytes, UGSendFunc sendFunc, void * arg);

/** Reads in as many queued bytes as possible (up to maxBytes, or until a full UMessage is read).
  * @param gw The Gateway that should do the inputting.
  * @param maxBytes The maximum number of bytes that should be read by this function call.  Pass in ~0 to read without limit.
  * @param recvFunc The function that the gateway way will call to actually do the read operation.
  * @param arg The argument to pass to the read function.
  * @param optRetMsg When a full UMessage has been read, this UMessage object will be updated to be valid, and it should
  *                  be examined after UGDoInput() returned.  Note that the UMessage will only remain valid until the next
  *                  call to UGDoInput(), so any data you need from the UMessage should be examined or copied out immediately.
  * @returns The number of bytes read, or a negative number if there was an error.
  */
int32 UGDoInput(UMessageGateway * gw, uint32 maxBytes, UGReceiveFunc recvFunc, void * arg, UMessage * optRetMsg);

/** @} */ // end of micromessagegateway doxygen group

#ifdef __cplusplus
};
#endif

#endif
