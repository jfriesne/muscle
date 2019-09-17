/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSeekableDataIO_h
#define MuscleSeekableDataIO_h

#include "dataio/DataIO.h"

namespace muscle {
 
/** Abstract base class for DataIO objects that represent seekable data streams (e.g
  * for files, or objects that can act like files)
  */
class SeekableDataIO : public virtual DataIO
{
public:
   /** Default Constructor. */
   SeekableDataIO() {/* empty */}

   /** Destructor. */
   virtual ~SeekableDataIO() {/* empty */}

   /** Values to pass in to SeekableDataIO::Seek()'s second parameter */
   enum {
      IO_SEEK_SET = 0,  /**< Tells Seek that its value specifies bytes-after-beginning-of-stream */
      IO_SEEK_CUR,      /**< Tells Seek that its value specifies bytes-after-current-stream-position */
      IO_SEEK_END,      /**< Tells Seek that its value specifies bytes-after-end-of-stream (you'll usually specify a non-positive seek value with this) */
      NUM_IO_SEEKS      /**< A guard value */
   };

   /**
    * Seek to a given position in the I/O stream.  
    * @param offset Byte offset to seek to or by (depending on the next arg)
    * @param whence Set this to IO_SEEK_SET if you want the offset to
    *               be relative to the start of the stream; or to 
    *               IO_SEEK_CUR if it should be relative to the current
    *               stream position, or IO_SEEK_END if it should be
    *               relative to the end of the stream.
    * @return B_NO_ERROR on success, or an error code on failure.
    */
   virtual status_t Seek(int64 offset, int whence) = 0;

   /**
    * Should return the current position, in bytes, of the stream from 
    * its start position, or -1 if the current position is not known.
    */
   virtual int64 GetPosition() const = 0;

   /** Returns the total length of this DataIO's stream, in bytes.
     * The default implementation computes this value by Seek()'ing
     * to the end of the stream, recording the current seek position, and then
     * Seek()'ing back to the previous position in the stream.  Subclasses may
     * override this method to provide a more efficient mechanism, if there is one.
     * @returns The total length of this DataIO's stream, in bytes, or -1 on error.
     */
   virtual int64 GetLength();
};
DECLARE_REFTYPES(SeekableDataIO);

} // end namespace muscle

#endif
