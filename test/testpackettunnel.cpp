/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include "dataio/UDPSocketDataIO.h"
#include "dataio/TCPSocketDataIO.h"
#include "dataio/PacketizedDataIO.h"
#include "iogateway/MessageIOGateway.h"
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
         if ((msg()->FindString("spam", temp) == B_NO_ERROR)&&(msg()->FindInt32("spamlen", spamLen) == B_NO_ERROR))
         {
            if (spamLen == temp.Length())
            {
               for (uint32 i=0; i<spamLen; i++)
               {
                  char c = 'A'+(((char)i)%26);
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


// This is a text based test of the PacketTunnelIOGateway class.  With this test we
// should be able to broadcast Messages of any size over UDP, and (barring UDP lossage)
// they should be received and properly re-assembled by the listeners.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args;
   (void) ParseArgs(argc, argv, args);
   HandleStandardDaemonArgs(args);

   const char * temp;

   uint16 port = 0;
   if (args.FindString("port", &temp) == B_NO_ERROR) port = atol(temp);
   if (port == 0) port = 9999;

   uint32 mtu = 0;
   if (args.FindString("mtu", &temp) == B_NO_ERROR) mtu = atol(temp);
   if (mtu == 0) mtu = MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET;

   uint32 magic = 0;
   if (args.FindString("magic", &temp) == B_NO_ERROR) magic = atol(temp);
   if (magic == 0) magic = 666;

   uint64 spamInterval = 0;
   if (args.FindString("spam", &temp) == B_NO_ERROR)
   {
      int spamHz = atol(temp);
      spamInterval = (spamHz > 0) ? MICROS_PER_SECOND/spamHz : 1;
   }

   bool useTCP = false;
   DataIORef dio;
   if (args.FindString("tcp", &temp) == B_NO_ERROR)
   {
      useTCP = true;
      ConstSocketRef s;
      ip_address connectTo = GetHostByName(temp);
      if (connectTo != invalidIP)
      {
         s = Connect(connectTo, port, NULL, "testpackettunnel", false);
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
      dio.SetRef(new PacketizedDataIO(DataIORef(new TCPSocketDataIO(s, false)), mtu));
   }
   else
   {
      ConstSocketRef s = CreateUDPSocket();
      if ((s() == NULL)||(SetUDPSocketBroadcastEnabled(s, true) != B_NO_ERROR)||(BindUDPSocket(s, port, NULL, invalidIP, true) != B_NO_ERROR))
      {
         LogTime(MUSCLE_LOG_CRITICALERROR, "Error setting up UDP broadcast Socket on port %i!\n", port);
         return 10;
      }

      UDPSocketDataIO * udpDio = new UDPSocketDataIO(s, false);
      udpDio->SetSendDestination(IPAddressAndPort(broadcastIP, port));  // gotta do it this way because SetUDPSocketTarget() would break our incoming messages!
      dio.SetRef(udpDio);
   }

   LogTime(MUSCLE_LOG_INFO, "Packet test running on port %u, mtu=" UINT32_FORMAT_SPEC " magic=" UINT32_FORMAT_SPEC "\n", port, mtu, magic);

   AbstractMessageIOGatewayRef slaveGatewayRef;
   if (args.HasName("usegw")) slaveGatewayRef.SetRef(new MessageIOGateway(MUSCLE_MESSAGE_ENCODING_ZLIB_9));

   PacketTunnelIOGateway gw(slaveGatewayRef, mtu, magic);
   gw.SetDataIO(dio);
   TestPacketGatewayMessageReceiver receiver;

   SocketMultiplexer multiplexer;

   uint64 nextSpamTime = 0;
   LogTime(MUSCLE_LOG_INFO, "%s Event loop starting [%s]...\n", useTCP?"TCP":"UDP", (spamInterval>0) ? "Broadcast mode" : "Receive mode");
   int readFD  = dio()->GetReadSelectSocket().GetFileDescriptor();
   int writeFD = dio()->GetWriteSelectSocket().GetFileDescriptor();
   uint64 lastTime = 0;
   while(1)
   {
      if (OnceEvery(MICROS_PER_SECOND, lastTime)) LogTime(MUSCLE_LOG_INFO, "Send counter is currently at " UINT32_FORMAT_SPEC ", Receive counter is currently at " UINT32_FORMAT_SPEC "\n", _sendWhatCounter, _recvWhatCounter);
      multiplexer.RegisterSocketForReadReady(readFD);
      if (gw.HasBytesToOutput()) multiplexer.RegisterSocketForWriteReady(writeFD);
      if (multiplexer.WaitForEvents((spamInterval>0)?nextSpamTime:MUSCLE_TIME_NEVER) < 0) LogTime(MUSCLE_LOG_CRITICALERROR, "testpackettunnel: WaitForEvents() failed!\n");

      bool reading = multiplexer.IsSocketReadyForRead(readFD);
      bool writing = multiplexer.IsSocketReadyForWrite(writeFD);
      bool writeError = ((writing)&&(gw.DoOutput() < 0));
      bool readError  = ((reading)&&(gw.DoInput(receiver) < 0));
      if ((readError)||(writeError))
      {
         LogTime(MUSCLE_LOG_INFO, "%s:  Connection closed, exiting (%i,%i).\n", readError?"Read Error":"Write Error",readError,writeError);
         break;
      }

      if (spamInterval > 0)
      {
         uint64 now = GetRunTime64();
         if (now >= nextSpamTime)
         {
            nextSpamTime = now + spamInterval;
            uint32 numMessages = rand() % 10;

            LogTime(MUSCLE_LOG_TRACE, "Spam! (" UINT32_FORMAT_SPEC " messages, counter=" UINT32_FORMAT_SPEC ")\n", numMessages, _sendWhatCounter);

            uint32 byteCount = 0;
            while(byteCount < mtu*5)
            {
               MessageRef m = GetMessageFromPool(_sendWhatCounter++);
               if (m())
               {
                  uint32 spamLen = ((rand()%5)==0)?(rand()%(mtu*50)):(rand()%(mtu/5));
                  char * tmp = new char[spamLen+1];
                  for (uint32 j=0; j<spamLen; j++) tmp[j] = 'A'+(((char)j)%26);
                  tmp[spamLen] = '\0';
                  if (m()->AddString("spam", tmp) != B_NO_ERROR) WARN_OUT_OF_MEMORY;
                  if (m()->AddInt32("spamlen", spamLen) != B_NO_ERROR) WARN_OUT_OF_MEMORY;
                  LogTime(MUSCLE_LOG_TRACE, "ADDING OUTGOING MESSAGE what=" UINT32_FORMAT_SPEC " size=" UINT32_FORMAT_SPEC "\n", m()->what, m()->FlattenedSize());
                  if (gw.AddOutgoingMessage(m) != B_NO_ERROR) WARN_OUT_OF_MEMORY;
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
