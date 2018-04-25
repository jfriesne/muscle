/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQSignalHandler_h
#define MuscleQSignalHandler_h

#include <qobject.h>
#include <qsocketnotifier.h>
#include "system/SignalMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"

class QSocketNotifier;

namespace muscle {

/**
 *  This is a useful Qt class that will catch system signals
 *  (e.g. SIGINT/SIGHUP/SIGTERM.... *not* Qt signals) and emit
 *  a signalReceived(int) Qt signal in response.  It uses the
 *  SignalReflectSession MUSCLE class in its implementation so it
 *  will have the same signal-handling semantics as that class.
 *  In all other respects it works like a SignalHandler object.
 */
class QSignalHandler : public QObject, public ISignalHandler
{
   Q_OBJECT

public:
   /** Constructor. 
     * @param parent Passed on to the QObject constructor
     * @param name Passed on to the QObject constructor
     */
   QSignalHandler(QObject * parent = NULL, const char * name = NULL);

   /** Destructor */
   virtual ~QSignalHandler();

   virtual void SignalHandlerFunc(int sigNum);

signals:
   /** Emitted when a signal is received.
     * @param sigNum The signal number received (e.g. SIGHUP, SIGINT, etc)
     */
   void SignalReceived(int sigNum);

private slots:
   void SocketDataReady();

private:
   ConstSocketRef _mainThreadSocket;
   ConstSocketRef _handlerFuncSocket;
   QSocketNotifier * _socketNotifier;   

   DECLARE_COUNTED_OBJECT(QSignalHandler);
};

}  // end namespace muscle

#endif
