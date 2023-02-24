/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/UDPSocketDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "dataio/PacketizedProxyDataIO.h"
#include "iogateway/MessageIOGateway.h"
#include "iogateway/MiniPacketTunnelIOGateway.h"
#include "iogateway/PacketTunnelIOGateway.h"
#include "reflector/StorageReflectConstants.h"
#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

static uint32 _recvWhatCounter = 0;
static uint32 _sendWhatCounter = 0;

// Similar to the QueueGatewayMessageReceiver class, but records the from address also
class TestPacketGatewayMessageReceiver : public AbstractGatewayMessageReceiver
{
public:
   /** Default constructor */
   TestPacketGatewayMessageReceiver() {/* empty */}

protected:
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData)
   {
      const IPAddressAndPort & iap = *(static_cast<const IPAddressAndPort *>(userData));
      LogTime(MUSCLE_LOG_TRACE, "RECEIVED MESSAGE from [%s]: (flatSize=" UINT32_FORMAT_SPEC ") (what=" UINT32_FORMAT_SPEC ") ---\n", iap.ToString()(), msg()->FlattenedSize(), msg()->what);
      if (msg()->what == _recvWhatCounter)
      {
         uint32 spamLen = 0;
         String temp;
         if ((msg()->FindString("spam", temp).IsOK())&&(msg()->FindInt32("spamlen", spamLen).IsOK()))
         {
            if (spamLen == temp.Length())
            {
               for (uint32 i=0; i<spamLen; i++)
               {
                  const char c = 'A'+(((char)i)%26);
                  if (temp[i] != c)
                  {
                     LogTime(MUSCLE_LOG_ERROR, "Incoming Message's String was malformed! (i=" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ") (expected %c, got %c\n", i, spamLen, c, temp[i]);
                     ExitWithoutCleanup(10);
                  }
               }
            }
            else
            {
               LogTime(MUSCLE_LOG_ERROR, "Incoming message had wrong spamLen (" UINT32_FORMAT_SPEC " vs " UINT32_FORMAT_SPEC ")\n", spamLen, temp.Length());
               ExitWithoutCleanup(10);
            }
         }
         else
         {
            LogTime(MUSCLE_LOG_ERROR, "Incoming message was malformed!\n");
            ExitWithoutCleanup(10);
         }
         _recvWhatCounter++;
      }
      else
      {
         LogTime(MUSCLE_LOG_ERROR, "Expected incoming what=" UINT32_FORMAT_SPEC ", got " UINT32_FORMAT_SPEC "\n", _recvWhatCounter, msg()->what);
         ExitWithoutCleanup(10);
      }
   }
};


// This is a text based test of the MiniPacketTunnelIOGateway class.  With this test we
// should be able to broadcast Messages of any size over UDP, and (barring UDP lossage)
// they should be received and properly re-assembled by the listeners.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args;
   (void) ParseArgs(argc, argv, args);

   if (args.HasName("fromscript"))
   {
      printf("Called from script, skipping test\n");
      return 0;
   }

   HandleStandardDaemonArgs(args);

   const char * temp;

   uint16 port = 0;
   if (args.FindString("port", &temp).IsOK()) port = (uint16) atoi(temp);
   if (port == 0) port = 9999;

   uint32 mtu = 0;
   if (args.FindString("mtu", &temp).IsOK()) mtu = atol(temp);
   if (mtu == 0) mtu = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET;

   uint32 magic = 0;
   if (args.FindString("magic", &temp).IsOK()) magic = atol(temp);
   if (magic == 0) magic = 666;

   uint64 spamIntervalMicros = 0;
   if (args.FindString("spam", &temp).IsOK())
   {
      const int spamHz = atol(temp);
      spamIntervalMicros = (spamHz > 0) ? MICROS_PER_SECOND/spamHz : 1;
   }

   status_t ret;

   bool useTCP = false;
   DataIORef dio;
   if (args.FindString("tcp", &temp).IsOK())
   {
      useTCP = true;
      ConstSocketRef s;
      IPAddress connectTo = GetHostByName(temp);
      if (connectTo != invalidIP)
      {
         s = Connect(IPAddressAndPort(connectTo, port), NULL, "testpackettunnel", false);
         if (s() == NULL) return 10;
      }
      else
      {
         ConstSocketRef as = CreateAcceptingSocket(port);
         if (as() == NULL)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Could not create TCP socket on port %u!\n", port);
            return 10;
         }
         LogTime(MUSCLE_LOG_INFO, "Awaiting TCP connection on port %u...\n", port);
         s = Accept(as);
         if (s() == NULL)
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Accept() failed!\n");
            return 10;
         }
      }
      dio.SetRef(new PacketizedProxyDataIO(DataIORef(new TCPSocketDataIO(s, false)), mtu));
   }
   else
   {
      ConstSocketRef s = CreateUDPSocket();
      if ((s() == NULL)||(SetUDPSocketBroadcastEnabled(s, true).IsError(ret))||(BindUDPSocket(s, port, NULL, invalidIP, true).IsError(ret)))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error setting up UDP broadcast Socket on port %i! [%s]\n", port, ret());
         return 10;
      }

      UDPSocketDataIO * udpDio = new UDPSocketDataIO(s, false);
      (void) udpDio->SetPacketSendDestination(IPAddressAndPort(broadcastIP_IPv4, port));  // gotta do it this way because SetUDPSocketTarget() would break our incoming messages!
      dio.SetRef(udpDio);
   }

   LogTime(MUSCLE_LOG_INFO, "Packet test running on port %u, mtu=" UINT32_FORMAT_SPEC " magic=" UINT32_FORMAT_SPEC "\n", port, mtu, magic);

   AbstractMessageIOGatewayRef slaveGatewayRef;
   if (args.HasName("usegw"))
   {
#ifdef MUSCLE_ENABLE_ZLIB_ENCODING
      slaveGatewayRef.SetRef(new MessageIOGateway(MUSCLE_MESSAGE_ENCODING_ZLIB_9));
#else
      slaveGatewayRef.SetRef(new MessageIOGateway);
#endif
   }

   const bool testMini = args.HasName("mini");
   LogTime(MUSCLE_LOG_INFO, "Using the %s class for I/O\n", testMini?"MiniPacketTunnelIOGateway":"PacketTunnelIOGateway");
   PacketTunnelIOGateway         gw(slaveGatewayRef, mtu, magic); if (!testMini) gw.SetDataIO(dio);
   MiniPacketTunnelIOGateway minigw(slaveGatewayRef, mtu, magic); if  (testMini) minigw.SetDataIO(dio);
   TestPacketGatewayMessageReceiver receiver;

   // Just so our event loop can keep going, so we can still print status messages if we're getting 100% spammed
   gw.SetSuggestedMaximumTimeSlice(MillisToMicros(500));
   minigw.SetSuggestedMaximumTimeSlice(MillisToMicros(500));

   AbstractMessageIOGateway * gateway;
   if (testMini) gateway = &minigw;
            else gateway = &gw;

   SocketMultiplexer multiplexer;

   uint64 nextSpamTime = 0;
   LogTime(MUSCLE_LOG_INFO, "%s Event loop starting [%s]...\n", useTCP?"TCP":"UDP", (spamIntervalMicros>0) ? "Broadcast mode" : "Receive mode");
   const int readFD  = dio()->GetReadSelectSocket().GetFileDescriptor();
   const int writeFD = dio()->GetWriteSelectSocket().GetFileDescriptor();
   uint64 lastTime = 0;
   while(1)
   {
      if (OnceEvery(MICROS_PER_SECOND, lastTime)) LogTime(MUSCLE_LOG_INFO, "Send counter is currently at " UINT32_FORMAT_SPEC ", Receive counter is currently at " UINT32_FORMAT_SPEC "\n", _sendWhatCounter, _recvWhatCounter);
      multiplexer.RegisterSocketForReadReady(readFD);
      if (gateway->HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(writeFD);

      status_t ret;
      if (multiplexer.WaitForEvents((spamIntervalMicros>0)?nextSpamTime:MUSCLE_TIME_NEVER).IsError(ret)) LogTime(MUSCLE_LOG_CRITICALERROR, "testpackettunnel: WaitForEvents() failed! [%s]\n", ret());

      const bool reading    = multiplexer.IsSocketReadyForRead(readFD);
      const bool writing    = multiplexer.IsSocketReadyForWrite(writeFD);
      const bool writeError = ((writing)&&(gateway->DoOutput().IsError()));
      const bool readError  = ((reading)&&(gateway->DoInput(receiver).IsError()));
      if ((readError)||(writeError))
      {
         LogTime(MUSCLE_LOG_INFO, "%s:  Connection closed, exiting (%i,%i).\n", readError?"Read Error":"Write Error",readError,writeError);
         break;
      }

      if (spamIntervalMicros > 0)
      {
         const uint64 now = GetRunTime64();
         if (now >= nextSpamTime)
         {
            nextSpamTime = now + spamIntervalMicros;
            const uint32 numMessages = rand() % 10;

            LogTime(MUSCLE_LOG_TRACE, "Spam! (" UINT32_FORMAT_SPEC " messages, counter=" UINT32_FORMAT_SPEC ")\n", numMessages, _sendWhatCounter);

            uint32 byteCount = 0;
            while((gateway->GetOutgoingMessageQueue().GetNumItems() < 100)&&(byteCount < mtu*5))
            {
               MessageRef m = GetMessageFromPool(_sendWhatCounter++);
               if (m())
               {
                  uint32 spamLen = ((rand()%5)==0)?(rand()%(mtu*50)):(rand()%(mtu/5));
                  if (testMini) spamLen = muscleMin(spamLen, mtu-128);  // the mini gateway will drop packets that are too large, which will mess up our test, so don't send large

                  char * tmp = new char[spamLen+1];
                  for (uint32 j=0; j<spamLen; j++) tmp[j] = 'A'+(((char)j)%26);
                  tmp[spamLen] = '\0';
                  if (m()->AddString("spam", tmp).IsError()) MWARN_OUT_OF_MEMORY;
                  if (m()->AddInt32("spamlen", spamLen).IsError()) MWARN_OUT_OF_MEMORY;
                  LogTime(MUSCLE_LOG_TRACE, "ADDING OUTGOING MESSAGE what=" UINT32_FORMAT_SPEC " size=" UINT32_FORMAT_SPEC "\n", m()->what, m()->FlattenedSize());
                  if (gateway->AddOutgoingMessage(m).IsError()) MWARN_OUT_OF_MEMORY;
                  delete [] tmp;
                  byteCount += m()->FlattenedSize();
               }
               else break;
            }
         }
      }
   }

   return 0;
}
