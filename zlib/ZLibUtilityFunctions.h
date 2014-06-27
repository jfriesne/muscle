/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibUtilityFunctions_h
#define ZLibUtilityFunctions_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

#include "message/Message.h"

namespace muscle {

/** @defgroup zlibutilityfunctions The ZLibUtilityFunctions function API
 *  These functions are all defined in ZLibUtilityFunctions(.cpp,.h), and are stand-alone
 *  functions that do give an easy-to-use, high-level way to compress and uncompress
 *  raw data and Message objects.
 *  @{
 */

/** Given some data, returns a ByteBuffer containing a compressed version of that date.
  * @param bytes Pointer to the data to compress
  * @param numBytes Number of bytes that (bytes) points to
  * @param compressionLevel The level of ZLib compression to use when creating the
  *                         compressed Message.  Should be between 0 (no compression)
  *                         and 9 (maximum compression).  Default value is 6.
  * @param addHeaderBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the beginning of the byte array,
  *                    before the first compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write header data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @param addFooterBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the end of the byte array,
  *                    after the last compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write footer data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @returns a reference to a compressed ByteBuffer on success, or a NULL reference on failure.
  */
ByteBufferRef DeflateByteBuffer(const uint8 * bytes, uint32 numBytes, int compressionLevel = 6, uint32 addHeaderBytes = 0, uint32 addFooterBytes = 0);

/** Given a ByteBuffer, returns a compressed version of the data.
  * @param buf a ByteBuffer to compress that you want to get a compressed version of.
  * @param compressionLevel The level of ZLib compression to use when creating the
  *                         compressed Message.  Should be between 0 (no compression)
  *                         and 9 (maximum compression).  Default value is 6.
  * @param addHeaderBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the beginning of the byte array,
  *                    before the first compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write header data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @param addFooterBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the end of the byte array,
  *                    after the last compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write footer data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @returns a reference to a compressed ByteBuffer (not (buf)!) on success, or a NULL reference on failure.
  */
static inline ByteBufferRef DeflateByteBuffer(const ByteBuffer & buf, int compressionLevel = 6, uint32 addHeaderBytes = 0, uint32 addFooterBytes = 0) {return DeflateByteBuffer(buf.GetBuffer(), buf.GetNumBytes(), compressionLevel, addHeaderBytes, addFooterBytes);}

/** Given a ByteBufferRef, returns a compressed version of the data.
  * @param buf reference to a ByteBuffer to compress that you want to get a compressed version of.
  * @param compressionLevel The level of ZLib compression to use when creating the
  *                         compressed Message.  Should be between 0 (no compression)
  *                         and 9 (maximum compression).  Default value is 6.
  * @param addHeaderBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the beginning of the byte array,
  *                    before the first compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write header data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @param addFooterBytes If set to non-zero, the returned ByteBuffer will contain
  *                    this many additional bytes at the end of the byte array,
  *                    after the last compressed-data byte.  The values in these bytes
  *                    are undefined; the caller can write footer data to them if desired.
  *                    Leave this set to zero if you're not sure of what you are doing.
  * @returns a reference to a compressed ByteBuffer (not (buf())!) on success, or a NULL reference on failure.
  */
static inline ByteBufferRef DeflateByteBuffer(const ByteBufferRef & buf, int compressionLevel = 6, uint32 addHeaderBytes = 0, uint32 addFooterBytes = 0) {return buf() ? DeflateByteBuffer(*buf(), compressionLevel, addHeaderBytes, addFooterBytes) : ByteBufferRef();}

/** Given come compressed data, returns a ByteBuffer containing the original/uncompressed data.
  * @param bytes Pointer to the data to uncompress
  * @param numBytes Number of bytes that (bytes) points to
  * @returns a reference to an uncompressed ByteBuffer on success, or a NULL reference on failure.
  */
ByteBufferRef InflateByteBuffer(const uint8 * bytes, uint32 numBytes);

/** Given a compressed ByteBuffer, returns the original/uncompressed version of the data.
  * @param buf a compressed ByteBuffer you want to get the decompressed version of.
  * @returns a reference to an uncompressed ByteBuffer (not (buf)!) on success, or a NULL reference on failure.
  */
static inline ByteBufferRef InflateByteBuffer(const ByteBuffer & buf) {return InflateByteBuffer(buf.GetBuffer(), buf.GetNumBytes());}

/** Given a reference to a compressed ByteBuffer, returns the original/uncompressed version of the data.
  * @param buf a compressed ByteBuffer you want to get the decompressed version of.
  * @returns a reference to an uncompressed ByteBuffer (not (buf())!) on success, or a NULL reference on failure.
  */
static inline ByteBufferRef InflateByteBuffer(const ByteBufferRef & buf) {return buf() ? InflateByteBuffer(*buf()) : ByteBufferRef();}

/** Returns true iff the given MessageRef points to a deflated Message.
  * Returns false iff the MessageRef is NULL or not deflated.
  * @param msgRef The Message to determine the inflated/deflated status of.
  */
bool IsMessageDeflated(const MessageRef & msgRef);

/** Examines the contents of the given Message, and creates and returns a new
 *  Message that represents the same data as the given Message, but in compressed form.
 *  If the passed-in Message is already in compressed form (i.e. it was created by
 *  a previous call to DeflateMessage()), or if the deflation didn't decrease the size
 *  any, then a reference to the original passed-in Message is returned instead.
 *  The returned Message is guaranteed to have the same 'what' code as the passed-in Message.
 *  If there is an error (out of memory?), a NULL reference is returned.
 *  @param msgRef The Message to create a compact version of.
 *  @param msgRef Reference to the newly generated compressed Message, or to the passed
 *                in Message, or a NULL reference on failure.
 *  @param compressionLevel The level of ZLib compression to use when creating the
 *                          compressed Message.  Should be between 0 (no compression)
 *                          and 9 (maximum compression).  Default value is 6.
 *  @param force If true, we will return a compressed Message even if the compressed Message's
 *               size is bigger than that of the original(!).  Otherwise, we'll return the
 *               original Message if the compression didn't actually make the Message's flattened
 *               size smaller.  Defaults to true.
 */
MessageRef DeflateMessage(const MessageRef & msgRef, int compressionLevel = 6, bool force=true);

/** Examines the given Message, and if it is a Message in compressed form (i.e. one
 *  that was previously created by DeflateMessage()), creates and returns the
 *  equivalent uncompressed Message.  If the passed-in Message was not in compressed
 *  form, then this function just returns a reference to the original passed-in Message.
 *  The returned Message is guaranteed to have the same 'what' code as the passed-in Message.
 *  Returns a NULL reference on failure (out of memory?)
 *  @param msgRef Message to examine and make an uncompressed equivalent of.
 *  @return Reference to an uncompressed Message on success, or a NULL reference on failure.
 */
MessageRef InflateMessage(const MessageRef & msgRef);

// This is the field name that we store deflated data into
#define MUSCLE_ZLIB_FIELD_NAME "_zlib"

/** @} */ // end of zlibutilityfunctions doxygen group

}; // end namespace muscle

#endif

#endif
