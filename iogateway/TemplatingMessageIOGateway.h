/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef TemplatingMuscleMessageIOGateway_h
#define TemplatingMuscleMessageIOGateway_h

#include "iogateway/MessageIOGateway.h"

namespace muscle {

/** This is a MessageIOGateway that has additional logic so that it
  * can send and receive Messages via template-encoding, which can
  * significantly reduce bandwidth in use-cases where most of the Messages
  * being sent have the same fields/layout as each other.  It does this by
  * sending the Messages' fields-layout-metadata just once, as a separate
  * "Message Template"; after that it needs only to send the payload-data
  * of subsequent Messages with that same fields-layout, rather than resending
  * both the payload-data and meta-data for every Message.  This can reduce
  * bandwidth usage by 50-75%, although it does require the use of a
  * TemplatingMessageIOGateway on both ends of the connection in order for
  * the received payload-data to be correctly decoded back into a full Message
  * again for the receiving program to consume.
  */
class TemplatingMessageIOGateway : public CountedMessageIOGateway
{
public:
   /**
    *  Constructor.
    *  @param maxLRUCacheSizeBytes the maximum mount of RAM that sender and receiver
    *                          should use to store cached template-Messages.  We use an LRU cache,
    *                          so when it gets full we will implicitly drop template-Messages
    *                          that have not been used recently, in order to keep RAM usage bounded.
    *                          Defaults to (1024*1024), a.k.a. 1 megabyte of cached template data.
    *  @param outgoingEncoding The byte-stream format the message should be encoded into.
    *                          Should be one of the MUSCLE_MESSAGE_ENCODING_* values.
    *                          Default is MUSCLE_MESSAGE_ENCODING_DEFAULT, meaning that no
    *                          zlib-compression will be done.  Note that to use any of the
    *                          MUSCLE_MESSAGE_ENCODING_ZLIB_* encodings, you MUST have
    *                          defined the compiler symbol -DMUSCLE_ENABLE_ZLIB_ENCODING.
    */
   TemplatingMessageIOGateway(uint32 maxTemplateCacheSizeBytes = 1024*1024, int32 outgoingEncoding = MUSCLE_MESSAGE_ENCODING_DEFAULT);

   /** Destructor. */
   virtual ~TemplatingMessageIOGateway() {/* empty */}

   virtual void Reset();

protected:
   virtual ByteBufferRef FlattenHeaderAndMessage(const MessageRef & msgRef) const;
   virtual MessageRef UnflattenHeaderAndMessage(const ConstByteBufferRef & bufRef) const;
   virtual uint32 GetHeaderSize() const {return CountedMessageIOGateway::GetHeaderSize() + sizeof(uint64);}  // we add a template-ID field
   virtual int32 GetBodySize(const uint8 * header) const;

   /** Returns the template-hash-code to use for the given Message that is about to
     * be sent, or 0 if the Message should be just sent the old/traditional way without
     * any templating.  Default implementation always calls through to the TemplateHashCode64()
     * method of the passed-in Message object; but an application-specific subclass of
     * TemplatingMessageIOGateway could override this method to always return 0 for
     * Messages that it knows are unlikely to benefit from templating.
     * @param msg the Message to get a template-hash-code for.
     */
   virtual uint64 GetTemplateHashCode64ForMessage(const Message & msg) const {return msg.TemplateHashCode64();}

private:
   const uint32 _maxLRUCacheSizeBytes;

   mutable Hashtable<uint64, MessageRef> _incomingTemplates;
   mutable Hashtable<uint64, MessageRef> _outgoingTemplates;
   mutable uint32 _incomingTemplatesTotalSizeBytes;
   mutable uint32 _outgoingTemplatesTotalSizeBytes;

   void TrimLRUCache(Hashtable<uint64, MessageRef> & lruCache, uint32 & tallyBytes) const;


   DECLARE_COUNTED_OBJECT(TemplatingMessageIOGateway);
};
DECLARE_REFTYPES(TemplatingMessageIOGateway);

} // end namespace muscle

#endif
