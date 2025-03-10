/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSignalHandlerSession_h
#define MuscleSignalHandlerSession_h

#include "reflector/AbstractReflectSession.h"
#include "system/SignalMultiplexer.h"

namespace muscle {

/** This session can be added to a ReflectServer in order to have the server
  * catch signals (eg SIGINT on Unix/MacOS, Console signals on Windows)
  * and react by initiating a controlled shutdown of the server.
  */
class SignalHandlerSession : public AbstractReflectSession, public ISignalHandler
{
public:
   /** Default constructor. */
   SignalHandlerSession();

   /** Destructor. */
   virtual ~SignalHandlerSession() {/* empty */}

   virtual String GetClientDescriptionString() const {return "signal handler";}

   virtual ConstSocketRef CreateDefaultSocket();
   virtual io_status_t DoInput(AbstractGatewayMessageReceiver &, uint32);
   virtual void MessageReceivedFromGateway(const MessageRef &, void *) {/* empty */}
   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void SignalHandlerFunc(const SignalEventInfo & sei);

protected:
   /** This method is called in the main thread whenever a signal is received.
     * @param sei information about what signal was receive, and from whom.
     * Default behavior is to always just call EndServer() so that the server process
     * will exit cleanly as soon as possible.
     */
   virtual void SignalReceived(const SignalEventInfo & sei);

private:
   ConstSocketRef _handlerSocket;

   DECLARE_COUNTED_OBJECT(SignalHandlerSession);

#ifdef MUSCLE_AVOID_CPLUSPLUS11
   uint8 _recvBuf[sizeof(int32)+sizeof(uint64)];  // ugly hack because C++03 doesn't know about constexpr methods
#else
   uint8 _recvBuf[SignalEventInfo::FlattenedSize()];
#endif
   uint32 _numValidRecvBytes;
};
DECLARE_REFTYPES(SignalHandlerSession);

/** Returns true iff any SignalHandlerSession ever caught a signal since this process was started. */
MUSCLE_NODISCARD bool WasSignalCaught();

/** Sets whether or not the ReflectServer in the main thread should try to handle signals.
  * Default state is false, unless MUSCLE_CATCH_SIGNALS_BY_DEFAULT was defined at compile time.
  * Note that this flag is read at the beginning of ReflectServer::ServerProcessLoop(), so
  * you must set it before then for it to have any effect.
  */
void SetMainReflectServerCatchSignals(bool enable);

/** Returns true iff the main-ReflectServer-handle-signals flags is set to true. */
MUSCLE_NODISCARD bool GetMainReflectServerCatchSignals();

} // end namespace muscle

#endif
