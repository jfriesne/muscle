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
class ZLibDataIO : public DataIO, private CountedObject<ZLibDataIO>
{
public:
   /** Default Constructor -- Be sure to call SetDataIO() before use.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   ZLibDataIO(int compressionLevel = 6);

   /** Constructor 
     * @param slaveIO Reference to the DataIO object to pass compressed data to/from.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   ZLibDataIO(const DataIORef & slaveIO, int compressionLevel = 6);

   /** Destructor */
   virtual ~ZLibDataIO();

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual status_t Seek(int64 offset, int whence);
   virtual int64 GetPosition() const;
   virtual uint64 GetOutputStallLimit() const;
   virtual void FlushOutput();
   virtual void Shutdown();
   virtual const ConstSocketRef & GetReadSelectSocket()  const;
   virtual const ConstSocketRef & GetWriteSelectSocket() const;

   virtual status_t GetReadByteTimeStamp(int32 whichByte, uint64 & retStamp) const;
   virtual bool HasBufferedOutput() const;
   virtual void WriteBufferedOutput();

   /** Returns the reference to the slave DataIO that was previously set in our constructor
     * or via SetSlaveIO().
     */
   DataIORef GetSlaveIO() const {return _slaveIO;}

   /** Sets our slave DataIO object, and resets our state. */
   void SetSlaveIO(const DataIORef & ref);

private:
   void Init();
   void InitStream(z_stream & stream, uint8 * toStreamBuf, uint8 * fromStreamBuf, uint32 outBufSize);
   int32 WriteAux(const void * buffer, uint32 size, bool flushAtEnd);
   void CleanupZLib();

   DataIORef _slaveIO;
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
   const uint8 * _sendToSlave;
   bool _deflateAllocated;
   z_stream _writeDeflater;
};

}; // end namespace muscle

#endif

#endif
