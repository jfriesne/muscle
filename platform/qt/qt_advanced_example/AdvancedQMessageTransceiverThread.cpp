#include "dataio/TCPSocketDataIO.h"
#include "iogateway/SignalMessageIOGateway.h"

#include "AdvancedQMessageTransceiverThread.h"
#include "ThreadedInternalSession.h"

using namespace muscle;

// Overridden to handle messages coming from the ThreadSupervisorSession (and therefore 
// by extension, from the GUI thread)
void AdvancedThreadWorkerSession :: MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData)
{
   switch(msgRef()->what)
   {
      case ADVANCED_COMMAND_ENDSESSION:
         printf("AdvancedThreadWorkerSession %p got ADVANCED_COMMAND_ENDSESSION Message, ending this session!\n", this);
         EndSession();
      break;

      default:
         ThreadWorkerSession::MessageReceivedFromSession(from, msgRef, userData);
      break;
   }
}

// This factory knows how to create an AdvancedThreadWorkerSession object whenever
// the MUSCLE server receives a TCP connection on our port
class AdvancedThreadWorkerSessionFactory : public ThreadWorkerSessionFactory
{
public:
   AdvancedThreadWorkerSessionFactory() {/* empty */}

   virtual ThreadWorkerSessionRef CreateThreadWorkerSession(const String & loc, const IPAddressAndPort & iap)
   {
      AdvancedThreadWorkerSession * ret = newnothrow AdvancedThreadWorkerSession();
      if (ret) printf("AdvancedThreadWorkerSessionFactory created AdvancedThreadWorkerSession %p for client at loc=[%s] iap=[%s]\n", ret, loc(), iap.ToString()());
          else MWARN_OUT_OF_MEMORY;
      return ThreadWorkerSessionRef(ret);
   }
};

AdvancedQMessageTransceiverThread :: AdvancedQMessageTransceiverThread()
{
   // We want our ThreadWorkerSessions to handle Messages from their remote peers themselves, not just
   // forward all incoming Messages to the GUI thread, so we'll tell the MessageTransceiverThread that.
   SetForwardAllIncomingMessagesToSupervisor(false); 

   // Set up a factory to accept incoming TCP connections on our port, for remote sessions to use to connect to us
   if (PutAcceptFactory(ADVANCED_EXAMPLE_PORT, ThreadWorkerSessionFactoryRef(newnothrow AdvancedThreadWorkerSessionFactory)).IsError()) printf("AdvancedQMessageTransceiverThread ctor:  Error, couldn't create accept-factory on port %i!\n", ADVANCED_EXAMPLE_PORT);
}

status_t AdvancedQMessageTransceiverThread :: AddNewThreadedInternalSession(const MessageRef & args)
{
   return AddNewSession(ThreadWorkerSessionRef(new ThreadedInternalSession(args)));
}

ReflectServerRef AdvancedQMessageTransceiverThread :: CreateReflectServer()
{
   ReflectServerRef rsRef = QMessageTransceiverThread::CreateReflectServer();
   if (rsRef()) rsRef()->SetDoLogging(true);  // so we can see sessions being created/deleted with displaylevel=trace
   return rsRef;
}

ThreadSupervisorSessionRef AdvancedQMessageTransceiverThread :: CreateSupervisorSession()
{
   char buf[20];
   printf("AdvancedQMessageTransceiverThread::CreateSupervisorSession() called in thread %s\n", muscle_thread_id::GetCurrentThreadID().ToString(buf));
   return ThreadSupervisorSessionRef(new AdvancedThreadSupervisorSession);
}

ThreadWorkerSessionRef AdvancedQMessageTransceiverThread :: CreateDefaultWorkerSession()
{
   char buf[20];
   printf("AdvancedQMessageTransceiverThread::CreateDefaultWorkerSession() called in thread %s\n", muscle_thread_id::GetCurrentThreadID().ToString(buf));
   return ThreadWorkerSessionRef(new AdvancedThreadWorkerSession);
}
