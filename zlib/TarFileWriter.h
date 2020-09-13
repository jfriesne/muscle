/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef TarFileUtilityFunctions_h
#define TarFileUtilityFunctions_h

#include "support/NotCopyable.h"
#include "dataio/SeekableDataIO.h"

namespace muscle {

/** These TarFileWriter class writes .tar files in a cross-platform-compatible manner.
 *  (I use this code instead of libtar because libtar doesn't work under Windows,
 *  and it doesn't support streaming data into a tar file easily).  Currently
 *  only the writing of a .tar file is supported; at some point I may add
 *  support for reading .tar files as well.
 */
class TarFileWriter : public NotCopyable
{
public:
   /** Default constructor. */
   TarFileWriter();

   /** Constructor.
     * @param outputFileName Name/path of the .tar file to write to
     * @param append If true, written data will be appended to this file; otherwise
     *               if the file already exists it will be deleted and replaced.
     * This constructor is equivalent to the default constructor, plus calling SetFile().
     */ 
   TarFileWriter(const char * outputFileName, bool append);

   /** Constructor.
     * @param dio Reference to a DataIO object to use to write out the .tar file data.
     * This constructor is equivalent to the default constructor, plus calling SetFile().
     */ 
   TarFileWriter(const DataIORef & dio);

   /** Destructor. */
   ~TarFileWriter();
   
   /** Writes any pending updates to the .tar file (if necessary), then closes the file (if one is open) 
    *  and returns this object to its basic just-default-constructed state.
     * @returns B_NO_ERROR on success, or an error code if there was an error writing out pending header-data changes.
     *          Note that this method will always unreference any held DataIO and reset our state to default, even if it returns an error-code.
     */
   status_t Close();

   /** Unreferences the currently held DataIO (if any) and attempts to open the new .tar file for writing.
     * @param outputFileName Name/path of the .tar file to write to
     * @param append If true, new written data will be appended to the existing file; otherwise
     *               if the file already exists it will be deleted and replaced.
     */ 
   status_t SetFile(const char * outputFileName, bool append);

   /** Unreferences the currently held DataIO (if any) and uses the specified DataIO instead.
     * @param dioRef Reference to The DataIORef to use to output .tar data to, or a NULL Ref to close only.
     * @note if (dioRef) isn't referencing a SeekableDataIORef, then you are required to pass a valid/correct file-size
     *       arguments to WriteFileHeader().  If (dioRef) is referencing a SeekableDataIORef, then the file-size argument
     *       can optionally passed in as 0, and the TarFileWriter will update the header-fields automatically based on how 
     *       much file-data was actually written.
     */ 
   void SetFile(const DataIORef & dioRef);

   /** Writes a .tar header fle-block with the given information.
     * @param fileName The name of the member file as it should be recorded inside the .tar file.
     * @param fileMode The file-mode bits that should be stored with this file.
     * @param ownerID The file's owner's numeric user ID
     * @param groupID The file's group's numeric user ID
     * @param modificationTime A timestamp indicating the file's last modification time (microseconds since 1970)
     * @param linkIndicator one of the TAR_LINK_INDICATOR_* values
     * @param linkedFileName Name of the linked file (if any);
     * @param fileSize if you know the size of the file in advance, you can pass in the file-size (in bytes) in this argument.
     *                 Note that if the DataIORef object the TarFileWriter isn't a SeekableDataIORef, then passing in the
     *                 correct file size here is mandatory, as the TarFileWriter will be unable to seek back and patch up
     *                 the file-size header field later.  OTOH if you are using a SeekableDataIORef and don't know the file-size
     *                 up-front, you can just pass in zero here.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteFileHeader(const char * fileName, uint32 fileMode, uint32 ownerID, uint32 groupID, uint64 modificationTime, int linkIndicator, const char * linkedFileName, uint64 fileSize);

   /** Writes (numBytes) of data into the currently active file-block.
     * Three must be a file-header currently active for this call to succeed.
     * @param fileData Pointer to some data bytes to write into the tar file.  
     * @param numBytes How many bytes (fileData) points to.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t WriteFileData(const uint8 * fileData, uint32 numBytes);

   /** Updates the current file-header-block and resets our state to receive the next one.
     * @returns B_NO_ERROR on success (or if no file-header-block was open), or an error code if there was an error updating the file-header-block.
     * @note that Close() and WriteFileHeader() will call FinishCurrentFileDataBlock() implicitly when necessary, so 
     *       calling this method isn't strictly necessary.
     */
   status_t FinishCurrentFileDataBlock();

   /** These values may be passed to the (linkIndicator) argument of WriteFileHeader() to indicate what kind of filesystem-object the header is describing. */
   enum {
      TAR_LINK_INDICATOR_NORMAL_FILE = 0,    /**< A normal data-file */
      TAR_LINK_INDICATOR_HARD_LINK,          /**< A hard-link */
      TAR_LINK_INDICATOR_SYMBOLIC_LINK,      /**< A symbolic link */
      TAR_LINK_INDICATOR_CHARACTER_SPECIAL,  /**< A file representing a character-based device */
      TAR_LINK_INDICATOR_BLOCK_SPECIAL,      /**< A file representing a block-based device */
      TAR_LINK_INDICATOR_DIRECTORY,          /**< A directory */
      TAR_LINK_INDICATOR_FIFO,               /**< A FIFO (named pipe) file*/
      TAR_LINK_INDICATOR_CONTIGUOUS_FILE,    /**< A contiguous file */
      NUM_TAR_LINK_INDICATORS                /**< Guard value */
   };

   /** Returns true iff we successfully opened the .tar output file. */
   bool IsFileOpen() const {return (_writerIO() != NULL);}

   /** Returns true iff we successfully started a .tar record block and it is currently open. */
   bool IsFileDataBlockOpen() const {return (_currentHeaderOffset >= 0);}

private:
   enum {TAR_BLOCK_SIZE=512};

   void UpdateCurrentHeaderChecksum();

   DataIORef _writerIO;
   SeekableDataIORef _seekableWriterIO;  // pre-downcast reference, for convenience.  Will be NULL if our DataIO isn't a SeekableDataIO!
   int64 _currentHeaderOffset;
   uint8 _currentHeaderBytes[TAR_BLOCK_SIZE];
   uint64 _prestatedFileSize;    // as passed in to WriteFileHeader()
   uint64 _currentSeekPosition;  // gotta track this manually since (_seekableWriterIO()) may be NULL)
};

} // end namespace muscle

#endif
