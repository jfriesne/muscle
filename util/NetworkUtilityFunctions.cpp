/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "util/MiscUtilityFunctions.h"  // for GetConnectString() (which is deliberately defined here)
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"

#if defined(__BEOS__) || defined(__HAIKU__)
# include <kernel/OS.h>     // for snooze()
#elif __ATHEOS__
# include <atheos/kernel.h> // for snooze()
#endif

#ifdef WIN32
# include <iphlpapi.h>
# include <mswsock.h>  // for SIO_UDP_CONNRESET, etc
# include <ws2tcpip.h>
# ifndef MUSCLE_AVOID_MULTICAST_API
#  include <ws2ipdef.h>  // for IP_MULTICAST_LOOP, etc
# endif
# pragma warning(disable: 4800 4018)
typedef char sockopt_arg;  // Windows setsockopt()/getsockopt() use char pointers
#else
typedef void sockopt_arg;  // Whereas sane operating systems use void pointers
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
# ifndef BEOS_OLD_NETSERVER
#  include <net/if.h>
# endif
# include <sys/ioctl.h>
# ifdef BEOS_OLD_NETSERVER
#  include <app/Roster.h>     // for the run-time bone check
#  include <storage/Entry.h>  // for the backup run-time bone check
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/tcp.h>
# endif
#endif

#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__) || defined(__linux__)
# if defined(ANDROID) // JFM
#  define USE_SOCKETPAIR 1
# else
#  define USE_GETIFADDRS 1
#  define USE_SOCKETPAIR 1
#  include <ifaddrs.h>
# endif
#endif

#include "system/GlobalMemoryAllocator.h"  // for muscleAlloc()/muscleFree()

namespace muscle {

#ifndef MUSCLE_AVOID_IPV6
static bool _automaticIPv4AddressMappingEnabled = true;   // if true, we automatically translate IPv4-compatible addresses (e.g. ::192.168.0.1) to IPv4-mapped-IPv6 addresses (e.g. ::ffff:192.168.0.1) and back
void SetAutomaticIPv4AddressMappingEnabled(bool e) {_automaticIPv4AddressMappingEnabled = e;}
bool GetAutomaticIPv4AddressMappingEnabled()       {return _automaticIPv4AddressMappingEnabled;}

# define MUSCLE_SOCKET_FAMILY AF_INET6
static inline void GET_SOCKADDR_IP(const struct sockaddr_in6 & sockAddr, ip_address & ipAddr)
{
   switch(sockAddr.sin6_family)
   {
      case AF_INET6:
      {
         uint32 tmp = sockAddr.sin6_scope_id;  // MacOS/X uses __uint32_t, which isn't quite the same somehow
         ipAddr.ReadFromNetworkArray(sockAddr.sin6_addr.s6_addr, &tmp);
         if ((_automaticIPv4AddressMappingEnabled)&&(ipAddr != localhostIP)&&(IsValidAddress(ipAddr))&&(IsIPv4Address(ipAddr))) ipAddr.SetLowBits(ipAddr.GetLowBits() & ((uint64)0xFFFFFFFF));  // remove IPv4-mapped-IPv6-bits
      }
      break;

      case AF_INET:
      {
         uint32 ipAddr32 = ntohl(((const struct sockaddr_in &)sockAddr).sin_addr.s_addr);
         ipAddr.SetBits(ipAddr32, 0);
         ipAddr.SetInterfaceIndex(0);
      }
      break;

      default:
         // empty
      break;
  }
}
static inline void SET_SOCKADDR_IP(struct sockaddr_in6 & sockAddr, const ip_address & ipAddr)
{
   uint32 tmp;  // MacOS/X uses __uint32_t, which isn't quite the same somehow
   if ((_automaticIPv4AddressMappingEnabled)&&(ipAddr != localhostIP)&&(IsValidAddress(ipAddr))&&(IsIPv4Address(ipAddr)))
   {
      ip_address tmpAddr = ipAddr;
      tmpAddr.SetLowBits(tmpAddr.GetLowBits() | (((uint64)0xFFFF)<<32));  // add IPv4-mapped-IPv6-bits
      tmpAddr.WriteToNetworkArray(sockAddr.sin6_addr.s6_addr, &tmp);
   }
   else ipAddr.WriteToNetworkArray(sockAddr.sin6_addr.s6_addr, &tmp);

   sockAddr.sin6_scope_id = tmp;
}
static inline uint16 GET_SOCKADDR_PORT(const struct sockaddr_in6 & addr) 
{
   switch(addr.sin6_family)
   {
      case AF_INET6: return ntohs(addr.sin6_port);
      case AF_INET:  return ntohs(((const struct sockaddr_in &)addr).sin_port);
      default:       return 0;
   }
}

static inline void SET_SOCKADDR_PORT(struct sockaddr_in6 & addr, uint16 port) {addr.sin6_port = htons(port);}
static inline uint16 GET_SOCKADDR_FAMILY(const struct sockaddr_in6 & addr) {return addr.sin6_family;}
static inline void SET_SOCKADDR_FAMILY(struct sockaddr_in6 & addr, uint16 family) {addr.sin6_family = family;}
static void InitializeSockAddr6(struct sockaddr_in6 & addr, const ip_address * optFrom, uint16 port)
{
   memset(&addr, 0, sizeof(struct sockaddr_in6));
#ifdef SIN6_LEN
   addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
   SET_SOCKADDR_FAMILY(addr, MUSCLE_SOCKET_FAMILY);
   if (optFrom) SET_SOCKADDR_IP(addr, *optFrom);
   if (port) SET_SOCKADDR_PORT(addr, port);
}
# define DECLARE_SOCKADDR(addr, ip, port) struct sockaddr_in6 addr; InitializeSockAddr6(addr, ip, port);
#else
# define MUSCLE_SOCKET_FAMILY AF_INET
static inline void GET_SOCKADDR_IP(const struct sockaddr_in & sockAddr, ip_address & ipAddr) {ipAddr = ntohl(sockAddr.sin_addr.s_addr);}
static inline void SET_SOCKADDR_IP(struct sockaddr_in & sockAddr, const ip_address & ipAddr) {sockAddr.sin_addr.s_addr = htonl(ipAddr);}
static inline uint16 GET_SOCKADDR_PORT(const struct sockaddr_in & addr) {return ntohs(addr.sin_port);}
static inline void SET_SOCKADDR_PORT(struct sockaddr_in & addr, uint16 port) {addr.sin_port = htons(port);}
static inline uint16 GET_SOCKADDR_FAMILY(const struct sockaddr_in & addr) {return addr.sin_family;}
static inline void SET_SOCKADDR_FAMILY(struct sockaddr_in & addr, uint16 family) {addr.sin_family = family;}
static void InitializeSockAddr4(struct sockaddr_in & addr, const ip_address * optFrom, uint16 port)
{
   memset(&addr, 0, sizeof(struct sockaddr_in));
   SET_SOCKADDR_FAMILY(addr, MUSCLE_SOCKET_FAMILY);
   if (optFrom) SET_SOCKADDR_IP(addr, *optFrom);
   if (port) SET_SOCKADDR_PORT(addr, port);
}
# define DECLARE_SOCKADDR(addr, ip, port) struct sockaddr_in addr; InitializeSockAddr4(addr, ip, port);
#endif

static GlobalSocketCallback * _globalSocketCallback = NULL;
void SetGlobalSocketCallback(GlobalSocketCallback * cb) {_globalSocketCallback = cb;}
GlobalSocketCallback * GetGlobalSocketCallback() {return _globalSocketCallback;}

static status_t DoGlobalSocketCallback(uint32 eventType, const ConstSocketRef & s)
{
   if (s() == NULL) return B_ERROR;
   if (_globalSocketCallback == NULL) return B_NO_ERROR;
   return _globalSocketCallback->SocketCallback(eventType, s);
}

static ConstSocketRef CreateMuscleSocket(int socketType, uint32 createType)
{
   int s = (int) socket(MUSCLE_SOCKET_FAMILY, socketType, 0);
   if (s >= 0)
   {
      ConstSocketRef ret = GetConstSocketRefFromPool(s);
      if (ret())
      {
#ifndef MUSCLE_AVOID_IPV6
         // If we're doing IPv4-mapped IPv6 addresses, we gotta tell Windows 7 that it's okay to use them, otherwise they won't work.  :^P
         if (_automaticIPv4AddressMappingEnabled)
         {
            int v6OnlyEnabled = 0;  // we want v6-only mode disabled, which is to say we want v6-to-v4 compatibility
            if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const sockopt_arg *) &v6OnlyEnabled, sizeof(v6OnlyEnabled)) != 0) LogTime(MUSCLE_LOG_DEBUG, "Could not disable v6-only mode for socket %i\n", s);
         }
#endif
         if (DoGlobalSocketCallback(createType, ret) == B_NO_ERROR) return ret;
      }
   }
   return ConstSocketRef();
}

ConstSocketRef CreateUDPSocket()
{
   ConstSocketRef ret = CreateMuscleSocket(SOCK_DGRAM, GlobalSocketCallback::SOCKET_CALLBACK_CREATE_UDP);
#if defined(WIN32) && !defined(__MINGW32__)
   if (ret())
   {
      // This setup code avoids the UDP WSAECONNRESET problem
      // described at http://support.microsoft.com/kb/263823/en-us
      DWORD dwBytesReturned = 0;
      BOOL bNewBehavior = FALSE;
      if (WSAIoctl(ret.GetFileDescriptor(), SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL) != 0) ret.Reset();
   }
#endif
   return ret;
}

status_t BindUDPSocket(const ConstSocketRef & sock, uint16 port, uint16 * optRetPort, const ip_address & optFrom, bool allowShared)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   if (allowShared)
   {
      const int trueValue = 1;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const sockopt_arg *) &trueValue, sizeof(trueValue));
#ifdef __APPLE__
      setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const sockopt_arg *) &trueValue, sizeof(trueValue));
#endif
   }

   DECLARE_SOCKADDR(saSocket, &optFrom, port);
   if (bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) == 0)
   {
      if (optRetPort)
      {
         muscle_socklen_t len = sizeof(saSocket);
         if (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0)
         {
            *optRetPort = GET_SOCKADDR_PORT(saSocket);
            return B_NO_ERROR;
         }
         else return B_ERROR;
      }
      return B_NO_ERROR;
   }
   else return B_ERROR;
}

status_t SetUDPSocketTarget(const ConstSocketRef & sock, const ip_address & remoteIP, uint16 remotePort)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   DECLARE_SOCKADDR(saAddr, &remoteIP, remotePort);
   return (connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr)) == 0) ? B_NO_ERROR : B_ERROR;
}

status_t SetUDPSocketTarget(const ConstSocketRef & sock, const char * remoteHostName, uint16 remotePort, bool expandLocalhost)
{
   ip_address hostIP = GetHostByName(remoteHostName, expandLocalhost);
   return (hostIP != invalidIP) ? SetUDPSocketTarget(sock, hostIP, remotePort) : B_ERROR;
}

ConstSocketRef CreateAcceptingSocket(uint16 port, int maxbacklog, uint16 * optRetPort, const ip_address & optInterfaceIP)
{
   ConstSocketRef ret = CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CREATE_ACCEPTING);
   if (ret())
   {
      int fd = ret.GetFileDescriptor();

#ifndef WIN32
      // (Not necessary under windows -- it has the behaviour we want by default)
      const int trueValue = 1;
      (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const sockopt_arg *) &trueValue, sizeof(trueValue));
#endif

      DECLARE_SOCKADDR(saSocket, &optInterfaceIP, port);
      if ((bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) == 0)&&(listen(fd, maxbacklog) == 0))
      {
         if (optRetPort)
         {
            muscle_socklen_t len = sizeof(saSocket);
            *optRetPort = (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0) ? GET_SOCKADDR_PORT(saSocket) : 0;
         }
         return ret;
      }
   }
   return ConstSocketRef();  // failure
}

int32 ReceiveData(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm)
{
   int fd = sock.GetFileDescriptor();
   return (fd >= 0) ? ConvertReturnValueToMuscleSemantics(recv_ignore_eintr(fd, (char *)buffer, size, 0L), size, bm) : -1;
}

int32 ReadData(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm)
{
#ifdef WIN32
   return ReceiveData(sock, buffer, size, bm);  // Windows doesn't support read(), only recv()
#else
   int fd = sock.GetFileDescriptor();
   return (fd >= 0) ? ConvertReturnValueToMuscleSemantics(read_ignore_eintr(fd, (char *)buffer, size), size, bm) : -1;
#endif
}

int32 ReceiveDataUDP(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm, ip_address * optFromIP, uint16 * optFromPort)
{
   int fd = sock.GetFileDescriptor();
   if (fd >= 0)
   {
      int r;
      if ((optFromIP)||(optFromPort))
      {
         DECLARE_SOCKADDR(fromAddr, NULL, 0);
         muscle_socklen_t fromAddrLen = sizeof(fromAddr);
         r = recvfrom_ignore_eintr(fd, (char *)buffer, size, 0L, (struct sockaddr *) &fromAddr, &fromAddrLen);
         if (r >= 0)
         {
            if (optFromIP) GET_SOCKADDR_IP(fromAddr, *optFromIP);
            if (optFromPort) *optFromPort = GET_SOCKADDR_PORT(fromAddr);
         }
      }
      else r = recv_ignore_eintr(fd, (char *)buffer, size, 0L);

      if (r == 0) return 0;  // for UDP, zero is a valid recv() size, since there is no EOS
      return ConvertReturnValueToMuscleSemantics(r, size, bm);
   }
   else return -1;
}

int32 SendData(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm)
{
   int fd = sock.GetFileDescriptor();
   return (fd >= 0) ? ConvertReturnValueToMuscleSemantics(send_ignore_eintr(fd, (const char *)buffer, size, 0L), size, bm) : -1;
}

int32 WriteData(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm)
{
#ifdef WIN32
   return SendData(sock, buffer, size, bm);  // Windows doesn't support write(), only send()
#else
   int fd = sock.GetFileDescriptor();
   return (fd >= 0) ? ConvertReturnValueToMuscleSemantics(write_ignore_eintr(fd, (const char *)buffer, size), size, bm) : -1;
#endif
}

int32 SendDataUDP(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm, const ip_address & optToIP, uint16 optToPort)
{
#ifdef DEBUG_SENDING_UDP_PACKETS_ON_INTERFACE_ZERO
   if ((optToIP != invalidIP)&&(optToIP.GetInterfaceIndex() == 0)&&(IsIPv4Address(optToIP) == false)&&(IsStandardLoopbackDeviceAddress(optToIP) == false))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "SendDataUDP:  Sending to interface 0!  [%s]:%u\n", Inet_NtoA(optToIP)(), optToPort);
      PrintStackTrace();
   }
#endif

#if !defined(MUSCLE_AVOID_IPV6) && !defined(MUSCLE_AVOID_MULTICAST_API)
# define MUSCLE_USE_IFIDX_WORKAROUND 1
#endif

   int fd = sock.GetFileDescriptor();
   if (fd >= 0)
   {
#ifdef MUSCLE_USE_IFIDX_WORKAROUND
      int oldInterfaceIndex = -1;  // and remember to set it back afterwards
#endif

      int s;
      if ((optToPort)||(optToIP != invalidIP))
      {
         DECLARE_SOCKADDR(toAddr, NULL, 0);
         if ((optToPort == 0)||(optToIP == invalidIP))
         {
            // Fill in the values with our socket's current target-values, as defaults
            muscle_socklen_t length = sizeof(sockaddr_in);
            if ((getpeername(fd, (struct sockaddr *)&toAddr, &length) != 0)||(GET_SOCKADDR_FAMILY(toAddr) != MUSCLE_SOCKET_FAMILY)) return -1;
         }

         if (optToIP != invalidIP) 
         {
            SET_SOCKADDR_IP(toAddr, optToIP);
#ifdef MUSCLE_USE_IFIDX_WORKAROUND
            // Work-around for MacOS/X problem (?) where the interface index in the specified IP address doesn't get used
            if ((optToIP.GetInterfaceIndex() != 0)&&(IsMulticastIPAddress(optToIP)))
            {
               int oidx = GetSocketMulticastSendInterfaceIndex(sock);
               if (oidx != (int) optToIP.GetInterfaceIndex())
               {
                  // temporarily set the socket's interface index to the desired one
                  if (SetSocketMulticastSendInterfaceIndex(sock, optToIP.GetInterfaceIndex()) != B_NO_ERROR) return -1;
                  oldInterfaceIndex = oidx;  // and remember to set it back afterwards
               }
            }
#endif
         }
         if (optToPort) SET_SOCKADDR_PORT(toAddr, optToPort);
         s = sendto_ignore_eintr(fd, (const char *)buffer, size, 0L, (struct sockaddr *)&toAddr, sizeof(toAddr));
      }
      else s = send_ignore_eintr(fd, (const char *)buffer, size, 0L);

      if (s == 0) return 0;  // for UDP, zero is a valid send() size, since there is no EOS
      int32 ret = ConvertReturnValueToMuscleSemantics(s, size, bm);
#ifdef MUSCLE_USE_IFIDX_WORKAROUND
      if (oldInterfaceIndex >= 0) (void) SetSocketMulticastSendInterfaceIndex(sock, oldInterfaceIndex);  // gotta do this AFTER computing the return value, as it clears errno!
#endif
      return ret;
   }
   else return -1;
}

status_t ShutdownSocket(const ConstSocketRef & sock, bool dRecv, bool dSend)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   if ((dRecv == false)&&(dSend == false)) return B_NO_ERROR;  // there's nothing we need to do!

   // Since these constants aren't defined everywhere, I'll define my own:
   const int MUSCLE_SHUT_RD   = 0;
   const int MUSCLE_SHUT_WR   = 1;
   const int MUSCLE_SHUT_RDWR = 2;

   return (shutdown(fd, dRecv?(dSend?MUSCLE_SHUT_RDWR:MUSCLE_SHUT_RD):MUSCLE_SHUT_WR) == 0) ? B_NO_ERROR : B_ERROR;
}

ConstSocketRef Accept(const ConstSocketRef & sock, ip_address * optRetInterfaceIP)
{
   DECLARE_SOCKADDR(saSocket, NULL, 0);
   muscle_socklen_t nLen = sizeof(saSocket);
   int fd = sock.GetFileDescriptor();
   if (fd >= 0)
   {
      ConstSocketRef ret = GetConstSocketRefFromPool((int) accept(fd, (struct sockaddr *)&saSocket, &nLen));
      if (DoGlobalSocketCallback(GlobalSocketCallback::SOCKET_CALLBACK_ACCEPT, ret) != B_NO_ERROR) return ConstSocketRef();  // called separately since accept() created this socket, not CreateMuscleSocket()

      if ((ret())&&(optRetInterfaceIP))
      {
         muscle_socklen_t len = sizeof(saSocket);
         if (getsockname(ret.GetFileDescriptor(), (struct sockaddr *)&saSocket, &len) == 0) GET_SOCKADDR_IP(saSocket, *optRetInterfaceIP);
                                                                                       else *optRetInterfaceIP = invalidIP;
      }
      return ret;
   }
   return ConstSocketRef();  // failure
}

ConstSocketRef Connect(const char * hostName, uint16 port, const char * debugTitle, bool errorsOnly, uint64 maxConnectTime, bool expandLocalhost)
{
   ip_address hostIP = GetHostByName(hostName, expandLocalhost);
   if (hostIP != invalidIP) return Connect(hostIP, port, hostName, debugTitle, errorsOnly, maxConnectTime);
   else
   {
      if (debugTitle) LogTime(MUSCLE_LOG_INFO, "%s: hostname lookup for [%s] failed!\n", debugTitle, hostName);
      return ConstSocketRef();
   }
}

ConstSocketRef Connect(const ip_address & hostIP, uint16 port, const char * optDebugHostName, const char * debugTitle, bool errorsOnly, uint64 maxConnectTime)
{
   char ipbuf[64]; Inet_NtoA(hostIP, ipbuf);

   if ((debugTitle)&&(errorsOnly == false))
   {
      LogTime(MUSCLE_LOG_INFO, "%s: Connecting to %s: ", debugTitle, GetConnectString(optDebugHostName?optDebugHostName:ipbuf, port)());
      LogFlush();
   }

   bool socketIsReady = false;
   ConstSocketRef s = (maxConnectTime == MUSCLE_TIME_NEVER) ? CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CONNECT) : ConnectAsync(hostIP, port, socketIsReady);
   if (s())
   {
      int fd = s.GetFileDescriptor();
      int ret = -1;
      if (maxConnectTime == MUSCLE_TIME_NEVER)
      {
         DECLARE_SOCKADDR(saAddr, &hostIP, port);
         ret = connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr));
      }
      else
      {
         if (socketIsReady) ret = 0;  // immediate success, yay!
         else
         {
            // The harder case:  the user doesn't want the Connect() call to take more than (so many) microseconds.
            // For this, we'll need to go into non-blocking mode and run a SocketMultiplexer loop to get the desired behaviour!
            const uint64 deadline = GetRunTime64()+maxConnectTime;
            SocketMultiplexer multiplexer; 
            uint64 now;
            while((now = GetRunTime64()) < deadline)
            {
               multiplexer.RegisterSocketForWriteReady(fd);
#ifdef WIN32
               multiplexer.RegisterSocketForExceptionRaised(fd);
#endif
               if (multiplexer.WaitForEvents(deadline) < 0) break;  // error out!
               else
               {
#ifdef WIN32
                  if (multiplexer.IsSocketExceptionRaised(fd)) break; // Win32:  failed async connect detected!
#endif
                  if (multiplexer.IsSocketReadyForWrite(fd))
                  {
                     if ((FinalizeAsyncConnect(s) == B_NO_ERROR)&&(SetSocketBlockingEnabled(s, true) == B_NO_ERROR)) ret = 0;
                     break;
                  }
               }
            }
         }
      }

      if (ret == 0)
      {
         if ((debugTitle)&&(errorsOnly == false)) Log(MUSCLE_LOG_INFO, "Connected!\n");
         return s;
      }
      else if (debugTitle)
      {
         if (errorsOnly) LogTime(MUSCLE_LOG_INFO, "%s: connect() to %s failed!\n", debugTitle, GetConnectString(optDebugHostName?optDebugHostName:ipbuf, port)());
                    else Log(MUSCLE_LOG_INFO, "Connection failed!\n");
      }
   }
   else if (debugTitle)
   {
      if (errorsOnly) LogTime(MUSCLE_LOG_INFO, "%s: socket() failed!\n", debugTitle);
                 else Log(MUSCLE_LOG_INFO, "socket() failed!\n");
   }
   return ConstSocketRef();
}

String GetLocalHostName()
{
   char buf[512];
   return (gethostname(buf, sizeof(buf)) == 0) ? buf : "";
}

static bool IsIP4Address(const char * s)
{
   int numDots     = 0;
   int numDigits   = 0;
   bool prevWasDot = true;  // an initial dot is illegal
   while(*s)
   {
      if (*s == '.')
      {
         if ((prevWasDot)||(++numDots > 3)) return false;
         numDigits  = 0;
         prevWasDot = true;
      }
      else
      {
         if ((prevWasDot)&&(atoi(s) > 255)) return false;
         prevWasDot = false;
         if ((muscleInRange(*s, '0', '9') == false)||(++numDigits > 3)) return false;
      }
      s++;
   }
   return (numDots == 3);
}

#ifndef MUSCLE_AVOID_IPV6

static const char * Inet_NtoP(int af, const void * src, char * dst, muscle_socklen_t size)
{
#ifdef WIN32
   switch(af)
   {
      case AF_INET:
      {
         struct sockaddr_in in;
         memset(&in, 0, sizeof(in));
         in.sin_family = AF_INET;
         memcpy(&in.sin_addr, src, sizeof(struct in_addr));
         return (getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, size, NULL, 0, NI_NUMERICHOST) == 0) ? dst : NULL;
      }

      case AF_INET6:
      {
         struct sockaddr_in6 in;
         memset(&in, 0, sizeof(in));
         in.sin6_family = AF_INET6;
         memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
         return (getnameinfo((struct sockaddr *)&in, sizeof(in), dst, size, NULL, 0, NI_NUMERICHOST) == 0) ? dst : NULL;
      }

      default:
         return NULL;
   }
#else
   return inet_ntop(af, src, dst, size);
#endif
}

static int Inet_PtoN(int af, const char * src, struct in6_addr * dst)
{
#ifdef WIN32
   struct addrinfo hints; memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = af;
   hints.ai_flags  = AI_NUMERICHOST;

   bool ok = false;  // default
   struct addrinfo * res;
   if (getaddrinfo(src, NULL, &hints, &res) != 0) return -1;
   if (res)
   {
      switch(res->ai_family)
      {
         case AF_INET:
            if (res->ai_addrlen == sizeof(struct sockaddr_in))
            {
               struct sockaddr_in * sin = (struct sockaddr_in *) res->ai_addr;
               memset(dst, 0, sizeof(*dst));
               // Copy IPv4 bits to the low word of the IPv6 address
               memcpy(((char *)dst)+12, &sin->sin_addr.s_addr, sizeof(uint32));

               // And make it IPv6-mapped-to-IPv4
               memset(((char *)dst)+10, 0xFF, 2);
               ok = true;
            }
         break;

         case AF_INET6:
            if (res->ai_addrlen == sizeof(struct sockaddr_in6))
            {
               struct sockaddr_in6 * sin6 = (struct sockaddr_in6 *) res->ai_addr;
               memcpy(dst, &sin6->sin6_addr, sizeof(*dst));
               ok = true;
            }
         break;
      }
      freeaddrinfo(res);
   }
   return ok ? 1 : -1;
#else
   return inet_pton(af, src, dst);
#endif
}

#endif

bool IsIPAddress(const char * s)
{
#ifdef MUSCLE_AVOID_IPV6
   return IsIP4Address(s);
#else
   struct in6_addr tmp;
   return ((Inet_PtoN(MUSCLE_SOCKET_FAMILY, s, &tmp) > 0)||(Inet_AtoN(s) != invalidIP)||(IsIP4Address(s)));  // Inet_AtoN() handles the "@blah" suffixes
#endif
}

static ip_address _cachedLocalhostAddress = invalidIP;

static void ExpandLocalhostAddress(ip_address & ipAddress)
{
   if (IsStandardLoopbackDeviceAddress(ipAddress))
   {
      ip_address altRet = GetLocalHostIPOverride();  // see if the user manually specified a preferred local address
      if (altRet == invalidIP)
      {
         // If not, try to grab one from the OS
         if (_cachedLocalhostAddress == invalidIP)
         {
            Queue<NetworkInterfaceInfo> ifs;
            (void) GetNetworkInterfaceInfos(ifs, GNII_INCLUDE_ENABLED_INTERFACES|GNII_INCLUDE_NONLOOPBACK_INTERFACES|GNII_INCLUDE_MUSCLE_PREFERRED_INTERFACES);  // just to set _cachedLocalhostAddress
         }
         altRet = _cachedLocalhostAddress;
      }
      if (altRet != invalidIP) ipAddress = altRet;
   }
}

// This stores the result of a GetHostByName() lookup, along with its expiration time.
class DNSRecord
{
public:
   DNSRecord() : _expirationTime(0) {/* empty */}
   DNSRecord(const ip_address & ip, uint64 expTime) : _ipAddress(ip), _expirationTime(expTime) {/* empty */}

   const ip_address & GetIPAddress() const {return _ipAddress;}
   uint64 GetExpirationTime() const {return _expirationTime;}

private:
   ip_address _ipAddress;
   uint64 _expirationTime;
};

static Mutex _hostCacheMutex;                // serialize access to the cache data
static uint32 _maxHostCacheSize = 0;         // DNS caching is disabled by default
static uint64 _hostCacheEntryLifespan = 0;   // how many microseconds before a cache entry is expired
static Hashtable<String, DNSRecord> _hostCache;

void SetHostNameCacheSettings(uint32 maxEntries, uint64 expirationTime)
{
   MutexGuard mg(_hostCacheMutex);
   _maxHostCacheSize = expirationTime ? maxEntries : 0;  // no sense storing entries that expire instantly
   _hostCacheEntryLifespan = expirationTime;
   while(_hostCache.GetNumItems() > _maxHostCacheSize) (void) _hostCache.RemoveLast();
}

static String GetHostByNameKey(const char * name, bool expandLocalhost, bool preferIPv6)
{
   String ret(name);
   ret = ret.ToLowerCase();
   if (expandLocalhost) ret += '!';  // so that a cached result from GetHostByName("foo", false) won't be returned for GetHostByName("foo", true)
   if (preferIPv6)      ret += '?';  // ditto
   return ret;
}

ip_address GetHostByName(const char * name, bool expandLocalhost, bool preferIPv6)
{
   if (IsIPAddress(name))
   {
      // No point in ever caching this result, since Inet_AtoN() is always fast anyway
      ip_address ret = Inet_AtoN(name);
      if (expandLocalhost) ExpandLocalhostAddress(ret);
      return ret;
   }
   else if (_maxHostCacheSize > 0)
   {
      String s = GetHostByNameKey(name, expandLocalhost, preferIPv6);
      MutexGuard mg(_hostCacheMutex);
      const DNSRecord * r = _hostCache.Get(s);
      if (r)
      {
         if ((r->GetExpirationTime() == MUSCLE_TIME_NEVER)||(GetRunTime64() < r->GetExpirationTime()))
         {
            (void) _hostCache.MoveToFront(s);  // LRU logic
            return r->GetIPAddress();
         }
      }
   }

   ip_address ret = invalidIP;
#ifdef MUSCLE_AVOID_IPV6
   struct hostent * he = gethostbyname(name);
   if (he) ret = ntohl(*((ip_address*)he->h_addr));
#else
   struct addrinfo * result;
   struct addrinfo hints; memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_UNSPEC;     // We're not too particular, for now
   hints.ai_socktype = SOCK_STREAM;   // so we don't get every address twice (once for UDP and once for TCP)
   ip_address ret6 = invalidIP;
   if (getaddrinfo(name, NULL, &hints, &result) == 0)
   {
      struct addrinfo * next = result;
      while(next)
      {
         switch(next->ai_family)
         {
            case AF_INET:
               if (IsValidAddress(ret) == false)
               {
                  ret.SetBits(ntohl(((struct sockaddr_in *) next->ai_addr)->sin_addr.s_addr), 0);  // read IPv4 address into low bits of IPv6 address structure
                  ret.SetLowBits(ret.GetLowBits() | ((uint64)0xFFFF)<<32);                         // and make it IPv6-mapped (why doesn't AI_V4MAPPED do this?)
               }
            break;

            case AF_INET6:
               if (IsValidAddress(ret6) == false)
               {
                  struct sockaddr_in6 * sin6 = (struct sockaddr_in6 *) next->ai_addr;
                  uint32 tmp = sin6->sin6_scope_id;  // MacOS/X uses __uint32_t, which isn't quite the same somehow
                  ret6.ReadFromNetworkArray(sin6->sin6_addr.s6_addr, &tmp);
               }
            break;
         }
         next = next->ai_next;
      }
      freeaddrinfo(result);

      if (IsValidAddress(ret))
      {
         if ((preferIPv6)&&(IsValidAddress(ret6))) ret = ret6;
      }
      else ret = ret6;
   }
#endif

   if (expandLocalhost) ExpandLocalhostAddress(ret);

   if (_maxHostCacheSize > 0)
   {
      // Store our result in the cache for later
      String s = GetHostByNameKey(name, expandLocalhost, preferIPv6);
      MutexGuard mg(_hostCacheMutex);
      DNSRecord * r = _hostCache.PutAndGet(s, DNSRecord(ret, (_hostCacheEntryLifespan==MUSCLE_TIME_NEVER)?MUSCLE_TIME_NEVER:(GetRunTime64()+_hostCacheEntryLifespan)));
      if (r)
      {
         _hostCache.MoveToFront(s);  // LRU logic
         while(_hostCache.GetNumItems() > _maxHostCacheSize) (void) _hostCache.RemoveLast();
      }
   }

   return ret;
}

ConstSocketRef ConnectAsync(const ip_address & hostIP, uint16 port, bool & retIsReady)
{
   ConstSocketRef s = CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CONNECT);
   if (s())
   {
      if (SetSocketBlockingEnabled(s, false) == B_NO_ERROR)
      {
         DECLARE_SOCKADDR(saAddr, &hostIP, port);
         int result = connect(s.GetFileDescriptor(), (struct sockaddr *) &saAddr, sizeof(saAddr));
#ifdef WIN32
         bool inProgress = ((result < 0)&&(WSAGetLastError() == WSAEWOULDBLOCK));
#else
         bool inProgress = ((result < 0)&&(errno == EINPROGRESS));
#endif
         if ((result == 0)||(inProgress))
         {
            retIsReady = (inProgress == false);
            return s;
         }
      }
   }
   return ConstSocketRef();
}

ip_address GetPeerIPAddress(const ConstSocketRef & sock, bool expandLocalhost, uint16 * optRetPort)
{
   ip_address ipAddress = invalidIP;
   int fd = sock.GetFileDescriptor();
   if (fd >= 0)
   {
      DECLARE_SOCKADDR(saTempAdd, NULL, 0);
      muscle_socklen_t length = sizeof(saTempAdd);
      if ((getpeername(fd, (struct sockaddr *)&saTempAdd, &length) == 0)&&(GET_SOCKADDR_FAMILY(saTempAdd) == MUSCLE_SOCKET_FAMILY))
      {
         GET_SOCKADDR_IP(saTempAdd, ipAddress);
         if (optRetPort) *optRetPort = GET_SOCKADDR_PORT(saTempAdd);
         if (expandLocalhost) ExpandLocalhostAddress(ipAddress);
      }
   }
   return ipAddress;
}

/* See the header file for description of what this does */
status_t CreateConnectedSocketPair(ConstSocketRef & socket1, ConstSocketRef & socket2, bool blocking)
{
   TCHECKPOINT;

#if defined(USE_SOCKETPAIR)
   int temp[2];
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, temp) == 0)
   {
      socket1 = GetConstSocketRefFromPool(temp[0]);
      socket2 = GetConstSocketRefFromPool(temp[1]);
      if ((SetSocketBlockingEnabled(socket1, blocking) == B_NO_ERROR)&&(SetSocketBlockingEnabled(socket2, blocking) == B_NO_ERROR)) return B_NO_ERROR;
   }
#else
   uint16 port;
   socket1 = CreateAcceptingSocket(0, 1, &port, localhostIP);
   if (socket1())
   {
      socket2 = Connect(localhostIP, port);
      if (socket2())
      {
         ConstSocketRef newfd = Accept(socket1);
         if (newfd())
         {
            socket1 = newfd;
            if ((SetSocketBlockingEnabled(socket1, blocking) == B_NO_ERROR)&&(SetSocketBlockingEnabled(socket2, blocking) == B_NO_ERROR))
            {
               (void) SetSocketNaglesAlgorithmEnabled(socket1, false);
               (void) SetSocketNaglesAlgorithmEnabled(socket2, false);
               return B_NO_ERROR;
            }
         }
      }
   }
#endif

   socket1.Reset();
   socket2.Reset();
   return B_ERROR;
}

status_t SetSocketBlockingEnabled(const ConstSocketRef & sock, bool blocking)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

#ifdef WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? B_NO_ERROR : B_ERROR;
#else
# ifdef BEOS_OLD_NETSERVER
   int b = blocking ? 0 : 1;
   return (setsockopt(fd, SOL_SOCKET, SO_NONBLOCK, (const sockopt_arg *) &b, sizeof(b)) == 0) ? B_NO_ERROR : B_ERROR;
# else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return B_ERROR;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? B_NO_ERROR : B_ERROR;
# endif
#endif
}

bool GetSocketBlockingEnabled(const ConstSocketRef & sock)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

#ifdef WIN32
   // Amazingly enough, Windows has no way to query if a socket is blocking or not.  So this function doesn't work under Windows.
   LogTime(MUSCLE_LOG_ERROR, "GetSocketBlockingEnabled() not implemented under Win32, returning false for socket %i.\n", fd);
   return false;
#else
# ifdef BEOS_OLD_NETSERVER
   int b;
   int len = sizeof(b);
   return (getsockopt(fd, SOL_SOCKET, SO_NONBLOCK, (sockopt_arg *) &b, &len) == 0) ? (b==0) : false;
# else
   int flags = fcntl(fd, F_GETFL, 0);
   return ((flags >= 0)&&((flags & O_NONBLOCK) == 0));
# endif
#endif
}

status_t SetUDPSocketBroadcastEnabled(const ConstSocketRef & sock, bool broadcast)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   int val = (broadcast ? 1 : 0);
#ifdef BEOS_OLD_NETSERVER
   return (setsockopt(fd, SOL_SOCKET, INADDR_BROADCAST, (const sockopt_arg *) &val, sizeof(val)) == 0) ? B_NO_ERROR : B_ERROR;
#else
   return (setsockopt(fd, SOL_SOCKET, SO_BROADCAST,     (const sockopt_arg *) &val, sizeof(val)) == 0) ? B_NO_ERROR : B_ERROR;
#endif
}

bool GetUDPSocketBroadcastEnabled(const ConstSocketRef & sock)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

   int val;
   socklen_t len = sizeof(val);
#ifdef BEOS_OLD_NETSERVER
   return (getsockopt(fd, SOL_SOCKET, INADDR_BROADCAST, (sockopt_arg *) &val, &len) == 0) ? (val != 0) : false;
#else
   return (getsockopt(fd, SOL_SOCKET, SO_BROADCAST,     (sockopt_arg *) &val, &len) == 0) ? (val != 0) : false;
#endif
}

status_t SetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock, bool enabled)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

#ifdef BEOS_OLD_NETSERVER
   (void) enabled;  // prevent 'unused var' warning
   return B_ERROR;  // old networking stack doesn't support this flag
#else
   int enableNoDelay = enabled ? 0 : 1;
   return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const sockopt_arg *) &enableNoDelay, sizeof(enableNoDelay)) == 0) ? B_NO_ERROR : B_ERROR;
#endif
}

bool GetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

#ifdef BEOS_OLD_NETSERVER
   return true;  // old networking stack doesn't support this flag
#else
   int enableNoDelay;
   socklen_t len = sizeof(enableNoDelay);
   return (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (sockopt_arg *) &enableNoDelay, &len) == 0) ? (enableNoDelay == 0) : false;
#endif
}

status_t FinalizeAsyncConnect(const ConstSocketRef & sock)
{
   TCHECKPOINT;

   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

#if defined(BEOS_OLD_NETSERVER)
   // net_server and BONE behave COMPLETELY differently as far as finalizing async connects
   // go... so we have to do this horrible hack where we figure out whether we're in a
   // true net_server or BONE environment at runtime.  Pretend you didn't see any of this,
   // you'll sleep better.  :^P
   static bool userIsRunningBone    = false;
   static bool haveDoneRuntimeCheck = false;
   if (haveDoneRuntimeCheck == false)
   {
      userIsRunningBone = be_roster ? (!be_roster->IsRunning("application/x-vnd.Be-NETS")) : BEntry("/boot/beos/system/lib/libsocket.so").Exists();
      haveDoneRuntimeCheck = true;  // only do this check once, it's rather expensive!
   }
   if (userIsRunningBone)
   {
      char junk;
      return (send_ignore_eintr(fd, &junk, 0, 0L) == 0) ? B_NO_ERROR : B_ERROR;
   }
   else
   {
      // net_server just HAS to do things differently from everyone else :^P
      struct sockaddr_in junk;
      memset(&junk, 0, sizeof(junk));
      return (connect(fd, (struct sockaddr *) &junk, sizeof(junk)) == 0) ? B_NO_ERROR : B_ERROR;
   }
#elif defined(__FreeBSD__) || defined(BSD)
   // Nathan Whitehorn reports that send() doesn't do this trick under FreeBSD 7,
   // so for BSD we'll call getpeername() instead.  -- jaf
   struct sockaddr_in junk;
   socklen_t length = sizeof(junk);
   memset(&junk, 0, sizeof(junk));
   return (getpeername(fd, (struct sockaddr *)&junk, &length) == 0) ? B_NO_ERROR : B_ERROR;
#else
   // For most platforms, the code below is all we need
   char junk;
   return (send_ignore_eintr(fd, &junk, 0, 0L) == 0) ? B_NO_ERROR : B_ERROR;
#endif
}

static status_t SetSocketBufferSizeAux(const ConstSocketRef & sock, uint32 numBytes, int optionName)
{
#ifdef BEOS_OLD_NETSERVER
   (void) sock;
   (void) numBytes;
   (void) optionName;
   return B_ERROR;  // not supported!
#else
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   int iSize = (int) numBytes;
   return (setsockopt(fd, SOL_SOCKET, optionName, (const sockopt_arg *) &iSize, sizeof(iSize)) == 0) ? B_NO_ERROR : B_ERROR;
#endif
}
status_t SetSocketSendBufferSize(   const ConstSocketRef & sock, uint32 sendBufferSizeBytes) {return SetSocketBufferSizeAux(sock, sendBufferSizeBytes, SO_SNDBUF);}
status_t SetSocketReceiveBufferSize(const ConstSocketRef & sock, uint32 recvBufferSizeBytes) {return SetSocketBufferSizeAux(sock, recvBufferSizeBytes, SO_RCVBUF);}

static int32 GetSocketBufferSizeAux(const ConstSocketRef & sock, int optionName)
{
#ifdef BEOS_OLD_NETSERVER
   (void) sock;
   (void) optionName;
   return -1;  // not supported!
#else
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return -1;

   int iSize;
   socklen_t len = sizeof(iSize);
   return (getsockopt(fd, SOL_SOCKET, optionName, (sockopt_arg *) &iSize, &len) == 0) ? (int32)iSize : -1;
#endif
}
int32 GetSocketSendBufferSize(   const ConstSocketRef & sock) {return GetSocketBufferSizeAux(sock, SO_SNDBUF);}
int32 GetSocketReceiveBufferSize(const ConstSocketRef & sock) {return GetSocketBufferSizeAux(sock, SO_RCVBUF);}

NetworkInterfaceInfo :: NetworkInterfaceInfo() : _ip(invalidIP), _netmask(invalidIP), _broadcastIP(invalidIP), _enabled(false), _copper(false)
{
   // empty
}

NetworkInterfaceInfo :: NetworkInterfaceInfo(const String &name, const String & desc, const ip_address & ip, const ip_address & netmask, const ip_address & broadcastIP, bool enabled, bool copper) : _name(name), _desc(desc), _ip(ip), _netmask(netmask), _broadcastIP(broadcastIP), _enabled(enabled), _copper(copper)
{
   // empty
}

String NetworkInterfaceInfo :: ToString() const
{
   return String("Name=[%1] Description=[%2] IP=[%3] Netmask=[%4] Broadcast=[%5] Enabled=%6 Copper=%7").Arg(_name).Arg(_desc).Arg(Inet_NtoA(_ip)).Arg(Inet_NtoA(_netmask)).Arg(Inet_NtoA(_broadcastIP)).Arg(_enabled).Arg(_copper);
}

uint32 NetworkInterfaceInfo :: HashCode() const
{
   return _name.HashCode() + _desc.HashCode() + GetHashCodeForIPAddress(_ip) + GetHashCodeForIPAddress(_netmask) + GetHashCodeForIPAddress(_broadcastIP) + _enabled + _copper;
}

#if defined(USE_GETIFADDRS) || defined(WIN32)
static ip_address SockAddrToIPAddr(const struct sockaddr * a)
{
   if (a)
   {
      switch(a->sa_family)
      {
         case AF_INET:  return ip_address(ntohl(((struct sockaddr_in *)a)->sin_addr.s_addr));

#ifndef MUSCLE_AVOID_IPV6
         case AF_INET6:
         {
            struct sockaddr_in6 * sin6 = (struct sockaddr_in6 *) a;
            ip_address ret;
            uint32 tmp = sin6->sin6_scope_id;  // MacOS/X uses __uint32_t, which isn't quite the same somehow
            ret.ReadFromNetworkArray(sin6->sin6_addr.s6_addr, &tmp);
            return ret;
         }
#endif
      }
   }
   return invalidIP;
}
#endif

bool IsIPv4Address(const ip_address & ip)
{
#ifdef MUSCLE_AVOID_IPV6
   (void) ip;
   return true;
#else
   if ((ip == invalidIP)||(ip == localhostIP)) return false;  // :: and ::1 are considered to be IPv6 addresses

   if (ip.GetHighBits() != 0) return false;
   uint64 lb = (ip.GetLowBits()>>32);
   return ((lb == 0)||(lb == 0xFFFF));  // 32-bit and IPv4-mapped, respectively
#endif
}

bool IsValidAddress(const ip_address & ip)
{
#ifdef MUSCLE_AVOID_IPV6
   return (ip != 0);
#else
   return ((ip.GetHighBits() != 0)||(ip.GetLowBits() != 0));
#endif
}

bool IsMulticastIPAddress(const ip_address & ip)
{
#ifndef MUSCLE_AVOID_IPV6
   // In IPv6, any address that starts with 0xFF is a multicast address
   if (((ip.GetHighBits() >> 56)&((uint64)0xFF)) == 0xFF) return true;

   const uint64 mapBits = (((uint64)0xFFFF)<<32);
   if ((ip.GetHighBits() == 0)&&((ip.GetLowBits() & mapBits) == mapBits))
   {
      ip_address temp = ip; temp.SetLowBits(temp.GetLowBits() & ~mapBits);
      return IsMulticastIPAddress(temp);  // don't count the map-to-IPv6 bits when determining multicast-ness
   }
#endif

   // check for IPv4 address-ness
   ip_address minMulticastAddress = Inet_AtoN("224.0.0.0");
   ip_address maxMulticastAddress = Inet_AtoN("239.255.255.255");
   return muscleInRange(ip, minMulticastAddress, maxMulticastAddress);
}

bool IsStandardLoopbackDeviceAddress(const ip_address & ip)
{
#ifdef MUSCLE_AVOID_IPV6
   return (ip == localhostIP);
#else
   // fe80::1 is another name for localhostIP in IPv6 land
   static const ip_address localhostIP_linkScope(localhostIP.GetLowBits(), ((uint64)0xFE80)<<48);
   return ((ip.EqualsIgnoreInterfaceIndex(localhostIP))||(ip.EqualsIgnoreInterfaceIndex(localhostIP_IPv4))||(ip.EqualsIgnoreInterfaceIndex(localhostIP_linkScope)));
#endif
}

static bool IsGNIIBitMatch(const ip_address & ip, bool isInterfaceEnabled, uint32 includeBits)
{
   if (((includeBits & GNII_INCLUDE_ENABLED_INTERFACES)  == 0)&&( isInterfaceEnabled)) return false;
   if (((includeBits & GNII_INCLUDE_DISABLED_INTERFACES) == 0)&&(!isInterfaceEnabled)) return false;

   if (ip == invalidIP)
   {
      if ((includeBits & GNII_INCLUDE_UNADDRESSED_INTERFACES) == 0) return false;  // FogBugz #10286
   }
   else
   {
      bool isLoopback = IsStandardLoopbackDeviceAddress(ip);
      if (((includeBits & GNII_INCLUDE_LOOPBACK_INTERFACES)    == 0)&&( isLoopback)) return false;
      if (((includeBits & GNII_INCLUDE_NONLOOPBACK_INTERFACES) == 0)&&(!isLoopback)) return false;

      bool isIPv4 = IsIPv4Address(ip);
      if (( isIPv4)&&((includeBits & GNII_INCLUDE_IPV4_INTERFACES) == 0)) return false;
      if ((!isIPv4)&&((includeBits & GNII_INCLUDE_IPV6_INTERFACES) == 0)) return false;
   }

   return true;
}

status_t GetNetworkInterfaceInfos(Queue<NetworkInterfaceInfo> & results, uint32 includeBits)
{
   uint32 origResultsSize = results.GetNumItems();
   status_t ret = B_ERROR;

#if defined(USE_GETIFADDRS)
   /////////////////////////////////////////////////////////////////////////////////////////////
   // "Apparently, getifaddrs() is gaining a lot of traction at becoming the One True Standard
   //  way of getting at interface info, so you're likely to find support for it on most modern
   //  Unix-like systems, at least..."
   //
   // http://www.developerweb.net/forum/showthread.php?t=5085
   /////////////////////////////////////////////////////////////////////////////////////////////

   struct ifaddrs * ifap;

   if (getifaddrs(&ifap) == 0)
   {
      ret = B_NO_ERROR;
      {
         struct ifaddrs * p = ifap;
         while(p)
         {
            ip_address unicastIP   = SockAddrToIPAddr(p->ifa_addr);
            ip_address netmask     = SockAddrToIPAddr(p->ifa_netmask);
            ip_address broadcastIP = SockAddrToIPAddr(p->ifa_broadaddr);
            bool isEnabled         = ((p->ifa_flags & IFF_UP)      != 0);
            bool hasCopper         = ((p->ifa_flags & IFF_RUNNING) != 0);
            if (IsGNIIBitMatch(unicastIP, isEnabled, includeBits))
            {
#ifndef MUSCLE_AVOID_IPV6
               unicastIP.SetInterfaceIndex(if_nametoindex(p->ifa_name));  // so the user can find out; it will be ignore by the TCP stack
#endif
               if (results.AddTail(NetworkInterfaceInfo(p->ifa_name, "", unicastIP, netmask, broadcastIP, isEnabled, hasCopper)) == B_NO_ERROR)
               {
                  if (_cachedLocalhostAddress == invalidIP) _cachedLocalhostAddress = unicastIP;
               }
               else
               {
                  ret = B_ERROR;  // out of memory!?
                  break;
               }
            }
            p = p->ifa_next;
         }
      }
      freeifaddrs(ifap);
   }
#elif defined(WIN32)
   // IPv6 implementation, adapted from
   // http://msdn.microsoft.com/en-us/library/aa365915(VS.85).aspx
   //
   SOCKET s = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
   if (s == INVALID_SOCKET) return B_ERROR;

   INTERFACE_INFO localAddrs[64];  // Assume there will be no more than 64 IP interfaces
   DWORD bytesReturned;
   if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0, &localAddrs, sizeof(localAddrs), &bytesReturned, NULL, NULL) == SOCKET_ERROR)
   {
      closesocket(s);
      return B_ERROR;
   }
   else closesocket(s);

   PIP_ADAPTER_ADDRESSES pAddresses = NULL;
   ULONG outBufLen = 0;
   while(ret != B_NO_ERROR)  // keep going until we succeeded (on failure we'll return directly)
   {
      DWORD flags = GAA_FLAG_INCLUDE_PREFIX|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER;
      switch(GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &outBufLen))
      {
         case ERROR_BUFFER_OVERFLOW:
            if (pAddresses) muscleFree(pAddresses);

            // Add some breathing room so that if the required size
            // grows during the time period between the two calls to
            // GetAdaptersAddresses(), we won't have to reallocate again.
            outBufLen *= 2;

            pAddresses = (IP_ADAPTER_ADDRESSES *) muscleAlloc(outBufLen);
            if (pAddresses == NULL)
            {
               WARN_OUT_OF_MEMORY;
               return B_ERROR;
            }
         break;

         case ERROR_SUCCESS:
            for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses; ((pCurrAddresses = pCurrAddresses->Next) != NULL))
            {
               PIP_ADAPTER_UNICAST_ADDRESS ua = pCurrAddresses->FirstUnicastAddress;
               while(ua)
               {
                  ip_address unicastIP = SockAddrToIPAddr(ua->Address.lpSockaddr);
                  bool isEnabled = true;  // for now.  TODO:  See if GetAdaptersAddresses() reports disabled interfaces
                  if (IsGNIIBitMatch(unicastIP, isEnabled, includeBits))
                  {
                     ip_address broadcastIP, netmask;
                     uint32 numLocalAddrs = (bytesReturned/sizeof(INTERFACE_INFO));
                     for (int i=0; i<numLocalAddrs; i++)
                     {
                        ip_address nextIP = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiAddress);
                        if (nextIP == unicastIP)
                        {
                           broadcastIP = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiBroadcastAddress);
                           netmask     = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiNetmask);
                           break;
                        }
                     }

#ifndef MUSCLE_AVOID_IPV6
                     unicastIP.SetInterfaceIndex(pCurrAddresses->Ipv6IfIndex);  // so the user can find out; it will be ignore by the TCP stack
#endif

                     char outBuf[512];
                     if (WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, outBuf, sizeof(outBuf), NULL, NULL) <= 0) outBuf[0] = '\0';

                     if (results.AddTail(NetworkInterfaceInfo(pCurrAddresses->AdapterName, outBuf, unicastIP, netmask, broadcastIP, isEnabled, false)) == B_NO_ERROR)
                     {
                        if (_cachedLocalhostAddress == invalidIP) _cachedLocalhostAddress = unicastIP;
                     }
                     else
                     {
                        if (pAddresses) muscleFree(pAddresses);
                        return B_ERROR;
                     }
                  }
                  ua = ua->Next;
               }
            }
            if (pAddresses) muscleFree(pAddresses);
            ret = B_NO_ERROR;  // this will drop us out of the loop
         break;

         default:
           if (pAddresses) muscleFree(pAddresses);
           return B_ERROR;
      }
   }
#else
   (void) results;  // for other OS's, this function isn't implemented.
#endif

   return ((ret == B_NO_ERROR)&&(results.GetNumItems() == origResultsSize)&&(includeBits & GNII_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT)) ? GetNetworkInterfaceInfos(results, (includeBits|GNII_INCLUDE_LOOPBACK_INTERFACES)&~(GNII_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT)) : ret;
}

status_t GetNetworkInterfaceAddresses(Queue<ip_address> & results, uint32 includeBits)
{
   Queue<NetworkInterfaceInfo> infos;
   if ((GetNetworkInterfaceInfos(infos, includeBits) != B_NO_ERROR)||(results.EnsureSize(infos.GetNumItems()) != B_NO_ERROR)) return B_ERROR;

   for (uint32 i=0; i<infos.GetNumItems(); i++) (void) results.AddTail(infos[i].GetLocalAddress());  // guaranteed not to fail
   return B_NO_ERROR;
}

static void Inet4_NtoA(uint32 addr, char * buf)
{
   sprintf(buf, INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC, (addr>>24)&0xFF, (addr>>16)&0xFF, (addr>>8)&0xFF, (addr>>0)&0xFF);
}

void Inet_NtoA(const ip_address & addr, char * ipbuf, bool preferIPv4)
{
#ifdef MUSCLE_AVOID_IPV6
   (void) preferIPv4;
   Inet4_NtoA(addr, ipbuf);
#else
   if ((preferIPv4)&&(IsIPv4Address(addr))) Inet4_NtoA(addr.GetLowBits()&0xFFFFFFFF, ipbuf);
   else
   {
      uint32 iIdx = 0;
      uint8 ip6[16]; addr.WriteToNetworkArray(ip6, &iIdx);
      if (Inet_NtoP(AF_INET6, (const in6_addr *) ip6, ipbuf, 46) != NULL)
      {
         if (iIdx > 0)
         {
            // Add the index suffix
            char buf[32]; sprintf(buf, "@" UINT32_FORMAT_SPEC, iIdx);
            strcat(ipbuf, buf);
         }
      }
      else ipbuf[0] = '\0';
   }
#endif
}

String Inet_NtoA(const ip_address & ipAddress, bool preferIPv4)
{
   char buf[64]; 
   Inet_NtoA(ipAddress, buf, preferIPv4);
   return buf;
}

static uint32 Inet4_AtoN(const char * buf)
{
   // net_server inexplicably doesn't have this function; so I'll just fake it
   uint32 ret = 0;
   int shift = 24;  // fill out the MSB first
   bool startQuad = true;
   while((shift >= 0)&&(*buf))
   {
      if (startQuad)
      {
         uint8 quad = (uint8) atoi(buf);
         ret |= (((uint32)quad) << shift);
         shift -= 8;
      }
      startQuad = (*buf == '.');
      buf++;
   }
   return ret;
}

#ifndef MUSCLE_AVOID_IPV6
static ip_address Inet6_AtoN(const char * buf, uint32 iIdx)
{
   struct in6_addr dst;
   if (Inet_PtoN(AF_INET6, buf, &dst) > 0)
   {
      ip_address ret;
      ret.ReadFromNetworkArray(dst.s6_addr, &iIdx);

      return ret;
   }
   else return (IsIP4Address(buf)) ? ip_address(Inet4_AtoN(buf), 0) : invalidIP;
}
#endif

ip_address Inet_AtoN(const char * buf)
{
#ifdef MUSCLE_AVOID_IPV6
   return Inet4_AtoN(buf);
#else
   const char * at = strchr(buf, '@');
   if (at)
   {
      // Gah... Inet_PtoN() won't accept the @idx suffix, so
      // I have to chop that out and parse it separately.  What a pain.
      uint32 charsBeforeAt = (uint32)(at-buf);
      char * tmp = newnothrow_array(char, 1+charsBeforeAt);
      if (tmp)
      {
         memcpy(tmp, buf, charsBeforeAt);
         tmp[charsBeforeAt] = '\0';
         ip_address ret = Inet6_AtoN(tmp, atoi(at+1));
         delete [] tmp;
         return ret;
      }
      else
      {
         WARN_OUT_OF_MEMORY;
         return invalidIP;
      }
   }
   else return Inet6_AtoN(buf, 0);
#endif
}

static ip_address ResolveIP(const String & s, bool allowDNSLookups)
{
   return allowDNSLookups ? GetHostByName(s()) : Inet_AtoN(s());
}

void IPAddressAndPort :: SetFromString(const String & s, uint16 defaultPort, bool allowDNSLookups)
{
#ifndef MUSCLE_AVOID_IPV6
   int32 rBracket = s.StartsWith('[') ? s.IndexOf(']') : -1;
   if (rBracket >= 0)
   {
      // If there are brackets, they are assumed to surround the address part, e.g. "[::1]:9999"
      _ip = ResolveIP(s.Substring(1,rBracket), allowDNSLookups);

      int32 colIdx = s.IndexOf(':', rBracket+1);
      _port = ((colIdx >= 0)&&(muscleInRange(s()[colIdx+1], '0', '9'))) ? atoi(s()+colIdx+1) : defaultPort;
      return;
   }
   else if (s.GetNumInstancesOf(':') != 1)  // I assume IPv6-style address strings never have exactly one colon in them
   {
      _ip   = ResolveIP(s, allowDNSLookups);
      _port = defaultPort;
      return;
   }
#endif

   // Old style IPv4 parsing (e.g. "192.168.0.1" or "192.168.0.1:2960")
   int colIdx = s.IndexOf(':');
   if ((colIdx >= 0)&&(muscleInRange(s()[colIdx+1], '0', '9')))
   {
      _ip   = ResolveIP(s.Substring(0, colIdx), allowDNSLookups);
      _port = atoi(s()+colIdx+1);
   }
   else
   {
      _ip   = ResolveIP(s, allowDNSLookups);
      _port = defaultPort;
   }
}

String IPAddressAndPort :: ToString(bool includePort, bool preferIPv4Style) const
{
   String s = Inet_NtoA(_ip, preferIPv4Style);

   if ((includePort)&&(_port > 0))
   {
      char buf[128];
#ifdef MUSCLE_AVOID_IPV6
      bool useIPv4Style = true;
#else
      bool useIPv4Style = ((preferIPv4Style)&&(IsIPv4Address(_ip)));  // FogBugz #8985
#endif
      if (useIPv4Style) sprintf(buf, "%s:%u", s(), _port);
                   else sprintf(buf, "[%s]:%u", s(), _port);
      return buf;
   }
   else return s;
}

// defined here to avoid having to pull in MiscUtilityFunctions.cpp for everything
String GetConnectString(const String & host, uint16 port)
{
#ifdef MUSCLE_AVOID_IPV6
   char buf[32]; sprintf(buf, ":%u", port);
   return host + buf;
#else
   char buf[32]; sprintf(buf, "]:%u", port);
   return host.Prepend("[") + buf;
#endif
}

static ip_address _customLocalhostIP = invalidIP;  // disabled by default
void SetLocalHostIPOverride(const ip_address & ip) {_customLocalhostIP = ip;}
ip_address GetLocalHostIPOverride() {return _customLocalhostIP;}

#ifdef MUSCLE_ENABLE_KEEPALIVE_API

static inline int KeepAliveMicrosToSeconds(uint64 micros) {return ((micros+(MICROS_PER_SECOND-1))/MICROS_PER_SECOND);}  // round up to the nearest second!
static inline uint64 KeepAliveSecondsToMicros(int second) {return (second*MICROS_PER_SECOND);}

status_t SetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 maxProbeCount, uint64 idleTime, uint64 retransmitTime)
{
#ifdef __linux__
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   int arg = KeepAliveMicrosToSeconds(idleTime);
   if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &arg, sizeof(arg)) != 0) return B_ERROR;

   arg = (int) maxProbeCount;
   if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &arg, sizeof(arg)) != 0) return B_ERROR;

   arg = KeepAliveMicrosToSeconds(retransmitTime);
   if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &arg, sizeof(arg)) != 0) return B_ERROR;

   arg = (maxProbeCount>0);  // true iff we want keepalive enabled
   if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const sockopt_arg *) &arg, sizeof(arg)) != 0) return B_ERROR;

   return B_NO_ERROR;
#else
   // Other OS's don't support these calls, AFAIK
   (void) sock;
   (void) maxProbeCount;
   (void) idleTime;
   (void) retransmitTime;
   return B_ERROR;
#endif
}

status_t GetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 * retMaxProbeCount, uint64 * retIdleTime, uint64 * retRetransmitTime)
{
#ifdef __linux__
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   int val;
   muscle_socklen_t valLen;
   if (retMaxProbeCount)
   {
      *retMaxProbeCount = 0;
      valLen = sizeof(val); if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_arg *) &val, &valLen) != 0) return B_ERROR;
      if (val != 0)  // we only set *retMaxProbeCount if SO_KEEPALIVE is enabled, otherwise we return 0 to indicate no-keepalive
      {
         valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPCNT, (sockopt_arg *) &val, &valLen) != 0) return B_ERROR;
         *retMaxProbeCount = val;
      } 
   }

   if (retIdleTime)
   {
      valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (sockopt_arg *) &val, &valLen) != 0) return B_ERROR;
      *retIdleTime = KeepAliveSecondsToMicros(val);
   }

   if (retRetransmitTime)
   {
      valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (sockopt_arg *) &val, &valLen) != 0) return B_ERROR;
      *retRetransmitTime = KeepAliveSecondsToMicros(val);
   }

   return B_NO_ERROR;
#else
   // Other OS's don't support these calls, AFAIK
   (void) sock;
   (void) retMaxProbeCount;
   (void) retIdleTime;
   (void) retRetransmitTime;
   return B_ERROR;
#endif
}

#endif

#ifndef MUSCLE_AVOID_MULTICAST_API

status_t SetSocketMulticastToSelf(const ConstSocketRef & sock, bool multicastToSelf)
{
   uint8 toSelf = (uint8) multicastToSelf;
   int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IP,   IP_MULTICAST_LOOP,   (const sockopt_arg *) &toSelf, sizeof(toSelf)) == 0)) ? B_NO_ERROR : B_ERROR;
#else
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const sockopt_arg *) &toSelf, sizeof(toSelf)) == 0)) ? B_NO_ERROR : B_ERROR;
#endif
}

bool GetSocketMulticastToSelf(const ConstSocketRef & sock)
{
   uint8 toSelf;
   muscle_socklen_t size = sizeof(toSelf);
   int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IP,   IP_MULTICAST_LOOP,   (sockopt_arg *) &toSelf, &size) == 0)&&(size == sizeof(toSelf))&&(toSelf));
#else
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (sockopt_arg *) &toSelf, &size) == 0)&&(size == sizeof(toSelf))&&(toSelf));
#endif
}

status_t SetSocketMulticastTimeToLive(const ConstSocketRef & sock, uint8 ttl)
{
   int fd = sock.GetFileDescriptor();
   int ttl_arg = (int) ttl;  // MacOS/X won't take a uint8
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IP,   IP_MULTICAST_TTL,    (const sockopt_arg *) &ttl_arg, sizeof(ttl_arg)) == 0)) ? B_NO_ERROR : B_ERROR;
#else
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const sockopt_arg *) &ttl_arg, sizeof(ttl_arg)) == 0)) ? B_NO_ERROR : B_ERROR;
#endif
}

uint8 GetSocketMulticastTimeToLive(const ConstSocketRef & sock)
{
   int ttl = 0;
   muscle_socklen_t size = sizeof(ttl);
   int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IP,   IP_MULTICAST_TTL,    (sockopt_arg *) &ttl, &size) == 0)&&(size == sizeof(ttl))) ? ttl : 0;
#else
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (sockopt_arg *) &ttl, &size) == 0)&&(size == sizeof(ttl))) ? ttl : 0;
#endif
}

#ifdef MUSCLE_AVOID_IPV6

// IPv4 multicast implementation

status_t SetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock, const ip_address & address)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   struct in_addr localInterface; memset(&localInterface, 0, sizeof(localInterface));
   localInterface.s_addr = htonl(address);
   return (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (const sockopt_arg *) &localInterface, sizeof(localInterface)) == 0) ? B_NO_ERROR : B_ERROR;
}

ip_address GetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return invalidIP;

   struct in_addr localInterface; memset(&localInterface, 0, sizeof(localInterface));
   muscle_socklen_t len = sizeof(localInterface);
   return ((getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (sockopt_arg *) &localInterface, &len) == 0)&&(len == sizeof(localInterface))) ? ntohl(localInterface.s_addr) : invalidIP;
}

status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress, const ip_address & localInterfaceAddress)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   struct ip_mreq req; memset(&req, 0, sizeof(req));
   req.imr_multiaddr.s_addr = htonl(groupAddress);
   req.imr_interface.s_addr = htonl(localInterfaceAddress);
   return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERROR;
}

status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress, const ip_address & localInterfaceAddress)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   struct ip_mreq req; memset(&req, 0, sizeof(req));
   req.imr_multiaddr.s_addr = htonl(groupAddress);
   req.imr_interface.s_addr = htonl(localInterfaceAddress);
   return (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERROR;
}

#else  // end IPv4 multicast, begin IPv6 multicast

// Apparently not everyone has settled on the same IPv6 multicast constant names.... :^P
#ifndef IPV6_ADD_MEMBERSHIP
# define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
#ifndef IPV6_DROP_MEMBERSHIP
# define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

status_t SetSocketMulticastSendInterfaceIndex(const ConstSocketRef & sock, uint32 interfaceIndex)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   int idx = interfaceIndex;
   return (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const sockopt_arg *) &idx, sizeof(idx)) == 0) ? B_NO_ERROR : B_ERROR;
}

int32 GetSocketMulticastSendInterfaceIndex(const ConstSocketRef & sock)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return -1;

   int idx = 0;
   socklen_t len = sizeof(idx);
   return ((getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (sockopt_arg *) &idx, &len) == 0)&&(len == sizeof(idx))) ? idx : -1;
}

status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   struct ipv6_mreq req; memset(&req, 0, sizeof(req));
   uint32 interfaceIdx;
   groupAddress.WriteToNetworkArray((uint8*)(&req.ipv6mr_multiaddr), &interfaceIdx);
   req.ipv6mr_interface = interfaceIdx;
   return (setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERROR;
}

status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress)
{
   int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_ERROR;

   struct ipv6_mreq req; memset(&req, 0, sizeof(req));
   uint32 interfaceIdx;
   groupAddress.WriteToNetworkArray((uint8*)(&req.ipv6mr_multiaddr), &interfaceIdx);
   req.ipv6mr_interface = interfaceIdx;
   return (setsockopt(fd, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERROR;
}

#endif  // IPv6 multicast

#endif  // !MUSCLE_AVOID_MULTICAST_API

}; // end namespace muscle
