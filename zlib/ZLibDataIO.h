/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibDataIO_h
#define ZLibDataIO_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "zlib/zlib/zlib.h"
# include "dataio/ProxyDataIO.h"

namespace muscle {
 
/** This class wraps around another DataIO and transparently compresses all 
  * data going to that DataIO, and decompresses all data coming from that
  * dataIO.
  */
class ZLibDataIO : public ProxyDataIO
{
public:
   /** Default Constructor -- Be sure to call SetDataIO() before use.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   ZLibDataIO(int compressionLevel = 6);

   /** Constructor 
     * @param childIO Reference to the DataIO object to pass compressed data to/from.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   ZLibDataIO(const DataIORef & childIO, int compressionLevel = 6);

   /** Destructor */
   virtual ~ZLibDataIO();

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual void FlushOutput();
   virtual void Shutdown();

   virtual bool HasBufferedOutput() const;
   virtual void WriteBufferedOutput();

   virtual void SetChildDataIO(const DataIORef & childDataIO);

private:
   void Init();
   void InitStream(z_stream & stream, uint8 * toStreamBuf, uint8 * fromStreamBuf, uint32 outBufSize);
   int32 WriteAux(const void * buffer, uint32 size, bool flushAtEnd);
   void CleanupZLib();

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

   DECLARE_COUNTED_OBJECT(ZLibDataIO);
};
DECLARE_REFTYPES(ZLibDataIO);

} // end namespace muscle

#endif

#endif
