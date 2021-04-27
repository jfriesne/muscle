/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>
#ifndef WIN32
# include <signal.h>  // for SIGINT
#endif

#include "dataio/ChildProcessDataIO.h"
#include "dataio/StdinDataIO.h"
#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/AbstractReflectSession.h"
#include "reflector/ReflectServer.h"
#include "regex/StringMatcher.h"
#include "system/SetupSystem.h"
#include "util/StringTokenizer.h"

using namespace muscle;

// This session handles communication with a child process that we spawned
class ChildProcessSession : public AbstractReflectSession
{
public:
   /** Constructor
     * @param processLabel a human-readable string to associate with our child process (e.g. "sub0" or "sub1")
     * @param childArgv the argv-vector of arguments that we'll use to launch the child process
     */
   ChildProcessSession(const String & processLabel, const Queue<String> & childArgv) : _processLabel(processLabel), _childArgv(childArgv)
   {
      SetAutoReconnectDelay(SecondsToMicros(1));  // so that if our child process crashes or exits, we will launch a replacement child process after 1 second
   }

   // Overridden to force CreateDataIO() to be called, even though no Socket was passed in to AddNewSession()
   virtual ConstSocketRef CreateDefaultSocket() {return GetInvalidSocket();}

   // This session's "client" will be a child process
   virtual DataIORef CreateDataIO(const ConstSocketRef &)
   {
      ChildProcessDataIORef cpioRef(newnothrow ChildProcessDataIO(false));
      if (cpioRef() == NULL) return DataIORef();

      // When shutting down, we'll give the child process three seconds to clean up, 
      // and if it hasn't exited by then, we'll nuke it from orbit.
#ifdef WIN32
      cpioRef()->SetChildProcessShutdownBehavior(true,     -1, SecondsToMicros(3));
#else
      cpioRef()->SetChildProcessShutdownBehavior(true, SIGINT, SecondsToMicros(3));
#endif

      status_t ret;
      if (cpioRef()->LaunchChildProcess(_childArgv).IsOK(ret))
      {
         LogTime(MUSCLE_LOG_ERROR, "Spawned child process [%s]\n", _processLabel());
         return cpioRef; 
      }
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "Could not launch child process!  [%s]\n", ret());
         return DataIORef();
      }
   }

   // We'll be communicating with the child process via its stdin and stout file handles, and we'll use lines of Plain ASCII text as our communication language
   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGateway * gw = newnothrow PlainTextMessageIOGateway;
      if (gw == NULL) MWARN_OUT_OF_MEMORY;
      return AbstractMessageIOGatewayRef(gw);
   }

   // Called when our child process sent text to its stdout.  We'll just display that text
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * /*ptr*/)
   {
      if ((msg())&&(msg()->what == PR_COMMAND_TEXT_STRINGS))
      {
         const String * nextLine;
         for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &nextLine).IsOK(); i++)
         {
            printf("[%s] said: %s\n", _processLabel(), nextLine->Cstr());
         }
      }
   }

   // Called by the StdinSession, when a text command has come from stdin for our child-process to handle
   virtual void MessageReceivedFromSession(AbstractReflectSession &, const MessageRef & msg, void *) {(void) AddOutgoingMessage(msg);}

   virtual bool ClientConnectionClosed()
   {
      ChildProcessDataIO * cpio = dynamic_cast<ChildProcessDataIO *>(GetDataIO()());
      if (cpio) 
      {
         (void) cpio->WaitForChildProcessToExit(SecondsToMicros(1));  // just so we can accurately report whether it crashed or not
         LogTime(MUSCLE_LOG_WARNING, "Child Process [%s] just %s.  Will re-spawn in one second...\n", _processLabel(), cpio->DidChildProcessCrash()?"crashed":"exited");
      }
      return AbstractReflectSession::ClientConnectionClosed();
   }

   status_t KillChildProcess()
   {
      ChildProcessDataIO * cpio = dynamic_cast<ChildProcessDataIO *>(GetDataIO()());
      return cpio ? cpio->KillChildProcess() : B_BAD_OBJECT;
   }

   const String & GetProcessLabel() const {return _processLabel;}

private:
   const String _processLabel;
   const Queue<String> _childArgv;
};

// This class listens to the stdin file handle.  It is used in both the
// parent/daemonsitter process and in the child processes.
class StdinSession : public AbstractReflectSession
{
public:
   explicit StdinSession(const String & processLabel) : _processLabel(processLabel) {/* empty */}

   /** This is overridden to force CreateDataIO() to be called, even though no Socket was passed in to AddNewSession() */
   virtual ConstSocketRef CreateDefaultSocket() {return GetInvalidSocket();}

   virtual DataIORef CreateDataIO(const ConstSocketRef &)
   {
      DataIO * dio = newnothrow StdinDataIO(false);
      if (dio == NULL) MWARN_OUT_OF_MEMORY;
      return DataIORef(dio);
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGateway * gw = newnothrow PlainTextMessageIOGateway;
      if (gw == NULL) MWARN_OUT_OF_MEMORY;
      return AbstractMessageIOGatewayRef(gw);
   }

   virtual bool ClientConnectionClosed()
   {
      LogTime(MUSCLE_LOG_INFO, "StdinSession %p:  stdin was closed, ending this process.\n", this);
      EndServer();  // we want our process to go away if we lose the stdin/stdout connection to the parent process
      return AbstractReflectSession::ClientConnectionClosed();
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * /*ptr*/)
   {
      if ((msg())&&(msg()->what == PR_COMMAND_TEXT_STRINGS))
      {
         const String * nextCmd;
         for (int32 i=0; msg()->FindString(PR_NAME_TEXT_LINE, i, &nextCmd).IsOK(); i++)
         {
            const String nc = nextCmd->Trim();
            if (nc.IsEmpty()) continue;  // no sense commenting about blank lines
 
            if (nc == "die")
            {
               LogTime(MUSCLE_LOG_INFO, "Ending process [%s]\n", _processLabel());
               EndServer();
            }
            if (nc == "crash")
            {
               LogTime(MUSCLE_LOG_INFO, "Crashing process [%s]\n", _processLabel());
               MCRASH("Deliberate crash");
            }
            else if (nc.StartsWith("echo "))
            {
               LogTime(MUSCLE_LOG_INFO, "Process [%s] echoing:  [%s]\n", _processLabel(), nc.Substring(5).Trim()());
            }
            else if (nc.StartsWith("hey "))
            {
               StringTokenizer tok(nc.Substring(4).Trim()());
               const String targetProcessName = tok();
               const String commandForProcess = tok.GetRemainderOfString();

               status_t ret;
               MessageRef msgToSubProcess = GetMessageFromPool(PR_COMMAND_TEXT_STRINGS);
               if ((msgToSubProcess())&&(msgToSubProcess()->AddString(PR_NAME_TEXT_LINE, commandForProcess).IsOK(ret)))
               {
                  StringMatcher wildcardMatcher(targetProcessName);

                  uint32 sentCount = 0;
                  for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
                  {
                     ChildProcessSession * cps = dynamic_cast<ChildProcessSession *>(iter.GetValue()());
                     if ((cps)&&(wildcardMatcher.Match(cps->GetProcessLabel()))) 
                     {
                        LogTime(MUSCLE_LOG_INFO, "StdinSession for process [%s]:  Sending command [%s] to sub-processes [%s]\n", _processLabel(), commandForProcess(), cps->GetProcessLabel()());
                        cps->MessageReceivedFromSession(*this, msgToSubProcess, NULL);
                        sentCount++;
                     }
                  }
                  if (sentCount == 0) LogTime(MUSCLE_LOG_WARNING, "Couldn't find any child processes with labels matching [%s], command [%s] was not sent.\n", targetProcessName(), commandForProcess());
               }
               else LogTime(MUSCLE_LOG_ERROR, "Couldn't set up PR_COMMAND_TEXT_STRINGS Message.  [%s]\n", ret());
            }
            else if (nc.StartsWith("kill "))
            {
               const String targetProcessName = nc.Substring(5).Trim()();
               StringTokenizer tok(targetProcessName());
               StringMatcher wildcardMatcher(tok());

               uint32 killCount = 0;
               for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
               {
                  ChildProcessSession * cps = dynamic_cast<ChildProcessSession *>(iter.GetValue()());
                  if ((cps)&&(wildcardMatcher.Match(cps->GetProcessLabel()))) 
                  {
                     LogTime(MUSCLE_LOG_INFO, "StdinSession for process [%s]:  Unilaterally killing sub-processes [%s]\n", _processLabel(), cps->GetProcessLabel()());
                     cps->KillChildProcess();
                     killCount++;
                  }
               }
               if (killCount == 0) LogTime(MUSCLE_LOG_WARNING, "Couldn't find any child processes with labels matching [%s] to kill.\n", targetProcessName());
            }
            else LogTime(MUSCLE_LOG_ERROR, "StdinSession for process [%s]:  Could not parse stdin command string [%s]\n", _processLabel(), nc());
         }
      }
   }

private:
   const String _processLabel;  // human-readable description of our own process
};

// Our dummy child-process program.  It just listens for commands from the parent process (sent to it from stdin)
static int DoChildProcess(const String & label, int /*argc*/, char ** /*argv*/)
{
   ReflectServer server;

   status_t ret;
   StdinSession stdinSession(label);
   if (server.AddNewSession(AbstractReflectSessionRef(&stdinSession, false)).IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "Child Process [%s] is running and listening to stdin.\n", label());
      if (server.ServerProcessLoop().IsOK()) LogTime(MUSCLE_LOG_INFO,  "Child Process [%s] event-loop finished.\n", label());
                                        else LogTime(MUSCLE_LOG_ERROR, "Child Process [%s] event-loop exited with an error [%s].\n", label(), ret());
   }
   else LogTime(MUSCLE_LOG_ERROR, "DoChildProcess:  Couldn't add stdin session to ReflectServer! [%s]\n", ret());

   server.Cleanup();

   return 0;
}

// This program demonstrates how to launch a set of child processes, communciate with them,
// and automatically re-launch them after a short delay if they ever crash or exit.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if ((argc > 1)&&(strncmp(argv[1], "subprocess=", 11)==0))
   {
      // We're being launched as a child process, so we'll do the child-process thing
      return DoChildProcess(argv[1]+11, argc, argv);
   }

   LogTime(MUSCLE_LOG_INFO, "The purpose of this program is to demonstrate how a 'daemon babysitter' process\n");
   LogTime(MUSCLE_LOG_INFO, "can launch a number of child processes, and automatically re-launch them if/when\n");
   LogTime(MUSCLE_LOG_INFO, "they crash or exit.\n");
   LogTime(MUSCLE_LOG_INFO, "\n");
   LogTime(MUSCLE_LOG_INFO, "This program accepts commands on stdin; here are some examples to try:\n");
   LogTime(MUSCLE_LOG_INFO, "\n");
   LogTime(MUSCLE_LOG_INFO, "hey sub0 echo hello\n");
   LogTime(MUSCLE_LOG_INFO, "hey sub0 die\n");
   LogTime(MUSCLE_LOG_INFO, "hey sub* die\n");
   LogTime(MUSCLE_LOG_INFO, "kill sub3\n");
   LogTime(MUSCLE_LOG_INFO, "echo hello\n");
   LogTime(MUSCLE_LOG_INFO, "die\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Parent process:  watch stdin (so the user can type in commands) and launch some child processes
   StdinSession stdinSession("launcher");

   status_t ret;
   ReflectServer server;
   if (server.AddNewSession(AbstractReflectSessionRef(&stdinSession, false)).IsOK())
   {
      // Also add some dummy child processes that we will manage and restart
      for (uint32 i=0; i<5; i++)
      {
         const String childProcessLabel = String("sub%1").Arg(i);
         Queue<String> childArgv;
         (void) childArgv.AddTail(argv[0]);
         (void) childArgv.AddTail(String("subprocess=%1").Arg(childProcessLabel));

         AbstractReflectSessionRef childSessionRef(newnothrow ChildProcessSession(childProcessLabel, childArgv));
              if (childSessionRef() == NULL) MWARN_OUT_OF_MEMORY;
         else if (server.AddNewSession(childSessionRef).IsError(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "daemonsitter:  Couldn't add child process #" UINT32_FORMAT_SPEC " [%s]\n", i, ret());
         }
      }
 
      // Then run our event loop
      LogTime(MUSCLE_LOG_INFO, "DaemonSitter parent process is running and listening to stdin.\n");
      if (server.ServerProcessLoop().IsOK(ret)) LogTime(MUSCLE_LOG_INFO,  "DaemonSitter parent process loop finished.\n");
                                           else LogTime(MUSCLE_LOG_ERROR, "DaemonSitter parent process loop exited with an error [%s].\n", ret());
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "daemonsitter:  Couldn't add stdin session!  [%s]\n", ret());

   LogTime(MUSCLE_LOG_INFO, "daemonsitter process exiting.\n");
   server.Cleanup(); 

   return 0;
}
