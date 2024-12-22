#include "util/MiscUtilityFunctions.h"
#include "util/IncrementalHashCalculator.h"

namespace muscle {

// NOLINTBEGIN

/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md5.c for more information.
 */

/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int MD5_u32plus;

typedef struct {
   MD5_u32plus lo, hi;
   MD5_u32plus a, b, c, d;
   unsigned char buffer[64];
   MD5_u32plus block[16];
} MD5_CTX;

/*
 * The basic MD5 functions.
 *
 * F and G are optimized compared to their RFC 1321 definitions for
 * architectures that lack an AND-NOT instruction, just like in Colin Plumb's
 * implementation.
 */
#define F(x, y, z)         ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)         ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)         (((x) ^ (y)) ^ (z))
#define H2(x, y, z)         ((x) ^ ((y) ^ (z)))
#define I(x, y, z)         ((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s) \
   (a) += f((b), (c), (d)) + (x) + (t); \
   (a) = (((a) << (s)) | (((a) & 0xffffffff) >> (32 - (s)))); \
   (a) += (b);

/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 *
 * The check for little-endian architectures that tolerate unaligned
 * memory accesses is just an optimization.  Nothing will break if it
 * doesn't work.
 */
#if defined(__i386__) || defined(__x86_64__) || defined(__vax__)
#define SET(n) \
   (*(MD5_u32plus *)&ptr[(n) * 4])
#define GET(n) \
   SET(n)
#else
#define SET(n) \
   (ctx->block[(n)] = \
   (MD5_u32plus)ptr[(n) * 4] | \
   ((MD5_u32plus)ptr[(n) * 4 + 1] << 8) | \
   ((MD5_u32plus)ptr[(n) * 4 + 2] << 16) | \
   ((MD5_u32plus)ptr[(n) * 4 + 3] << 24))
#define GET(n) \
   (ctx->block[(n)])
#endif

/*
 * This processes one or more 64-byte data blocks, but does NOT update
 * the bit counters.  There are no alignment requirements.
 */
static const void *body(MD5_CTX *ctx, const void *data, unsigned long size)
{
   const unsigned char *ptr;
   MD5_u32plus a, b, c, d;
   MD5_u32plus saved_a, saved_b, saved_c, saved_d;

   ptr = (const unsigned char *)data;

   a = ctx->a;
   b = ctx->b;
   c = ctx->c;
   d = ctx->d;

   do {
      saved_a = a;
      saved_b = b;
      saved_c = c;
      saved_d = d;

/* Round 1 */
      STEP(F, a, b, c, d, SET(0), 0xd76aa478, 7)
      STEP(F, d, a, b, c, SET(1), 0xe8c7b756, 12)
      STEP(F, c, d, a, b, SET(2), 0x242070db, 17)
      STEP(F, b, c, d, a, SET(3), 0xc1bdceee, 22)
      STEP(F, a, b, c, d, SET(4), 0xf57c0faf, 7)
      STEP(F, d, a, b, c, SET(5), 0x4787c62a, 12)
      STEP(F, c, d, a, b, SET(6), 0xa8304613, 17)
      STEP(F, b, c, d, a, SET(7), 0xfd469501, 22)
      STEP(F, a, b, c, d, SET(8), 0x698098d8, 7)
      STEP(F, d, a, b, c, SET(9), 0x8b44f7af, 12)
      STEP(F, c, d, a, b, SET(10), 0xffff5bb1, 17)
      STEP(F, b, c, d, a, SET(11), 0x895cd7be, 22)
      STEP(F, a, b, c, d, SET(12), 0x6b901122, 7)
      STEP(F, d, a, b, c, SET(13), 0xfd987193, 12)
      STEP(F, c, d, a, b, SET(14), 0xa679438e, 17)
      STEP(F, b, c, d, a, SET(15), 0x49b40821, 22)

/* Round 2 */
      STEP(G, a, b, c, d, GET(1), 0xf61e2562, 5)
      STEP(G, d, a, b, c, GET(6), 0xc040b340, 9)
      STEP(G, c, d, a, b, GET(11), 0x265e5a51, 14)
      STEP(G, b, c, d, a, GET(0), 0xe9b6c7aa, 20)
      STEP(G, a, b, c, d, GET(5), 0xd62f105d, 5)
      STEP(G, d, a, b, c, GET(10), 0x02441453, 9)
      STEP(G, c, d, a, b, GET(15), 0xd8a1e681, 14)
      STEP(G, b, c, d, a, GET(4), 0xe7d3fbc8, 20)
      STEP(G, a, b, c, d, GET(9), 0x21e1cde6, 5)
      STEP(G, d, a, b, c, GET(14), 0xc33707d6, 9)
      STEP(G, c, d, a, b, GET(3), 0xf4d50d87, 14)
      STEP(G, b, c, d, a, GET(8), 0x455a14ed, 20)
      STEP(G, a, b, c, d, GET(13), 0xa9e3e905, 5)
      STEP(G, d, a, b, c, GET(2), 0xfcefa3f8, 9)
      STEP(G, c, d, a, b, GET(7), 0x676f02d9, 14)
      STEP(G, b, c, d, a, GET(12), 0x8d2a4c8a, 20)

/* Round 3 */
      STEP(H, a, b, c, d, GET(5), 0xfffa3942, 4)
      STEP(H2, d, a, b, c, GET(8), 0x8771f681, 11)
      STEP(H, c, d, a, b, GET(11), 0x6d9d6122, 16)
      STEP(H2, b, c, d, a, GET(14), 0xfde5380c, 23)
      STEP(H, a, b, c, d, GET(1), 0xa4beea44, 4)
      STEP(H2, d, a, b, c, GET(4), 0x4bdecfa9, 11)
      STEP(H, c, d, a, b, GET(7), 0xf6bb4b60, 16)
      STEP(H2, b, c, d, a, GET(10), 0xbebfbc70, 23)
      STEP(H, a, b, c, d, GET(13), 0x289b7ec6, 4)
      STEP(H2, d, a, b, c, GET(0), 0xeaa127fa, 11)
      STEP(H, c, d, a, b, GET(3), 0xd4ef3085, 16)
      STEP(H2, b, c, d, a, GET(6), 0x04881d05, 23)
      STEP(H, a, b, c, d, GET(9), 0xd9d4d039, 4)
      STEP(H2, d, a, b, c, GET(12), 0xe6db99e5, 11)
      STEP(H, c, d, a, b, GET(15), 0x1fa27cf8, 16)
      STEP(H2, b, c, d, a, GET(2), 0xc4ac5665, 23)

/* Round 4 */
      STEP(I, a, b, c, d, GET(0), 0xf4292244, 6)
      STEP(I, d, a, b, c, GET(7), 0x432aff97, 10)
      STEP(I, c, d, a, b, GET(14), 0xab9423a7, 15)
      STEP(I, b, c, d, a, GET(5), 0xfc93a039, 21)
      STEP(I, a, b, c, d, GET(12), 0x655b59c3, 6)
      STEP(I, d, a, b, c, GET(3), 0x8f0ccc92, 10)
      STEP(I, c, d, a, b, GET(10), 0xffeff47d, 15)
      STEP(I, b, c, d, a, GET(1), 0x85845dd1, 21)
      STEP(I, a, b, c, d, GET(8), 0x6fa87e4f, 6)
      STEP(I, d, a, b, c, GET(15), 0xfe2ce6e0, 10)
      STEP(I, c, d, a, b, GET(6), 0xa3014314, 15)
      STEP(I, b, c, d, a, GET(13), 0x4e0811a1, 21)
      STEP(I, a, b, c, d, GET(4), 0xf7537e82, 6)
      STEP(I, d, a, b, c, GET(11), 0xbd3af235, 10)
      STEP(I, c, d, a, b, GET(2), 0x2ad7d2bb, 15)
      STEP(I, b, c, d, a, GET(9), 0xeb86d391, 21)

      a += saved_a;
      b += saved_b;
      c += saved_c;
      d += saved_d;

      ptr += 64;
   } while (size -= 64);

   ctx->a = a;
   ctx->b = b;
   ctx->c = c;
   ctx->d = d;

   return ptr;
}

static void MD5_Init(MD5_CTX *ctx)
{
   ctx->a = 0x67452301;
   ctx->b = 0xefcdab89;
   ctx->c = 0x98badcfe;
   ctx->d = 0x10325476;

   ctx->lo = 0;
   ctx->hi = 0;
}

static void MD5_Update(MD5_CTX *ctx, const void *data, unsigned long size)
{
   MD5_u32plus saved_lo;
   unsigned long used, available;

   saved_lo = ctx->lo;
   if ((ctx->lo = (saved_lo + size) & 0x1fffffff) < saved_lo)
      ctx->hi++;
   ctx->hi += size >> 29;

   used = saved_lo & 0x3f;

   if (used) {
      available = 64 - used;

      if (size < available) {
         memcpy(&ctx->buffer[used], data, size);
         return;
      }

      memcpy(&ctx->buffer[used], data, available);
      data = (const unsigned char *)data + available;
      size -= available;
      body(ctx, ctx->buffer, 64);
   }

   if (size >= 64) {
      data = body(ctx, data, size & ~(unsigned long)0x3f);
      size &= 0x3f;
   }

   memcpy(ctx->buffer, data, size);
}

static void MD5_Final(unsigned char *result, MD5_CTX *ctx)
{
   unsigned long used, available;

   used = ctx->lo & 0x3f;

   ctx->buffer[used++] = 0x80;

   available = 64 - used;

   if (available < 8) {
      memset(&ctx->buffer[used], 0, available);
      body(ctx, ctx->buffer, 64);
      used = 0;
      available = 64;
   }

   memset(&ctx->buffer[used], 0, available - 8);

   ctx->lo <<= 3;
   ctx->buffer[56] = ctx->lo;
   ctx->buffer[57] = ctx->lo >> 8;
   ctx->buffer[58] = ctx->lo >> 16;
   ctx->buffer[59] = ctx->lo >> 24;
   ctx->buffer[60] = ctx->hi;
   ctx->buffer[61] = ctx->hi >> 8;
   ctx->buffer[62] = ctx->hi >> 16;
   ctx->buffer[63] = ctx->hi >> 24;

   body(ctx, ctx->buffer, 64);

   result[0] = ctx->a;
   result[1] = ctx->a >> 8;
   result[2] = ctx->a >> 16;
   result[3] = ctx->a >> 24;
   result[4] = ctx->b;
   result[5] = ctx->b >> 8;
   result[6] = ctx->b >> 16;
   result[7] = ctx->b >> 24;
   result[8] = ctx->c;
   result[9] = ctx->c >> 8;
   result[10] = ctx->c >> 16;
   result[11] = ctx->c >> 24;
   result[12] = ctx->d;
   result[13] = ctx->d >> 8;
   result[14] = ctx->d >> 16;
   result[15] = ctx->d >> 24;

   memset(ctx, 0, sizeof(*ctx));
}

#undef F

/** SHA-1 code below was stolen from md5deep, which is a public-domain encryption package -- jaf */

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                             \
{                                                       \
    (n) = ( (unsigned long) (b)[(i)    ] << 24 )        \
        | ( (unsigned long) (b)[(i) + 1] << 16 )        \
        | ( (unsigned long) (b)[(i) + 2] <<  8 )        \
        | ( (unsigned long) (b)[(i) + 3]       );       \
}
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                             \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

typedef struct
{
    unsigned long total[2];     /*!< number of bytes processed  */
    unsigned long state[5];     /*!< intermediate digest state  */
    unsigned char buffer[64];   /*!< data block being processed */

    unsigned char ipad[64];     /*!< HMAC: inner padding        */
    unsigned char opad[64];     /*!< HMAC: outer padding        */
} sha1_context;

/*
 * SHA-1 context setup
 */
static void sha1_starts( sha1_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
}

static void sha1_process( sha1_context *ctx, const unsigned char data[64] )
{
    unsigned long temp, W[16], A, B, C, D, E;

    GET_ULONG_BE( W[ 0], data,  0 );
    GET_ULONG_BE( W[ 1], data,  4 );
    GET_ULONG_BE( W[ 2], data,  8 );
    GET_ULONG_BE( W[ 3], data, 12 );
    GET_ULONG_BE( W[ 4], data, 16 );
    GET_ULONG_BE( W[ 5], data, 20 );
    GET_ULONG_BE( W[ 6], data, 24 );
    GET_ULONG_BE( W[ 7], data, 28 );
    GET_ULONG_BE( W[ 8], data, 32 );
    GET_ULONG_BE( W[ 9], data, 36 );
    GET_ULONG_BE( W[10], data, 40 );
    GET_ULONG_BE( W[11], data, 44 );
    GET_ULONG_BE( W[12], data, 48 );
    GET_ULONG_BE( W[13], data, 52 );
    GET_ULONG_BE( W[14], data, 56 );
    GET_ULONG_BE( W[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
(                                                       \
    temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^     \
           W[(t - 14) & 0x0F] ^ W[ t      & 0x0F],      \
    ( W[t & 0x0F] = S(temp,1) )                         \
)

#define P(a,b,c,d,e,x)                                  \
{                                                       \
    e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);        \
}

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

    P( A, B, C, D, E, W[0]  );
    P( E, A, B, C, D, W[1]  );
    P( D, E, A, B, C, W[2]  );
    P( C, D, E, A, B, W[3]  );
    P( B, C, D, E, A, W[4]  );
    P( A, B, C, D, E, W[5]  );
    P( E, A, B, C, D, W[6]  );
    P( D, E, A, B, C, W[7]  );
    P( C, D, E, A, B, W[8]  );
    P( B, C, D, E, A, W[9]  );
    P( A, B, C, D, E, W[10] );
    P( E, A, B, C, D, W[11] );
    P( D, E, A, B, C, W[12] );
    P( C, D, E, A, B, W[13] );
    P( B, C, D, E, A, W[14] );
    P( A, B, C, D, E, W[15] );
    P( E, A, B, C, D, R(16) );
    P( D, E, A, B, C, R(17) );
    P( C, D, E, A, B, R(18) );
    P( B, C, D, E, A, R(19) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

    P( A, B, C, D, E, R(20) );
    P( E, A, B, C, D, R(21) );
    P( D, E, A, B, C, R(22) );
    P( C, D, E, A, B, R(23) );
    P( B, C, D, E, A, R(24) );
    P( A, B, C, D, E, R(25) );
    P( E, A, B, C, D, R(26) );
    P( D, E, A, B, C, R(27) );
    P( C, D, E, A, B, R(28) );
    P( B, C, D, E, A, R(29) );
    P( A, B, C, D, E, R(30) );
    P( E, A, B, C, D, R(31) );
    P( D, E, A, B, C, R(32) );
    P( C, D, E, A, B, R(33) );
    P( B, C, D, E, A, R(34) );
    P( A, B, C, D, E, R(35) );
    P( E, A, B, C, D, R(36) );
    P( D, E, A, B, C, R(37) );
    P( C, D, E, A, B, R(38) );
    P( B, C, D, E, A, R(39) );

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

    P( A, B, C, D, E, R(40) );
    P( E, A, B, C, D, R(41) );
    P( D, E, A, B, C, R(42) );
    P( C, D, E, A, B, R(43) );
    P( B, C, D, E, A, R(44) );
    P( A, B, C, D, E, R(45) );
    P( E, A, B, C, D, R(46) );
    P( D, E, A, B, C, R(47) );
    P( C, D, E, A, B, R(48) );
    P( B, C, D, E, A, R(49) );
    P( A, B, C, D, E, R(50) );
    P( E, A, B, C, D, R(51) );
    P( D, E, A, B, C, R(52) );
    P( C, D, E, A, B, R(53) );
    P( B, C, D, E, A, R(54) );
    P( A, B, C, D, E, R(55) );
    P( E, A, B, C, D, R(56) );
    P( D, E, A, B, C, R(57) );
    P( C, D, E, A, B, R(58) );
    P( B, C, D, E, A, R(59) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

    P( A, B, C, D, E, R(60) );
    P( E, A, B, C, D, R(61) );
    P( D, E, A, B, C, R(62) );
    P( C, D, E, A, B, R(63) );
    P( B, C, D, E, A, R(64) );
    P( A, B, C, D, E, R(65) );
    P( E, A, B, C, D, R(66) );
    P( D, E, A, B, C, R(67) );
    P( C, D, E, A, B, R(68) );
    P( B, C, D, E, A, R(69) );
    P( A, B, C, D, E, R(70) );
    P( E, A, B, C, D, R(71) );
    P( D, E, A, B, C, R(72) );
    P( C, D, E, A, B, R(73) );
    P( B, C, D, E, A, R(74) );
    P( A, B, C, D, E, R(75) );
    P( E, A, B, C, D, R(76) );
    P( D, E, A, B, C, R(77) );
    P( C, D, E, A, B, R(78) );
    P( B, C, D, E, A, R(79) );

#undef K
#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
}

/*
 * SHA-1 process buffer
 */
static void sha1_update( sha1_context *ctx, const unsigned char *input, size_t ilen )
{
    size_t fill;
    unsigned long left;

    if( ilen <= 0 )
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += (unsigned long) ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if( ctx->total[0] < (unsigned long) ilen )
        ctx->total[1]++;

    if( left && ilen >= fill )
    {
        memcpy( (void *) (ctx->buffer + left),
                (const void *) input, fill );
        sha1_process( ctx, ctx->buffer );
        input += fill;
        ilen  -= fill;
        left = 0;
    }

    while( ilen >= 64 )
    {
        sha1_process( ctx, input );
        input += 64;
        ilen  -= 64;
    }

    if( ilen > 0 )
    {
        memcpy( (void *) (ctx->buffer + left),
                (const void *) input, ilen );
    }
}

static const unsigned char sha1_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * SHA-1 final digest
 */
static void sha1_finish( sha1_context *ctx, unsigned char output[20] )
{
    unsigned long last, padn;
    unsigned long high, low;
    unsigned char msglen[8];

    high = ( ctx->total[0] >> 29 )
         | ( ctx->total[1] <<  3 );
    low  = ( ctx->total[0] <<  3 );

    PUT_ULONG_BE( high, msglen, 0 );
    PUT_ULONG_BE( low,  msglen, 4 );

    last = ctx->total[0] & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    sha1_update( ctx, (const unsigned char *) sha1_padding, padn );
    sha1_update( ctx, msglen, 8 );

    PUT_ULONG_BE( ctx->state[0], output,  0 );
    PUT_ULONG_BE( ctx->state[1], output,  4 );
    PUT_ULONG_BE( ctx->state[2], output,  8 );
    PUT_ULONG_BE( ctx->state[3], output, 12 );
    PUT_ULONG_BE( ctx->state[4], output, 16 );
}

String IncrementalHash :: ToString() const {return HexBytesToString(_hashBytes, ARRAYITEMS(_hashBytes));}

void IncrementalHashCalculator :: Reset()
{
   muscleClearArray(_union._stateBytes);

   switch(_algorithm)
   {
      case HASH_ALGORITHM_MD5:  MD5_Init(reinterpret_cast<MD5_CTX *>(_union._stateBytes));                                                             break;
      case HASH_ALGORITHM_SHA1: sha1_starts(reinterpret_cast<sha1_context *>(_union._stateBytes));                                                     break;
      default:                  LogTime(MUSCLE_LOG_CRITICALERROR, "IncrementalHashCalculator %p:  Unknown algorithm " UINT32_FORMAT_SPEC "\n", this, _algorithm); break;
   }
}

void IncrementalHashCalculator :: HashBytes(const uint8 * inBytes, uint32 numInBytes)
{
   switch(_algorithm)
   {
      case HASH_ALGORITHM_MD5:  MD5_Update( reinterpret_cast<MD5_CTX *>     (_union._stateBytes), inBytes, numInBytes); break;
      case HASH_ALGORITHM_SHA1: sha1_update(reinterpret_cast<sha1_context *>(_union._stateBytes), inBytes, numInBytes); break;
      default:                  /* empty */                                                                             break;
   }
}

IncrementalHash IncrementalHashCalculator :: GetCurrentHash() const
{
   // Make a copy of the hashing-state so we can finalize the copy without finalizing the ongoing-hash
   union {
      uint64 _forceAlignment;
      uint8 _stateBytes[MAX_STATE_SIZE];
   } tempUnion;
   memcpy(tempUnion._stateBytes, _union._stateBytes, sizeof(tempUnion._stateBytes));

   uint8 outBytes[IncrementalHash::MAX_HASH_RESULT_SIZE_BYTES];
   switch(_algorithm)
   {
      case HASH_ALGORITHM_MD5:  MD5_Final(outBytes, reinterpret_cast<MD5_CTX *>(tempUnion._stateBytes));        break;
      case HASH_ALGORITHM_SHA1: sha1_finish(reinterpret_cast<sha1_context *>(tempUnion._stateBytes), outBytes); break;
      default:                  muscleClearArray(outBytes); /* this is here solely to make pvs-studio happy */  break;
   }
   return IncrementalHash(outBytes, GetNumResultBytesUsedByAlgorithm(_algorithm));
}

IncrementalHash IncrementalHashCalculator :: CalculateHashSingleShot(uint32 algorithm, const uint8 * inBytes, uint32 numInBytes)
{
   IncrementalHashCalculator hc(algorithm);
   hc.HashBytes(inBytes, numInBytes);
   return hc.GetCurrentHash();
}

IncrementalHash IncrementalHashCalculator :: CalculateHashSingleShot(uint32 algorithm, DataIO & dio, uint64 maxNumBytesToRead)
{
   IncrementalHashCalculator hc(algorithm);

   while(maxNumBytesToRead > 0)
   {
      uint8 tempBuf[128*1024];
      const io_status_t rfRet = dio.ReadFullyUpTo(tempBuf, (uint32) muscleMin((uint64)sizeof(tempBuf), maxNumBytesToRead));
      if (rfRet.IsOK())
      {
         const uint32 numBytesRead = rfRet.GetByteCount();
         hc.HashBytes(tempBuf, numBytesRead);
         maxNumBytesToRead -= numBytesRead;
      }
      else break;
   }

   return hc.GetCurrentHash();
}

uint32 IncrementalHashCalculator :: GetNumResultBytesUsedByAlgorithm(uint32 algorithm)
{
   switch(algorithm)
   {
      case HASH_ALGORITHM_MD5:  return 16;
      case HASH_ALGORITHM_SHA1: return 20;
      default:                  return 0;
   }
}

}; // end namespace muscle
