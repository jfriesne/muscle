#include "iogateway/SignalMessageIOGateway.h"

#define FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED
#include "system/DetectNetworkConfigChangesSession.h"
#undef FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED

#ifdef __APPLE__
# include <mach/mach_port.h>
# include <mach/mach_interface.h>
# include <mach/mach_init.h>
# include <IOKit/pwr_mgt/IOPMLib.h>
# include <IOKit/IOMessage.h>
# include <SystemConfiguration/SystemConfiguration.h>
#elif WIN32
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
# include <sys/socket.h>
# include <linux/netlink.h>
# include <linux/rtnetlink.h>
#endif

namespace muscle {

#if defined(__APPLE__) || defined(WIN32)
static void SignalInterfacesChanged(DetectNetworkConfigChangesSession * s, const Hashtable<String, Void> & optInterfaceNames)
{
   MessageRef msg;  // demand-allocated
   if (optInterfaceNames.HasItems())
   {
      msg = GetMessageFromPool(DetectNetworkConfigChangesSession::DNCCS_MESSAGE_INTERFACES_CHANGED);
      if (msg()) for (HashtableIterator<String, Void> iter(optInterfaceNames); iter.HasData(); iter++) msg()->AddString("if", iter.GetKey());
   }
   static Message _msg(DetectNetworkConfigChangesSession::DNCCS_MESSAGE_INTERFACES_CHANGED);
   s->ThreadSafeSendMessageToOwner(msg() ? msg : MessageRef(&_msg, false));
}
#endif

#ifndef __linux__
status_t DetectNetworkConfigChangesSession :: ThreadSafeSendMessageToOwner(const MessageRef & msg)
{
#ifdef WIN32
   // Needed because Windows notification callbacks get called from various threads
   MutexGuard mg(_sendMessageToOwnerMutex);
#endif
   return SendMessageToOwner(msg);
}
#endif

DetectNetworkConfigChangesSession :: DetectNetworkConfigChangesSession() :
#ifndef __linux__
   _threadKeepGoing(false),
#endif
#ifdef __APPLE__
   _threadRunLoop(NULL), // paranoia
#elif WIN32
   _wakeupSignal(MY_INVALID_HANDLE_VALUE),
#endif
   _explicitDelayMicros(MUSCLE_TIME_NEVER),
   _callbackTime(MUSCLE_TIME_NEVER),
   _enabled(true),
   _changeAllPending(false),
   _isComputerSleeping(false)
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
      if (_enabled) NetworkInterfacesChanged(_changeAllPending ? Hashtable<String, Void>() : _pendingChangedInterfaceNames);
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
   return ((ret())&&(bind(ret()->GetFileDescriptor(), (struct sockaddr*)&sa, sizeof(sa)) == 0)&&(SetSocketBlockingEnabled(ret, false) == B_NO_ERROR)) ? ret : ConstSocketRef();
#else
   return GetOwnerWakeupSocket();
#endif
}

void DetectNetworkConfigChangesSession :: MessageReceivedFromGateway(const MessageRef & /*msg*/, void * /*ptr*/)
{
#ifndef __linux__
   bool sendReport = false;
   MessageRef ref;
   while(GetNextReplyFromInternalThread(ref) >= 0)
   {
      if (ref() == NULL)
      {
         LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesSession:  Internal thread exited!\n");
         continue;
      }

      switch(ref()->what)
      {
         case DNCCS_MESSAGE_INTERFACES_CHANGED:
            sendReport = true;  // we only need to send one report, even for multiple Messages
            if ((ref())&&(_changeAllPending == false))
            {
               if (ref()->HasName("if", B_STRING_TYPE))
               {
                  const String * ifName;
                  for (int32 i=0; ref()->FindString("if", i, &ifName) == B_NO_ERROR; i++) _pendingChangedInterfaceNames.PutWithDefault(*ifName);
               }
               else _changeAllPending = true;  // no interfaces specified means "it could be anything"
            }
         break;

         case DNCCS_MESSAGE_ABOUT_TO_SLEEP:
            ComputerIsAboutToSleep();
         break;

         case DNCCS_MESSAGE_JUST_WOKE_UP:
            ComputerJustWokeUp();
         break;
      }
   }
   if (sendReport) ScheduleSendReport();
#endif
}

#ifdef __linux__

int32 DetectNetworkConfigChangesSession :: DoInput(AbstractGatewayMessageReceiver & /*r*/, uint32 /*maxBytes*/)
{
   int fd = GetSessionReadSelectSocket().GetFileDescriptor();
   if (fd < 0) return -1;

   bool sendReport = false;
   char buf[4096];
   struct iovec iov = {buf, sizeof(buf)};
   struct sockaddr_nl sa;
   struct msghdr msg = {(void *)&sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
   int len = recvmsg(fd, &msg, 0);
   if (len >= 0)  // FogBugz #9620
   {
      for (struct nlmsghdr *nh = (struct nlmsghdr *)buf; ((sendReport == false)&&(NLMSG_OK(nh, (unsigned int)len))); nh=NLMSG_NEXT(nh, len))
      {
         /* The end of multipart message. */
         if (nh->nlmsg_type == NLMSG_DONE) break;
         else
         {
            switch(nh->nlmsg_type)
            {
               case RTM_NEWLINK: case RTM_DELLINK: case RTM_NEWADDR: case RTM_DELADDR:
               {
                  struct ifinfomsg * iface = (struct ifinfomsg *) NLMSG_DATA(nh);
                  int len = nh->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));
                  for (struct rtattr * a = IFLA_RTA(iface); RTA_OK(a, len); a = RTA_NEXT(a, len))
                     if (a->rta_type == IFLA_IFNAME)
                       _pendingChangedInterfaceNames.PutWithDefault((const char *) RTA_DATA(a));
                  sendReport = true;
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
   return len;
}

#else

status_t DetectNetworkConfigChangesSession :: AttachedToServer()
{
   _threadKeepGoing = true;
# ifdef __APPLE__
   _threadRunLoop = NULL;
# endif
# ifdef WIN32
   _wakeupSignal = CreateEvent(0, false, false, 0);
   if (_wakeupSignal == MY_INVALID_HANDLE_VALUE) return B_ERROR;
# endif
   return (AbstractReflectSession::AttachedToServer() == B_NO_ERROR) ? StartInternalThread() : B_ERROR;
}

void DetectNetworkConfigChangesSession :: EndSession()
{
   ShutdownInternalThread();  // do this ASAP, otherwise we get the occasional crash on shutdown :(
   AbstractReflectSession::EndSession();
}

void DetectNetworkConfigChangesSession :: AboutToDetachFromServer()
{
   ShutdownInternalThread();
#ifdef WIN32
   if (_wakeupSignal != MY_INVALID_HANDLE_VALUE)
   {
      CloseHandle(_wakeupSignal);
      _wakeupSignal = MY_INVALID_HANDLE_VALUE;
   }
#endif
   AbstractReflectSession::AboutToDetachFromServer();
}

AbstractMessageIOGatewayRef DetectNetworkConfigChangesSession :: CreateGateway()
{
   AbstractMessageIOGateway * gw = newnothrow SignalMessageIOGateway;
   if (gw == NULL) WARN_OUT_OF_MEMORY;
   return AbstractMessageIOGatewayRef(gw);
}

void DetectNetworkConfigChangesSession :: SignalInternalThread()
{
   _threadKeepGoing = false;
   Thread::SignalInternalThread();
# ifdef __APPLE__
   if (_threadRunLoop) CFRunLoopStop(_threadRunLoop);
# elif WIN32
   SetEvent(_wakeupSignal);
# endif
}

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
   const CFStringRef keyStr = (const CFStringRef) key;
   const CFDictionaryRef propList = (const CFDictionaryRef) value;
   if ((keyStr)&&(propList))
   {
      String k = CFStringGetCStringPtr(keyStr, kCFStringEncodingMacRoman);
      if (k.StartsWith("State:/Network/Interface/"))
      {
         String interfaceName = k.Substring(25).Substring(0, "/");
         (void) ((Hashtable<String, String> *)(context))->Put(k, interfaceName);
      }
      else
      {
         const CFStringRef interfaceNameRef = (const CFStringRef) CFDictionaryGetValue((CFDictionaryRef) propList, CFSTR("InterfaceName"));
         if (interfaceNameRef)
         {
            String v = CFStringGetCStringPtr(interfaceNameRef, kCFStringEncodingMacRoman);
            (void) ((Hashtable<String, String> *)(context))->Put(k, v);
         }
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

   if (err == noErr) err = MoreSCErrorBoolean(SCDynamicStoreSetNotificationKeys(ref, NULL, patternList));
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

static void IPConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void * info)
{
   DetectNetworkConfigChangesSession * s = (DetectNetworkConfigChangesSession *) info;
   Hashtable<String, Void> changedInterfaceNames;

   int c = CFArrayGetCount(changedKeys);
   for (int i=0; i<c; i++)
   {
      CFStringRef p = (CFStringRef) CFArrayGetValueAtIndex(changedKeys, i);
      if (p)  // paranoia
      {
         String keyStr = CFStringGetCStringPtr( p, kCFStringEncodingMacRoman);

         String interfaceName;
         if (keyStr.StartsWith("State:/Network/Interface/")) interfaceName = keyStr.Substring(25).Substring(0, "/");
         else
         {
            CFPropertyListRef propList = SCDynamicStoreCopyValue(store, p);
            if (propList)
            {
               CFStringRef inStr = (CFStringRef) CFDictionaryGetValue((CFDictionaryRef) propList, CFSTR("InterfaceName"));
               if (inStr) interfaceName = CFStringGetCStringPtr(inStr, kCFStringEncodingMacRoman);
               CFRelease(propList);
            }
         }

         Hashtable<String, String> & scKeyToInterfaceName = GetKeyToInterfaceNameTable(s);
         if (interfaceName.HasChars()) (void) scKeyToInterfaceName.Put(keyStr, interfaceName);
                                  else interfaceName = scKeyToInterfaceName.RemoveWithDefault(keyStr);
         if (interfaceName.HasChars()) (void) changedInterfaceNames.PutWithDefault(interfaceName);
      }
   }

   SignalInterfacesChanged(s, changedInterfaceNames);
}

Hashtable<String, String> & GetKeyToInterfaceNameTable(DetectNetworkConfigChangesSession * s)
{
   return s->_scKeyToInterfaceName;
}

# endif  // __APPLE__

#if defined(WIN32) && !defined(MUSCLE_AVOID_NETIOAPI)

static void SignalInterfacesChangedAux(void * context, uint32 changedIdx)
{
   Hashtable<String, Void> iNames;
   Queue<NetworkInterfaceInfo> q;
   if (GetNetworkInterfaceInfos(q) == B_NO_ERROR)
   {
      for (uint32 i=0; i<q.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = q[i];
         if (nii.GetLocalAddress().GetInterfaceIndex() == changedIdx) iNames.PutWithDefault(nii.GetName());
      }
   }
   SignalInterfacesChanged((DetectNetworkConfigChangesSession *) context, iNames);
}

VOID __stdcall AddressCallbackDemo(IN PVOID context, IN PMIB_UNICASTIPADDRESS_ROW Address OPTIONAL, IN MIB_NOTIFICATION_TYPE /*NotificationType*/)
{
   if (Address != NULL) SignalInterfacesChangedAux(context, Address->InterfaceIndex);
}

VOID __stdcall InterfaceCallbackDemo(IN PVOID context, IN PMIB_IPINTERFACE_ROW interfaceRow, IN MIB_NOTIFICATION_TYPE /*NotificationType*/)
{
   if (interfaceRow != NULL) SignalInterfacesChangedAux(context, interfaceRow->InterfaceIndex);
}

#endif

#if defined(__APPLE__) || defined(WIN32)
static void MySleepCallbackAux(DetectNetworkConfigChangesSession * s, bool isGoingToSleep) {s->MySleepCallbackAux(isGoingToSleep);}

void DetectNetworkConfigChangesSession :: MySleepCallbackAux(bool isAboutToSleep)
{
   if (isAboutToSleep != _isComputerSleeping)
   {
      _isComputerSleeping = isAboutToSleep;

      static Message _aboutToSleepMessage(DNCCS_MESSAGE_ABOUT_TO_SLEEP);
      static Message _justWokeUpMessage(DNCCS_MESSAGE_JUST_WOKE_UP);
      (void) ThreadSafeSendMessageToOwner(MessageRef(isAboutToSleep ? &_aboutToSleepMessage : &_justWokeUpMessage, false));
   }
}
#endif

#ifdef __APPLE__
static void * GetRootPortPointerFromSession(const DetectNetworkConfigChangesSession * s) {return s->_rootPortPointer;}
static void MySleepCallBack(void * refCon, io_service_t /*service*/, natural_t messageType, void * messageArgument)
{
   //printf("messageType %08lx, arg %08lx\n", (long unsigned int)messageType, (long unsigned int)messageArgument);
   DetectNetworkConfigChangesSession * s = (DetectNetworkConfigChangesSession *)refCon;
   io_connect_t * root_port = (io_connect_t *) GetRootPortPointerFromSession(s);

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
         //IOCancelPowerChange(*root_port, (long)messageArgument);

         // we will allow idle sleep
         IOAllowPowerChange(*root_port, (long)messageArgument);
       break;

       case kIOMessageSystemWillSleep:
          /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
             IOCancelPowerChange to acknowledge this message, sleep will be
             delayed by 30 seconds.

             NOTE: If you call IOCancelPowerChange to deny sleep it returns
             kIOReturnSuccess, however the system WILL still go to sleep.
          */
          MySleepCallbackAux(s, true);
          IOAllowPowerChange(*root_port, (long)messageArgument);
       break;

       case kIOMessageSystemWillPowerOn:
          //System has started the wake up process...
       break;

       case kIOMessageSystemHasPoweredOn:
          // System has finished waking up...
          MySleepCallbackAux(s, false);
       break;

       default:
          // empty
       break;
   }
}

#endif

#ifdef WIN32
static LRESULT CALLBACK window_message_handler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   if (message == WM_POWERBROADCAST)
   {
      switch(wParam)
      {
         case PBT_APMRESUMEAUTOMATIC: case PBT_APMRESUMESUSPEND: case PBT_APMQUERYSUSPENDFAILED: case PBT_APMRESUMECRITICAL:
            MySleepCallbackAux((DetectNetworkConfigChangesSession *)(GetWindowLongPtr(hWnd, GWL_USERDATA)), false);
         break;

         case PBT_APMQUERYSUSPEND: case PBT_APMSUSPEND:
            MySleepCallbackAux((DetectNetworkConfigChangesSession *)(GetWindowLongPtr(hWnd, GWL_USERDATA)), true);
         break;

         default:
            // empty
         break;
      }
   }
   return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

void DetectNetworkConfigChangesSession :: InternalThreadEntry()
{
# ifdef __APPLE__
   _threadRunLoop = CFRunLoopGetCurrent();

   // notification port allocated by IORegisterForSystemPower
   IONotificationPortRef powerNotifyPortRef = NULL;
   io_object_t notifierObject;    // notifier object, used to deregister later
   void * refCon = (void *) this; // this parameter is passed to the callback
   CFRunLoopSourceRef powerNotifyRunLoopSource = NULL;

   // register to receive system sleep notifications
   io_connect_t root_port = IORegisterForSystemPower(refCon, &powerNotifyPortRef, MySleepCallBack, &notifierObject);
   _rootPortPointer = &root_port;
   if (root_port != 0)
   {
      powerNotifyRunLoopSource = IONotificationPortGetRunLoopSource(powerNotifyPortRef);
      CFRunLoopAddSource(_threadRunLoop, powerNotifyRunLoopSource, kCFRunLoopCommonModes);
   }
   else LogTime(MUSCLE_LOG_WARNING, "DetectNetworkConfigChangesSession::InternalThreadEntry():  IORegisterForSystemPower() failed\n");

   SCDynamicStoreRef storeRef = NULL;
   CFRunLoopSourceRef sourceRef = NULL;
   if (CreateIPAddressListChangeCallbackSCF(IPConfigChangedCallback, this, &storeRef, &sourceRef, _scKeyToInterfaceName) == noErr)
   {
      CFRunLoopAddSource(CFRunLoopGetCurrent(), sourceRef, kCFRunLoopDefaultMode);
      while(_threadKeepGoing)
      {
         CFRunLoopRun();
         while(1)
         {
            MessageRef msgRef;
            int32 numLeft = WaitForNextMessageFromOwner(msgRef, 0);
            if (numLeft >= 0)
            {
               if (MessageReceivedFromOwner(msgRef, numLeft) != B_NO_ERROR) _threadKeepGoing = false;
            }
            else break;
         }
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

# elif WIN32
#define WINDOW_CLASS_NAME   L"DetectNetworkConfigChangesSession_HiddenWndClass"
#define WINDOW_MENU_NAME    L"DetectNetworkConfigChangesSession_MainMenu"

   // Gotta create a hidden window to receive WM_POWERBROADCAST events, lame-o!
   // Register the window class for the main window.
   WNDCLASS window_class; memset(&window_class, 0, sizeof(window_class));
   window_class.style          = 0;
   window_class.lpfnWndProc    = (WNDPROC) window_message_handler;
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
   HWND hiddenWindow = CreateWindowW(WINDOW_CLASS_NAME, L"", WS_OVERLAPPEDWINDOW, -1, -1, 0, 0, (HWND)NULL, (HMENU) NULL, NULL, (LPVOID)NULL);
   if (hiddenWindow) SetWindowLongPtr(hiddenWindow, GWL_USERDATA, (LONG) this);  // ok because we don't need access to (this) in WM_CREATE
                else LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesSession::InternalThreadEntry():  CreateWindow() failed!\n");

# ifndef MUSCLE_AVOID_NETIOAPI
   HANDLE handle1 = MY_INVALID_HANDLE_VALUE; (void) NotifyUnicastIpAddressChange(AF_UNSPEC, &AddressCallbackDemo,   this, FALSE, &handle1);
   HANDLE handle2 = MY_INVALID_HANDLE_VALUE; (void) NotifyIpInterfaceChange(     AF_UNSPEC, &InterfaceCallbackDemo, this, FALSE, &handle2);
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
               ThreadSafeSendMessageToOwner(MessageRef(&_msg, false));
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
            LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesSession:  NotifyAddrChange() failed, code %i (%i)\n", nacRet, WSAGetLastError());
            break;
         }
      }
      CloseHandle(olap.hEvent);
   }
   else LogTime(MUSCLE_LOG_ERROR, "DetectNetworkConfigChangesSession:  CreateEvent() failed\n");

# ifndef MUSCLE_AVOID_NETIOAPI
   if (handle2 != MY_INVALID_HANDLE_VALUE) CancelMibChangeNotify2(handle2);
   if (handle1 != MY_INVALID_HANDLE_VALUE) CancelMibChangeNotify2(handle1);
# endif

   if (hiddenWindow) DestroyWindow(hiddenWindow);
   // Deliberately leaving the window_class registered here, since to do otherwise could be thread-unsafe

# else
#  error "NetworkInterfacesSession:  OS not supported!"
# endif
}

#endif  // !__linux__

};  // end namespace muscle


