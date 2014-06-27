/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQSocketDataIO_h
#define MuscleQSocketDataIO_h

#include <qsocket.h>
#include "dataio/DataIO.h"

namespace muscle {

/** This class was contributed to the MUSCLE archive by Jonathon Padfield
  * (jpadfield@hotkey.net.au).  It's a good alternative to the QMessageTransceiverThread
  * class if you don't want to use the multi-threaded version of Qt.
  *
  * The QSocket object emits signals whenever it receives data, which you
  * can connect to your Muscle client. eg.
  *                         
  *  connect(muscleSocket, SIGNAL(hostFound()), SLOT(slotHostFound()));
  *  connect(muscleSocket, SIGNAL(connected()), SLOT(slotConnected()));
  *  connect(muscleSocket, SIGNAL(readyRead()), SLOT(slotReadyRead()));
  *  connect(muscleSocket, SIGNAL(bytesWritten(int)), SLOT(slotBytesWritten(int)));
  *
  * You can then use the normal QSocket routines to connect to the server
  * while running all the input through the MessageIO interface.
  *
  * @author Jonathon Padfield
  */
class QSocketDataIO : public DataIO  
{
public: 
   /** Class constructor.
    *  @param newSocket a QSocket object that was allocated off the heap.  This object becomes owner of newSocket.
    */
   QSocketDataIO(QSocket * newSocket) : _socket(newSocket), _socketRef(newSocket?GetConstSocketRefFromPool(newSocket, false):ConstSocketRef()) {/* empty */}

   /** Destructor - deletes the held QSocket (if any) */
   virtual ~QSocketDataIO() {Shutdown();}

   /** Reads up to (size) bytes of new data from the QSocket into (buffer).  
    *  Returns the actual number of bytes placed, or a negative value if there
    *  was an error.
    *  @param buffer Buffer to write the bytes into
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes read, or -1 on error.
    */
   virtual int32 Read(void * buffer, uint32 size) {return _socket ? _socket->readBlock((char *) buffer, size) : -1;}

   /** Takes (size) bytes from (buffer) and pushes them in to the
    *  outgoing I/O stream of the QSocket.  Returns the actual number of bytes
    *  read from (buffer) and pushed, or a negative value if there
    *  was an error.
    *  @param buffer Buffer to read the bytes from.
    *  @param size Number of bytes in the buffer.
    *  @return Number of bytes written, or -1 on error.
    */
   virtual int32 Write(const void * buffer, uint32 size) {return _socket ? _socket->writeBlock((char *) buffer, size) : -1;}

   /**
    * Always returns B_ERROR (you can't seek a QSocket!)
    */
   virtual status_t Seek(int64 /*offset*/, int /*whence*/) {return B_ERROR;}

   /** Always returns -1, since a socket has no position to speak of */
   virtual int64 GetPosition() const {return -1;}

   /**
    * Flushes the output buffer, if possible.
    */
   virtual void FlushOutput() {if (_socket) _socket->flush();}

   /**
    * Closes the connection.  After calling this method, the
    * DataIO object should not be used any more.
    */
   virtual void Shutdown() {if (_socket) _socket->close(); delete _socket; _socket = NULL; _socketRef.Reset();}

   virtual const ConstSocketRef & GetReadSelectSocket()  const {return _socket ? _socketRef : GetNullSocket();}
   virtual const ConstSocketRef & GetWriteSelectSocket() const {return _socket ? _socketRef : GetNullSocket();}

   /**
    * Returns the held QSocket object (in case you need to access it directly for some reason)
    */
   QSocket * GetSocket() {return _socket;}

   /**
    * Releases the held QSocket object into your custody.  After calling this, this
    * QSocketDataIObject no longer has anything to do with the QSocket object and
    * it becomes your responsibility to delete the QSocket.
    */
   void ReleaseSocket() {_socket = NULL; _socketRef.Neutralize();}

private:
   ConstSocketRef _socketRef;
   QSocket * _socket;
};

}; // end namespace muscle

#endif
