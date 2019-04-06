#include "dataio/TCPSocketDataIO.h"
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

using namespace muscle;

void HandleSession(const ConstSocketRef & sock, bool myTurnToThrow, bool doFlush)
{
   LogTime(MUSCLE_LOG_ERROR, "Beginning catch session (%s)\n", doFlush?"flush enabled":"flush disabled");

   TCPSocketDataIO sockIO(sock, false);
   uint64 lastThrowTime = 0;
   uint64 min=((uint64)-1), max=0;
   uint64 lastPrintTime = 0;
   uint64 count = 0;
   uint64 total = 0;
   uint8 ball = 'B';  // this is what we throw back and forth over the TCP socket!
   SocketMultiplexer multiplexer;
   while(1)
   {
      const int fd = sock.GetFileDescriptor();
      multiplexer.RegisterSocketForReadReady(fd);
      if (myTurnToThrow) multiplexer.RegisterSocketForWriteReady(fd);

      if (multiplexer.WaitForEvents() < 0)
      {
         LogTime(MUSCLE_LOG_ERROR, "WaitForEvents() failed, aborting!\n");
         break;
      }

      if ((myTurnToThrow)&&(multiplexer.IsSocketReadyForWrite(fd)))
      {
         const int32 bytesWritten = sockIO.Write(&ball, sizeof(ball));
         if (bytesWritten == sizeof(ball))
         {
            if (doFlush) sockIO.FlushOutput();   // nagle's algorithm gets toggled here!
            lastThrowTime = GetRunTime64();
            myTurnToThrow = false;  // we thew the ball, now wait to catch it again!
         }
         else if (bytesWritten < 0)
         {
            LogTime(MUSCLE_LOG_ERROR, "Error sending ball, aborting!\n");
            break;
         }
      }

      if (multiplexer.IsSocketReadyForRead(fd))
      {
         const int32 bytesRead = sockIO.Read(&ball, sizeof(ball));
         if (bytesRead == sizeof(ball))
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
         else if (bytesRead < 0)
         {
            LogTime(MUSCLE_LOG_ERROR, "Error reading ball, aborting!\n");
            break;
         }
      }
   }
}

// This program helps me test whether or not the host OS supports TCPSocketDataIO::FlushOutput() properly or not.
// The two instances of it play "catch" with a byte over a TCP socket, and it measures how fast it takes the
// byte to make each round-trip, and prints statistics about it.
int main(int argc, char ** argv)
{
   const bool doFlush = (strcmp(argv[argc-1], "flush") == 0);
   if (doFlush) argc--;

   const uint16 TEST_PORT = 15000;
   CompleteSetupSystem css;
   if (argc > 1)
   {
      ConstSocketRef s = Connect(argv[1], TEST_PORT, "testnagle");
      if (s()) HandleSession(s, true, doFlush);
   }
   else
   {
      const uint16 port = TEST_PORT;
      ConstSocketRef as = CreateAcceptingSocket(port);
      if (as())
      {
         LogTime(MUSCLE_LOG_INFO, "testnagle awaiting incoming TCP connections on port %u.\n", port);
         ConstSocketRef s = Accept(as);
         if (s()) HandleSession(s, false, doFlush);
      }
      else LogTime(MUSCLE_LOG_CRITICALERROR, "Could not bind to TCP port %u (already in use?)\n", port);
   }
   return 0;
}
