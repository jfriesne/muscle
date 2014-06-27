#include "iogateway/SignalMessageIOGateway.h"

#define FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED
#include "system/DetectNetworkConfigChangesSession.h"
#undef FORWARD_DECLARE_SIGNAL_INTERFACES_CHANGED

#ifdef __APPLE__
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
      msg = GetMessageFromPool();
      if (msg()) for (HashtableIterator<String, Void> iter(optInterfaceNames); iter.HasData(); iter++) msg()->AddString("if", iter.GetKey());
   }

#ifdef WIN32
   // Needed because Windows notification callbacks get called from various threads
   MutexGuard mg(s->_sendMessageToOwnerMutex);
#endif
   s->SendMessageToOwner(msg);
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
   _changeAllPending(false)
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

void DetectNetworkConfigChangesSession :: InternalThreadEntry()
{
# ifdef __APPLE__
   _threadRunLoop = CFRunLoopGetCurrent();

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
# elif WIN32

# ifndef MUSCLE_AVOID_NETIOAPI
   HANDLE handle1 = MY_INVALID_HANDLE_VALUE;
   HANDLE handle2 = MY_INVALID_HANDLE_VALUE;
   (void) NotifyUnicastIpAddressChange(AF_UNSPEC, &AddressCallbackDemo,   this, FALSE, &handle1);
   (void) NotifyIpInterfaceChange(     AF_UNSPEC, &InterfaceCallbackDemo, this, FALSE, &handle2);
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
            ::HANDLE events[] = {olap.hEvent, _wakeupSignal};
            switch(WaitForMultipleObjects(ARRAYITEMS(events), events, false, INFINITE))
            {
               case WAIT_OBJECT_0: 
               {        
                  // Serialized since the NotifyUnicast*Change() callbacks
                  // get called from a different thread
                  MutexGuard mg(_sendMessageToOwnerMutex);
                  SendMessageToOwner(MessageRef());
               }          
               break;

               default:
                  (void) CancelIPChangeNotify(&olap);
                  _threadKeepGoing = false;
               break;
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

# else
#  error "NetworkInterfacesSession:  OS not supported!"
# endif
}

#endif  // !__linux__

};  // end namespace muscle


