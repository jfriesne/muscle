/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZLibDataIO_h
#define ZLibDataIO_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "dataio/DataIO.h"

namespace muscle {

class ZLibDataIOImp;

/** This class wraps around another DataIO and transparently compresses all
  * data going to that DataIO, and decompresses all data coming from that
  * dataIO.
  */
class ZLibDataIO : public DataIO
{
public:
   /** Default Constructor -- Be sure to call SetChildDataIO() before use.
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
   const DataIORef & GetChildDataIO() const;

protected:
   /** Internal constructor for GZLibDataIO to call
     * @param childIO Reference to the DataIO object to pass compressed data to/from.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     * @param useGZFormat set true to use the .GZ-compatible output mode
     */
   ZLibDataIO(const DataIORef & childIO, int compressionLevel, bool useGZFormat);

private:
   ZLibDataIOImp * _imp;

   DECLARE_COUNTED_OBJECT(ZLibDataIO);
};
DECLARE_REFTYPES(ZLibDataIO);

/** This class is the same as ZLibDataIO, except that deflated data it
  * produces is compatible with the .gz file format.
  */
class GZLibDataIO : public ZLibDataIO
{
public:
   /** Default Constructor -- Be sure to call SetChildDataIO() before use.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   GZLibDataIO(int compressionLevel = 6) : ZLibDataIO(DataIORef(), compressionLevel, true) {/* empty */}

   /** Constructor
     * @param childIO Reference to the DataIO object to pass compressed data to/from.
     * @param compressionLevel how much to compress outgoing data.  0 is no
     *                         compression 9 is maximum compression.  Default is 6.
     */
   GZLibDataIO(const DataIORef & childIO, int compressionLevel = 6) : ZLibDataIO(childIO, compressionLevel, true) {/* empty */}

private:
   DECLARE_COUNTED_OBJECT(GZLibDataIO);
};
DECLARE_REFTYPES(GZLibDataIO);

} // end namespace muscle

#endif

#endif
