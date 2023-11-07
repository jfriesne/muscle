/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(MUSCLE_USE_PTHREADS)
# include <pthread.h>  // in case both MUSCLE_USE_CPLUSPLUS11_THREADS and MUSCLE_USE_PTHREADS are defined at once, e.g. for better SetThreadPriority() support
#endif

#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"
#include "dataio/TCPSocketDataIO.h"  // to get the proper #includes for recv()'ing
#include "system/SetupSystem.h"      // for GetCurrentThreadID()

#if defined(MUSCLE_USE_QT_THREADS) && defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
# include "platform/qt/QMessageTransceiverThread.h"   // for MuscleQThreadSocketNotifier
#endif

#if defined(MUSCLE_PREFER_WIN32_OVER_QT)
# include <process.h>  // for _beginthreadex()
#endif

#ifdef MUSCLE_SINGLE_THREAD_ONLY
# error "You're not allowed use the Thread class if you have the MUSCLE_SINGLE_THREAD_ONLY compiler constant defined!"
#endif

namespace muscle {

Thread :: Thread(bool useMessagingSockets, ICallbackMechanism * optCallbackMechanism)
   : ICallbackSubscriber(optCallbackMechanism)
   , _useMessagingSockets(useMessagingSockets)
   , _messageSocketsAllocated(!useMessagingSockets)  // preset to true if we're not using sockets, to prevent us from demand-allocating them
   , _threadRunning(false)
   , _suggestedStackSize(0)
   , _threadStackBase(NULL)
   , _threadPriority(PRIORITY_UNSPECIFIED)
{
#if defined(MUSCLE_USE_QT_THREADS)
   _thread.SetOwner(this);
#endif

   if (_useMessagingSockets == false)
   {
      // make sure the WaitConditions get constructed now, to avoid any race conditions in constructing them later
      for (uint32 i=0; i<ARRAYITEMS(_threadData); i++) _threadData[i]._waitCondition.EnsureObjectConstructed();
   }
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
   if (_messageSocketsAllocated == false)
   {
      if (CreateConnectedSocketPair(_threadData[MESSAGE_THREAD_INTERNAL]._messageSocket, _threadData[MESSAGE_THREAD_OWNER]._messageSocket).IsError()) return GetNullSocket();
      _messageSocketsAllocated = true;
   }
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
      MRETURN_ON_ERROR(StartInternalThreadAux());
      if (needsInitialSignal) SignalInternalThread();  // make sure he gets his already-queued messages!
      return B_NO_ERROR;
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
#else
   #error "Thread::StartInternalThreadAuxAux():  Unsupported platform?"
#endif
}

void Thread :: ShutdownInternalThread(bool waitForThread)
{
   if (IsInternalThreadRunning())
   {
      (void) SendMessageToInternalThread(MessageRef());  // a NULL message ref tells him to quit
      if (waitForThread) (void) WaitForInternalThreadToExit();
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
   ThreadSpecificData & tsd = _threadData[whichQueue];
   MRETURN_ON_ERROR(tsd._queueLock.Lock());

   status_t ret;
   const bool sendNotification = ((tsd._messages.AddTail(replyRef).IsOK(ret))&&(tsd._messages.GetNumItems() == 1));  // don't reorder this!  AddTail() has to be first!
   (void) tsd._queueLock.Unlock();

   if (sendNotification)
   {
      switch(whichQueue)
      {
         case MESSAGE_THREAD_INTERNAL: SignalInternalThread(); break;
         case MESSAGE_THREAD_OWNER:    SignalOwner();          break;
      }
   }
   return B_NO_ERROR;
}

void Thread :: SignalInternalThread()
{
   SignalAux(MESSAGE_THREAD_OWNER);  // we send a byte on the owner's socket and the byte comes out on the internal socket
}

void Thread :: SignalOwner()
{
   SignalAux(MESSAGE_THREAD_INTERNAL);  // we send a byte on the internal socket and the byte comes out on the owner's socket
   RequestCallbackInDispatchThread();
}

void Thread :: SignalAux(int whichSocket)
{
   if (_useMessagingSockets)
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
   else (void) _threadData[(whichSocket==MESSAGE_THREAD_OWNER)?MESSAGE_THREAD_INTERNAL:MESSAGE_THREAD_OWNER]._waitCondition.GetObject().Notify();
}

// Called in the main thread by the ICallbackMechanism, if there is one
void Thread :: DispatchCallbacks(uint32 /*eventTypeBits*/)
{
   MessageRef ref;
   uint32 numLeft = 0;
   while(GetNextReplyFromInternalThread(ref, 0, &numLeft).IsOK()) MessageReceivedFromInternalThread(ref, numLeft);
}

// default implementation is a no-op
void Thread :: MessageReceivedFromInternalThread(const MessageRef & ref, uint32 numLeft)
{
   (void) ref;
   (void) numLeft;
}

status_t Thread :: GetNextReplyFromInternalThread(MessageRef & ref, uint64 wakeupTime, uint32 * optRetNumMessagesLeftInQueue)
{
   return WaitForNextMessageAux(_threadData[MESSAGE_THREAD_OWNER],    ref, wakeupTime, optRetNumMessagesLeftInQueue);
}

status_t Thread :: WaitForNextMessageFromOwner(MessageRef & ref, uint64 wakeupTime, uint32 * optRetNumMessagesLeftInQueue)
{
   return WaitForNextMessageAux(_threadData[MESSAGE_THREAD_INTERNAL], ref, wakeupTime, optRetNumMessagesLeftInQueue);
}

status_t Thread :: WaitForNextMessageAux(ThreadSpecificData & tsd, MessageRef & ref, uint64 wakeupTime, uint32 * optRetNumMessagesLeftInQueue)
{
   if (optRetNumMessagesLeftInQueue) *optRetNumMessagesLeftInQueue = 0;

   if (_useMessagingSockets)
   {
      // Be sure to always absorb any signal-bytes that were sent to us, now that we've been awoken,
      // otherwise we could end up in a CPU-burning loop with select() always returning immediately
      uint8 bytes[256];
      (void) recv_ignore_eintr(tsd._messageSocket.GetFileDescriptor(), (char *)bytes, sizeof(bytes), 0);
   }

   if (tsd._queueLock.Lock().IsError()) return B_LOCK_FAILED;
   else
   {
      const status_t ret = tsd._messages.RemoveHead(ref);
      if (optRetNumMessagesLeftInQueue) *optRetNumMessagesLeftInQueue = tsd._messages.GetNumItems();
      (void) tsd._queueLock.Unlock();

      if (ret.IsOK())      return ret;
      if (wakeupTime == 0) return B_TIMED_OUT;
   }

   // If we got here, no Message was available, so we'll have to wait until there is one (or until wakeupTime)
   if (_useMessagingSockets)
   {
      const int msgfd = tsd._messageSocket.GetFileDescriptor();
      if (msgfd < 0) return B_BAD_OBJECT;  // semi-paranoia

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
               const int nextFD = iter.GetKey().GetFileDescriptor();
               if (nextFD >= 0) (void) tsd._multiplexer.RegisterSocketForEventsByTypeIndex(nextFD, i);
            }
         }
      }
      (void) tsd._multiplexer.RegisterSocketForReadReady(msgfd);

      MRETURN_ON_ERROR(tsd._multiplexer.WaitForEvents(wakeupTime));

      status_t ret = B_TIMED_OUT;
      for (uint32 j=0; j<ARRAYITEMS(tsd._socketSets); j++)
      {
         Hashtable<ConstSocketRef, bool> & t = tsd._socketSets[j];
         if (t.HasItems())
         {
            for (HashtableIterator<ConstSocketRef, bool> iter(t, HTIT_FLAG_NOREGISTER); iter.HasData(); iter++)
            {
               const bool isFlagged = iter.GetValue() = tsd._multiplexer.IsSocketEventOfTypeFlagged(iter.GetKey().GetFileDescriptor(), j);
               if (isFlagged) ret = B_IO_READY;
            }
         }
      }

      return tsd._multiplexer.IsSocketReadyForRead(msgfd) ? WaitForNextMessageAux(tsd, ref, 0, optRetNumMessagesLeftInQueue) : ret;
   }
   else
   {
      MRETURN_ON_ERROR(tsd._waitCondition.GetObject().Wait(wakeupTime));
      return WaitForNextMessageAux(tsd, ref, 0, optRetNumMessagesLeftInQueue);
   }
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
      uint32 numLeft = 0;
      if ((WaitForNextMessageFromOwner(msgRef).IsOK())&&(MessageReceivedFromOwner(msgRef, numLeft).IsError())) break;
   }
#endif
}

#if defined(MUSCLE_USE_QT_THREADS) && defined(MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION)
void Thread :: QtSocketReadReady(int /*sock*/)
{
   MessageRef msgRef;
   while(1)
   {
      if (WaitForNextMessageFromOwner(msgRef, 0).IsOK())  // 0 because we don't want to block here, this is a poll only
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
   return ref() ? B_NO_ERROR : B_SHUTTING_DOWN;
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
      (void) _curThreadsMutex.Unlock();
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
      (void) _curThreadsMutex.Unlock();
   }

   status_t ret;
   if ((_threadPriority != PRIORITY_UNSPECIFIED)&&(SetThreadPriorityAux(_threadPriority, true).IsError(ret)))  // true is hard-coded here to avoid race conditions with _thread.native_handle()
   {
      LogTime(MUSCLE_LOG_ERROR, "Thread %p:  Unable to set thread priority to %i [%s]\n", this, _threadPriority, ret());
   }

   if (_threadData[MESSAGE_THREAD_OWNER]._messages.HasItems()) SignalOwner();
   InternalThreadEntry();
   _threadData[MESSAGE_THREAD_INTERNAL]._messageSocket.Reset();  // this will wake up the owner thread with EOF on socket

   if (_curThreadsMutex.Lock().IsOK())
   {
      (void) _curThreads.Remove(curThreadKey);
      (void) _curThreadsMutex.Unlock();
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
   if (IsInternalThreadRunning()) MRETURN_ON_ERROR(SetThreadPriorityAux(newPriority, IsCallerInternalThread()));
   _threadPriority = newPriority;  // just remember the setting for now; when we actually launch the thread we'll set the thread's priority
   return B_NO_ERROR;
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

status_t Thread :: SetThreadPriorityAux(int newPriority, bool calledFromInternalThread)
{
   if (newPriority == PRIORITY_UNSPECIFIED) return B_NO_ERROR;  // sure, unspecified is easy, anything goes

   (void) calledFromInternalThread;  // just to inhibit compiler warnings

#if defined(MUSCLE_USE_PTHREADS)
   int schedPolicy;
   sched_param param;
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   const int pret = pthread_getschedparam(calledFromInternalThread ? pthread_self() : _thread.native_handle(), &schedPolicy, &param);
# else
   const int pret = pthread_getschedparam(calledFromInternalThread ? pthread_self() : _thread,                 &schedPolicy, &param);
# endif
   if (pret != 0) return B_ERRNUM(pret);

   const int minPrio = sched_get_priority_min(schedPolicy);
   const int maxPrio = sched_get_priority_max(schedPolicy);
   if ((minPrio == -1)||(maxPrio == -1)) return B_UNIMPLEMENTED;

   param.sched_priority = muscleClamp(((newPriority*(maxPrio-minPrio))/(NUM_PRIORITIES-1))+minPrio, minPrio, maxPrio);
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return B_ERRNUM(pthread_setschedparam(calledFromInternalThread ? pthread_self() : _thread.native_handle(), schedPolicy, &param));
# else
   return B_ERRNUM(pthread_setschedparam(calledFromInternalThread ? pthread_self() : _thread,                 schedPolicy, &param));
# endif
#elif defined(MUSCLE_PREFER_WIN32_OVER_QT)
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return ::SetThreadPriority(calledFromInternalThread ? ::GetCurrentThread() : _thread.native_handle(), MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# else
   return ::SetThreadPriority(calledFromInternalThread ? ::GetCurrentThread() : _thread,                 MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# endif
#elif defined(MUSCLE_USE_QT_THREADS)
   _thread.setPriority(MuscleThreadPriorityToQtThreadPriority(newPriority));
   return B_NO_ERROR;
#elif defined(WIN32)
# if defined(MUSCLE_USE_CPLUSPLUS11_THREADS)
   return ::SetThreadPriority(calledFromInternalThread ? ::GetCurrentThread() :_thread.native_handle(), MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
# else
   return ::SetThreadPriority(calledFromInternalThread ? ::GetCurrentThread() :_thread,                 MuscleThreadPriorityToWindowsThreadPriority(newPriority)) ? B_NO_ERROR : B_ERRNO;
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
