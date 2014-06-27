/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef NEW_H_NOT_AVAILABLE
# include <new>
# include <typeinfo>
#endif

#include "system/GlobalMemoryAllocator.h"
#include "system/SetupSystem.h"  // for GetGlobalMuscleLock()

// Metrowerk's compiler crashes at run-time if this memory-tracking
// code is enabled.  I haven't been able to figure out why (everything
// looks okay, but after a few dozen malloc()/free() calls, free()
// crashes!) so I'm just going to disable this code for PowerPC/Metrowerks
// machines.  Sorry!   --jaf 12/01/00
#ifndef __osf__
# ifdef __MWERKS__
#  ifdef MUSCLE_ENABLE_MEMORY_TRACKING
#   undef MUSCLE_ENABLE_MEMORY_TRACKING
#   undef MUSCLE_ENABLE_MEMORY_PARANOIA
#  endif
# endif
#endif

#ifdef MUSCLE_ENABLE_MEMORY_TRACKING

// VC++ doesn't know from exceptions  :^P
# ifdef WIN32
#  ifndef MUSCLE_NO_EXCEPTIONS
#   define MUSCLE_NO_EXCEPTIONS
#  endif
# endif

// No exceptions?  Then make all of these keywords go away
# ifdef MUSCLE_NO_EXCEPTIONS
#  define BAD_ALLOC
#  define THROW
#  define LPAREN
#  define RPAREN
# else
#  define BAD_ALLOC bad_alloc
#  define THROW     throw
#  define LPAREN    (
#  define RPAREN    )
# endif

namespace muscle {

extern void SetFailedMemoryRequestSize(uint32 numBytes);  // FogBugz #7547

# if MUSCLE_ENABLE_MEMORY_PARANOIA > 0

// Functions for converting user-visible pointers (etc) to our internal implementation and back
#  define MEMORY_PARANOIA_ALLOCATED_GARBAGE_VALUE   (0x55)
#  define MEMORY_PARANOIA_DEALLOCATED_GARBAGE_VALUE (0x66)

static inline uint32    CONVERT_USER_TO_INTERNAL_SIZE(uint32 uNumBytes) {return (uNumBytes+(sizeof(size_t)+(2*MUSCLE_ENABLE_MEMORY_PARANOIA*sizeof(size_t *))));}
static inline uint32    CONVERT_INTERNAL_TO_USER_SIZE(uint32 iNumBytes) {return (iNumBytes-(sizeof(size_t)+(2*MUSCLE_ENABLE_MEMORY_PARANOIA*sizeof(size_t *))));}
static inline size_t *  CONVERT_USER_TO_INTERNAL_POINTER(void * uptr)   {return (((size_t*)uptr)-(1+MUSCLE_ENABLE_MEMORY_PARANOIA));}
static inline void   *  CONVERT_INTERNAL_TO_USER_POINTER(size_t * iptr) {return ((void *)(iptr+1+MUSCLE_ENABLE_MEMORY_PARANOIA));}
static inline size_t ** CONVERT_INTERNAL_TO_FRONT_GUARD(size_t * iptr)  {return ((size_t **)(((size_t*)iptr)+1));}
static inline size_t ** CONVERT_INTERNAL_TO_REAR_GUARD(size_t * iptr)   {return ((size_t **)((((char *)iptr)+(*iptr))-(MUSCLE_ENABLE_MEMORY_PARANOIA*sizeof(size_t *))));}

status_t MemoryParanoiaCheckBuffer(void * userPtr, bool crashIfInvalid)
{
   if (userPtr)
   {
      size_t * internalPtr = CONVERT_USER_TO_INTERNAL_POINTER(userPtr);
      size_t ** frontRead  = CONVERT_INTERNAL_TO_FRONT_GUARD(internalPtr);
      size_t ** rearRead   = CONVERT_INTERNAL_TO_REAR_GUARD(internalPtr);
      size_t userBufLen    = CONVERT_INTERNAL_TO_USER_SIZE(*internalPtr);

      bool foundCorruption = false;
      for (int i=0; i<MUSCLE_ENABLE_MEMORY_PARANOIA; i++)
      {
         const size_t * expectedFrontVal = internalPtr+i;
         const size_t * expectedRearVal  = internalPtr+i+MUSCLE_ENABLE_MEMORY_PARANOIA;

         if (frontRead[i] != expectedFrontVal)
         {
            foundCorruption = true;
            TCHECKPOINT;
            printf("MEMORY GUARD CORRUPTION (%i words before front): buffer (%p," UINT32_FORMAT_SPEC ") (userptr=%p," UINT32_FORMAT_SPEC ") expected %p, got %p!\n", (MUSCLE_ENABLE_MEMORY_PARANOIA-i), internalPtr, (uint32)(*internalPtr), userPtr, (uint32)userBufLen, expectedFrontVal, frontRead[i]);
         }
         if (rearRead[i] != expectedRearVal)
         {
            foundCorruption = true;
            TCHECKPOINT;
            printf("MEMORY GUARD CORRUPTION (%i words after rear):   buffer (%p,%u) (userptr=%p,%u) expected %p, got %p!\n", i+1, internalPtr, *internalPtr, userPtr, userBufLen, expectedRearVal, rearRead[i]);
         }
      }
      if (foundCorruption) 
      {
         printf("CORRUPTED MEMORY BUFFER CONTENTS ARE (including %i front-guards and %i rear-guards of %u bytes each):\n", MUSCLE_ENABLE_MEMORY_PARANOIA, MUSCLE_ENABLE_MEMORY_PARANOIA, sizeof(size_t *));
         for (size_t i=0; i<*internalPtr; i++) printf("%02x ", ((char *)internalPtr)[i]); printf("\n");
         if (crashIfInvalid) 
         {
            fflush(stdout);
            MCRASH("MEMORY PARANOIA:  MEMORY CORRUPTION DETECTED!");
         }
         return B_ERROR;
      }
   }
   return B_NO_ERROR;
}

static void MemoryParanoiaPrepareBuffer(size_t * internalPtr, uint32 oldSize)
{
   size_t ** frontWrite = CONVERT_INTERNAL_TO_FRONT_GUARD(internalPtr);
   size_t ** rearWrite  = CONVERT_INTERNAL_TO_REAR_GUARD(internalPtr);
   for (int i=0; i<MUSCLE_ENABLE_MEMORY_PARANOIA; i++) 
   {
      frontWrite[i] = internalPtr+i;
      rearWrite[i]  = internalPtr+i+MUSCLE_ENABLE_MEMORY_PARANOIA;
   }

   uint32 newSize = CONVERT_INTERNAL_TO_USER_SIZE(*internalPtr);
   if (newSize > oldSize) memset(((char *)CONVERT_INTERNAL_TO_USER_POINTER(internalPtr))+oldSize, MEMORY_PARANOIA_ALLOCATED_GARBAGE_VALUE, newSize-oldSize);
}

# else

// Without memory paranoia, here are the conversion functions used for plain old memory usage tracking
static inline uint32    CONVERT_USER_TO_INTERNAL_SIZE(uint32 uNumBytes) {return (uNumBytes+sizeof(size_t));}
static inline uint32    CONVERT_INTERNAL_TO_USER_SIZE(uint32 iNumBytes) {return (iNumBytes-sizeof(size_t));}
static inline size_t *  CONVERT_USER_TO_INTERNAL_POINTER(void * uptr)   {return (((size_t*)uptr)-1);}
static inline void   *  CONVERT_INTERNAL_TO_USER_POINTER(size_t * iptr) {return ((void *)(iptr+1));}

# endif

static size_t _currentlyAllocatedBytes = 0;  // Running tally of how many bytes our process has allocated

size_t GetNumAllocatedBytes() {return _currentlyAllocatedBytes;}

void * muscleAlloc(size_t userSize, bool retryOnFailure)
{
   using namespace muscle;

   size_t internalSize = CONVERT_USER_TO_INTERNAL_SIZE(userSize);

   void * userPtr = NULL;
   MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   Mutex * glock = ma ? GetGlobalMuscleLock() : NULL;
   if ((glock)&&(glock->Lock() != B_NO_ERROR))
   {
      printf("Error, muscleAlloc() could not lock the global muscle lock!\n");
      SetFailedMemoryRequestSize(userSize);   // FogBugz #7547
      return NULL;  // serialize access to (ma)
   }
#endif

   if ((ma == NULL)||(ma->AboutToAllocate(_currentlyAllocatedBytes, internalSize) == B_NO_ERROR))
   {
      size_t * internalPtr = (size_t *) malloc(internalSize);
      if (internalPtr)
      {
         *internalPtr = internalSize;  // our little header tag so that muscleFree() will know how big the allocation was
         _currentlyAllocatedBytes += internalSize;

#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
         MemoryParanoiaPrepareBuffer(internalPtr, 0);
#endif
         userPtr = CONVERT_INTERNAL_TO_USER_POINTER(internalPtr);
//printf("+"UINT32_FORMAT_SPEC" = "UINT32_FORMAT_SPEC" userPtr=%p\n", (uint32)internalSize, (uint32)_currentlyAllocatedBytes, userPtr);
      }
      else if (ma) ma->AboutToFree(_currentlyAllocatedBytes+internalSize, internalSize);  // FogBugz #4494:  roll back the call to AboutToAllocate()!
   }

   if ((ma)&&(userPtr == NULL)) 
   {
      // I call printf() instead of LogTime() to avoid any chance of an infinite recursion
      printf("muscleAlloc:  allocation failure (tried to allocate " UINT32_FORMAT_SPEC " internal bytes / " UINT32_FORMAT_SPEC " user bytes)\n", (uint32)internalSize, (uint32)userSize);
      fflush(stdout);  // make sure this message gets out!

      ma->AllocationFailed(_currentlyAllocatedBytes, internalSize);  // see if ma can free spare buffers up for us

      // Maybe the AllocationFailed() method was able to free up some memory; so we'll try it one more time
      // That way we might be able to recover without interrupting the operation that was in progress.
      if (retryOnFailure) 
      {
         userPtr = muscleAlloc(userSize, false);
         if (userPtr == NULL) ma->SetAllocationHasFailed(true);
      }
   }

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   if (glock) (void) glock->Unlock();
#endif

   if (userPtr == NULL) SetFailedMemoryRequestSize(userSize);   // FogBugz #7547
   return userPtr;
}

void * muscleRealloc(void * oldUserPtr, size_t newUserSize, bool retryOnFailure)
{
   using namespace muscle;

#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
   TCHECKPOINT;
   (void) MemoryParanoiaCheckBuffer(oldUserPtr);
#endif

        if (oldUserPtr == NULL) return muscleAlloc(newUserSize, retryOnFailure);
   else if (newUserSize == 0)
   {
      muscleFree(oldUserPtr);
      return NULL;
   }

   size_t newInternalSize  = CONVERT_USER_TO_INTERNAL_SIZE(newUserSize);
   size_t * oldInternalPtr = CONVERT_USER_TO_INTERNAL_POINTER(oldUserPtr);
   size_t oldInternalSize  = *oldInternalPtr;
   if (newInternalSize == oldInternalSize) return oldUserPtr;  // same size as before?  Then we are already done!

   void * newUserPtr = NULL;
   MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   Mutex * glock = ma ? GetGlobalMuscleLock() : NULL;
   if ((glock)&&(glock->Lock() != B_NO_ERROR)) 
   {
      printf("Error, muscleRealloc() could not lock the global muscle lock!\n");
      SetFailedMemoryRequestSize(newUserSize);   // FogBugz #7547
      return NULL;  // serialize access to (ma)
   }
#endif

   size_t oldUserSize = CONVERT_INTERNAL_TO_USER_SIZE(oldInternalSize);
   if (newInternalSize > oldInternalSize)
   {
      size_t growBy = newInternalSize-oldInternalSize;
      if ((ma == NULL)||(ma->AboutToAllocate(_currentlyAllocatedBytes, growBy) == B_NO_ERROR))
      {
         size_t * newInternalPtr = (size_t *) realloc(oldInternalPtr, newInternalSize);
         if (newInternalPtr)
         {
            _currentlyAllocatedBytes += growBy;  // only reflect the newly-allocated bytes
            *newInternalPtr = newInternalSize;  // our little header tag so that muscleFree() will know how big the allocation was
            newUserPtr = CONVERT_INTERNAL_TO_USER_POINTER(newInternalPtr);
//printf("r+"UINT32_FORMAT_SPEC"(->"UINT32_FORMAT_SPEC") = "UINT32_FORMAT_SPEC" oldUserPtr=%p newUserPtr=%p\n", (uint32)growBy, (uint32)newInternalSize, (uint32)_currentlyAllocatedBytes, oldUserPtr, newUserPtr);

#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
            MemoryParanoiaPrepareBuffer(newInternalPtr, oldUserSize);
#endif
         }
         else if (ma) ma->AboutToFree(_currentlyAllocatedBytes+growBy, growBy);  // FogBugz #4494:  roll back the call to AboutToAllocate()!
      }

      if ((ma)&&(newUserPtr == NULL)) 
      {
         // I call printf() instead of LogTime() to avoid any chance of an infinite recursion
         printf("muscleRealloc:  reallocation failure (tried to grow " UINT32_FORMAT_SPEC "->" UINT32_FORMAT_SPEC " internal bytes / " UINT32_FORMAT_SPEC "->" UINT32_FORMAT_SPEC " user bytes))\n", (uint32)oldInternalSize, (uint32)newInternalSize, (uint32)oldUserSize, (uint32)newUserSize);
         fflush(stdout);  // make sure this message gets out!

         ma->AllocationFailed(_currentlyAllocatedBytes, growBy);  // see if ma can free spare buffers up for us

         // Maybe the AllocationFailed() method was able to free up some memory; so we'll try it one more time
         // That way we might be able to recover without interrupting the operation that was in progress.
         if (retryOnFailure) 
         {
            newUserPtr = muscleRealloc(oldUserPtr, newUserSize, false);
            if (newUserPtr == NULL) ma->SetAllocationHasFailed(true);
         }
      }
   }
   else
   {
      size_t shrinkBy = oldInternalSize-newInternalSize;
      if (ma) ma->AboutToFree(_currentlyAllocatedBytes, shrinkBy);
      size_t * newInternalPtr = (size_t *) realloc(oldInternalPtr, newInternalSize);
      if (newInternalPtr)
      {
         *newInternalPtr = newInternalSize;  // our little header tag so that muscleFree() will know how big the allocation is now
         _currentlyAllocatedBytes -= shrinkBy;
//printf("r-"UINT32_FORMAT_SPEC"(->"UINT32_FORMAT_SPEC") = "UINT32_FORMAT_SPEC" oldUserPtr=%p newUserPtr=%p\n", (uint32)shrinkBy, (uint32)newInternalSize, (uint32)_currentlyAllocatedBytes, oldUserPtr, newUserPtr);

#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
         MemoryParanoiaPrepareBuffer(newInternalPtr, MUSCLE_NO_LIMIT);
#endif
         newUserPtr = CONVERT_INTERNAL_TO_USER_POINTER(newInternalPtr);
      }
      else 
      {
         newUserPtr = oldUserPtr;  // I guess the best thing to do is just send back the old pointer?  Not sure what to do here.
         printf("muscleRealloc:  reallocation failure (tried to shrink " UINT32_FORMAT_SPEC "->" UINT32_FORMAT_SPEC " internal bytes / " UINT32_FORMAT_SPEC "->" UINT32_FORMAT_SPEC " user bytes))\n", (uint32)oldInternalSize, (uint32)newInternalSize, (uint32)oldUserSize, (uint32)newUserSize);
         fflush(stdout);  // make sure this message gets out!
      }
   }

#ifndef MUSCLE_SINGLE_THREAD_ONLY
   if (glock) (void) glock->Unlock();
#endif

   if (newUserPtr == NULL) SetFailedMemoryRequestSize(newUserSize);   // FogBugz #7547
   return newUserPtr;
}

void muscleFree(void * userPtr)
{
   using namespace muscle;
   if (userPtr)
   {
#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
      TCHECKPOINT;
      MemoryParanoiaCheckBuffer(userPtr);
#endif

      MemoryAllocator * ma = GetCPlusPlusGlobalMemoryAllocator()();

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      Mutex * glock = ma ? GetGlobalMuscleLock() : NULL;
      if ((glock)&&(glock->Lock() != B_NO_ERROR)) 
      {
         printf("Error, muscleFree() could not lock the global muscle lock!!!\n");
         return;  // serialize access to (ma)
      }
#endif

      size_t * internalPtr = CONVERT_USER_TO_INTERNAL_POINTER(userPtr);
      _currentlyAllocatedBytes -= *internalPtr;

      if (ma) ma->AboutToFree(_currentlyAllocatedBytes, *internalPtr);
//printf("-"UINT32_FORMAT_SPEC" = "UINT32_FORMAT_SPEC" userPtr=%p\n", (uint32)*internalPtr, (uint32)_currentlyAllocatedBytes, userPtr);

#ifndef MUSCLE_SINGLE_THREAD_ONLY
      if (glock) (void) glock->Unlock();
#endif

#if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
      memset(internalPtr, MEMORY_PARANOIA_DEALLOCATED_GARBAGE_VALUE, *internalPtr);  // make it obvious this memory was freed by munging it
#endif

      free(internalPtr);
   }
}

}; // end namespace muscle

void * operator new(size_t s) THROW LPAREN BAD_ALLOC RPAREN
{
   using namespace muscle;
   void * ret = muscleAlloc(s);
   if (ret == NULL) {THROW BAD_ALLOC LPAREN RPAREN;}
   return ret;
}

void * operator new[](size_t s) THROW LPAREN BAD_ALLOC RPAREN
{
   using namespace muscle;
   void * ret = muscleAlloc(s);
   if (ret == NULL) {THROW BAD_ALLOC LPAREN RPAREN;}
   return ret;
}

// Borland, VC++, and OSF don't like separate throw/no-throw operators, it seems
# ifndef WIN32
#  ifndef __osf__
void * operator new(  size_t s, nothrow_t const &) THROW LPAREN RPAREN {using namespace muscle; return muscleAlloc(s);}
void * operator new[](size_t s, nothrow_t const &) THROW LPAREN RPAREN {using namespace muscle; return muscleAlloc(s);}
#  endif
# endif

void operator delete(  void * p) THROW LPAREN RPAREN {using namespace muscle; muscleFree(p);}
void operator delete[](void * p) THROW LPAREN RPAREN {using namespace muscle; muscleFree(p);}

#else
# if MUSCLE_ENABLE_MEMORY_PARANOIA > 0
#  error "If you want to enable MUSCLE_ENABLE_MEMORY_PARANOIA, you must define MUSCLE_ENABLE_MEMORY_TRACKING also!"
# endif
status_t MemoryParanoiaCheckBuffer(void *, bool) {return B_NO_ERROR;}
#endif
