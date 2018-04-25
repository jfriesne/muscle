/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFileDataIO_h
#define MuscleFileDataIO_h

#include "dataio/SeekableDataIO.h"

namespace muscle {

/**
 *  Data I/O to and from a stdio FILE. 
 */
class FileDataIO : public SeekableDataIO
{
public:
   /** Constructor.
    *  @param file File to read from or write to.  Becomes property of this FileDataIO object,
    *         and will be fclose()'d when this object is deleted.  Defaults to NULL.
    */
   FileDataIO(FILE * file = NULL) : _file(file) {SetSocketsFromFile(_file);}

   /** Destructor.
    *  Calls fclose() on the held file.
    */
   virtual ~FileDataIO() {if (_file) fclose(_file);}

   /** Reads bytes from our file and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or -1 on error.  
    *  @see DataIO::Read()
    */
   virtual int32 Read(void * buffer, uint32 size)  
   {
      if (_file)
      {
         int32 ret = (int32) fread(buffer, 1, size, _file);
         return (ret > 0) ? ret : -1;  // EOF is an error, and it's returned as zero
      }
      else return -1;
   }

   /** Takes bytes from (buffer) and writes them out to our file.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes written, or -1 on error.
    *  @see DataIO::Write()
    */
   virtual int32 Write(const void * buffer, uint32 size)
   {
      if (_file)
      {
         int32 ret = (int32) fwrite(buffer, 1, size, _file);
         return (ret > 0) ? ret : -1;   // zero is an error
      }
      else return -1;
   }

   /** Seeks to the specified point in the file.
    *  @note this subclass only supports 32-bit offsets.
    *  @param offset Where to seek to.
    *  @param whence IO_SEEK_SET, IO_SEEK_CUR, or IO_SEEK_END. 
    *  @return B_NO_ERROR on success, B_ERROR on failure.
    */ 
   virtual status_t Seek(int64 offset, int whence)
   {
      if (_file)
      {
         switch(whence)
         {
            case IO_SEEK_SET:  whence = SEEK_SET;  break;
            case IO_SEEK_CUR:  whence = SEEK_CUR;  break;
            case IO_SEEK_END:  whence = SEEK_END;  break;
            default:           return B_ERROR;
         }
         if (fseek(_file, (long) offset, whence) == 0) return B_NO_ERROR;
      }
      return B_ERROR;
   }
   
   /** Returns our current position in the file.
    *  @note this subclass only supports 32-bit offsets.
    */
   virtual int64 GetPosition() const
   {
      return _file ? (int64) ftell(_file) : -1;
   }

   /** Flushes the file output by calling fflush() */
   virtual void FlushOutput() {if (_file) fflush(_file);}
   
   /** Calls fclose() on the held file descriptor (if any) and forgets it */
   virtual void Shutdown()
   {
      if (_file)
      {
         fclose(_file);
         ReleaseFile();
      }
   }
 
   /**
    * Releases control of the contained FILE object to the calling code.
    * After this method returns, this object no longer owns or can
    * use or close the file descriptor descriptor it once held.
    */
   void ReleaseFile() 
   {
      _file = NULL;
      SetSocketsFromFile(NULL);
   }

   /**
    * Returns the FILE object held by this object, or NULL if there is none.
    */
   FILE * GetFile() const {return _file;}

   /**
    * Sets our file pointer to the specified handle, closing any previously held file handle first.
    * @param fp The new file handle.  If non-NULL, this FileDataIO becomes the owner of (fp).
    */
   void SetFile(FILE * fp) {Shutdown(); _file = fp; SetSocketsFromFile(_file);}

   /**
    * This method should return a ConstSocketRef object containing a file descriptor
    * that can be passed to the readSet argument of select(), so that select() can
    * return when there is data available to be read from this DataIO (via Read()).
    *
    * Note that under Windows, this method will always return a NULL socket, because
    * under Windows it is not possible to select() on the file descriptor associated
    * with fileno(my_FILE_pointer).
    *
    * Note that the only thing you are allowed to do with the returned ConstSocketRef
    * is pass it to a SocketMultiplexer to block on (or pass the underlying file descriptor
    * to select()/etc's readSet).  For all other operations, use the appropriate
    * methods in the DataIO interface instead.  If you attempt to do any other I/O operations
    * on Socket or its file descriptor directly, the results are undefined.
    */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return _selectSocketRef;}

   /**
    * This method should return a ConstSocketRef object containing a file descriptor
    * that can be passed to the writeSet argument of select(), so that select() can
    * return when there is buffer space available to Write() to this DataIO.
    *
    * Note that under Windows, this method will always return a NULL socket, because
    * under Windows it is not possible to select() on the file descriptor associated
    * with fileno(my_FILE_pointer).
    *
    * Note that the only thing you are allowed to do with the returned ConstSocketRef
    * is pass it to a SocketMultiplexer to block on (or pass the underlying file descriptor
    * to select()/etc's writeSet).  For all other operations, use the appropriate
    * methods in the DataIO interface instead.  If you attempt to do any other I/O operations
    * on Socket or its file descriptor directly, the results are undefined.
    */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _selectSocketRef;}

private:
   void SetSocketsFromFile(FILE * optFile)
   {
      _selectSocketRef.Reset();
#ifndef SELECT_ON_FILE_DESCRIPTORS_NOT_AVAILABLE   // windows can't do the select-on-file-descriptor trick, sorry!
      _selectSocket.Clear(); 

      int fd = optFile ? fileno(optFile) : -1;
      if (fd >= 0)
      {
         _selectSocket.SetFileDescriptor(fd, false);  // false because the fclose() will call close(fd), so we should not
         _selectSocketRef.SetRef(&_selectSocket, false);
      } 
#else
      (void) optFile; // avoid compiler warning
#endif
   }

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
