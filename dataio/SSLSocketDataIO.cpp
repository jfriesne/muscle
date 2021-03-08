#include "dataio/FileDataIO.h"
#include "dataio/SSLSocketDataIO.h"
#include "dataio/SSLSocketDataIO.h"

// keep these AFTER the MUSCLE includes, or Windows throws a fit
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace muscle {

SSLSocketDataIO :: SSLSocketDataIO(const ConstSocketRef & sockfd, bool blocking, bool accept)
   : TCPSocketDataIO(sockfd, blocking)
   , _isServer(accept)
   , _sslState(0)
   , _forceReadReady(false)
{
   status_t ret;
   bool ok = false;
   ConstSocketRef tempSocket;  // yes, it's intentional that this socket will be closed as soon as we exit this scope
   if (CreateConnectedSocketPair(tempSocket, _alwaysReadableSocket).IsOK(ret))
   {
      _ctx = SSL_CTX_new(TLS_method());
      if (_ctx)
      {
         if (!blocking) SSL_CTX_set_mode(_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

         _ssl = SSL_new(_ctx);
         if (_ssl)
         {
            if (SSL_set_ex_data(_ssl, 0, this) > 0)
            {
               BIO * sbio = BIO_new_socket(sockfd.GetFileDescriptor(), BIO_NOCLOSE);
               if (sbio)
               {
                  BIO_set_nbio(sbio, !blocking);
                  SSL_set_bio(_ssl, sbio, sbio);

                  if (accept) SSL_set_accept_state(_ssl);
                         else SSL_set_connect_state(_ssl);

                  ok = true;
               }
               else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  BIO_new_socket() failed!\n");
            }
            else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  SSL_CTX_set_ex_data() failed!\n");
         }
         else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  SSL_new() failed!\n");
      }
      else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  SSL_CTX_new() failed!\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  Error setting up dummy socket pair! [%s]\n", ret());

   if (ok == false) Shutdown();  // might as well clean up now
}

SSLSocketDataIO :: ~SSLSocketDataIO() 
{
   Shutdown();
}

void SSLSocketDataIO :: Shutdown() 
{
   if (_ssl)  {SSL_shutdown(_ssl); SSL_free(_ssl); _ssl = NULL;}
   if (_ctx)  {SSL_CTX_free(_ctx); _ctx = NULL;}
   TCPSocketDataIO::Shutdown();
}

status_t SSLSocketDataIO :: SetPrivateKey(const char * path) 
{
   if (_ssl == NULL) return B_BAD_OBJECT;
   return (SSL_use_PrivateKey_file(_ssl, path, SSL_FILETYPE_PEM) == 1) ? B_NO_ERROR : B_FILE_NOT_FOUND;
}

status_t SSLSocketDataIO :: SetPrivateKey(const uint8 * bytes, uint32 numBytes)
{
   if (_ssl == NULL) return B_BAD_OBJECT;

   status_t ret;

   BIO * in = BIO_new_mem_buf((void *)bytes, numBytes);
   if (in)
   {
      EVP_PKEY * pkey = PEM_read_bio_PrivateKey(in, NULL, SSL_get_default_passwd_cb(_ssl), SSL_get_default_passwd_cb_userdata(_ssl));
      if (pkey)
      {
         if (SSL_use_PrivateKey(_ssl, pkey) != 1) ret = B_SSL_ERROR;
         EVP_PKEY_free(pkey);
      }
      else ret = B_SSL_ERROR;

      BIO_free(in);
   }
   else ret = B_OUT_OF_MEMORY;

   return ret;
}

status_t SSLSocketDataIO :: SetPrivateKey(const ConstByteBufferRef & privateKeyFile)
{
   return privateKeyFile() ? SetPrivateKey(privateKeyFile()->GetBuffer(), privateKeyFile()->GetNumBytes()) : B_BAD_ARGUMENT;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const char * path) 
{
   if (_ssl == NULL) return B_BAD_OBJECT;

   FileDataIO fdio(muscleFopen(path, "rb"));
   if (fdio.GetFile() == NULL) return B_FILE_NOT_FOUND;

   ByteBufferRef buf = GetByteBufferFromPool((uint32)fdio.GetLength());
   MRETURN_OOM_ON_NULL(buf());

   return (fdio.ReadFully(buf()->GetBuffer(), buf()->GetNumBytes()) == buf()->GetNumBytes()) ? SetPublicKeyCertificate(buf) : B_IO_ERROR;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const uint8 * bytes, uint32 numBytes)
{       
   return _ssl ? SetPublicKeyCertificate(GetByteBufferFromPool(numBytes, bytes)) : B_BAD_OBJECT;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const ConstByteBufferRef & buf)
{
   if (buf() == NULL) return B_BAD_OBJECT;

   status_t ret;

   BIO * in = BIO_new_mem_buf((void *)buf()->GetBuffer(), buf()->GetNumBytes());
   if (in)
   {
      X509 * x509Cert = PEM_read_bio_X509(in, NULL, SSL_get_default_passwd_cb(_ssl), SSL_get_default_passwd_cb_userdata(_ssl));
      if (x509Cert)
      {
         if (SSL_use_certificate(_ssl, x509Cert) == 1) _publicKey = buf;
                                                  else ret = B_SSL_ERROR;
         X509_free(x509Cert);
      }
      else ret = B_SSL_ERROR;

      BIO_free(in);
   }
   else ret = B_OUT_OF_MEMORY;

   return ret;
}

unsigned int SSLSocketDataIO :: pskClientCallbackFunc(SSL * ssl, const char *hint, char * identity, unsigned int maxIdentityLen, unsigned char * psk, unsigned int maxPSKLen)
{
   SSLSocketDataIO * ssdio = (SSLSocketDataIO *) SSL_get_ex_data(ssl, 0);
   if (ssdio == NULL)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskClientCallback:  no SSLSocketDataIO object found!\n");
      return 0;
   }

   return ssdio->PSKClientCallback(hint, identity, maxIdentityLen, psk, maxPSKLen);
}

unsigned int SSLSocketDataIO :: pskServerCallbackFunc(SSL * ssl, const char *identity, unsigned char *outPSKBuf, unsigned int outPSKBufLen)
{
   SSLSocketDataIO * ssdio = (SSLSocketDataIO *) SSL_get_ex_data(ssl, 0);
   if (ssdio == NULL)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskServerCallback:  no SSLSocketDataIO object found!\n");
      return 0;
   }

   return ssdio->PSKServerCallback(identity, outPSKBuf, outPSKBufLen);
}

unsigned int SSLSocketDataIO :: PSKServerCallback(const char *identity, unsigned char * psk, unsigned int pskLen) const
{
   if (_pskUserName != identity)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskServerCallback:  unrecognized user name [%s]\n", identity);
      return 0;
   }

   if (_pskPassword.FlattenedSize() > pskLen)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskServerCallback:  output buffer not long enough to hold password!\n");
      return 0;  // failure
   }

   _pskPassword.Flatten((uint8 *)psk);
   return _pskPassword.Length();
}

unsigned int SSLSocketDataIO :: PSKClientCallback(const char * /*hint*/, char * identity, unsigned int maxIdentityLen, unsigned char * psk, unsigned int pskLen) const
{
   if (_pskUserName.FlattenedSize() > maxIdentityLen)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskClientCallback:  output buffer not long enough to hold identity!\n");
      return 0;  // failure
   }

   if (_pskPassword.FlattenedSize() > pskLen)
   {
      LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO::pskClientCallback:  output buffer not long enough to hold password!\n");
      return 0;  // failure
   }

   _pskUserName.Flatten((uint8 *)identity);
   _pskPassword.Flatten((uint8 *)psk);
   return _pskPassword.Length();
}

void SSLSocketDataIO :: SetPreSharedKeyLoginInfo(const String & userName, const String & password)
{
   _pskUserName = userName;
   _pskPassword = password;
   if (_isServer) SSL_set_psk_server_callback(_ssl, pskServerCallbackFunc);
             else SSL_set_psk_client_callback(_ssl, pskClientCallbackFunc);
}

int32 SSLSocketDataIO :: Read(void *buffer, uint32 size) 
{
   if (_ssl == NULL) return -1;

   int32 bytes = SSL_read(_ssl, buffer, size);
        if (bytes > 0) _sslState &= ~(SSL_STATE_READ_WANTS_READABLE_SOCKET | SSL_STATE_READ_WANTS_WRITEABLE_SOCKET);
   else if (bytes == 0) return -1;  // connection was terminated
   else
   {
      const int err = SSL_get_error(_ssl, bytes);
      if (err == SSL_ERROR_WANT_WRITE)
      {
         // We have to wait until our socket is writeable, and then repeat our SSL_read() call.
         _sslState &= ~SSL_STATE_READ_WANTS_READABLE_SOCKET;
         _sslState |=  SSL_STATE_READ_WANTS_WRITEABLE_SOCKET;
         bytes = 0;
      }
      else if (err == SSL_ERROR_WANT_READ) 
      {
         // We have to wait until our socket is readable, and then repeat our SSL_read() call.
         _sslState |=  SSL_STATE_READ_WANTS_READABLE_SOCKET;
         _sslState &= ~SSL_STATE_READ_WANTS_WRITEABLE_SOCKET;
         bytes = 0;
      }
      else
      {
         fprintf(stderr, "SSL_read() ERROR:  ");
         ERR_print_errors_fp(stderr);
      }
   }
   return bytes;
}

int32 SSLSocketDataIO :: Write(const void *buffer, uint32 size) 
{
   if (_ssl == NULL) return -1;

   int32 bytes = SSL_write(_ssl, buffer, size);
        if (bytes > 0) _sslState &= ~(SSL_STATE_WRITE_WANTS_READABLE_SOCKET | SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET);
   else if (bytes == 0) return -1;  // connection was terminated
   else
   {
      const int err = SSL_get_error(_ssl, bytes);
      if (err == SSL_ERROR_WANT_READ)
      {
         // We have to wait until our socket is readable, and then repeat our SSL_write() call.
         _sslState |=  SSL_STATE_WRITE_WANTS_READABLE_SOCKET;
         _sslState &= ~SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET;
         bytes = 0;
      }
      else if (err == SSL_ERROR_WANT_WRITE)
      {
         // We have to wait until our socket is writeable, and then repeat our SSL_write() call.
         _sslState &= ~SSL_STATE_WRITE_WANTS_READABLE_SOCKET;
         _sslState |=  SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET;
         bytes = 0;
      }
      else 
      {
         fprintf(stderr,"SSL_write() ERROR!");
         ERR_print_errors_fp(stderr);
      }
   }
   return bytes;
}

const ConstSocketRef & SSLSocketDataIO :: GetReadSelectSocket() const 
{
   return ((_forceReadReady)||((_ssl)&&(SSL_pending(_ssl)>0))) ? _alwaysReadableSocket : TCPSocketDataIO::GetReadSelectSocket();
}

} // end namespace muscle
