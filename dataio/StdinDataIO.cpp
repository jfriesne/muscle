/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/StdinDataIO.h"
#include "util/Hashtable.h"

#if defined(WIN32) || defined(CYGWIN)
# define USE_WIN32_STDINDATAIO_IMPLEMENTATION
# include <process.h>  // for _beginthreadex()
#endif

namespace muscle {

#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
enum {
   STDIN_THREAD_STATUS_UNINITIALIZED = 0,
   STDIN_THREAD_STATUS_RUNNING,
   STDIN_THREAD_STATUS_EXITED,
   NUM_STDIN_THREAD_STATUSES
};
static Mutex _slaveSocketsMutex;  // serializes access to these static variables
static Hashtable<uint32, ConstSocketRef> _slaveSockets;
static uint32 _slaveSocketTagCounter = 0;
static uint32 _stdinThreadStatus     = STDIN_THREAD_STATUS_UNINITIALIZED;
static ::HANDLE _slaveThread         = INVALID_HANDLE_VALUE;
static ::HANDLE _stdinHandle         = INVALID_HANDLE_VALUE;

static unsigned __stdcall StdinThreadEntryFunc(void *)
{
   if (_stdinHandle != INVALID_HANDLE_VALUE)
   {
      DWORD oldConsoleMode = 0;
      GetConsoleMode(_stdinHandle, &oldConsoleMode);
      SetConsoleMode(_stdinHandle, oldConsoleMode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);

      // The only time this thread is allowed to exit is if stdin is closed.  Otherwise it runs until the
      // process terminates, because Windows has no way to unblock the call to ReadFile()!
      Hashtable<uint32, ConstSocketRef> temp;  // declared out here only to avoid reallocations on every loop iteration
      char buf[4096];
      DWORD numBytesRead;
      while(ReadFile(_stdinHandle, buf, sizeof(buf), &numBytesRead, NULL))
      {
         // Grab a temporary copy of the listeners-set.  That we we don't risk blocking in SendData() while holding the mutex.
         if (_slaveSocketsMutex.Lock() == B_NO_ERROR)
         {
            temp = _slaveSockets;
            _slaveSocketsMutex.Unlock();
         }

         // Now send the data we read from stdin to all the registered sockets
         bool trim = false;
         for (HashtableIterator<uint32, ConstSocketRef> iter(temp); iter.HasData(); iter++) if (SendData(iter.GetValue(), buf, numBytesRead, true) != numBytesRead) {trim = true; iter.GetValue().Reset();}

         // Lastly, remove from the registered-sockets-set any sockets that SendData() errored out on.
         // This will cause the socket connection to be closed and the master thread(s) to be notified.
         if ((trim)&&(_slaveSocketsMutex.Lock() == B_NO_ERROR))
         {
            for (HashtableIterator<uint32, ConstSocketRef> iter(_slaveSockets); iter.HasData(); iter++)
            {
               const ConstSocketRef * v = temp.Get(iter.GetKey());
               if ((v)&&(v->IsValid() == false)) (void) _slaveSockets.Remove(iter.GetKey());
            }
            _slaveSocketsMutex.Unlock();
         }

         temp.Clear();  // it's important not to have extra Refs hanging around in case the process exits!
      }

      // Restore the old console mode before we leave
      SetConsoleMode(_stdinHandle, oldConsoleMode);
   }

   // Oops, stdin failed... clear the slave sockets table so that the client objects will know to close up shop
   if (_slaveSocketsMutex.Lock() == B_NO_ERROR)
   {
      _stdinThreadStatus = STDIN_THREAD_STATUS_EXITED;
      _slaveSockets.Clear();

      // We'll close our own handle, thankyouverymuch
      if (_slaveThread != INVALID_HANDLE_VALUE)
      { 
         CloseHandle(_slaveThread);
         _slaveThread = INVALID_HANDLE_VALUE;
      }

      if (_stdinHandle != INVALID_HANDLE_VALUE)
      {
         CloseHandle(_stdinHandle);
         _stdinHandle = INVALID_HANDLE_VALUE;
      }

      _slaveSocketsMutex.Unlock(); 
   }
   return 0;
}
#else
static Socket _stdinSocket(STDIN_FILENO, false);  // we generally don't want to close stdin
#endif

StdinDataIO :: StdinDataIO(bool blocking) : _stdinBlocking(blocking)
#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
 , _slaveSocketTag(0)
#else
 , _fdIO(ConstSocketRef(&_stdinSocket, false), true)
#endif
{
#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
   if (_stdinBlocking == false)
   {
      // For non-blocking I/O, we need to handle stdin in a separate thread. 
      // note that I freopen stdin to "nul" so that other code (read: Python)
      // won't try to muck about with stdin and interfere with StdinDataIO's
      // operation.  I don't know of any good way to restore it again after,
      // though... so a side effect of StdinDataIO under Windows is that
      // stdin gets redirected to nul... once you've created one non-blocking
      // StdinDataIO, you'll need to continue accessing stdin only via
      // non-blocking StdinDataIOs.
      bool okay = false;
      ConstSocketRef slaveSocket;
      if ((CreateConnectedSocketPair(_masterSocket, slaveSocket, false) == B_NO_ERROR)&&(SetSocketBlockingEnabled(slaveSocket, true) == B_NO_ERROR)&&(_slaveSocketsMutex.Lock() == B_NO_ERROR))
      {
         bool threadCreated = false;
         if (_stdinThreadStatus == STDIN_THREAD_STATUS_UNINITIALIZED) 
         {
            DWORD junkThreadID;
            _stdinThreadStatus = ((DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_INPUT_HANDLE), GetCurrentProcess(), &_stdinHandle, 0, false, DUPLICATE_SAME_ACCESS))&&(freopen("nul", "r", stdin) != NULL)&&((_slaveThread = (::HANDLE) _beginthreadex(NULL, 0, StdinThreadEntryFunc, NULL, CREATE_SUSPENDED, (unsigned *) &junkThreadID)) != 0)) ? STDIN_THREAD_STATUS_RUNNING : STDIN_THREAD_STATUS_EXITED;
            threadCreated = (_stdinThreadStatus == STDIN_THREAD_STATUS_RUNNING);
         }
         if ((_stdinThreadStatus == STDIN_THREAD_STATUS_RUNNING)&&(_slaveSockets.Put(_slaveSocketTag = (++_slaveSocketTagCounter), slaveSocket) == B_NO_ERROR)) okay = true;
                                                                                                                                                        else LogTime(MUSCLE_LOG_ERROR, "StdinDataIO:  Could not start stdin thread!\n");
         _slaveSocketsMutex.Unlock();

         // We don't start the thread running until here, that way there's no chance of race conditions if the thread exits immediately
         if (threadCreated) ResumeThread(_slaveThread);
      }
      else LogTime(MUSCLE_LOG_ERROR, "StdinDataIO:  Error setting up I/O sockets!\n");

      if (okay == false) Close();
   }
#endif
}

StdinDataIO ::
~StdinDataIO() 
{
   Close();
}

void StdinDataIO :: Shutdown()
{
   Close();
}

void StdinDataIO :: Close()
{
#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
   if ((_stdinBlocking == false)&&(_slaveSocketsMutex.Lock() == B_NO_ERROR))
   {
      _slaveSockets.Remove(_slaveSocketTag);
      _slaveSocketsMutex.Unlock();
      // Note that I deliberately let the Stdin thread keep running, since there's no clean way to stop it from another thread.
   }
#else
   _fdIO.Shutdown();
#endif
}

int32 StdinDataIO :: Read(void * buffer, uint32 size)
{
#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
   if (_stdinBlocking)
   {
      DWORD actual_read;
      return ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, size, &actual_read, 0) ? actual_read : -1;
   }
   else return ReceiveData(_masterSocket, buffer, size, _stdinBlocking);
#else
   // Turn off stdin's blocking I/O mode only during the Read() call.
   if (_stdinBlocking == false) (void) _fdIO.SetBlockingIOEnabled(false);
   int32 ret = _fdIO.Read(buffer, size);
   if (_stdinBlocking == false) (void) _fdIO.SetBlockingIOEnabled(true);
   return ret;
#endif
}

const ConstSocketRef & StdinDataIO :: GetReadSelectSocket() const
{
#ifdef USE_WIN32_STDINDATAIO_IMPLEMENTATION
   return _stdinBlocking ? GetNullSocket() : _masterSocket;
#else
   return _fdIO.GetReadSelectSocket();
#endif
}

}; // end namespace muscle
