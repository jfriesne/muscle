/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleRawDataMessageIOGateway_h
#define MuscleRawDataMessageIOGateway_h

#include "iogateway/AbstractMessageIOGateway.h"

namespace muscle {

/** The 'what' code that will be found in incoming Messages. */
#define PR_COMMAND_RAW_DATA 1919181923 // 'rddc'

/** This is the name of the field used to hold data chunks */
#define PR_NAME_DATA_CHUNKS "rd"

/** This is the name of the int64 field used to hold received-data-timestamps */
#define PR_NAME_DATA_TIMESTAMP "ts"

/**
 * This gateway is used to send and receive raw data (ie streams of uint8's)
 * without making any attempt to convert the bytes from/to a higher-level message-format.
 * @note Messages sent and received by a RawDataMessageIOGateway should have all of their
 *       data contained in a field of type B_RAW_DATA, named PR_NAME_DATA_CHUNKS (aka "rd").
 *       Any other fields will be happily ignored.
 */
class RawDataMessageIOGateway : public AbstractMessageIOGateway
{
public:
   /** Constructor.
     * @param minChunkSizeBytes Don't return any data in chunks smaller than this.  Defaults to zero.
     * @param maxChunkSizeBytes Don't return any data in chunks larger than this.  Defaults to MUSCLE_NO_LIMIT.
     */
   RawDataMessageIOGateway(uint32 minChunkSizeBytes=0, uint32 maxChunkSizeBytes=MUSCLE_NO_LIMIT);

   /** Destructor */
   virtual ~RawDataMessageIOGateway();

   MUSCLE_NODISCARD virtual bool HasBytesToOutput() const;
   virtual void Reset();

   /** Sets whether PR_NAME_DATA_TIMESTAMP int64s should be added to incoming Messages,
     * to indicate the time at which the corresponding chunk of data was received.
     * Default value is false.
     * @param en true to enable timestamps, or false to disable them
     */
   void SetReceiveTimestampingEnabled(bool en) {_receiveTimestampingEnabled = en;}

   /** Returns true if received-data timestamping is enabled */
   MUSCLE_NODISCARD bool GetReceiveTimestampingEnabled() const {return _receiveTimestampingEnabled;}

protected:
   virtual io_status_t DoOutputImplementation(uint32 maxBytes = MUSCLE_NO_LIMIT);
   virtual io_status_t DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);

private:
   MessageRef _sendMsgRef;
   const void * _sendBuf;
   int32 _sendBufLength;      // # of bytes in current buffer
   int32 _sendBufIndex;       // index of the buffer currently being sent
   int32 _sendBufByteOffset;  // Index of Next byte to send in the current buffer

   MessageRef _recvMsgRef;
   void * _recvBuf;
   int32 _recvBufLength;
   int32 _recvBufByteOffset;

   uint8 * _recvScratchSpace;        // demand-allocated
   uint32 _recvScratchSpaceSize;

   uint32 _minChunkSize;
   uint32 _maxChunkSize;

   bool _receiveTimestampingEnabled;

   DECLARE_COUNTED_OBJECT(RawDataMessageIOGateway);
};
DECLARE_REFTYPES(RawDataMessageIOGateway);

/**
 * This class is the same as a RawDataMessageIOGateway, except that it is instrumented
 * to keep track of the number of bytes of raw data currently in its outgoing-Message
 * queue.
 */
class CountedRawDataMessageIOGateway : public RawDataMessageIOGateway
{
public:
   /** Constructor.
     * @param minChunkSize Don't return any data in chunks smaller than this.  Defaults to zero.
     * @param maxChunkSize Don't return any data in chunks larger than this.  Defaults to the largest possible uint32 value.
     */
   CountedRawDataMessageIOGateway(uint32 minChunkSize=0, uint32 maxChunkSize=MUSCLE_NO_LIMIT);

   virtual status_t AddOutgoingMessage(const MessageRef & messageRef);

   /** Returns the number of bytes of data currently present in our outgoing-data-queue */
   uint32 GetNumOutgoingDataBytes() const {return _outgoingByteCount;}

   virtual void Reset();

protected:
   virtual status_t PopNextOutgoingMessage(MessageRef & retMsg);

private:
   uint32 GetNumRawBytesInMessage(const MessageRef & messageRef) const;

   uint32 _outgoingByteCount;
};
DECLARE_REFTYPES(CountedRawDataMessageIOGateway);

} // end namespace muscle

#endif
