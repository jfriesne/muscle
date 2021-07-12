/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQDataIODevice_h
#define MuscleQDataIODevice_h

#include <qiodevice.h>
#include <qsocketnotifier.h>
#include "dataio/SeekableDataIO.h"

namespace muscle {

/** This adapter class allows you to use a MUSCLE DataIO object as a Qt QIODevice. */
class QDataIODevice : public QIODevice  
{
public: 
   /** Class constructor.
    *  @param dataIO a QSocket object that was allocated off the heap.  This object becomes owner of newSocket.
    *  @param parent Passed to the QIODevice constructor
    */
   QDataIODevice(const DataIORef & dataIO, QObject * parent) : QIODevice(parent), _dataIO(dataIO), _dataSize(dynamic_cast<SeekableDataIO*>(dataIO())?(dynamic_cast<SeekableDataIO*>(dataIO())->GetLength()):-1), _readReady(dataIO()->GetReadSelectSocket().GetFileDescriptor(), QSocketNotifier::Read), _isHosed(false)
   {
      connect(&_readReady, SIGNAL(activated(int)), this, SIGNAL(readyRead()));
   }   

   /** Destructor */
   virtual ~QDataIODevice() {_readReady.setEnabled(false);}

   /** Returns true iff this device is in sequential-access-only mode. */
   virtual bool isSequential() const {return (_dataSize < 0);}

   /** Returns the current read-position of this device. */
   virtual qint64 pos()        const {const SeekableDataIO * sdio = dynamic_cast<const SeekableDataIO *>(_dataIO()); return sdio ? muscleMax((qint64)sdio->GetPosition(), (qint64)0) : 0;}

   /** Returns the total number of bytes available in this device. */
   virtual qint64 size()       const {return isSequential() ? bytesAvailable() : _dataSize;}

   /** Returns true iff this device has reached its End-of-File state. */
   virtual bool atEnd()        const {return isSequential() ? _isHosed : ((_isHosed)||(QIODevice::atEnd()));}

   /** Attempts to read the specified number of bytes from the device.
     * @param data A buffer to place the read data into.  Must be at least (maxSize) bytes long.
     * @param maxSize the maximum number of bytes to read.
     * @returns the actual number of bytes read.
     */
   virtual qint64 readData(       char * data, qint64 maxSize) {const int32 ret = _dataIO()->Read( data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)); if (ret < 0) _isHosed = true; return muscleMax(ret, (int32)0);}

   /** Attempts to write the specified number of bytes to the device.
     * @param data A buffer of data to be written.  Must be at least (maxSize) bytes long.
     * @param maxSize the maximum number of bytes to write.
     * @returns the actual number of bytes written.
     */
   virtual qint64 writeData(const char * data, qint64 maxSize) {const int32 ret = _dataIO()->Write(data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)); if (ret < 0) _isHosed = true; return muscleMax(ret, (int32)0);}

private:
   DataIORef _dataIO;
   qint64 _dataSize;  // will be -1 if the I/O is sequential

   QSocketNotifier _readReady;
   bool _isHosed;
};

} // end namespace muscle

#endif
