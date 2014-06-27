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

#if defined(FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED) && (defined(__APPLE__) || defined(WIN32))
class DetectNetworkConfigChangesSession;
static void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames);
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

private:
   void ScheduleSendReport();

#ifndef __linux__
   virtual void InternalThreadEntry();

   volatile bool _threadKeepGoing;
# ifdef __APPLE__
   friend void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames);
   friend Hashtable<String, String> & GetKeyToInterfaceNameTable(DetectNetworkConfigChangesSession * s);
   CFRunLoopRef _threadRunLoop;
   Hashtable<String, String> _scKeyToInterfaceName;
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
};

};  // end namespace muscle

#endif
