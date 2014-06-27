/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTCPSocketDataIO_h
#define MuscleTCPSocketDataIO_h

#include "support/MuscleSupport.h"

#include "dataio/DataIO.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

#ifndef MUSCLE_DEFAULT_TCP_STALL_TIMEOUT
# define MUSCLE_DEFAULT_TCP_STALL_TIMEOUT MinutesToMicros(3)  // 3 minutes is our default timeout period
#endif

/**
 *  Data I/O to and from a TCP socket! 
 */
class TCPSocketDataIO : public DataIO, private CountedObject<TCPSocketDataIO>
{
public:
   /**
    *  Constructor.
    *  @param sock The ConstSocketRef we should use for our I/O.
    *  @param blocking specifies whether to use blocking or non-blocking socket I/O.
    *  If you will be using this object with a AbstractMessageIOGateway,
    *  and/or select(), then it's usually better to set blocking to false.
    */
   TCPSocketDataIO(const ConstSocketRef & sock, bool blocking) : _sock(sock), _blocking(true), _naglesEnabled(true), _stallLimit(MUSCLE_DEFAULT_TCP_STALL_TIMEOUT)
   {
      (void) SetBlockingIOEnabled(blocking);
   }

   /** Destructor.
    *  Closes the socket descriptor, if necessary.
    */
   virtual ~TCPSocketDataIO() 
   {
      Shutdown();
   }

   virtual int32 Read(void * buffer, uint32 size) {return ReceiveData(_sock, buffer, size, _blocking);}
   virtual int32 Write(const void * buffer, uint32 size) {return SendData(_sock, buffer, size, _blocking);}

   /**
    *  This method implementation always returns B_ERROR, because you can't seek on a socket!
    */
   virtual status_t Seek(int64 /*seekOffset*/, int /*whence*/) {return B_ERROR;}

   /** Always returns -1, since a socket has no position to speak of */
   virtual int64 GetPosition() const {return -1;}

   /**
    * Stall limit for TCP streams is 180000000 microseconds (aka 3 minutes) by default.
    * Or change it by calling SetOutputStallLimit().
    */
   virtual uint64 GetOutputStallLimit() const {return _stallLimit;}

   /** Set a new output stall time limit.  Set to MUSCLE_TIME_NEVER to disable stall limiting.  */
   void SetOutputStallLimit(uint64 limit) {_stallLimit = limit;}

   /**
    * Flushes the output buffer by turning off Nagle's Algorithm and then turning it back on again.
    * If Nagle's Algorithm is disabled, then this call is a no-op (since there is never anything to flush)
    */
   virtual void FlushOutput()
   {
      if ((_sock())&&(_naglesEnabled))
      {
         SetSocketNaglesAlgorithmEnabled(_sock, false);
#ifndef __linux__
         (void) SendData(_sock, NULL, 0, _blocking);  // Force immediate buffer flush (not necessary under Linux)
#endif
         SetSocketNaglesAlgorithmEnabled(_sock, true);
      }
   }
   
   /**
    * Closes our socket connection
    */
   virtual void Shutdown() {_sock.Reset();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _sock;}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _sock;}

   /**
    * Enables or diables blocking I/O on this socket.
    * If this object is to be used by an AbstractMessageIOGateway,
    * then non-blocking I/O is usually better to use.
    * @param blocking If true, socket is set to blocking I/O mode.  Otherwise, non-blocking I/O.
    * @return B_NO_ERROR on success, B_ERROR on error.
    */
   status_t SetBlockingIOEnabled(bool blocking)
   {
      status_t ret = SetSocketBlockingEnabled(_sock, blocking);
      if (ret == B_NO_ERROR) _blocking = blocking;
      return ret;
   }

   /**
    * Turns Nagle's algorithm (output packet buffering/coalescing) on or off.
    * @param enabled If true, data will be held momentarily before sending, to allow for bigger packets.
    *                If false, each Write() call will cause a new packet to be sent immediately.
    * @return B_NO_ERROR on success, B_ERROR on error.
    */
   status_t SetNaglesAlgorithmEnabled(bool enabled)
   {
      status_t ret = SetSocketNaglesAlgorithmEnabled(_sock, enabled);
      if (ret == B_NO_ERROR) _naglesEnabled = enabled;
      return ret;
   }

   /** Returns true iff our socket is set to use blocking I/O (as specified in
    *  the constructor or in our SetBlockingIOEnabled() method)
    */
   bool IsBlockingIOEnabled() const {return _blocking;}

   /** Returns true iff our socket has Nagle's algorithm enabled (as specified
    *  in our SetNaglesAlgorithmEnabled() method.  Default state is true.
    */
   bool IsNaglesAlgorithmEnabled() const {return _naglesEnabled;}

private:
   ConstSocketRef _sock;
   bool _blocking;
   bool _naglesEnabled;
   uint64 _stallLimit;
};

}; // end namespace muscle

#endif
