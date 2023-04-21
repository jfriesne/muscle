#include "dataio/TCPSocketDataIO.h"
#include "system/SetupSystem.h"
#include "system/Thread.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

status_t HandleSession(const ConstSocketRef & sock, bool myTurnToThrow, bool doFlush, uint64 testDuration)
{
   LogTime(MUSCLE_LOG_INFO, "Beginning catch session (%s) sock=%i\n", doFlush?"flush enabled":"flush disabled", sock.GetFileDescriptor());

   const uint64 endTime = (testDuration == MUSCLE_TIME_NEVER) ? MUSCLE_TIME_NEVER : (GetRunTime64()+testDuration);

   TCPSocketDataIO sockIO(sock, false);
   uint64 lastThrowTime = 0;
   uint64 min=((uint64)-1), max=0;
   uint64 lastPrintTime = 0;
   uint64 count = 0;
   uint64 total = 0;
   uint8 ball = 'B';  // this is what we throw back and forth over the TCP socket!
   SocketMultiplexer multiplexer;
   while(GetRunTime64() < endTime)
   {
      const int fd = sock.GetFileDescriptor();
      (void) multiplexer.RegisterSocketForReadReady(fd);
      if (myTurnToThrow) (void) multiplexer.RegisterSocketForWriteReady(fd);

      MRETURN_ON_ERROR(multiplexer.WaitForEvents(endTime));

      if ((myTurnToThrow)&&(multiplexer.IsSocketReadyForWrite(fd)))
      {
         const io_status_t bytesWritten = sockIO.Write(&ball, sizeof(ball));
         if (bytesWritten.GetByteCount() == sizeof(ball))
         {
            if (doFlush) sockIO.FlushOutput();   // nagle's algorithm gets toggled here!
            lastThrowTime = GetRunTime64();
            myTurnToThrow = false;  // we thew the ball, now wait to catch it again!
         }
         else if (bytesWritten.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "Error sending ball, aborting! [%s]\n", bytesWritten.GetStatus()());
            return bytesWritten.GetStatus();
         }
      }

      if (multiplexer.IsSocketReadyForRead(fd))
      {
         const io_status_t bytesRead = sockIO.Read(&ball, sizeof(ball));
         if (bytesRead.GetByteCount() == sizeof(ball))
         {
            if (myTurnToThrow == false)
            {
               if (lastThrowTime > 0)
               {
                  const uint64 elapsedTime = GetRunTime64() - lastThrowTime;
                  count++;
                  total += elapsedTime;
                  min = muscleMin(min, elapsedTime);
                  max = muscleMax(max, elapsedTime);
                  if (OnceEvery(MICROS_PER_SECOND, lastPrintTime)) LogTime(MUSCLE_LOG_INFO, "count=" UINT64_FORMAT_SPEC " min=" UINT64_FORMAT_SPEC "us max=" UINT64_FORMAT_SPEC "us avg=" UINT64_FORMAT_SPEC "us\n", count, min, max, total/count);
               }
               myTurnToThrow = true;  // we caught the ball, now throw it back!
            }
         }
         else if (bytesRead.IsError())
         {
            LogTime(MUSCLE_LOG_ERROR, "Error reading ball, aborting! [%s]\n", bytesRead.GetStatus()());
            return bytesRead.GetStatus();
         }
      }
   }
   return B_NO_ERROR;
}

class NaglePingPongThread : public Thread
{
public:
   NaglePingPongThread(const ConstSocketRef & s, bool hasTheBall, bool doFlush)
      : _sock(s)
      , _hasTheBall(hasTheBall)
      , _doFlush(doFlush)
   {
      // empty
   }

   virtual void InternalThreadEntry()
   {
      const status_t ret = HandleSession(_sock, _hasTheBall, _doFlush, SecondsToMicros(2));
      if (ret.IsError()) LogTime(MUSCLE_LOG_ERROR, "NaglePingPongThread %p:   HandleSession() returned [%s]\n", this, ret());
   }

private:
   ConstSocketRef _sock;
   const bool _hasTheBall;
   const bool _doFlush;
};

static status_t DoNagleTest(bool doFlush)
{
   // Note that I'm doing all this explicit socket setup instead of just calling CreateSocketPair()
   // only because I want to be certain that we are using TCP sockets here and not Unix sockets.

   uint16 port = 0;
   ConstSocketRef acceptSock = CreateAcceptingSocket(0, 1, &port);
   MRETURN_ON_ERROR(acceptSock);

   ConstSocketRef sendSock = Connect(IPAddressAndPort(localhostIP, port), "testnagle", "testnagle", false);
   MRETURN_ON_ERROR(sendSock);

   ConstSocketRef recvSock = Accept(acceptSock);
   MRETURN_ON_ERROR(recvSock);

   NaglePingPongThread t1(recvSock, false, doFlush);
   MRETURN_ON_ERROR(t1.StartInternalThread());

   NaglePingPongThread t2(sendSock, true, doFlush);
   MRETURN_ON_ERROR(t2.StartInternalThread());

   MRETURN_ON_ERROR(t1.WaitForInternalThreadToExit());
   MRETURN_ON_ERROR(t2.WaitForInternalThreadToExit());

   return B_NO_ERROR;
}

// This program helps me test whether or not the host OS supports TCPSocketDataIO::FlushOutput() properly or not.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if ((argc > 1)&&(strcmp(argv[1], "fromscript") == 0))
   {
      // For automated testing:  Two threads play "catch" with a byte over a TCP socket, and it measures
      // how fast it takes the byte to make each round-trip, and prints statistics about it for ten seconds.
      status_t ret;

      printf("\nRunning test with FlushOutput disabled...\n");
      if (DoNagleTest(false).IsError(ret)) {LogTime(MUSCLE_LOG_ERROR, "DoNagleTest(false) returned [%s]\n", ret()); return 10;}

      printf("\nRunning test with FlushOutput enabled...\n");
      if (DoNagleTest(true).IsError(ret)) {LogTime(MUSCLE_LOG_ERROR, "DoNagleTest(true) returned [%s]\n", ret()); return 10;}
   }
   else
   {
      // For manual testing (i.e. user launches a separate session in Terminal on one or more machines)
      const uint16 TEST_PORT = 15000;
      const bool doFlush = (strcmp(argv[argc-1], "flush") == 0);
      if (doFlush) argc--;

      status_t ret;
      const uint16 port = TEST_PORT;

      const IPAddress connectTo = (argc > 1) ? IPAddress(argv[1]) : IPAddress();
      if (connectTo.IsValid())
      {
         ConstSocketRef s = Connect(IPAddressAndPort(connectTo, TEST_PORT), "testnagle", "testnagle", false);
         ret = s() ? HandleSession(s, true, doFlush, MUSCLE_TIME_NEVER) : s.GetStatus();
      }
      else
      {
         ConstSocketRef as = CreateAcceptingSocket(port);
         if (as())
         {
            LogTime(MUSCLE_LOG_INFO, "testnagle awaiting incoming TCP connections on port %u.\n", port);
            ConstSocketRef s = Accept(as);
            ret = s() ? HandleSession(s, false, doFlush, MUSCLE_TIME_NEVER) : s.GetStatus();
         }
         else
         {
            LogTime(MUSCLE_LOG_CRITICALERROR, "Could not bind to TCP port %u (already in use?)\n", port);
            ret = as.GetStatus();
         }
      }
      return ret.IsOK() ? 0 : 10;
   }

   return 0;
}

