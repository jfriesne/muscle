/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <limits.h>   // for PATH_MAX
#include "dataio/ChildProcessDataIO.h"
#include "util/MiscUtilityFunctions.h"     // for ExitWithoutCleanup()
#include "util/NetworkUtilityFunctions.h"  // SendData() and ReceiveData()
#include "util/SocketMultiplexer.h"

#if defined(WIN32) || defined(__CYGWIN__)
# include <process.h>     // for _beginthreadex()
# if defined(_UNICODE) || defined(UNICODE)
#  undef GetEnvironmentStrings   // here because Windows headers are FUBAR ( https://devblogs.microsoft.com/oldnewthing/20130117-00/?p=5533 )
# endif
# define USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
#else
# if defined(__linux__)
#  include <pty.h>     // for forkpty() on Linux
# elif !defined(MUSCLE_AVOID_FORKPTY)
#  include <util.h>    // for forkpty() on MacOS/X
# endif
# include <termios.h>
# include <signal.h>  // for SIGHUP, etc
# include <sys/wait.h>  // for waitpid()
#endif

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
# include <fcntl.h>    // for F_GETOWN
# include <Security/Authorization.h>
# include <Security/AuthorizationTags.h>
#endif

#include "util/NetworkUtilityFunctions.h"
#include "util/String.h"

namespace muscle {

ChildProcessDataIO :: ChildProcessDataIO(bool blocking)
   : _blocking(blocking)
   , _killChildOkay(true)
   , _maxChildWaitTime(0)
   , _signalNumber(-1)
   , _childProcessCrashed(false)
   , _childProcessIsIndependent(false)
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   , _readFromStdout(INVALID_HANDLE_VALUE), _writeToStdin(INVALID_HANDLE_VALUE), _ioThread(INVALID_HANDLE_VALUE), _wakeupSignal(INVALID_HANDLE_VALUE), _childProcess(INVALID_HANDLE_VALUE), _childThread(INVALID_HANDLE_VALUE), _requestThreadExit(false)
#else
   , _childPID(-1)
#endif
#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   , _authRef(NULL)
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

status_t ChildProcessDataIO :: LaunchChildProcess(const Queue<String> & argq, ChildProcessLaunchFlags launchFlags, const char * optDirectory, const Hashtable<String, String> * optEnvironmentVariables)
{
   const uint32 numItems = argq.GetNumItems();
   if (numItems == 0) return B_BAD_ARGUMENT;

   const char ** argv = newnothrow_array(const char *, numItems+1);
   MRETURN_OOM_ON_NULL(argv);
   for (uint32 i=0; i<numItems; i++) argv[i] = argq[i]();
   argv[numItems] = NULL;
   status_t ret = LaunchChildProcess(numItems, argv, launchFlags, optDirectory, optEnvironmentVariables);
   delete [] argv;
   return ret;
}

void ChildProcessDataIO :: SetChildProcessShutdownBehavior(bool okayToKillChild, int sendSignalNumber, uint64 maxChildWaitTime)
{
   _killChildOkay    = okayToKillChild;
   _signalNumber     = sendSignalNumber;
   _maxChildWaitTime = maxChildWaitTime;
}

status_t ChildProcessDataIO :: LaunchChildProcessAux(int argc, const void * args, ChildProcessLaunchFlags launchFlags, const char * optDirectory, const Hashtable<String, String> * optEnvironmentVariables)
{
   TCHECKPOINT;

   Close();  // paranoia
   _childProcessCrashed = false;  // we don't care about the crashed-flag of a previous process anymore!

#ifdef MUSCLE_AVOID_FORKPTY
   launchFlags.ClearBit(CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY);   // no sense trying to use pseudo-terminals if they were forbidden at compile time
#endif

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   SECURITY_ATTRIBUTES saAttr;
   {
      memset(&saAttr, 0, sizeof(saAttr));
      saAttr.nLength        = sizeof(saAttr);
      saAttr.bInheritHandle = true;
   }

   status_t ret;

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
                  const bool hideChildGUI = launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_WIN32_HIDE_GUI);

                  memset(&siStartInfo, 0, sizeof(siStartInfo));
                  siStartInfo.cb          = sizeof(siStartInfo);
                  siStartInfo.hStdError   = childStdoutWrite;
                  siStartInfo.hStdOutput  = childStdoutWrite;
                  siStartInfo.hStdInput   = childStdinRead;
                  siStartInfo.dwFlags     = STARTF_USESTDHANDLES | (hideChildGUI ? STARTF_USESHOWWINDOW : 0);
                  siStartInfo.wShowWindow = hideChildGUI ? SW_HIDE : 0;
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

               // If environment-vars are specified, we need to create a new environment-variable-block
               // for the child process to use.  It will be the same as our own, but with the specified
               // env-vars added/updated in it.
               void * envVars  = NULL;
               uint8 * newBlock = NULL;
               if ((optEnvironmentVariables)&&(optEnvironmentVariables->HasItems()))
               {
                  Hashtable<String, String> curEnvVars;

                  char * oldEnvs = GetEnvironmentStrings();
                  if (oldEnvs)
                  {
                     const char * s = oldEnvs;
                     while(s)
                     {
                        if (*s)
                        {
                           const char * equals = strchr(s, '=');
                           if ((equals ? curEnvVars.Put(String(s, (uint32)(equals-s)), equals+1) : curEnvVars.Put(s, GetEmptyString())).IsOK(ret)) s = strchr(s, '\0')+1;
                                                                                                                                              else break;
                        }
                        else break;
                     }

                     FreeEnvironmentStringsA(oldEnvs);
                  }

                  (void) curEnvVars.Put(*optEnvironmentVariables);  // update our existing vars with the specified ones

                  // Now we can make a new environment-variables-block out of (curEnvVars)
                  uint32 newBlockSize = 1;  // this represents the final NUL terminator (after the last string)
                  for (HashtableIterator<String, String> iter(curEnvVars); iter.HasData(); iter++) newBlockSize += iter.GetKey().FlattenedSize()+iter.GetValue().FlattenedSize();  // includes NUL terminators

                  newBlock = newnothrow uint8[newBlockSize];
                  if (newBlock)
                  {
                     uint8 * s = newBlock;
                     for (HashtableIterator<String, String> iter(curEnvVars); iter.HasData(); iter++)
                     {
                        iter.GetKey().Flatten(s);   s += iter.GetKey().FlattenedSize();
                        *(s-1) = '=';  // replace key's trailing-NUL with an '=' sign
                        iter.GetValue().Flatten(s); s += iter.GetValue().FlattenedSize();
                     }
                     *s++ = '\0';

                     envVars = newBlock;
                  }
                  else
                  {
                     MWARN_OUT_OF_MEMORY;
                     ret = B_OUT_OF_MEMORY;
                  }
               }
             
               if (ret.IsOK())
               {
                  if (CreateProcessA((argc>=0)?(((const char **)args)[0]):NULL, (char *)cmd(), NULL, NULL, TRUE, 0, envVars, optDirectory, &siStartInfo, &piProcInfo))
                  {
                     delete [] newBlock;
                     newBlock = NULL;  // void possible double-delete below
   
                     _childProcess   = piProcInfo.hProcess;
                     _childThread    = piProcInfo.hThread;
   
                     if (_blocking) return B_NO_ERROR;  // done!
                     else 
                     {
                        // For non-blocking, we must have a separate proxy thread do the I/O for us :^P
                        _wakeupSignal = CreateEvent(0, false, false, 0);
                             if (_wakeupSignal == INVALID_HANDLE_VALUE) ret = B_ERRNO;
                        else if (CreateConnectedSocketPair(_masterNotifySocket, _slaveNotifySocket, false).IsOK(ret))
                        {
                           DWORD junkThreadID;
                           typedef unsigned (__stdcall *PTHREAD_START) (void *);
                           if ((_ioThread = (::HANDLE) _beginthreadex(NULL, 0, (PTHREAD_START)IOThreadEntryFunc, this, 0, (unsigned *) &junkThreadID)) != INVALID_HANDLE_VALUE) return B_NO_ERROR;
                                                                                                                                                                           else ret = B_ERRNO;
                        }
                     }
                  }
                  else ret = B_ERRNO;
               }

               delete [] newBlock;
            }
            else ret = B_ERRNO;

            SafeCloseHandle(childStdinRead);     // cleanup
            SafeCloseHandle(childStdinWrite);    // cleanup
         }
         else ret = B_ERRNO;
      }
      else ret = B_ERRNO;

      SafeCloseHandle(childStdoutRead);    // cleanup
      SafeCloseHandle(childStdoutWrite);   // cleanup
   }
   else ret = B_ERRNO;

   Close();  // free all allocated object state we may have
   return ret | B_ERROR;
#else
   status_t ret;

   // First, set up our arguments array here in the parent process, since the child process won't be able to do any dynamic allocations.
   Queue<String> scratchChildArgQ;  // holds the strings that the pointers in the argv buffer will point to
   const bool isParsed = (argc<0);
   if (argc < 0)
   {
      MRETURN_ON_ERROR(ParseArgs(String((const char *)args), scratchChildArgQ));
      argc = scratchChildArgQ.GetNumItems();
   }

   Queue<const char *> scratchChildArgv;  // the child process's argv array, NULL terminated
   MRETURN_ON_ERROR(scratchChildArgv.EnsureSize(argc+1, true));

   // Populate the argv array for our child process to use
   const char ** argv = scratchChildArgv.HeadPointer();
   if (isParsed) for (int i=0; i<argc; i++) argv[i] = scratchChildArgQ[i]();
            else memcpy(argv, args, argc*sizeof(char *));
   argv[argc] = NULL; // argv array must be NULL terminated!

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   if (_dialogPrompt.HasChars()) return LaunchPrivilegedChildProcess(argv);
#endif
   
   pid_t pid = (pid_t) -1;
   if (launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY))
   {
# ifdef MUSCLE_AVOID_FORKPTY
      return B_UNIMPLEMENTED;  // this branch should never be taken, due to the ifdef at the top of this function... but just in case
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
      MRETURN_ON_ERROR(CreateConnectedSocketPair(masterSock, slaveSock, true));
      pid = fork();
           if (pid > 0) _handle = masterSock;
      else if (pid == 0)
      {
         int fd = slaveSock()->GetFileDescriptor();
         if ((launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDIN)  == false)&&(dup2(fd, STDIN_FILENO)  < 0)) ExitWithoutCleanup(20);
         if ((launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDOUT) == false)&&(dup2(fd, STDOUT_FILENO) < 0)) ExitWithoutCleanup(20);
         if ((launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDERR) == false)&&(dup2(fd, STDERR_FILENO) < 0)) ExitWithoutCleanup(20);
      }
   }

        if (pid < 0) return B_ERRNO;  // fork failure!
   else if (pid == 0)
   {
      // we are the child process
      (void) signal(SIGHUP, SIG_DFL);  // FogBugz #2918

      // Close any file descriptors leftover from the parent process
      if (launchFlags.IsBitSet(CHILD_PROCESS_LAUNCH_FLAG_INHERIT_FDS) == false)
      {
         const int fdlimit = (int)sysconf(_SC_OPEN_MAX);
         for (int i=STDERR_FILENO+1; i<fdlimit; i++) close(i);
      }

      if (_childProcessIsIndependent) (void) BecomeDaemonProcess();  // used by the LaunchIndependentChildProcess() static methods only

      char absArgv0[PATH_MAX];
      const char ** zargv = &scratchChildArgv[0];
      const char * zargv0 = scratchChildArgv[0];
      if (optDirectory)
      {
         // If we are going to change to a different directory, then we need to
         // generate an absolute-filepath for zargv[0] first, otherwise we won't
         // be able to find the executable to run!
         if (realpath(zargv[0], absArgv0) != NULL) zargv[0] = zargv0 = absArgv0;
         if (chdir(optDirectory) < 0) perror("ChildProcessDataIO::chdir");  // FogBugz #10023
      }

      if (optEnvironmentVariables)
      {
         for (HashtableIterator<String,String> iter(*optEnvironmentVariables); iter.HasData(); iter++) (void) setenv(iter.GetKey()(), iter.GetValue()(), 1);
      }

      if (ChildProcessReadyToRun().IsOK(ret))
      {
         if (execvp(zargv0, const_cast<char **>(zargv)) < 0) perror("ChildProcessDataIO::execvp");  // execvp() should never return
      }
      else LogTime(MUSCLE_LOG_ERROR, "ChildProcessDataIO:  ChildProcessReadyToRun() returned [%s], not running child process!\n", ret());

      ExitWithoutCleanup(20);
   }
   else if (_handle())   // if we got this far, we are the parent process
   {
      _childPID = pid;
      if (SetSocketBlockingEnabled(_handle, _blocking).IsOK(ret)) return B_NO_ERROR;
   }

   Close();  // roll back!
   return ret | B_ERROR;
#endif
}

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
status_t ChildProcessDataIO :: LaunchPrivilegedChildProcess(const char ** argv)
{
   // Code adapted from Performant Design's cocoasudo program 
   // at git://github.com/performantdesign/cocoasudo.git
   OSStatus status;
   AuthorizationRef authRef = NULL;

   AuthorizationItem right = { kAuthorizationRightExecute, strlen(argv[0]), &argv[0], 0 };
   AuthorizationRights rightSet = { 1, &right };

   AuthorizationEnvironment myAuthorizationEnvironment; memset(&myAuthorizationEnvironment, 0, sizeof(myAuthorizationEnvironment));
   AuthorizationItem kAuthEnv;                          memset(&kAuthEnv, 0, sizeof(kAuthEnv));
   myAuthorizationEnvironment.items = &kAuthEnv;

   kAuthEnv.name        = kAuthorizationEnvironmentPrompt;
   kAuthEnv.valueLength = _dialogPrompt.Length();
   kAuthEnv.value       = (void *) _dialogPrompt();
   kAuthEnv.flags       = 0;
   myAuthorizationEnvironment.count = 1;

   if (AuthorizationCreate(NULL, &myAuthorizationEnvironment, kAuthorizationFlagDefaults, &authRef) != errAuthorizationSuccess)
   {
      LogTime(MUSCLE_LOG_ERROR, "ChildProcessDataIO::LaunchPrivilegedChildProcess():  Could not create authorization reference object.\n");
      return B_ERROR("AuthorizationCreate() failed");
   }
   else status = AuthorizationCopyRights(authRef, &rightSet, &myAuthorizationEnvironment, kAuthorizationFlagDefaults|kAuthorizationFlagPreAuthorize|kAuthorizationFlagInteractionAllowed|kAuthorizationFlagExtendRights, NULL);

   if (status == errAuthorizationSuccess)
   {
      FILE *ioPipe;
      status = AuthorizationExecuteWithPrivileges(authRef, argv[0], kAuthorizationFlagDefaults, (char **)(&argv[1]), &ioPipe);  // +1 because it doesn't take the traditional argv[0]
      if (status == errAuthorizationSuccess)
      {
         _ioPipe.SetFile(ioPipe);
         _handle  = _ioPipe.GetReadSelectSocket();
         _authRef = authRef;
         return _handle() ? SetSocketBlockingEnabled(_handle, false) : B_ERROR;
      }
      else 
      {
         AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
         return (status == errAuthorizationCanceled) ? B_ACCESS_DENIED : B_ERROR("AuthorizationExecuteWithPrivileges() failed");
      }
   }
   else
   {
      AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
      return (status == errAuthorizationCanceled) ? B_ACCESS_DENIED : B_ERROR("AuthorizationCopyRights() pre-authorization failed");
   }
}
#endif

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
   if (_childProcess == INVALID_HANDLE_VALUE) return B_BAD_OBJECT;
   return TerminateProcess(_childProcess, 0) ? B_NO_ERROR : B_ERRNO;
#else
# if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   if (_ioPipe.GetFile()) return B_UNIMPLEMENTED;  // no can do
# endif
   if (_childPID < 0) return B_BAD_OBJECT;
   if (kill(_childPID, SIGKILL) == 0)
   {
      (void) waitpid(_childPID, NULL, 0);  // avoid creating a zombie process
      _childPID = -1;
      return B_NO_ERROR;
   }
   else return B_ERRNO;
#endif
}

status_t ChildProcessDataIO :: SignalChildProcess(int sigNum)
{
   TCHECKPOINT;

#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   (void) sigNum;   // to shut the compiler up
   return B_UNIMPLEMENTED;  // Not implemented under Windows!
#else
# if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   if (_ioPipe.GetFile()) return B_UNIMPLEMENTED;  // no can do
# endif
   if (_childPID < 0) return B_BAD_OBJECT;
   return (kill(_childPID, sigNum) == 0) ? B_NO_ERROR : B_ERRNO; // Yes, kill() is a misnomer.  Silly Unix people!
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

# if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   _ioPipe.Shutdown();
   if (_authRef)
   {  
      AuthorizationFree((AuthorizationRef)_authRef, kAuthorizationFlagDestroyRights);
      _authRef = NULL;
   }
# endif
#endif
}

void ChildProcessDataIO :: DoGracefulChildShutdown()
{
   if (_signalNumber >= 0) (void) SignalChildProcess(_signalNumber);
   if ((WaitForChildProcessToExit(_maxChildWaitTime).IsError())&&(_killChildOkay)) (void) KillChildProcess();
}

status_t ChildProcessDataIO :: WaitForChildProcessToExit(uint64 maxWaitTimeMicros)
{
#ifdef USE_WINDOWS_CHILDPROCESSDATAIO_IMPLEMENTATION
   if (_childProcess == INVALID_HANDLE_VALUE) return B_NO_ERROR; // a non-existent child process is an exited child process, if you ask me.
   _childProcessCrashed = false;                                 // reset the flag only when there is an actual child process to wait for

   if (WaitForSingleObject(_childProcess, (maxWaitTimeMicros==MUSCLE_TIME_NEVER)?INFINITE:((DWORD)(maxWaitTimeMicros/1000))) == WAIT_OBJECT_0)
   {
      DWORD exitCode;
      if (GetExitCodeProcess(_childProcess, &exitCode))
      {
         // Note that this is only a heuristic since a sufficiently
         // demented child process could return a code that matches
         // this criterion as part of its normal exit(), and a crashed
         // program could (conceivably) have an exit code that doesn't
         // meet this criterion.  But in general this will work.  --jaf
         _childProcessCrashed = ((exitCode & (0xC0000000)) != 0);
      }
      return B_NO_ERROR;
   }
#else
# if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   if (_ioPipe.GetFile())
   {
      const int fd = fileno(_ioPipe.GetFile());

      // Since AuthorizationExecuteWithPrivileges() doesn't give us a _childPID to wait on, all we can do is 
      // block-and-read until either we read EOF from the _ioPipe or we reach the timeout-time.
      bool sawEOF = false;
      SocketMultiplexer sm;
      const uint64 endTime = (maxWaitTimeMicros == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+maxWaitTimeMicros);
      while(GetRunTime64() < endTime)
      {
         if ((sm.RegisterSocketForReadReady(fd).IsError())||(sm.WaitForEvents(endTime) < 0)) break;
         else
         {
            char junk[1024] = "";
            if ((fread(junk, sizeof(junk), 1, _ioPipe.GetFile()) == 0)||(feof(_ioPipe.GetFile())))
            {
               sawEOF = true;
               break;
            }
         }
      }
      return sawEOF ? B_NO_ERROR : B_TIMED_OUT;  // if we saw EOF on the _ioPipe, we'll take that to mean the child process exited -- best we can do
   }
# endif

   if (_childPID < 0) return B_NO_ERROR; // a non-existent child process is an exited child process, if you ask me.
   _childProcessCrashed = false;         // reset the flag only when there is an actual child process to wait for

   if (maxWaitTimeMicros == MUSCLE_TIME_NEVER) 
   {
      int status = 0;
      const int pid = waitpid(_childPID, &status, 0);
      if (pid == _childPID)
      {
         _childProcessCrashed = WIFSIGNALED(status);
         return B_NO_ERROR;
      }
   }
   else
   {
      // The tricky case... waiting for the child process to exit, with a timeout.
      // I'm implementing it via a polling loop, which is a sucky way to implement
      // it but the only alternative would involve mucking about with signal handlers,
      // and doing it that way would be unreliable in multithreaded environments.
      const uint64 endTime = GetRunTime64()+maxWaitTimeMicros;
      uint64 pollInterval = 0;  // we'll start quickly, and start polling more slowly only if the child process doesn't exit soon
      while(1)
      {
         int status = 0;
         const int r = waitpid(_childPID, &status, WNOHANG);  // WNOHANG should guarantee that this call will not block
         if (r == _childPID) 
         {
            _childProcessCrashed = WIFSIGNALED(status);
            return B_NO_ERROR;  // yay, he exited!
         }
         else if (r == -1) break;      // fail on error

         const int64 microsLeft = endTime-GetRunTime64();
         if (microsLeft <= 0) break;   // we're out of time!

         // At this point, r was probably zero because the child wasn't ready to exit
         if ((int64)pollInterval < MillisToMicros(200)) pollInterval += MillisToMicros(10);
         Snooze64(muscleMin(pollInterval, (uint64)microsLeft));
      }
   }
#endif

   return B_TIMED_OUT;
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
         const int32 ret = ReceiveData(_masterNotifySocket, buf, len, _blocking);
         if (ret >= 0) SetEvent(_wakeupSignal);  // wake up the thread in case he has more data to give us
         return ret;
      }
#else
      const long r = read_ignore_eintr(_handle.GetFileDescriptor(), buf, len);
      return _blocking ? (int32)r : ConvertReturnValueToMuscleSemantics(r, len, _blocking);
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
         const int32 ret = SendData(_masterNotifySocket, buf, len, _blocking);
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

status_t ChildProcessDataIO :: ChildProcessReadyToRun()
{
   return B_NO_ERROR;
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
   ChildProcessBuffer()
      : _length(0)
      , _index(0) 
   {
      // empty
   }

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

   const uint64 minPollTimeMicros = MillisToMicros(0);
   const uint64 maxPollTimeMicros = MillisToMicros(250);
   uint64 pollTimeMicros          = maxPollTimeMicros;

   ::HANDLE events[] = {_wakeupSignal, _childProcess};
   while(_requestThreadExit == false)
   {
      // IOThread <-> UserThread i/o handling here
      {
         // While we have any data in inBuf, send as much of it as possible back to the user thread.  This won't block.
         while(inBuf._index < inBuf._length)
         {
            const int32 bytesToWrite = inBuf._length-inBuf._index;
            const int32 bytesWritten = (bytesToWrite > 0) ? SendData(_slaveNotifySocket, &inBuf._buf[inBuf._index], bytesToWrite, false) : 0;
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
            const int32 maxLen = sizeof(outBuf._buf)-outBuf._length;
            const int32 ret = ReceiveData(_slaveNotifySocket, &outBuf._buf[outBuf._length], maxLen, false);
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
         const int evt = WaitForMultipleObjects(ARRAYITEMS(events), events, false, MicrosToMillis(pollTimeMicros))-WAIT_OBJECT_0;
         if (evt == 1) childProcessExited = true;

         int32 totalNumBytesRead = 0;
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
                     totalNumBytesRead += numBytesRead;
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

         int32 totalNumBytesWritten = 0;
         int32 numBytesToWrite;
         while((numBytesToWrite = outBuf._length-outBuf._index) > 0)
         {
            DWORD bytesWritten;
            if (WriteFile(_writeToStdin, &outBuf._buf[outBuf._index], numBytesToWrite, &bytesWritten, 0))
            {
               if (bytesWritten > 0)
               {
                  totalNumBytesWritten += bytesWritten;
                  outBuf._index += bytesWritten;
                  if (outBuf._index == outBuf._length) outBuf._index = outBuf._length = 0;
               }
               else break;  // no more space to write to, for now
            }
            else IOThreadAbort();  // wtf?
         }

         if ((totalNumBytesRead > 0)||(totalNumBytesWritten > 0))
         {
            // traffic!  Quickly decrease poll time to improve throughput (MAV-80)
            pollTimeMicros = (pollTimeMicros+minPollTimeMicros)/2;
         }
         else
         {
            // quiet!  Gradually increase poll time to reduce polling overhead
            pollTimeMicros = ((pollTimeMicros*95)+(maxPollTimeMicros*5))/100;
         }
      }
   }
}
#endif

status_t ChildProcessDataIO :: System(int argc, const char * argv[], ChildProcessLaunchFlags launchFlags, uint64 maxWaitTimeMicros, const char * optDirectory, const Hashtable<String, String> * optEnvironmentVariables)
{
   status_t ret;
   ChildProcessDataIO cpdio(false);
   if (cpdio.LaunchChildProcess(argc, argv, launchFlags, optDirectory, optEnvironmentVariables).IsOK(ret))
   {
      (void) cpdio.WaitForChildProcessToExit(maxWaitTimeMicros);
      return B_NO_ERROR;
   }
   else return ret;
}

status_t ChildProcessDataIO :: System(const Queue<String> & argq, ChildProcessLaunchFlags launchFlags, uint64 maxWaitTimeMicros, const char * optDirectory, const Hashtable<String, String> * optEnvironmentVariables)
{
   const uint32 numItems = argq.GetNumItems();
   if (numItems == 0) return B_BAD_ARGUMENT;

   const char ** argv = newnothrow_array(const char *, numItems+1);
   MRETURN_OOM_ON_NULL(argv);
   for (uint32 i=0; i<numItems; i++) argv[i] = argq[i]();
   argv[numItems] = NULL;
   const status_t ret = System(numItems, argv, launchFlags, maxWaitTimeMicros, optDirectory, optEnvironmentVariables);
   delete [] argv;
   return ret;
}

status_t ChildProcessDataIO :: System(const char * cmdLine, ChildProcessLaunchFlags launchFlags, uint64 maxWaitTimeMicros, const char * optDirectory, const Hashtable<String, String> * optEnvironmentVariables)
{
   ChildProcessDataIO cpdio(false);
   status_t ret;
   if (cpdio.LaunchChildProcess(cmdLine, launchFlags, optDirectory, optEnvironmentVariables).IsOK(ret))
   {
      (void) cpdio.WaitForChildProcessToExit(maxWaitTimeMicros);
      return B_NO_ERROR;
   }
   else return ret;
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(int argc, const char * argv[], const char * optDirectory, ChildProcessLaunchFlags launchFlags, const Hashtable<String, String> * optEnvironmentVariables)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(argc, argv, launchFlags, optDirectory, optEnvironmentVariables);
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(const char * cmdLine, const char * optDirectory, ChildProcessLaunchFlags launchFlags, const Hashtable<String, String> * optEnvironmentVariables)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(cmdLine, launchFlags, optDirectory, optEnvironmentVariables);
}

status_t ChildProcessDataIO :: LaunchIndependentChildProcess(const Queue<String> & argv, const char * optDirectory, ChildProcessLaunchFlags launchFlags, const Hashtable<String, String> * optEnvironmentVariables)
{
   ChildProcessDataIO cpdio(true);
   cpdio._childProcessIsIndependent = true;  // so the cpdio dtor won't block waiting for the child to exit
   cpdio.SetChildProcessShutdownBehavior(false);
   return cpdio.LaunchChildProcess(argv, launchFlags, optDirectory, optEnvironmentVariables);
}

} // end namespace muscle
