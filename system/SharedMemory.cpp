#include "system/SharedMemory.h"  // must be first!

#ifndef WIN32
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/sem.h>
# include <sys/shm.h>
#endif

namespace muscle {

#ifndef WIN32
static const short LARGEST_SEMAPHORE_DELTA = 10000;  // I'm assuming there will never be this many processes

// Unbelievable how messed up the semctl() API is :^P
# if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
#  define DECLARE_SEMCTL_ARG(semopts) semun semopts
# elif defined(__APPLE__)
#  define DECLARE_SEMCTL_ARG(semopts) semun semopts
# else
#  define DECLARE_SEMCTL_ARG(semopts) union semun {int val; struct semid_ds * buf; unsigned short * array; struct seminfo * __buf;} semopts
#endif

#endif

SharedMemory :: SharedMemory() : 
#ifdef WIN32
   _mutex(NULL), _file(INVALID_HANDLE_VALUE), _map(NULL),
#else
   _key(IPC_PRIVATE), _areaID(-1), _semID(-1), 
#endif
   _area(NULL), _areaSize(0), _isLocked(false), _isLockedReadOnly(false), _isCreatedLocally(false)
{
   // empty
}

SharedMemory :: ~SharedMemory()
{
   UnsetArea();  // clean up
}

status_t SharedMemory :: SetArea(const char * keyString, uint32 createSize, bool returnLocked)
{
   UnsetArea();  // make sure everything is deallocated to start with

#if defined(MUSCLE_FAKE_SHARED_MEMORY)
   if (createSize > 0)
   {
      _area = muscleAlloc(createSize);
      if (_area) 
      {
         memset(_area, 0, createSize);
         _areaName         = keyString;
         _areaSize         = createSize; 
         _isCreatedLocally = true;
         _isLocked         = returnLocked;
         _isLockedReadOnly = false;
         return B_NO_ERROR;
      }
      else WARN_OUT_OF_MEMORY;   
   }
#elif defined(WIN32)
   char buf[64];
   if (keyString == NULL)
   {
      sprintf(buf, INT32_FORMAT_SPEC, GetTickCount());  // No user-supplied name?  We'll pick an arbitrary name then
      keyString = buf;
   }
   _areaName = keyString;

   // For windows we only use a Mutex, because even a Windows semaphore isn't enough to
   // do shared read locking.  When I figure out how to do interprocess shared read locking
   // under Windows I will implement that, but for now it's always exclusive-locking.  :^(
   _mutex = CreateMutexA(NULL, true, (_areaName+"__mutex")());
   if (_mutex != NULL)
   {
      bool ok = true;
      if (GetLastError() == ERROR_ALREADY_EXISTS) ok = (LockAreaReadWrite() == B_NO_ERROR);
      else
      {
         // We created it in our CreateMutex() call, and it's already locked for us
         _isLocked = true;
         _isLockedReadOnly = false;
      }
      if (ok)
      {
         char buf[MAX_PATH];
         if (GetTempPathA(sizeof(buf), buf) > 0)
         {
            _fileName = _areaName.Prepend(buf)+"__file";
            _file = CreateFileA(_fileName(), GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH|FILE_FLAG_RANDOM_ACCESS, NULL);
            if (_file != INVALID_HANDLE_VALUE)
            {
               _isCreatedLocally = (GetLastError() != ERROR_ALREADY_EXISTS);
               if (createSize == 0) createSize = GetFileSize(_file, NULL);
               _areaSize = createSize;  // assume the file will be resized automagically for us
               if (_areaSize > 0)
               {
                  _map = CreateFileMappingA(_file, NULL, PAGE_READWRITE, 0, createSize, (_areaName+"__map")());
                  if (_map)
                  {
                     _area = MapViewOfFile(_map, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                     if (_area)
                     {
                        if (returnLocked == false) UnlockArea();
                        return B_NO_ERROR;
                     }
                  }
               }
            }
         }
      }
   }
#else
   key_t requestedKey = IPC_PRIVATE;
   if (keyString)
   {
      requestedKey = (key_t) CalculateHashCode(keyString,strlen(keyString));
      if (requestedKey == IPC_PRIVATE) requestedKey++;
      _areaName = keyString;
   }

   DECLARE_SEMCTL_ARG(semopts);
   const int permissionBits = 0777;

   // Try to create a new semaphore to control access to our area
   _semID = semget(requestedKey, 1, IPC_CREAT|IPC_EXCL|permissionBits);
   if (_semID >= 0)
   {
      // race condition here!?
      semopts.val = LARGEST_SEMAPHORE_DELTA;
      if (semctl(_semID, 0, SETVAL, semopts) < 0) _semID = -1; // oops!
   }
   else _semID = semget(requestedKey, 1, permissionBits);  // Couldn't create?  then get the existing one

   if (_semID >= 0)
   {
      _key = requestedKey;

      // If we requested a private key, we still need to know the actual key value
      if (_key == IPC_PRIVATE)
      {
         struct semid_ds semInfo = {};  // the braces zero-initialize the struct for us, to keep clang++SA happy
         semopts.buf = &semInfo;
         if (semctl(_semID, 0, IPC_STAT, semopts) == 0) 
         {
# ifdef __linux__
            _key = semInfo.sem_perm.__key;

// Mac os-x leopard uses '_key' by default. Both Tiger and Leopard may use _key if the following condition is true, otherwise they use 'key'.
# elif (defined(__APPLE__) && (defined(__POSIX_C_SOURCE) || defined(__LP64__))) || __DARWIN_UNIX03
            _key = semInfo.sem_perm._key;
# else
            _key = semInfo.sem_perm.key;
# endif
         }
         _areaName = "private";  // sorry, it's the best I can do short of figuring out how to invert the hash function!
      }

      if ((_key != IPC_PRIVATE)&&(LockAreaReadWrite() == B_NO_ERROR))
      {
         _areaID = shmget(_key, 0, permissionBits);
         if ((_areaID < 0)&&(createSize > 0)) 
         {
            _areaID = shmget(_key, createSize, IPC_CREAT|IPC_EXCL|permissionBits);
            _isCreatedLocally = true;
         }
         if (_areaID >= 0)
         {
            _area = shmat(_areaID, NULL, 0);
            if ((_area)&&(_area != ((void *)-1)))  // FogBugz #7294
            {
               // Now get the stats on our area
               struct shmid_ds buf;
               if (shmctl(_areaID, IPC_STAT, &buf) == 0)
               {
                  _areaSize = (uint32) buf.shm_segsz;
                  if (returnLocked == false) UnlockArea();
                  return B_NO_ERROR;
               }
            }
         }
      }
   }
#endif

   UnsetArea();  // oops, roll back everything!
   return B_ERROR;
}

status_t SharedMemory :: DeleteArea()
{
#if defined(MUSCLE_FAKE_SHARED_MEMORY)
   (void) UnsetArea();
   return B_NO_ERROR;
#else 
# if defined(WIN32)
   if (_mutex != NULL)
# else
   if (_semID >= 0)
# endif
   {
      if ((_isLocked)&&(_isLockedReadOnly)) UnlockArea();
      if ((_isLocked)||(LockAreaReadWrite() == B_NO_ERROR))
      {
# ifdef WIN32
         String fileName = _fileName;  // hold as temp since UnsetArea() will clear it
         UnsetArea();
         return DeleteFileA(fileName()) ? B_NO_ERROR : B_ERROR;  // now that everything is detached, try to delete the file
# else
         if (_areaID >= 0) (void) shmctl(_areaID, IPC_RMID, NULL);  // bye bye shared memory!
         _areaID = -1;

         (void) semctl(_semID, 0, IPC_RMID, 0);  // bye bye semaphore!

         _semID = -1;
         UnsetArea();
         return B_NO_ERROR;
# endif
      }
   }
   return B_ERROR;
#endif
}

void SharedMemory :: UnsetArea()
{
   UnlockArea();

#if defined(MUSCLE_FAKE_SHARED_MEMORY)
   if (_area) 
   {
      muscleFree(_area);
      _area = NULL;
   }
#elif defined(WIN32)
   if (_area)
   {
      UnmapViewOfFile(_area);
      _area = NULL;
   }
   if (_map)
   {
      CloseHandle(_map);
      _map = NULL;
   }
   if (_file != INVALID_HANDLE_VALUE) 
   {
      CloseHandle(_file);
      _file = INVALID_HANDLE_VALUE;
   }
   if (_mutex) 
   {
      CloseHandle(_mutex);
      _mutex = NULL;
   }
   _fileName.Clear();
#else
   if (_area) 
   {
      (void) shmdt(_area);  // we're done with it now
      _area = NULL;
   }
   _areaID           = -1;
   _key              = IPC_PRIVATE;
   _semID            = -1;
#endif

   _areaName.Clear();
   _areaSize         = 0;
   _isCreatedLocally = false;
}

status_t SharedMemory :: LockArea(bool readOnly)
{
#if defined(MUSCLE_FAKE_SHARED_MEMORY)
   _isLocked = true;
   _isLockedReadOnly = readOnly;
   return B_NO_ERROR;
#else
   if (_isLocked == false)
   {
      _isLocked = true;  // Set these first just so they are correct while we're waiting
      _isLockedReadOnly = readOnly;
# ifdef WIN32
      if (WaitForSingleObject(_mutex, INFINITE) == WAIT_OBJECT_0) return B_NO_ERROR;
# else
      if (AdjustSemaphore(_isLockedReadOnly ? -1: -LARGEST_SEMAPHORE_DELTA) == B_NO_ERROR) return B_NO_ERROR;
# endif
      _isLocked = _isLockedReadOnly = false;  // oops, roll back!
   }
   return B_ERROR;
#endif
}

void SharedMemory :: UnlockArea()
{
   if (_isLocked)
   {
#if !defined(MUSCLE_FAKE_SHARED_MEMORY)
# ifdef WIN32
      (void) ReleaseMutex(_mutex);
# else
      (void) AdjustSemaphore(_isLockedReadOnly ? 1 : LARGEST_SEMAPHORE_DELTA);
# endif
#endif
      _isLocked = _isLockedReadOnly = false;
   }
}

#ifndef WIN32
status_t SharedMemory :: AdjustSemaphore(short delta)
{
   if (_semID >= 0)
   {
      struct sembuf sop = {0, delta, SEM_UNDO};

      while(1)
      {
         if (semop(_semID, &sop, 1) == 0) return B_NO_ERROR;
         if (errno != EINTR) break;  // on EINTR, we'll try again --jaf
      }
   }
   return B_ERROR;
}
#endif

}; // end namespace muscle
