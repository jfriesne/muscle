#include "system/SharedMemory.h"  // must be first!
#include "util/TimeUtilityFunctions.h"  // for Snooze64()

#if !defined(WIN32) && !defined(MUSCLE_FAKE_SHARED_MEMORY)
# include <sys/types.h>
# include <sys/ipc.h>
# include <sys/sem.h>
# include <sys/shm.h>
#endif

#if defined(ANDROID) && !defined(MUSCLE_FAKE_SHARED_MEMORY) && !defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE) && (__ANDROID_API__ < 26)
# define MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE
# warning SharedMemory API requires Android SDK version 26 or higher in order to function correctly.
#endif

namespace muscle {

#if !defined(WIN32) && !defined(MUSCLE_FAKE_SHARED_MEMORY) && !defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
enum {LARGEST_SEMAPHORE_DELTA = 10000};  // I'm assuming there will never be this many processes

// Unbelievable how messed up the semctl() API is :^P
# if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
#  define DECLARE_SEMCTL_ARG(semopts) semun semopts
# elif defined(__APPLE__)
#  define DECLARE_SEMCTL_ARG(semopts) semun semopts
# else
#  define DECLARE_SEMCTL_ARG(semopts) union semun {int val; struct semid_ds * buf; unsigned short * array; struct seminfo * __buf;} semopts
#endif

#endif

SharedMemory :: SharedMemory()
   :
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
   status_t ret;

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
      else {MWARN_OUT_OF_MEMORY; ret = B_OUT_OF_MEMORY;}
   }
#elif defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
   (void) keyString;
   (void) createSize;
   (void) returnLocked;
   return B_UNIMPLEMENTED;   // Android SDK level too low (<26) ?
#elif defined(WIN32)
   if (keyString == NULL)
   {
      char buf[128];
      muscleSprintf(buf, UINT64_FORMAT_SPEC, GetTickCount64());  // No user-supplied name?  We'll pick an arbitrary name then
      keyString = buf;
   }
   _areaName = keyString;

   // For windows we only use a Mutex, because even a Windows semaphore isn't enough to
   // do shared read locking.  When I figure out how to do interprocess shared read locking
   // under Windows I will implement that, but for now it's always exclusive-locking.  :^(
   _mutex = CreateMutexA(NULL, true, (_areaName+"__mutex")());
   if (_mutex != NULL)
   {
      if (GetLastError() == ERROR_ALREADY_EXISTS) (void) LockAreaReadWrite().IsOK(ret);
      else
      {
         // We created it in our CreateMutex() call, and it's already locked for us
         _isLocked = true;
         _isLockedReadOnly = false;
      }

      if (ret.IsOK())
      {
         char buf[MAX_PATH];
         if (GetTempPathA(sizeof(buf), buf) > 0)
         {
            _fileName = _areaName.WithPrepend(buf)+"__file";
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
                     else ret = B_ERROR("MapViewOfFile() failed");
                  }
                  else ret = B_ERROR("CreateFileMappingA() failed");
               }
            }
            else ret = B_ERROR("CreateFileA() failed");
         }
         else ret = B_ERROR("GetTempPathA() failed");
      }
   }
   else ret = B_ERROR("CreateMutexA() failed");
#else
   key_t requestedKey = IPC_PRIVATE;
   if (keyString)
   {
      requestedKey = (key_t) CalculateHashCode(keyString, (uint32)strlen(keyString));
      if (requestedKey == IPC_PRIVATE) requestedKey++;
      _areaName = keyString;
   }

   DECLARE_SEMCTL_ARG(semopts);
   memset(&semopts, 0, sizeof(semopts));
   const int permissionBits = 0777;

   // Try to create a new semaphore to control access to our area
   _semID = semget(requestedKey, 1, IPC_CREAT|IPC_EXCL|permissionBits);
   if (_semID >= 0)
   {
      // there's a small race condition here (between when we create the semaphore and when
      // we adjust its value upwards), so we'll poll-loop on sem_otime in the other process/codepath
      // to avoid any chance of getting bit by it
      if (AdjustSemaphore(LARGEST_SEMAPHORE_DELTA, false).IsError(ret))  // AdjustSemaphore() will set sem_otime to non-zero for other processes to see
      {
         // roll back the creation of the semaphore on error
         (void) semctl(_semID, 0, IPC_RMID);  // clean up
         _semID = -1;
      }
   }
   else
   {
      _semID = semget(requestedKey, 1, permissionBits);  // Couldn't create a new semaphore?  then try to open the existing one
      if (_semID >= 0)
      {
         bool okToGo = false;

         // avoid a potential race condition by not continuing onwards until we see sem_otime become non-zero
         for (uint32 i=0; i<10; i++)
         {
            struct semid_ds semInfo = {};  // the braces zero-initialize the struct for us, to keep clang++SA happy
            semopts.buf = &semInfo;
            if (semctl(_semID, 0, IPC_STAT, semopts) == 0)
            {
               if (semInfo.sem_otime != 0)
               {
                  okToGo = true;
                  break;
               }
               else (void) Snooze64(MillisToMicros(i*5));  // give the creator a little more time (up to 225mS) to finish his semaphore-setup
            }
            else
            {
               ret = B_ERRNO;
               break;  // hmm, something is wrong
            }
         }

         if (okToGo == false) _semID = -1;
      }
      else ret = B_ERRNO;
   }

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

      if ((_key != IPC_PRIVATE)&&(LockAreaReadWrite().IsOK(ret)))
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
               else ret = B_ERRNO;
            }
            else ret = B_ERRNO;
         }
         else ret = B_ERRNO;
      }
   }
#endif

   UnsetArea();  // oops, roll back everything!
   return ret | B_BAD_OBJECT;
}

status_t SharedMemory :: DeleteArea()
{
#if defined(MUSCLE_FAKE_SHARED_MEMORY)
   (void) UnsetArea();
   return B_NO_ERROR;
#elif defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
   return B_UNIMPLEMENTED;
#else
# if defined(WIN32)
   if (_mutex != NULL)
# else
   if (_semID >= 0)
# endif
   {
      status_t ret;
      if ((_isLocked)&&(_isLockedReadOnly)) UnlockArea();
      if ((_isLocked)||(LockAreaReadWrite().IsOK(ret)))
      {
# ifdef WIN32
         const String fileName = _fileName;  // hold as temp since UnsetArea() will clear it
         UnsetArea();
         return DeleteFileA(fileName()) ? B_NO_ERROR : B_ERRNO;  // now that everything is detached, try to delete the file
# else
         if (_areaID >= 0) (void) shmctl(_areaID, IPC_RMID, NULL);  // bye bye shared memory!
         _areaID = -1;

         (void) semctl(_semID, 0, IPC_RMID, 0);  // bye bye semaphore!

         _semID = -1;
         UnsetArea();
         return B_NO_ERROR;
# endif
      }
      else return ret;
   }
   return B_BAD_OBJECT;
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
#elif defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
   // empty -- Android SDK level too low (<26) ?
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
#elif defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
   (void) readOnly;
   return B_UNIMPLEMENTED;  // Android SDK level too low (<26) ?
#else
   status_t ret;
   if (_isLocked) ret = B_LOCK_FAILED;
   else
   {
      _isLocked = true;  // Set these first just so they are correct while we're waiting
      _isLockedReadOnly = readOnly;
# ifdef WIN32
      if (WaitForSingleObject(_mutex, INFINITE) == WAIT_OBJECT_0) return B_NO_ERROR;
                                                             else ret = B_ERRNO;
# else
      if (AdjustSemaphore(_isLockedReadOnly ? -1: -LARGEST_SEMAPHORE_DELTA, true).IsOK(ret)) return B_NO_ERROR;
# endif
      _isLocked = _isLockedReadOnly = false;  // oops, roll back!
   }
   return ret;
#endif
}

void SharedMemory :: UnlockArea()
{
   if (_isLocked)
   {
#if !defined(MUSCLE_FAKE_SHARED_MEMORY)
# ifdef WIN32
      (void) ReleaseMutex(_mutex);
# elif defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
      // empty
# else
      (void) AdjustSemaphore(_isLockedReadOnly ? 1 : LARGEST_SEMAPHORE_DELTA, true);
# endif
#endif
      _isLocked = _isLockedReadOnly = false;
   }
}

#if !defined(WIN32) && !defined(MUSCLE_FAKE_SHARED_MEMORY) && !defined(MUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE)
status_t SharedMemory :: AdjustSemaphore(short delta, bool enableUndoOnProcessExit)
{
   if (_semID >= 0)
   {
      const short flags = enableUndoOnProcessExit ? SEM_UNDO : 0;  // kept separate just to keep the compiler happy
      struct sembuf sop = {0, delta, flags};

      while(1)
      {
         if (semop(_semID, &sop, 1) == 0) return B_NO_ERROR;
         if (errno != EINTR) return B_ERRNO;  // on EINTR, we'll try again --jaf
      }
   }
   return B_BAD_OBJECT;
}
#endif

} // end namespace muscle
