#include "dataio/TCPSocketDataIO.h"
#include "reflector/DumbReflectSession.h"
#include "reflector/ReflectServer.h"
#include "iogateway/SignalMessageIOGateway.h"  // useful for integrating our Thread's command/reply communications in to our ReflectServer's socket-set
#include "regex/QueryFilter.h"   // for QueryFilterRef()
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "system/Thread.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program implements a \"dumb\" Message server (as in the\n");
   printf("reflector/example_1_dumb_server.cpp program) except this one\n");
   printf("will also add a session that represents a separate Thread.\n");
   printf("\n");
   printf("Any time a Message is received from a client, that Message will\n");
   printf("be passed to the Thread, which will then wait for a second or two\n");
   printf("(to simulate data processing, and demonstrate that it is asynchronous\n");
   printf("from the main thread's event loop) before passing back a response to\n");
   printf("the client that originated the command Message.\n");
   printf("\n");
}

static const uint16 DUMB_SERVER_TCP_PORT = 8765;  // arbitrary port number for the "dumb" server

static const String MESSAGE_SOURCE_SESSION_ID_NAME = "__messageWasFrom";

class ServerThreadSession : public DumbReflectSession, private Thread
{
public:
   ServerThreadSession()
   {
      // Set up our communication mechanism with our internally held I/O thread
      // Must be done in the constructor so that the ReflectServer's event loop
      // will have access to our signalling socket.
      _gatewayOK = (SetupNotifierGateway().IsOK());
   }

   /** Called during setup, when we are first attached to the ReflectServer */
   virtual status_t AttachedToServer()
   {
      if (_gatewayOK == false) return B_BAD_OBJECT;

      // Only agree to be attached to the server if we can start up our internal thread
      status_t ret;
      return DumbReflectSession::AttachedToServer().IsOK(ret) ? StartInternalThread() : ret;
   }

   /** Called in the main thread whenever our slave thread has a result Message for us to get 
     * from him.  Note that the (signalMsg) Message parameter isn't interesting, as it's just 
     * a dummy Message telling us that we should check our internal-thread-replies-queue now.
     */
   virtual void MessageReceivedFromGateway(const MessageRef & /*signalMsg*/, void *)
   {
      MessageRef ref;
      while(GetNextReplyFromInternalThread(ref) >= 0)
      {
         String replyTo;
         if ((ref())&&(ref()->FindString(MESSAGE_SOURCE_SESSION_ID_NAME, replyTo).IsOK()))
         {
            ref()->RemoveName(MESSAGE_SOURCE_SESSION_ID_NAME);  // might as well clean up after myself
            LogTime(MUSCLE_LOG_INFO, "ServerThreadSession: got Message from my internal thread, sending it back to [%s]\n", replyTo());

            AbstractReflectSessionRef replyToSession = GetSessions().GetWithDefault(&replyTo);
            if (replyToSession()) replyToSession()->MessageReceivedFromSession(*this, ref, NULL);
                             else LogTime(MUSCLE_LOG_WARNING, "Oops, session [%s] doesn't appear to be connected anymore.  Dropping this reply Message.\n", replyTo());
         }
      }
   }

   /** Called whenever we receive a Message from one of our neighboring (i.e. TCP-client) sessions */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData)
   {
      LogTime(MUSCLE_LOG_INFO, "SlaveThreadSession received a Message from a fellow session, handing it off to my internal Thread\n");

      // Add the source session's ID string to the Message, so that we'll know
      // where to send the reply Message to later on!
      msgRef()->AddString(MESSAGE_SOURCE_SESSION_ID_NAME, from.GetSessionIDString());
      SendMessageToInternalThread(msgRef);  // and off it goes for asynchronous processing
   }

   /** Called when we are about to go away -- overridden so we can shut down the slave thread first */
   virtual void AboutToDetachFromServer()
   {
      (void) ShutdownInternalThread();  // important, to avoid race conditions!
      DumbReflectSession::AboutToDetachFromServer();
   }

protected:
   /** Called, in the slave thread, whenever the main thread has a Message to give the slave Thread */
   virtual status_t MessageReceivedFromOwner(const MessageRef & msgRef, uint32 numLeft)
   {
      if (msgRef())
      {
         LogTime(MUSCLE_LOG_ERROR, "Internal Thread now processing a Message (5 seconds to complete!)\n");
         Snooze64(5*1000000);  // simulate a lengthy operation (e.g. disk I/O) for demonstration purposes
         _count++;
         LogTime(MUSCLE_LOG_ERROR, "Internal Thread processing complete!  Tagging the Message with the result (" UINT32_FORMAT_SPEC "), and returning it!\n", _count);

         // Add a tag to the Message -- in real life, you might add the results of the operation or whatnot
         msgRef()->AddInt32("ServerThreadSession's processing-result was", _count);
         SendMessageToOwner(msgRef);

         return B_NO_ERROR;
      }
      else return B_ERROR;  // a NULL (msgRef) means it's time for us (the internal thread) to go away; returning an error code will accomplish our demise
   }

private:
   status_t SetupNotifierGateway()
   {
      // Allocate the sockets we will use to signal the thread...
      const ConstSocketRef & sock = GetOwnerWakeupSocket();  // the socket we will read a byte on when the internal thread has a reply Message ready for us
      if (sock() == NULL) return B_BAD_OBJECT;

      DataIORef dataIORef(newnothrow TCPSocketDataIO(sock, false));
      MRETURN_OOM_ON_NULL(dataIORef());

      AbstractMessageIOGatewayRef gw(newnothrow SignalMessageIOGateway());
      MRETURN_OOM_ON_NULL(gw());

      gw()->SetDataIO(dataIORef);
      SetGateway(gw);
      return B_NO_ERROR;
   }

   bool _gatewayOK;
   uint32 _count;
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the server is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   // This object contains our server's event loop.
   ReflectServer reflectServer;

   // This factory will create a DumbReflectSession object whenever
   // a TCP connection is received on DUMB_SERVER_TCP_PORT, and
   // attach the DumbReflectSession to the ReflectServer for use.   
   DumbReflectSessionFactory dumbSessionFactory;
   status_t ret;
   if (reflectServer.PutAcceptFactory(DUMB_SERVER_TCP_PORT, ReflectSessionFactoryRef(&dumbSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?) [%s]\n", DUMB_SERVER_TCP_PORT, ret());
      return 5;
   }

   // This session will represent the internal thread.
   ServerThreadSession threadSession;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&threadSession, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't set up ServerThreadSession! [%s]\n", ret());
      return 5;
   }

   LogTime(MUSCLE_LOG_INFO, "example_2_dumb_server_with_thread is listening for incoming TCP connections on port %u\n", DUMB_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of reflector/example_2_dumb_client to connect and chat with the thread!\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_2_dumb_server_with_thread is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_2_dumb_server_with_thread is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
