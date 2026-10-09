#include "dataio/FileDataIO.h"
#include "ssl/SSLSignedPacketProxyDataIO.h"
#include "support/DataFlattener.h"
#include "support/DataUnflattener.h"

// keep these AFTER the MUSCLE includes, or Windows throws a fit
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

namespace muscle {

enum {
   SSL_PACKET_PROXY_TRAILER_SIZE = sizeof(uint32) + sizeof(uint32), ///< Our appended trailer consists of the payload-size, in bytes, and then the magic word
   SSL_PACKET_PROXY_MAGIC_WORD   = 1297303374                       ///< 'MSGN' -- magic bytes we place at the end of our appended signature's trailer to make it obvious it's there
};

SSLSignedPacketProxyDataIO :: SSLSignedPacketProxyDataIO()
   : _incomingContext(NULL)
   , _publicKey(NULL)
   , _outgoingContext(NULL)
   , _privateKey(NULL)
{
   // empty
}

SSLSignedPacketProxyDataIO :: SSLSignedPacketProxyDataIO(const PacketDataIORef & childRef)
   : ProxyDataIO(childRef)
   , _incomingContext(NULL)
   , _publicKey(NULL)
   , _outgoingContext(NULL)
   , _privateKey(NULL)
{
   // empty
}

SSLSignedPacketProxyDataIO :: ~SSLSignedPacketProxyDataIO()
{
   ResetIncomingContext();
   ResetOutgoingContext();
}

void SSLSignedPacketProxyDataIO :: ResetOutgoingContext()
{
   if (_privateKey)
   {
      EVP_PKEY_free(reinterpret_cast<EVP_PKEY *>(_privateKey));
      _privateKey = NULL;
   }

   if (_outgoingContext)
   {
      EVP_MD_CTX_free(reinterpret_cast<EVP_MD_CTX *>(_outgoingContext));
      _outgoingContext = NULL;
   }
}

void SSLSignedPacketProxyDataIO :: ResetIncomingContext()
{
   if (_publicKey)
   {
      EVP_PKEY_free(reinterpret_cast<EVP_PKEY *>(_publicKey));
      _publicKey = NULL;
   }

   if (_incomingContext)
   {
      EVP_MD_CTX_free(reinterpret_cast<EVP_MD_CTX *>(_incomingContext));
      _incomingContext = NULL;
   }
}

status_t SSLSignedPacketProxyDataIO :: SetPrivateKey(const char * privateKeyFilePath)
{
   FileDataIO fdio(privateKeyFilePath, "rb");
   ConstByteBufferRef bufRef = GetByteBufferFromPool(fdio);
   MRETURN_ON_ERROR(bufRef);
   return (bufRef()->GetNumBytes() > 0) ? SetPrivateKey(bufRef) : B_FILE_NOT_FOUND;
}

status_t SSLSignedPacketProxyDataIO :: SetPrivateKey(const uint8 * bytes, uint32 numBytes)
{
   ResetOutgoingContext();
   if (bytes == NULL) return B_NO_ERROR;  // NULL == just clear any existing private key

   BIO * in = BIO_new_mem_buf((void *)bytes, numBytes);
   if (in)
   {
      _privateKey = PEM_read_bio_PrivateKey(in, NULL, NULL, NULL);
      BIO_free(in);  // now that we're done with it

      if (_privateKey)
      {
         _outgoingContext = EVP_MD_CTX_new();
         if (_outgoingContext) return B_NO_ERROR;  // success!
      }
   }

   ResetOutgoingContext();  // roll back any partial allocations from above
   return B_SSL_ERROR;
}

status_t SSLSignedPacketProxyDataIO :: SetPrivateKey(const ConstByteBufferRef & pkd)
{
   return SetPrivateKey(pkd()?pkd()->GetBuffer():NULL, pkd()?pkd()->GetNumBytes():0);
}

status_t SSLSignedPacketProxyDataIO :: SetPublicKey(const char * publicKeyFilePath)
{
   FileDataIO fdio(publicKeyFilePath, "rb");
   ConstByteBufferRef bufRef = GetByteBufferFromPool(fdio);
   MRETURN_ON_ERROR(bufRef);
   return (bufRef()->GetNumBytes() > 0) ? SetPublicKey(bufRef) : B_FILE_NOT_FOUND;
}

status_t SSLSignedPacketProxyDataIO :: SetPublicKey(const uint8 * bytes, uint32 numBytes)
{
   ResetIncomingContext();
   if (bytes == NULL) return B_NO_ERROR;  // NULL argument just clears our outgoing context

   BIO * in = BIO_new_mem_buf((void *)bytes, numBytes);
   if (in)
   {
      _publicKey = PEM_read_bio_PUBKEY(in, NULL, NULL, NULL);
      BIO_free(in);  // now that we're done with it

      if (_publicKey == NULL)
      {
         // fall back to reading the public key out of an X509 certificate, if there is one
         in = BIO_new_mem_buf((void *)bytes, numBytes);
         if (in)
         {
            X509 * cert = PEM_read_bio_X509(in, NULL, NULL, NULL);
            BIO_free(in);  // now that we're done with it

            if (cert) _publicKey = X509_get_pubkey(cert);
            X509_free(cert);
         }
      }

      if (_publicKey)
      {
         _incomingContext = EVP_MD_CTX_new();
         if (_incomingContext) return B_NO_ERROR;  // success!
      }
   }

   ResetIncomingContext();  // roll back any partial allocations from above
   return B_SSL_ERROR;
}

status_t SSLSignedPacketProxyDataIO :: SetPublicKey(const ConstByteBufferRef & pk)
{
   return SetPublicKey(pk()?pk()->GetBuffer():NULL, pk()?pk()->GetNumBytes():0);
}

uint32 SSLSignedPacketProxyDataIO :: GetExpectedPacketSigningOverheadBytesCount()
{
   const uint32 ED25119_SIGNATURE_SIZE = 64;
   return ED25119_SIGNATURE_SIZE + SSL_PACKET_PROXY_TRAILER_SIZE;  // so the caller will allocate enough space that we can receive the signature and trailer bytes too
}

uint32 SSLSignedPacketProxyDataIO :: GetMaximumPacketSize() const
{
   const uint32 superSize    = ProxyDataIO::GetMaximumPacketSize();
   const uint32 overheadSize = GetExpectedPacketSigningOverheadBytesCount();
   return (superSize > overheadSize) ? (superSize-overheadSize) : 0;  // since we'll be adding trailing bytes, the available payload size gets reduced
}

io_status_t SSLSignedPacketProxyDataIO :: ReadAux(void * buffer, uint32 size, IPAddressAndPort * optRetPacketSource)
{
   if (_incomingContext)
   {
      const uint32 scratchBufSize = size+GetExpectedPacketSigningOverheadBytesCount();
      MRETURN_ON_ERROR(_scratchBuf.SetNumBytes(scratchBufSize, false));

      uint8 * scratchBuf = _scratchBuf.GetBuffer();
      const io_status_t subRet = optRetPacketSource ? ProxyDataIO::ReadFrom(scratchBuf, scratchBufSize, *optRetPacketSource) : ProxyDataIO::Read(scratchBuf, scratchBufSize);
      MRETURN_ON_ERROR(subRet);

      const io_status_t vRet = ValidateIncomingSignedPacket(scratchBuf, subRet.GetByteCount());
      MRETURN_ON_ERROR(vRet);

      const uint32 numPayloadBytes = muscleMin((uint32)vRet.GetByteCount(), size);
      memcpy(buffer, scratchBuf, numPayloadBytes);  // gotta copy because the user's buffer might not be big enough to hold the full packet including auth data
      return numPayloadBytes;
   }
   else return optRetPacketSource ? ProxyDataIO::ReadFrom(buffer, size, *optRetPacketSource) : ProxyDataIO::Read(buffer, size);
}

io_status_t SSLSignedPacketProxyDataIO :: Write(const void * buffer, uint32 size)
{
   if (_outgoingContext)
   {
      MRETURN_ON_ERROR(GenerateSignedOutputData(reinterpret_cast<const uint8 *>(buffer), size));

      const io_status_t subRet = ProxyDataIO::Write(_scratchBuf.GetBuffer(), _scratchBuf.GetNumBytes());
      MRETURN_ON_ERROR(subRet);

      return (int32) muscleMin((uint32)subRet.GetByteCount(), size);  // don't confuse the caller by reporting that we've written more bytes than he asked us to; even though we did
   }
   else return ProxyDataIO::Write(buffer, size);
}

io_status_t SSLSignedPacketProxyDataIO :: WriteTo(const void * buffer, uint32 size, const IPAddressAndPort & packetDest)
{
   if (_outgoingContext)
   {
      MRETURN_ON_ERROR(GenerateSignedOutputData(reinterpret_cast<const uint8 *>(buffer), size));

      const io_status_t subRet = ProxyDataIO::WriteTo(_scratchBuf.GetBuffer(), _scratchBuf.GetNumBytes(), packetDest);
      MRETURN_ON_ERROR(subRet);

      return (int32) muscleMin((uint32)subRet.GetByteCount(), size);  // don't confuse the caller by reporting that we've written more bytes than he asked us to; even though we did
   }
   else return ProxyDataIO::WriteTo(buffer, size, packetDest);
}

io_status_t SSLSignedPacketProxyDataIO :: ValidateIncomingSignedPacket(const uint8 * buffer, uint32 numBytes)
{
   if (numBytes < SSL_PACKET_PROXY_TRAILER_SIZE) return B_BAD_DATA;
   if ((_incomingContext == NULL)||(_publicKey == NULL)) return B_BAD_OBJECT;

   // Gotta validate that the trailer is there and use it to get the payload size and the signature size
   DataUnflattener unflat(buffer+numBytes-SSL_PACKET_PROXY_TRAILER_SIZE, SSL_PACKET_PROXY_TRAILER_SIZE);
   const uint32 numPayloadBytes = unflat.ReadInt32();
   const uint32 magicWord       = unflat.ReadInt32();
   if (magicWord != SSL_PACKET_PROXY_MAGIC_WORD) return B_BAD_DATA;
   if (numBytes < SaturatingUnsignedAdd(numPayloadBytes, (uint32)SSL_PACKET_PROXY_TRAILER_SIZE)) return B_BAD_DATA;  // signature-size can't be negative!

   const uint8 * sigBytes   = buffer+numPayloadBytes;
   const uint32 numSigBytes = numBytes-(numPayloadBytes+SSL_PACKET_PROXY_TRAILER_SIZE);

   EVP_MD_CTX * ctx = reinterpret_cast<EVP_MD_CTX *>(_incomingContext);

   char errBuf[256];  // ERR_error_string() docs require this buffer to be at least 256 bytes long
   if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL, reinterpret_cast<EVP_PKEY *>(_publicKey)) > 0)
   {
      if (EVP_DigestVerify(ctx, sigBytes, numSigBytes, buffer, numPayloadBytes) > 0)
      {
         return (int32) numPayloadBytes;
      }
      else LogTime(MUSCLE_LOG_DEBUG, "ValidateIncomingSignedPacket(): EVP_DigestVerify() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf)); // DEBUG to avoid log noise if we're receiving random packets
   }
   else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): EVP_DigestVerifyInit() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));

   return (int32)0;  // SSL error just means the packet wasn't propery signed, so we want the calling code to ignore its contents
}

status_t SSLSignedPacketProxyDataIO :: GenerateSignedOutputData(const uint8 * payloadBytes, uint32 numPayloadBytes)
{
   if ((_outgoingContext == NULL)||(_privateKey == NULL)) return B_BAD_OBJECT;

   EVP_MD_CTX * ctx = reinterpret_cast<EVP_MD_CTX *>(_outgoingContext);

   char errBuf[256];  // ERR_error_string() docs require this buffer to be at least 256 bytes long
   if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, reinterpret_cast<EVP_PKEY *>(_privateKey)) > 0)
   {
      size_t numSigBytes = 0;
      if (EVP_DigestSign(ctx, NULL, &numSigBytes, payloadBytes, numPayloadBytes) > 0)  // this will populate (numSigBytes), it should always be 64 but for form's sake
      {
         MRETURN_ON_ERROR(_scratchBuf.SetNumBytes(numPayloadBytes+(uint32)numSigBytes+SSL_PACKET_PROXY_TRAILER_SIZE, false));

         uint8 * scratchPayload = _scratchBuf.GetBuffer();
         if (EVP_DigestSign(ctx, scratchPayload+numPayloadBytes, &numSigBytes, payloadBytes, numPayloadBytes) > 0)  // writes out the signature into our scratch-buffer
         {
            memcpy(scratchPayload, payloadBytes, numPayloadBytes);

            DataFlattener tailWriter(_scratchBuf.GetBuffer()+numPayloadBytes+numSigBytes, SSL_PACKET_PROXY_TRAILER_SIZE);
            tailWriter.WriteInt32(numPayloadBytes);
            tailWriter.WriteInt32(SSL_PACKET_PROXY_MAGIC_WORD);  // just to make it obvious that the trailer exists

            _scratchBuf.TruncateToLength(numPayloadBytes+(uint32)numSigBytes+SSL_PACKET_PROXY_TRAILER_SIZE);  // just in case (numSigBytes) was set smaller by the second call to EVP_DigestSign()
            return B_NO_ERROR;
         }
         else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_DigestSignFinal failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
      }
      else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_DigestSignUpdate() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
   }
   else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_DigestSignInit() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));

   return B_SSL_ERROR;
}

} // end namespace muscle
