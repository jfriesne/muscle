/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleQSignalHandler_h
#define MuscleQSignalHandler_h

#include <QObject>
#include <QSocketNotifier>

#include "system/SignalMultiplexer.h"
#include "util/NetworkUtilityFunctions.h"

namespace muscle {

/**
 *  This is a useful Qt class that will catch system signals
 *  (eg SIGINT/SIGHUP/SIGTERM.... *not* Qt signals) and emit
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

   virtual void SignalHandlerFunc(const SignalEventInfo & sei);

signals:
   /** Emitted when a signal is received.
     * @param sei info about what signal was received, and from whom.
     */
   void SignalReceived(const SignalEventInfo & sei);

private slots:
   void SocketDataReady();

private:
   ConstSocketRef _mainThreadSocket;
   ConstSocketRef _handlerFuncSocket;
   QSocketNotifier * _socketNotifier;

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   uint8 _recvBuf[sizeof(int32)+sizeof(uint64)];  // ugly hack because C++03 doesn't know about constexpr methods
#else
   uint8 _recvBuf[SignalEventInfo::FlattenedSize()];
#endif
   uint32 _numValidRecvBytes;

   DECLARE_COUNTED_OBJECT(QSignalHandler);
};

}  // end namespace muscle

#endif
