/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleGlobalMemoryAllocator_h
#define MuscleGlobalMemoryAllocator_h

#include "util/MemoryAllocator.h"

namespace muscle {

// You can't use these functions unless memory tracking is enabled!
// So if you are getting errors, make sure -DMUSCLE_ENABLE_MEMORY_TRACKING
// is specified in your Makefile.
#ifdef MUSCLE_ENABLE_MEMORY_TRACKING

/** Set the MemoryAllocator object that is to be called by the C++ global new and delete operators.
  * @note this function is only available is -DMUSCLE_ENABLE_MEMORY_TRACKING is defined in the Makefile.
  * @param maRef Reference to The new MemoryAllocator object to use.  May be a NULL reference
  *              if you just want to remove any current MemoryAllocator.
  */
void SetCPlusPlusGlobalMemoryAllocator(const MemoryAllocatorRef & maRef);

/** Returns a reference to the current MemoryAllocator object that is being used by the
  * C++ global new and delete operators.  Will return a NULL reference if no MemoryAllocator is in use.
  * @note this function is only available is -DMUSCLE_ENABLE_MEMORY_TRACKING is defined in the Makefile.
  */
const MemoryAllocatorRef & GetCPlusPlusGlobalMemoryAllocator();

/** Returns the number of bytes currently dynamically allocated by this process.
  * @note this function is only available is -DMUSCLE_ENABLE_MEMORY_TRACKING is defined in the Makefile.
  */
size_t GetNumAllocatedBytes();

/** MUSCLE version of the C malloc() call.  Unlike the C malloc() call, however
 *  this function will use the global MemoryAllocator object when allocating memory.
 *  The only time you should need to call this directly is from C code where
 *  you want to use the global memory allocators but don't want to replace all
 *  the calls to malloc() and free() with new and delete.  For C++ programs, you
 *  can just use newnothrow and delete as usual and ignore this function.
 *  @param numBytes Number of bytes to attempt to allocate
 *  @param retryOnFailure This argument governs muscleAlloc's behaviour when
 *                        an out-of-memory condition occurs.  If true, muscleAlloc()
 *                        will attempt the allocation a second time after calling
 *                        AllocationFailed() on the global memory allocator, in the hope
 *                        that AllocationFailed() was able to free enough memory
 *                        to allow the allocation to succeed.  If set false, AllocationFailed()
 *                        will still be called after an out-of-memory error, but muscleAlloc()
 *                        will return NULL.
 *  @return Pointer to an allocated memory buffer on success, or NULL on failure.
 */
void * muscleAlloc(size_t numBytes, bool retryOnFailure = true);

/** Companion to muscleAlloc().  Any buffers allocated with muscleAlloc() should
 *  be freed with muscleFree() when you are done with them, to avoid memory leaks.
 *  @param buf Buffer that was previously allocated with muscleAlloc(), muscleStrdup(), or muscleRealloc().
 *             If NULL, then this call will be a no-op.
 */
void muscleFree(void * buf);

/** MUSCLE version of the C realloc() call.  Works just like the C realloc(),
 *  except that it calls the proper callbacks on the global MemoryAllocator
 *  object, as appropriate.
 *  @param ptr Pointer to the buffer to resize, or a NULL pointer if you wish
 *             to allocate a new buffer.
 *  @param s Desired new size for the buffer, or 0 if you wish to free the buffer.
 *  @param retryOnFailure See muscleAlloc() for a description of this argument.
 *  @returns Pointer to the resized array on success, or NULL on failure or
 *           if (s) was zero.  Note that the returned pointer may be the
 *           same as (ptr).
 */
void * muscleRealloc(void * ptr, size_t s, bool retryOnFailure = true);

/** MUSCLE version of the C strdup() call.  Works just like the C strdup(),
 *  except that it calls the proper callbacks on the global MemoryAllocator
 *  object, as appropriate.
 *  @param s the string to replicate and return a pointer to the copy of.
 *  @param retryOnFailure See muscleAlloc() for a description of this argument.
 *  @returns Pointer to the duplicate string on success, or NULL on failure.
 */
char * muscleStrdup(const char * s, bool retryOnFailure = true);

/** Given a pointer that was allocated with muscleAlloc(), muscleRealloc(), or the new operator,
  * returns B_NO_ERROR if the memory paranoia guard values are correct, or B_LOGIC_ERROR if they are
  * not.  Note that this function is a no-op unless MUSCLE_ENABLE_MEMORY_PARANOIA is defined
  * as a positive integer.
  * @param p the pointer to check for validity/memory corruption.  If NULL, this function will return B_NO_ERROR.
  * @param crashIfInvalid If true, this function will crash the app if corruption is detected.  Defaults to true.
  * @returns B_NO_ERROR if the buffer is valid, B_LOGIC_ERROR if it isn't (or we won't return if we crash the app!)
  *                     If MUSCLE_ENABLE_MEMORY_PARANOIA isn't defined, then this function always returns B_NO_ERROR.
  */
status_t MemoryParanoiaCheckBuffer(void * p, bool crashIfInvalid = true);

#else

/** Dummy/pass-through implementation of muscleAlloc().  Simply calls through to malloc().
  * @param numBytes the number of byes to allocate
  * @param retryOnFailure this parameter is ignored in this implementation
  * @returns a pointer to the returned buffer of memory, or NULL on failure.
  */
static inline void * muscleAlloc(size_t numBytes, bool retryOnFailure = true) {(void) retryOnFailure; return malloc(numBytes);}

/** Dummy/pass-through implementation of muscleFree().  Simply calls through to free().
  * @param buf the buffer to free() (as was previously returned by a call to muscleAlloc() or muscleRealloc().
  * @note calling muscleFree(NULL) is safe to do, it will be treated as a no-op.
  */
static inline void muscleFree(void * buf) {if (buf) free(buf);}

/** Dummy/pass-through implementation of muscleRealloc().  Simply calls through to realloc().
  * @param ptr pointer argument (see realloc()'s man page for details)
  * @param s size argument (see realloc()'s man page for details)
  * @param retryOnFailure this parameter is ignored in this implementation
  * @returns the return value from realloc()
  */
static inline void * muscleRealloc(void * ptr, size_t s, bool retryOnFailure = true) {(void) retryOnFailure; return realloc(ptr, s);}

/** Dummy/pass-through implementation of muscleStrdup().  Simply calls through to strdup().
 *  @param s the string to replicate and return a pointer to the copy of.
 *  @param retryOnFailure this parameter is ignored in this implementation
 *  @returns Pointer to the duplicate string on success, or NULL on failure.
 */
static inline char * muscleStrdup(const char * s, bool retryOnFailure = true)
{
   (void) retryOnFailure;
#ifdef WIN32
   return _strdup(s);
#else
   return strdup(s);
#endif
}

#endif

} // end namespace muscle

#endif
