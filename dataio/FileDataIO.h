/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFileDataIO_h
#define MuscleFileDataIO_h

#include "dataio/DataIO.h"

namespace muscle {

/**
 *  Data I/O to and from a stdio FILE. 
 */
class FileDataIO : public DataIO, private CountedObject<FileDataIO>
{
public:
   /** Constructor.
    *  @param file File to read from or write to.  Becomes property of this FileDataIO object,
    *         and will be fclose()'d when this object is deleted.  Defaults to NULL.
    */
   FileDataIO(FILE * file = NULL) : _file(file) {/* empty */}

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
         _file = NULL;
      }
   }
 
   /**
    * Releases control of the contained FILE object to the calling code.
    * After this method returns, this object no longer owns or can
    * use or close the file descriptor descriptor it once held.
    */
   void ReleaseFile() {_file = NULL;}

   /**
    * Returns the FILE object held by this object, or NULL if there is none.
    */
   FILE * GetFile() const {return _file;}

   /**
    * Sets our file pointer to the specified handle, closing any previously held file handle first.
    * @param fp The new file handle.  If non-NULL, this FileDataIO becomes the owner of (fp).
    */
   void SetFile(FILE * fp) {Shutdown(); _file = fp;}

   /** Returns a NULL reference;  (can't select on this one, sorry) */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetNullSocket();}

   /** Returns a NULL reference;  (can't select on this one, sorry) */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

private:
   FILE * _file;
};

}; // end namespace muscle

#endif
