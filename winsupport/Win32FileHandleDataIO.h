/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleWin32FileHandleDataIO_h
#define MuscleWin32FileHandleDataIO_h

#include "dataio/SeekableDataIO.h"

namespace muscle {

/**
 *  Data I/O to and from a Win32 style file descriptor 
 */
class Win32FileHandleDataIO : public SeekableDataIO
{
public:
   /**
    *  Constructor.
    *  @param handle The file descriptor to use.  Becomes property of this Win32FileHandleDataIO object.
    */
   Win32FileHandleDataIO(::HANDLE handle);

   /** Destructor.
    *  close()'s the held file descriptor.
    */
   virtual ~Win32FileHandleDataIO();

   /** Reads bytes from the file descriptor and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or -1 on error.
    *  @see DataIO::Read()
    */
   virtual int32 Read(void * buffer, uint32 size);

   /**
    * Reads bytes from (buffer) and sends them out to the file descriptor.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes written, or -1 on error.
    *  @see DataIO::Write()    
    */
   virtual int32 Write(const void * buffer, uint32 size);

   /**
    *  Implemented as a no-op (I don't believe file descriptors need flushing?)
    */
   virtual void FlushOutput();

   /**
    * If implemented, this would enable or disable blocking I/O on this Win32FileHandleDataIO.
    * NOTE: Win32FileHandleDataIO currently does not use this flag; as only blocking-mode is supported.
    * @param blocking If true, indicates a desire to set this object to blocking I/O mode.  Otherwise, to non-blocking I/O.
    * @return B_NO_ERROR if (blocking) was passed in as true, or B_UNIMPLEMENTED if (blocking) was passed in as false.
    * @note this method is currently a no-op.
    */
   status_t SetBlockingIOEnabled(bool blocking);

   virtual void Shutdown();

   /** Seeks to the specified point in the file stream.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END. 
    *  @return B_NO_ERROR on success, an error code on failure.
    */ 
   virtual status_t Seek(int64 offset, int whence);

   /** Returns our current position in the file */
   virtual int64 GetPosition() const;

   /** Win32 HANDLES are not compatible with unix-style select, so this method always returns a NULL socket ref.  */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Win32 HANDLES are not compatible with unix-style select, so this method always returns a NULL socket ref.  */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

   /**
    * Releases control of the contained file descriptor to the calling code.
    * After this method returns, this object no longer owns or can
    * use or close the file descriptor descriptor it once held.
    */
   void ReleaseFileHandle() {_handle = INVALID_HANDLE_VALUE;}

   /**
    * Returns the file descriptor held by this object, or
    * INVALID_HANDLE_VALUE if there is none. 
    */
   ::HANDLE GetFileHandle() const {return _handle;}

private:
   ::HANDLE _handle;

   DECLARE_COUNTED_OBJECT(Win32FileHandleDataIO);
};
DECLARE_REFTYPES(Win32FileHandleDataIO);

} // end namespace muscle

#endif

