/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDirectory_h
#define MuscleDirectory_h

#include "support/MuscleSupport.h"
#include "util/CountedObject.h"
#include "util/RefCount.h"

namespace muscle {

/** A cross-platform API for iterating over the contents of a specified filesystem directory. */
class Directory : public RefCountable, private CountedObject<Directory>
{
public:
   /** Default constructor:  creates an invalid Directory object.  */
   Directory() : _path(NULL), _dirPtr(NULL), _currentFileName(NULL) {/* empty */}

   /** Constructor
     * @param dirPath The path to the directory to open.  This is the same as calling SetDir(dirPath).
     */
   Directory(const char * dirPath) : _path(NULL), _dirPtr(NULL), _currentFileName(NULL) {(void) SetDir(dirPath);}

   /** Destructor.  Closes our held directory descriptor, if we have one. */
   ~Directory() {Reset();}

   /** Returns true iff we were able to open the specified directory. */
   bool IsValid() const {return (_dirPtr != NULL);}

   /** Returns a pointer to the current file name in the directory iteration, or NULL if there is no current file name.
     * Note that the returned string will not remain valid if this Directory object is changed.
     */
   const char * GetCurrentFileName() const {return _currentFileName;}

   /** Iterates to the next file name in the directory. */
   void operator++(int);

   /** Rewinds the iteration back to the top of the directory. */
   void Rewind();

   /** Closes this Directory object's directory and resets it to the invalid state. */
   void Reset();

   /** Closes any existing held directory and replaces it with the one indicated by (dirPath).
     * @param dirPath Path to the new directory to open.  SetDir(NULL) is the same as calling Reset().
     * @returns B_NO_ERROR if the new directory was successfully opened, B_ERROR if it was not (directory not found?).
     */
   status_t SetDir(const char * dirPath);

   /** This static method will create a directory with the specified path.
     * @param dirPath the directory's name (include path if desired) to create.
     * @param forceCreateParentDirsIfNecessary If true, we'll create directories above the new directory also if necessary.
     *                                         Otherwise we'll fail if the new directory's parent director doesn't exist.
     * @param errorIfAlreadyExists If true, and the requested folder already exists, then this method will return B_ERROR.
     *                             If false (the default), then this method will return B_NO_ERROR if the directory already exists.
     * @note This method was originally called CreateDirectory() but that was causing namespace collisions with
     *       some defines in the Microsoft Windows system headers, so I've renamed it to MakeDirectory() to avoid that problem.
     * @return B_NO_ERROR on success, or B_ERROR on failure (directory already exists, or permission denied).
     */
   static status_t MakeDirectory(const char * dirPath, bool forceCreateParentDirsIfNecessary, bool errorIfAlreadyExists=false);

   /** This static method will delete a directory with the specified path.
     * @param dirPath the directory's name (include path if desired) to delete.
     * @param forceDeleteSubItemsIfNecessary If true, we'll recursively delete all the items in the directory as well.
     *                                         Otherwise we'll fail if the directory to be deleted isn't empty.
     * @return B_NO_ERROR on success, or B_ERROR on failure (directory wasn't empty, or permission denied).
     */
   static status_t DeleteDirectory(const char * dirPath, bool forceDeleteSubItemsIfNecessary);

   /** Convenience method.  Given a path to a file, this method will create any missing directories
     * along that path, so that the file can be created.
     * @param filePath a path to a file, including the filename itself (the filename part will be ignored)
     * @returns B_NO_ERROR on success (i.e. directory was created, or already exists), or B_ERROR on failure (out of memory or permission denied?)
     */
   static status_t MakeDirectoryForFile(const char * filePath);

   /** Returns true iff the specified directory exists. 
     * @param dirPath Path to check to see if it refers to an existing directory.
     * @returns true iff (dirPath) does in fact refer to an existing directory.
     */
   static bool Exists(const char * dirPath);

   /** Returns the path string that was passed in to this Directory object, or NULL if
     * there is no current directory active.  Note that the Directory object makes an
     * internal copy of the passed in string, so this pointer will be valid even if
     * the string passed in to the constructor (or SetDir()) isn't anymore.
     * @note this string, if non-NULL, will always have a file-path-separator character at the end.
     */
   const char * GetPath() const {return _path;}

private:
   Directory(const Directory & rhs);  // deliberately private and unimplemented

   char * _path;
   void * _dirPtr;
   const char * _currentFileName;
};
DECLARE_REFTYPES(Directory);

}; // end namespace muscle

#endif
