/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/AbstractReflectSession.h"
#include "system/SignalMultiplexer.h"

namespace muscle {

/** This session can be added to a ReflectServer in order to have the server
  * catch signals (e.g. SIGINT on Unix/MacOS, Console signals on Windows)
  * and react by initiating a controlled shutdown of the server.
  */
class SignalHandlerSession : public AbstractReflectSession, public ISignalHandler, private CountedObject<SignalHandlerSession>
{
public:
   /** Default constructor. */
   SignalHandlerSession() {/* empty */}
   virtual ~SignalHandlerSession() {/* empty */}

   virtual ConstSocketRef CreateDefaultSocket();
   virtual int32 DoInput(AbstractGatewayMessageReceiver &, uint32);
   virtual void MessageReceivedFromGateway(const MessageRef &, void *) {/* empty */}
   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual const char * GetTypeName() const {return "SignalHandler";}
   virtual void SignalHandlerFunc(int whichSignal);

protected:
   /** This method is called in the main thread whenever a signal is received.
     * @param whichSignal the number of the signal received, as provided by the OS.
     *                    On POSIX OS's this might be SIGINT or SIGHUP; under Windows
     *                    it would be something like CTRL_CLOSE_EVENT or CTRL_LOGOFF_EVENT.
     * Default behavior is to always just call EndServer() so that the server process
     * will exit cleanly as soon as possible.
     */
   virtual void SignalReceived(int whichSignal);

private:
   ConstSocketRef _handlerSocket;
};

/** Returns true iff any SignalHandlerSession ever caught a signal since this process was started. */
bool WasSignalCaught();

/** Sets whether or not the ReflectServer in the main thread should try to handle signals.
  * Default state is false, unless MUSCLE_CATCH_SIGNALS_BY_DEFAULT was defined at compile time.
  * Note that this flag is read at the beginning of ReflectServer::ServerProcessLoop(), so
  * you must set it before then for it to have any effect.
  */
void SetMainReflectServerCatchSignals(bool enable);

/** Returns true iff the main-ReflectServer-handle-signals flags is set to true. */
bool GetMainReflectServerCatchSignals();

}; // end namespace muscle
