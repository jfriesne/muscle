#include "reflector/DumbReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "syslog/SysLog.h"       // for SetConsoleLogLevel()
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program is the same as example_1_dumb_server except in this version\n");
   printf("we create our own DumbReflectSessionFactory and DumbReflectSession subclasses\n");
   printf("instead of using the ones built in to the MUSCLE codebase.  That way we can\n");
   printf("override all of their methods to print debug output and that way we can see\n");
   printf("when their various methods are called.\n");
   printf("\n");
}

static const uint16 DUMB_SERVER_TCP_PORT = 8765;  // arbitrary port number for the "dumb" server

/** This session subclass is the same as DumbReflectSession except we do a lot of additional
  * logging to stdout, so that you can watch which methods are being called in response to
  * which network events.
  */
class MyDumbReflectSession : public DumbReflectSession
{
public:
   MyDumbReflectSession()
   {
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession Constructor called (this=%p)\n", this);
   }

   virtual ~MyDumbReflectSession() 
   {
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession Destructor called (this=%p)\n",  this);
   }

   virtual status_t AttachedToServer()
   {
      status_t ret = DumbReflectSession::AttachedToServer();  // this call is what attaches us to the ReflectServer
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::AttachedToServer() called -- returning %s (my session ID is " UINT32_FORMAT_SPEC ")\n", this, ret(), GetSessionID());
      return ret;
   }

   virtual ConstSocketRef CreateDefaultSocket()
   {
      ConstSocketRef ret = DumbReflectSession::CreateDefaultSocket();
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::CreateDefaultSocket() called -- returning %p (socket_fd=%i)\n", this, ret(), ret.GetFileDescriptor());
      return ret;
   }

   virtual DataIORef CreateDataIO(const ConstSocketRef & sock)
   {
      DataIORef ret = DumbReflectSession::CreateDataIO(sock);
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::CreateDataIO(%p) called -- returning TCPSocketDataIO %p\n", this, sock(), ret());
      return ret;
   }

   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGatewayRef ret = DumbReflectSession::CreateGateway();
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::CreateGateway() called -- returning MessageIOGateway %p\n", this, ret());
      return ret;
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::MessageReceivedFromGateway(%p,%p) called\n", this, msg(), userData);
      LogTime(MUSCLE_LOG_INFO, "The Message from session #" UINT32_FORMAT_SPEC "'s client is:\n", GetSessionID());
      msg()->PrintToStream();
      DumbReflectSession::MessageReceivedFromGateway(msg, userData);  // will call MessageReceivedFromSession(*this, msg, userData) on all other attached session objects 
   }

   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData)
   {
      printf("\n");
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::MessageReceivedSession(%p,%p,%p) called\n", this, &from, msg(), userData);
      LogTime(MUSCLE_LOG_INFO, "The Message from session #" UINT32_FORMAT_SPEC " is:\n", from.GetSessionID());
      msg()->PrintToStream();
      LogTime(MUSCLE_LOG_INFO, "Forwarding the Message on to our own client (of session #" UINT32_FORMAT_SPEC ")\n", GetSessionID());
      DumbReflectSession::MessageReceivedFromSession(from, msg, userData);  // will call AddOutgoingMesssage(msg) on this session
   }

   virtual bool ClientConnectionClosed()
   {
      bool ret = DumbReflectSession::ClientConnectionClosed();
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::ClientConnectionClosed() called -- returning %i (aka \"%s\")\n", this, ret, ret?"destroy the session":"keep the session anyway");
      return ret;
   }

   virtual void AboutToDetachFromServer()
   {
      LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSession(%p)::AboutToDetachFromServer() called -- session # " UINT32_FORMAT_SPEC " is about to go away!\n", this, GetSessionID());
      DumbReflectSession::AboutToDetachFromServer();  // this call is what detaches us from the ReflectServer
   }
};

/** This factory will create a MyDumbReflectSession object whenever an incoming TCP
  * connection is received.  The MyDumbReflectSession it returns will be attached to
  * the ReflectServer.
  */
class MyDumbReflectSessionFactory : public DumbReflectSessionFactory
{
public:
   MyDumbReflectSessionFactory()          {LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSessionFactory Constructor called (this=%p)\n", this);}
   virtual ~MyDumbReflectSessionFactory() {LogTime(MUSCLE_LOG_INFO, "MyDumbReflectSessionFactory Destructor called (this=%p)\n",  this);}

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo)
   {
      printf("MyDumbReflectSessionFactory::CreateSession() called!  (clientAddress=[%s] factoryInfo=[%s])\n", clientAddress(), factoryInfo.ToString()());
      return AbstractReflectSessionRef(new MyDumbReflectSession);
   }
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // This object contains our server's event loop.
   ReflectServer reflectServer;

   // This factory will create a DumbReflectSession object whenever
   // a TCP connection is received on DUMB_SERVER_TCP_PORT, and
   // attach the DumbReflectSession to the ReflectServer for use.   
   status_t ret;
   MyDumbReflectSessionFactory dumbSessionFactory;
   if (reflectServer.PutAcceptFactory(DUMB_SERVER_TCP_PORT, ReflectSessionFactoryRef(&dumbSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?)  [%s]\n", DUMB_SERVER_TCP_PORT, ret());
      return 5;
   }

   // Here's where our server will spend all of its time
   LogTime(MUSCLE_LOG_INFO, "example_3_annotated_dumb_server is listening for incoming TCP connections on port %u\n", DUMB_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of example_2_dumb_client to connect and chat!\n");
   printf("\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_3_annotated_dumb_server is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_3_annotated_dumb_server is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
