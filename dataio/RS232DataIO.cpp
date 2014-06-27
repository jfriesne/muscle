/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "dataio/RS232DataIO.h"

#if defined(__APPLE__)
# include <CoreFoundation/CoreFoundation.h>
# include <IOKit/IOKitLib.h>
# include <IOKit/serial/IOSerialKeys.h>
# include <IOKit/IOBSD.h>
#endif

#if defined(WIN32) || defined(__CYGWIN__)
# include <process.h>  // for _beginthreadex()
# include "util/Queue.h"
# define USE_WINDOWS_IMPLEMENTATION
#else
# include <termios.h>
# include <fcntl.h>
#endif

#include "util/NetworkUtilityFunctions.h"

namespace muscle {

RS232DataIO :: RS232DataIO(const char * port, uint32 baudRate, bool blocking) : _blocking(blocking)
#ifdef USE_WINDOWS_IMPLEMENTATION
   , _handle(INVALID_HANDLE_VALUE)
   , _ioThread(INVALID_HANDLE_VALUE)
   , _wakeupSignal(INVALID_HANDLE_VALUE)
   , _requestThreadExit(false)
#endif
{
   bool okay = false;

#ifdef USE_WINDOWS_IMPLEMENTATION
   memset(&_ovWait,  0, sizeof(_ovWait));
   memset(&_ovRead,  0, sizeof(_ovRead));
   memset(&_ovWrite, 0, sizeof(_ovWrite));
   _handle = CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
   if (_handle != INVALID_HANDLE_VALUE)
   {
      SetupComm(_handle, 32768, 32768);
      
      DCB dcb;
      dcb.DCBlength = sizeof(DCB);
      GetCommState((void *)_handle, &dcb);
      
      char modebuf[128]; sprintf(modebuf, "%s baud="UINT32_FORMAT_SPEC" parity=N data=8 stop=1", port, baudRate);
      if (BuildCommDCBA(modebuf, &dcb))
      {
         dcb.fBinary           = 1;
         dcb.fOutX             = 0;
         dcb.fInX              = 0;
         dcb.fErrorChar        = 0xfe;
         dcb.fTXContinueOnXoff = 0;
         dcb.fOutxCtsFlow      = 0;
         dcb.fOutxDsrFlow      = 0;
         dcb.fDtrControl       = DTR_CONTROL_DISABLE;
         dcb.fDsrSensitivity   = 0;
         dcb.fNull             = 0;
         dcb.fRtsControl       = RTS_CONTROL_DISABLE;
         dcb.fAbortOnError     = 0;
         if (SetCommState(_handle, &dcb))
         {
            COMMTIMEOUTS tmout;
            tmout.ReadIntervalTimeout         = MAXDWORD;
            tmout.ReadTotalTimeoutMultiplier  = 0;
            tmout.ReadTotalTimeoutConstant    = 0;
            tmout.WriteTotalTimeoutMultiplier = 0;
            tmout.WriteTotalTimeoutConstant   = 0;
            if ((SetCommTimeouts(_handle, &tmout))&&(SetCommMask(_handle, EV_TXEMPTY|EV_RXCHAR))) 
            {
               if (_blocking == false)
               {
                  // Oops, in non-blocking mode we'll need to spawn a separate thread to manage the I/O.  Fun!
                  _wakeupSignal   = CreateEvent(0, false, false, 0);
                  _ovWait.hEvent  = CreateEvent(0, true, false, 0);
                  _ovRead.hEvent  = CreateEvent(0, true, false, 0);
                  _ovWrite.hEvent = CreateEvent(0, true, false, 0);
                  if ((_wakeupSignal != INVALID_HANDLE_VALUE)&&(_ovWait.hEvent != INVALID_HANDLE_VALUE)&&(_ovRead.hEvent != INVALID_HANDLE_VALUE)&&(_ovWrite.hEvent != INVALID_HANDLE_VALUE)&&(CreateConnectedSocketPair(_masterNotifySocket, _slaveNotifySocket, false) == B_NO_ERROR))
                  {
                     DWORD junkThreadID;
                     typedef unsigned (__stdcall *PTHREAD_START) (void *);
                     if ((_ioThread = (::HANDLE) _beginthreadex(NULL, 0, (PTHREAD_START)IOThreadEntryFunc, this, 0, (unsigned *) &junkThreadID)) != INVALID_HANDLE_VALUE) okay = true;
                  }
               }
               else okay = true;
            }
         }
      }
   }
#else
#  if defined(__BEOS__) || defined(__HAIKU__)
   _handle = GetConstSocketRefFromPool(open(port, O_RDWR | O_NONBLOCK));
#  else
   _handle = GetConstSocketRefFromPool(open(port, O_RDWR | O_NOCTTY));
#  endif
   if (SetSocketBlockingEnabled(_handle, _blocking) == B_NO_ERROR)
   {
      okay = true;

      int fd = _handle.GetFileDescriptor();

      struct termios t;
      tcgetattr(fd, &t);
      switch(baudRate)
      {
         case 1200:
            cfsetospeed(&t, B1200);
            cfsetispeed(&t, B1200);
            break;
         case 9600:
            cfsetospeed(&t, B9600);
            cfsetispeed(&t, B9600);
            break;
         case 19200:
            cfsetospeed(&t, B19200);
            cfsetispeed(&t, B19200);
            break;
         case 38400:
            cfsetospeed(&t, B38400);
            cfsetispeed(&t, B38400);
            break;
         case 57600:
            cfsetospeed(&t, B57600);
            cfsetispeed(&t, B57600);
            break;
         case 115200:
            cfsetospeed(&t, B115200);
            cfsetispeed(&t, B115200);
            break;
         default:
            okay = false;  // unknown baud rate!
            break;
      }
      if (okay)
      {
         t.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG | IEXTEN);
         t.c_iflag &= ~(INPCK | ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
#if !defined(__BEOS__) && !defined(__HAIKU__)
         t.c_iflag &= ~(IMAXBEL);
#endif
         t.c_iflag |= (IGNBRK);
         t.c_cflag &= ~(HUPCL | PARENB | CRTSCTS | CSIZE);
         t.c_cflag |= (CS8 | CLOCAL);
         t.c_oflag &= ~OPOST;
         tcsetattr(fd, TCSANOW, &t);
      }
   }
#endif

   if (okay == false) Close();
}

RS232DataIO :: ~RS232DataIO()
{
   Close();
}

bool RS232DataIO :: IsPortAvailable() const
{
#ifdef USE_WINDOWS_IMPLEMENTATION
   return (_handle != INVALID_HANDLE_VALUE);
#else
   return (_handle.GetFileDescriptor() >= 0);
#endif
} 

void RS232DataIO :: Close()
{
#ifdef USE_WINDOWS_IMPLEMENTATION
   if (_ioThread != INVALID_HANDLE_VALUE)  // if this is valid, _wakeupSignal is guaranteed valid too
   {
      _requestThreadExit = true;                // set the "Please go away" flag
      SetEvent(_wakeupSignal);                  // wake the thread up so he'll check the bool
      WaitForSingleObject(_ioThread, INFINITE); // then wait for him to go away
      ::CloseHandle(_ioThread);                 // fix handle leak
      _ioThread = INVALID_HANDLE_VALUE;
   }
   _masterNotifySocket.Reset();
   _slaveNotifySocket.Reset();
   if (_wakeupSignal   != INVALID_HANDLE_VALUE) {CloseHandle(_wakeupSignal);   _wakeupSignal   = INVALID_HANDLE_VALUE;}
   if (_handle         != INVALID_HANDLE_VALUE) {CloseHandle(_handle);         _handle         = INVALID_HANDLE_VALUE;}
   if (_ovWait.hEvent  != INVALID_HANDLE_VALUE) {CloseHandle(_ovWait.hEvent);  _ovWait.hEvent  = INVALID_HANDLE_VALUE;}
   if (_ovRead.hEvent  != INVALID_HANDLE_VALUE) {CloseHandle(_ovRead.hEvent);  _ovRead.hEvent  = INVALID_HANDLE_VALUE;}
   if (_ovWrite.hEvent != INVALID_HANDLE_VALUE) {CloseHandle(_ovWrite.hEvent); _ovWrite.hEvent = INVALID_HANDLE_VALUE;}
#else
   _handle.Reset();
#endif
}

int32 RS232DataIO :: Read(void *buf, uint32 len)
{
   if (IsPortAvailable())
   {
#ifdef USE_WINDOWS_IMPLEMENTATION
      if (_blocking)
      {
         DWORD actual_read;
         if (ReadFile(_handle, buf, len, &actual_read, 0)) return actual_read;
      }
      else 
      {
         int32 ret = ReceiveData(_masterNotifySocket, buf, len, _blocking);
         if (ret >= 0) SetEvent(_wakeupSignal);  // wake up the thread in case he has more data to give us
         return ret;
      }
#else
      return ReadData(_handle, buf, len, _blocking);
#endif
   }
   return -1;
}

int32 RS232DataIO :: Write(const void *buf, uint32 len)
{
   if (IsPortAvailable())
   {
#ifdef USE_WINDOWS_IMPLEMENTATION
      if (_blocking)
      {
         DWORD actual_write;
         if (WriteFile(_handle, buf, len, &actual_write, 0)) return actual_write;
      }
      else 
      {
         int32 ret = SendData(_masterNotifySocket, buf, len, _blocking);
         if (ret > 0) SetEvent(_wakeupSignal);  // wake up the thread so he'll check his socket for our new data
         return ret;
      }
#else
      return WriteData(_handle, buf, len, _blocking);
#endif
   }
   return -1;
}

void RS232DataIO :: FlushOutput()
{ 
   if (IsPortAvailable()) 
   {
#ifdef USE_WINDOWS_IMPLEMENTATION
      // not implemented yet!
#else 
      int fd = _handle.GetFileDescriptor();
      if (fd >= 0) tcdrain(fd);
#endif
   }
}

const ConstSocketRef & RS232DataIO :: GetSerialSelectSocket() const
{
#ifdef USE_WINDOWS_IMPLEMENTATION
   return _blocking ? GetNullSocket() : _masterNotifySocket;
#else 
   return _handle;
#endif
}

void RS232DataIO :: Shutdown()
{
   Close();
}

status_t RS232DataIO :: GetAvailableSerialPortNames(Queue<String> & retList)
{
#ifdef USE_WINDOWS_IMPLEMENTATION
   // This implementation stolen from PJ Naughter's (pjn@indigo.ie) post 
   // at http://www.codeproject.com/system/enumports.asp
   //
   // Under NT-based versions of Windows, use the QueryDosDevice API, since it's more efficient
   OSVERSIONINFO osvi; osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
   if (GetVersionEx(&osvi)&&(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT))
   {
      char szDevices[65535];
      DWORD dwChars = QueryDosDeviceA(NULL, szDevices, 65535);
      if (dwChars)
      {
         for (uint32 i=0; szDevices[i] != '\0';)
         {
            const char * pszCurrentDevice = &szDevices[i];
            if (strncmp(pszCurrentDevice, "COM", 3) == 0) retList.AddTail(pszCurrentDevice);

            // Go to next NULL character
            while(szDevices[i] != '\0') i++;
            i++; // Bump pointer to the beginning of the next string
         }
         return B_NO_ERROR;
      }
      return B_ERROR;
   }
   else
   {
      // On 95/98 open up each port to determine their existence
      // Up to 255 COM ports are supported so we iterate through all of them seeing
      // if we can open them or if we fail to open them, get an access denied or general error error.
      // Both of these cases indicate that there is a COM port at that number. 
      for (uint32 i=1; i<256; i++)
      {
         // Try to open the port
         char buf[128]; sprintf(buf, "COM"UINT32_FORMAT_SPEC, i);
         ::HANDLE hPort = CreateFileA(buf, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
         if (hPort == INVALID_HANDLE_VALUE)
         {
            DWORD err = GetLastError();
            if ((err == ERROR_ACCESS_DENIED)||(err == ERROR_GEN_FAILURE)) retList.AddTail(buf);  // acceptable errors
         }
         else
         {
            retList.AddTail(buf);  // success!
            CloseHandle(hPort);
         }
      }
      return B_NO_ERROR;
   }
#else
# if defined(__APPLE__)
   mach_port_t masterPort;
   if (IOMasterPort(MACH_PORT_NULL, &masterPort) == KERN_SUCCESS)
   {
      CFMutableDictionaryRef classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
      if (classesToMatch)
      {
         CFDictionarySetValue(classesToMatch, CFSTR(kIOSerialBSDTypeKey), CFSTR(kIOSerialBSDRS232Type));
         io_iterator_t serialPortIterator;
         if (IOServiceGetMatchingServices(masterPort, classesToMatch, &serialPortIterator) == KERN_SUCCESS)
         {
            io_object_t modemService;
            while((modemService = IOIteratorNext(serialPortIterator)) != MACH_PORT_NULL)
            {
               CFTypeRef bsdPathAsCFString = IORegistryEntryCreateCFProperty(modemService, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
               if (bsdPathAsCFString)
               {
                  char bsdPath[256] = "";
                  if (CFStringGetCString((CFStringRef)bsdPathAsCFString, bsdPath, sizeof(bsdPath), kCFStringEncodingASCII)) retList.AddTail(bsdPath);
                  CFRelease(bsdPathAsCFString);
               }
               (void) IOObjectRelease(modemService);
            }
            IOObjectRelease(serialPortIterator);
            return B_NO_ERROR;
         }
      }
   }
   return B_ERROR;  // if we got here, it didn't work
# else
   for (int i=0; /*empty*/; i++)
   {
      char buf[64]; 
#  if defined(__BEOS__) || defined(__HAIKU__)
      sprintf(buf, "/dev/ports/serial%i", i+1);
      int temp = open(buf, O_RDWR | O_NONBLOCK);
#  else
      sprintf(buf, "/dev/ttyS%i", i);
      int temp = open(buf, O_RDWR | O_NOCTTY);
#  endif
      if (temp >= 0)
      {
         close(temp);
         retList.AddTail(buf);
      }
      else break;
   }
   return B_NO_ERROR;
# endif
#endif
}


#ifdef USE_WINDOWS_IMPLEMENTATION

const uint32 SERIAL_BUFFER_SIZE = 1024;

// Used as a temporary holding area for data (since we can't just block, or the serial port
// buffers will overflow and data will be lost!)
class SerialBuffer
{
public:
   SerialBuffer() : _length(0), _index(0) {/* empty */}

   char _buf[SERIAL_BUFFER_SIZE];
   uint32 _length;  // how many bytes in _buf are actually valid
   uint32 _index;   // Index of the next byte to process
};

// Called when we have finished reading some bytes in from the serial port.  Adds the bytes to
// our queue of incoming serial bytes.  (numBytesRead) MUST be <= SERIAL_BUFFER_SIZE !
static void ProcessReadBytes(Queue<SerialBuffer *> & inQueue, const char * inBytes, uint32 numBytesRead)
{
   MASSERT(numBytesRead <= SERIAL_BUFFER_SIZE, "ProcessReadBytes: numBytesRead was too large!");

   SerialBuffer * buf = (inQueue.HasItems()) ? inQueue.Tail() : NULL;
   if ((buf)&&((sizeof(buf->_buf)-buf->_length) >= numBytesRead))
   {
      // this buffer has enough room left to just add the extra bytes into it.
      // No need to allocate a new SerialBuffer!
      memcpy(&buf->_buf[buf->_length], inBytes, numBytesRead);
      buf->_length += numBytesRead;
   }
   else
   {
      // Oops, not enough room.  We'll allocate a new SerialBuffer object instead.
      buf = newnothrow SerialBuffer;
      if ((buf)&&(inQueue.AddTail(buf) == B_NO_ERROR))
      {
         memcpy(&buf->_buf, inBytes, numBytesRead);
         buf->_length = numBytesRead;
      }
      else
      {
         WARN_OUT_OF_MEMORY;
         delete buf;
      }
   }
}

// Called when we have finished writing some bytes out to the serial port.
static void ProcessWriteBytes(SerialBuffer & buf, uint32 numBytesWritten)
{
   buf._index += numBytesWritten;
   if (buf._index == buf._length) buf._index = buf._length = 0;  // the buffer is done, so reset it
}

void RS232DataIO :: IOThreadEntry()
{
   SerialBuffer inBuf;               // bytes from the serial port, waiting to go into the inQueue
   SerialBuffer outBuf;              // bytes from the user socket, waiting to go to the serial port
   Queue<SerialBuffer *> inQueue;    // bytes from inBuf, waiting to go to the user socket

   uint32 pendingReadBytes  = 0;
   uint32 pendingWriteBytes = 0;
   bool isWaiting = false;
   bool checkRead = false;
   ::HANDLE events[] = {_ovWait.hEvent, _ovRead.hEvent, _ovWrite.hEvent, _wakeupSignal};  // order is important!!!
   while(_requestThreadExit == false)
   {
      if (isWaiting == false)
      {
         // Tell the system to start any I/O...
         DWORD eventMask;
         if (WaitCommEvent(_handle, &eventMask, &_ovWait))
         {
            if (eventMask & EV_RXCHAR) checkRead = true;
         }
         else 
         {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING) isWaiting = true;
                                    else LogTime(MUSCLE_LOG_ERROR, "WaitCommEvent() failed! errorCode="INT32_FORMAT_SPEC"\n", err);
         }
      }

      bool doResetEvent = false;
      switch(WaitForMultipleObjects(ARRAYITEMS(events), events, false, INFINITE)-WAIT_OBJECT_0)
      {
         case 0:  // ovWait
         {
            isWaiting    = false;
            doResetEvent = true;
            DWORD eventMask;
            if ((GetCommMask(_handle,&eventMask))&&(eventMask & EV_RXCHAR)) checkRead = true;
         }
         break;

         case 1:  // ovRead
         {
            if (pendingReadBytes > 0)
            {
               ProcessReadBytes(inQueue, inBuf._buf, pendingReadBytes);
               pendingReadBytes = 0;
            }           
            ResetEvent(_ovRead.hEvent);
         }
         break;

         case 2:  // ovWrite
         {
            if (pendingWriteBytes >= 0)
            {
               ProcessWriteBytes(outBuf, pendingWriteBytes);
               pendingWriteBytes = 0;
            }
            ResetEvent(_ovWrite.hEvent);
         }
         break;

         case 3:  // wakeupSignal
            // empty
         break;
      }

 
      // Dump serial data into inQueue as much as possible...
      if ((pendingReadBytes == 0)&&(checkRead))
      {
         while(true)
         {
            int32 numBytesToRead = sizeof(inBuf._buf);
            DWORD numBytesRead;
            if (ReadFile(_handle, inBuf._buf, numBytesToRead, &numBytesRead, &_ovRead))
            {
               if (numBytesRead > 0) ProcessReadBytes(inQueue, inBuf._buf, numBytesRead);
                                else break;
            }
            else
            {
               if (GetLastError() == ERROR_IO_PENDING) pendingReadBytes = numBytesToRead;
               break;
            }
         }
         checkRead = false;
      }

      // Dump inQueue into our slave socket as much as possible (this will signal the user thread, too)
      while(inQueue.HasItems())
      {
         SerialBuffer * buf = inQueue.Head();
         int32 bytesToWrite = buf->_length-buf->_index;
         int32 bytesWritten = (bytesToWrite > 0) ? SendData(_slaveNotifySocket, &buf->_buf[buf->_index], bytesToWrite, false) : 0;
         if (bytesWritten > 0)
         {
            buf->_index += bytesWritten;
            if (buf->_index == buf->_length)
            {
               (void) inQueue.RemoveHead();
               delete buf;
            }
         }
         else break;
      }

      // Dump outgoing data to serial buffer as much as possible
      if (pendingWriteBytes == 0)
      {
         while(1)
         {
            bool keepGoing = false;

            // fill up the outBuf with as many more bytes as possible...
            int32 numBytesToRead = sizeof(outBuf._buf)-outBuf._length;
            int32 numBytesRead = (numBytesToRead > 0) ? ReceiveData(_slaveNotifySocket, &outBuf._buf[outBuf._length], numBytesToRead, false) : 0;
            if (numBytesRead > 0) outBuf._length += numBytesRead;
      
            // Try to write the bytes from outBuf to the serial port
            int32 numBytesToWrite = outBuf._length-outBuf._index;
            if (numBytesToWrite > 0)
            {
               DWORD numBytesWritten;
               if (WriteFile(_handle, &outBuf._buf[outBuf._index], numBytesToWrite, &numBytesWritten, &_ovWrite))
               {
                  if (numBytesWritten > 0) 
                  {
                     ProcessWriteBytes(outBuf, numBytesWritten);
                     keepGoing = true;  // see if we can write some more....
                  }
               }
               else 
               {
                  if (GetLastError() == ERROR_IO_PENDING) pendingWriteBytes = numBytesToWrite;
                                                     else LogTime(MUSCLE_LOG_ERROR, "RS232SerialDataIO: WriteFile() failed!  err="INT32_FORMAT_SPEC"\n", GetLastError());
               }
            }
            if (keepGoing == false) break;
         }
      }

      if (doResetEvent) ResetEvent(_ovWait.hEvent);
   }

   // Clean up!
   for (int32 i=inQueue.GetNumItems()-1;  i>=0; i--) delete inQueue[i];
}
#endif

}; // end namespace muscle
