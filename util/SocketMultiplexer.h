/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSocketMultiplexer_h
#define MuscleSocketMultiplexer_h

#include "util/NetworkUtilityFunctions.h"

#if defined(MUSCLE_USE_KQUEUE)
# include <sys/event.h>
# include "system/Mutex.h"
#elif defined(MUSCLE_USE_EPOLL)
# include <sys/epoll.h>
# include "system/Mutex.h"
#elif defined(MUSCLE_USE_POLL)
# ifdef WIN32
#  if !defined(_WIN32_WINNT) || (_WIN32_WINNT < 0x0600)
#   error "Compiling with MUSCLE_USE_POLL under Windows requires that _WIN32_WINNT be defined as 0x600 (aka Vista) or higher!"
#  endif
# else
#  include <poll.h>
# endif
#else
# ifndef MUSCLE_USE_SELECT
#  define MUSCLE_USE_SELECT 1  // we use select() by default if none of the above MUSCLE_USE_* compiler flags were defined
# endif
#endif

#ifndef MUSCLE_USE_SELECT
# include "util/Hashtable.h"
# include "util/Queue.h"
#endif

namespace muscle {

/** This class allows a thread to wait until one or more of a set of
 *  file descriptors is ready for use, or until a specified time is
 *  reached, whichever comes first.
 *
 *  This class has underlying implementations available for various OS
 *  mechanisms.  The default implementation uses select(), since that
 *  mechanism is the most widely portable.  However, you can force this class
 *  to use poll(), epoll(), or kqueue() instead, by specifying the compiler
 *  flag -DMUSCLE_USE_POLL, -DMUSCLE_USE_EPOLL, or -DMUSCLE_USE_KQUEUE
 *  (respectively) on the compile line.
 */
class SocketMultiplexer
{
public:
   /** Constructor. */
   SocketMultiplexer();

   /** Destructor. */
   ~SocketMultiplexer();

   /** Call this to indicate that you want the next call to Wait() to return if the specified
     * socket has data ready to read.
     * @note this registration is cleared after WaitForEvents() returns, so you will generally want to re-register
     *       your socket on each iteration of your event loop.
     * @param fd The file descriptor to watch for data-ready-to-read.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory or bad fd value?)
     */
   inline status_t RegisterSocketForReadReady(int fd) {return GetCurrentFDState().RegisterSocket(fd, FDSTATE_SET_READ);}

   /** Call this to indicate that you want the next call to WaitForEvents() to return if the specified
     * socket has buffer space available to write to.
     * @note this registration is cleared after WaitForEvents() returns, so you will generally want to re-register
     *       your socket on each iteration of your event loop.
     * @param fd The file descriptor to watch for space-available-to-write.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory or bad fd value?)
     */
   inline status_t RegisterSocketForWriteReady(int fd) {return GetCurrentFDState().RegisterSocket(fd, FDSTATE_SET_WRITE);}

   /** Call this to indicate that you want the next call to WaitForEvents() to return if the specified
     * socket has buffer space available to write to.
     * @note this registration is cleared after WaitForEvents() returns, so you will generally want to re-register
     *       your socket on each iteration of your event loop.
     * @param fd The file descriptor to watch for space-available-to-write.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory or bad fd value?)
     */
   inline status_t RegisterSocketForExceptionRaised(int fd) {return GetCurrentFDState().RegisterSocket(fd, FDSTATE_SET_EXCEPT);}

   /** This method is equivalent to any of the three other Register methods, except that in this method
     * you can specify the set via a FDSTATE_SET_* value.
     * @note this registration is cleared after WaitForEvents() returns, so you will generally want to re-register
     *       your socket on each iteration of your event loop.
     * @param fd The file descriptor to watch for the event type specified by (whichSet)
     * @param whichSet A FDSTATE_SET_* value indicating the type of event to watch the socket for.
     * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory or bad fd value?)
     */
   inline status_t RegisterSocketForEventsByTypeIndex(int fd, uint32 whichSet) {return GetCurrentFDState().RegisterSocket(fd, whichSet);}

   /** Blocks until at least one of the events specified in previous RegisterSocketFor*()
     * calls becomes valid, or for (optMaxWaitTimeMicros) microseconds, whichever comes first.
     * @note All socket-registrations will be cleared after this method call returns.  You will typically
     *       want to re-register them (by calling RegisterSocketFor*() again) before calling WaitForEvents() again.
     * @param timeoutAtTime The time to return 0 at if no events occur before then, or MUSCLE_TIME_NEVER
     *                      if not timeout is desired.  Uses the sameDefaults to MUSCLE_TIME_NEVER.
     *                      Specifying 0 (or any other value not greater than the current value returned
     *                      by GetRunTime64()) will effect a poll, guaranteed to return immediately.
     * @returns The number of socket-registrations that indicated that they are currently ready,
     *          or 0 if the timeout period elapsed without any events happening, or -1 if an error occurred.
     */
   int WaitForEvents(uint64 timeoutAtTime = MUSCLE_TIME_NEVER);

   /** Call this after WaitForEvents() returns, to find out if the specified file descriptor has
     * data ready to read or not.
     * @param fd The file descriptor to inquire about.  Note that (fd) must have been previously
     *           registered via RegisterSocketForReadyReady() for this method to work correctly.
     * @returns true if (fd) has data ready for reading, or false if it does not.
     */
   inline bool IsSocketReadyForRead(int fd) const {return GetAlternateFDState().IsSocketReady(fd, FDSTATE_SET_READ);}

   /** Call this after WaitForEvents() returns, to find out if the specified file descriptor has
     * buffer space available to write to, or not.
     * @param fd The file descriptor to inquire about.  Note that (fd) must have been previously
     *           registered via RegisterSocketForWriteReady() for this method to work correctly.
     * @returns true if (fd) has buffer space available for writing to, or false if it does not.
     */
   inline bool IsSocketReadyForWrite(int fd) const {return GetAlternateFDState().IsSocketReady(fd, FDSTATE_SET_WRITE);}

   /** Call this after WaitForEvents() returns, to find out if the specified file descriptor has
     * an exception state raised, or not.
     * @param fd The file descriptor to inquire about.  Note that (fd) must have been previously
     *           registered via RegisterSocketForExceptionRaised() for this method to work correctly.
     * @returns true if (fd) has an exception state raised, or false if it does not.
     */
   inline bool IsSocketExceptionRaised(int fd) const {return GetAlternateFDState().IsSocketReady(fd, FDSTATE_SET_EXCEPT);}

   /** Call this after WaitForEvents() returns, to find out if the specified file descriptor has
     * an event of the specified type flagged, or not.  Note that this method can be used as an
     * equivalent to any of the previous three methods.
     * @param fd The file descriptor to inquire about.  Note that (fd) must have been previously
     *           registered via the appropriate RegisterSocketFor*() call for this method to work correctly.
     * @param whichSet A FDSTATE_SET_* value indicating the type of event to query the socket for.
     * @returns true if (fd) has the specified event-type flagged, or false if it does not.
     */
   inline bool IsSocketEventOfTypeFlagged(int fd, uint32 whichSet) const {return GetAlternateFDState().IsSocketReady(fd, whichSet);}

   enum {
      FDSTATE_SET_READ = 0,
      FDSTATE_SET_WRITE,
      FDSTATE_SET_EXCEPT,
      NUM_FDSTATE_SETS
   };

private:
   class FDState
   {
   public:
      FDState();
      ~FDState();

      void Reset();

      inline status_t RegisterSocket(int fd, int whichSet)
      {
         if (fd < 0) return B_ERROR;

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
         uint16 * b = _bits.GetOrPut(fd);
         if (b == NULL) return B_ERROR;
         *b |= (1<<whichSet);
#elif defined(MUSCLE_USE_POLL)
         uint32 idx;
         if (_pollFDToArrayIndex.Get(fd, idx) == B_NO_ERROR) _pollFDArray[idx].events |= GetPollBitsForFDSet(whichSet, true);
                                                        else return PollRegisterNewSocket(fd, whichSet);
#else
# ifndef WIN32  // Window supports file descriptors that are greater than FD_SETSIZE!  Other OS's do not
         if (fd >= FD_SETSIZE) return B_ERROR;
# endif
         FD_SET(fd, &_fdSets[whichSet]);
         _maxFD[whichSet] = muscleMax(_maxFD[whichSet], fd);
#endif
         return B_NO_ERROR;
      }

      inline bool IsSocketReady(int fd, int whichSet) const
      {
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
         return ((_bits.GetWithDefault(fd) & (1<<(whichSet+8))) != 0);
#elif defined(MUSCLE_USE_POLL)
         uint32 idx;
         return ((_pollFDToArrayIndex.Get(fd, idx) == B_NO_ERROR)&&((_pollFDArray[idx].revents & GetPollBitsForFDSet(whichSet, false)) != 0));
#else
         return (FD_ISSET(fd, const_cast<fd_set *>(&_fdSets[whichSet])) != 0);
#endif
      }
      int WaitForEvents(uint64 timeoutAtTime);

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
      void NotifySocketClosed(int fd)
      {
         MutexGuard mg(_closedSocketsMutex);
         (void) _closedSockets.PutWithDefault(fd);
      }
#endif

   private:
#if defined(MUSCLE_USE_KQUEUE)
      status_t AddKQueueChangeRequest(int fd, uint32 whichSet, bool add);
#endif
#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
      status_t ComputeStateBitsChangeRequests();
      uint32 GetMaxNumEvents() const {return _bits.GetNumItems()*2;}  // times two since each FD could have both read and write events

      Mutex _closedSocketsMutex;  // necessary since NotifySocketClosed() might get called from any thread
      Hashtable<int, Void> _closedSockets;  // written to by NotifySocketClosed(), read-and-cleared by WaitForEvents(), protected by _closedSocketsMutex
      Hashtable<int, Void> _scratchClosedSockets;  // Used for double-buffering purposes

      int _kernelFD;
      Hashtable<int, uint16> _bits;   // fd -> (nybble #0 for userland registrations, nybble #1 for kernel-state, nybble #2 for results)
# if defined(MUSCLE_USE_KQUEUE)
      Queue<struct kevent> _scratchChanges;
      Queue<struct kevent> _scratchEvents;
# else
      Queue<struct epoll_event> _scratchEvents;
# endif
#elif defined(MUSCLE_USE_POLL)
      short GetPollBitsForFDSet(uint32 whichSet, bool isRegister) const
      {
         switch(whichSet)
         {
            case FDSTATE_SET_READ:   return POLLIN |(isRegister?0:POLLHUP);
            case FDSTATE_SET_WRITE:  return POLLOUT|(isRegister?0:POLLHUP);
#ifdef WIN32
            case FDSTATE_SET_EXCEPT: return (isRegister?0:POLLERR);  // WSAPoll() won't accept POLLERR as an events flag... see discussion at http://daniel.haxx.se/blog/2012/10/10/wsapoll-is-broken/
#else
            case FDSTATE_SET_EXCEPT: return POLLERR;
#endif
            default:                 return 0;
         }
      }
      status_t PollRegisterNewSocket(int fd, uint32 whichSet);
      Hashtable<int, uint32> _pollFDToArrayIndex;
      Queue<struct pollfd> _pollFDArray;
#else
      int _maxFD[NUM_FDSTATE_SETS];
      fd_set _fdSets[NUM_FDSTATE_SETS];
#endif
   };

#if defined(MUSCLE_USE_KQUEUE) || defined(MUSCLE_USE_EPOLL)
   friend void NotifySocketMultiplexersThatSocketIsClosed(int);
   void NotifySocketClosed(int fd)                    {GetCurrentFDState().NotifySocketClosed(fd);}
   inline FDState & GetCurrentFDState()               {return _fdState;}
   inline const FDState & GetCurrentFDState() const   {return _fdState;}
   inline FDState & GetAlternateFDState()             {return _fdState;}
   inline const FDState & GetAlternateFDState() const {return _fdState;}
   FDState _fdState;  // For kqueue and epoll we only need to keep one FDState in memory at at time

   SocketMultiplexer * _prevMultiplexer;
   SocketMultiplexer * _nextMultiplexer;
#else
   inline FDState & GetCurrentFDState()               {return _fdStates[_curFDState];}
   inline const FDState & GetCurrentFDState() const   {return _fdStates[_curFDState];}
   inline FDState & GetAlternateFDState()             {return _fdStates[_curFDState?0:1];}
   inline const FDState & GetAlternateFDState() const {return _fdStates[_curFDState?0:1];}

   FDState _fdStates[2];
   int _curFDState;
#endif
};

}; // end namespace muscle

#endif
