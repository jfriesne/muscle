#include "dataio/FileDataIO.h"
#include "dataio/SSLSocketDataIO.h"
#include "dataio/SSLSocketDataIO.h"

// keep these AFTER the MUSCLE includes, or Windows throws a fit
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace muscle {

#define CAST_SSL ((SSL*)_ssl)
#define CAST_CTX ((SSL_CTX*)_ctx)

SSLSocketDataIO :: SSLSocketDataIO(const ConstSocketRef & sockfd, bool blocking, bool accept) : TCPSocketDataIO(sockfd, blocking), _sslState(0), _forceReadReady(false)
{
   bool ok = false;
   ConstSocketRef tempSocket;  // yes, it's intentional that this socket will be closed as soon as we live this scope
   if (CreateConnectedSocketPair(tempSocket, _alwaysReadableSocket) == B_NO_ERROR)
   {
      _ctx = SSL_CTX_new(SSLv3_method());  // Using SSLv3_method() instead of SSLv23_method to avoid errors from SSL_pending()
      if (_ctx)
      {
         if (!blocking) SSL_CTX_set_mode(CAST_CTX, SSL_MODE_ENABLE_PARTIAL_WRITE);

         _ssl = SSL_new(CAST_CTX);
         if (_ssl)
         {
            BIO * sbio = BIO_new_socket(sockfd.GetFileDescriptor(), BIO_NOCLOSE);
            if (sbio)
            {
               BIO_set_nbio(sbio, !blocking);
               SSL_set_bio(CAST_SSL, sbio, sbio);

               if (accept) SSL_set_accept_state(CAST_SSL);
                      else SSL_set_connect_state(CAST_SSL);

               ERR_print_errors_fp(stderr);

               ok = true;
            }
            else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  BIO_new_socket() failed!\n");
         }
         else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  SSL_new() failed!\n");
      }
      else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  SSL_CTX_new() failed!\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "SSLSocketDataIO:  Error setting up dummy socket pair!\n");

   if (ok == false) Shutdown();  // might as well clean up now
}

SSLSocketDataIO :: ~SSLSocketDataIO() 
{
   Shutdown();
}

void SSLSocketDataIO :: Shutdown() 
{
   if (_ssl)  {SSL_shutdown(CAST_SSL); SSL_free(CAST_SSL); _ssl = NULL;}
   if (_ctx)  {SSL_CTX_free(CAST_CTX); _ctx = NULL;}
   TCPSocketDataIO::Shutdown();
}

status_t SSLSocketDataIO :: SetPrivateKey(const char * path) 
{
   return ((_ssl)&&(SSL_use_PrivateKey_file(CAST_SSL, path, SSL_FILETYPE_PEM) == 1)) ? B_NO_ERROR : B_ERROR;
}

status_t SSLSocketDataIO :: SetPrivateKey(const uint8 * bytes, uint32 numBytes)
{
   if (_ssl == NULL) return B_ERROR;

   status_t ret = B_ERROR;

   BIO * in = BIO_new_mem_buf((void *)bytes, numBytes);
   if (in)
   {
      EVP_PKEY * pkey = PEM_read_bio_PrivateKey(in, NULL, CAST_SSL->ctx->default_passwd_callback, CAST_SSL->ctx->default_passwd_callback_userdata);
      if (pkey)
      {
         if (SSL_use_PrivateKey(CAST_SSL, pkey) == 1) ret = B_NO_ERROR;
         EVP_PKEY_free(pkey);
      }
      BIO_free(in);
   }
   return ret;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const char * path) 
{
   if (_ssl == NULL) return B_ERROR;

   FileDataIO fdio(fopen(path, "rb"));
   if (fdio.GetFile() == NULL) return B_ERROR;

   ByteBufferRef buf = GetByteBufferFromPool((uint32)fdio.GetLength());
   return ((buf())&&(fdio.ReadFully(buf()->GetBuffer(), buf()->GetNumBytes()) == buf()->GetNumBytes())) ? SetPublicKeyCertificate(buf) : B_ERROR;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const uint8 * bytes, uint32 numBytes)
{       
   return _ssl ? SetPublicKeyCertificate(GetByteBufferFromPool(numBytes, bytes)) : B_ERROR;
}

status_t SSLSocketDataIO :: SetPublicKeyCertificate(const ConstByteBufferRef & buf)
{
   if (buf() == NULL) return B_ERROR;

   status_t ret = B_ERROR;

   BIO * in = BIO_new_mem_buf((void *)buf()->GetBuffer(), buf()->GetNumBytes());
   if (in)
   {
      X509 * x509Cert = PEM_read_bio_X509(in, NULL, CAST_SSL->ctx->default_passwd_callback, CAST_SSL->ctx->default_passwd_callback_userdata);
      if (x509Cert)
      {
         if (SSL_use_certificate(CAST_SSL, x509Cert) == 1) 
         {
            _publicKey = buf;
            ret = B_NO_ERROR;
         }
         X509_free(x509Cert);
      }
      BIO_free(in);
   }
   return ret;
}

int32 SSLSocketDataIO :: Read(void *buffer, uint32 size) 
{
   if (_ssl == NULL) return -1;

   int32 bytes = SSL_read(CAST_SSL, buffer, size);
        if (bytes > 0) _sslState &= ~(SSL_STATE_READ_WANTS_READABLE_SOCKET | SSL_STATE_READ_WANTS_WRITEABLE_SOCKET);
   else if (bytes == 0) return -1;  // connection was terminated
   else
   {
      int err = SSL_get_error(CAST_SSL, bytes);
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

   int32 bytes = SSL_write(CAST_SSL, buffer, size);
        if (bytes > 0) _sslState &= ~(SSL_STATE_WRITE_WANTS_READABLE_SOCKET | SSL_STATE_WRITE_WANTS_WRITEABLE_SOCKET);
   else if (bytes == 0) return -1;  // connection was terminated
   else
   {
      int err = SSL_get_error(CAST_SSL, bytes);
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
   return ((_forceReadReady)||((_ssl)&&(SSL_pending(CAST_SSL)>0))) ? _alwaysReadableSocket : TCPSocketDataIO::GetReadSelectSocket();
}

}; // end namespace muscle
