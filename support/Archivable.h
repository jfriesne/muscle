/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleArchivable_h
#define MuscleArchivable_h

#include "support/MuscleSupport.h"

namespace muscle {

class Message;

/** This class is an interface representing any object whose state
 *  can be saved to a Message or restored from a Message using the
 *  standard SaveToArchive() and SetFromArchive() method-calls.
 */
class Archivable
{
public:
   /** Constructor */
   Archivable() {/* empty */}

   /** Destructor */
   virtual ~Archivable() {/* empty */}

   /** Must be implemented to save this object's current state into (archive).
     * @param archive the Message to save our state into
     * @returns B_NO_ERROR on success, or an error-code on failure.
     */
   virtual status_t SaveToArchive(Message & archive) const = 0;

   /** Must be implemented to set/restore this object's current state from (archive).
     * @param archive the Message to restore our state from
     * @returns B_NO_ERROR on success, or an error-code on failure.
     */
   virtual status_t SetFromArchive(const Message & archive) = 0;
};

} // end namespace muscle

#endif /* MuscleArchivable_h */

