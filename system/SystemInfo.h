/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef MuscleSystemInfos_h
#define MuscleSystemInfos_h

#include "util/String.h"
#include "util/Queue.h"

namespace muscle {

/** @defgroup systeminfo The SystemInfo function API
 *  These functions are all defined in SystemInfo(.cpp,.h), and are stand-alone
 *  functions that provide information about the computer and OS that the program is executing on.
 *  @{
 */

/** Returns a human-readable name for the operating system that the code has 
  * been compiled on.  For example, "Windows", "MacOS/X", or "Linux".  If the 
  * operating system's name is unknown, returns "Unknown".
  * @param defaultString What to return if we don't know what the host OS is.  Defaults to "Unknown".
  */
const char * GetOSName(const char * defaultString = "Unknown");

/** An enumeration of filesystem locations that can be passed to GetSystemPath() */
enum {
   SYSTEM_PATH_CURRENT = 0, /**< our current working directory */
   SYSTEM_PATH_EXECUTABLE,  /**< directory where our process's executable binary is  */
   SYSTEM_PATH_TEMPFILES,   /**< scratch directory where temp files may be stored */
   SYSTEM_PATH_USERHOME,    /**< the current user's home folder */
   SYSTEM_PATH_DESKTOP,     /**< the current user's desktop folder */
   SYSTEM_PATH_DOCUMENTS,   /**< the current user's documents folder */
   SYSTEM_PATH_ROOT,        /**< the root directory */
   NUM_SYSTEM_PATHS         /**< guard value */
};

/** Given a SYSTEM_PATH_* token, returns the system's directory path
  * for that directory.  This is an easy cross-platform way to determine
  * where various various directories of interest are.
  * @param whichPath a SYSTEM_PATH_* token.
  * @param outStr on success, this string will contain the appopriate
  *               path name,  The path is guaranteed to end with a file
  *               separator character (i.e. "/" or "\\", as appropriate).
  * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the requested path could
  *          not be determined. 
  */
status_t GetSystemPath(uint32 whichPath, String & outStr);

/** Queries the number of CPU processing cores available on this computer.
  * @param retNumProcessors On success, the number of cores is placed here
  * @returns B_NO_ERROR on success, or B_UNIMPLEMENTED, or some other error code.
  */
status_t GetNumberOfProcessors(uint32 & retNumProcessors);

/** Returns the file-path-separator character to use for this operating
  * system:  i.e. backslash for Windows, and forward-slash for every
  * other operating system.
  */
inline const char * GetFilePathSeparator()
{
#ifdef WIN32
   return "\\";
#else
   return "/";
#endif
}

/** Convenience method for debugging.  Returns a list of human-readable strings
  * of the various MUSCLE-specific build flags (as documented in BUILDOPTIONS.txt)
  * that the MUSCLE codebase was compiled.
  */
Queue<String> GetBuildFlags();

/** Convenience method for debugging.  Dumps a human-readable record of the 
  * various MUSCLE-specific build flags (as documented in BUILDOPTIONS.txt)
  * that the MUSCLE codebase was compiled with to the log, at the specified log level.
  * @param logLevel Optional MUSCLE_LOG_* value to log at.  Defaults to MUSCLE_LOG_INFO.
  */
void LogBuildFlags(int logLevel = MUSCLE_LOG_INFO);

/** Same as LogBuildFlags() except the text is printed directly to stdout
  * rather than going through the LogTime() logging function.
  */
void PrintBuildFlags();

/** @} */ // end of systeminfo doxygen group

} // end namespace muscle

#endif
