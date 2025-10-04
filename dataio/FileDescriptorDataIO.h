/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFileDescriptorDataIO_h
#define MuscleFileDescriptorDataIO_h

#include "dataio/SeekableDataIO.h"  // keep this above the ifndef WIN32 line below, otherwise WIN32 might not be set appropriately

// Might as well include these here, since any code using FileDescriptorDataIO is probably going to need them
#ifndef WIN32
# include <fcntl.h>
# include <stdio.h>
# include <unistd.h>
# include <errno.h>
#endif

namespace muscle {

/**
 *  A DataIO for communicating using a POSIX-style file descriptor -- useful for talking to Linux device drivers and the like.
 *  @note This class doesn't do anything useful under Windows, since Windows never uses POSIX-style file descriptors.
 *        The equivalent class for Windows is Win32FileHandleDataIO.
 */
class FileDescriptorDataIO : public SeekableDataIO
{
public:
   /**
    *  Constructor.
    *  @param fd The file descriptor to use.  Becomes property of this FileDescriptorDataIO object.
    *  @param blocking determines whether to use blocking or non-blocking I/O.
    *  If you will be using this object with a AbstractMessageIOGateway,
    *  and/or select(), then it's usually better to set blocking to false.
    */
   FileDescriptorDataIO(const ConstSocketRef & fd, bool blocking);

   /** Destructor.
    *  close()'s the held file descriptor.
    */
   virtual ~FileDescriptorDataIO();

   /** Reads bytes from the file descriptor and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or an error code on error.
    *  @see DataIO::Read()
    */
   virtual io_status_t Read(void * buffer, uint32 size);

   /**
    * Reads bytes from (buffer) and sends them out to the file descriptor.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes written, or an error code on error.
    *  @see DataIO::Write()
    */
   virtual io_status_t Write(const void * buffer, uint32 size);

   /**
    *  Implemented as a no-op (I don't believe file descriptors need flushing?)
    */
   virtual void FlushOutput();

   /**
    * Enables or disables blocking I/O on this file descriptor.
    * If this object is to be used by an AbstractMessageIOGateway,
    * then non-blocking I/O is usually better to use.
    * @param blocking If true, file descriptor is set to blocking I/O mode.  Otherwise, non-blocking I/O.
    * @return B_NO_ERROR on success, or an error code on error.
    */
   status_t SetBlockingIOEnabled(bool blocking);

   /** Returns true iff this object is using blocking I/O mode. */
   MUSCLE_NODISCARD bool IsBlockingIOEnabled() const {return _blocking;}

   /** Clears our held ConstSocketRef. */
   virtual void Shutdown();

   /** Seeks to the specified point in the file stream.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END.
    *  @return B_NO_ERROR on success, or an error code on failure.
    */
   virtual status_t Seek(int64 offset, int whence);

   MUSCLE_NODISCARD virtual int64 GetPosition() const;
   MUSCLE_NODISCARD virtual int64 GetLength() const;
   virtual status_t Truncate();

   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket()  const {return _fd;}
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _fd;}

   /** Set whether or not this object should call fsync() on our file descriptor in the FileDescriptorDataIO destructor.  Defaults to false.
     * @param doFsyncOnClose If true, fsync(fd) will be called in our destructor.  If false (the default), it won't be.
     */
   void SetFSyncOnClose(bool doFsyncOnClose) {_dofSyncOnClose = doFsyncOnClose;}

   /** Returns whether or not this object should call fsync() on our file descriptor in the FileDescriptorDataIO destructor.  Defaults to false. */
   MUSCLE_NODISCARD bool IsFSyncOnClose() const {return _dofSyncOnClose;}

private:
   ConstSocketRef _fd;
   bool _blocking;
   bool _dofSyncOnClose;

   DECLARE_COUNTED_OBJECT(FileDescriptorDataIO);
};
DECLARE_REFTYPES(FileDescriptorDataIO);

} // end namespace muscle

#endif
