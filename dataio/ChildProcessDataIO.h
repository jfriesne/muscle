/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef ChildProcessDataIO_h
#define ChildProcessDataIO_h

#include <errno.h>
#include "dataio/FileDataIO.h"
#include "support/BitChord.h"
#include "util/Queue.h"
#include "util/Hashtable.h"

namespace muscle {

// Flags that can be passed as a bit-chord to LaunchChildProcess()
enum {
   CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY = 0, /**< if set, we'll use forkpty() instead of fork() (ignored under Windows) */
   CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDIN,   /**< if set, we won't redirect from the child process's stdin (supported by fork() implementation only, for now) */
   CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDOUT,  /**< if set, we won't capture and return output from the child process's stdout (supported by fork() implementation only, for now) */
   CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDERR,  /**< if set, we won't capture and return output from the child process's stderr (supported by fork() implementation only, for now) */
   CHILD_PROCESS_LAUNCH_FLAG_WIN32_HIDE_GUI,  /**< Windows only:  if set, the child process's GUI windows will be hidden */
   CHILD_PROCESS_LAUNCH_FLAG_INHERIT_FDS,     /**< If set, we will allow the child process to inherit file descriptors from this process. */
   NUM_CHILD_PROCESS_LAUNCH_FLAGS             /**< Guard value */
};
DECLARE_BITCHORD_FLAGS_TYPE(ChildProcessLaunchFlags, NUM_CHILD_PROCESS_LAUNCH_FLAGS);

#ifndef MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS
/** The default value of (launchFlags) that will be suppplied to LaunchChildProcess(), if no value is explicitly supplied.  Defaults to CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY unless overridden at compile time via e.g. -DMUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS=CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY,CHILD_PROCESS_LAUNCH_FLAG_EXCLUDE_STDERR or etc. */
# define MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS CHILD_PROCESS_LAUNCH_FLAG_USE_FORKPTY
#endif

/** This DataIO class is a handy cross-platform way to spawn 
 *  and talk to a child process.  Any data that the child process
 *  prints to stdout can be read from this object, and any data
 *  that is written to this object will be send to the child process's
 *  stdin.  Note that this class is currently only implemented to work 
 *  under Windows, MacOS/X, BeOS, and Linux.
 */
class ChildProcessDataIO : public DataIO
{
public:
   /** Constructor.
    *  @param blocking If true, I/O will be blocking; else non-blocking.
    *  @note that you will need to call LaunchChildProcess() to actually start the child process going.
    */
   ChildProcessDataIO(bool blocking);

   /** Destructor */
   virtual ~ChildProcessDataIO();
   
   /** Launch the child process.  Note that this method should only be called once!
     * @param argc The argc variable to be passed to the child process
     * @param argv The argv variable to be passed to the child process
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     * @return B_NO_ERROR on success, or an error code if the launch failed.
     */
   status_t LaunchChildProcess(int argc, const char * argv[], ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL) {return LaunchChildProcessAux(muscleMax(0,argc), argv, launchFlags, optDirectory, optEnvironmentVariables);}

   /** As above, but the program name and all arguments are specified as a single string.
     * @param cmd String to launch the child process with
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     * @return B_NO_ERROR on success, or an error code if the launch failed.
     */
   status_t LaunchChildProcess(const char * cmd, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL) {return LaunchChildProcessAux(-1, cmd, launchFlags, optDirectory, optEnvironmentVariables);}

   /** Convenience method.  Launches a child process using an (argc,argv) that is constructed from the passed in argument list.
     * @param argv A list of strings to construct the (argc,argv) from.  The first string should be the executable name, the second string
     *             should be the first argument to the executable, and so on.
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     * @return B_NO_ERROR on success, or an error code if the launch failed.
     */
   status_t LaunchChildProcess(const Queue<String> & argv, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Read data from the child process's stdout stream. 
     * @param buffer The read bytes will be placed here
     * @param size Maximum number of bytes that may be placed into (buffer).
     * @returns The number of bytes placed into (buffer), or a negative value if there was an error.
     */
   virtual int32 Read(void * buffer, uint32 size);

   /** Write data to the child process's stdin stream. 
     * @param buffer The bytes to write to the child process's stdin.
     * @param size Maximum number of bytes to read from (buffer) and written to the child process's stdin.
     * @returns The number of bytes written, or a negative value if there was an error.
     */
   virtual int32 Write(const void * buffer, uint32 size);

   /** Doesn't return until all outgoing have been sent */
   virtual void FlushOutput();

   /** Kills the child process, using the sequence described at SetChildProcessShutdownBehavior(). */
   virtual void Shutdown();

   /** Returns a socket that can be select()'d on for notifications of read availability from the 
    *  child's stdout or stderr streams.
    *  Even works under Windows (in non-blocking mode, anyway), despite Microsoft's best efforts 
    *  to make such a thing impossible :^P Note that you should only pass this socket to select(); 
    *  to read from the child process, call Read() on this object but don't try to recv()/etc on 
    *  this socket directly!
    */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetChildSelectSocket();}

   /** Returns a socket that can be select()'d on for notifications of write availability to the 
    *  child's stdin stream.
    *  Even works under Windows (in non-blocking mode, anyway), despite Microsoft's best efforts 
    *  to make such a thing impossible :^P Note that you should only pass this socket to select(); 
    *  to write to the child process, call Write() on this object but don't try to send()/etc on 
    *  this socket directly!
    */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetChildSelectSocket();}

   /** Returns true iff the child process is available (i.e. if startup succeeded). */
   bool IsChildProcessAvailable() const;

   /** Specify the series of actions we should take to gracefully shutdown the child process 
     * when this object is closed.  
     *
     * The shutdown sequence we use is the following:
     *   1) Close the socket to the child process
     *   2) (optional) send a signal to the child process (note:  this step is not available under Windows)
     *   3) (optional) wait up to a specified amount of time for the child process to exit voluntarily
     *   4) (optional) forcibly kill the child process if it hasn't exited by this time
     *
     * @param okayToKillChild If true, we are allowed to violently terminate the child if he still 
     *                        hasn't exited by the time we are done waiting for him to exit.  If false,
     *                        we will just let the child continue running if he feels he must.
     * @param sendSignalNumber If non-negative, we will prompt the child process to exit by
     *                         sending him this signal before beginning to wait.  Note that
     *                         this argument is not used under Windows.
     *                         Defaults to -1.
     * @param maxWaitTimeMicros If specified, this is the maximum amount of time (in microseconds) 
     *                          that we should wait for the child process to exit before continuing.
     *                          Defaults to MUSCLE_TIME_NEVER, meaning that we will wait indefinitely
     *                          for the child to exit, if necessary.
     *
     * Note that if this method is never called, then the default behavior is to immediately 
     * kill the child (with no signal sent and no wait time elapsed).
     */
   void SetChildProcessShutdownBehavior(bool okayToKillChild, int sendSignalNumber = -1, uint64 maxWaitTimeMicros = MUSCLE_TIME_NEVER);

   /** Called within the child process, just before the child process's
     * executable image is loaded in.  Default implementation is a no-op
     * that always just returns B_NO_ERROR.
     * @note This method is not called when running under Windows!
     * @return B_NO_ERROR on success, or an error code on failure (in which case the child process will not execute, but rather just exit immediately)
     */
   virtual status_t ChildProcessReadyToRun();

   /** Returns the process ID of the child process. */
#if defined(WIN32) || defined(CYGWIN)
   uint32 GetChildProcessID() const {return (uint32)GetProcessId(_childProcess);}
#else
   pid_t GetChildProcessID() const {return _childPID;}
#endif

   /** Tries to forcibly kill the child process immediately. 
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t KillChildProcess();

   /** Sends the specified signal to the child process.
     * Note that this method is not currently implemented under Windows, 
     * and thus under Windows this method is a no-op that just returns B_UNIMPLEMENTED.
     * @param sigNum a signal number, e.g. SIGINT or SIGHUP.
     * @returns B_NO_ERROR on success, or an error code on failure.
     */
   status_t SignalChildProcess(int sigNum);

   /** Will not return until our child process has exited.
     * If the child process is not currently running, returns immediately.
     * @param maxWaitTime The maximum amount of time to wait, in microseconds.
     *                    Defaults to MUSCLE_TIME_NEVER, indicating no timeout.
     * @returns true B_NO_ERROR the child process is known to be gone, or an
     *          error code otherwise (e.g. B_TIMED_OUT if our timeout period
     *          has elapsed and the child process still hadn't exited)
     */
   status_t WaitForChildProcessToExit(uint64 maxWaitTime = MUSCLE_TIME_NEVER);

   /** Returns true iff our child-process-has-crashed flag was set to true.
     * Note that this flag is set only when WaitForChildProcessToExit() is called
     * (either directly by external code, or implicitly by a call to Close()).
     * The flag is cleared whenever a new child process is launched.
     *
     * @note The windows implementation of this method is based on the child
     *       process's exit code; any exit code that has the two highest bits
     *       set will be interpreted as a crashed process.  That is the
     *       convention for exit codes under Windows, but it is possible
     *       for the child process to flout convention, leading to false
     *       positives or false negatives in this method's return value.
     */
   bool DidChildProcessCrash() const {return _childProcessCrashed;}

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   /** Currently implemented under MacOS/X only, and only if you set 
     * -DMUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES as a compiler-argument.
     * If you'd like the child process to execute with an effective-user-ID of 0/root
     * (and you are okay with the OS presenting a type-in-your-password-for-access
     * dialog before it will permit that to happen), then you can call this method 
     * before calling LaunchChildProcess() to enable that behavior.
     * @param dialogPrompt the text you'd like the user to see in the dialog
     *                     (e.g. "Please enter your password so that MyProgram can do privileged stuff")
     *                     If an empty String is passed, then privileged-mode will not be requested.
     * @note this method only has an effect if called before LaunchChildProcess() is called.
     */
   void SetRequestRootAccessForChildProcessEnabled(const String & dialogPrompt) {_dialogPrompt = dialogPrompt;}
#endif

   /** Convenience method:  acts similar to the POSIX system() call, but
     * implemented internally via a ChildProcessDataIO object.  In particular,
     * this static method will launch the specified process and not return
     * until that process has completed.
     * @param argc Number of items in the (argv) array
     * @param argv A standard argv array for the child process to use
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @param maxWaitTimeMicros If specified, this is the maximum amount of time (in microseconds) 
     *                          that we should wait for the child process to exit before continuing.
     *                          Defaults to MUSCLE_TIME_NEVER, meaning that we will wait indefinitely
     *                          for the child to exit, if necessary.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     * @returns B_NO_ERROR if the child process was launched, or an error code if the child process could not be launched.
     */
   static status_t System(int argc, const char * argv[], ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), uint64 maxWaitTimeMicros = MUSCLE_TIME_NEVER, const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Convenience method:  acts similar to the POSIX system() call, but
     * implemented internally via a ChildProcessDataIO object.  In particular,
     * this static method will launch the specified process and not return
     * until that process has completed.
     * @param argv A list of strings to construct the (argc,argv) from.  The first string should be the executable name, the second string
     *             should be the first argument to the executable, and so on.
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @param maxWaitTimeMicros If specified, this is the maximum amount of time (in microseconds) 
     *                          that we should wait for the child process to exit before continuing.
     *                          Defaults to MUSCLE_TIME_NEVER, meaning that we will wait indefinitely
     *                          for the child to exit, if necessary.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     * @return B_NO_ERROR on success, or an error code if the launch failed.
     */
   static status_t System(const Queue<String> & argv, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), uint64 maxWaitTimeMicros = MUSCLE_TIME_NEVER, const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Convenience method:  acts similar to the POSIX system() call, but
     * implemented internally via a ChildProcessDataIO object.  In particular,
     * this static method will launch the specified process and not return
     * until that process has completed.
     * @param cmdLine The command string to launch (as if typed into a shell)
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.
     * @returns B_NO_ERROR if the child process was launched, or an error code
     *          if the child process could not be launched.
     * @param maxWaitTimeMicros If specified, this is the maximum amount of time (in microseconds) 
     *                          that we should wait for the child process to exit before continuing.
     *                          Defaults to MUSCLE_TIME_NEVER, meaning that we will wait indefinitely
     *                          for the child to exit, if necessary.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                           variables in the child process.
     */
   static status_t System(const char * cmdLine, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(MUSCLE_DEFAULT_CHILD_PROCESS_LAUNCH_FLAGS), uint64 maxWaitTimeMicros = MUSCLE_TIME_NEVER, const char * optDirectory = NULL, const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Convenience method:  launches a child process that will be completely independent of the current process.
     * @param argc Number of items in the (argv) array
     * @param argv A standard argv array for the child process to use
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.  Defaults to no-flags-set.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                                      variables in the child process.
     * @returns B_NO_ERROR if the child process was launched, or an error code if the child process could not be launched.
     */
   static status_t LaunchIndependentChildProcess(int argc, const char * argv[], const char * optDirectory = NULL, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(), const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Convenience method:  launches a child process that will be completely independent of the current process.
     * @param argv A list of strings to construct the (argc,argv) from.  The first string should be the executable name, the second string
     *             should be the first argument to the executable, and so on.
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.  Defaults to no-flags-set.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                                      variables in the child process.
     * @return B_NO_ERROR on success, or an error code if the launch failed.
     */
   static status_t LaunchIndependentChildProcess(const Queue<String> & argv, const char * optDirectory = NULL, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(), const Hashtable<String,String> * optEnvironmentVariables = NULL);

   /** Convenience method:  launches a child process that will be completely independent of the current process.
     * @param cmdLine The command string to launch (as if typed into a shell)
     * @param optDirectory Optional directory path to set the child process's current directory to.  
     *                     Defaults to NULL, which will cause the child process to inherit this process's current directory.
     * @param launchFlags A bit-chord of CHILD_PROCESS_LAUNCH_FLAG_* bit values.  Defaults to no-flags-set.
     * @param optEnvironmentVariables if non-NULL, these key/value pairs will be placed as environment
                                      variables in the child process.
     * @returns B_NO_ERROR if the child process was launched, or an error code if the child process could not be launched.
     */
   static status_t LaunchIndependentChildProcess(const char * cmdLine, const char * optDirectory = NULL, ChildProcessLaunchFlags launchFlags = ChildProcessLaunchFlags(), const Hashtable<String,String> * optEnvironmentVariables = NULL);

private:
   void Close();
   status_t LaunchChildProcessAux(int argc, const void * argv, ChildProcessLaunchFlags launchFlags, const char * optDirectory, const Hashtable<String,String> * optEnvironmentVariables);
#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   status_t LaunchPrivilegedChildProcess(const char ** argv);
#endif
   void DoGracefulChildShutdown();
   const ConstSocketRef & GetChildSelectSocket() const;

   bool _blocking;

   bool _killChildOkay;
   uint64 _maxChildWaitTime;
   int _signalNumber;
   bool _childProcessCrashed;

   bool _childProcessIsIndependent;

#if defined(WIN32) || defined(CYGWIN)
   void IOThreadEntry();
   void IOThreadAbort();
   static DWORD WINAPI IOThreadEntryFunc(LPVOID This) {((ChildProcessDataIO*)This)->IOThreadEntry(); return 0;}
   ::HANDLE _readFromStdout;
   ::HANDLE _writeToStdin;
   ::HANDLE _ioThread;
   ::HANDLE _wakeupSignal;
   ::HANDLE _childProcess;
   ::HANDLE _childThread;
   ConstSocketRef _masterNotifySocket;
   ConstSocketRef _slaveNotifySocket;
   volatile bool _requestThreadExit;
#else
   ConstSocketRef _handle;
   pid_t _childPID;
#endif

#if defined(__APPLE__) && defined(MUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES)
   String _dialogPrompt;
   const void * _authRef;  // Actually of type AuthorizationRef but I don't want to add the necessary MacOS/X #includes to this .h file
   FileDataIO _ioPipe;
#endif

   DECLARE_COUNTED_OBJECT(ChildProcessDataIO);
};
DECLARE_REFTYPES(ChildProcessDataIO);

} // end namespace muscle

#endif
