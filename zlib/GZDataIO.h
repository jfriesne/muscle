/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef GZDataIO_h
#define GZDataIO_h

#ifdef MUSCLE_ENABLE_ZLIB_ENCODING

# include "dataio/DataIO.h"

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
     * @param mode the mode-string to pass to gzopen() (eg "wb9" to write with maximum compression)
     */
   GZDataIO(const char * filePath, const char * mode);

   /** Destructor */
   virtual ~GZDataIO();

   virtual io_status_t Read(void * buffer, uint32 size);
   virtual io_status_t Write(const void * buffer, uint32 size);
   virtual void FlushOutput();
   virtual void Shutdown();

   /** Returns a NULL ConstSocketRef -- selecting on this class is not currently supported */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const;

   /** Returns a NULL ConstSocketRef -- selecting on this class is not currently supported */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const;

   /** Returns true iff our internal file-handle is currently valid */
   MUSCLE_NODISCARD bool IsFileOpen() const {return (_file != NULL);}

private:
   DECLARE_COUNTED_OBJECT(GZDataIO);

   void ShutdownAux();

   void * _file;  // really of type gzFile but I don't want to include zlib.h in this header, so
};
DECLARE_REFTYPES(GZDataIO);

} // end namespace muscle

#endif

#endif
