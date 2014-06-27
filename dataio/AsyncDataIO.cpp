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
   int32 ret = SendData(GetOwnerWakeupSocket(), buffer, size, false);
   if (ret > 0) _mainThreadBytesWritten += ret;  // we count the bytes written, so that Seek()/Flush()/Shutdown() commands can be re-spliced into the stream later if necessary
   return ret;
}

status_t AsyncDataIO :: Seek(int64 offset, int whence)
{
   if (IsInternalThreadRunning() == false) {LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::Seek()!\n"); return B_ERROR;}

   status_t ret = B_ERROR;
   if (_asyncCommandsMutex.Lock() == B_NO_ERROR)
   {
      ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_SEEK, offset, whence));
      _asyncCommandsMutex.Unlock();
      if (ret == B_NO_ERROR) NotifyInternalThread();
   }
   return ret;
}

void AsyncDataIO :: FlushOutput()
{
   if (IsInternalThreadRunning()) 
   {
      if (_asyncCommandsMutex.Lock() == B_NO_ERROR)
      {
         status_t ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_FLUSH));
         _asyncCommandsMutex.Unlock();
         if (ret == B_NO_ERROR) NotifyInternalThread();
      }
   }
   else LogTime(MUSCLE_LOG_ERROR, "StartInternalThread() must be called before calling AsyncDataIO::FlushOutput()!\n");
}

void AsyncDataIO :: Shutdown()
{
   if (IsInternalThreadRunning())
   {
      if (_asyncCommandsMutex.Lock() == B_NO_ERROR)
      {
         status_t ret = _asyncCommands.AddTail(AsyncCommand(_mainThreadBytesWritten, ASYNC_COMMAND_SHUTDOWN));
         _asyncCommandsMutex.Unlock();
         if (ret == B_NO_ERROR) NotifyInternalThread();
      }
   }
   else if (_slaveIO()) 
   {
      _slaveIO()->Shutdown();
      _slaveIO.Reset();
   }
}

status_t AsyncDataIO :: StartInternalThread()
{
   if (CreateConnectedSocketPair(_mainThreadNotifySocket, _ioThreadNotifySocket) != B_NO_ERROR) return B_ERROR;
   return Thread::StartInternalThread(); 
}

void AsyncDataIO :: NotifyInternalThread()
{
   char buf = 'j'; 
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

   char fromSlaveIOBuf[4096];
   uint32 fromSlaveIOBufReadIdx  = 0;  // which byte to read out of (fromSlaveIOBuf) next
   uint32 fromSlaveIOBufNumValid = 0;  // how many bytes are currently in (fromSlaveIOBuf) (including already-read ones)

   SocketMultiplexer multiplexer;
   while(keepGoing)
   {
      int slaveReadFD  = _slaveIO()?_slaveIO()->GetReadSelectSocket().GetFileDescriptor():-1;
      int slaveWriteFD = _slaveIO()?_slaveIO()->GetWriteSelectSocket().GetFileDescriptor():-1;
      int fromMainFD   = GetInternalThreadWakeupSocket().GetFileDescriptor();
      int notifyFD     = _ioThreadNotifySocket.GetFileDescriptor();

      if ((slaveReadFD  >= 0)&&(fromSlaveIOBufNumValid    < sizeof(fromSlaveIOBuf)))   multiplexer.RegisterSocketForReadReady(slaveReadFD);
      if ((slaveWriteFD >= 0)&&(fromMainThreadBufNumValid > fromMainThreadBufReadIdx)) multiplexer.RegisterSocketForWriteReady(slaveWriteFD);

      if (fromMainFD >= 0)
      {
         if (fromMainThreadBufNumValid < sizeof(fromMainThreadBuf)) multiplexer.RegisterSocketForReadReady(fromMainFD);
         if (fromSlaveIOBufNumValid > fromSlaveIOBufReadIdx)        multiplexer.RegisterSocketForWriteReady(fromMainFD);
      }
      if (notifyFD >= 0) multiplexer.RegisterSocketForReadReady(notifyFD);  // always be on the lookout for notifications...

      // we block here, waiting for data availability
      if (multiplexer.WaitForEvents() < 0) break;

      // All the notify socket needs to do is make select() return.  We just read the junk notify-bytes and ignore them.
      char junk[128]; if ((notifyFD >= 0)&&(multiplexer.IsSocketReadyForRead(notifyFD))) (void) ReceiveData(_ioThreadNotifySocket, junk, sizeof(junk), false);

      // Determine how many bytes until the next command in the output stream (we want them to be executed at the same point
      // in the I/O thread's output stream as they were called at in the main thread's output stream)
      uint32 bytesUntilNextCommand = MUSCLE_NO_LIMIT;
      if (_asyncCommandsMutex.Lock() == B_NO_ERROR)
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
         // Read the data from the slave FD, into our from-slave buffer
         if ((slaveReadFD >= 0)&&(fromSlaveIOBufNumValid < sizeof(fromSlaveIOBuf))&&(multiplexer.IsSocketReadyForRead(slaveReadFD)))
         {
            int32 bytesRead = SlaveRead(&fromSlaveIOBuf[fromSlaveIOBufNumValid], sizeof(fromSlaveIOBuf)-fromSlaveIOBufNumValid);
            if (bytesRead >= 0) fromSlaveIOBufNumValid += bytesRead;
                           else break;
         }

         if (slaveWriteFD >= 0)
         {
            // Write the data from our from-main-thread buffer, to our slave I/O
            uint32 bytesToWriteToSlave = muscleMin(bytesUntilNextCommand, fromMainThreadBufNumValid-fromMainThreadBufReadIdx);
            if ((bytesToWriteToSlave > 0)&&(multiplexer.IsSocketReadyForWrite(slaveWriteFD)))
            {
               int32 bytesWritten = SlaveWrite(&fromMainThreadBuf[fromMainThreadBufReadIdx], bytesToWriteToSlave);
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
               int32 bytesRead = ReceiveData(GetInternalThreadWakeupSocket(), &fromMainThreadBuf[fromMainThreadBufNumValid], sizeof(fromMainThreadBuf)-fromMainThreadBufNumValid, false);
               if (bytesRead >= 0) fromMainThreadBufNumValid += bytesRead;
                              else exitWhenDoneWriting = true;
            }

            // Write the data from our from-slave-IO buffer, to the main thread's socket
            if ((fromSlaveIOBufReadIdx < fromSlaveIOBufNumValid)&&(multiplexer.IsSocketReadyForWrite(fromMainFD)))
            {
               int32 bytesWritten = SendData(GetInternalThreadWakeupSocket(), &fromSlaveIOBuf[fromSlaveIOBufReadIdx], fromSlaveIOBufNumValid-fromSlaveIOBufReadIdx, false);
               if (bytesWritten >= 0) 
               {
                  fromSlaveIOBufReadIdx += bytesWritten;
                  if (fromSlaveIOBufReadIdx == fromSlaveIOBufNumValid) fromSlaveIOBufReadIdx = fromSlaveIOBufNumValid = 0;
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
               if (_slaveIO()) _slaveIO()->Seek(curCmd.GetOffset(), curCmd.GetWhence());
            break;

            case ASYNC_COMMAND_FLUSH:
               if (_slaveIO()) _slaveIO()->FlushOutput();
            break;

            case ASYNC_COMMAND_SHUTDOWN:
               if (_slaveIO()) _slaveIO()->Shutdown();
            break;

            default: 
               LogTime(MUSCLE_LOG_ERROR, "AsyncDataIO:  Unknown ASYNC_COMMAND code %u\n", curCmd.GetCommand());
            break;
         }
      }
   }
}

}; // end namespace muscle
