#include "iogateway/SignalMessageIOGateway.h"

#include "system/DetectNetworkConfigChangesSession.h"

#ifdef __APPLE__
# include <mach/mach_port.h>
# include <mach/mach_interface.h>
# include <mach/mach_init.h>
# include <TargetConditionals.h>
# if !(TARGET_OS_IPHONE)
#  include <IOKit/pwr_mgt/IOPMLib.h>
#  include <IOKit/IOMessage.h>
# else
#  ifndef MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION
#   define MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION
#  endif
# endif
# include <SystemConfiguration/SystemConfiguration.h>
#elif WIN32
# if defined(UNICODE) && !defined(_UNICODE)
#  define _UNICODE 1
# endif
# include <tchar.h>
# if defined(_WIN32_WINNT) && (_WIN32_WINNT < 0x0600) && !defined(MUSCLE_AVOID_NETIOAPI)
   // If we're compiling pre-Vista, the NetIOApi isn't available
#  define MUSCLE_AVOID_NETIOAPI
# endif
# ifndef MUSCLE_AVOID_NETIOAPI
#  include <Netioapi.h>
#  include "util/NetworkUtilityFunctions.h"  // for GetNetworkInterfaceInfos()
# endif
# include <Iphlpapi.h>
# define MY_INVALID_HANDLE_VALUE ((::HANDLE)(-1))  // bloody hell...
#endif

#ifdef __linux__
# include <asm/types.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <net/if.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#endif

#include "reflector/ReflectServer.h"

#if (defined(__APPLE__) && !(TARGET_OS_IPHONE)) || defined(WIN32)
# define USE_SINGLETON_THREAD
# include "system/Thread.h"
# if defined(MUSCLE_SINGLE_THREAD_ONLY)
#  error "Can't compile DetectNetworkConfigChangesSession for MacOS/X or Windos when MUSCLE_SINGLE_THREAD_ONLY is set!"
# endif
#endif

namespace muscle {

#if defined(USE_SINGLETON_THREAD)

class DetectNetworkConfigChangesThread;

static Mutex _singletonThreadMutex;
static DetectNetworkConfigChangesThread * _singletonThread = NULL;  // demand-allocated

#ifdef __APPLE__
static OSStatus CreateIPAddressListChangeCallbackSCF(SCDynamicStoreCallBack callback, void *contextPtr, SCDynamicStoreRef * storeRef, CFRunLoopSourceRef *sourceRef, Hashtable<String, String> & keyToInterfaceName);
static void IPConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void * info);
static void SleepCallback(void * refCon, io_service_t /*service*/, natural_t messageType, void * messageArgument);
#endif

#if defined(WIN32)
static LRESULT CALLBACK dnccsWindowHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
# if !defined(MUSCLE_AVOID_NETIOAPI)
static VOID __stdcall AddressCallback(IN PVOID context, IN PMIB_UNICASTIPADDRESS_ROW Address OPTIONAL, IN MIB_NOTIFICATION_TYPE NotificationType);
static VOID __stdcall InterfaceCallback(IN PVOID context, IN PMIB_IPINTERFACE_ROW interfaceRow, IN MIB_NOTIFICATION_TYPE NotificationType);
# endif
#endif

enum {
   DNCCS_MESSAGE_INTERFACES_CHANGED = 1684956003, // 'dncc'
   DNCCS_MESSAGE_ABOUT_TO_SLEEP,
   DNCCS_MESSAGE_JUST_WOKE_UP
};

// We'll create just one DetectNetworkConfigChangesThread per process and let all DetectNetworkConfigChangesSession objects
// use it.  That way we limit the number of detect-network-changes-threads created to 1, and relieve the user's code from 
// any pressure to minimize the number of DetectNetworkConfigChangesSessions it creates.
class DetectNetworkConfigChangesThread : public Thread
{
public:
   DetectNetworkConfigChangesThread()
      : Thread(false)
      , _threadKeepGoing(false)
      , _isComputerSleeping(false)
#ifdef __APPLE__
      , _threadRunLoop(NULL)
      , _rootPortPointer(NULL)
#elif WIN32
      , _wakeupSignal(MY_INVALID_HANDLE_VALUE)
#endif
   {
      // empty
   }

   virtual ~DetectNetworkConfigChangesThread()
   {
      MASSERT(_registeredSessions.IsEmpty(), "DetectNetworkConfigChangesThread destroyed while sessions were still registered");
   }

   status_t RegisterSession(DetectNetworkConfigChangesSession * s) 
   {
      const bool startInternalThread = _registeredSessions.IsEmpty();

      MRETURN_ON_ERROR(_registeredSessions.PutWithDefault(s));
      return startInternalThread ? StartInternalThread() : B_NO_ERROR;
   }

   status_t UnregisterSession(DetectNetworkConfigChangesSession * s) 
   {
      status_t ret;
      if ((_registeredSessions.Remove(s).IsOK(ret))&&(_registeredSessions.IsEmpty())) ShutdownInternalThread();
      return ret;
   }

   bool HasRegisteredSessions() const {return _registeredSessions.HasItems();}

   void ThreadSafeSendMessageToSessions(const MessageRef & msg)
   {
      MutexGuard mg(_singletonThreadMutex);
      for (HashtableIterator<DetectNetworkConfigChangesSession *, Void> iter(_registeredSessions); iter.HasData(); iter++) 
         iter.GetKey()->ThreadSafeMessageReceivedFromSingletonThread(msg);
   }

   void SleepCallback(bool isAboutToSleep)
   {
      if (isAboutToSleep != _isComputerSleeping)
      {
         _isComputerSleeping = isAboutToSleep;

         static Message _aboutToSleepMessage(DNCCS_MESSAGE_ABOUT_TO_SLEEP);
         static Message _justWokeUpMessage(DNCCS_MESSAGE_JUST_WOKE_UP);
         (void) ThreadSafeSendMessageToSessions(MessageRef(isAboutToSleep ? &_aboutToSleepMessage : &_justWokeUpMessage, false));
      }       
   }

#ifdef __APPLE__
   void SleepCallback(natural_t messageType, long messageArgument)
   {
      switch(messageType)
      {
         case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
               Applications have a chance to prevent sleep by calling IOCancelPowerChange.
               Most applications should not prevent idle sleep.

               Power Management waits up to 30 seconds for you to either allow or deny idle
               sleep. If you don't acknowledge this power change by calling either
               IOAllowPowerChange or IOCancelPowerChange, the system will wait 30
               seconds then go to sleep.
            */

            //Uncomment to cancel idle sleep
            //IOCancelPowerChange(*_rootPortPointer, (long)messageArgument);

            // we will allow idle sleep
            IOAllowPowerChange(*_rootPortPointer, (long)messageArgument);
          break;
    
          case kIOMessageSystemWillSleep:
             /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
                IOCancelPowerChange to acknowledge this message, sleep will be
                delayed by 30 seconds.

                NOTE: If you call IOCancelPowerChange to deny sleep it returns
                kIOReturnSuccess, however the system WILL still go to sleep.
             */
             SleepCallback(true);
             IOAllowPowerChange(*_rootPortPointer, (long)messageArgument);
          break;

          case kIOMessageSystemWillPowerOn:
             //System has started the wake up process...
          break;
    
          case kIOMessageSystemHasPoweredOn:
             // System has finished waking up...
             SleepCallback(false);
          break;

          default:
             // empty
          break;
      }
   }

   void IPConfigChanged(SCDynamicStoreRef store, CFArrayRef changedKeys)
   {
      Hashtable<String, Void> changedInterfaceNames;

      CFIndex c = CFArrayGetCount(changedKeys);
      for (CFIndex i=0; i<c; i++)
      {
         const CFStringRef p = (CFStringRef) CFArrayGetValueAtIndex(changedKeys, i);
         const String keyStr(p);
         if (keyStr.HasChars())
         {
            String interfaceName;
            if (keyStr.StartsWith("State:/Network/Interface/")) interfaceName = keyStr.Substring(25).Substring(0, "/");
            else
            {
               CFPropertyListRef propList = SCDynamicStoreCopyValue(store, p);
               if (propList)
               {
                  interfaceName = String((const CFStringRef) CFDictionaryGetValue((CFDictionaryRef) propList, CFSTR("InterfaceName")));
                  CFRelease(propList);
               }
            }

            if (interfaceName.HasChars()) (void) _scKeyToInterfaceName.Put(keyStr, interfaceName);
                                     else interfaceName = _scKeyToInterfaceName.RemoveWithDefault(keyStr);

            if (interfaceName.HasChars()) (void) changedInterfaceNames.PutWithDefault(interfaceName);
         }
      }

      SignalInterfacesChanged(changedInterfaceNames);
   }
#endif

   void SignalInterfacesChanged(uint32 changedIdx)
   {
      Hashtable<String, Void> iNames;
      Queue<NetworkInterfaceInfo> q;
      if (GetNetworkInterfaceInfos(q).IsOK())
      {
         for (uint32 i=0; i<q.GetNumItems(); i++)
         {
            const NetworkInterfaceInfo & nii = q[i];
            const IPAddress & ip = nii.GetLocalAddress();
            if ((ip.IsInterfaceIndexValid())&&(ip.GetInterfaceIndex() == changedIdx)) iNames.PutWithDefault(nii.GetName());
         }
      }
      SignalInterfacesChanged(iNames);
   }

   void SignalInterfacesChanged(const Hashtable<String, Void> & optInterfaceNames)
   {
      MessageRef msg;  // demand-allocated
      if (optInterfaceNames.HasItems())
      {
         msg = GetMessageFromPool(DNCCS_MESSAGE_INTERFACES_CHANGED);
         if (msg()) for (HashtableIterator<String, Void> iter(optInterfaceNames); iter.HasData(); iter++) msg()->AddString("if", iter.GetKey());
      }
      static Message _msg(DNCCS_MESSAGE_INTERFACES_CHANGED);
      ThreadSafeSendMessageToSessions(msg() ? msg : MessageRef(&_msg, false));
   }

protected:
   virtual void InternalThreadEntry()
   {
# if defined(MUSCLE_USE_DUMMY_DETECT_NETWORK_CONFIG_CHANGES_SESSION)
      // empty
# elif defined(__APPLE__)
      _threadRunLoop = CFRunLoopGetCurrent();

      // notification port allocated by IORegisterForSystemPower
      IONotificationPortRef powerNotifyPortRef = NULL;
      io_object_t notifierObject;    // notifier object, used to deregister later
      void * refCon = (void *) this; // this parameter is passed to the callback
      CFRunLoopSourceRef powerNotifyRunLoopSource = NULL;
    
      // register to receive system sleep notifications
      io_connect_t root_port = IORegisterForSystemPower(refCon, &powerNotifyPortRef, muscle::SleepCallback, &notifierObject);
      _rootPortPointer = &root_port;
      if (root_port != 0)
      {
         powerNotifyRunLoopSource = IONotificationPortGetRunLoopSource(powerNotifyPortRef);
         CFRunLoopAddSource((CFRunLoopRef)_threadRunLoop, powerNotifyRunLoopSource, kCFRunLoopCommonModes);
      }
      else LogTime(MUSCLE_LOG_WARNING, "DetectNetworkConfigChangesThread::InternalThreadEntry():  IORegisterForSystemPower() failed [%s]\n", B_ERRNO());

      SCDynamicStoreRef storeRef = NULL;
      CFRunLoopSourceRef sourceRef = NULL;
      if (CreateIPAddressListChangeCallbackSCF(muscle::IPConfigChangedCallback, this, &storeRef, &sourceRef, _scKeyToInterfaceName) == noErr)
      {
         CFRunLoopAddSource(CFRunLoopGetCurrent(), sourceRef, kCFRunLoopDefaultMode);
         while(_threadKeepGoing) 
         {
            CFRunLoopRun();
#ifdef TODO_REWRITE_THIS
            while(1)
            {
               MessageRef msgRef;
               int32 numLeft = WaitForNextMessageFromOwner(msgRef, 0);
               if (numLeft >= 0)
               {
                  if (MessageReceivedFromOwner(msgRef, numLeft).IsError()) _threadKeepGoing = false;
               }
               else break; 
            }
#endif
         }
         CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sourceRef, kCFRunLoopDefaultMode);
         CFRelease(storeRef);
         CFRelease(sourceRef);
      }
      if (powerNotifyRunLoopSource) CFRunLoopRemoveSource(CFRunLoopGetCurrent(), powerNotifyRunLoopSource, kCFRunLoopDefaultMode);
      if (powerNotifyPortRef) 
      {
         (void) IODeregisterForSystemPower(&root_port);
         (void) IONotificationPortDestroy(powerNotifyPortRef);
      }
      _rootPortPointer = NULL;
# elif defined(WIN32)
#define WINDOW_CLASS_NAME   _T("DetectNetworkConfigChangesThread_HiddenWndClass")
#define WINDOW_MENU_NAME    _T("DetectNetworkConfigChangesThread_MainMenu")

      // Gotta create a hidden window to receive WM_POWERBROADCAST events, lame-o!
      // Register the window class for the main window. 
      WNDCLASS window_class; memset(&window_class, 0, sizeof(window_class));
      window_class.style          = 0;
      window_class.lpfnWndProc    = (WNDPROC) dnccsWindowHandler;
      window_class.cbClsExtra     = 0;
      window_class.cbWndExtra     = 0;
      window_class.hInstance      = NULL;
      window_class.hIcon          = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION);
      window_class.hCursor        = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
      window_class.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
      window_class.lpszMenuName   = WINDOW_MENU_NAME; 
      window_class.lpszClassName  = WINDOW_CLASS_NAME; 
      (void) RegisterClass(&window_class); // Deliberately not checking result, per Chris Guzak at http://msdn.microsoft.com/en-us/library/windows/desktop/ms633586(v=vs.85).aspx
     
      // This window will never be shown; its only purpose is to allow us to receive WM_POWERBROADCAST events so we can alert the calling code to sleep and wake events
      HWND hiddenWindow = CreateWindow(WINDOW_CLASS_NAME, _T(""), WS_OVERLAPPEDWINDOW, -1, -1, 0, 0, (HWND)NULL, (HMENU) NULL, NULL, (LPVOID)NULL); 
      if (hiddenWindow) 
      {
# if defined(MUSCLE_64_BIT_PLATFORM)
         SetWindowLongPtr(hiddenWindow, GWLP_USERDATA, (LONG_PTR) this);
# else
         SetWindowLongPtr(hiddenWindow, GWLP_USERDATA, (LONG) this);
# endif
      }
      else LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesThread::InternalThreadEntry():  CreateWindow() failed! [%s]\n", B_ERRNO());
   
# ifndef MUSCLE_AVOID_NETIOAPI
      HANDLE handle1 = MY_INVALID_HANDLE_VALUE; (void) NotifyUnicastIpAddressChange(AF_UNSPEC, &AddressCallback,   this, FALSE, &handle1);
      HANDLE handle2 = MY_INVALID_HANDLE_VALUE; (void) NotifyIpInterfaceChange(     AF_UNSPEC, &InterfaceCallback, this, FALSE, &handle2);
#endif

      OVERLAPPED olap; memset(&olap, 0, sizeof(olap));
      olap.hEvent = CreateEvent(NULL, false, false, NULL);
      if (olap.hEvent != NULL)
      {
         while(_threadKeepGoing)
         {
            ::HANDLE junk;
            int nacRet = NotifyAddrChange(&junk, &olap); 
            if ((nacRet == NO_ERROR)||(WSAGetLastError() == WSA_IO_PENDING))
            {
               if (hiddenWindow)
               {
                  MSG message;
                  while(PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) DispatchMessage(&message); 
               }

               ::HANDLE events[] = {olap.hEvent, _wakeupSignal};
               DWORD waitResult = hiddenWindow ? MsgWaitForMultipleObjects(ARRAYITEMS(events), events, false, INFINITE, QS_ALLINPUT) : WaitForMultipleObjects(ARRAYITEMS(events), events, false, INFINITE);
               if (waitResult == WAIT_OBJECT_0)
               {
                  // Serialized since the NotifyUnicast*Change() callbacks get called from a different thread
                  static Message _msg(DNCCS_MESSAGE_INTERFACES_CHANGED);
                  ThreadSafeSendMessageToSessions(MessageRef(&_msg, false));
               }          
               else if ((hiddenWindow)&&(waitResult == DWORD(WAIT_OBJECT_0+ARRAYITEMS(events))))
               {
                  // Message received from Window-message-handler; go around the loop to process it
               }
               else 
               {
                  // Anything else is an error and we should pack it in
                  (void) CancelIPChangeNotify(&olap);
                  _threadKeepGoing = false;
               }
            }
            else 
            {
               LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesThread:  NotifyAddrChange() failed, code %i (%i) [%s]\n", nacRet, WSAGetLastError(), B_ERRNO());
               break;
            }
         }
         CloseHandle(olap.hEvent);
      }
      else LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesThread:  CreateEvent() failed [%s]\n", B_ERRNO());

# ifndef MUSCLE_AVOID_NETIOAPI
      if (handle2 != MY_INVALID_HANDLE_VALUE) CancelMibChangeNotify2(handle2);
      if (handle1 != MY_INVALID_HANDLE_VALUE) CancelMibChangeNotify2(handle1);
# endif

      if (hiddenWindow) DestroyWindow(hiddenWindow);
      // Deliberately leaving the window_class registered here, since to do otherwise could be thread-unsafe
# else
#  error "DetectNetworkConfigChangesThread:  OS not supported!"
# endif
   }

private:
   virtual status_t StartInternalThread()
   {
      MRETURN_ON_ERROR(SetupSignalling());

      status_t ret;
      _threadKeepGoing = true;
      if (Thread::StartInternalThread().IsError(ret)) 
      {
         _threadKeepGoing = false;
         CleanupSignalling();
      }
      return ret;
   }

   virtual void ShutdownInternalThread(bool waitForThread = true)
   {
      _threadKeepGoing = false;
# ifdef __APPLE__
      if (_threadRunLoop) CFRunLoopStop((CFRunLoopRef)_threadRunLoop);
# elif WIN32
      SetEvent(_wakeupSignal);
# endif

      Thread::ShutdownInternalThread(waitForThread);
      CleanupSignalling();
   }

   status_t SetupSignalling()
   {
#ifdef WIN32
      _wakeupSignal = CreateEvent(0, false, false, 0);
      if (_wakeupSignal == MY_INVALID_HANDLE_VALUE) return B_ERROR("CreateEvent() failed");
#endif
      return B_NO_ERROR;
   }

   void CleanupSignalling()
   {
#ifdef WIN32
      if (_wakeupSignal != MY_INVALID_HANDLE_VALUE)
      {
         CloseHandle(_wakeupSignal);
         _wakeupSignal = MY_INVALID_HANDLE_VALUE;
      }
#endif
   }

   Hashtable<DetectNetworkConfigChangesSession *, Void> _registeredSessions;

   volatile bool _threadKeepGoing;
   bool _isComputerSleeping;

# ifdef __APPLE__
   void * _threadRunLoop; // actually of type CFRunLoopRef but I don't want to include CFRunLoop.h from this header file becaues doing so breaks things
   Hashtable<String, String> _scKeyToInterfaceName;
   io_connect_t * _rootPortPointer;
# elif _WIN32
   ::HANDLE _wakeupSignal;
# endif
};

# ifdef __APPLE__
// MacOS/X Code taken from http://developer.apple.com/technotes/tn/tn1145.html
static OSStatus MoreSCErrorBoolean(Boolean success)
{
   OSStatus err = noErr;
   if (!success) 
   {
      int scErr = SCError();
      if (scErr == kSCStatusOK) scErr = kSCStatusFailed;
      err = scErr;
   }
   return err;
}

static OSStatus MoreSCError(const void *value) {return MoreSCErrorBoolean(value != NULL);}
static OSStatus CFQError(CFTypeRef cf) {return (cf == NULL) ? -1 : noErr;}
static void CFQRelease(CFTypeRef cf) {if (cf != NULL) CFRelease(cf);}

static void StoreRecordFunc(const void * key, const void * value, void * context)
{
   const CFStringRef     keyStr   = (const CFStringRef)     key;
   const CFDictionaryRef propList = (const CFDictionaryRef) value;
   if ((keyStr)&&(propList))
   {
      const String k(keyStr);
      if (k.StartsWith("State:/Network/Interface/")) 
      {
         const String interfaceName = k.Substring(25).Substring(0, "/");
         (void) ((Hashtable<String, String> *)(context))->Put(k, interfaceName);
      }
      else
      {
         const String interfaceName((const CFStringRef) CFDictionaryGetValue((CFDictionaryRef) propList, CFSTR("InterfaceName")));
         if (interfaceName.HasChars()) ((Hashtable<String, String> *)(context))->Put(k, interfaceName);
      }
   }
}

// Create a SCF dynamic store reference and a corresponding CFRunLoop source.  If you add the
// run loop source to your run loop then the supplied callback function will be called when local IP
// address list changes.
static OSStatus CreateIPAddressListChangeCallbackSCF(SCDynamicStoreCallBack callback, void *contextPtr, SCDynamicStoreRef * storeRef, CFRunLoopSourceRef *sourceRef, Hashtable<String, String> & keyToInterfaceName)
{
   OSStatus                err;
   SCDynamicStoreContext   context = {0, NULL, NULL, NULL, NULL};
   SCDynamicStoreRef       ref = NULL;
   CFStringRef             patterns[3] = {NULL, NULL, NULL};
   CFArrayRef              patternList = NULL;
   CFRunLoopSourceRef      rls = NULL;

   assert(callback   != NULL);
   assert( storeRef  != NULL);
   assert(*storeRef  == NULL);
   assert( sourceRef != NULL);
   assert(*sourceRef == NULL);

   // Create a connection to the dynamic store, then create
   // a search pattern that finds all entities.
   context.info = contextPtr;
   ref = SCDynamicStoreCreate(NULL, CFSTR("AddIPAddressListChangeCallbackSCF"), callback, &context);
   err = MoreSCError(ref);
   if (err == noErr) 
   {
      // This pattern is "State:/Network/Service/[^/]+/IPv4".
      patterns[0] = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv4);  // FogBugz #6075
      err = MoreSCError(patterns[0]);
      if (err == noErr)
      {
         // This pattern is "State:/Network/Service/[^/]+/IPv6".
         patterns[1] = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetIPv6);  // FogBugz #6075
         err = MoreSCError(patterns[1]);
         if (err == noErr)
         {
            // This pattern is "State:/Network/Interface/[^/]+/Link"
            patterns[2] = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, kSCDynamicStoreDomainState, kSCCompAnyRegex, kSCEntNetLink);  // FogBugz #10048
            err = MoreSCError(patterns[2]);
         }
      }
   }

   // Tell SCF that we want to watch changes in keys that match that pattern list, then create our run loop source.
   if (err == noErr) 
   {
       patternList = CFArrayCreate(NULL, (const void **) patterns, 3, &kCFTypeArrayCallBacks);
       err = CFQError(patternList);
   }

   // Query the current values matching our patterns, so we know what interfaces are currently operative
   if (err == noErr)
   {
      CFDictionaryRef curVals = SCDynamicStoreCopyMultiple(ref, NULL, patternList);
      if (curVals)
      {
         CFDictionaryApplyFunction(curVals, StoreRecordFunc, &keyToInterfaceName);
         CFRelease(curVals);
      }
   }

   err = MoreSCErrorBoolean(SCDynamicStoreSetNotificationKeys(ref, NULL, patternList));

   if (err == noErr) 
   {
       rls = SCDynamicStoreCreateRunLoopSource(NULL, ref, 0);
       err = MoreSCError(rls);
   }

   // Clean up.
   CFQRelease(patterns[0]);
   CFQRelease(patterns[1]);
   CFQRelease(patterns[2]);
   CFQRelease(patternList);
   if (err != noErr) 
   {
      CFQRelease(ref);
      ref = NULL;
   }
   *storeRef = ref;
   *sourceRef = rls;

   assert( (err == noErr) == (*storeRef  != NULL) );
   assert( (err == noErr) == (*sourceRef != NULL) );

   return err;
}

static void SleepCallback(void * refCon, io_service_t /*service*/, natural_t messageType, void * messageArgument) {static_cast<DetectNetworkConfigChangesThread *>(refCon)->SleepCallback(messageType, (long)messageArgument);}
static void IPConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void * info) {static_cast<DetectNetworkConfigChangesThread *>(info)->IPConfigChanged(store, changedKeys);}

# endif  // __APPLE__

# if defined(WIN32)
#  if !defined(MUSCLE_AVOID_NETIOAPI)

VOID __stdcall AddressCallback(IN PVOID context, IN PMIB_UNICASTIPADDRESS_ROW Address OPTIONAL, IN MIB_NOTIFICATION_TYPE /*NotificationType*/)
{
   if (Address != NULL) static_cast<DetectNetworkConfigChangesThread *>(context)->SignalInterfacesChanged(Address->InterfaceIndex);
}

VOID __stdcall InterfaceCallback(IN PVOID context, IN PMIB_IPINTERFACE_ROW interfaceRow, IN MIB_NOTIFICATION_TYPE /*NotificationType*/)
{
   if (interfaceRow != NULL) static_cast<DetectNetworkConfigChangesThread *>(context)->SignalInterfacesChanged(interfaceRow->InterfaceIndex);
}
#  endif

static LRESULT CALLBACK dnccsWindowHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   if (message == WM_POWERBROADCAST)
   {
      switch(wParam)
      {
         case PBT_APMRESUMEAUTOMATIC: case PBT_APMRESUMESUSPEND: case PBT_APMQUERYSUSPENDFAILED: case PBT_APMRESUMECRITICAL:
            reinterpret_cast<DetectNetworkConfigChangesThread *>(GetWindowLongPtr(hWnd, GWLP_USERDATA))->SleepCallback(false);
         break;

         case PBT_APMQUERYSUSPEND: case PBT_APMSUSPEND:
            reinterpret_cast<DetectNetworkConfigChangesThread *>(GetWindowLongPtr(hWnd, GWLP_USERDATA))->SleepCallback(true);
         break;

         default:
            // empty
         break;
      }
   }
   return DefWindowProc(hWnd, message, wParam, lParam);
}
# endif

static status_t RegisterWithSingletonThread(DetectNetworkConfigChangesSession * s)
{
   MutexGuard mg(_singletonThreadMutex);

   if (_singletonThread == NULL)
   {
      _singletonThread = newnothrow DetectNetworkConfigChangesThread;
      MRETURN_OOM_ON_NULL(_singletonThread);
   }

   status_t ret;
   if ((_singletonThread)&&(_singletonThread->RegisterSession(s).IsError(ret))&&(_singletonThread->HasRegisteredSessions() == false))
   {
      delete _singletonThread;
      _singletonThread = NULL;
      return ret;
   }

   return ret;
}

static status_t UnRegisterFromSingletonThread(DetectNetworkConfigChangesSession * s)
{
   MutexGuard mg(_singletonThreadMutex);
   if (_singletonThread)
   {
      status_t ret;
      if ((_singletonThread->UnregisterSession(s).IsOK(ret))&&(_singletonThread->HasRegisteredSessions() == false))
      {
         delete _singletonThread;
         _singletonThread = NULL;
      }
      return ret;
   }
   else return B_DATA_NOT_FOUND;
}

#endif

DetectNetworkConfigChangesSession :: DetectNetworkConfigChangesSession(bool notifyReflectServer)
   : _explicitDelayMicros(MUSCLE_TIME_NEVER),
     _callbackTime(MUSCLE_TIME_NEVER),
     _enabled(true),
     _changeAllPending(false),
     _notifyReflectServer(notifyReflectServer)
{
   // empty
}

void DetectNetworkConfigChangesSession :: ScheduleSendReport()
{
   // We won't actually send the report for a certain number of
   // seconds (OS-specific); that way any additional changes the OS
   // is making to the network config will have time to be reported
   // and we (hopefully) won't end up sending multiple reports in a row.
#ifdef WIN32
   const int hysteresisDelaySeconds = 5;  // Windows needs 5, it is lame
#else
   const int hysteresisDelaySeconds = 3;  // MacOS/X needs about 3 seconds
#endif
   _callbackTime = GetRunTime64() + ((_explicitDelayMicros == MUSCLE_TIME_NEVER) ? SecondsToMicros(hysteresisDelaySeconds) : _explicitDelayMicros);
   InvalidatePulseTime();
}

void DetectNetworkConfigChangesSession :: CallNetworkInterfacesChangedOnAllTargets(const Hashtable<String, Void> & interfaceNames)
{
   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->NetworkInterfacesChanged(interfaceNames);
   }

   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->NetworkInterfacesChanged(interfaceNames);
   }
}

void DetectNetworkConfigChangesSession :: CallComputerIsAboutToSleepOnAllTargets()
{
   if (_notifyReflectServer)
   {
      ReflectServer * rs = GetOwner();
      if (rs) rs->ComputerIsAboutToSleep();
   }

   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->ComputerIsAboutToSleep();
   }

   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->ComputerIsAboutToSleep();
   }
}

void DetectNetworkConfigChangesSession :: CallComputerJustWokeUpOnAllTargets()
{
   for (HashtableIterator<const String *, AbstractReflectSessionRef> iter(GetSessions()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->ComputerJustWokeUp();
   }

   for (HashtableIterator<IPAddressAndPort, ReflectSessionFactoryRef> iter(GetFactories()); iter.HasData(); iter++)
   {
      INetworkConfigChangesTarget * t = dynamic_cast<INetworkConfigChangesTarget *>(iter.GetValue()());
      if (t) t->ComputerJustWokeUp();
   }

   if (_notifyReflectServer)
   {
      ReflectServer * rs = GetOwner();
      if (rs) rs->ComputerJustWokeUp();
   }
}

void DetectNetworkConfigChangesSession :: NetworkInterfacesChanged(const Hashtable<String, Void> &)
{
   // default implementation is a no-op.
}

void DetectNetworkConfigChangesSession :: ComputerIsAboutToSleep()
{
   // default implementation is a no-op.
}

void DetectNetworkConfigChangesSession :: ComputerJustWokeUp()
{
   // default implementation is a no-op.
}

void DetectNetworkConfigChangesSession :: Pulse(const PulseArgs & pa)
{
   if (pa.GetCallbackTime() >= _callbackTime)
   {
      _callbackTime = MUSCLE_TIME_NEVER;
      if (_enabled) CallNetworkInterfacesChangedOnAllTargets(_changeAllPending ? Hashtable<String, Void>() : _pendingChangedInterfaceNames);
      _pendingChangedInterfaceNames.Clear();
      _changeAllPending = false;
   }
   AbstractReflectSession::Pulse(pa);
}

ConstSocketRef DetectNetworkConfigChangesSession :: CreateDefaultSocket()
{
#ifdef __linux__
   struct sockaddr_nl sa; memset(&sa, 0, sizeof(sa));
   sa.nl_family = AF_NETLINK;
   sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;

   ConstSocketRef ret = GetConstSocketRefFromPool(socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE));
   return ((ret())&&(bind(ret()->GetFileDescriptor(), (struct sockaddr*)&sa, sizeof(sa)) == 0)&&(SetSocketBlockingEnabled(ret, false).IsOK())) ? ret : ConstSocketRef();
#else
   return CreateConnectedSocketPair(_notifySocket, _waitSocket).IsOK() ? _waitSocket : ConstSocketRef();
#endif
}

int32 DetectNetworkConfigChangesSession :: DoInput(AbstractGatewayMessageReceiver & /*r*/, uint32 /*maxBytes*/)
{
   const int fd = GetSessionReadSelectSocket().GetFileDescriptor();
   if (fd < 0) return -1;

#ifdef __linux__
   bool sendReport = false;
   char buf[4096];
   struct iovec iov = {buf, sizeof(buf)};
   struct sockaddr_nl sa;
   struct msghdr msg = {(void *)&sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
   int msgLen = recvmsg(fd, &msg, 0);
   if (msgLen >= 0)  // FogBugz #9620
   {
      for (struct nlmsghdr *nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, (unsigned int)msgLen); nh=NLMSG_NEXT(nh, msgLen))
      {
         /* The end of multipart message. */
         if (nh->nlmsg_type == NLMSG_DONE) break;
         else
         {
            switch(nh->nlmsg_type)
            {
               case RTM_NEWLINK: case RTM_DELLINK:
               {
                  struct ifinfomsg * iface = (struct ifinfomsg *) NLMSG_DATA(nh);
                  int nextLen = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));
                  for (struct rtattr * a = IFLA_RTA(iface); RTA_OK(a, nextLen); a = RTA_NEXT(a, nextLen))
                     if (a->rta_type == IFLA_IFNAME)
                       (void) _pendingChangedInterfaceNames.PutWithDefault((const char *) RTA_DATA(a));
                  sendReport = true;
               }
               break; 

               case RTM_NEWADDR: case RTM_DELADDR:
               {
                  struct ifaddrmsg * ifa = (struct ifaddrmsg *) NLMSG_DATA(nh);
                  struct rtattr *rth = IFA_RTA(ifa);
                  int rtl = IFA_PAYLOAD(nh);
                  while(rtl && RTA_OK(rth, rtl))
                  {
                     if (rth->rta_type == IFA_LOCAL)
                     {
                        char ifName[IFNAMSIZ];
                        (void) if_indextoname(ifa->ifa_index, ifName);
                        (void) _pendingChangedInterfaceNames.PutWithDefault(ifName);
                        sendReport = true;  // FogBugz #17683:  only notify if IFA_LOCAL was specified, to avoid getting spammed about IFA_CACHEINFO
                     }
                     rth = RTA_NEXT(rth, rtl);
                  }
               }
               break; 

               default:
                  // do nothing
               break;
            }
         }
      }
   }
   if (sendReport) ScheduleSendReport();
   return msgLen;
#elif defined(USE_SINGLETON_THREAD)
   char buf[128]; 
   const int32 ret = ReceiveData(_waitSocket, buf, sizeof(buf), false);  // clear any received signalling bytes
   Queue<MessageRef> incomingMessages;
   {
      MutexGuard mg(_messagesFromSingletonThreadMutex);
      incomingMessages.SwapContents(_messagesFromSingletonThread);
   }

   bool sendReport = false;
   MessageRef msg;
   while(incomingMessages.RemoveHead(msg).IsOK())
   {
      switch(msg()->what)
      {
         case DNCCS_MESSAGE_INTERFACES_CHANGED:
            sendReport = true;  // we only need to send one report, even for multiple Messages
            if ((msg())&&(_changeAllPending == false))
            {
               if (msg()->HasName("if", B_STRING_TYPE))
               {
                  const String * ifName;
                  for (int32 i=0; msg()->FindString("if", i, &ifName).IsOK(); i++) _pendingChangedInterfaceNames.PutWithDefault(*ifName);
               }
               else _changeAllPending = true;  // no interfaces specified means "it could be anything"
            }
         break;

         case DNCCS_MESSAGE_ABOUT_TO_SLEEP:
            if (_enabled) CallComputerIsAboutToSleepOnAllTargets();
         break;

         case DNCCS_MESSAGE_JUST_WOKE_UP:
            if (_enabled) CallComputerJustWokeUpOnAllTargets();
         break;
      }
   }
   if (sendReport) ScheduleSendReport();
   return ret;
#else
   return -1;
#endif
}

status_t DetectNetworkConfigChangesSession :: AttachedToServer()
{
   status_t ret;
   if (AbstractReflectSession::AttachedToServer().IsOK(ret))
   {
#if defined(USE_SINGLETON_THREAD)
      return RegisterWithSingletonThread(this);
#endif
   }
   return ret;
}

void DetectNetworkConfigChangesSession :: EndSession()
{
#if defined(USE_SINGLETON_THREAD)
   UnRegisterFromSingletonThread(this);  // do this ASAP, otherwise we get the occasional crash on shutdown :(
#endif
   AbstractReflectSession::EndSession();
}

void DetectNetworkConfigChangesSession :: AboutToDetachFromServer()
{
#if defined(USE_SINGLETON_THREAD)
   UnRegisterFromSingletonThread(this);
#endif
   AbstractReflectSession::AboutToDetachFromServer();
}

}  // end namespace muscle
