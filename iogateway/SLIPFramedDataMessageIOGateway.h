/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSLIPFramedDataMessageIOGateway_h
#define MuscleSLIPFramedDataMessageIOGateway_h

#include "iogateway/RawDataMessageIOGateway.h"
#include "util/ByteBuffer.h"

namespace muscle {

/**
 * This gateway is similar to the RawDataMessageIOGateway, except that
 * it encodes outgoing data using SLIP data framing (RFC 1055), and also
 * it parses incoming data as SLIP-framed data and decodes it before
 * passing it back to the calling code.
 *
 * Note that this gateway assumes that each item in the PR_NAME_DATA_CHUNKS
 * field is to be SLIP-encoded into its own SLIP frame, so you may need to
 * be a bit careful about how you segment your outgoing data.
 */
class SLIPFramedDataMessageIOGateway : public RawDataMessageIOGateway, private CountedObject<SLIPFramedDataMessageIOGateway>
{
public:
   /** Constructor.  */
   SLIPFramedDataMessageIOGateway();

   /** Destructor */
   virtual ~SLIPFramedDataMessageIOGateway();

   virtual void Reset();

protected:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

protected:
   virtual int32 DoInputImplementation(AbstractGatewayMessageReceiver & receiver, uint32 maxBytes = MUSCLE_NO_LIMIT);

   /** Overridden to SLIP-encode the popped Message before returning it. */
   virtual MessageRef PopNextOutgoingMessage();

private:
   void AddPendingByte(uint8 b);

   // State used to decode incoming SLIP data
   ByteBufferRef _pendingBuffer;
   MessageRef _pendingMessage;
   bool _lastReceivedCharWasEscape;
};

}; // end namespace muscle

#endif
