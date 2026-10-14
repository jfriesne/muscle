#include "dataio/FileDataIO.h"
#include "ssl/SSLSignedPacketProxyDataIO.h"
#include "support/DataFlattener.h"
#include "support/DataUnflattener.h"

// keep these AFTER the MUSCLE includes, or Windows throws a fit
#include <openssl/bio.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

namespace muscle {

// Our packets are organized like this, to make the data-signing and data-validation easier:
//   [caller's payload bytes]
//   [signed-trailer-fields]
//   [signature-bytes]
//   [unsigned-trailer-fields]
enum {
   SIGNED_TRAILER_SIZE   = sizeof(uint64) + sizeof(uint64), ///< signed-trailer consists of the _outgoingSessionID and the _outgoingPacketIDCounter
   UNSIGNED_TRAILER_SIZE = sizeof(uint32) + sizeof(uint32), ///< unsigned-trailer consists of the payload-size-word, and then the MAGIC_WORD
   MAGIC_WORD            = 1297303374                       ///< 'MSGN' -- magic bytes we place at the end of our appended signature's trailer to make it obvious it's there
};

// For this class I want good-quality randomness; just calling GetInsecurePseudoRandomNumber64() won't cut it
static uint64 GetSecurePseudoRandomNumber64()
{
   uint64 ret = 0;
   if (RAND_bytes((unsigned char *)&ret, sizeof(ret)) != 1) MCRASH("RAND_bytes() failed!");
   return ret;
}

static uint32 RoundUpToMultipleOfEight(uint32 val) {return ((val+7)/8)*8;}

SSLSignedPacketProxyDataIO :: SSLSignedPacketProxyDataIO(uint32 receivedPacketIDWindowSize)
   : _receivedPacketIDWindowSize(RoundUpToMultipleOfEight(receivedPacketIDWindowSize))
   , _incomingContextIsHMAC(false)
   , _incomingContext(NULL)
   , _publicKey(NULL)
   , _outgoingSessionID(GetSecurePseudoRandomNumber64())
   , _outgoingPacketIDCounter(GetSecurePseudoRandomNumber64() >> 24)  // the >>24 is just to avoid any realistic chance of ever reaching ((uint64)-1)
   , _outgoingContextIsHMAC(false)
   , _outgoingContext(NULL)
   , _privateKey(NULL)
{
   // empty
}

SSLSignedPacketProxyDataIO :: SSLSignedPacketProxyDataIO(const PacketDataIORef & childRef, uint32 receivedPacketIDWindowSize)
   : ProxyDataIO(childRef)
   , _receivedPacketIDWindowSize(RoundUpToMultipleOfEight(receivedPacketIDWindowSize))
   , _incomingContextIsHMAC(false)
   , _incomingContext(NULL)
   , _publicKey(NULL)
   , _outgoingSessionID(GetSecurePseudoRandomNumber64())
   , _outgoingPacketIDCounter(GetSecurePseudoRandomNumber64() >> 24)  // the >>24 is just to avoid any realistic chance of ever reaching ((uint64)-1)
   , _outgoingContextIsHMAC(false)
   , _outgoingContext(NULL)
   , _privateKey(NULL)
{
   // empty
}

SSLSignedPacketProxyDataIO :: ~SSLSignedPacketProxyDataIO()
{
   ShutdownAux();  // make sure public and private keys are freed
}

static void FreeContext(void * context, bool & isHMAC)
{
   if (isHMAC) EVP_MAC_CTX_free(reinterpret_cast<EVP_MAC_CTX *>(context));
          else EVP_MD_CTX_free( reinterpret_cast<EVP_MD_CTX  *>(context));
   isHMAC = false;
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
      FreeContext(_outgoingContext, _outgoingContextIsHMAC);
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
      FreeContext(_incomingContext, _incomingContextIsHMAC);
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

            if (cert)
            {
               _publicKey = X509_get_pubkey(cert);
               X509_free(cert);
            }
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

status_t SSLSignedPacketProxyDataIO :: SetPreSharedKey(const ConstByteBufferRef & pskBuf, bool enableTransmit, bool enableReceive)
{
   if (pskBuf() == NULL) return B_BAD_ARGUMENT;

   EVP_MAC * mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
   if (mac == NULL) return B_SSL_ERROR;

   ResetIncomingContext();
   ResetOutgoingContext();
   _preSharedKey.Reset();

   bool ok = true;

   if ((ok)&&(enableReceive))
   {
      _incomingContext = EVP_MAC_CTX_new(mac);
      if (_incomingContext) _incomingContextIsHMAC = true;
                       else ok = false;
   }

   if ((ok)&&(enableTransmit))
   {
      _outgoingContext = EVP_MAC_CTX_new(mac);
      if (_outgoingContext) _outgoingContextIsHMAC = true;
                       else ok = false;
   }

   EVP_MAC_free(mac);

   if (ok)
   {
      _preSharedKey = pskBuf;
      return B_NO_ERROR;
   }
   else
   {
      ResetIncomingContext();
      ResetOutgoingContext();
      return B_SSL_ERROR;
   }
}

status_t SSLSignedPacketProxyDataIO :: SetPreSharedKey(const uint8 * preSharedKeyBytes, uint32 numPreSharedKeyBytes, bool enableTransmit, bool enableReceive)
{
   ConstByteBufferRef pskRef = GetByteBufferFromPool(numPreSharedKeyBytes, preSharedKeyBytes);
   MRETURN_ON_ERROR(pskRef);

   return SetPreSharedKey(pskRef, enableTransmit, enableReceive);
}

status_t SSLSignedPacketProxyDataIO :: SetPreSharedKey(const char * preSharedKeyString, bool enableTransmit, bool enableReceive)
{
   if (preSharedKeyString == NULL) return B_BAD_ARGUMENT;

   // Convert the ASCII password into some suitably pseudorandom bytes
   unsigned int size = 0;
   unsigned char digest[EVP_MAX_MD_SIZE];
   if (EVP_Digest(preSharedKeyString, strlen(preSharedKeyString), digest, &size, EVP_sha256(), NULL) != 1) return B_SSL_ERROR;

   return SetPreSharedKey(digest, (uint32)size, enableTransmit, enableReceive);
}

static const EVP_MD * GetMessageDigestType(EVP_PKEY * key)
{
   return EVP_PKEY_is_a(key, "HMAC") ? EVP_sha256() : NULL;
}

uint32 SSLSignedPacketProxyDataIO :: GetExpectedPacketSigningOverheadBytesCount(bool forOutput) const
{
   const uint32 SHA256_HMAC_SIGNATURE_SIZE = 32;  // hard-coded because EVP_MAC_CTX_get_mac_size() won't return the correct result if EVP_MAC_init() hasn't been called on it yet
   const uint32 DEFAULT_SIGNATURE_SIZE     = 64;  // conservative fallback to use when no key is installed yet

   uint32 pkeySize = 0;
   if (forOutput ? _outgoingContextIsHMAC : _incomingContextIsHMAC) pkeySize = muscleMax(pkeySize, SHA256_HMAC_SIGNATURE_SIZE);

   EVP_PKEY * key = reinterpret_cast<EVP_PKEY *>(forOutput ? _privateKey : _publicKey);
   if (key) pkeySize = muscleMax(pkeySize, (uint32)EVP_PKEY_get_size(key));

   return ((pkeySize != 0) ? pkeySize : DEFAULT_SIGNATURE_SIZE) + SIGNED_TRAILER_SIZE + UNSIGNED_TRAILER_SIZE;  // so the caller will allocate enough space that we can receive the signature and trailer bytes too
}

uint32 SSLSignedPacketProxyDataIO :: GetMaximumPacketSize() const
{
   const uint32 superSize    = ProxyDataIO::GetMaximumPacketSize();
   const uint32 overheadSize = muscleMax(GetExpectedPacketSigningOverheadBytesCount(false), GetExpectedPacketSigningOverheadBytesCount(true));
   return (superSize > overheadSize) ? (superSize-overheadSize) : 0;  // since we'll be adding trailing bytes, the available payload size gets reduced
}

io_status_t SSLSignedPacketProxyDataIO :: ReadAux(void * buffer, uint32 size, IPAddressAndPort * optRetPacketSource)
{
   if (_incomingContext)
   {
      const uint32 scratchBufSize = size+GetExpectedPacketSigningOverheadBytesCount(false);
      MRETURN_ON_ERROR(_scratchBuf.SetNumBytes(scratchBufSize, false));

      uint8 * scratchBuf = _scratchBuf.GetBuffer();
      const io_status_t subRet = optRetPacketSource ? ProxyDataIO::ReadFrom(scratchBuf, scratchBufSize, *optRetPacketSource) : ProxyDataIO::Read(scratchBuf, scratchBufSize);
      MRETURN_ON_ERROR(subRet);
      if (subRet.GetByteCount() == 0) return subRet;  // there's no point trying to validate a 0-byte result

      const io_status_t vRet = ValidateIncomingSignedPacket(scratchBuf, subRet.GetByteCount());
      if (vRet.IsError())
      {
         LogTime(MUSCLE_LOG_DEBUG, "SSLSignedPacketProxyDataIO::ReadAux(%p):  ValidateIncomingSignedPacket(" UINT32_FORMAT_SPEC ") from [%s] returned [%s]\n", this, subRet.GetByteCount(), optRetPacketSource?optRetPacketSource->ToString()():"???", vRet());
         return (int32) 0;  // Validation failure isn't an I/O failure, so just drop the data and continue
      }

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

      _outgoingPacketIDCounter++;

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

      _outgoingPacketIDCounter++;

      return (int32) muscleMin((uint32)subRet.GetByteCount(), size);  // don't confuse the caller by reporting that we've written more bytes than he asked us to; even though we did
   }
   else return ProxyDataIO::WriteTo(buffer, size, packetDest);
}

SSLSignedPacketProxyDataIO::ReceivedPacketIDsWindow * SSLSignedPacketProxyDataIO :: GetOrPutReceivedPacketIDsWindow(uint64 senderSessionID, uint64 senderPacketID)
{
   ReceivedPacketIDsWindowRef * r = _receivedPacketWindowBySenderID.Get(senderSessionID);
   if (r) return r->GetItemPointer();
   else
   {
      // demand-allocate
      ByteBufferRef bitmap = GetByteBufferFromPool(_receivedPacketIDWindowSize/8);
      if (bitmap() == NULL) return NULL;

      ReceivedPacketIDsWindowRef newWindow(new ReceivedPacketIDsWindow(bitmap, senderPacketID));
      if (_receivedPacketWindowBySenderID.Put(senderSessionID, newWindow).IsError()) return NULL;

      return newWindow();
   }
}

static status_t InitMACAux(EVP_MAC_CTX * ctx, const ConstByteBufferRef & preSharedKey, char * errBuf)
{
   if ((ctx == NULL)||(preSharedKey() == NULL)) return B_BAD_ARGUMENT;  // semi-paranoia

   const OSSL_PARAM params[] = {
      OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, (char *)"SHA256", 0),
      OSSL_PARAM_construct_end()
   };
   return (EVP_MAC_init(ctx, preSharedKey()->GetBuffer(), preSharedKey()->GetNumBytes(), params) > 0) ? B_NO_ERROR : B_ERROR(ERR_error_string(ERR_get_error(), errBuf));
}

io_status_t SSLSignedPacketProxyDataIO :: ValidateIncomingSignedPacket(const uint8 * buffer, uint32 numBytes)
{
   if (_incomingContext == NULL) return B_BAD_OBJECT;
   if (numBytes < (SIGNED_TRAILER_SIZE+UNSIGNED_TRAILER_SIZE)) return B_BAD_DATA;

   // First check that the unsigned-trailer is valid, and if so we can use it to calculate the payload-size and the signature-size
   DataUnflattener unsignedUnflat(buffer+numBytes-UNSIGNED_TRAILER_SIZE, UNSIGNED_TRAILER_SIZE);
   const uint32 numPayloadBytes = unsignedUnflat.ReadInt32();
   const uint32 magicWord       = unsignedUnflat.ReadInt32();
   if (magicWord != MAGIC_WORD) return B_BAD_DATA;
   if (numBytes < SaturatingUnsignedAdd(numPayloadBytes, (uint32)(SIGNED_TRAILER_SIZE+UNSIGNED_TRAILER_SIZE))) return B_BAD_DATA;  // signature-size can't be negative!

   const uint8 * sigBytes   = buffer+numPayloadBytes+SIGNED_TRAILER_SIZE;
   const uint32 numSigBytes = numBytes-(numPayloadBytes+(SIGNED_TRAILER_SIZE+UNSIGNED_TRAILER_SIZE));

   const uint32 numBytesToVerify = numPayloadBytes+SIGNED_TRAILER_SIZE;

   char errBuf[256];  // ERR_error_string() docs require this buffer to be at least 256 bytes long
   if (_incomingContextIsHMAC)
   {
      if (_preSharedKey() == NULL) return B_BAD_OBJECT;

      EVP_MAC_CTX * ctx = reinterpret_cast<EVP_MAC_CTX *>(_incomingContext);
      const status_t r = InitMACAux(ctx, _preSharedKey, errBuf);
      if (r.IsOK())
      {
         if (EVP_MAC_update(ctx, buffer, numBytesToVerify) > 0)
         {
            uint8 tempSigBytes[256];
            if (numSigBytes <= sizeof(tempSigBytes))
            {
               size_t numTempSigBytesWritten = 0;
               if (EVP_MAC_final(ctx, tempSigBytes, &numTempSigBytesWritten, sizeof(tempSigBytes)) > 0)
               {
                  if (numTempSigBytesWritten == (size_t)numSigBytes)
                  {
                     if (CRYPTO_memcmp(sigBytes, tempSigBytes, numTempSigBytesWritten) == 0)  // don't call vanilla memcmp() here, it's vulnerable to timing attacks
                     {
                        return ValidateIncomingSignedPacketAux(buffer, numPayloadBytes);
                     }
                     else LogTime(MUSCLE_LOG_DEBUG, "ValidateIncomingSignedPacket(): CRYPTO_memcmp() didn't match!\n"); // DEBUG to avoid log noise if we're receiving random packets
                  }
                  else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): numTempSigBytesWritten was %zu but numSigBytes was " UINT32_FORMAT_SPEC "\n", numTempSigBytesWritten, numSigBytes);
               }
               else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): EVP_MAC_final() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
            }
            else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): numSigBytes (" UINT32_FORMAT_SPEC ") was too large!\n", numSigBytes);
         }
         else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): EVP_MAC_update() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
      }
      else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): EVP_MAC_init() failed! [%s]\n", r());  // don't return r(), it will dangle if you do
   }
   else
   {
      if (_publicKey == NULL) return B_BAD_OBJECT;

      EVP_MD_CTX * ctx = reinterpret_cast<EVP_MD_CTX *>(_incomingContext);

      EVP_PKEY * publicKey = reinterpret_cast<EVP_PKEY *>(_publicKey);
      if (EVP_DigestVerifyInit(ctx, NULL, GetMessageDigestType(publicKey), NULL, publicKey) > 0)
      {
         if (EVP_DigestVerify(ctx, sigBytes, numSigBytes, buffer, numBytesToVerify) > 0)
         {
            return ValidateIncomingSignedPacketAux(buffer, numPayloadBytes);
         }
         else LogTime(MUSCLE_LOG_DEBUG, "ValidateIncomingSignedPacket(): EVP_DigestVerify() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf)); // DEBUG to avoid log noise if we're receiving random packets
      }
      else LogTime(MUSCLE_LOG_ERROR, "ValidateIncomingSignedPacket(): EVP_DigestVerifyInit() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
   }

   return B_SSL_ERROR;  // SSL error is non-fatal (it just means the packet wasn't properly signed, so we want the calling code to ignore its contents)
}

io_status_t SSLSignedPacketProxyDataIO :: ValidateIncomingSignedPacketAux(const uint8 * buffer, uint32 numPayloadBytes)
{
   if (_receivedPacketIDWindowSize > 0)
   {
      DataUnflattener signedUnflat(buffer+numPayloadBytes, SIGNED_TRAILER_SIZE);
      const uint64 senderSessionID = signedUnflat.ReadInt64();
      const uint64 senderPacketID  = signedUnflat.ReadInt64();

      ReceivedPacketIDsWindow * rpw = GetOrPutReceivedPacketIDsWindow(senderSessionID, senderPacketID);
      if (rpw == NULL) return B_OUT_OF_MEMORY;

      if (rpw->MaybeAcceptPacket(senderPacketID) == false) return (int32)0;  // mitigation against duplicate packet IDs and replay attacks
   }

   return (int32) numPayloadBytes;  // success!
}

status_t SSLSignedPacketProxyDataIO :: GenerateSignedOutputData(const uint8 * payloadBytes, uint32 numPayloadBytes)
{
   if (_outgoingContext == NULL) return B_BAD_OBJECT;

   const uint32 numBytesToSign = numPayloadBytes+SIGNED_TRAILER_SIZE;

   char errBuf[256];  // ERR_error_string() docs require this buffer to be at least 256 bytes long
   if (_outgoingContextIsHMAC)
   {
      if (_preSharedKey() == NULL) return B_BAD_OBJECT;

      EVP_MAC_CTX * ctx = reinterpret_cast<EVP_MAC_CTX *>(_outgoingContext);
      const status_t r = InitMACAux(ctx, _preSharedKey, errBuf);
      if (r.IsOK())
      {
         return GenerateSignedOutputDataAux(payloadBytes, numPayloadBytes, numBytesToSign, EVP_MAC_CTX_get_mac_size(ctx), errBuf);
      }
      else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_MAC_init() failed! [%s]\n", r());  // don't return r, it will dangle if you do
   }
   else
   {
      if (_privateKey == NULL) return B_BAD_OBJECT;

      EVP_MD_CTX * ctx = reinterpret_cast<EVP_MD_CTX *>(_outgoingContext);

      EVP_PKEY * privateKey = reinterpret_cast<EVP_PKEY *>(_privateKey);
      if (EVP_DigestSignInit(ctx, NULL, GetMessageDigestType(privateKey), NULL, privateKey) > 0)
      {
         size_t numSigBytes = 0;
         if (EVP_DigestSign(ctx, NULL, &numSigBytes, payloadBytes, numBytesToSign) > 0)  // this just populates (numSigBytes); it will generally always set it to 64, but just for form's sake
         {
            return GenerateSignedOutputDataAux(payloadBytes, numPayloadBytes, numBytesToSign, numSigBytes, errBuf);
         }
         else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_DigestSignUpdate() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
      }
      else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_DigestSignInit() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
   }

   return B_SSL_ERROR;
}

status_t SSLSignedPacketProxyDataIO :: GenerateSignedOutputDataAux(const uint8 * payloadBytes, uint32 numPayloadBytes, uint32 numBytesToSign, size_t numSigBytes, char * errBuf)
{
   MRETURN_ON_ERROR(_scratchBuf.SetNumBytes(numBytesToSign+((uint32)numSigBytes)+UNSIGNED_TRAILER_SIZE, false));

   uint8 * scratchPayload = _scratchBuf.GetBuffer();

   DataFlattener signedFlat(scratchPayload, numBytesToSign);
   signedFlat.WriteBytes(payloadBytes, numPayloadBytes);
   signedFlat.WriteInt64(_outgoingSessionID);
   signedFlat.WriteInt64(_outgoingPacketIDCounter);

   EVP_MAC_CTX * optMacCtx = _outgoingContextIsHMAC ? reinterpret_cast<EVP_MAC_CTX *>(_outgoingContext) : NULL;

   if ((optMacCtx)&&(EVP_MAC_update(optMacCtx, scratchPayload, numBytesToSign) <= 0))
   {
      LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputData(): EVP_MAC_update() failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));
      return B_SSL_ERROR;
   }

   uint8 * sigPtr = scratchPayload+numBytesToSign;

   if ((optMacCtx
        ? EVP_MAC_final(optMacCtx, sigPtr, NULL, numSigBytes)
        : EVP_DigestSign(reinterpret_cast<EVP_MD_CTX *>(_outgoingContext), sigPtr, &numSigBytes, scratchPayload, numBytesToSign)) > 0)  // writes out the signature into our scratch-buffer
   {
      DataFlattener unsignedFlat(sigPtr+numSigBytes, UNSIGNED_TRAILER_SIZE);
      unsignedFlat.WriteInt32(numPayloadBytes);
      unsignedFlat.WriteInt32(MAGIC_WORD);  // just to make it easy to sanity-check that the unsigned-trailer exists

      _scratchBuf.TruncateToLength(numPayloadBytes+(uint32)numSigBytes+SIGNED_TRAILER_SIZE+UNSIGNED_TRAILER_SIZE);  // just in case (numSigBytes) was set smaller by the second call to EVP_DigestSign()
      return B_NO_ERROR;
   }
   else LogTime(MUSCLE_LOG_ERROR, "GenerateSignedOutputDataAux(): EVP_DigestSignFinal failed! [%s]\n", ERR_error_string(ERR_get_error(), errBuf));

   return B_SSL_ERROR;
}

SSLSignedPacketProxyDataIO :: ReceivedPacketIDsWindow :: ReceivedPacketIDsWindow(const ByteBufferRef & bitmap, uint64 firstReceivedPacketID)
   : _bitmap(bitmap)
   , _largestReceivedPacketID(firstReceivedPacketID)
   , _largestPacketIDBitIndex(0)
{
   ClearBitmap();
   // deliberately not calling SetBit(GetBitIndexForPacketID(_largestReceivedPacketID), true) here because the code that constructed us will call MaybeAcceptPacket(firstReceivedPacketID) right after this
}

bool SSLSignedPacketProxyDataIO :: ReceivedPacketIDsWindow :: MaybeAcceptPacket(uint64 newPacketID)
{
   if (newPacketID > _largestReceivedPacketID)
   {
      // advance our window, and accept the packet
      const uint64 advanceBy = (newPacketID - _largestReceivedPacketID);

      if (advanceBy >= GetWindowSize()) ClearBitmap(); // Oops, this blows our entire window, so we might as well just reset
      else
      {
         // zero out the oldest bits, whose meaning we are re-assigning
         const uint64 smallestPacketID = GetSmallestPacketIDInWindow();
         for (uint64 i=0; i<advanceBy; i++) SetBit(GetBitIndexForPacketID(smallestPacketID+i), false);

         _largestPacketIDBitIndex = (_largestPacketIDBitIndex + advanceBy) % GetWindowSize();
      }

      _largestReceivedPacketID = newPacketID;
      SetBit(GetBitIndexForPacketID(_largestReceivedPacketID), true);
   }
   else
   {
      if (newPacketID < GetSmallestPacketIDInWindow()) return false; // packet ID is too old for our window, sorry

      const uint32 bitIndex = GetBitIndexForPacketID(newPacketID);
      if (IsBitSet(bitIndex)) return false;            // we don't accept duplicate packets, sorry
                         else SetBit(bitIndex, true);  // mark that we received a packet with ID (newPacketID)
   }

   return true;
}

void SSLSignedPacketProxyDataIO :: ShutdownAux()
{
   ResetIncomingContext();
   ResetOutgoingContext();

   _preSharedKey.Reset();

   ProxyDataIO::Shutdown();
}

} // end namespace muscle
