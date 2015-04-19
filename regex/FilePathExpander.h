/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFilePathExpander_h
#define MuscleFilePathExpander_h

#include "util/Queue.h"
#include "util/String.h"

namespace muscle {

/** Given a file path (e.g. "*.wav" or "/tmp/myfiles/foo_*.txt"), traverses the local filesystem
  * and adds to (outputPaths) the expanded path of any matching files or folders that were discovered.
  * @param path A potentially wildcarded file path (absolute or relative)
  * @param outputPaths On successful return, this will contain all matching files and folders discovered.
  * @param isSimpleFormat if true, a simple globbing syntax is expected in (expression).
  *                       Otherwise, the full regex syntax will be expected.  Defaults to true.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t ExpandFilePathWildCards(const String & path, Queue<String> & outputPaths, bool isSimpleFormat = true);

}; // end namespace muscle


#endif
