/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQDataIODevice_h
#define MuscleQDataIODevice_h

#include <qiodevice.h>
#include <qsocketnotifier.h>
#include "dataio/DataIO.h"

namespace muscle {

/** This adapter class allows you to use a MUSCLE DataIO object as a Qt QIODevice. */
class QDataIODevice : public QIODevice  
{
public: 
   /** Class constructor.
    *  @param dataIO a QSocket object that was allocated off the heap.  This object becomes owner of newSocket.
    *  @param parent Passed to the QIODevice constructor
    */
   QDataIODevice(const DataIORef & dataIO, QObject * parent) : QIODevice(parent), _dataIO(dataIO), _dataSize(dataIO()->GetLength()), _readReady(dataIO()->GetReadSelectSocket().GetFileDescriptor(), QSocketNotifier::Read), _isHosed(false)
   {
      connect(&_readReady, SIGNAL(activated(int)), this, SIGNAL(readyRead()));
   }   

   /** Destructor */
   virtual ~QDataIODevice() {_readReady.setEnabled(false);}

   virtual bool isSequential() const {return (_dataSize < 0);}
   virtual qint64 pos()        const {return muscleMax((qint64)_dataIO()->GetPosition(), (qint64)0);}
   virtual qint64 size()       const {return isSequential() ? bytesAvailable() : _dataSize;}
   virtual bool atEnd()        const {return isSequential() ? _isHosed : ((_isHosed)||(QIODevice::atEnd()));}

   virtual qint64 readData(       char * data, qint64 maxSize) {int32 ret = _dataIO()->Read( data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)); if (ret < 0) _isHosed = true; return muscleMax(ret, (int32)0);}
   virtual qint64 writeData(const char * data, qint64 maxSize) {int32 ret = _dataIO()->Write(data, (uint32) muscleMin(maxSize, (qint64)MUSCLE_NO_LIMIT)); if (ret < 0) _isHosed = true; return muscleMax(ret, (int32)0);}

private:
   DataIORef _dataIO;
   qint64 _dataSize;  // will be -1 if the I/O is sequential

   QSocketNotifier _readReady;
   bool _isHosed;
};

}; // end namespace muscle

#endif
