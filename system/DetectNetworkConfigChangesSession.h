#ifndef DetectNetworkConfigChangesSession_h
#define DetectNetworkConfigChangesSession_h

#include "system/INetworkConfigChangesTarget.h"
#include "reflector/AbstractReflectSession.h"
#include "system/Mutex.h"
#include "util/NetworkUtilityFunctions.h"  /// for send_ignore_eintr()

namespace muscle {

class DetectNetworkConfigChangesThread;

/** This class watches the set of available network interfaces and when that set
  * changes, this class calls the NetworkInterfacesChanged() virtual method on any
  * session or factory object that is attached to the same ReflectServer (including itself).
  *  
  * Note that this functionality is currently implemented for Linux, Windows, and MacOS/X only.
  * Note also that the Windows and MacOS/X implementations currently make use of the MUSCLE
  * Thread class, and therefore won't compile if -DMUSCLE_SINGLE_THREAD_ONLY is set.
  * 
  * This class also provides notification callbacks when the host computer is about to go 
  * to sleep, and when it has just reawoken from sleep.  This can be useful e.g. if you 
  * want to make sure your program's TCP connections get cleanly disconnected and are not 
  * left open while the host computer is sleeping.  This functionality is currently 
  * implemented under MacOS/X and Windows only.
  *
  * @see tests/testnetconfigdetect.cpp for an example usage of this class.
  */
class DetectNetworkConfigChangesSession : public AbstractReflectSession, public INetworkConfigChangesTarget
{
public:
   /** Constructor
     * @param notifyReflectServer if set to true, this class will also call ComputerIsAboutToSleep()
     *                            and ComputerJustWokeUp() on the ReflectServer it is attached to, so that
     *                            TCP connections to other computers get disconnected just before the local
     *                            computer goes to sleep.  Defaults to true; pass in false if you don't want that behavior.
     */
   DetectNetworkConfigChangesSession(bool notifyReflectServer=true);

   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void EndSession();

   virtual void MessageReceivedFromGateway(const MessageRef & /*msg*/, void * /*userData*/) {/* empty */}

   virtual ConstSocketRef CreateDefaultSocket();
   virtual uint64 GetPulseTime(const PulseArgs & args) {return muscleMin(_callbackTime, AbstractReflectSession::GetPulseTime(args));}
   virtual void Pulse(const PulseArgs & args);

   virtual int32 DoInput(AbstractGatewayMessageReceiver & r, uint32 maxBytes);

   /** This method can be called to disable or enable this session.
     * A disabled session will not call any of the callback methods in the INetworkConfigChangesTarget class.
     * The default state of this session is enabled.
     * @param e False to disable the calling of callback methods on INetworkConfigChangesTarget objects, or
     *          True to enable it again.
     */
   void SetEnabled(bool e) {_enabled = e;}

   /** Returns true iff the calling of callback methods is enabled.  Default value is true. */
   bool IsEnabled() const {return _enabled;}

   /** Specified the amount of time the session should delay after receiving an indication of
     * a network-config change from the OS, before calling NetworkInterfacesChanged().
     * By default this class will wait an OS-specific number of seconds (5 seconds under Windows,
     * 3 seconds under Mac) before making the call, to make it more likely that the network
     * interfaces are in a usable state at the moment NetworkInterfacesChanged() is called.
     * However, you can specify a different delay-period here if you want to.
     * @param micros Number of microseconds of delay between when a config change is detected
     *               and when NetworkInterfacesChanged() should be called.  Set to MUSCLE_TIME_NEVER
     *               if you want to return to the default (OS-specific) behavior.
     */
   void SetExplicitDelayMicros(uint64 micros) {_explicitDelayMicros = micros;}

   /** Returns the current delay time, or MUSCLE_TIME_NEVER if we are using the default behavior. */
   uint64 GetExplicitDelayMicros() const {return _explicitDelayMicros;}

protected:
   /** Called when a change in the local interfaces set is detected.
     * Default implementation is a no-op.  Note, however, that NetworkInterfacesChanged(optInterfaceNames)
     * will be called on any session or factory that implements INetworkConfigChangesTarget (including this session).
     * @param optInterfaceNames optional table containing the names of the interfaces that
     *                          have changed (e.g. "en0", "en1", etc).  If this table is empty,
     *                          that indicates that any or all of the network interfaces may have
     *                          changed.  Note that changed-interface enumeration is currently only
     *                          implemented under MacOS/X and Windows, so under other operating systems this
     *                          argument will currently always be an empty table.
     */
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames);

   /** Called when the host computer is about to go to sleep.  Currently implemented for Windows and MacOS/X only.
     * Default implementation is a no-op.  Note, however, that ComputerIsAboutToSleep()
     * will be called on any session or factory that implements INetworkConfigChangesTarget (including this session).
     */
   virtual void ComputerIsAboutToSleep();

   /** Called when the host computer has just woken up from sleep.  Currently implemented for Windows and MacOS/X only.
     * Default implementation is a no-op.  Note, however, that ComputerJustWokeUp()
     * will be called on any session or factory that implements INetworkConfigChangesTarget (including this session).
     */
   virtual void ComputerJustWokeUp();

private:
   void ScheduleSendReport();
   void CallNetworkInterfacesChangedOnAllTargets(const Hashtable<String, Void> & optInterfaceNames);
   void CallComputerIsAboutToSleepOnAllTargets();
   void CallComputerJustWokeUpOnAllTargets();

#ifndef __linux__
   friend class DetectNetworkConfigChangesThread;
   status_t ThreadSafeMessageReceivedFromSingletonThread(const MessageRef & msg)
   {
      MutexGuard mg(_messagesFromSingletonThreadMutex);

      MRETURN_ON_ERROR(_messagesFromSingletonThread.AddTail(msg));

      // send a byte on the socket-pair to wake up the user thread so he'll check his _messagesFromSingletonThread queue
      const char junk = 'S';
      (void) send_ignore_eintr(_notifySocket.GetFileDescriptor(), &junk, sizeof(junk), 0);
      return B_NO_ERROR;
   }
   Mutex _messagesFromSingletonThreadMutex;
   Queue<MessageRef> _messagesFromSingletonThread;
   ConstSocketRef _notifySocket, _waitSocket;
#endif

   uint64 _explicitDelayMicros;
   uint64 _callbackTime;
   bool _enabled;

   Hashtable<String, Void> _pendingChangedInterfaceNames;  // currently used under MacOS/X only
   bool _changeAllPending;
   const bool _notifyReflectServer;

   DECLARE_COUNTED_OBJECT(DetectNetworkConfigChangesSession);
};
DECLARE_REFTYPES(DetectNetworkConfigChangesSession);

}  // end namespace muscle

#endif
