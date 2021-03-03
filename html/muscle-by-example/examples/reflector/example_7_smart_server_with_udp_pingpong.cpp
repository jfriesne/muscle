
#include "dataio/UDPSocketDataIO.h"
#include "iogateway/RawDataMessageIOGateway.h"
#include "reflector/StorageReflectSession.h"
#include "reflector/ReflectServer.h"
#include "system/SetupSystem.h"         // for CompleteSetupSystem
#include "syslog/SysLog.h"              // for SetConsoleLogLevel()
#include "util/MiscUtilityFunctions.h"  // for PrintHexBytes()
#include "util/NetworkUtilityFunctions.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program is the same as example_4_smart_server except in this example\n");
   printf("we also add in a UDPPingPongSession that knows how to play the \"UDP ping pong\"\n");
   printf("game from the networkutilityfunctions example folder.\n");
   printf("\n");
   printf("This is just to demonstrate how the high-level API can be expanded to do\n");
   printf("multiple tasks at once, without having to modify the code for the existing tasks.\n");
   printf("\n");
}

static const uint16 SMART_SERVER_TCP_PORT = 9876;  // arbitrary port number for the "smart" server

/** This session will handle our server's one UDP socket */
class UDPPingPongSession : public AbstractReflectSession
{
public:
   UDPPingPongSession() {/* empty */}

   // We want our socket to be a UDP socket that is bound to a port
   virtual ConstSocketRef CreateDefaultSocket()
   {
      ConstSocketRef sock = CreateUDPSocket();

      uint16 udpPort;
      if (BindUDPSocket(sock, 0, &udpPort).IsOK())
      {
         LogTime(MUSCLE_LOG_INFO, "UDP Ping Pong Session is Listening for incoming UDP packets on port %u\n", udpPort);
         return sock;
      }
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "UDPPingPongSession::CreateDefaultSocket(): Couldn't bind UDP socket!?\n");
         return ConstSocketRef();
      }
   }

   // We want our DataIO to be a UDPSocketDataIO
   virtual DataIORef CreateDataIO(const ConstSocketRef & socket)
   {
      DataIORef ret(newnothrow UDPSocketDataIO(socket, false));
      if (ret() == NULL) MWARN_OUT_OF_MEMORY;
      return ret;
   }

   // We want our gateway to be a RawDataMessageIOGateway
   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGatewayRef ret(newnothrow RawDataMessageIOGateway);
      if (ret() == NULL) MWARN_OUT_OF_MEMORY;
      return ret;
   }

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData)
   {
      ByteBufferRef receivedData;
      if (msg()->FindFlat(PR_NAME_DATA_CHUNKS, receivedData).IsOK())
      {
         IPAddressAndPort sourceIAP;
         if (msg()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, sourceIAP).IsOK())
         {
            printf("Received from [%s]:\n", sourceIAP.ToString()());
            PrintHexBytes(receivedData);

            // If we wanted to reply immediately, we could just call AddOutgoingMessage(msg) right here
            // But example_2_udp_pingpong waits 100mS before sending back the reply, so let's do that
            // here as well.  We'll use Pulse() and GetPulseTime() to implement without blocking the
            // server's event loop.
            if (_pendingReplies.Put(msg, GetRunTime64()+MillisToMicros(100)).IsOK()) InvalidatePulseTime(); 
         }
         else LogTime(MUSCLE_LOG_ERROR, "Error, gateway didn't provide the UDP packet's source location?!\n");
      }
   }

   virtual uint64 GetPulseTime(const PulseArgs & args)
   {
      // If we have any _pendingReplies we want a Pulse() at the time of the first one.
      const uint64 ret = AbstractReflectSession::GetPulseTime(args);
      return _pendingReplies.HasItems() ? muscleMin(ret, *_pendingReplies.GetFirstValue()) : ret;
   }

   virtual void Pulse(const PulseArgs & args)
   {
      AbstractReflectSession::Pulse(args);

      while(_pendingReplies.HasItems())
      {
         const uint64 nextSendTime = *_pendingReplies.GetFirstValue();
         if (args.GetCallbackTime() >= nextSendTime)
         {
            MessageRef msgToSend;
            if (_pendingReplies.RemoveFirst(msgToSend).IsOK()) 
            {
               IPAddressAndPort dest; (void) msgToSend()->FindFlat(PR_NAME_PACKET_REMOTE_LOCATION, dest);

               printf("Sending UDP reply to [%s]:\n", dest.ToString()());
               ByteBufferRef msgData; (void) msgToSend()->FindFlat(PR_NAME_DATA_CHUNKS, msgData);
               PrintHexBytes(msgData);

               // Hand the Message off to the RawMessageDataGateway for immediate transmission
               (void) AddOutgoingMessage(msgToSend);
            }
         }
         else break;  // nothing to do yet!?
      }
   }

private:
   Hashtable<MessageRef, uint64> _pendingReplies;  // Message -> timeToSendItAt
};

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Let's enable a bit of debug-output, just to see what the server is doing
   SetConsoleLogLevel(MUSCLE_LOG_DEBUG);

   // This object contains our server's event loop.
   ReflectServer reflectServer;

   // This factory will create a StorageReflectSession object whenever
   // a TCP connection is received on SMART_SERVER_TCP_PORT, and
   // attach the StorageReflectSession to the ReflectServer for use.   
   StorageReflectSessionFactory smartSessionFactory;
   status_t ret;
   if (reflectServer.PutAcceptFactory(SMART_SERVER_TCP_PORT, ReflectSessionFactoryRef(&smartSessionFactory, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't bind to TCP port %u!  (Perhaps a copy of this program is already running?) [%s]\n", SMART_SERVER_TCP_PORT, ret());
      return 5;
   }

   // This UDP session will handle the UDP ping pong games
   UDPPingPongSession udpPingPong;
   if (reflectServer.AddNewSession(AbstractReflectSessionRef(&udpPingPong, false)).IsError(ret))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Couldn't add UDP ping pong session! [%s]\n", ret());
      return 5;
   }
   
   LogTime(MUSCLE_LOG_INFO, "example_7_smart_server_wth_udp_pingpong is listening for incoming TCP connections on port %u\n", SMART_SERVER_TCP_PORT);
   LogTime(MUSCLE_LOG_INFO, "Try running one or more instances of example_5_smart_client to connect/chat/subscribe!\n");
   LogTime(MUSCLE_LOG_INFO, "\n");

   // Our server's event loop will run here -- ServerProcessLoop() will not return until it's time for the server to exit
   if (reflectServer.ServerProcessLoop().IsOK(ret))
   {
       LogTime(MUSCLE_LOG_INFO, "example_7_smart_server_wth_udp_pingpong is exiting normally.\n");
   }
   else LogTime(MUSCLE_LOG_ERROR, "example_7_smart_server_wth_udp_pingpong is exiting due to error [%s].\n", ret());

   // Make sure our server lets go of all of its sessions and factories
   // before they are destroyed (necessary only because we may have 
   // allocated some of them on the stack rather than on the heap)
   reflectServer.Cleanup();

   return 0;
}
