/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQAcceptSocketsThread_h
#define MuscleQAcceptSocketsThread_h

#include <qobject.h>
#include <qthread.h>
#include "system/AcceptSocketsThread.h"

namespace muscle {

/**
 *  This is a Qt-specific subclass of AcceptSocketsThread.
 *  It will listen on a port, and emit a ConnectionAccepted signal
 *  whenever a new TCP connection is received on that port.  In all
 *  other respects it works like an AcceptSocketsThread object.
 */
class QAcceptSocketsThread : public QObject, public AcceptSocketsThread
{
   Q_OBJECT

public:
   /** Constructor.
     * @param parent Passed on to the QObject constructor
     * @param name Passed on to the QObject constructor
     */
   QAcceptSocketsThread(QObject * parent = NULL, const char * name = NULL);

   /**
    *  Destructor.  This constructor will call ShutdownInternalThread() itself,
    *  so you don't need to call ShutdownInternalThread() explicitly UNLESS you
    *  have subclassed this class and overridden virtual methods that can get
    *  called from the internal thread -- in that case you should call
    *  ShutdownInternalThread() yourself to avoid potential race conditions between
    *  the internal thread and your own destructor method.
    */
   virtual ~QAcceptSocketsThread();

signals:
   /** Emitted when a new TCP connection is accepted
     * @param socketRef Reference to the newly accepted socket.
     */
   void ConnectionAccepted(const ConstSocketRef & socketRef);

protected:
   /** Overridden to send a QEvent */
   virtual void SignalOwner();

private slots:
   virtual bool event(QEvent * event);
};

};  // end namespace muscle

#endif
