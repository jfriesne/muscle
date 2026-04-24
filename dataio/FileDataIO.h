/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFileDataIO_h
#define MuscleFileDataIO_h

#include "dataio/SeekableDataIO.h"

namespace muscle {

/**
 *  DataIO wrapper around a stdio (FILE *) file-handle.
 */
class FileDataIO : public SeekableDataIO
{
public:
   /** Constructor.
    *  @param file File to read from or write to.  Becomes property of this FileDataIO object,
    *         and will be fclose()'d when this object is deleted.  Defaults to NULL.
    */
   FileDataIO(FILE * file = NULL);

   /** Alternative constructor that can be used if you'd prefer to defer the call to fopen()
     * until when the file-handle is needed.
     * @param path the path to the file to open (will be passed as the first argument to fopen())
     * @param mode the mode of the file to open (will be passed as the second argument to fopen())
     * @note fopen() will be called the first time Read()/Write()/Seek()/etc are called on this object.
     */
   FileDataIO(const char * path, const char * mode);

   /** Destructor.
    *  Calls fclose() on the held file.
    */
   virtual ~FileDataIO();

   /** Reads bytes from our file and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or an error code on error.
    *  @see DataIO::Read()
    */
   virtual io_status_t Read(void * buffer, uint32 size);

   /** Takes bytes from (buffer) and writes them out to our file.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes written, or an error code on error.
    *  @see DataIO::Write()
    */
   virtual io_status_t Write(const void * buffer, uint32 size);

   /** Seeks to the specified point in the file.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END.
    *  @return B_NO_ERROR on success, an error code on failure.
    */
   virtual status_t Seek(int64 offset, int whence);

   /** Returns our current position in the file. */
   MUSCLE_NODISCARD virtual int64 GetPosition() const;

   /** Returns the current length of our file.  Currently this just calls up to the superclass-method;
     * this method is only here to demonstrate that I didn't forget to consider the implementation of it.
     */
   MUSCLE_NODISCARD virtual int64 GetLength() const {return SeekableDataIO::GetLength();}

   /** Truncates our file's length to its current seek-position */
   virtual status_t Truncate();

   /** Flushes the file output by calling fflush() */
   virtual void FlushOutput();

   /** Calls fclose() on the held file descriptor (if any) and forgets it */
   virtual void Shutdown();

   /**
    * Releases control of the contained FILE object to the calling code.
    * After this method returns, this object no longer owns or can
    * use or close the file descriptor descriptor it once held.
    */
   void ReleaseFile();

   /**
    * Returns the FILE object held by this object, or NULL if there is none.
    */
   MUSCLE_NODISCARD FILE * GetFile() const {return _file;}

   /**
    * Sets our file pointer to the specified handle, closing any previously held file handle first.
    * @param fp The new file handle.  If non-NULL, this FileDataIO becomes the owner of (fp).
    */
   void SetFile(FILE * fp);

   /** Call this to force a pending-file-open to occur (e.g. if you used the two-argument FileDataIO constructor,
     * and you want to force the file-open to happen now so you can check its result and handle any error)
     * @returns B_NO_ERROR on success (meaning the the file is now open), or B_BAD_OBJECT if there was no pending
     *                     file to open and no file was already open), or another error if the file-open operation failed.
     * @note you aren't required to call this method explicitly since the other methods of this class will call
     *       it implicitly if necessary.
     */
   status_t EnsureFileOpen();

   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket()  const {return _selectSocketRef;}
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _selectSocketRef;}

private:
   void SetSocketsFromFile(FILE * optFile);
   void FreePendingFileInfo();

   char * _pendingFilePath;  // used in deferred-mode only
   char * _pendingFileMode;  // ditto

   FILE * _file;
   ConstSocketRef _selectSocketRef;
#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE
   Socket _selectSocket;
#endif

   DECLARE_COUNTED_OBJECT(FileDataIO);
};
DECLARE_REFTYPES(FileDataIO);

} // end namespace muscle

#endif
