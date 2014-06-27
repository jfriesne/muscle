/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSharedMemory_h
#define MuscleSharedMemory_h

#include "util/String.h"
#include "util/CountedObject.h"

// This needs to be AFTER the MUSCLE includes, so that WIN32 will be defined if appropriate
#ifndef WIN32
# include <sys/ipc.h>
#endif

namespace muscle {

/** This class is a simple platform-independent API wrapper around the native OS's shared-memory facilities.
  * It can be used to create and access shared memory areas from different processes.
  * The current implementation only works under Windows and POSIX, but other implementations may be
  * added in the future.
  */
class SharedMemory : private CountedObject<SharedMemory>
{
public:
   /** Default constructor.  You'll need to call SetArea() before this object will be useful. */
   SharedMemory();

   /** Destructor.  Calls UnsetArea(). */
   ~SharedMemory();

   /** Finds or demand-allocates a shared memory area with at least the specified size.
    *  Calls UnsetArea() first to make sure any previously attached area gets detached.
    *  Note that created areas will remain until DeleteArea() is called (if it isn't called,
    *  they will persist even after the process has terminated!)
    *  @see IsCreatedLocally() to determine if the area got created by this call or not.
    *  @param areaName Name of the shared memory area to open or create.  
    *                  If NULL, then a new, unnamed area will be created.
    *  @param createSize If a new area needs to be created, this is the minimum size of the
    *                    created area, in bytes.  If zero, auto-creation of a shared area is disabled.
    *  @param returnLocked If set true, then the area will be read/write locked when this function
    *                      returns successfully, and it is the caller's responsibility to call
    *                      UnlockArea() when appropriate.
    *  @return B_NO_ERROR on success, or B_ERROR on failure.
    */
   status_t SetArea(const char * areaName, uint32 createSize = 0, bool returnLocked = false);

   /** Unlocks any current locks, and terminates our relationship with the current shared memory area.
    *  @returns B_NO_ERROR on success, or B_ERROR if we had no current area. 
    */
   void UnsetArea();

   /** Locks the current area, using a shared locking mode.
    *  Reading the contents of the shared memory area is safe while in this mode,
    *  but modifying them is not.  If you need to modify the shared memory area,
    *  use LockAreaReadWrite() instead.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (no area, or already locked)
    *  @note under Windows this call is currently equivalent to the LockAreaReadWrite()
    *        call, because as far as I can tell there is no good way to implement
    *        shared-reader/single-writer interprocess locks under Windows.  If you
    *        know of a way to do it, please tell me how so I can improve the code. :^)
    */
   status_t LockAreaReadOnly() {return LockArea(true);}

   /** Locks the current area using an exclusive locking mode.  It is guaranteed
    *  that no more than one process will have an exclusive lock on a given area 
    *  at any given time.
    *  It is safe to read or modify the shared memory area while this lock is active,
    *  but if you will not need to modify anything, then it is more efficient to
    *  lock using LockAreaReadOnly() instead.
    *  @returns B_NO_ERROR on success, or B_ERROR on failure (no area, or already locked)
    */
   status_t LockAreaReadWrite() {return LockArea(false);}

   /** Unlocks any current lock on our shared memory area.
    *  Has no effect if our area is already unlocked.
    */
   void UnlockArea();

   /** Returns the size of our current area in bytes, or zero if there is no current area */
   uint32 GetAreaSize() const {return _areaSize;}

   /** Returns true iff we are currently locked */
   bool IsLocked() const {return _isLocked;}

   /** Returns true iff we created our memory area ourselves, or false if we found
    *  it already created.  The returned value is meaningless if there is no current area.
    */ 
   bool IsCreatedLocally() const {return _isCreatedLocally;}

   /** Returns the name of our current area (as passed in to SetArea() or
    *  generated within SetArea(), or "" if we have no current area. 
    */
   const String & GetAreaName() const {return _areaName;}

   /** Returns a pointer to shared memory area.  Note that this memory
    *  may be accessed, written to, or even deleted by other processes!  So you'll typically
    *  want to call one of the LockArea*() methods before accessing the memory it points to.
    *  @returns Pointer to the shared memory area, or NULL if there is no current area.
    */
   uint8 * GetAreaPointer() const {return (uint8 *) _area;}

   /** Convenience synonym for GetAreaPointer(). */
   uint8 * operator()() const {return (uint8 *) _area;}

   /** Rudely deletes the current shared area.
    *  After this call returns, no processes will be able to SetArea() or LockArea()
    *  our shared memory any more.  Note that this method will call LockAreaReadWrite()
    *  before deleting everything, so other processes who currently have the area
    *  locked will at least be able to finish their current transaction before everything
    *  goes away.
    *  @returns B_NO_ERROR on success, or B_ERROR if there is no current area or
    *           some other problem prevented the current area from being deleted.
    */
   status_t DeleteArea();

private:
   status_t LockArea(bool readOnly);

#ifdef WIN32
   ::HANDLE _mutex;
   ::HANDLE _file;
   ::HANDLE _map;
   String _fileName;
#else
   status_t AdjustSemaphore(short delta);
   key_t _key;
   int _areaID;
   int _semID;
#endif

   String _areaName;
   void * _area;
   uint32 _areaSize;
   bool _isLocked;
   bool _isLockedReadOnly;
   bool _isCreatedLocally;
};

}; // end namespace muscle

#endif
