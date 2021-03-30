/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include "dataio/AsyncDataIO.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

namespace muscle {

AsyncDataIO :: ~AsyncDataIO()
{
   ShutdownInternalThread();
}

int32 AsyncDataIO :: Read(void * buffer, uint32 size)
{
   if (IsInternalThreadRunning() == false) {LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::Read()!\n"); return -1;}
   return ReceiveData(GetOwnerWakeupSocket(), buffer, size, false);
}

int32 AsyncDataIO :: Write(const void * buffer, uint32 size)
{
   if (IsInternalThreadRunning() == false) {LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::Write()!\n"); return -1;}
   const int32 ret = SendData(GetOwnerWakeupSocket(), buffer, size, false);
   if (ret > 0) _mainThreadBytesWritten += ret;  // we count the bytes written, so that Seek()/Flush()/Shutdown() commands can be re-spliced into the stream later if necessary
   return ret;
}

status_t AsyncDataIO :: Seek(int64 offset, int whence)
{
   if (IsInternalThreadRunning() == false) {LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::Seek()!\n"); return B_BAD_OBJECT;}

   status_t ret;
   if (_asyncCommandsMutex.Lock().IsOK(ret))
   {
      ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_SEEK, offset, whence));
      _asyncCommandsMutex.Unlock();
      if (ret.IsOK()) NotifyInternalThread();
   }
   return ret;
}

void AsyncDataIO :: FlushOutput()
{
   if (IsInternalThreadRunning()) 
   {
      if (_asyncCommandsMutex.Lock().IsOK())
      {
         const status_t ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_FLUSH));
         _asyncCommandsMutex.Unlock();
         if (ret.IsOK()) NotifyInternalThread();
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::FlushOutput()!\n");
}

void AsyncDataIO :: Shutdown()
{
   if (IsInternalThreadRunning())
   {
      if (_asyncCommandsMutex.Lock().IsOK())
      {
         const status_t ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_SHUTDOWN));
         _asyncCommandsMutex.Unlock();
         if (ret.IsOK()) NotifyInternalThread();
      }
   }
   else ProxyDataIO::Shutdown();
}

void AsyncDataIO :: ShutdownInternalThread(bool waitForThread)
{
   _mainThreadNotifySocket.Reset();  // so that the internal thread will know to exit now
   Thread::ShutdownInternalThread(waitForThread); 
}

status_t AsyncDataIO :: StartInternalThread()
{
   MRETURN_ON_ERROR(CreateConnectedSocketPair(_mainThreadNotifySocket, _ioThreadNotifySocket));
   return Thread::StartInternalThread(); 
}

void AsyncDataIO :: NotifyInternalThread()
{
   const char buf = 'j'; 
   (void) SendData(_mainThreadNotifySocket, &buf, sizeof(buf), false);
}

void AsyncDataIO :: InternalThreadEntry()
{
   bool exitWhenDoneWriting = false;
   bool keepGoing = true;
   uint64 ioThreadBytesWritten = 0;
   AsyncCommand curCmd; 

   char fromMainThreadBuf[4096];
   uint32 fromMainThreadBufReadIdx  = 0;  // which byte to read out of (fromMainThreadBuf) next
   uint32 fromMainThreadBufNumValid = 0;  // how many bytes are currently in (fromMainThreadBuf) (including already-read ones)

   char fromChildIOBuf[4096];
   uint32 fromChildIOBufReadIdx  = 0;  // which byte to read out of (fromChildIOBuf) next
   uint32 fromChildIOBufNumValid = 0;  // how many bytes are currently in (fromChildIOBuf) (including already-read ones)

   // Copy these out to member variables so that WriteToMainThread() can access them
   _fromChildIOBuf         = fromChildIOBuf;
   _fromChildIOBufSize     = sizeof(fromChildIOBuf);
   _fromChildIOBufReadIdx  = &fromChildIOBufReadIdx;
   _fromChildIOBufNumValid = &fromChildIOBufNumValid;

   uint64 pulseTime = MUSCLE_TIME_NEVER;
   SocketMultiplexer multiplexer;
   DataIORef childIORef = GetChildDataIO(); // keep a ref just to make sure it won't get deallocated out from under our feet
   DataIO  * childIO    = childIORef();     // just for convenience
   while(keepGoing)
   {
      int childReadFD  = childIO?childIO->GetReadSelectSocket().GetFileDescriptor():-1;
      int childWriteFD = childIO?childIO->GetWriteSelectSocket().GetFileDescriptor():-1;
      int fromMainFD   = GetInternalThreadWakeupSocket().GetFileDescriptor();
      int notifyFD     = _ioThreadNotifySocket.GetFileDescriptor();

      if ((childReadFD  >= 0)&&(fromChildIOBufNumValid    < sizeof(fromChildIOBuf)))   multiplexer.RegisterSocketForReadReady(childReadFD);
      if ((childWriteFD >= 0)&&(fromMainThreadBufNumValid > fromMainThreadBufReadIdx)) multiplexer.RegisterSocketForWriteReady(childWriteFD);

      if (fromMainFD >= 0)
      {
         if (fromMainThreadBufNumValid < sizeof(fromMainThreadBuf)) multiplexer.RegisterSocketForReadReady(fromMainFD);
         if (fromChildIOBufNumValid > fromChildIOBufReadIdx)        multiplexer.RegisterSocketForWriteReady(fromMainFD);
      }
      if (notifyFD >= 0) multiplexer.RegisterSocketForReadReady(notifyFD);  // always be on the lookout for notifications...

      pulseTime = InternalThreadGetPulseTime(pulseTime);
      if (multiplexer.WaitForEvents(pulseTime) < 0) break; // we block here, waiting for data availability or for the next pulse time
      if (pulseTime != MUSCLE_TIME_NEVER)
      {
         const uint64 now = GetRunTime64();
         if (now >= pulseTime) InternalThreadPulse(pulseTime);
      }

      // All the notify socket needs to do is make WaitForEvents() return.  We just read the junk notify-bytes and ignore them.
      if ((notifyFD >= 0)&&(multiplexer.IsSocketReadyForRead(notifyFD))) 
      {
         char junk[128]; 
         if (ReceiveData(_ioThreadNotifySocket, junk, sizeof(junk), false) < 0) break;
      }

      // Determine how many bytes until the next command in the output stream (we want them to be executed at the same point
      // in the I/O thread's output stream as they were called at in the main thread's output stream)
      uint32 bytesUntilNextCommand = MUSCLE_NO_LIMIT;
      if (_asyncCommandsMutex.Lock().IsOK())
      {
         if (_asyncCommands.HasItems())
         {
            const AsyncCommand & nextCmd = _asyncCommands.Head();
            if (nextCmd.GetStreamLocation() <= ioThreadBytesWritten)
            {
               bytesUntilNextCommand = 0;
               (void) _asyncCommands.RemoveHead(curCmd);
            }
            else bytesUntilNextCommand = (uint32) (nextCmd.GetStreamLocation()-ioThreadBytesWritten);
         }
         else if ((exitWhenDoneWriting)&&(fromMainThreadBufReadIdx == fromMainThreadBufNumValid)) keepGoing = false;
         _asyncCommandsMutex.Unlock();
      }

      if (bytesUntilNextCommand > 0)
      {
         // Read the data from the child FD, into our from-child buffer
         if ((childReadFD >= 0)&&(fromChildIOBufNumValid < sizeof(fromChildIOBuf))&&(multiplexer.IsSocketReadyForRead(childReadFD)))
         {
            const int32 bytesRead = ProxyDataIO::Read(&fromChildIOBuf[fromChildIOBufNumValid], sizeof(fromChildIOBuf)-fromChildIOBufNumValid);
            if (bytesRead >= 0) fromChildIOBufNumValid += bytesRead;
                           else break;
         }

         if (childWriteFD >= 0)
         {
            // Write the data from our from-main-thread buffer, to our child I/O
            const uint32 bytesToWriteToChild = muscleMin(bytesUntilNextCommand, fromMainThreadBufNumValid-fromMainThreadBufReadIdx);
            if ((bytesToWriteToChild > 0)&&(multiplexer.IsSocketReadyForWrite(childWriteFD)))
            {
               const int32 bytesWritten = ProxyDataIO::Write(&fromMainThreadBuf[fromMainThreadBufReadIdx], bytesToWriteToChild);
               if (bytesWritten >= 0)
               {
                  ioThreadBytesWritten         += bytesWritten;
                  fromMainThreadBufReadIdx     += bytesWritten;
                  if (fromMainThreadBufReadIdx == fromMainThreadBufNumValid) fromMainThreadBufReadIdx = fromMainThreadBufNumValid = 0;
               }
               else break;
            }
            if ((fromMainThreadBufNumValid == fromMainThreadBufReadIdx)&&(exitWhenDoneWriting)) break;
         }

         if (fromMainFD >= 0)
         {
            // Read the data from the main thread's socket, into our from-main-thread buffer
            if ((fromMainThreadBufNumValid < sizeof(fromMainThreadBuf))&&(multiplexer.IsSocketReadyForRead(fromMainFD)))
            {
               const int32 bytesRead = ReceiveData(GetInternalThreadWakeupSocket(), &fromMainThreadBuf[fromMainThreadBufNumValid], sizeof(fromMainThreadBuf)-fromMainThreadBufNumValid, false);
               if (bytesRead >= 0) fromMainThreadBufNumValid += bytesRead;
                              else exitWhenDoneWriting = true;
            }

            // Write the data from our from-child-IO buffer, to the main thread's socket
            if ((fromChildIOBufReadIdx < fromChildIOBufNumValid)&&(multiplexer.IsSocketReadyForWrite(fromMainFD)))
            {
               const int32 bytesWritten = SendData(GetInternalThreadWakeupSocket(), &fromChildIOBuf[fromChildIOBufReadIdx], fromChildIOBufNumValid-fromChildIOBufReadIdx, false);
               if (bytesWritten >= 0) 
               {
                  fromChildIOBufReadIdx += bytesWritten;
                  if (fromChildIOBufReadIdx == fromChildIOBufNumValid) fromChildIOBufReadIdx = fromChildIOBufNumValid = 0;
               }
               else break;
            }
         }
      }
      else
      {
         switch(curCmd.GetCommand())
         {
            case ASYNC_COMMAND_SEEK:
               if (childIO) (void) ProxyDataIO::Seek(curCmd.GetOffset(), curCmd.GetWhence());
            break;

            case ASYNC_COMMAND_FLUSH:
               if (childIO) childIO->FlushOutput();
            break;

            case ASYNC_COMMAND_SHUTDOWN:
               if (childIO) childIO->Shutdown();
            break;

            default: 
               LogTime(MUSCLE_LOG_ERROR, "AsyncDataIO:  Unknown ASYNC_COMMAND code %u\n", curCmd.GetCommand());
            break;
         }
      }
   }
}

// Should only be called from inside InternalThreadEntry() (e.g. by InternalThreadPulse())
uint32 AsyncDataIO :: WriteToMainThread(const uint8 * bytes, uint32 numBytes, bool allowPartial)
{
   const uint32 freeSpaceAvailable = _fromChildIOBufSize-(*_fromChildIOBufNumValid);
   if ((allowPartial == false)&&(freeSpaceAvailable < numBytes)) return 0;

   const uint32 numToWrite = muscleMin(numBytes, freeSpaceAvailable);
   memcpy(_fromChildIOBuf+(*_fromChildIOBufNumValid), bytes, numToWrite);
   (*_fromChildIOBufNumValid) += numToWrite;
   return numToWrite;
}

} // end namespace muscle
