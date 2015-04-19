/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibCodec_h
#define ZLibCodec_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "util/ByteBuffer.h"
# include "zlib/zlib/zlib.h"

namespace muscle {

/** This class is a handy wrapper around the zlib C functions.
  * It quickly and easily inflates and deflates data to/from independently compressed chunks.
  */
class ZLibCodec MUSCLE_FINAL_CLASS
{
public:
   /** Constructor.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression, 9 is maximum compression.  Default is 6.
     */
   ZLibCodec(int compressionLevel = 6);

   /** Destructor */
   ~ZLibCodec();

   /** Given a buffer of raw data, returns a reference to a Buffer containing
     * the matching compressed data.
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

   /** Given a buffer of compressed data, returns a reference to a Buffer containing
     * the matching raw data, or NULL on failure.
     * @param compressedData The compressed data to expand.  This should be data that was previously produced by the Deflate() method.
     * @param numBytes The number of bytes (compressedData) points to
     * @returns Reference to a buffer of decompressed data on success, or a NULL reference on failure.
     */
   ByteBufferRef Inflate(const uint8 * compressedData, uint32 numBytes);

   /** Given a ByteBuffer that was previously produced by Deflate(), returns the number of bytes
     * of raw data that the buffer represents, or -1 if the buffer isn't recognized as valid.
     * @param compressedData Pointer to data that was previously created by ZLibCodec::Deflate().
     * @param numBytes The number of bytes (compressedData) points to
     * @param optRetIsIndependent If non-NULL, the bool that this argument points to will have
     *                            the independent/non-independent state of this buffer written into it.
     *                            See Deflate()'s documentation for details.
     */
   int32 GetInflatedSize(const uint8 * compressedData, uint32 numBytes, bool * optRetIsIndependent = NULL) const;

   /** Returns this codec's compression level, as was specified in the constructor.
     * Note that this value only affects what we compress to -- we can compress any compression
     * level, although the compression level cannot change from one Inflate() call to another.
     */
   int GetCompressionLevel() const {return _compressionLevel;}

   // Convenience methods -- they work the same as their counterparts above, but take a ByteBuffer argument rather than a pointer and byte count.
   ByteBufferRef Deflate(const ByteBuffer & rawData, bool independent, uint32 addHeaderBytes=0, uint32 addFooterBytes=0) {return Deflate(rawData.GetBuffer(), rawData.GetNumBytes(), independent, addHeaderBytes, addFooterBytes);}
   ByteBufferRef Inflate(const ByteBuffer & compressedData) {return Inflate(compressedData.GetBuffer(), compressedData.GetNumBytes());}
   int32 GetInflatedSize(const ByteBuffer & compressedData, bool * optRetIsIndependent = NULL) const {return GetInflatedSize(compressedData.GetBuffer(), compressedData.GetNumBytes(), optRetIsIndependent);}

private:
   void InitStream(z_stream & stream);

   int _compressionLevel;

   bool _inflateOkay;
   z_stream _inflater;

   bool _deflateOkay;
   z_stream _deflater;
};

}; // end namespace muscle

#endif

#endif
