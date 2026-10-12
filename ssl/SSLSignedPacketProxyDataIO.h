/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSSLSignedPacketProxyDataIO_h
#define MuscleSSLSignedPacketProxyDataIO_h

#include "dataio/ProxyDataIO.h"
#include "util/Hashtable.h"
#include "util/ByteBuffer.h"

namespace muscle {

#ifndef MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE
# define MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE 1024
#endif

/** This is a proxy DataIO that can use a private key to add signature bytes to the end of
  * outgoing packets, and/or use a public key to verify the signature bytes at the end of
  * incoming packets and drop any incoming packets that don't have the expected signature.
  * @note this class probably isn't useful in combination with a streaming DataIO, since
  *       it relies on message boundaries being preserved to know where the message signatures should be.
  */
class SSLSignedPacketProxyDataIO : public ProxyDataIO
{
public:
   /**
    * Default Constructor.
    * @param receivedPacketIDWindowSize the size of our receive-window for de-duplication and replay-attack-defense of incoming UDP packets.
    *                                   Defaults to MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE (e.g. incoming packet IDs that are within (receivedPacketIDWindowSize) of the largest received packet ID, and
    *                                   haven't already been received, will be accepted).  As a special case, you can pass zero for this argument
    *                                   to disable the packet-ID-filtering logic entirely.
    * @note be sure to call SetChildDataIO(), and SetPrivateKey() and/or SetPublicKey() before using this object, or it won't do anything useful.
    */
   SSLSignedPacketProxyDataIO(uint32 receivedPacketIDWindowSize = MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE);

   /**
    * Constructor.
    * @param childRef reference to the PacketDataIO object this SSLSignedPacketProxyDataIO should use to do its sending and/or recieving
    *                 of raw packet data.  (Usually this should be a UDPSocketDataIO, unless you're doing something particularly creative)
    * @param receivedPacketIDWindowSize the size of our receive-window for de-duplication and replay-attack-defense of incoming UDP packets.
    *                                   Defaults to MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE (e.g. incoming packet IDs that are within (receivedPacketIDWindowSize) of the largest received packet ID, and
    *                                   haven't already been received, will be accepted).  As a special case, you can pass zero for this argument
    *                                   to disable the packet-ID-filtering logic entirely.
    * @note be sure to call SetPrivateKey() and/or SetPublicKey() before using this object, or it won't do anything useful.
    */
   SSLSignedPacketProxyDataIO(const PacketDataIORef & childRef, uint32 receivedPacketIDWindowSize = MUSCLE_DEFAULT_SSL_SIGNED_PACKET_PROXY_WINDOW_SIZE);

   /** Destructor. */
   virtual ~SSLSignedPacketProxyDataIO();

   /** Sets a public key to use for this DataIO.  With a public-key installed, this object will
     * verify that incoming packets have the expected signature at the end, and if they do, it
     * will remove that signature and just return the payload of the packet.  Packets that do not have
     * the expected signature will be silently dropped, and Read()/ReadFrom() will return zero.
     * If no public key is set, then all incoming packets will be returned verbatim.
     * @param publicKeyFilePath File path of the public key file to use.
     * @returns B_NO_ERROR on success, or an error code on failure (couldn't find file?)
     * @note the public key file can contain either a BEGIN PUBLIC KEY section or a BEGIN CERTIFICATE section; either will work.
     */
   status_t SetPublicKey(const char * publicKeyFilePath);

   /** Same as above, except instead of reading the public key from a file, the public key is read from memory.
     * @param bytes The array containing the public key (ie the contents of a .pub file)
     * @param numBytes The number of bytes that (bytes) points to.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note the public key data can contain either a BEGIN PUBLIC KEY section or a BEGIN CERTIFICATE section; either will work.
     */
   status_t SetPublicKey(const uint8 * bytes, uint32 numBytes);

   /** Same as above, except instead of reading from a raw array we read from a ConstByteBufferRef.
     * @param publicKeyBytes The bytes to read from.
     *                       If NULL, then any existing public key will be forgotten and this method will return B_NO_ERROR.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note the public key data can contain either a BEGIN PUBLIC KEY section or a BEGIN CERTIFICATE section; either will work.
     */
   status_t SetPublicKey(const ConstByteBufferRef & publicKeyBytes);

   /** Sets a private key to use for this DataIO.  Setting a private key will allow this DataIO
     * to sign any outgoing data packets.  If no private key is set, then outgoing data packets
     * will be sent out as-is.
     * @param privateKeyFilePath File path of the private key file (e.g. a .pem file) to use.
     * @returns B_NO_ERROR on success, or an error code on failure (couldn't find file?)
     * @note Typically on the server side you'll want to call both SetPrivateKey() *and*
     *       SetPublicKey() on your SSLSignedPacketProxyDataIO object.  The same
     *       file data can be passed to both (assuming the file in question contains
     *       both a PRIVATE key section and a CERTIFICATE section)
     */
   status_t SetPrivateKey(const char * privateKeyFilePath);

   /** Same as above, except instead of reading the private key from a file, the private key is read from memory.
     * @param privateKeyBytes The array containing the private key (ie the contents of a .pem file), or NULL
     *                        if you just want to get rid of an existing private key.
     * @param numPrivateKeyBytes The number of bytes that (privateKeyBytes) points to.
     * @returns B_NO_ERROR on success, or an error code on failure.
     * @note Typically on the server side you'll want to call both SetPrivateKey() *and*
     *       SetPublicKey() on your SSLSignedPacketProxyDataIO object.  The same
     *       file data can be passed to both (assuming the file in question contains
     *       both a PRIVATE key section and a CERTIFICATE section)
     */
   status_t SetPrivateKey(const uint8 * privateKeyBytes, uint32 numPrivateKeyBytes);

   /** Same as above, except instead of reading from a raw array we read from a ConstByteBufferRef.
     * @param privateKeyData The bytes to read from, or NULL if you just want to get rid of an existing private key.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SetPrivateKey(const ConstByteBufferRef & privateKeyData);

   MUSCLE_NODISCARD virtual uint32 GetMaximumPacketSize() const;
   virtual io_status_t Read(void * buffer, uint32 size) {return ReadAux(buffer, size, NULL);}
   virtual io_status_t Write(const void * buffer, uint32 size);
   virtual io_status_t ReadFrom(void * buffer, uint32 size, IPAddressAndPort & retPacketSource) {return ReadAux(buffer, size, &retPacketSource);}
   virtual io_status_t WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest);

   /** Returns the number of bytes of data that signing an outgoing packet is expected to add to its wire-size */
   static uint32 GetExpectedPacketSigningOverheadBytesCount();

private:
   class ReceivedPacketIDsWindow : public RefCountable
   {
   public:
      /** Constructor
        * @param bitmap should be a ByteBuffer that we will use as a running bitmap to determine which packet IDs we have seen recently
        * @param firstReceivedPacketID the ID of the received packet that caused this ReceivedPacketIDsWindow object to be created
        */
      ReceivedPacketIDsWindow(const ByteBufferRef & bitmap, uint64 firstReceivedPacketID);

      /** Call this when a new, validated packet is received.
        * @param newPacketID the packet ID of the new packet
        * @returns true (and updates our internal state) if the packet is acceptable.  Return false if the packet was rejected.
        */
      bool MaybeAcceptPacket(uint64 newPacketID);

   private:
      void SetBit(uint32 bitIndex, bool newBitValue)
      {
         MASSERT(bitIndex < GetWindowSize(), "SetBit(): bad bitIndex");

         const uint8 theMask = (1<<(bitIndex%8));
         uint8 & theByte     = _bitmap()->GetBuffer()[bitIndex/8];

         if (newBitValue) theByte |=  theMask;
                     else theByte &= ~theMask;
      }

      bool IsBitSet(uint32 bitIndex) const
      {
         MASSERT(bitIndex < GetWindowSize(), "IsBitSet(): bad bitIndex");

         const uint8 theMask   = (1<<(bitIndex%8));
         const uint8 & theByte = _bitmap()->GetBuffer()[bitIndex/8];

         return ((theByte & theMask) != 0);
      }

      // Note that (packetID) MUST be within our current window, or else we'll trigger an assertion failure
      uint32 GetBitIndexForPacketID(uint64 packetID) const
      {
         MASSERT(muscleInRange(packetID, GetSmallestPacketIDInWindow(), _largestReceivedPacketID), "GetBitIndexForPacketID():  packetID wasn't in the window");

         const uint32 windowSize = GetWindowSize();
         return ((packetID-_largestReceivedPacketID)+_largestPacketIDBitIndex+windowSize)%windowSize;
      }

      uint32 GetWindowSize() const {return (_bitmap()->GetNumBytes()*8);}

      uint64 GetSmallestPacketIDInWindow() const
      {
         const uint64 ws = GetWindowSize();
         return (_largestReceivedPacketID > ws) ? (_largestReceivedPacketID-(ws-1)) : 0;
      }

      void ClearBitmap() {memset(_bitmap()->GetBuffer(), 0, _bitmap()->GetNumBytes());}

      ByteBufferRef _bitmap;
      uint64 _largestReceivedPacketID;  // the largest Packet ID represented in our window
      uint32 _largestPacketIDBitIndex;  // index of the bit in our bitmap that represents the state of (_largestReceivedPacketID)
   };
   DECLARE_REFTYPES(ReceivedPacketIDsWindow);

   Hashtable<uint64, ReceivedPacketIDsWindowRef> _receivedPacketWindowBySenderID;

   ReceivedPacketIDsWindow * GetOrPutReceivedPacketIDsWindow(uint64 senderSessionID, uint64 senderPacketID);

   io_status_t ReadAux(void * buffer, uint32 size, IPAddressAndPort * optRetPacketSource);
   io_status_t ValidateIncomingSignedPacket(const uint8 * buffer, uint32 numBytes);
   status_t GenerateSignedOutputData(const uint8 * dataBytes, uint32 numDataBytes);

   void ResetIncomingContext();
   void ResetOutgoingContext();

   const uint32 _receivedPacketIDWindowSize;

   void * _incomingContext; // actually of type EVP_MD_CTX, but I don't want to include OpenSSL headers from here, so
   void * _publicKey;       // actually of type EVP_PKEY

   const uint64 _outgoingSessionID;
   uint64 _outgoingPacketIDCounter;

   void * _outgoingContext; // actually of type EVP_MD_CTX, but I don't want to include OpenSSL headers from here, so
   void * _privateKey;      // actually of type EVP_PKEY

   ByteBuffer _scratchBuf;  // scratch space used to combine outgoing data with its signature (maybe we could use writev() instead, someday?)

   DECLARE_COUNTED_OBJECT(SSLSignedPacketProxyDataIO);
};
DECLARE_REFTYPES(SSLSignedPacketProxyDataIO);

} // end namespace muscle

#endif
