/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef RS232DataIO_h
#define RS232DataIO_h

#include <errno.h>
#include "dataio/DataIO.h"
#include "util/String.h"
#include "util/Queue.h"

namespace muscle {

/** A serial port DataIO for serial communications.  Note that
 *  this class currently only works under Windows, OS/X and Linux, and offers
 *  only minimal control of the serial parameters (baud rate only at the moment).
 *  On the plus side, it provides a serial-port-socket for use with select(), even under Windows.
 */
class RS232DataIO : public DataIO, private CountedObject<RS232DataIO>
{
public:
   /** Constructor.
    *  @param portName The port to open for this IO gateway to use.
    *  @param baudRate The baud rate to communicate at.
    *  @param blocking If true, I/O will be blocking; else non-blocking.
    */
   RS232DataIO(const char * portName, uint32 baudRate, bool blocking);

   /** Destructor */
   virtual ~RS232DataIO();
   
   virtual int32 Read(void * buffer, uint32 size);

   virtual int32 Write(const void * buffer, uint32 size);

   /** Always returns B_ERROR, since you can't seek on a serial port! */
   virtual status_t Seek(int64, int) {return B_ERROR;}

   /** Always returns -1, since a serial port has no position to speak of */
   virtual int64 GetPosition() const {return -1;}

   /** Doesn't return until all outgoing serial bytes have been sent */
   virtual void FlushOutput();

   /** Closes our held serial port */
   virtual void Shutdown();

   /** Returns a socket that can be select()'d on for notifications of read availability.
    *  Even works under Windows (in non-blocking mode, anyway), despite Microsoft's best efforts 
    *  to make such a thing impossible :^P Note that you should only use this socket with select(); 
    *  to read from the serial port, call Read() instead.
    */
   virtual const ConstSocketRef & GetReadSelectSocket() const {return GetSerialSelectSocket();}

   /** Returns a socket that can be select()'d on for notifications of write availability.
    *  Even works under Windows (in non-blocking mode, anyway), despite Microsoft's best efforts 
    *  to make such a thing impossible :^P Note that you should only use this socket with select(); 
    *  to write to the serial port, call Write() instead.
    */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetSerialSelectSocket();}

   /** Returns true iff we have a valid serial port to communicate through */
   bool IsPortAvailable() const;

   /** Returns a list of serial port names that are present on this machine.
    *  These names may be passed in to the constructor of this class verbatim.
    *  @param retList On success, this list will contain serial port names.
    *  @return B_NO_ERROR on success, or B_ERROR on failure.
    */
   static status_t GetAvailableSerialPortNames(Queue<String> & retList);

private:
   void Close();
   const ConstSocketRef & GetSerialSelectSocket() const;

   bool _blocking;
#if defined(WIN32) || defined(CYGWIN)
   void IOThreadEntry();
   static DWORD WINAPI IOThreadEntryFunc(LPVOID This) {((RS232DataIO*)This)->IOThreadEntry(); return 0;}
   ::HANDLE _handle;
   ::HANDLE _ioThread;
   ::HANDLE _wakeupSignal;
   OVERLAPPED _ovWait;
   OVERLAPPED _ovRead;
   OVERLAPPED _ovWrite;
   ConstSocketRef _masterNotifySocket;
   ConstSocketRef _slaveNotifySocket;
   volatile bool _requestThreadExit;
#else
   ConstSocketRef _handle;
#endif
};

}; // end namespace muscle

#endif
