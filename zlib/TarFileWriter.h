/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef TarFileUtilityFunctions_h
#define TarFileUtilityFunctions_h

#include "dataio/DataIO.h"

namespace muscle {

/** @defgroup tarfileutilityfunctions The TarFileUtilityFunctions function API
 *  These functions deal with .tar files in a cross-platform-compatible manner.
 *  (I use these instead of libtar, because libtar doesn't work under Windows,
 *  and it doesn't support streaming data into a tar file easily).  Currently
 *  only the writing of a .tar file is supported; at some point I may add
 *  support for reading .tar files as well.
 *  @{
 */

class TarFileWriter
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
     * @param dio Reference to a DataIO to use to write data out.
     * This constructor is equivalent to the default constructor, plus calling SetFile().
     */
   TarFileWriter(const DataIORef & dio);

   /** Destructor. */
   ~TarFileWriter();

   /** Closes the current file (if any is open) and returns to the just-default-constructed state.
     * @returns B_NO_ERROR on success, or B_ERROR if there was an error writing out header data.
     *          Note that the file will always be closed, even if we return B_ERROR.
     */
   status_t Close();

   /** Closes the currently open file (if any) and attempts to open the new one.
     * @param outputFileName Name/path of the .tar file to write to
     * @param append If true, new written data will be appended to the existing file; otherwise
     *               if the file already exists it will be deleted and replaced.
     */
   status_t SetFile(const char * outputFileName, bool append);

   /** Closes the currently open file (if any) and uses the specified DataIO instead.
     * @param dioRef Reference to The DataIORef to use to output .tar data to, or a NULL Ref to close only.
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
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t WriteFileHeader(const char * fileName, uint32 fileMode, uint32 ownerID, uint32 groupID, uint64 modificationTime, int linkIndicator, const char * linkedFileName);

   /** Writes (numBytes) of data into the current file block.
     * Three must be a file-header currently active for this call to succeed.
     * @param fileData Pointer to some data bytes to write into the tar file.
     * @param numBytes How many bytes (fileData) points to.
     * @returns B_NO_ERROR on success, or B_ERROR on failure.
     */
   status_t WriteFileData(const uint8 * fileData, uint32 numBytes);

   /** Updates the current file-header-block and resets our state to receive the next one.
     * Note that Close() and WriteFileHeader() will call FinishCurrentFileDataBlock() if necessary, so calling this method isn't strictly necessary.
     * @returns B_NO_ERROR on success (or if no header-block was open), or B_ERROR if there was an error updating the header block.
     */
   status_t FinishCurrentFileDataBlock();

   enum {
      TAR_LINK_INDICATOR_NORMAL_FILE = 0,
      TAR_LINK_INDICATOR_HARD_FILE,
      TAR_LINK_INDICATOR_SYMBOLIC_LINK,
      TAR_LINK_INDICATOR_CHARACTER_SPECIAL,
      TAR_LINK_INDICATOR_BLOCK_SPECIAL,
      TAR_LINK_INDICATOR_DIRECTORY,
      TAR_LINK_INDICATOR_FIFO,
      TAR_LINK_INDICATOR_CONTIGUOUS_FILE,
      NUM_TAR_LINK_INDICATORS
   };

   /** Returns true iff we successfully opened the .tar output file. */
   bool IsFileOpen() const {return (_writerIO() != NULL);}

   /** Returns true iff we successfully started a .tar record block and it is currently open. */
   bool IsFileDataBlockOpen() const {return (_currentHeaderOffset >= 0);}

private:
   enum {TAR_BLOCK_SIZE=512};

   int64 GetCurrentSeekPosition() const;

   DataIORef _writerIO;
   int64 _currentHeaderOffset;
   uint8 _currentHeaderBytes[TAR_BLOCK_SIZE];
};

/** @} */ // end of tarfileutilityfunctions doxygen group

}; // end namespace muscle

#endif
