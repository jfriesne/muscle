/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <limits.h>   // for PATH_MAX
#include "dataio/ChildProcessDataIO.h"
#include "util/MiscUtilityFunctions.h"     // for ExitWithoutCleanup()
#include "util/NetworkUtilityFunctions.h"  // SendData() and ReceiveData()

#if defined(WIN32) || defined(__CYGWIN__)
# include <process.h>  // for _beginthreadex()
# define USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
#else
# if defined(__linux__)
#  include <pty.h>     // for forkpty() on Linux
# else
#  include <util.h>    // for forkpty() on MacOS/X
# endif
# include <termios.h>
# include <signal.h>  // for SIGHUP, etc
# include <sys/wait.h>  // for waitpid()
#endif

#include "util/NetworkUtilityFunctions.h"
#include "util/String.h"
#include "util/StringTokenizer.h"

namespace muscle {

ChildProcessDataIO :: ChildProcessDataIO(bool blocking) : _blocking(blocking), _killChildOkay(true), _maxChildWaitTime(0), _signalNumber(-1), _childProcessInheritFileDescriptors(false), _childProcessIsIndependent(false)
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   , _readFromStdout(INVALID_HANDLE_VALUE), _writeToStdin(INVALID_HANDLE_VALUE), _ioThread(INVALID_HANDLE_VALUE), _wakeupSignal(INVALID_HANDLE_VALUE), _childProcess(INVALID_HANDLE_VALUE), _childThread(INVALID_HANDLE_VALUE), _requestThreadExit(false)
#else
   , _childPID(-1)
#endif
{
   // empty
}

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
static void SafeCloseHandle(::HANDLE & h)
{
   if (h != INVALID_HANDLE_VALUE)
   {
      CloseHandle(h);
      h = INVALID_HANDLE_VALUE;
   }
}
#endif

status_t ChildProcessDataIO :: LaunchChildProcess(const Queue<String> & argq, uint32 launchBits, const char * optDirectory)
{
   uint32 numItems = argq.GetNumItems();
   if (numItems == 0) return B_ERROR;

   const char ** argv = newnothrow_array(const char *, numItems+1);
   if (argv == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
   for (uint32 i=0; i<numItems; i++) argv[i] = argq[i]();
   argv[numItems] = NULL;
   status_t ret = LaunchChildProcess(numItems, argv, launchBits, optDirectory);
   delete [] argv;
   return ret;
}

void ChildProcessDataIO :: SetChildProcessShutdownBehavior(bool okayToKillChild, int sendSignalNumber, uint64 maxChildWaitTime)
{
   _killChildOkay    = okayToKillChild;
   _signalNumber     = sendSignalNumber;
   _maxChildWaitTime = maxChildWaitTime;
}

status_t ChildProcessDataIO :: LaunchChildProcessAux(int argc, const void * args, uint32 launchBits, const char * optDirectory)
{
   TCHECKPOINT;

   Close();  // paranoia

#ifdef MUSCLE_AVOID_FORKPTY
   launchBits &= ~CHILD_PROCESS_LAUNCH_BIT_USE_FORKPTY;   // no sense trying to use pseudo-terminals if they were forbidden at compile time
#endif

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   (void) launchBits;  // avoid compiler warning

   SECURITY_ATTRIBUTES saAttr;
   {
      memset(&saAttr, 0, sizeof(saAttr));
      saAttr.nLength        = sizeof(saAttr);
      saAttr.bInheritHandle = true;
   }

   ::HANDLE childStdoutRead, childStdoutWrite;
   if (CreatePipe(&childStdoutRead, &childStdoutWrite, &saAttr, 0))
   {
      if (DuplicateHandle(GetCurrentProcess(), childStdoutRead, GetCurrentProcess(), &_readFromStdout, 0, false, DUPLICATE_SAME_ACCESS))
      {
         SafeCloseHandle(childStdoutRead);  // we'll use the dup from now on

         ::HANDLE childStdinRead, childStdinWrite;
         if (CreatePipe(&childStdinRead, &childStdinWrite, &saAttr, 0))
         {
            if (DuplicateHandle(GetCurrentProcess(), childStdinWrite, GetCurrentProcess(), &_writeToStdin, 0, false, DUPLICATE_SAME_ACCESS))
            {
               SafeCloseHandle(childStdinWrite);  // we'll use the dup from now on

               PROCESS_INFORMATION piProcInfo;
               memset(&piProcInfo, 0, sizeof(piProcInfo));

               STARTUPINFOA siStartInfo;
               {
                  memset(&siStartInfo, 0, sizeof(siStartInfo));
                  siStartInfo.cb         = sizeof(siStartInfo);
                  siStartInfo.hStdError  = childStdoutWrite;
                  siStartInfo.hStdOutput = childStdoutWrite;
                  siStartInfo.hStdInput  = childStdinRead;
                  siStartInfo.dwFlags    = STARTF_USESTDHANDLES;
               }

               String cmd;
               if (argc < 0) cmd = (const char *) args;
               else
               {
                  const char ** argv = (const char **) args;
                  Queue<String> tmpQ; (void) tmpQ.EnsureSize(argc);
                  for (int i=0; i<argc; i++) (void) tmpQ.AddTail(argv[i]);
                  cmd = UnparseArgs(tmpQ);
               }

               if (CreateProcessA((argc>=0)?(((const char **)args)[0]):NULL, (char *)cmd(), NULL, NULL, TRUE, 0, NULL, optDirectory, &siStartInfo, &piProcInfo))
               {
                  _childProcess   = piProcInfo.hProcess;
                  _childThread    = piProcInfo.hThread;

                  if (_blocking) return B_NO_ERROR;  // done!
                  else
                  {
                     // For non-blocking, we must have a separate proxy thread do the I/O for us :^P
                     _wakeupSignal = CreateEvent(0, false, false, 0);
                     if ((_wakeupSignal != INVALID_HANDLE_VALUE)&&(CreateConnectedSocketPair(_masterNotifySocket, _slaveNotifySocket, false) == B_NO_ERROR))
                     {
                        DWORD junkThreadID;
                        typedef unsigned (__stdcall *PTHREAD_START) (void *);
                        if ((_ioThread = (::HANDLE) _beginthreadex(NULL, 0, (PTHREAD_START)IOThreadEntryFunc, this, 0, (unsigned *) &junkThreadID)) != INVALID_HANDLE_VALUE) return B_NO_ERROR;
                     }
                  }
               }
            }
            SafeCloseHandle(childStdinRead);     // cleanup
            SafeCloseHandle(childStdinWrite);    // cleanup
         }
      }
      SafeCloseHandle(childStdoutRead);    // cleanup
      SafeCloseHandle(childStdoutWrite);   // cleanup
   }
   Close();  // free all allocated object state we may have
   return B_ERROR;
#else
   // First, set up our arguments array here in the parent process, since the child process won't be able to do any dynamic allocations.
   Queue<String> scratchChildArgQ;  // holds the strings that the pointers in the argv buffer will point to
   bool isParsed = (argc<0);
   if (argc < 0)
   {
      if (ParseArgs(String((const char *)args), scratchChildArgQ) != B_NO_ERROR) return B_ERROR;
      argc = scratchChildArgQ.GetNumItems();
   }

   ByteBuffer scratchChildArgv;  // the child process's argv array, NULL terminated
   if (scratchChildArgv.SetNumBytes((argc+1)*sizeof(char *), false) != B_NO_ERROR) return B_ERROR;

   // Populate the argv array for our child process to use
   const char ** argv = (const char **) scratchChildArgv.GetBuffer();
   if (isParsed) for (int i=0; i<argc; i++) argv[i] = scratchChildArgQ[i]();
            else memcpy(argv, (char **) args, argc*sizeof(char *));
   argv[argc] = NULL; // argv array must be NULL terminated!

   pid_t pid = (pid_t) -1;
   if (launchBits & CHILD_PROCESS_LAUNCH_BIT_USE_FORKPTY)
   {
# ifdef MUSCLE_AVOID_FORKPTY
      return B_ERROR;  // this branch should never be taken, due to the ifdef at the top of this function... but just in case
# else
      // New-fangled forkpty() implementation
      int masterFD = -1;
      pid = forkpty(&masterFD, NULL, NULL, NULL);
           if (pid > 0) _handle = GetConstSocketRefFromPool(masterFD);
      else if (pid == 0)
      {
         // Turn off the echo, we don't want to see that back on stdout
         struct termios tios;
         if (tcgetattr(STDIN_FILENO, &tios) >= 0)
         {
            tios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
            tios.c_oflag &= ~(ONLCR); /* also turn off NL to CR/NL mapping on output */
            (void) tcsetattr(STDIN_FILENO, TCSANOW, &tios);
         }
      }
#endif
   }
   else
   {
      // Old-fashioned fork() implementation
      ConstSocketRef masterSock, slaveSock;
      if (CreateConnectedSocketPair(masterSock, slaveSock, true) != B_NO_ERROR) return B_ERROR;
      pid = fork();
           if (pid > 0) _handle = masterSock;
      else if (pid == 0)
      {
         int fd = slaveSock()->GetFileDescriptor();
         if (((launchBits & CHILD_PROCESS_LAUNCH_BIT_EXCLUDE_STDIN)  == 0)&&(dup2(fd, STDIN_FILENO)  < 0)) ExitWithoutCleanup(20);
         if (((launchBits & CHILD_PROCESS_LAUNCH_BIT_EXCLUDE_STDOUT) == 0)&&(dup2(fd, STDOUT_FILENO) < 0)) ExitWithoutCleanup(20);
         if (((launchBits & CHILD_PROCESS_LAUNCH_BIT_EXCLUDE_STDERR) == 0)&&(dup2(fd, STDERR_FILENO) < 0)) ExitWithoutCleanup(20);
      }
   }

        if (pid < 0) return B_ERROR;      // fork failure!
   else if (pid == 0)
   {
      // we are the child process
      (void) signal(SIGHUP, SIG_DFL);  // FogBugz #2918

      // Close any file descriptors leftover from the parent process
      if (_childProcessInheritFileDescriptors == false)
      {
         int fdlimit = sysconf(_SC_OPEN_MAX);
         for (int i=STDERR_FILENO+1; i<fdlimit; i++) close(i);
      }

      if (_childProcessIsIndependent) (void) BecomeDaemonProcess();  // used by the LaunchIndependentChildProcess() static methods only

      char absArgv0[PATH_MAX];
      char ** argv = (char **) scratchChildArgv.GetBuffer();
      if (optDirectory)
      {
         // If we are going to change to a different directory, then we need to
         // generate an abolute-filepath for argv[0] first, otherwise we're likely
         // to be unable to find the executable to run!
         if (realpath(argv[0], absArgv0) != NULL) argv[0] = absArgv0;
         (void) chdir(optDirectory);  // FogBugz #10023
      }

      ChildProcessReadyToRun();

      if (execvp(argv[0], argv) < 0) perror("ChildProcessDataIO::execvp");  // execvp() should never return

      ExitWithoutCleanup(20);
   }
   else if (_handle())   // if we got this far, we are the parent process
   {
      _childPID = pid;
      if (SetSocketBlockingEnabled(_handle, _blocking) == B_NO_ERROR) return B_NO_ERROR;
   }

   Close();  // roll back!
   return B_ERROR;
#endif
}

ChildProcessDataIO :: ~ChildProcessDataIO()
{
   TCHECKPOINT;
   Close();
}

bool ChildProcessDataIO :: IsChildProcessAvailable() const
{
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   return (_readFromStdout != INVALID_HANDLE_VALUE);
#else
   return (_handle.GetFileDescriptor() >= 0);
#endif
}

status_t ChildProcessDataIO :: KillChildProcess()
{
   TCHECKPOINT;

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   if ((_childProcess != INVALID_HANDLE_VALUE)&&(TerminateProcess(_childProcess, 0))) return B_NO_ERROR;
#else
   if ((_childPID >= 0)&&(kill(_childPID, SIGKILL) == 0))
   {
      (void) waitpid(_childPID, NULL, 0);  // avoid creating a zombie process
      _childPID = -1;
      return B_NO_ERROR;
   }
#endif

   return B_ERROR;
}

status_t ChildProcessDataIO :: SignalChildProcess(int sigNum)
{
   TCHECKPOINT;

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   (void) sigNum;   // to shut the compiler up
   return B_ERROR;  // Not implemented under Windows!
#else
   // Yes, kill() is a misnomer.  Silly Unix people!
   return ((_childPID >= 0)&&(kill(_childPID, sigNum) == 0)) ? B_NO_ERROR : B_ERROR;
#endif
}

void ChildProcessDataIO :: Close()
{
   TCHECKPOINT;

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   if (_ioThread != INVALID_HANDLE_VALUE)  // if this is valid, _wakeupSignal is guaranteed valid too
   {
      _requestThreadExit = true;                // set the "Please go away" flag
      SetEvent(_wakeupSignal);                  // wake the thread up so he'll check the flag
      WaitForSingleObject(_ioThread, INFINITE); // then wait for him to go away
      ::CloseHandle(_ioThread);                 // fix handle leak
      _ioThread = INVALID_HANDLE_VALUE;
   }
   _masterNotifySocket.Reset();
   _slaveNotifySocket.Reset();
   SafeCloseHandle(_wakeupSignal);
   SafeCloseHandle(_readFromStdout);
   SafeCloseHandle(_writeToStdin);
   if ((_childProcess != INVALID_HANDLE_VALUE)&&(_childProcessIsIndependent == false)) DoGracefulChildShutdown();  // Windows can't double-fork, so in the independent case we just won't wait for him
   SafeCloseHandle(_childProcess);
   SafeCloseHandle(_childThread);
#else
   _handle.Reset();
   if (_childPID >= 0) DoGracefulChildShutdown();
   _childPID = -1;
#endif
}

void ChildProcessDataIO :: DoGracefulChildShutdown()
{
   if (_signalNumber >= 0) (void) SignalChildProcess(_signalNumber);
   if ((WaitForChildProcessToExit(_maxChildWaitTime) == false)&&(_killChildOkay)) (void) KillChildProcess();
}

bool ChildProcessDataIO :: WaitForChildProcessToExit(uint64 maxWaitTimeMicros)
{
#ifdef WIN32
   return ((_childProcess == INVALID_HANDLE_VALUE)||(WaitForSingleObject(_childProcess, (maxWaitTimeMicros==MUSCLE_TIME_NEVER)?INFINITE:((DWORD)(maxWaitTimeMicros/1000))) == WAIT_OBJECT_0));
#else
   if (_childPID < 0) return true;   // a non-existent child process is an exited child process, if you ask me.
   if (maxWaitTimeMicros == MUSCLE_TIME_NEVER) return (waitpid(_childPID, NULL, 0) == _childPID);
   else
   {
      // The tricky case... waiting for the child process to exit, with a timeout.
      // I'm implementing it via a polling loop, which is a sucky way to implement
      // it but the only alternative would involve mucking about with signal handlers,
      // and doing it that way would be unreliable in multithreaded environments.
      uint64 endTime = GetRunTime64()+maxWaitTimeMicros;
      uint64 pollInterval = 0;  // we'll start quickly, and work our way up.
      while(1)
      {
         int r = waitpid(_childPID, NULL, WNOHANG);  // WNOHANG should guarantee that this call will not block
              if (r  == _childPID) return true;  // yay, he exited!
         else if (r == -1) return false;         // fail on error

         int64 microsLeft = endTime-GetRunTime64();
         if (microsLeft <= 0) return false;  // we're out of time!

         // At this point, r was probably zero because the child wasn't ready to exit
         if (pollInterval < (200*1000)) pollInterval += (10*1000);
         Snooze64(muscleMin(pollInterval, (uint64)microsLeft));
      }
   }
#endif
}

int32 ChildProcessDataIO :: Read(void *buf, uint32 len)
{
   TCHECKPOINT;

   if (IsChildProcessAvailable())
   {
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
      if (_blocking)
      {
         DWORD actual_read;
         if (ReadFile(_readFromStdout, buf, len, &actual_read, NULL)) return actual_read;
      }
      else
      {
         int32 ret = ReceiveData(_masterNotifySocket, buf, len, _blocking);
         if (ret >= 0) SetEvent(_wakeupSignal);  // wake up the thread in case he has more data to give us
         return ret;
      }
#else
      int r = read_ignore_eintr(_handle.GetFileDescriptor(), buf, len);
      return _blocking ? r : ConvertReturnValueToMuscleSemantics(r, len, _blocking);
#endif
   }
   return -1;
}

int32 ChildProcessDataIO :: Write(const void *buf, uint32 len)
{
   TCHECKPOINT;

   if (IsChildProcessAvailable())
   {
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
      if (_blocking)
      {
         DWORD actual_write;
         if (WriteFile(_writeToStdin, buf, len, &actual_write, 0)) return actual_write;
      }
      else
      {
         int32 ret = SendData(_masterNotifySocket, buf, len, _blocking);
         if (ret > 0) SetEvent(_wakeupSignal);  // wake up the thread so he'll check his socket for our new data
         return ret;
      }
#else
      return ConvertReturnValueToMuscleSemantics(write_ignore_eintr(_handle.GetFileDescriptor(), buf, len), len, _blocking);
#endif
   }
   return -1;
}

void ChildProcessDataIO :: FlushOutput()
{
   // not implemented
}

void ChildProcessDataIO :: ChildProcessReadyToRun()
{
   // empty
}

const ConstSocketRef & ChildProcessDataIO :: GetChildSelectSocket() const
{
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   return _blocking ? GetNullSocket() : _masterNotifySocket;
#else
   return _handle;
#endif
}

void ChildProcessDataIO :: Shutdown()
{
   Close();
}

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION

const uint32 CHILD_BUFFER_SIZE = 1024;

// Used as a temporary holding area for data in transit
class ChildProcessBuffer
{
public:
   ChildProcessBuffer() : _length(0), _index(0) {/* empty */}

   char _buf[CHILD_BUFFER_SIZE];
   uint32 _length;  // how many bytes in _buf are actually valid
   uint32 _index;   // Index of the next byte to process
};

// to be called from within the I/O thread only!
void ChildProcessDataIO :: IOThreadAbort()
{
   // If we read zero bytes, that means EOF!  Child process has gone away!
   _slaveNotifySocket.Reset();
   _requestThreadExit = true;  // this will cause the I/O thread to go away now
}

void ChildProcessDataIO :: IOThreadEntry()
{
   bool childProcessExited = false;

   ChildProcessBuffer inBuf;  // bytes from the child process's stdout, waiting to go to the _slaveNotifySocket
   ChildProcessBuffer outBuf; // bytes from the _slaveNotifySocket, waiting to go to the child process's stdin

   ::HANDLE events[] = {_wakeupSignal, _childProcess};
   while(_requestThreadExit == false)
   {
      // IOThread <-> UserThread i/o handling here
      {
         // While we have any data in inBuf, send as much of it as possible back to the user thread.  This won't block.
         while(inBuf._index < inBuf._length)
         {
            int32 bytesToWrite = inBuf._length-inBuf._index;
            int32 bytesWritten = (bytesToWrite > 0) ? SendData(_slaveNotifySocket, &inBuf._buf[inBuf._index], bytesToWrite, false) : 0;
            if (bytesWritten > 0)
            {
               inBuf._index += bytesWritten;
               if (inBuf._index == inBuf._length) inBuf._index = inBuf._length = 0;
            }
            else
            {
               if (bytesWritten < 0) IOThreadAbort();  // use thread connection closed!?
               break;  // can't write any more, for now
            }
         }

         // While we have room in our outBuf, try to read some more data into it from the slave socket.  This won't block.
         while(outBuf._length < sizeof(outBuf._buf))
         {
            int32 maxLen = sizeof(outBuf._buf)-outBuf._length;
            int32 ret = ReceiveData(_slaveNotifySocket, &outBuf._buf[outBuf._length], maxLen, false);
            if (ret > 0) outBuf._length += ret;
            else
            {
               if (ret < 0) IOThreadAbort();  // user thread connection closed!?
               break;  // no more to read, for now
            }
         }
      }

      // IOThread <-> ChildProcess i/o handling (and blocking) here
      {
         if (childProcessExited)
         {
            if (inBuf._index == inBuf._length) IOThreadAbort();
            break;
         }

         // block here until an event happens... gotta poll because
         // the Window anonymous pipes system doesn't allow me to
         // to check for events on the pipe using WaitForMultipleObjects().
         // It may be worth it to use named pipes some day to get around this...
         int evt = WaitForMultipleObjects(ARRAYITEMS(events)-(childProcessExited?1:0), events, false, 250)-WAIT_OBJECT_0;
         if (evt == 1) childProcessExited = true;

         int32 numBytesToRead;
         while((numBytesToRead = sizeof(inBuf._buf)-inBuf._length) > 0)
         {
            // See if there is actually any data available for reading first
            DWORD pipeSize;
            if (PeekNamedPipe(_readFromStdout, NULL, 0, NULL, &pipeSize, NULL))
            {
               if (pipeSize > 0)
               {
                  DWORD numBytesRead;
                  if (ReadFile(_readFromStdout, &inBuf._buf[inBuf._length], numBytesToRead, &numBytesRead, NULL))
                  {
                     inBuf._length += numBytesRead;
                  }
                  else
                  {
                     IOThreadAbort();  // child process exited?
                     break;
                  }
               }
               else break;
            }
            else
            {
               IOThreadAbort();  // child process exited?
               break;
            }
         }

         int32 numBytesToWrite;
         while((numBytesToWrite = outBuf._length-outBuf._index) > 0)
         {
            DWORD bytesWritten;
            if (WriteFile(_writeToStdin, &outBuf._buf[outBuf._index], numBytesToWrite, &bytesWritten, 0))
            {
               if (bytesWritten > 0)
               {
                  outBuf._index += bytesWritten;
                  if (outBuf._index == outBuf._length) outBuf._index = outBuf._length = 0;
               }
               else break;  // no more space to write to, for now
            }
            else IOThreadAbort();  // wtf?
         }
      }
   }
}
#endif

status_t ChildProcessDataIO :: System(int argc, const char * argv[], uint32 launchBits, uint64 maxWaitTimeMicros, const char * optDirectory)
{
   ChildProcessDataIO cpdio(false);
   if (cpdio.LaunchChildProcess(argc, argv, launchBits, optDirectory) == B_NO_ERROR)
   {
      cpdio.WaitForChildProcessToExit(maxWaitTimeMicros);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t ChildProcessDataIO :: System(const Queue<String> & argq, uint32 launchBits, uint64 maxWaitTimeMicros, const char * optDirectory)
{
   uint32 numItems = argq.GetNumItems();
   if (numItems == 0) return B_ERROR;

   const char ** argv = newnothrow_array(const char *, numItems+1);
   if (argv == NULL) {WARN_OUT_OF_MEMORY; return B_ERROR;}
   for (uint32 i=0; i<numItems; i++) argv[i] = argq[i]();
   argv[numItems] = NULL;
   status_t ret = System(numItems, argv, launchBits, maxWaitTimeMicros, optDirectory);
   delete [] argv;
   return ret;
}

status_t ChildProcessDataIO :: System(const char * cmdLine, uint32 launchBits, uint64 maxWaitTimeMicros, const char * optDirectory)
{
   ChildProcessDataIO cpdio(false);
   if (cpdio.LaunchChildProcess(cmdLine, launchBits, optDirectory) == B_NO_ERROR)
   {
      cpdio.WaitForChildProcessToExit(maxWaitTimeMicros);
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(int argc, const char * argv[], bool inheritFileDescriptors, const char * optDirectory)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessInheritFileDescriptors(inheritFileDescriptors);
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(argc, argv, false, optDirectory);
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(const char * cmdLine, bool inheritFileDescriptors, const char * optDirectory)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessInheritFileDescriptors(inheritFileDescriptors);
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(cmdLine, false, optDirectory);
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(const Queue<String> & argv, bool inheritFileDescriptors, const char * optDirectory)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessInheritFileDescriptors(inheritFileDescriptors);
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(argv, false, optDirectory);
}

}; // end namespace muscle
