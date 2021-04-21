/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "iogateway/SignalMessageIOGateway.h"
#include "reflector/AbstractReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/Thread.h"
#include "system/SetupSystem.h"

using namespace muscle;

class TestThread;

// This session's job is solely to watch the internal thread's wakeup-socket
// and handle any received Messages from the main thread.  If we receive
// a NULL Message from the main thread, that means it is time to exit!
class WatchNotifySocketSession : public AbstractReflectSession
{
public:
   WatchNotifySocketSession(ReflectServer & reflectServer, TestThread & testThread) : _reflectServer(reflectServer), _testThread(testThread) {/* empty */}

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      // SignalMessageIOGateway because the main thread doesn't actually serialize Message objects
      // over the main<->child notification socket; rather it just appends the MessageRefs to a Queue
      // and then sends a single byte to let the child thread know when to check the queue.
      SignalMessageIOGatewayRef gwRef(newnothrow SignalMessageIOGateway);
      if (gwRef() == NULL) MWARN_OUT_OF_MEMORY;
      return gwRef;
   }

   // Called when the wakeup-signal-byte is received from the main thread
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

private:
   ReflectServer & _reflectServer;
   TestThread & _testThread;
};

// Represents our child thread
class TestThread : public Thread
{
public:
   TestThread() {/* empty */}

   // Called by our WatchNotifySocketSession when it's time to check for incoming Messages from the main thread
   void HandleEventsFromMainThread(ReflectServer & reflectServer)
   {
      MessageRef msgFromOwner;
      while(WaitForNextMessageFromOwner(msgFromOwner, 0) >= 0)
      {
         if (msgFromOwner())
         {
            printf("Child thread received the following Message from the main thread:\n");
            msgFromOwner()->PrintToStream();
         }
         else
         {
            printf("Child thread received a NULL MessageRef from the main thread -- time to quit!\n");
            reflectServer.EndServer();
         }
      }
   }

protected:
   virtual void InternalThreadEntry()
   {
      printf("Child thread begins!\n");

      ReflectServer reflectServer;

      status_t ret;
      WatchNotifySocketSession wnss(reflectServer, *this);
      if (reflectServer.AddNewSession(AbstractReflectSessionRef(&wnss, false), GetInternalThreadWakeupSocket()).IsOK(ret))
      {
         printf("Child thread running...\n");
         (void) reflectServer.ServerProcessLoop();
         printf("Child thread:  ServerProcessLoop() returned!\n");
      }
      else LogTime(MUSCLE_LOG_ERROR, "Child thread:  Couldn't add WatchNotifySocketSession!  [%s]\n", ret());

      reflectServer.Cleanup();
   }
};

// Called when the wakeup-signal-byte is received from the main thread
void WatchNotifySocketSession :: MessageReceivedFromGateway(const MessageRef & /*msg*/, void * /*userData*/)
{
   _testThread.HandleEventsFromMainThread(_reflectServer);
}

// This program demonstrates running a ReflectServer event-loop in a child thread, and communicating with it from the main thread
int main(int, char **) 
{
   CompleteSetupSystem css;

   status_t ret;

   TestThread t;
   if (t.StartInternalThread().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't spawn child thread!  [%s]\n", ret());
      return 10;
   }

   while(true)
   {
      printf("\nEnter a string to send to the child thread, or enter quit to quit.\n");
      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin) == NULL) break;

      String s = buf; s = s.Trim();
      printf("You typed:  [%s]\n", s());

      if (s == "quit") break;
      else
      {
         MessageRef msg = GetMessageFromPool(1234);
         msg()->AddString("text", s);
         if (t.SendMessageToInternalThread(msg).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Error sending Message to child thread!  [%s]\n", ret());
      }
   }

   printf("Telling child thread to shut down...\n");
   t.ShutdownInternalThread();

   printf("Child thread has exited -- main thread is exiting now -- bye!\n");
   return 0;
}
