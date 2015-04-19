/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ZipFileUtilityFunctions_h
#define ZipFileUtilityFunctions_h

#include "dataio/DataIO.h"
#include "message/Message.h"

namespace muscle {

/** @defgroup zipfileutilityfunctions The ZipFileUtilityFunctions function API
 *  These functions are all defined in ZipFileUtilityFunctions(.cpp,.h), and are stand-alone
 *  functions that conveniently convert .zip files into Message objects, and vice versa.
 *  @{
 */

/** Given a Message object, outputs a .zip file containing the B_RAW_TYPE data in that Message.
  * Each B_RAW_TYPE field in the Message object will be written to the .zip file as a contained file.
  * Message fields will be written recursively to the .zip file as sub-directories.
  * @param writeTo DataIO object representing the file (or whatever) that we are outputting data to.
  * @param msg Reference to the Message to write to (writeTo).
  * @param compressionLevel A number between 0 (no compression) and 9 (maximum compression).  Default value is 9.
  * @param fileCreationTime the file creation time (in microseconds since 1970) to assign to all of the
  *                         file-records in the .zip file.  If left as MUSCLE_TIME_NEVER (the default) then
  *                         the current time will be used.
  * @note This function is useful only when you need to be compatible with the .ZIP file format.
  *       In particular, it does NOT save all the information in the Message -- only the contents
  *       of the B_RAW_TYPE fields (as "files") and B_MESSAGE_TYPE fields as ("folders").
  *       If you need to store Message objects to disk and read them back verbatim later, you should
  *       use Message::Flatten() and Message::Unflatten() to do so, not this function.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t WriteZipFile(DataIO & writeTo, const Message & msg, int compressionLevel = 9, uint64 fileCreationTime = MUSCLE_TIME_NEVER);

/** Convenience method:  as above, but takes a file name instead of a DataIO object.
  * @param fileName File path of the place to write the .zip file to
  * @param msg Reference to the Message to write to (fileName).
  * @param compressionLevel A number between 0 (no compression) and 9 (maximum compression).  Default value is 9.
  * @param fileCreationTime the file creation time (in microseconds since 1970) to assign to all of the
  *                         file-records in the .zip file.  If left as MUSCLE_TIME_NEVER (the default) then
  *                         the current time will be used.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t WriteZipFile(const char * fileName, const Message & msg, int compressionLevel = 9, uint64 fileCreationTime = MUSCLE_TIME_NEVER);

/** Given a DataIO object to read from (often a FileDataIO for a .zip file on disk),
  * reads the file and creates and returns an equivalent Message object.  Each contained
  * file in the .zip file will appear in the Message object as a B_RAW_DATA field
  * (or a B_INT64_TYPE field, if you specified (loadData) to be false), and each directory
  * in the .zip file will appear in the Message object as a Message field.
  * @param readFrom DataIO to read the .zip file data from.
  * @param loadData If true, all the zip file's data will be decompressed and added
  *                 to the return Message.  If false, only the filenames and lengths
  *                 will be read (each file will show up as a B_INT64_TYPE field
  *                 with the int64 value being the file's length); the actual file
  *                 data will not be decompressed or placed into memory.  Defaults
  *                 to true.  (You might set this argument to false if you just wanted
  *                 to check that the expected files were present in the .zip, but
  *                 not actually load them)
  * @returns a valid MessageRef on success, or a NULL MessageRef on failure.
  */
MessageRef ReadZipFile(DataIO & readFrom, bool loadData = true);

/** Convenience method:  as above, but takes a file name instead of a DataIO object.
  * @param fileName File name of a .zip file to read
  * @param loadData Whether or not to read data or just directory structure (see previous
  *                 overload of ReadZipFile() for a full explanation)
  * @returns a valid MessageRef on success, or a NULL MessageRef on failure.
  */
MessageRef ReadZipFile(const char * fileName, bool loadData = true);

/** @} */ // end of zipfileutilityfunctions doxygen group

}; // end namespace muscle

#endif
