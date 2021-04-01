/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibCodec_h
#define ZLibCodec_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "support/NotCopyable.h"
# include "util/ByteBuffer.h"
# include "util/RefCount.h"
# include "zlib/zlib/zlib.h"

namespace muscle {
 
class DataIO;

/** This class is a handy wrapper around the zlib C functions.
  * It quickly and easily inflates and deflates data to/from independently compressed chunks.
  */
class ZLibCodec MUSCLE_FINAL_CLASS : public RefCountable, public NotCopyable
{
public:
   /** Constructor.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression, 9 is maximum compression.  Default is 6.
     */
   ZLibCodec(int compressionLevel = 6);

   /** Destructor */
   ~ZLibCodec();

   /** Returns this codec's compression level, as was specified in the constructor.
     * Note that this value only affects the behavior of Deflate() -- we can Inflate() data of any compression
     * level, although the compression level cannot change from one Inflate() call to another, unless the data
     * was compressed with the (independent) argument set to true.
     */
   int GetCompressionLevel() const {return _compressionLevel;}

   /** Given a buffer of raw data, returns a reference to a Buffer containing the corresponding compressed data.
     * @param rawData The raw data to compress
     * @param numBytes The number of bytes (rawData) points to
     * @param independent If true, the generated buffer will be decompressible on its
     *                    own, not depending on any previously decompressed data.
     *                    If false, the generated buffer will only be uncompressable
     *                    if the previously Deflate()'d buffers have been reinflated
     *                    before it.  Setting this value to true will reduce the
     *                    compression efficiency, but allows for more flexibility.
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
     * @returns Reference to a buffer of compressed data on success, or a NULL reference on failure.
     */
   ByteBufferRef Deflate(const uint8 * rawData, uint32 numBytes, bool independent, uint32 addHeaderBytes=0, uint32 addFooterBytes=0);

   /** As above, except the deflated data is written into an existing ByteBuffer object rather than
     * allocating a new ByteBuffer from the byte-buffer pool.
     * @param rawData The raw data to compress
     * @param numBytes The number of bytes (rawData) points to
     * @param independent If true, the generated buffer will be decompressible on its
     *                    own, not depending on any previously decompressed data.
     *                    If false, the generated buffer will only be uncompressable
     *                    if the previously Deflate()'d buffers have been reinflated
     *                    before it.  Setting this value to true will reduce the
     *                    compression efficiency, but allows for more flexibility.
     * @param targetBuf On success, this ByteBuffer object will contain the deflated data.
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
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t Deflate(const uint8 * rawData, uint32 numBytes, bool independent, ByteBuffer & targetBuf, uint32 addHeaderBytes=0, uint32 addFooterBytes=0);

   /** Given a buffer of raw data, returns a reference to a Buffer containing the corresponding compressed data.
     * @param rawData The raw data to compress
     * @param independent If true, the generated buffer will be decompressible on its
     *                    own, not depending on any previously decompressed data.
     *                    If false, the generated buffer will only be uncompressable
     *                    if the previously Deflate()'d buffers have been reinflated
     *                    before it.  Setting this value to true will reduce the
     *                    compression efficiency, but allows for more flexibility.
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
     * @returns Reference to a buffer of compressed data on success, or a NULL reference on failure.
     */
   ByteBufferRef Deflate(const ByteBuffer & rawData, bool independent, uint32 addHeaderBytes=0, uint32 addFooterBytes=0) {return Deflate(rawData.GetBuffer(), rawData.GetNumBytes(), independent, addHeaderBytes, addFooterBytes);}

   /** As above, except the deflated data is written into an existing ByteBuffer object rather than
     * allocating a new ByteBuffer from the byte-buffer pool.
     * @param rawData The raw data to compress
     * @param independent If true, the generated buffer will be decompressible on its
     *                    own, not depending on any previously decompressed data.
     *                    If false, the generated buffer will only be uncompressable
     *                    if the previously Deflate()'d buffers have been reinflated
     *                    before it.  Setting this value to true will reduce the
     *                    compression efficiency, but allows for more flexibility.
     * @param targetBuf On success, this ByteBuffer object will contain the deflated data.
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
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t Deflate(const ByteBuffer & rawData, bool independent, ByteBuffer & targetBuf, uint32 addHeaderBytes=0, uint32 addFooterBytes=0) {return Deflate(rawData.GetBuffer(), rawData.GetNumBytes(), independent, targetBuf, addHeaderBytes, addFooterBytes);}

   /** Given a buffer of compressed data, returns a reference to a Buffer containing the corresponding raw data, or NULL on failure.
     * @param compressedData The compressed data to expand.  This should be data that was previously produced by the Deflate() method.
     * @param numBytes The number of bytes (compressedData) points to
     * @returns Reference to a buffer of decompressed data on success, or a NULL reference on failure.
     */
   ByteBufferRef Inflate(const uint8 * compressedData, uint32 numBytes);

   /** As above, except that the inflated data is written into an existing ByteBuffer object rather
     * than allocating a new ByteBuffer from the byte-buffer pool.
     * @param compressedData The compressed data to expand.  This should be data that was previously produced by the Deflate() method.
     * @param numBytes The number of bytes (compressedData) points to
     * @param targetBuf On success, this ByteBuffer object will contain the inflated data.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t Inflate(const uint8 * compressedData, uint32 numBytes, ByteBuffer & targetBuf);

   /** Given a buffer of compressed data, returns a reference to a Buffer containing the corresponding raw data, or NULL on failure.
     * @param compressedData The compressed data to expand.  This should be data that was previously produced by the Deflate() method.
     * @returns Reference to a buffer of decompressed data on success, or a NULL reference on failure.
     */
   ByteBufferRef Inflate(const ByteBuffer & compressedData) {return Inflate(compressedData.GetBuffer(), compressedData.GetNumBytes());}

   /** As above, except that the inflated data is written into an existing ByteBuffer object rather
     * than allocating a new ByteBuffer from the byte-buffer pool.
     * @param compressedData The compressed data to expand.  This should be data that was previously produced by the Deflate() method.
     * @param targetBuf On success, this ByteBuffer object will contain the deflated data.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t Inflate(const ByteBuffer & compressedData, ByteBuffer & targetBuf) {return Inflate(compressedData.GetBuffer(), compressedData.GetNumBytes(), targetBuf);}

   /** Given a ByteBuffer that was previously produced by Deflate(), returns the number of bytes
     * of raw data that the buffer represents, or -1 if the buffer isn't recognized as valid.
     * @param compressedData Pointer to data that was previously created by ZLibCodec::Deflate().
     * @param numBytes The number of bytes (compressedData) points to
     * @param optRetIsIndependent If non-NULL, the bool that this argument points to will have
     *                            the independent/non-independent state of this buffer written into it.
     *                            See Deflate()'s documentation for details.
     * @returns the number of bytes of raw data that would be produced by Inflating (compressedData).
     */
   int32 GetInflatedSize(const uint8 * compressedData, uint32 numBytes, bool * optRetIsIndependent = NULL) const;

   /** Given a ByteBuffer that was previously produced by Deflate(), returns the number of bytes
     * of raw data that the buffer represents, or -1 if the buffer isn't recognized as valid.
     * @param compressedData Pointer to data that was previously created by ZLibCodec::Deflate().
     * @param optRetIsIndependent If non-NULL, the bool that this argument points to will have
     *                            the independent/non-independent state of this buffer written into it.
     *                            See Deflate()'s documentation for details.
     * @returns the number of bytes of raw data that would be produced by Inflating (compressedData).
     */
   int32 GetInflatedSize(const ByteBuffer & compressedData, bool * optRetIsIndependent = NULL) const {return GetInflatedSize(compressedData.GetBuffer(), compressedData.GetNumBytes(), optRetIsIndependent);}

   /** Convenience method for deflating large amounts of data without having to hold all of it in RAM at once.
     * @note All DataIO objects should be set to blocking mode, as this is a synchronous operation.
     * @param sourceRawIO The DataIO object to read uncompressed data from
     * @param destDeflatedIO The DataIO object to write compressed data to (data written will be equivalent to the data returned by DeflateByteBuffer())
     * @param independent If true, the generated buffer will be decompressible on its
     *                    own, not depending on any previously decompressed data.
     *                    If false, the generated buffer will only be uncompressable
     *                    if the previously Deflate()'d buffers have been reinflated
     *                    before it.  Setting this value to true will reduce the
     *                    compression efficiency, but allows for more flexibility.
     * @param numRawBytes The number of bytes of raw data we should read from (sourceRawIO).
     *                    Note that if fewer than (this many) bytes of data can be read from (sourceRawIO) then this operation will fail.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t ReadAndDeflateAndWrite(DataIO & sourceRawIO, DataIO & destDeflatedIO, bool independent, uint32 numRawBytes);

   /** Convenience method for inflating large amounts of data without having to hold all of it in RAM at once.
     * @note All DataIO objects should be set to blocking mode, as this is a synchronous operation.
     * @param sourceDeflatedIO The DataIO to read the zlib-compressed/deflated data from.  (Should be data that was previously produced by ReadAndDeflateAndWrite())
     * @param destInflatedIO The DataIO to write the inflated/raw data to.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t ReadAndInflateAndWrite(DataIO & sourceDeflatedIO, DataIO & destInflatedIO);

private:
   void InitStream(z_stream & stream);

   int _compressionLevel;

   bool _inflateOkay;
   z_stream _inflater;

   bool _deflateOkay;
   z_stream _deflater;
};
DECLARE_REFTYPES(ZLibCodec);

} // end namespace muscle

#endif

#endif
