/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFilePathInfo_h
#define MuscleFilePathInfo_h

#include "support/BitChord.h"
#include "util/TimeUtilityFunctions.h"

namespace muscle {

/** A cross-platform API for examining the attributes of a particular file. */
class MUSCLE_NODISCARD FilePathInfo MUSCLE_FINAL_CLASS
{
public:
   /** Default constructor:  creates an invalid FilePathInfo object.  */
   FilePathInfo() {Reset();}

   /** Constructor
     * @param optFilePath The path to the directory to open.  This is the same as calling SetFilePath(optFilePath).
     */
   FilePathInfo(const char * optFilePath) {SetFilePath(optFilePath);}

   /** Manual constructor, for creating a fake FilePathInfo with the specified attributes
     * @param exists true iff this FilePathInfo should indicate that the referenced file path exists.
     * @param isRegularFile true iff this FilePathInfo should indicate that the referenced file path is a regular-file.
     * @param isDir true iff this FilePathInfo should indicate that the referenced file path is a directory.
     * @param isSymlink true iff this FilePathInfo should indicate that the referenced file path is a symlink.
     * @param fileSizeBytes Size of the file, in bytes.
     * @param aTime Access time, in microseconds-since-1970.
     * @param cTime Creation time, in microseconds-since-1970.
     * @param mTime Modification time, in microseconds-since-1970.
     * @param hardLinkCount The number of directories that this file-path appears under
     */
   FilePathInfo(bool exists, bool isRegularFile, bool isDir, bool isSymlink, uint64 fileSizeBytes, uint64 aTime, uint64 cTime, uint64 mTime, uint32 hardLinkCount);

   /** Destructor.  */
   ~FilePathInfo() {/* empty */}

   /** Returns true iff something exists at the specified path. */
   MUSCLE_NODISCARD bool Exists() const {return _flags.IsBitSet(FPI_FLAG_EXISTS);}

   /** Returns true iff the item at the specified path is a regular data file. */
   MUSCLE_NODISCARD bool IsRegularFile() const {return _flags.IsBitSet(FPI_FLAG_ISREGULARFILE);}

   /** Returns true iff the item at the specified path is a directory. */
   MUSCLE_NODISCARD bool IsDirectory()    const {return _flags.IsBitSet(FPI_FLAG_ISDIRECTORY);}

   /** Returns true iff the item at the specified path is a directory. */
   MUSCLE_NODISCARD bool IsSymLink()     const {return _flags.IsBitSet(FPI_FLAG_ISSYMLINK);}

   /** Returns the current size of the file */
   MUSCLE_NODISCARD uint64 GetFileSize() const {return _size;}

   /** Returns the most recent access time, in microseconds since 1970.  Note that
     * not all filesystems update this time, so it may not be correct.
     */
   MUSCLE_NODISCARD uint64 GetAccessTime() const {return _atime;}

   /** Returns the most recent modification time, in microseconds since 1970.  */
   MUSCLE_NODISCARD uint64 GetModificationTime() const {return _mtime;}

   /** Returns the most creation time, in microseconds since 1970.  */
   MUSCLE_NODISCARD uint64 GetCreationTime() const {return _ctime;}

   /** Returns the number of directories that this file-path appears under.  (Typically 1, but
     * can be more if hard-links are in use and pointing to the same file from various locations)
     * AFAIK the only time this method should ever return 0 is if the FilePathInfo object isn't valid.
     */
   MUSCLE_NODISCARD uint32 GetHardLinkCount() const {return _hardLinkCount;}

   /** Sets this object's state to reflect the item that exists at (optFilePath)
     * @param optFilePath The path to examine.  SetFilePath(NULL) is the same as calling Reset().
     */
   void SetFilePath(const char * optFilePath);

   /** Resets this object to its default/invalid state */
   void Reset();

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator == (const FilePathInfo & rhs) const
   {
      return (_flags == rhs._flags)
          && (_size  == rhs._size)
          && (_atime == rhs._atime)
          && (_ctime == rhs._ctime)
          && (_mtime == rhs._mtime)
          && (_hardLinkCount == rhs._hardLinkCount);
   }

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator != (const FilePathInfo & rhs) const {return !(*this==rhs);}

   /** Returns a hash code for this object */
   MUSCLE_NODISCARD uint32 HashCode() const
   {
      return _flags.HashCode() + CalculateHashCode(_size) + (3*CalculateHashCode(_atime)) + (7*CalculateHashCode(_ctime)) + (11*CalculateHashCode(_mtime)) + (13*CalculateHashCode(_hardLinkCount));
   }

private:
#ifdef WIN32
   MUSCLE_NODISCARD uint64 InternalizeFileTime(const FILETIME & ft) const;
#else
   MUSCLE_NODISCARD uint64 InternalizeTimeSpec(const struct timespec & ts) const {return ((((uint64)ts.tv_sec)*MICROS_PER_SECOND)+(((uint64)ts.tv_nsec)/1000));}
   MUSCLE_NODISCARD uint64 InternalizeTimeT(time_t t) const {return (t==((time_t)-1)) ? 0 : (((uint64)t)*MICROS_PER_SECOND);}
#endif

   enum {
      FPI_FLAG_EXISTS = 0,
      FPI_FLAG_ISREGULARFILE,
      FPI_FLAG_ISDIRECTORY,
      FPI_FLAG_ISSYMLINK,
      NUM_FPI_FLAGS
   };
   static const char * _fpiFlagLabels[];
   DECLARE_LABELLED_BITCHORD_FLAGS_TYPE(FPIFlags, NUM_FPI_FLAGS, _fpiFlagLabels);

   FPIFlags _flags;
   uint64 _size;   // file size
   uint64 _atime;  // access time
   uint64 _ctime;  // creation time
   uint64 _mtime;  // modification time
   uint32 _hardLinkCount;
};

} // end namespace muscle

#endif
