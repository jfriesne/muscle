/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibDataIO_h
#define ZLibDataIO_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "zlib/zlib/zlib.h"
# include "dataio/DataIO.h"

namespace muscle {

/** This class wraps around another DataIO and transparently compresses all
  * data going to that DataIO, and decompresses all data coming from that
  * dataIO.
  */
class ZLibDataIO : public DataIO
{
public:
   /** Default Constructor -- Be sure to call SetDataIO() before use.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     * @param emitGZipHeadersAndFooters if set true, this ZLibDataIO will include
     *                         the 10-byte .gz-file-format header at the start of its
     *                         output and the 8-byte .gz-file-format footer at the end
     *                         of its output, making its output readable by the gunzip
     *                         command line utility (and other .gz-reading software)
     *                         The argument currently has no effect when using this
     *                         ZLibDataIO for input -- maybe someday.  Defaults to false.
     */
   ZLibDataIO(int compressionLevel = 6, bool emitGZipHeadersAndFooters = false);

   /** Constructor
     * @param childIO Reference to the DataIO object to pass compressed data to/from.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     * @param emitGZipHeadersAndFooters if set true, this ZLibDataIO will include
     *                         the 10-byte .gz-file-format header at the start of its
     *                         output and the 8-byte .gz-file-format footer at the end
     *                         of its output, making its output readable by the gunzip
     *                         command line utility (and other .gz-reading software)
     *                         The argument currently has no effect when using this
     *                         ZLibDataIO for input -- maybe someday.  Defaults to false.
     */
   ZLibDataIO(const DataIORef & childIO, int compressionLevel = 6, bool emitGZipHeadersAndFooters = false);

   /** Destructor */
   virtual ~ZLibDataIO();

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual void FlushOutput();
   virtual void Shutdown();

   virtual const ConstSocketRef & GetReadSelectSocket() const;
   virtual const ConstSocketRef & GetWriteSelectSocket() const;

   virtual bool HasBufferedOutput() const;
   virtual void WriteBufferedOutput();

   /** Sets the child-data-IO we should use as our back-end for writing zlib-deflated bytes
     * or reading zlib-inflated bytes.
     * @param childDataIO the back-end DataIO to use.
     * @note you don't need to call this if you passed in the DataIORef to the ZLibDataIO constructor.
     * @returns B_NO_ERROR on success, or another value of failure (zlib initialization failed?)
     */
   status_t SetChildDataIO(const DataIORef & childDataIO);

   /** Returns our current back-end DataIORef, if we have one */
   const DataIORef & GetChildDataIO() const {return _childDataIO;}

private:
   void Init();
   void InitStream(z_stream & stream, uint8 * toStreamBuf, uint8 * fromStreamBuf, uint32 outBufSize);
   int32 WriteAux(const void * buffer, uint32 size, bool flushAtEnd);
   status_t WriteDeflatedOutputToChild();
   void CleanupZLib();

   DataIORef _childDataIO;

   int _compressionLevel;

   uint8 _toInflateBuf[2048];
   uint8 _inflatedBuf[2048];
   const uint8 * _sendToUser;
   bool _inflateAllocated;
   bool _inflateOkay;
   bool _inputStreamOkay;
   z_stream _readInflater;

   uint8 _toDeflateBuf[2048];
   uint8 _deflatedBuf[2048];
   const uint8 * _sendToChild;
   bool _deflateAllocated;
   z_stream _writeDeflater;

   const bool _emitGZipHeadersAndFooters;
   DECLARE_COUNTED_OBJECT(ZLibDataIO);
};
DECLARE_REFTYPES(ZLibDataIO);

} // end namespace muscle

#endif

#endif
