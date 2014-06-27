/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef StdinDataIO_h
#define StdinDataIO_h

#include "util/NetworkUtilityFunctions.h"  // for ConstSocketRef, WIN32, etc.

#if defined(WIN32) || defined(CYGWIN)
# include "dataio/DataIO.h"
#else
# include "dataio/FileDescriptorDataIO.h"
#endif

namespace muscle {

/**
 *  This DataIO handles I/O input from the STDIN_FILENO file descriptor.
 *  In order to support non-blocking input on stdin without causing loss of
 *  data sent to stdout, this DataIO object will keep its file descriptor in
 *  blocking mode at all times except when it is actually about to read from
 *  it.  Writing to stdin is not supported, of course.
 */
class StdinDataIO : public DataIO, private CountedObject<StdinDataIO>
{
public:
   /**
    *  Constructor.
    *  @param blocking determines whether to use blocking or non-blocking I/O.
    *  If you will be using this object with a AbstractMessageIOGateway,
    *  and/or select(), then it's usually better to set blocking to false.
    */
   StdinDataIO(bool blocking);

   /** Destructor */
   virtual ~StdinDataIO();

   /** Reads bytes from stdin and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or -1 on error.
    *  @see DataIO::Read()
    */
   virtual int32 Read(void * buffer, uint32 size);

   /** Overridden to always return -1, because you can't write to stdin! */
   virtual int32 Write(const void *, uint32) {return -1;}

   /** Overridden to always return B_ERROR, because you can't seek() stdin! */
   virtual status_t Seek(int64, int) {return B_ERROR;}

   /** Always returns -1, since stdin doesn't have a notion of current position. */
   virtual int64 GetPosition() const {return -1;}

   /** Implemented as a no-op because we don't ever do output on stdin */
   virtual void FlushOutput() {/* empty */}

   virtual void Shutdown();

   /** Returns a socket to select() on to be notified when there is data
     * ready to be read.  Note that this works even under Windows, as long
     * as this object was created with (blocking==false), and the returned
     * socket is only used for select() (call StdinDataIO::Read() to do the 
     * actual data reading)
     */
   virtual const ConstSocketRef & GetReadSelectSocket() const;

   /** Returns a NULL socket -- since you can't write to stdin, there is no point
     * in waiting for space to be available for writing on stdin!
     */
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return GetNullSocket();}

   /** Returns the blocking flag that was passed into our constructor */
   bool IsBlockingIOEnabled() const {return _stdinBlocking;}

private:
   bool _stdinBlocking;

   void Close();

#if defined(WIN32) || defined(CYGWIN)
   ConstSocketRef _masterSocket;
   uint32 _slaveSocketTag;
#else
   FileDescriptorDataIO _fdIO;
#endif
};

}; // end namespace muscle

#endif
