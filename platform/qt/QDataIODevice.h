/* This file is Copyright 2000-2026 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQDataIODevice_h
#define MuscleQDataIODevice_h

#include <QIODevice>
#include <QSocketNotifier>

#include "dataio/SeekableDataIO.h"

namespace muscle {

/** This adapter class allows you to use a MUSCLE DataIO object via the Qt QIODevice API. */
class QDataIODevice : public QIODevice
{
public:
   /** Class constructor.
    *  @param dataIO Reference to the DataIO object we should use as our internal implementation.
    *  @param parent Passed to the QIODevice constructor
    */
   QDataIODevice(const DataIORef & dataIO, QObject * parent)
      : QIODevice(parent)
      , _dataIO(dataIO)
      , _optSeekableDataIO(dynamic_cast<SeekableDataIO*>(dataIO()))
      , _readReady(GetReadFD(), QSocketNotifier::Read)
      , _isHosed(false)
   {
      if (GetReadFD() >= 0) connect(&_readReady, SIGNAL(activated(int)), this, SIGNAL(readyRead()));
                       else _readReady.setEnabled(false);  // no pointing getting notifications about fd -1
   }

   /** Destructor */
   virtual ~QDataIODevice() {_readReady.setEnabled(false);}

   /** Returns true iff this device is in sequential-access-only mode. */
   virtual bool isSequential() const {return (_optSeekableDataIO == NULL);}

   /** Returns the current read-position of this device. */
   virtual qint64 pos() const {return _optSeekableDataIO ? muscleMax((qint64) _optSeekableDataIO->GetPosition(), (qint64)0) : 0;}

   /** Returns the total number of bytes available in this device. */
   virtual qint64 size() const {return isSequential() ? bytesAvailable() : (_optSeekableDataIO ? _optSeekableDataIO->GetLength() : 0);}

   /** Returns true iff this device has reached its End-of-File state. */
   virtual bool atEnd() const {return isSequential() ? _isHosed : ((_isHosed)||(QIODevice::atEnd()));}

   /** Attempts to read the specified number of bytes from the device.
     * @param data A buffer to place the read data into.  Must be at least (maxSize) bytes long.
     * @param maxSize the maximum number of bytes to read.
     * @returns the actual number of bytes read.
     */
   virtual qint64 readData(char * data, qint64 maxSize)
   {
      if (_isHosed) return -1;
      else
      {
         const int32 ret = _dataIO()->Read(data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)).GetByteCount();
         if (ret < 0) _isHosed = true;
         return ret;
      }
   }

   /** Attempts to write the specified number of bytes to the device.
     * @param data A buffer of data to be written.  Must be at least (maxSize) bytes long.
     * @param maxSize the maximum number of bytes to write.
     * @returns the actual number of bytes written.
     */
   virtual qint64 writeData(const char * data, qint64 maxSize)
   {
      if (_isHosed) return -1;
      else
      {
         const int32 ret = _dataIO()->Write(data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)).GetByteCount();
         if (ret < 0) _isHosed = true;
         return ret;
      }
   }

private:
   int GetReadFD() const {return _dataIO() ? _dataIO()->GetReadSelectSocket().GetFileDescriptor() : -1;}

   DataIORef _dataIO;
   SeekableDataIO * _optSeekableDataIO;

   QSocketNotifier _readReady;
   bool _isHosed;
};

} // end namespace muscle

#endif
