/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef GZDataIO_h
#define GZDataIO_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "dataio/DataIO.h"
# include "zlib/zlib/zlib.h"  // for gzFile

namespace muscle {
 
/** This class wraps around zlib's gzread() and gzwrite() APIs so that you
  * can use it to read or write .gz files.  This is currently implemented for
  * blocking I/O only.
  */
class GZDataIO : public DataIO
{
public:
   /** Constructor
     * @param filePath the path to the gz-compressed file to read or write
     * @param mode the mode-string to pass to gzopen() (e.g. "wb9" to write with maximum compression)
     */
   GZDataIO(const char * filePath, const char * mode);

   /** Destructor */
   virtual ~GZDataIO();

   virtual int32 Read(void * buffer, uint32 size);
   virtual int32 Write(const void * buffer, uint32 size);
   virtual void FlushOutput();
   virtual void Shutdown();

   /** Returns a NULL ConstSocketRef -- selecting on this class is not currently supported */
   virtual const ConstSocketRef & GetReadSelectSocket() const;

   /** Returns a NULL ConstSocketRef -- selecting on this class is not currently supported */
   virtual const ConstSocketRef & GetWriteSelectSocket() const;

private:
   DECLARE_COUNTED_OBJECT(GZDataIO);

   void ShutdownAux();

   gzFile _file;
};
DECLARE_REFTYPES(GZDataIO);

} // end namespace muscle

#endif

#endif
