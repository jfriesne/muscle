/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#if defined(MUSCLE_USE_PTHREADS)
# include <pthread.h>  // in case both MUSCLE_USE_CPLUSPLUS11_THREADS and MUSCLE_USE_PTHREADS are defined at once, e.g. for better SetThreadPriority() support
#endif

#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"
#include "dataio/TCPSocketDataIO.h"  // to get the proper #includes for recv()'ing
#include "system/SetupSystem.h"  // for GetCurrentThreadID()

#if defined(MUSCLE_USE_QT_THREADS) && defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
# include "qtsupport/QMessageTransceiverThread.h"   // for MuscleQThreadSocketNotifier
#endif

#if defined(MUSCLE_PREFER_WIN32_OVER_QT)
# include <process.h>  // for _beginthreadex()
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# error "You're not allowed use the Thread class if you have the MUSCLE_SINGLE_THREAD_ONLY compiler constant defined!"
#endif

namespace muscle {

#ifdef MUSCLE_ENABLE_DEADLOCK_FINDER
extern void DeadlockFinder_PrintAndClearLogEventsForCurrentThread();
#endif

Thread :: Thread(bool useMessagingSockets)
   : _useMessagingSockets(useMessagingSockets)
   , _messageSocketsAllocated(!useMessagingSockets)
   , _threadRunning(false)
   , _suggestedStackSize(0)
   , _threadStackBase(NULL)
   , _threadPriority(PRIORITY_UNSPECIFIED)
{
#if defined(MUSCLE_USE_QT_THREADS)
   _thread.SetOwner(this);
#endif
}

Thread :: ~Thread()
{
   MASSERT(IsInternalThreadRunning() == false, "You must not delete a Thread object while its internal thread is still running! (i.e. You must call thread.ShutdownInternalThread() or thread.WaitForThreadToExit() before deleting the Thread object)");
   CloseSockets();
}

const ConstSocketRef & Thread :: GetInternalThreadWakeupSocket()
{
   return GetThreadWakeupSocketAux(_threadData[MESSAGE_THREAD_INTERNAL]);
}

const ConstSocketRef & Thread :: GetOwnerWakeupSocket()
{
   return GetThreadWakeupSocketAux(_threadData[MESSAGE_THREAD_OWNER]);
}

const ConstSocketRef & Thread :: GetThreadWakeupSocketAux(ThreadSpecificData & tsd)
{
   if ((_messageSocketsAllocated == false)&&(CreateConnectedSocketPair(_threadData[MESSAGE_THREAD_INTERNAL]._messageSocket, _threadData[MESSAGE_THREAD_OWNER]._messageSocket).IsError())) return GetNullSocket();

   _messageSocketsAllocated = true;
   return tsd._messageSocket;
}

void Thread :: CloseSockets()
{
   if (_useMessagingSockets)
   {
      for (uint32 i=0; i<NUM_MESSAGE_THREADS; i++) _threadData[i]._messageSocket.Reset();
      _messageSocketsAllocated = false;
   }
}

status_t Thread :: StartInternalThread()
{
   if (IsInternalThreadRunning() == false)
   {
      const bool needsInitialSignal = (_threadData[MESSAGE_THREAD_INTERNAL]._messages.HasItems());
      status_t ret;
      if (StartInternalThreadAux().IsOK(ret))
      {
         if (needsInitialSignal) SignalInternalThread();  // make sure he gets his already-queued messages!
         return B_NO_ERROR;
      }
      else return ret;
   }
   return B_BAD_OBJECT;
}

status_t Thread :: StartInternalThreadAux()
{
   if ((_messageSocketsAllocated)||(GetInternalThreadWakeupSocket()()))
   {
      _threadRunning = true;  // set this first, to avoid a race condition with the thread's startup...

      const status_t ret = StartInternalThreadAuxAux();
      if (ret.IsOK()) return ret;  // success!

      _threadRunning = false;  // oops, nevermind, thread spawn failed
      return ret;
   }
   else return B_BAD_OBJECT;
}

status_t Thread :: StartInternalThreadAuxAux()
{
#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
# if !defined(MUSCLE_NO_EXCEPTIONS)
   try {
# endif
      _thread = std::thread(InternalThreadEntryFunc, this);
      return B_NO_ERROR;
# if !defined(MUSCLE_NO_EXCEPTIONS)
   }
   catch(...) {/* empty */} 
   return B_OUT_OF_MEMORY;
# endif
#elif defined(MUSCLE_USE_PTHREADS)
   pthread_attr_t attr;
   if (_suggestedStackSize != 0)
   {
      pthread_attr_init(&attr);
      (void) pthread_attr_setstacksize(&attr, _suggestedStackSize);
   }
   return B_ERRNUM(pthread_create(&_thread, (_suggestedStackSize!=0)?&attr:NULL, InternalThreadEntryFunc, this));
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   typedef unsigned (__stdcall *PTHREAD_START) (void *);
   return ((_thread = (::HANDLE)_beginthreadex(NULL, _suggestedStackSize, (PTHREAD_START)InternalThreadEntryFunc, this, 0, (unsigned *)&_threadID)) != NULL) ? B_NO_ERROR : B_ERRNO;
#elif defined(MUSCLE_USE_QT_THREADS)
   _thread.start();
   return B_NO_ERROR;
#elif defined(__BEOS__) || defined(__HAIKU__)
   if ((_thread = spawn_thread(InternalThreadEntryFunc, "MUSCLE Thread", B_NORMAL_PRIORITY, this)) >= 0)
   {
      if (resume_thread(_thread) == B_NO_ERROR) return B_NO_ERROR;
      else 
      {
         ret = B_ERRNO; 
         kill_thread(_thread);
         return ret;
      }
   }
   return B_ERRNO;
#elif defined(__ATHEOS__)
   if (((_thread = spawn_thread("MUSCLE Thread", InternalThreadEntryFunc, NORMAL_PRIORITY, 32767, this)) >= 0)&&(resume_thread(_thread) >= 0)) return B_NO_ERROR;
   return B_ERRNO;
#endif
}

void Thread :: ShutdownInternalThread(bool waitForThread)
{
   if (IsInternalThreadRunning())
   {
      (void) SendMessageToInternalThread(MessageRef());  // a NULL message ref tells him to quit
      if (waitForThread) WaitForInternalThreadToExit();
   }
}

status_t Thread :: SendMessageToInternalThread(const MessageRef & ref) 
{
   return SendMessageAux(MESSAGE_THREAD_INTERNAL, ref);
}

status_t Thread :: SendMessageToOwner(const MessageRef & ref)
{
   return SendMessageAux(MESSAGE_THREAD_OWNER, ref);
}

status_t Thread :: SendMessageAux(int whichQueue, const MessageRef & replyRef)
{
   status_t ret;
   ThreadSpecificData & tsd = _threadData[whichQueue];
   if (tsd._queueLock.Lock().IsOK(ret))
   {
      const bool sendNotification = ((tsd._messages.AddTail(replyRef).IsOK(ret))&&(tsd._messages.GetNumItems() == 1));
      (void) tsd._queueLock.Unlock();
      if ((sendNotification)&&(_signalLock.Lock().IsOK(ret)))
      {
         switch(whichQueue)
         {
            case MESSAGE_THREAD_INTERNAL: SignalInternalThread(); break;
            case MESSAGE_THREAD_OWNER:    SignalOwner();          break;
         }
         (void) _signalLock.Unlock();
      }
   }
   return ret;
}

void Thread :: SignalInternalThread() 
{
   SignalAux(MESSAGE_THREAD_OWNER);  // we send a byte on the owner's socket and the byte comes out on the internal socket
}

void Thread :: SignalOwner() 
{
   SignalAux(MESSAGE_THREAD_INTERNAL);  // we send a byte on the internal socket and the byte comes out on the owner's socket
}

void Thread :: SignalAux(int whichSocket)
{
   if (_messageSocketsAllocated)
   {
      const int fd = _threadData[whichSocket]._messageSocket.GetFileDescriptor();
      if (fd >= 0) 
      {
         const char junk = 'S';
         (void) send_ignore_eintr(fd, &junk, sizeof(junk), 0);
      }
   }
}

int32 Thread :: GetNextReplyFromInternalThread(MessageRef & ref, uint64 wakeupTime)
{
   return WaitForNextMessageAux(_threadData[MESSAGE_THREAD_OWNER], ref, wakeupTime);
}

int32 Thread :: WaitForNextMessageFromOwner(MessageRef & ref, uint64 wakeupTime)
{
   return WaitForNextMessageAux(_threadData[MESSAGE_THREAD_INTERNAL], ref, wakeupTime);
}

int32 Thread :: WaitForNextMessageAux(ThreadSpecificData & tsd, MessageRef & ref, uint64 wakeupTime)
{
   int32 ret = -1;  // pessimistic default
   if (tsd._queueLock.Lock().IsOK())
   {
      if (tsd._messages.RemoveHead(ref).IsOK()) ret = tsd._messages.GetNumItems();
      (void) tsd._queueLock.Unlock();

      int msgfd;
      if ((ret < 0)&&((msgfd = tsd._messageSocket.GetFileDescriptor()) >= 0))  // no Message available?  then we'll have to wait until there is one!
      {
         // block until either 
         //   (a) a new-message-signal-byte wakes us, or 
         //   (b) we reach our wakeup/timeout time, or 
         //   (c) a user-specified socket in the socket set selects as ready-for-something
         for (uint32 i=0; i<ARRAYITEMS(tsd._socketSets); i++)
         {
            const Hashtable<ConstSocketRef, bool> & t = tsd._socketSets[i];
            if (t.HasItems())
            {
               for (HashtableIterator<ConstSocketRef, bool> iter(t, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
               {
                  int nextFD = iter.GetKey().GetFileDescriptor();
                  if (nextFD >= 0) tsd._multiplexer.RegisterSocketForEventsByTypeIndex(nextFD, i);
               }
            }
         }
         tsd._multiplexer.RegisterSocketForReadReady(msgfd);

         if (tsd._multiplexer.WaitForEvents(wakeupTime) >= 0)
         {
            for (uint32 j=0; j<ARRAYITEMS(tsd._socketSets); j++)
            {
               Hashtable<ConstSocketRef, bool> & t = tsd._socketSets[j];
               if (t.HasItems()) for (HashtableIterator<ConstSocketRef, bool> iter(t, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++) iter.GetValue() = tsd._multiplexer.IsSocketEventOfTypeFlagged(iter.GetKey().GetFileDescriptor(), j);
            }

            if (tsd._multiplexer.IsSocketReadyForRead(msgfd))  // any signals from the other thread?
            {
               uint8 bytes[256];
               if (ConvertReturnValueToMuscleSemantics(recv_ignore_eintr(msgfd, (char *)bytes, sizeof(bytes), 0), sizeof(bytes), false) > 0) ret = WaitForNextMessageAux(tsd, ref, wakeupTime);
            }
         }
      }
   }
   return ret;
}

void Thread :: InternalThreadEntry()
{
#if defined(MUSCLE_USE_QT_THREADS) && defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
   // In this mode, our Thread will run Qt's event-loop instead of using our own while-loop.
   // That way users who wish to subclass the Thread class and then use QObjects in
   // the the internal thread can get the Qt-appropriate behaviors that they are looking for.
   MuscleQThreadSocketNotifier internalSocketNotifier(this, GetInternalThreadWakeupSocket().GetFileDescriptor(), QSocketNotifier::Read, NULL);
   (void) _thread.CallExec();
#else
   while(true)
   {
      MessageRef msgRef;
      const int32 numLeft = WaitForNextMessageFromOwner(msgRef);
      if ((numLeft >= 0)&&(MessageReceivedFromOwner(msgRef, numLeft).IsError())) break;
   }
#endif
}

#if defined(MUSCLE_USE_QT_THREADS) && defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
void Thread :: QtSocketReadReady(int /*sock*/)
{
   MessageRef msgRef;
   while(1)
   {
      const int32 numLeft = WaitForNextMessageFromOwner(msgRef, 0);  // 0 because we don't want to block here, this is a poll only
      if (numLeft >= 0)
      {
         if (MessageReceivedFromOwner(msgRef, numLeft).IsError())
         {
            // Oops, MessageReceivedFromOwner() wants us to exit!
            _thread.quit();
            break;
         }
      }
      else break;  // no more incoming Messages to process, for now
   }
}
#endif

status_t Thread :: MessageReceivedFromOwner(const MessageRef & ref, uint32)
{
   return ref() ? B_NO_ERROR : B_ERROR;
}

status_t Thread :: WaitForInternalThreadToExit()
{
   if (_threadRunning)
   {
      status_t ret;

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
# if !defined(MUSCLE_NO_EXCEPTIONS)
      try {
# endif
         _thread.join();
# if !defined(MUSCLE_NO_EXCEPTIONS)
      }
      catch(...) {return B_LOGIC_ERROR;}
# endif
#elif defined(MUSCLE_USE_PTHREADS)
      const int pret = pthread_join(_thread, NULL);
      if (pret != 0) ret = B_ERRNUM(pret);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
      (void) WaitForSingleObject(_thread, INFINITE);
      ::CloseHandle(_thread);  // Raymond Dahlberg's fix for handle-leak problem
#elif defined(MUSCLE_QT_HAS_THREADS)
      (void) _thread.wait();
#elif defined(__BEOS__) || defined(__HAIKU__)
      status_t junk;
      (void) wait_for_thread(_thread, &junk);
#elif defined(__ATHEOS__)
      (void) wait_for_thread(_thread);
#endif
      _threadRunning = false;
      CloseSockets();
      return ret;
   }
   else return B_BAD_OBJECT;
}

Queue<MessageRef> * Thread :: LockAndReturnMessageQueue()
{
   ThreadSpecificData & tsd = _threadData[MESSAGE_THREAD_INTERNAL];
   return (tsd._queueLock.Lock().IsOK()) ? &tsd._messages : NULL;
}

status_t Thread :: UnlockMessageQueue()
{
   return _threadData[MESSAGE_THREAD_INTERNAL]._queueLock.Unlock();
}

Queue<MessageRef> * Thread :: LockAndReturnReplyQueue()
{
   ThreadSpecificData & tsd = _threadData[MESSAGE_THREAD_OWNER];
   return (tsd._queueLock.Lock().IsOK()) ? &tsd._messages : NULL;
}

status_t Thread :: UnlockReplyQueue()
{
   return _threadData[MESSAGE_THREAD_OWNER]._queueLock.Unlock();
}

Hashtable<Thread::muscle_thread_key, Thread *> Thread::_curThreads;
Mutex Thread::_curThreadsMutex;

Thread * Thread :: GetCurrentThread()
{
   muscle_thread_key key = GetCurrentThreadKey();

   Thread * ret = NULL; 
   if (_curThreadsMutex.Lock().IsOK())
   {
      (void) _curThreads.Get(key, ret);
      _curThreadsMutex.Unlock();
   }
   return ret;
}

// This method is here to 'wrap' the internal thread's virtual method call with some standard setup/tear-down code of our own
void Thread::InternalThreadEntryAux()
{
   const uint32 threadStackBase = 0;  // only here so we can get its address below
   _threadStackBase = &threadStackBase;  // remember this stack location so GetCurrentStackUsage() can reference it later on

   muscle_thread_key curThreadKey = GetCurrentThreadKey();
   if (_curThreadsMutex.Lock().IsOK())
   {
      (void) _curThreads.Put(curThreadKey, this);
      _curThreadsMutex.Unlock();
   }

   status_t ret;
   if ((_threadPriority != PRIORITY_UNSPECIFIED)&&(SetThreadPriorityAux(_threadPriority).IsError(ret)))
   {
      LogTime(MUSCLE_LOG_ERROR, "Thread %p:  Unable to set thread priority to %i [%s]\n", this, _threadPriority, ret());
   }

   if (_threadData[MESSAGE_THREAD_OWNER]._messages.HasItems()) SignalOwner();
   InternalThreadEntry();
   _threadData[MESSAGE_THREAD_INTERNAL]._messageSocket.Reset();  // this will wake up the owner thread with EOF on socket

   if (_curThreadsMutex.Lock().IsOK())
   {
      (void) _curThreads.Remove(curThreadKey);
      _curThreadsMutex.Unlock();
   }

   _threadStackBase = NULL;
}

Thread::muscle_thread_key Thread :: GetCurrentThreadKey()
{
#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return std::this_thread::get_id();
#elif defined(MUSCLE_USE_PTHREADS)
   return pthread_self();
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   return GetCurrentThreadId();
#elif defined(MUSCLE_QT_HAS_THREADS)
   return QThread::currentThread();
#elif defined(__BEOS__) || defined(__HAIKU__) || defined(__ATHEOS__)
   return find_thread(NULL);
#else
   #error "Thread::GetCurrentThreadKey():  Unsupported platform?"
#endif
}

bool Thread :: IsCallerInternalThread() const
{
   if (IsInternalThreadRunning() == false) return false;  // we can't be him if he doesn't exist!

#if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return (std::this_thread::get_id() == _thread.get_id());
#elif defined(MUSCLE_USE_PTHREADS)
   return pthread_equal(pthread_self(), _thread);
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
   return (_threadID == GetCurrentThreadId());
#elif defined(MUSCLE_QT_HAS_THREADS)
   return (QThread::currentThread() == static_cast<const QThread *>(&_thread));
#elif defined(__BEOS__) || defined(__HAIKU__)
   return (_thread == find_thread(NULL));
#elif defined(__ATHEOS__)
   return (_thread == find_thread(NULL));
#else
   #error "Thread::IsCallerInternalThread():  Unsupported platform?"
#endif
}

uint32 Thread :: GetCurrentStackUsage() const
{
   if ((IsCallerInternalThread() == false)||(_threadStackBase == NULL)) return 0;

   const uint32 curStackPos = 0;
   const uint32 * curStackPtr = &curStackPos;
   return (uint32) muscleAbs(curStackPtr-_threadStackBase);
}

status_t Thread :: SetThreadPriority(int newPriority)
{
   if (IsInternalThreadRunning())
   {
      status_t ret;
      if (SetThreadPriorityAux(newPriority).IsOK(ret))
      {
         _threadPriority = newPriority;
         return B_NO_ERROR;
      }
      else return ret;
   }
   else
   {
      _threadPriority = newPriority;  // we'll actually try to change to that thread priority in the thread's own startup-sequence
      return B_NO_ERROR;
   }
}

#if defined(MUSCLE_USE_QT_THREADS)
static QThread::Priority MuscleThreadPriorityToQtThreadPriority(int muscleThreadPriority)
{
   switch(muscleThreadPriority)
   {
      case Thread::PRIORITY_IDLE:         return QThread::IdlePriority;
      case Thread::PRIORITY_LOWEST:       return QThread::LowestPriority;
      case Thread::PRIORITY_LOWER:        return QThread::LowPriority;
      case Thread::PRIORITY_LOW:          return QThread::LowPriority;
      case Thread::PRIORITY_NORMAL:       return QThread::NormalPriority;
      case Thread::PRIORITY_HIGH:         return QThread::HighPriority;
      case Thread::PRIORITY_HIGHER:       return QThread::HighPriority;
      case Thread::PRIORITY_HIGHEST:      return QThread::HighestPriority;
      case Thread::PRIORITY_TIMECRITICAL: return QThread::TimeCriticalPriority;
      default:                            return QThread::InheritPriority;
   }
}
#elif defined(WIN32)
static int MuscleThreadPriorityToWindowsThreadPriority(int muscleThreadPriority)
{
   switch(muscleThreadPriority)
   {
      case Thread::PRIORITY_IDLE:         return THREAD_PRIORITY_IDLE;
      case Thread::PRIORITY_LOWEST:       return THREAD_PRIORITY_LOWEST;
      case Thread::PRIORITY_LOWER:        return THREAD_PRIORITY_BELOW_NORMAL;
      case Thread::PRIORITY_LOW:          return THREAD_PRIORITY_BELOW_NORMAL;
      case Thread::PRIORITY_NORMAL:       return THREAD_PRIORITY_NORMAL;
      case Thread::PRIORITY_HIGH:         return THREAD_PRIORITY_ABOVE_NORMAL;
      case Thread::PRIORITY_HIGHER:       return THREAD_PRIORITY_ABOVE_NORMAL;
      case Thread::PRIORITY_HIGHEST:      return THREAD_PRIORITY_HIGHEST;
      case Thread::PRIORITY_TIMECRITICAL: return THREAD_PRIORITY_TIME_CRITICAL;
      default:                            return THREAD_PRIORITY_NORMAL;
   }
}
#endif

status_t Thread :: SetThreadPriorityAux(int newPriority)
{
   if (newPriority == PRIORITY_UNSPECIFIED) return B_NO_ERROR;  // sure, unspecified is easy, anything goes

#if defined(MUSCLE_USE_PTHREADS)
   int schedPolicy;
   sched_param param;
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   int pret = pthread_getschedparam(_thread.native_handle(), &schedPolicy, &param);
# else
   int pret = pthread_getschedparam(_thread, &schedPolicy, &param);
# endif
   if (pret != 0) return B_ERRNUM(pret);

   const int minPrio = sched_get_priority_min(schedPolicy);
   const int maxPrio = sched_get_priority_max(schedPolicy);
   if ((minPrio == -1)||(maxPrio == -1)) return B_UNIMPLEMENTED;

   param.sched_priority = muscleClamp(((newPriority*(maxPrio-minPrio))/(NUM_PRIORITIES-1))+minPrio, minPrio, maxPrio);
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return B_ERRNUM(pthread_setschedparam(_thread.native_handle(), schedPolicy, &param));
# else
   return B_ERRNUM(pthread_setschedparam(_thread, schedPolicy, &param));
# endif
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return ::SetThreadPriority(_thread.native_handle(), MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# else
   return ::SetThreadPriority(_thread, MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# endif
#elif defined(MUSCLE_USE_QT_THREADS)
   _thread.setPriority(MuscleThreadPriorityToQtThreadPriority(newPriority));
   return B_NO_ERROR;
#elif defined(WIN32)
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return ::SetThreadPriority(_thread.native_handle(), MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# else
   return ::SetThreadPriority(_thread, MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# endif
#else
   return B_UNIMPLEMENTED;  // dunno how to set thread priorities on this platform
#endif
}

void CheckThreadStackUsage(const char * fileName, uint32 line)
{
   Thread * curThread = Thread::GetCurrentThread();
   if (curThread)
   {
      const uint32 maxUsage = curThread->GetSuggestedStackSize();
      if (maxUsage != 0)  // if the Thread doesn't have a suggested stack size, then we don't know what the limit is
      {
         const uint32 curUsage = curThread->GetCurrentStackUsage();
         if (curUsage > maxUsage)
         {
            char buf[20];
            LogTime(MUSCLE_LOG_CRITICALERROR, "Thread %s exceeded its suggested stack usage (" UINT32_FORMAT_SPEC " > " UINT32_FORMAT_SPEC ") at (%s:" UINT32_FORMAT_SPEC "), aborting program!\n", muscle_thread_id::GetCurrentThreadID().ToString(buf), curUsage, maxUsage, fileName, line);
            MCRASH("MUSCLE Thread exceeded its suggested stack allowance");
         }
      }
   }
   else 
   {
      char buf[20];
      printf("Warning, CheckThreadStackUsage() called from non-MUSCLE thread %s at (%s:" UINT32_FORMAT_SPEC ")\n", muscle_thread_id::GetCurrentThreadID().ToString(buf), fileName, line);
   }
}

} // end namespace muscle
