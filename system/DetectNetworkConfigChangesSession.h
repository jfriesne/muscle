#ifndef DetectNetworkConfigChangesSession_h
#define DetectNetworkConfigChangesSession_h

#include "reflector/AbstractReflectSession.h"

#ifndef __linux__
# include "system/Thread.h"  // For Linux we can just listen directly on an AF_NETLINK socket, so no thread is needed
#endif

#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>
#endif

namespace muscle {

class DetectNetworkConfigChangesSession;

#if defined(FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED) && (defined(__APPLE__) || defined(WIN32))
static void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames);
static void MySleepCallbackAux(DetectNetworkConfigChangesSession * s, bool isGoingToSleep);
# ifdef __APPLE__
static void * GetRootPortPointerFromSession(const DetectNetworkConfigChangesSession * s);
# endif
#endif

/** This class watches the set of available network interfaces and calls its
  * NetworkInterfacesChanged() virtual method when a network-configuration change
  * has been detected.  The default implementation of NetworkInterfacesChanged() is a no-op,
  * so you will want to subclass this class and implement your own version of
  * NetworkInterfacesChanged() that does something useful (like posting a log message,
  * or tearing down and recreating any sockets that relied on the old networking config).
  *
  * Note that this functionality is currently implemented for Linux, Windows, and MacOS/X only.
  * Note also that the Windows and MacOS/X implementations currently make use of the MUSCLE
  * Thread class, and therefore won't compile if -DMUSCLE_SINGLE_THREAD_ONLY is set.
  *
  * As of MUSCLE v6.10, this class also provides notification callbacks when the host
  * computer is about to go to sleep, and when it has just reawoken from sleep.  This can
  * be useful e.g. if you want to make sure your program's TCP connections get cleanly
  * disconnected and are not left open while the host computer is sleeping.  This
  * functionality is currently implemented under MacOS/X and Windows only.
  *
  * @see tests/testnetconfigdetect.cpp for an example usage of this class.
  */
class DetectNetworkConfigChangesSession : public AbstractReflectSession, private CountedObject<DetectNetworkConfigChangesSession>
#ifndef __linux__
   , private Thread
#endif
{
public:
   DetectNetworkConfigChangesSession();

#ifndef __linux__
   virtual status_t AttachedToServer();
   virtual void AboutToDetachFromServer();
   virtual void EndSession();
   virtual AbstractMessageIOGatewayRef CreateGateway();
#endif

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   virtual ConstSocketRef CreateDefaultSocket();
   virtual const char * GetTypeName() const {return "DetectNetworkConfigChanges";}

   virtual uint64 GetPulseTime(const PulseArgs & args) {return muscleMin(_callbackTime, AbstractReflectSession::GetPulseTime(args));}
   virtual void Pulse(const PulseArgs & args);

#ifdef __linux__
   virtual int32 DoInput(AbstractGatewayMessageReceiver & r, uint32 maxBytes);
#endif

   /** This method can be called to disable or enable this session.
     * A disabled session will not call NetworkInterfacesChanged(), even if a network interface change is detected.
     * The default state of this session is enabled.
     * @param e True to enable calling of NetworkInterfacesChanged() when appropriate, or false to disable it.
     */
   void SetEnabled(bool e) {_enabled = e;}

   /** Returns true iff the calling of NetworkInterfaceChanged() is enabled.  Default value is true. */
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
#ifndef __linux__
   /** Overridden to do the signalling the Carbon way */
   virtual void SignalInternalThread();
#endif

   /** Called when a change in the local interfaces set is detected.
     * Default implementation calls FindAppropriateNetworkInterfaceIndices()
     * to update the process's interface list.  Subclass can augment
     * that behavior to include update various other objects that
     * need to be notified of the change.
     * @param optInterfaceNames optional table containing the names of the interfaces that
     *                          have changed (e.g. "en0", "en1", etc).  If this table is empty,
     *                          that indicates that any or all of the network interfaces may have
     *                          changed.  Note that changed-interface enumeration is currently only
     *                          implemented under MacOS/X, so under other operating systems this
     *                          argument will currently always be an empty table.
     */
   virtual void NetworkInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames);

   /** Called when the host computer is about to go to sleep.  Currently implemented for Windows and MacOS/X only.
     * Default implementation is a no-op.
     */
   virtual void ComputerIsAboutToSleep();

   /** Called when the host computer has just woken up from sleep.  Currently implemented for Windows and MacOS/X only.
     * Default implementation is a no-op.
     */
   virtual void ComputerJustWokeUp();

private:
   void ScheduleSendReport();

   enum {
      DNCCS_MESSAGE_INTERFACES_CHANGED = 1684956003, // 'dncc'
      DNCCS_MESSAGE_ABOUT_TO_SLEEP,
      DNCCS_MESSAGE_JUST_WOKE_UP
   };

#ifndef __linux__
   virtual void InternalThreadEntry();
   status_t ThreadSafeSendMessageToOwner(const MessageRef & msg);

   volatile bool _threadKeepGoing;
#if defined(__APPLE__) || defined(WIN32)
   void MySleepCallbackAux(bool isGoingToSleep);
   friend void MySleepCallbackAux(DetectNetworkConfigChangesSession * s, bool isGoingToSleep);
#endif
# ifdef __APPLE__
   friend void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames);
   friend Hashtable<String, String> & GetKeyToInterfaceNameTable(DetectNetworkConfigChangesSession * s);
   friend void * GetRootPortPointerFromSession(const DetectNetworkConfigChangesSession * s);
   CFRunLoopRef _threadRunLoop;
   Hashtable<String, String> _scKeyToInterfaceName;
   void * _rootPortPointer;
# elif _WIN32
   friend void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames);
   ::HANDLE _wakeupSignal;
   Mutex _sendMessageToOwnerMutex;
# endif
#endif // __linux__

   uint64 _explicitDelayMicros;
   uint64 _callbackTime;
   bool _enabled;

   Hashtable<String, Void> _pendingChangedInterfaceNames;  // currently used under MacOS/X only
   bool _changeAllPending;
   bool _isComputerSleeping;
};

};  // end namespace muscle

#endif
