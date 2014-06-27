#ifndef MuscleSSLSocketDataIO_h
#define MuscleSSLSocketDataIO_h

#include "dataio/TCPSocketDataIO.h"
#include "util/ByteBuffer.h"

namespace muscle {

class SSLSocketAdapterGateway;

/** This class lets you communicate over a TCP socket with SSL encryption enabled on it.
  * @note In most cases when using this class you will want to wrap your MessageIOGateway
  *       in a SSLSocketAdapterGateway object; otherwise OpenSSL's internal state machine
  *       will not be able to work properly!
  * @note If you simply want to enable SSL on all your TCP connections, the easiest way
  *       to do that is to recompile MUSCLE with the -DMUSCLE_ENABLE_SSL compiler argument,
  *       and then call either SetSSLPublicKeyCertificate() or SetSSLPrivateKey() on your
  *       ReflectServer or MessageTransceiverThread object.  Then the MUSCLE event loop
  *       will transparently insert the SSL layer for you.  Alternatively, creating the
  *       SSLSocketDataIO objects and SSLSocketAdapterGateway objects explicitly in your 
  *       own code will also work and may be necessary in cases where only certain sessions 
  *       should use SSL.
  */
class SSLSocketDataIO : public TCPSocketDataIO, private CountedObject<SSLSocketDataIO>
{
public:
   /**
    * Constructor.
    * @param sockfd The socket to use.
    * @param blocking If true, the socket will be set to blocking mode; otherwise non-blocking
    * @param accept True iff the SSL logic should be put into "accept-connection" mode, or false for "outgoing-connection" mode.
    */
   SSLSocketDataIO(const ConstSocketRef & sockfd, bool blocking, bool accept);

   virtual ~SSLSocketDataIO();

   /** Adds a certification to use for this session.
     * @param certFilePath File path of the certificate file to use.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (couldn't find file?)
     */ 
   status_t SetPublicKeyCertificate(const char * certFilePath);

   /** Same as above, except instead of reading the certificate from a
     * file, the certificate is read from memory.
     * @param bytes The array containing the certificate (i.e. the contents of a .pub file)
     * @param numBytes The number of bytes that (bytes) points to.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t SetPublicKeyCertificate(const uint8 * bytes, uint32 numBytes);

   /** Same as above, except instead of reading from a raw array we read from a ConstByteBufferRef.
     * @param publicKeyFile The bytes to read from.  We will retain a reference to this buffer.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t SetPublicKeyCertificate(const ConstByteBufferRef & publicKeyFile);

   /** Contents of our current public key file */
   ConstByteBufferRef GetPublicKeyCertificate() const {return _publicKey;}

   /** Adds a private key to use for this session.
     * @param privateKeyFilePath File path of the private key file to use.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (couldn't find file?)
     * @note Typically on the server side you'll want to call both SetPrivateKey() *and*
     *       SetPublicKeyCertificate() on your SSLSocketDataIO object.  The same
     *       file data can be passed to both (assuming the file in question contains
     *       both a PRIVATE key section and a CERTIFICATE section)
     */ 
   status_t SetPrivateKey(const char * privateKeyFilePath);

   /** Same as above, except instead of reading the private key from a
     * file, the private key is read from memory.
     * @param bytes The array containing the private key (i.e. the contents of a .pem file)
     * @param numBytes The number of bytes that (bytes) points to.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     * @note Typically on the server side you'll want to call both SetPrivateKey() *and*
     *       SetPublicKeyCertificate() on your SSLSocketDataIO object.  The same
     *       file data can be passed to both (assuming the file in question contains
     *       both a PRIVATE key section and a CERTIFICATE section)
     */
   status_t SetPrivateKey(const uint8 * bytes, uint32 numBytes);

   /** Overridden to return a dummy (always-ready-for-read) socket when necessary,
     * as there are times when we need gateway->DoInput() to be called when even when there
     * aren't any actual bytes present to be read from our TCP socket. 
     * See http://www.rtfm.com/openssl-examples/part2.pdf (Figure 8) for details.
     */
   virtual const ConstSocketRef & GetReadSelectSocket() const;

   virtual int32 Read(void *buffer, uint32 size);
   virtual int32 Write(const void *buffer, uint32 size);
   virtual void Shutdown();

private:
   friend class SSLSocketAdapterGateway;

   enum {
      SSL_STATE_READ_WANTS_READABLE_SOCKET   = 0x01,
      SSL_STATE_READ_WANTS_WRITEABLE_SOCKET  = 0x02,
      SSL_STATE_WRITE_WANTS_READABLE_SOCKET  = 0x04,
      SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET = 0x08,
      NUM_SSL_STATES
   };
   uint32 _sslState;

   bool _forceReadReady;
   ConstSocketRef _alwaysReadableSocket;  // this dummy socket will ALWAYS select as ready-for-read!

   ConstByteBufferRef _publicKey;

   void * _ctx; // actually (SSL_CTX*) but declared (void *) here so I don't have to #include openssl headers 
   void * _ssl; // actually (SSL*)     but declared (void *) here so I don't have to #include openssl headers
};

}; // end namespace muscle

#endif
