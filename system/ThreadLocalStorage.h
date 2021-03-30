/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#ifndef MuscleThreadLocalStorage_h
#define MuscleThreadLocalStorage_h

#include "system/Thread.h"  // to get the #defines that are calculated in Thread.h

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
# if !defined(MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD) && defined(_MSC_VER) && (_MSC_VER < 1900)
#  define MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD  // MSVC2013 and earlier don't support thread_local, sigh
# endif
# if !defined(MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD) && defined(__apple_build_version__) && (__apple_build_version__ < 8000042)
#  define MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD  // XCode before 8.0 doesn't support thread_local, sigh
# endif
#elif defined(MUSCLE_USE_PTHREADS)
  // deliberately empty
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
  // deliberately empty
#elif defined(MUSCLE_QT_HAS_THREADS) && (QT_VERSION >= 0x030200)
#  if (QT_VERSION >= 0x040000)
#   include <QThreadStorage>
#  else
#   include <qthreadstorage.h>
#  endif
#  define MUSCLE_USE_QT_THREADLOCALSTORAGE
#else
# error "muscle/ThreadLocalStorage.h:  ThreadLocalStorage not implemented for this environment, sorry!"
#endif

namespace muscle {

/** This class implements easy-to-use thread-local-storage.  Typically what you would do is
  * to create a ThreadLocalStorage object at the beginning of program execution (e.g. as a global
  * variable, or at the top of main()).  Then the various threads of your program can call
  * GetThreadLocalObject() on it, and each thread will receive a pointer that is unique to that
  * thread, which it can use without needing to do any serialization.
  */
template <class ObjType> class ThreadLocalStorage
{
public:
   /** Constructor.
     * @param freeHeldObjectsOnExit If left as true (the default value) then any thread-local objects we allocate
     *                              will be automatically deleted when their thread exits.  If set to false, they will
     *                              be left in-place.
     */
   ThreadLocalStorage(bool freeHeldObjectsOnExit = true) : _freeHeldObjects(freeHeldObjectsOnExit)
   {
#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
      // Avoid re-entrancy problems when the deadlock callbacks are using ThreadLocalStorage to initialize themselves!
      _allocedObjsMutex.AvoidFindDeadlockCallbacks();
#endif

#if defined(MUSCLE_USE_PTHREADS)
      _isKeyValid = (pthread_key_create(&_key, _freeHeldObjects?((PthreadDestructorFunction)DeleteObjFunc):NULL) == 0);
#elif !defined(MUSCLE_USE_CPLUSPLUS11_THREADS) && defined(MUSCLE_PREFER_WIN32_OVER_QT)
      _tlsIndex = TlsAlloc();
#endif
   }

   /** Destructor.  Deletes all objects that were previously created by this ThreadLocalStorage object. */
   virtual ~ThreadLocalStorage()
   {
      if (IsSetupOkay())
      {
#if defined(MUSCLE_USE_PTHREADS)
         (void) pthread_key_delete(_key);
#elif !defined(MUSCLE_USE_CPLUSPLUS11_THREADS) && defined(MUSCLE_PREFER_WIN32_OVER_QT)
         TlsFree(_tlsIndex);
#endif
      }
#if !(defined(MUSCLE_USE_QT_THREADLOCALSTORAGE) || defined(MUSCLE_USE_PTHREADS))
      if (_freeHeldObjects) for (uint32 i=0; i<_allocedObjs.GetNumItems(); i++) delete _allocedObjs[i];
#endif
   }

   /** Returns a pointer to the copy of the thread-local-object to be used by the calling thread, or NULL if no such object is installed. */
   ObjType * GetThreadLocalObject() const {return IsSetupOkay() ? GetThreadLocalObjectAux() : NULL;}

   /** Returns a pointer to the thread-local object used by the calling thread, if there is one.
     * If there isn't one, a default object will be created and installed for the current thread, and a pointer
     * to that object will be returned.
     * If the creation of the default object fails, NULL will be returned.
     */
   ObjType * GetOrCreateThreadLocalObject()
   {
      ObjType * ret = GetThreadLocalObject();
      if (ret) return ret;

      // If we got here, we'll auto-create an object to return
      ret = newnothrow ObjType;
      if (ret == NULL) {MWARN_OUT_OF_MEMORY; return NULL;}

      if (SetThreadLocalObject(ret).IsOK()) return ret;
      else
      {
         delete ret;
         return NULL;
      }
   }

   /** Sets the thread-local object to (newObj).
     * @param newObj An object to install as the thread-local object, or NULL.
     *               If (newObj) is non-NULL, then this class takes ownership of the object
     *               and will call delete on it at the appropriate time.  If (newObj) is
     *               NULL, then this method will free any existing object only.
     * @returns B_NO_ERROR if (newObj) was successfully installed, or an error code if it was not.
     *                     If an error occurred, then (newObj) still belongs to the caller.
     */
   status_t SetThreadLocalObject(ObjType * newObj)
   {
      if (IsSetupOkay() == false) return B_BAD_OBJECT;

      ObjType * oldObj = GetThreadLocalObjectAux();
      if (oldObj == newObj) return B_NO_ERROR;  // nothing to do!

#if defined(MUSCLE_USE_QT_THREADLOCALSTORAGE) || defined(MUSCLE_USE_PTHREADS)
      return SetThreadLocalObjectAux(newObj);   // pthreads and Qt manage memory so we don't have to
#else

      MRETURN_ON_ERROR(_allocedObjsMutex.Lock());

      status_t ret;
      if (SetThreadLocalObjectAux(newObj).IsOK(ret))  // SetThreadLocalObjectAux() MUST be called first to avoid re-entrancy trouble!
      {
         const int32 idx = oldObj ? _allocedObjs.IndexOf(oldObj) : -1;
         if (idx >= 0)
         {
            if (newObj) _allocedObjs[idx] = newObj;
                   else (void) _allocedObjs.RemoveItemAt(idx);
         }
         else if (newObj) (void) _allocedObjs.AddTail(newObj);
      }

      _allocedObjsMutex.Unlock();
      if (ret.IsOK()) delete oldObj;
      return ret;
#endif
   }

private:
#if defined(MUSCLE_USE_QT_THREADLOCALSTORAGE)
   QThreadStorage<ObjType *> _storage;
#else
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
#  if defined(MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD)
#   if defined(_MSC_VER)
   static __declspec(thread) ObjType * _threadLocalObject;
#   elif defined(__GNUC__)
   static __thread ObjType * _threadLocalObject;
#   endif
#  else
   static thread_local ObjType * _threadLocalObject;
#  endif
# elif defined(MUSCLE_USE_PTHREADS)
   typedef void (*PthreadDestructorFunction )(void *);
   static void DeleteObjFunc(void * o) {delete ((ObjType *)o);}
   bool _isKeyValid;
   pthread_key_t _key;
# elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   DWORD _tlsIndex;
# endif
   Mutex _allocedObjsMutex;
   Queue<ObjType *> _allocedObjs;
#endif

   bool _freeHeldObjects;

   inline ObjType * GetThreadLocalObjectAux() const
   {
#if defined(MUSCLE_USE_QT_THREADLOCALSTORAGE)
      return _storage.localData();
#elif defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
      return _threadLocalObject;
#elif defined(MUSCLE_USE_PTHREADS)
      return (ObjType *) pthread_getspecific(_key);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      return (ObjType *) TlsGetValue(_tlsIndex);
#endif
   }

   inline status_t SetThreadLocalObjectAux(ObjType * o)
   {
#if defined(MUSCLE_USE_QT_THREADLOCALSTORAGE)
      _storage.setLocalData(o); return B_NO_ERROR;
#elif defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
      _threadLocalObject = o;   return B_NO_ERROR;
#elif defined(MUSCLE_USE_PTHREADS)
      return B_ERRNUM(pthread_setspecific(_key, o));
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      return TlsSetValue(_tlsIndex, o) ? B_NO_ERROR : B_ERRNO;
#endif
   }

   inline bool IsSetupOkay() const
   {
#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS) || defined(MUSCLE_USE_QT_THREADLOCALSTORAGE)
      return true;
#elif defined(MUSCLE_USE_PTHREADS)
      return _isKeyValid;
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      return (_tlsIndex != TLS_OUT_OF_INDEXES);
#endif
   }
};

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
// declare storage space for our thread-local object
# if defined(MUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD)
#  if defined(_MSC_VER)
template <typename T> __declspec(thread) T * ThreadLocalStorage<T>::_threadLocalObject;
#  elif defined(__GNUC__)
template <typename T> __thread           T * ThreadLocalStorage<T>::_threadLocalObject;
#  else
template <typename T> T * ThreadLocalStorage<T>::_threadLocalObject;
#  endif
# else
template <typename T> thread_local T * ThreadLocalStorage<T>::_threadLocalObject;
# endif
#endif

} // end namespace muscle

#endif
