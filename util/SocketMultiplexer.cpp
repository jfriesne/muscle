/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#if defined(MUSCLE_USE_POLL) || defined(MUSCLE_USE_EPOLL)
# include <limits.h>  // for INT_MAX
#endif

#include "util/SocketMultiplexer.h"

namespace muscle {

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
static Mutex _multiplexersListMutex;
static SocketMultiplexer * _headMultiplexer = NULL;
void NotifySocketMultiplexersThatSocketIsClosed(int fd)
{
   if (_multiplexersListMutex.Lock() == B_NO_ERROR)
   {
      SocketMultiplexer * sm = _headMultiplexer;
      while(sm)
      {
         sm->NotifySocketClosed(fd);
         sm = sm->_nextMultiplexer;
      }
      _multiplexersListMutex.Unlock();
   }
}
#endif

SocketMultiplexer :: SocketMultiplexer()
#if !defined(MUSCLE_USE_KQUEUE) && !defined(MUSCLE_USE_EPOLL)
  : _curFDState(0)
#endif
{
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
   // Prepend this object to the global linked list of SocketMultiplexers
   MutexGuard mg(_multiplexersListMutex);
   _prevMultiplexer = NULL;
   _nextMultiplexer = _headMultiplexer;
   if (_headMultiplexer) _headMultiplexer->_prevMultiplexer = this;
   _headMultiplexer = this;
#endif
}

SocketMultiplexer :: ~SocketMultiplexer()
{
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
   // Unregister this object from the global linked list of SocketMultiplexers
   MutexGuard mg(_multiplexersListMutex);
   if (_headMultiplexer == this) _headMultiplexer = _nextMultiplexer;
   if (_prevMultiplexer) _prevMultiplexer->_nextMultiplexer = _nextMultiplexer;
   if (_nextMultiplexer) _nextMultiplexer->_prevMultiplexer = _prevMultiplexer;
#endif
}

int SocketMultiplexer :: WaitForEvents(uint64 optTimeoutAtTime)
{
   int ret = GetCurrentFDState().WaitForEvents(optTimeoutAtTime);
#if !defined(MUSCLE_USE_KQUEUE) && !defined(MUSCLE_USE_EPOLL)
   _curFDState = _curFDState?0:1;
#endif
   GetCurrentFDState().Reset();
   return ret;
}

int SocketMultiplexer :: FDState :: WaitForEvents(uint64 optTimeoutAtTime)
{
   // Calculate how long we should wait before timing out
   uint64 waitTimeMicros;
   if (optTimeoutAtTime == MUSCLE_TIME_NEVER) waitTimeMicros = MUSCLE_TIME_NEVER;
   else
   {
      uint64 now = GetRunTime64();
      waitTimeMicros = (optTimeoutAtTime>now) ? (optTimeoutAtTime-now) : 0;
   }

#if defined(MUSCLE_USE_SELECT)
   int maxFD = -1;
   fd_set * sets[NUM_FDSTATE_SETS];
   for (uint32 i=0; i<NUM_FDSTATE_SETS; i++)
   {
      if (_maxFD[i] >= 0)
      {
         maxFD = muscleMax(maxFD, _maxFD[i]);
         sets[i] = &_fdSets[i];
      }
      else sets[i] = NULL;
   }
#endif

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
   if (1)   // for kqueue/epoll we need to always do the full check, or our state-bits might not get updated properly
#elif defined(MUSCLE_USE_POLL)
   if (_pollFDArray.HasItems())
#else
   if (maxFD >= 0)
#endif
   {
#if defined(MUSCLE_USE_KQUEUE)
      if (ComputeStateBitsChangeRequests() != B_NO_ERROR) return B_ERROR;

      struct timespec waitTime;
      struct timespec * pWaitTime;
      if (waitTimeMicros == MUSCLE_TIME_NEVER) pWaitTime = NULL;
      else
      {
         waitTime.tv_sec  = MicrosToSeconds(waitTimeMicros);
         waitTime.tv_nsec = MicrosToNanos(waitTimeMicros%MICROS_PER_SECOND);
         pWaitTime = &waitTime;
      }

      int ret = kevent(_kernelFD, _scratchChanges.HeadPointer(), _scratchChanges.GetNumItems(), _scratchEvents.HeadPointer(), _scratchEvents.GetNumItems(), pWaitTime);
      if (ret >= 0)
      {
         // Record that kevent() accepted our _scratchChanges into its kernel-state, so we'll know not to send the changes again next time
         for (HashtableIterator<int, uint16> iter(_bits); iter.HasData(); iter++)
         {
            uint16 & b = iter.GetValue();
            uint16 userBits = (b&0x0F);
            if (userBits) b = (userBits<<4);                   // post-kevent() state is:  bits registered in-kernel but not in-userland or in-results
                     else (void) _bits.Remove(iter.GetKey());  // this FD isn't being monitored anymore, so we can forget about it
         }

         // Now go through our _scratchEvents list and set bits for any flagged events, for quick lookup by the user
         for (int i=0; i<ret; i++)
         {
            const struct kevent & event = _scratchEvents[i];
            uint16 * bits = _bits.Get(event.ident);
            if (bits)
            {
               switch(event.filter)
               {
                  case EVFILT_READ:  *bits |= (1<<(FDSTATE_SET_READ +8)); break;  // +8 because this bit goes into the results-nybble
                  case EVFILT_WRITE: *bits |= (1<<(FDSTATE_SET_WRITE+8)); break;  // ditto
               }
            }
         }
      }
#elif defined(MUSCLE_USE_EPOLL)
      if (ComputeStateBitsChangeRequests() != B_NO_ERROR) return B_ERROR;

      int ret = epoll_wait(_kernelFD, _scratchEvents.HeadPointer(), _scratchEvents.GetNumItems(), (waitTimeMicros==MUSCLE_TIME_NEVER)?-1:(int)(muscleMin(MicrosToMillis(waitTimeMicros), (int64)INT_MAX)));
      if (ret >= 0)
      {
         // Now go through our _scratchEvents list and set bits for any flagged events, for quick lookup by the user
         for (int i=0; i<ret; i++)
         {
            const struct epoll_event & event = _scratchEvents[i];
            uint16 * bits = _bits.Get(event.data.fd);
            if (bits)
            {
               if (event.events & (EPOLLIN|EPOLLHUP|EPOLLRDHUP)) *bits |= (1<<(FDSTATE_SET_READ  +8)); // +8 because this bit goes into the results-nybble
               if (event.events & (EPOLLOUT|EPOLLHUP))           *bits |= (1<<(FDSTATE_SET_WRITE +8)); // ditto
               if (event.events & (EPOLLERR))                    *bits |= (1<<(FDSTATE_SET_EXCEPT+8)); // ditto
            }
         }
      }
#elif defined(MUSCLE_USE_POLL)
      int timeoutMillis = (waitTimeMicros == MUSCLE_TIME_NEVER) ? -1 : ((int) muscleMin(MicrosToMillis(waitTimeMicros), (int64)(INT_MAX)));
# ifdef WIN32
      int ret = WSAPoll(_pollFDArray.GetItemAt(0), _pollFDArray.GetNumItems(), timeoutMillis);
# else
      int ret = poll(   _pollFDArray.GetItemAt(0), _pollFDArray.GetNumItems(), timeoutMillis);
# endif
#else
      struct timeval waitTime;
      struct timeval * pWaitTime;
      if (waitTimeMicros == MUSCLE_TIME_NEVER) pWaitTime = NULL;
      else
      {
         Convert64ToTimeVal(waitTimeMicros, waitTime);
         pWaitTime = &waitTime;
      }
      int ret = select(maxFD+1, sets[0], sets[1], sets[2], pWaitTime);
#endif
      if ((ret < 0)&&(PreviousOperationWasInterrupted())) ret = 0;  // on interruption we'll just go round gain
      return ret;
   }
   else
   {
      // Hmm, no sockets to wait on at all.  All we have to do is sleep!
      // Note doing it this way avoids a Windows problem where select() will fail if no sockets are provided to wait on.
      Snooze64(waitTimeMicros);
      return 0;
   }
}

void SocketMultiplexer :: FDState :: Reset()
{
#if !defined(MUSCLE_USE_KQUEUE) && !defined(MUSCLE_USE_EPOLL)
# if defined(MUSCLE_USE_POLL)
   _pollFDArray.FastClear();
   _pollFDToArrayIndex.Clear();
# else
   for (uint32 i=0; i<NUM_FDSTATE_SETS; i++)
   {
      _maxFD[i] = -1;
      FD_ZERO(&_fdSets[i]);
   }
# endif
#endif
}

#ifdef MUSCLE_USE_POLL
status_t SocketMultiplexer :: FDState :: PollRegisterNewSocket(int fd, uint32 whichSet)
{
   struct pollfd * p = _pollFDArray.AddTailAndGet();
   if (p)
   {
      p->fd      = fd;
      p->events  = GetPollBitsForFDSet(whichSet, true);
      p->revents = 0;
      if (_pollFDToArrayIndex.Put(fd, _pollFDArray.GetNumItems()-1) == B_NO_ERROR) return B_NO_ERROR;
                                                                              else _pollFDArray.RemoveTail();  // roll back!
   }
   return B_ERROR;
}
#endif

SocketMultiplexer :: FDState :: FDState()
{
#if defined(MUSCLE_USE_KQUEUE)
   _kernelFD = kqueue();
   if (_kernelFD < 0) printf("FDState:  Error, couldn't allocate a kqueue!\n");
#elif defined(MUSCLE_USE_EPOLL)
   _kernelFD = epoll_create(1024);  // note that this argument is ignored in modern unix, it just has to be greater than zero
   if (_kernelFD < 0) printf("FDState:  Error, epoll_create() failed!\n");
#endif

   Reset();
}

SocketMultiplexer :: FDState :: ~FDState()
{
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
   if (_kernelFD >= 0) close(_kernelFD);
#endif
}

#ifdef MUSCLE_USE_KQUEUE
status_t SocketMultiplexer :: FDState :: AddKQueueChangeRequest(int fd, uint32 whichSet, bool add)
{
   int16 filter;
   switch(whichSet)
   {
      case FDSTATE_SET_READ:   filter = EVFILT_READ;  break;
      case FDSTATE_SET_WRITE:  filter = EVFILT_WRITE; break;
      case FDSTATE_SET_EXCEPT: return B_NO_ERROR;  // not sure how kqueue handles exception-sets:  nerf them for now, since MUSCLE only uses them under Windows anyway
      default:                 return B_ERROR;     // bad set type!?
   }

   struct kevent * kevt = _scratchChanges.AddTailAndGet();
   if (kevt == NULL) return B_ERROR;  // wtf?

   EV_SET(kevt, fd, filter, add?EV_ADD:EV_DELETE, 0, 0, NULL);
   return B_NO_ERROR;
}
#endif

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
status_t SocketMultiplexer :: FDState :: ComputeStateBitsChangeRequests()
{
#if defined(MUSCLE_USE_KQUEUE)
   _scratchChanges.FastClear();
   (void) _scratchChanges.EnsureSize(GetMaxNumEvents());  // try to avoid multiple reallocs if possible
#endif

   // If any of our sockets were closed since the last call, we need to make sure to note that the kernel is no longer
   // tracking them.  Otherwise we can run into this problem:
   //   http://stackoverflow.com/questions/8608931/is-there-any-way-to-tell-that-a-file-descriptor-value-has-been-reused
   {
      // This scope is present so that _closedSocketMutex won't be locked while we iterate
      {
         MutexGuard mg(_closedSocketsMutex);
         if (_closedSockets.HasItems()) _scratchClosedSockets.SwapContents(_closedSockets);
      }
      if (_scratchClosedSockets.HasItems())
      {
         for (HashtableIterator<int, Void> iter(_scratchClosedSockets); iter.HasData(); iter++)
         {
            uint16 * bits = _bits.Get(iter.GetKey());
            if (bits)
            {
               uint16 & b = *bits;
               b &= 0x0F;  // Remove all bits except for the userland-registration-bits, to force a kernel re-registration below
               if (b == 0) _bits.Remove(iter.GetKey());  // No userland-registration-bits either?  Then we can discard the record
            }
         }
         _scratchClosedSockets.Clear();
      }
   }

   // Generate change requests to the kernel, based on how the userBits differ from the kernelBits
   for (HashtableIterator<int, uint16> iter(_bits); iter.HasData(); iter++)
   {
      uint16 & bits  = iter.GetValue();
      bits &= ~(0xF00);  // get rid of any leftover results-bits from the previous iteration
      uint8 userBits = ((bits>>0)&0x0F);
      uint8 kernBits = ((bits>>4)&0x0F);
      if (userBits != kernBits)
      {
#if defined(MUSCLE_USE_KQUEUE)
         for (uint32 i=0; i<NUM_FDSTATE_SETS; i++)
         {
            const bool hasBit = ((userBits&(1<<i)) != 0);
            const bool hadBit = ((kernBits&(1<<i)) != 0);
            if ((hasBit != hadBit)&&(AddKQueueChangeRequest(iter.GetKey(), i, hasBit) != B_NO_ERROR)) return B_ERROR;
         }
#else
         struct epoll_event evt; memset(&evt, 0, sizeof(evt));  // paranoia
         evt.data.fd = iter.GetKey();
         if (userBits & (1<<FDSTATE_SET_READ))   evt.events |= EPOLLIN;
         if (userBits & (1<<FDSTATE_SET_WRITE))  evt.events |= EPOLLOUT;
         if (userBits & (1<<FDSTATE_SET_EXCEPT)) evt.events |= EPOLLERR;
         int op = ((userBits==0)&&(kernBits != 0)) ? EPOLL_CTL_DEL : (((userBits!=0)&&(kernBits==0)) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD);
         if ((epoll_ctl(_kernelFD, op, iter.GetKey(), &evt) != 0)&&(op != EPOLL_CTL_DEL)) return B_ERROR;  // DEL may fail if fd was already closed, that's okay
#endif
      }

#if defined(MUSCLE_USE_EPOLL)
      bits = (((uint16)userBits)<<4);   // for epoll() we update the bits now, since epoll_ctrl() succeeded already
#endif
   }

   return _scratchEvents.EnsureSize(GetMaxNumEvents(), true);  // try to ensure we have plenty of room for whatever events epoll_wait() will want to return.
}

#endif

}; // end namespace muscle
