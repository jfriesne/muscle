/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleTCPSocketDataIO_h
#define MuscleTCPSocketDataIO_h

#include "support/MuscleSupport.h"

#include "dataio/DataIO.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

#ifndef MUSCLE_DEFAULT_TCP_STALL_TIMEOUT
/** The default number of microseconds that MUSCLE should allow a TCP connection to remain in a "stalled" state for, before giving up and closing it.  Defaults to three minutes, but the default may be overridden at compile-time via e.g. -DMUSCLE_DEFAULT_TCP_STALL_TIMEOUT=MinutesToMicros(5) or similar. */
# define MUSCLE_DEFAULT_TCP_STALL_TIMEOUT MinutesToMicros(3)  // 3 minutes is our default timeout period
#endif

/**
 *  Data I/O to and from a TCP socket! 
 */
class TCPSocketDataIO : public DataIO
{
public:
   /**
    *  Constructor.
    *  @param sock The ConstSocketRef we should use for our I/O.
    *  @param blocking specifies whether to use blocking or non-blocking socket I/O.
    *  If you will be using this object with a AbstractMessageIOGateway,
    *  and/or select(), then it's usually better to set blocking to false.
    */
   TCPSocketDataIO(const ConstSocketRef & sock, bool blocking);

   /** Destructor.
    *  Closes the socket descriptor, if necessary.
    */
   virtual ~TCPSocketDataIO();

   virtual int32 Read(void * buffer, uint32 size) {return ReceiveData(_sock, buffer, size, _blocking);}
   virtual int32 Write(const void * buffer, uint32 size) {return SendData(_sock, buffer, size, _blocking);}

   /**
    * Stall limit for TCP streams is 180000000 microseconds (aka 3 minutes) by default.
    * Or change it by calling SetOutputStallLimit().
    */
   virtual uint64 GetOutputStallLimit() const {return _stallLimit;}

   /** Set a new output stall time limit.  Set to MUSCLE_TIME_NEVER to disable stall limiting.
     * @param limit the new time-limit, in microseconds, or MUSCLE_TIME_NEVER
     */
   void SetOutputStallLimit(uint64 limit) {_stallLimit = limit;}

   /**
    * Flushes the output buffer by turning off Nagle's Algorithm and then turning it back on again.
    * If Nagle's Algorithm is disabled, then this call is a no-op (since there is never anything to flush)
    */
   virtual void FlushOutput();
   
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
    * @return B_NO_ERROR on success, or an error code on error.
    */
   status_t SetBlockingIOEnabled(bool blocking);

   /**
    * Turns Nagle's algorithm (output packet buffering/coalescing) on or off.
    * @param enabled If true, data will be held momentarily before sending, to allow for bigger packets.
    *                If false, each Write() call will cause a new packet to be sent immediately.
    * @return B_NO_ERROR on success, or an error code on error.
    */
   status_t SetNaglesAlgorithmEnabled(bool enabled);

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

   DECLARE_COUNTED_OBJECT(TCPSocketDataIO);
};
DECLARE_REFTYPES(TCPSocketDataIO);

} // end namespace muscle

#endif
