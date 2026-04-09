/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef StdinDataIO_h
#define StdinDataIO_h

#include "util/NetworkUtilityFunctions.h"  // for ConstSocketRef

#if defined(WIN32) || defined(CYGWIN)
# include "dataio/DataIO.h"
#else
# include "dataio/FileDescriptorDataIO.h"
#endif

namespace muscle {

/**
 *  This DataIO handles I/O input from the process's stdin stream.
 *  Writing to stdin is not supported, of course, but if you pass in
 *  'true' as the second argument to the constructor, than any data passed
 *  to this class's Write() method will be printed to stdout via fwrite().
 *  @note this class performs heroic measures under Windows to ensure that
 *        StdinDataIO's read-behavior there is the same as under any POSIX OS,
 *        even though Windows does its best to make that difficult.
 */
class StdinDataIO : public DataIO
{
public:
   /**
    *  Constructor.
    *  @param blocking determines whether to use blocking or non-blocking I/O.
    *                  If you will be using this object with a AbstractMessageIOGateway,
    *                  and/or select(), then it's usually better to set blocking to false.
    *  @param writeToStdout If set true, then any data passed to our Write() method
    *                       will be written to stdout via fwrite().  If set false, any data
    *                       passed to our Write() method will be silently discarded.  Defaults to false.
    */
   StdinDataIO(bool blocking, bool writeToStdout=false);

   /** Destructor */
   virtual ~StdinDataIO();

   /** Reads bytes from stdin and places them into (buffer).
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or an error code on error.
    *  @see DataIO::Read()
    */
   virtual io_status_t Read(void * buffer, uint32 size);

   /** If (writeToStdout) was passed as true to the constructor,
     * then we will try to fwrite() the passed-in bytes to stdout.
     * Otherwise we will just return (size) verbatim, without writing the bytes anywhere.
     * @param buffer Pointer to a buffer of data to output
     * @param size The number of bytes pointed to by (buffer)
     * @returns the number of bytes actually written to stdout (if writing to stdout is enabled) or (size) otherwise.
     * @note writes to stdout are expected to be blocking, even if this StdinDataIO is in non-blocking mode.
     */
   virtual io_status_t Write(const void * buffer, uint32 size);

   /** Flushes stdout if (writeToStdout) was specified as true; otherwise this is a no-op. */
   virtual void FlushOutput();

   virtual void Shutdown();

   /** Returns a socket to select() on to be notified when there is data
     * ready to be read.  Note that this works even under Windows, as long
     * as this object was created with (blocking==false), and the returned
     * socket is only used for select() (call StdinDataIO::Read() to do the
     * actual data reading)
     */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetReadSelectSocket() const;

   /** Returns a socket to select() on to be notified when there is space available to write outgoing data to. */
   MUSCLE_NODISCARD virtual const ConstSocketRef & GetWriteSelectSocket() const {return _writeSelectSocket;}

   /** Returns the blocking flag that was passed into our constructor */
   MUSCLE_NODISCARD bool IsBlockingIOEnabled() const {return _stdinBlocking;}

   /** Sets whether outgoing data should be written to stdout.
     * @param writeEnabled true to enable writing to stdout; false to disable it
     */
   void SetWriteToStdoutEnabled(bool writeEnabled) {_writeToStdout = writeEnabled;}

   /** Returns true iff we are set to write outgoing data to stdout, or false if we are set to simply discard outgoing data */
   MUSCLE_NODISCARD bool IsWriteToStdoutEnabled() const {return _writeToStdout;}

private:
   const bool _stdinBlocking;
   bool _writeToStdout;

   void Close();

#if defined(WIN32) || defined(CYGWIN)
   ConstSocketRef _masterSocket;
   uint32 _slaveSocketTag;
#else
   FileDescriptorDataIO _fdIO;
#endif

   ConstSocketRef _writeSelectSocket;

   DECLARE_COUNTED_OBJECT(StdinDataIO);
};
DECLARE_REFTYPES(StdinDataIO);

} // end namespace muscle

#endif
