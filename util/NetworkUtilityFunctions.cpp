/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "util/MiscUtilityFunctions.h"  // for GetConnectString() (which is deliberately defined here)
#include "util/NetworkUtilityFunctions.h"
#include "util/SocketMultiplexer.h"
#include "util/Hashtable.h"
#include "util/ICallbackSubscriber.h"
#include "util/SocketCallbackMechanism.h"
#include "util/TimeUnitConversionFunctions.h"

#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>
# include <TargetConditionals.h>
# if !(TARGET_OS_IPHONE)
#  include <SystemConfiguration/SystemConfiguration.h>
# endif
#endif

#ifdef WIN32
typedef char sockopt_arg;  // Windows setsockopt()/getsockopt() use char pointers
# include <iphlpapi.h>
# include <mswsock.h>  // for SIO_UDP_CONNRESET, etc
# include <ws2tcpip.h>
# if !(defined(__MINGW32__) || defined(__MINGW64__)) && !defined(MUSCLE_AVOID_MULTICAST_API)
#  include <ws2ipdef.h>  // for IP_MULTICAST_LOOP, etc
# endif
# pragma warning(disable: 4800 4018)
#else
# include <arpa/inet.h>
# include <fcntl.h>
# include <netdb.h>
# include <net/if.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <unistd.h>
typedef void sockopt_arg;  // Whereas sane operating systems use void pointers
#endif

#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__) || defined(__linux__) || defined(__CYGWIN__)
# if defined(ANDROID) // JFM
#  define USE_SOCKETPAIR 1
#  if __ANDROID_API__ >= 24   // Android only supports getifaddrs() in API level 24 or higher
#   define USE_GETIFADDRS 1
#   include <ifaddrs.h>
#  else
#   warning GetNetworkInterfaceInfos() requies Android SDK version 24 or higher in order to function correctly.
#  endif
# else
#  define USE_SOCKETPAIR 1
#  define USE_GETIFADDRS 1
#  include <ifaddrs.h>
#  if defined(__linux__)
#   include <linux/if_packet.h>  // for sockaddr_ll
#  else
#   if defined(__FreeBSD__)
#    include <net/if_types.h>  // for IFT_* defines
#    if !defined(IFT_CELLULAR)  // removed in later FreeBSD versions
#     define IFT_CELLULAR 0xff
#    endif
#   endif
#   include <net/if_dl.h>  // for the LLADDR macro
#  endif
# endif
#endif

#if defined(__APPLE__)
# include <net/if_types.h>
# include <sys/sockio.h>
#endif

#if defined(__linux__) && !defined(MUSCLE_AVOID_LINUX_DETECT_NETWORK_HARDWARE_TYPES)
# include <net/if_arp.h>
#endif

#include "system/GlobalMemoryAllocator.h"  // for muscleAlloc()/muscleFree()

namespace muscle {

#ifndef MUSCLE_AVOID_IPV6
static bool _automaticIPv4AddressMappingEnabled = true;   // if true, we automatically translate IPv4-compatible addresses (e.g. ::192.168.0.1) to IPv4-mapped-IPv6 addresses (e.g. ::ffff:192.168.0.1) and back
void SetAutomaticIPv4AddressMappingEnabled(bool e) {_automaticIPv4AddressMappingEnabled = e;}
bool GetAutomaticIPv4AddressMappingEnabled()       {return _automaticIPv4AddressMappingEnabled;}

IPAddress :: IPAddress(const struct in6_addr & in6Addr, uint32 optInterfaceIndex)
{
   ReadFromNetworkArray(in6Addr.s6_addr, &optInterfaceIndex);
   if (IsIPv6LinkLocal() == false) UnsetInterfaceIndex();  // no point keeping an interface-index for an IP address that doesn't support that anyway
   if ((_automaticIPv4AddressMappingEnabled)&&(*this != localhostIP)&&(IsValid())&&(IsIPv4())) SetLowBits(GetLowBits() & ((uint64)0xFFFFFFFF));  // remove IPv4-mapped-IPv6-bits
}

void IPAddress :: WriteToIn6Addr(struct in6_addr & writeToInAddr6, uint32 * optWriteInterfaceIndex) const
{
   if ((_automaticIPv4AddressMappingEnabled)&&(*this != localhostIP)&&(IsValid())&&(IsIPv4()))
   {
      IPAddress tmpAddr = *this;
      tmpAddr.SetLowBits(tmpAddr.GetLowBits() | (((uint64)0xFFFF)<<32));  // add IPv4-mapped-IPv6-bits
      tmpAddr.WriteToNetworkArray(writeToInAddr6.s6_addr, optWriteInterfaceIndex);
   }
   else WriteToNetworkArray(writeToInAddr6.s6_addr, optWriteInterfaceIndex);
}

static void GET_SOCKADDR_IP_IPV6(const struct sockaddr_in6 & sockAddr, IPAddress & ipAddr)
{
   switch(sockAddr.sin6_family)
   {
      case AF_INET6: ipAddr = IPAddress(sockAddr.sin6_addr, sockAddr.sin6_scope_id);      break;
      case AF_INET:  ipAddr = IPAddress(((const struct sockaddr_in &)sockAddr).sin_addr); break;  // shouldn't ever happen, but just in case
      default:       /* empty */                                                          break;
   }
}

static void SET_SOCKADDR_IP_IPV6(struct sockaddr_in6 & sockAddr, const IPAddress & ipAddr)
{
   uint32 temp = 0;  // just because MacOS/X likes to use a slightly different type
   ipAddr.WriteToIn6Addr(sockAddr.sin6_addr, &temp);
   sockAddr.sin6_scope_id = temp;
}

static uint16 GET_SOCKADDR_PORT_IPV6(const struct sockaddr_in6 & addr)
{
   switch(addr.sin6_family)
   {
      case AF_INET6: return ntohs(addr.sin6_port);
      case AF_INET:  return ntohs(((const struct sockaddr_in &)addr).sin_port);
      default:       return 0;
   }
}

static void   SET_SOCKADDR_FAMILY_IPV6(      struct sockaddr_in6 & addr, uint16 family) {addr.sin6_family = family;}
static void   SET_SOCKADDR_PORT_IPV6(        struct sockaddr_in6 & addr, uint16 port)   {addr.sin6_port   = htons(port);}
static uint16 GET_SOCKADDR_FAMILY_IPV6(const struct sockaddr_in6 & addr)                {return addr.sin6_family;}
static void InitializeSockAddr6(struct sockaddr_in6 & addr, const IPAddress * optFrom, uint16 port)
{
   memset(&addr, 0, sizeof(struct sockaddr_in6));
#ifdef SIN6_LEN
   addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
   SET_SOCKADDR_FAMILY_IPV6(addr, AF_INET6);
   if (optFrom) SET_SOCKADDR_IP_IPV6(  addr, *optFrom);
   if (port)    SET_SOCKADDR_PORT_IPV6(addr, port);
}
# define DECLARE_SOCKADDR_IPV6(addr, ip, port) struct sockaddr_in6 addr; InitializeSockAddr6(addr, ip, port);

IPAddressAndPort :: IPAddressAndPort(const struct sockaddr_in6 & sockAddr6)
{
   GET_SOCKADDR_IP_IPV6(sockAddr6, _ip);
   _port = GET_SOCKADDR_PORT_IPV6(sockAddr6);
}

void IPAddressAndPort :: WriteToSockAddrIn6(struct sockaddr_in6 & writeToMe) const
{
   InitializeSockAddr6(writeToMe, &_ip, _port);
}

#endif   // !MUSCLE_AVOID_IPV6

static void GET_SOCKADDR_IP_IPV4(const struct sockaddr_in & sockAddr, IPAddress & ipAddr) {ipAddr.SetIPv4AddressFromUint32(ntohl(sockAddr.sin_addr.s_addr));}
static void SET_SOCKADDR_IP_IPV4(struct sockaddr_in & sockAddr, const IPAddress & ipAddr) {sockAddr.sin_addr.s_addr = htonl(ipAddr.GetIPv4AddressAsUint32());}
static uint16 GET_SOCKADDR_PORT_IPV4(const struct sockaddr_in & addr) {return ntohs(addr.sin_port);}
static void SET_SOCKADDR_PORT_IPV4(struct sockaddr_in & addr, uint16 port) {addr.sin_port = htons(port);}
static uint16 GET_SOCKADDR_FAMILY_IPV4(const struct sockaddr_in & addr) {return addr.sin_family;}
static void SET_SOCKADDR_FAMILY_IPV4(struct sockaddr_in & addr, uint16 family) {addr.sin_family = family;}
static void InitializeSockAddr4(struct sockaddr_in & addr, const IPAddress * optFrom, uint16 port)
{
   memset(&addr, 0, sizeof(struct sockaddr_in));
   SET_SOCKADDR_FAMILY_IPV4(addr, AF_INET);
   if (optFrom) SET_SOCKADDR_IP_IPV4(addr, *optFrom);
   if (port) SET_SOCKADDR_PORT_IPV4(addr, port);
}
#define DECLARE_SOCKADDR_IPV4(addr, ip, port) struct sockaddr_in addr; InitializeSockAddr4(addr, ip, port);

IPAddress :: IPAddress(const struct in_addr & inAddr)
{
   SetIPv4AddressFromUint32(ntohl(inAddr.s_addr));
}

void IPAddress :: WriteToInAddr(struct in_addr & writeToMe) const
{
   writeToMe.s_addr = htonl(GetIPv4AddressAsUint32());
}

IPAddressAndPort :: IPAddressAndPort(const struct sockaddr_in & sockAddr)
{
   GET_SOCKADDR_IP_IPV4(sockAddr, _ip);
   _port = GET_SOCKADDR_PORT_IPV4(sockAddr);
}

void IPAddressAndPort :: WriteToSockAddrIn(struct sockaddr_in & writeToMe) const
{
   InitializeSockAddr4(writeToMe, &_ip, _port);
}

static GlobalSocketCallback * _globalSocketCallback = NULL;
void SetGlobalSocketCallback(GlobalSocketCallback * cb) {_globalSocketCallback = cb;}
GlobalSocketCallback * GetGlobalSocketCallback() {return _globalSocketCallback;}

static status_t DoGlobalSocketCallback(uint32 eventType, const ConstSocketRef & s)
{
   if (s() == NULL) return B_BAD_ARGUMENT;
   if (_globalSocketCallback == NULL) return B_NO_ERROR;
   return _globalSocketCallback->SocketCallback(eventType, s);
}

static ConstSocketRef CreateMuscleSocket(int socketType, uint32 createType, int socketFamily)
{
#ifdef MUSCLE_AVOID_IPV6
   if (socketFamily != SOCKET_FAMILY_IPV4) return ConstSocketRef();
   const int fam = AF_INET;
#else
   int fam;
   switch(socketFamily)
   {
      case SOCKET_FAMILY_IPV4: fam = AF_INET;  break;
      case SOCKET_FAMILY_IPV6: fam = AF_INET6; break;
      default:                 return ConstSocketRef();
   }
#endif

   const int s = static_cast<int>(socket(fam, socketType, 0));
   if (s >= 0)
   {
#if defined(__APPLE__) || defined(BSD)
      // This is here just so that MUSCLE programs can be run in a debugger without having the debugger catch spurious SIGPIPE signals --jaf
      const int value = 1; // Set NOSIGPIPE to ON
      if (setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const sockopt_arg *) &value, sizeof(value)) != 0) LogTime(MUSCLE_LOG_DEBUG, "Could not disable SIGPIPE signals on socket %i [%s]\n", s, B_ERRNO());
#endif

      ConstSocketRef ret = GetConstSocketRefFromPool(s);
      if (ret())
      {
#ifndef MUSCLE_AVOID_IPV6
         // If we're doing IPv4-mapped IPv6 addresses, we gotta tell Windows 7 that it's okay to use them, otherwise they won't work.  :^P
         if ((socketFamily == SOCKET_FAMILY_IPV6)&&(_automaticIPv4AddressMappingEnabled))
         {
            int v6OnlyEnabled = 0;  // we want v6-only mode disabled, which is to say we want v6-to-v4 compatibility
            if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const sockopt_arg *) &v6OnlyEnabled, sizeof(v6OnlyEnabled)) != 0) LogTime(MUSCLE_LOG_DEBUG, "Could not disable v6-only mode for socket %i [%s]\n", s, B_ERRNO());
         }
#endif
         if (DoGlobalSocketCallback(createType, ret).IsOK()) return ret;
      }
   }
   return ConstSocketRef();
}

ConstSocketRef CreateUDPSocket(int socketFamily)
{
   ConstSocketRef ret = CreateMuscleSocket(SOCK_DGRAM, GlobalSocketCallback::SOCKET_CALLBACK_CREATE_UDP, socketFamily);
#if defined(WIN32) && !defined(__MINGW32__)
   if (ret())
   {
      // This setup code avoids the UDP WSAECONNRESET problem
      // described at http://support.microsoft.com/kb/263823/en-us
      DWORD dwBytesReturned = 0;
      BOOL bNewBehavior = FALSE;
      if (WSAIoctl(ret.GetFileDescriptor(), SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL) != 0) ret.SetStatus(B_IO_ERROR);
   }
#endif
   return ret;
}

status_t BindUDPSocket(const ConstSocketRef & sock, uint16 port, uint16 * optRetPort, const IPAddress & optFrom, bool allowShared)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   if (allowShared)
   {
      const int trueValue = 1;
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const sockopt_arg *) &trueValue, sizeof(trueValue)) == -1) return B_ERRNO;
#ifdef __APPLE__
      if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, (const sockopt_arg *) &trueValue, sizeof(trueValue)) == -1) return B_ERRNO;
#endif
   }

   switch(sock.GetSocketFamily())
   {
      case SOCKET_FAMILY_IPV4:
      {
         DECLARE_SOCKADDR_IPV4(saSocket, &optFrom, port);
         if (bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) == 0)
         {
            if (optRetPort)
            {
               muscle_socklen_t len = sizeof(saSocket);
               if (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0)
               {
                  *optRetPort = GET_SOCKADDR_PORT_IPV4(saSocket);
                  return B_NO_ERROR;
               }
               else return B_ERRNO;
            }
            return B_NO_ERROR;
         }
         else return B_ERRNO;
      }
      break;

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         DECLARE_SOCKADDR_IPV6(saSocket, &optFrom, port);
         if (bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) == 0)
         {
            if (optRetPort)
            {
               muscle_socklen_t len = sizeof(saSocket);
               if (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0)
               {
                  *optRetPort = GET_SOCKADDR_PORT_IPV6(saSocket);
                  return B_NO_ERROR;
               }
               else return B_ERRNO;
            }
            return B_NO_ERROR;
         }
         else return B_ERRNO;
      }
      break;
#endif

      default: return B_BAD_OBJECT;
   }
}

status_t SetUDPSocketTarget(const ConstSocketRef & sock, const IPAddress & remoteIP, uint16 remotePort)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   switch(sock.GetSocketFamily())
   {
      case SOCKET_FAMILY_IPV4:
      {
         DECLARE_SOCKADDR_IPV4(saAddr, &remoteIP, remotePort);
         return (connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr)) == 0) ? B_NO_ERROR : B_ERRNO;
      }

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         DECLARE_SOCKADDR_IPV6(saAddr, &remoteIP, remotePort);
         return (connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr)) == 0) ? B_NO_ERROR : B_ERRNO;
      }
#endif

      default:
         return B_UNIMPLEMENTED;
      break;
   }
}

status_t SetUDPSocketTarget(const ConstSocketRef & sock, const char * remoteHostName, uint16 remotePort, bool expandLocalhost)
{
   const IPAddress hostIP = GetHostByName(remoteHostName, expandLocalhost);
   return (hostIP != invalidIP) ? SetUDPSocketTarget(sock, hostIP, remotePort) : B_ERROR("GetHostByName() failed");
}

ConstSocketRef CreateAcceptingSocket(uint16 port, int maxbacklog, uint16 * optRetPort, const IPAddress & optInterfaceIP, int socketFamily)
{
   ConstSocketRef ret = CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CREATE_ACCEPTING, socketFamily);
   MRETURN_ON_ERROR(ret);

   const int fd = ret.GetFileDescriptor();
   if (fd < 0) return B_BAD_OBJECT;

#ifndef WIN32
   // (Not necessary under windows -- it has the behaviour we want by default)
   const int trueValue = 1;
   (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const sockopt_arg *) &trueValue, sizeof(trueValue));
#endif

   // coverity[returned_null : FALSE] - if ret() was going to return NULL, then fd would be negative above and execution wouldn't get here
   switch(ret()->GetSocketFamily())
   {
      case SOCKET_FAMILY_IPV4:
      {
         DECLARE_SOCKADDR_IPV4(saSocket, &optInterfaceIP, port);
         if (bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) != 0) return B_ERRNO;
         if (listen(fd, maxbacklog) != 0) return B_ERRNO;
         if (optRetPort)
         {
            muscle_socklen_t len = sizeof(saSocket);
            *optRetPort = (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0) ? GET_SOCKADDR_PORT_IPV4(saSocket) : 0;
         }
      }
      return ret;

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         DECLARE_SOCKADDR_IPV6(saSocket, &optInterfaceIP, port);
         if (bind(fd, (struct sockaddr *) &saSocket, sizeof(saSocket)) != 0) return B_ERRNO;
         if (listen(fd, maxbacklog) != 0) return B_ERRNO;
         if (optRetPort)
         {
            muscle_socklen_t len = sizeof(saSocket);
            *optRetPort = (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0) ? GET_SOCKADDR_PORT_IPV6(saSocket) : 0;
         }
      }
      return ret;
#endif

      default:
         return B_UNIMPLEMENTED;  // unknown family!?
   }
}

io_status_t ReceiveData(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int32 r = recv_ignore_eintr(fd, (char *)buffer, size, 0L);
   if (r == 0) return B_END_OF_STREAM;

   const int32 ret = ConvertReturnValueToMuscleSemantics(r, size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
}

io_status_t ReadData(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm)
{
#ifdef WIN32
   return ReceiveData(sock, buffer, size, bm);  // Windows doesn't support read(), only recv()
#else
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int32 r = read_ignore_eintr(fd, (char *)buffer, size);
   if (r == 0) return B_END_OF_STREAM;

   const int32 ret = ConvertReturnValueToMuscleSemantics(r, size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
#endif
}

io_status_t ReceiveDataUDP(const ConstSocketRef & sock, void * buffer, uint32 size, bool bm, IPAddress * optFromIP, uint16 * optFromPort)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int32 r = -1;
   if ((optFromIP)||(optFromPort))
   {
      switch(sock.GetSocketFamily())
      {
         case SOCKET_FAMILY_IPV4:
         {
            DECLARE_SOCKADDR_IPV4(fromAddr, NULL, 0);
            muscle_socklen_t fromAddrLen = sizeof(fromAddr);
            r = recvfrom_ignore_eintr(fd, (char *)buffer, size, 0L, (struct sockaddr *) &fromAddr, &fromAddrLen);
            if (r >= 0)
            {
               if (optFromIP) GET_SOCKADDR_IP_IPV4(fromAddr, *optFromIP);
               if (optFromPort) *optFromPort = GET_SOCKADDR_PORT_IPV4(fromAddr);
            }
         }
         break;

#ifndef MUSCLE_AVOID_IPV6
         case SOCKET_FAMILY_IPV6:
         {
            DECLARE_SOCKADDR_IPV6(fromAddr, NULL, 0);
            muscle_socklen_t fromAddrLen = sizeof(fromAddr);
            r = recvfrom_ignore_eintr(fd, (char *)buffer, size, 0L, (struct sockaddr *) &fromAddr, &fromAddrLen);
            if (r >= 0)
            {
               if (optFromIP) GET_SOCKADDR_IP_IPV6(fromAddr, *optFromIP);
               if (optFromPort) *optFromPort = GET_SOCKADDR_PORT_IPV6(fromAddr);
            }
         }
         break;
#endif

         default:
            return B_UNIMPLEMENTED;
      }
   }
   else r = recv_ignore_eintr(fd, (char *)buffer, size, 0L);

   if (r == 0) return io_status_t();  // for UDP, zero is a valid recv() size, since there is no EOS

   const int32 ret = ConvertReturnValueToMuscleSemantics(r, size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
}

io_status_t SendData(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int32 ret = ConvertReturnValueToMuscleSemantics(send_ignore_eintr(fd, (const char *)buffer, size, 0L), size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
}

io_status_t WriteData(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm)
{
#ifdef WIN32
   return SendData(sock, buffer, size, bm);  // Windows doesn't support write(), only send()
#else
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int32 ret = ConvertReturnValueToMuscleSemantics(write_ignore_eintr(fd, (const char *)buffer, size), size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
#endif
}

static io_status_t SendDataUDPIPv4(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm, const IPAddress & optToIP, uint16 optToPort)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int32 s;
   if ((optToPort)||(optToIP != invalidIP))
   {
      DECLARE_SOCKADDR_IPV4(toAddr, NULL, 0);
      if ((optToPort == 0)||(optToIP == invalidIP))
      {
         // Fill in the values with our socket's current target-values, as defaults
         muscle_socklen_t length = sizeof(sockaddr_in);
         if (getpeername(fd, (struct sockaddr *)&toAddr, &length) != 0) return B_ERRNO;
         if (GET_SOCKADDR_FAMILY_IPV4(toAddr) != AF_INET) return B_BAD_OBJECT;
      }

      if (optToIP != invalidIP) SET_SOCKADDR_IP_IPV4(toAddr, optToIP);
      if (optToPort) SET_SOCKADDR_PORT_IPV4(toAddr, optToPort);
      s = sendto_ignore_eintr(fd, (const char *)buffer, size, 0L, (struct sockaddr *)&toAddr, sizeof(toAddr));
   }
   else s = send_ignore_eintr(fd, (const char *)buffer, size, 0L);

   if (s == 0) return 0;  // for UDP, zero is a valid send() size, since there is no EOS

   const int32 ret = ConvertReturnValueToMuscleSemantics(s, size, bm);
   return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
}

#ifndef MUSCLE_AVOID_IPV6
static io_status_t SendDataUDPIPv6(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm, const IPAddress & optToIP, uint16 optToPort)
{
#ifdef DEBUG_SENDING_UDP_PACKETS_ON_INTERFACE_ZERO
   if ((optToIP != invalidIP)&&(optToIP.IsInterfaceIndexValid() == false)&&(optToIP.IsIPv4() == false)&&(optToIP.IsStandardLoopbackDeviceAddress() == false))
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "SendDataUDP:  Sending to IP address with invalid interface-index!  [%s]:%u\n", Inet_NtoA(optToIP)(), optToPort);
      PrintStackTrace();
   }
#endif

#if !defined(MUSCLE_AVOID_IPV6) && !defined(MUSCLE_AVOID_MULTICAST_API)
# define MUSCLE_USE_IFIDX_WORKAROUND 1
#endif

   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

#ifdef MUSCLE_USE_IFIDX_WORKAROUND
   int oldInterfaceIndex = -1;  // and remember to set it back afterwards
#endif

   int32 s;
   if ((optToPort)||(optToIP != invalidIP))
   {
      DECLARE_SOCKADDR_IPV6(toAddr, NULL, 0);
      if ((optToPort == 0)||(optToIP == invalidIP))
      {
         // Fill in the values with our socket's current target-values, as defaults
         muscle_socklen_t length = sizeof(sockaddr_in);
         if (getpeername(fd, (struct sockaddr *)&toAddr, &length) != 0) return B_ERRNO;
         if (GET_SOCKADDR_FAMILY_IPV6(toAddr) != AF_INET6) return B_BAD_OBJECT;
      }

      if (optToIP != invalidIP)
      {
         SET_SOCKADDR_IP_IPV6(toAddr, optToIP);
#ifdef MUSCLE_USE_IFIDX_WORKAROUND
         // Work-around for MacOS/X problem (?) where the interface index in the specified IP address doesn't get used
         if ((optToIP.IsInterfaceIndexValid())&&(optToIP.IsMulticast()))
         {
            const int         oidx = GetSocketMulticastSendInterfaceIndex(sock);
            const uint32 actualIdx = optToIP.GetInterfaceIndex();
            if (oidx != ((int)actualIdx))
            {
               // temporarily set the socket's interface index to the desired one
               MRETURN_ON_ERROR(SetSocketMulticastSendInterfaceIndex(sock, actualIdx));
               oldInterfaceIndex = oidx;  // and remember to set it back afterwards
            }
         }
#endif
      }
      if (optToPort) SET_SOCKADDR_PORT_IPV6(toAddr, optToPort);
      s = sendto_ignore_eintr(fd, (const char *)buffer, size, 0L, (struct sockaddr *)&toAddr, sizeof(toAddr));
   }
   else s = send_ignore_eintr(fd, (const char *)buffer, size, 0L);

   const int32 ret = ConvertReturnValueToMuscleSemantics(s, size, bm);

#ifdef MUSCLE_USE_IFIDX_WORKAROUND
   const int errnoFromSendCall = GetErrno();
   if (oldInterfaceIndex >= 0)
   {
      (void) SetSocketMulticastSendInterfaceIndex(sock, oldInterfaceIndex);  // gotta do this AFTER computing the return value, as it clears errno!
      SetErrno(errnoFromSendCall);  // restore the errno from the send_ignore_eintr() call, in case our calling code wants to examine it
   }
#endif

   if (s == 0) return 0;  // for UDP, zero is a valid send() size, since there is no EOS
          else return (ret >= 0) ? io_status_t(ret) : io_status_t(B_ERRNO);
}
#endif

io_status_t SendDataUDP(const ConstSocketRef & sock, const void * buffer, uint32 size, bool bm, const IPAddress & optToIP, uint16 optToPort)
{
   switch(sock.GetSocketFamily())
   {
      case SOCKET_FAMILY_INVALID: return B_BAD_ARGUMENT;
      case SOCKET_FAMILY_IPV4:    return SendDataUDPIPv4(sock, buffer, size, bm, optToIP, optToPort);
#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:    return SendDataUDPIPv6(sock, buffer, size, bm, optToIP, optToPort);
#endif
      default:                    return B_UNIMPLEMENTED;
   }
}


status_t ShutdownSocket(const ConstSocketRef & sock, bool dRecv, bool dSend)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   if ((dRecv == false)&&(dSend == false)) return B_NO_ERROR;  // there's nothing we need to do!

   // Since these constants aren't defined everywhere, I'll define my own:
   enum {
      MUSCLE_SHUT_RD = 0,
      MUSCLE_SHUT_WR,
      MUSCLE_SHUT_RDWR,
      NUM_MUSCLE_SHUTS
   };
   return (shutdown(fd, dRecv?(dSend?MUSCLE_SHUT_RDWR:MUSCLE_SHUT_RD):MUSCLE_SHUT_WR) == 0) ? B_NO_ERROR : B_ERRNO;
}

ConstSocketRef Accept(const ConstSocketRef & sock, IPAddress * optRetInterfaceIP)
{
   const int sfd = sock.GetFileDescriptor();
   if (sfd < 0) return B_BAD_ARGUMENT;

   // coverity[returned_null : FALSE] - if ret() was going to return NULL, then sfd would be negative above and execution wouldn't get here
   switch(sock()->GetSocketFamily())
   {
      case SOCKET_FAMILY_IPV4:
      {
         DECLARE_SOCKADDR_IPV4(saSocket, NULL, 0);
         muscle_socklen_t nLen = sizeof(saSocket);

         const int cfd = accept(sfd, (struct sockaddr *)&saSocket, &nLen);
         if (cfd < 0) return B_ERRNO;  // mostly just to keep Coverity happy

         ConstSocketRef ret = GetConstSocketRefFromPool(cfd);
         MRETURN_ON_ERROR(ret);
         MRETURN_ON_ERROR(DoGlobalSocketCallback(GlobalSocketCallback::SOCKET_CALLBACK_ACCEPT, ret));  // called separately since accept() created this socket, not CreateMuscleSocket()

         if (optRetInterfaceIP)
         {
            muscle_socklen_t len = sizeof(saSocket);
            if (getsockname(cfd, (struct sockaddr *)&saSocket, &len) == 0) GET_SOCKADDR_IP_IPV4(saSocket, *optRetInterfaceIP);
                                                                      else *optRetInterfaceIP = invalidIP;
         }
         return ret;
      }

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         DECLARE_SOCKADDR_IPV6(saSocket, NULL, 0);
         muscle_socklen_t nLen = sizeof(saSocket);

         const int cfd = accept(sfd, (struct sockaddr *)&saSocket, &nLen);
         if (cfd < 0) return B_ERRNO;  // mostly just to keep Coverity happy

         ConstSocketRef ret = GetConstSocketRefFromPool(cfd);
         MRETURN_ON_ERROR(ret);
         MRETURN_ON_ERROR(DoGlobalSocketCallback(GlobalSocketCallback::SOCKET_CALLBACK_ACCEPT, ret)); // called separately since accept() created this socket, not CreateMuscleSocket()

         if (optRetInterfaceIP)
         {
            muscle_socklen_t len = sizeof(saSocket);
            if (getsockname(cfd, (struct sockaddr *)&saSocket, &len) == 0) GET_SOCKADDR_IP_IPV6(saSocket, *optRetInterfaceIP);
                                                                      else *optRetInterfaceIP = invalidIP;
         }
         return ret;
      }
#endif

      default:
         return B_UNIMPLEMENTED; // unknown family?
   }
}

ConstSocketRef Connect(const char * hostName, uint16 port, const char * debugTitle, bool errorsOnly, uint64 maxConnectTime, bool expandLocalhost)
{
   const IPAddress hostIP = GetHostByName(hostName, expandLocalhost);
   if (hostIP != invalidIP) return Connect(IPAddressAndPort(hostIP, port), hostName, debugTitle, errorsOnly, maxConnectTime);
   else
   {
      if (debugTitle) LogTime(MUSCLE_LOG_INFO, "%s: hostname lookup for [%s] failed!\n", debugTitle, hostName);
      return B_DATA_NOT_FOUND;
   }
}

ConstSocketRef Connect(const IPAddressAndPort & hostIAP, const char * optDebugHostName, const char * debugTitle, bool errorsOnly, uint64 maxConnectTime)
{
   char ipbuf[64]; Inet_NtoA(hostIAP.GetIPAddress(), ipbuf);

   if ((debugTitle)&&(errorsOnly == false))
   {
      LogTime(MUSCLE_LOG_INFO, "%s: Connecting to %s: ", debugTitle, GetConnectString(optDebugHostName?optDebugHostName:ipbuf, hostIAP.GetPort())());
      (void) LogFlush();
   }

   bool socketIsReady = false;
   ConstSocketRef s = (maxConnectTime == MUSCLE_TIME_NEVER) ? CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CONNECT, SOCKET_FAMILY_PREFERRED) : ConnectAsync(hostIAP, socketIsReady);
   if (s())
   {
      const int fd = s.GetFileDescriptor();
      if (fd < 0) return B_LOGIC_ERROR;  // just to keep Coverity happy -- should never actually happen

      status_t ret;
      if (maxConnectTime == MUSCLE_TIME_NEVER)
      {
         switch(s()->GetSocketFamily())
         {
            case SOCKET_FAMILY_IPV4:
            {
               DECLARE_SOCKADDR_IPV4(saAddr, &hostIAP.GetIPAddress(), hostIAP.GetPort());
               const int r = connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr));
               if (r < 0) ret = B_ERRNO;
            }
            break;

#ifndef MUSCLE_AVOID_IPV6
            case SOCKET_FAMILY_IPV6:
            {
               DECLARE_SOCKADDR_IPV6(saAddr, &hostIAP.GetIPAddress(), hostIAP.GetPort());
               const int r = connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr));
               if (r < 0) ret = B_ERRNO;
            }
            break;
#endif
            default:
               ret = B_UNIMPLEMENTED;
         }
      }
      else
      {
         if (socketIsReady == false)
         {
            // The harder case:  the user doesn't want the Connect() call to take more than (so many) microseconds.
            // For this, we'll need to go into non-blocking mode and run a SocketMultiplexer loop to get the desired behaviour!
            ret = B_TIMED_OUT;
            const uint64 deadline = GetRunTime64()+maxConnectTime;
            SocketMultiplexer multiplexer;
            while(GetRunTime64() < deadline)
            {
               (void) multiplexer.RegisterSocketForWriteReady(fd);
#ifdef WIN32
               (void) multiplexer.RegisterSocketForExceptionRaised(fd);
#endif

               const io_status_t eret = multiplexer.WaitForEvents(deadline);
               if (eret.IsError())
               {
                  ret = eret.GetStatus();
                  break; // error out!
               }
               else
               {
#ifdef WIN32
                  if (multiplexer.IsSocketExceptionRaised(fd)) break; // Win32:  failed async connect detected!
#endif
                  if (multiplexer.IsSocketReadyForWrite(fd))
                  {
                     ret = B_NO_ERROR;
                     if (FinalizeAsyncConnect(s).IsOK(ret)) ret = SetSocketBlockingEnabled(s, true);
                     break;
                  }
               }
            }
         }
      }

      if (ret.IsOK())
      {
         if ((debugTitle)&&(errorsOnly == false)) LogPlain(MUSCLE_LOG_INFO, "Connected!\n");
         return s;
      }
      else if (debugTitle)
      {
         if (errorsOnly) LogTime(MUSCLE_LOG_INFO, "%s: connect() to %s failed! [%s]\n", debugTitle, GetConnectString(optDebugHostName?optDebugHostName:ipbuf, hostIAP.GetPort())(), ret());
                    else LogPlain(MUSCLE_LOG_INFO, "Connection failed! [%s]\n", ret());
      }
      return ret;
   }
   else if (debugTitle)
   {
      if (errorsOnly) LogTime(MUSCLE_LOG_INFO, "%s: socket() failed!\n", debugTitle);
                 else LogPlain(MUSCLE_LOG_INFO, "socket() failed!\n");
   }
   return s.GetStatus();
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
   return ((Inet_PtoN(AF_INET6, s, &tmp) > 0)||(Inet_AtoN(s) != invalidIP)||(IsIP4Address(s)));  // Inet_AtoN() handles the "@blah" suffixes
#endif
}

static Mutex _cachedLocalhostAddressLock;
static IPAddress _cachedLocalhostAddress = invalidIP;

static void ExpandLocalhostAddress(IPAddress & ipAddress)
{
   if ((ipAddress.IsStandardLoopbackDeviceAddress())&&(ipAddress.IsIPv4()))
   {
      IPAddress altRet = GetLocalHostIPOverride();  // see if the user manually specified a preferred local address
      if (altRet == invalidIP)
      {
         DECLARE_MUTEXGUARD(_cachedLocalhostAddressLock);
         // If not, try to grab one from the OS
         if (_cachedLocalhostAddress == invalidIP)
         {
            Queue<NetworkInterfaceInfo> ifs;
            (void) GetNetworkInterfaceInfos(ifs, GNIIFlags(GNII_FLAG_INCLUDE_ENABLED_INTERFACES,GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES,GNII_FLAG_INCLUDE_MUSCLE_PREFERRED_INTERFACES));  // just to set _cachedLocalhostAddress
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
   DNSRecord()
      : _expirationTime(0)
   {
      // empty
   }
   DNSRecord(const IPAddress & ip, uint64 expTime) : _ipAddress(ip), _expirationTime(expTime) {/* empty */}

   const IPAddress & GetIPAddress() const {return _ipAddress;}
   uint64 GetExpirationTime() const {return _expirationTime;}

private:
   IPAddress _ipAddress;
   uint64 _expirationTime;
};

static Mutex _hostCacheMutex;                // serialize access to the cache data
static uint32 _maxHostCacheSize = 0;         // DNS caching is disabled by default
static uint64 _hostCacheEntryLifespan = 0;   // how many microseconds before a cache entry is expired
static Hashtable<String, DNSRecord> _hostCache;

void SetHostNameCacheSettings(uint32 maxEntries, uint64 expirationTime)
{
   DECLARE_MUTEXGUARD(_hostCacheMutex);
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

IPAddress GetHostByNameNative(const char * name, bool expandLocalhost, bool preferIPv6)
{
   if (IsIPAddress(name))
   {
      // No point in ever caching this result, since Inet_AtoN() is always fast anyway
      IPAddress ret = Inet_AtoN(name);
      if (expandLocalhost) ExpandLocalhostAddress(ret);
      return ret;
   }
   else if (_maxHostCacheSize > 0)
   {
      const String s = GetHostByNameKey(name, expandLocalhost, preferIPv6);
      DECLARE_MUTEXGUARD(_hostCacheMutex);
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

   IPAddress ret = invalidIP;
#ifdef MUSCLE_AVOID_IPV6
   struct hostent * he = gethostbyname(name);
   if (he) ret.SetIPv4AddressFromUint32(ntohl(*((uint32*)he->h_addr)));
#else
   struct addrinfo * result;
   struct addrinfo hints; memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_UNSPEC;     // We're not too particular, for now
   hints.ai_socktype = SOCK_STREAM;   // so we don't get every address twice (once for UDP and once for TCP)
   IPAddress ret6 = invalidIP;
   if (getaddrinfo(name, NULL, &hints, &result) == 0)
   {
      struct addrinfo * next = result;
      while(next)
      {
         switch(next->ai_family)
         {
            case AF_INET:
               if (ret.IsValid() == false)
               {
                  ret.SetIPv4AddressFromUint32(ntohl(((struct sockaddr_in *) next->ai_addr)->sin_addr.s_addr)); // read IPv4 address into low bits of IPv6 address structure
                  ret.SetLowBits(ret.GetLowBits() | ((uint64)0xFFFF)<<32);                                      // and make it IPv6-mapped (why doesn't AI_V4MAPPED do this?)
               }
            break;

            case AF_INET6:
               if (ret6.IsValid() == false)
               {
                  struct sockaddr_in6 * sin6 = (struct sockaddr_in6 *) next->ai_addr;
                  const uint32 tmp = sin6->sin6_scope_id;  // MacOS/X uses __uint32_t, which isn't quite the same somehow
                  ret6.ReadFromNetworkArray(sin6->sin6_addr.s6_addr, tmp ? &tmp : NULL);
               }
            break;

            default:
               // empty
            break;
         }
         next = next->ai_next;
      }
      freeaddrinfo(result);

      if (ret.IsValid())
      {
         if ((preferIPv6)&&(ret6.IsValid())) ret = ret6;
      }
      else ret = ret6;
   }
#endif

   if (expandLocalhost) ExpandLocalhostAddress(ret);

   if (_maxHostCacheSize > 0)
   {
      // Store our result in the cache for later
      const String s = GetHostByNameKey(name, expandLocalhost, preferIPv6);
      DECLARE_MUTEXGUARD(_hostCacheMutex);
      DNSRecord * r = _hostCache.PutAndGet(s, DNSRecord(ret, (_hostCacheEntryLifespan==MUSCLE_TIME_NEVER)?MUSCLE_TIME_NEVER:(GetRunTime64()+_hostCacheEntryLifespan)));
      if (r)
      {
         (void) _hostCache.MoveToFront(s);  // LRU logic
         while(_hostCache.GetNumItems() > _maxHostCacheSize) (void) _hostCache.RemoveLast();
      }
   }

   return ret;
}

static OrderedValuesHashtable<IHostNameResolverRef, int> _hostNameResolvers;
status_t PutHostNameResolver(const IHostNameResolverRef & resolver, int priority) {return _hostNameResolvers.Put(resolver, priority);}
status_t RemoveHostNameResolver(const IHostNameResolverRef & resolver) {return _hostNameResolvers.Remove(resolver);}
void ClearHostNameResolvers() {_hostNameResolvers.Clear();}

IPAddress GetHostByName(const char * name, bool expandLocalhost, bool preferIPv6)
{
   IPAddress ret;
   if (_hostNameResolvers.HasItems())
   {
      for (HashtableIterator<IHostNameResolverRef, int> iter(_hostNameResolvers, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
      {
         if (iter.GetValue() < 0) break;  // we'll do any negative-priority callbacks only after our built-in functionality has failed
         if (iter.GetKey()()->GetIPAddressForHostName(name, expandLocalhost, preferIPv6, ret).IsOK()) break;  // returning here bothers clang-tidy
      }
   }
   if (ret.IsValid()) return ret;

   ret = GetHostByNameNative(name, expandLocalhost, preferIPv6);
   if (ret.IsValid()) return ret;

   if (_hostNameResolvers.HasItems())
   {
      for (HashtableIterator<IHostNameResolverRef, int> iter(_hostNameResolvers, HTIT_FLAG_BACKWARDS); iter.HasData(); iter++)
      {
         if ((iter.GetValue() < 0)&&(iter.GetKey()()->GetIPAddressForHostName(name, expandLocalhost, preferIPv6, ret).IsOK())) break;  // returning here bothers clang-tidy
      }
   }
   return ret;
}

ConstSocketRef ConnectAsync(const IPAddressAndPort & hostIAP, bool & retIsReady)
{
   ConstSocketRef s = CreateMuscleSocket(SOCK_STREAM, GlobalSocketCallback::SOCKET_CALLBACK_CONNECT, SOCKET_FAMILY_PREFERRED);
   MRETURN_ON_ERROR(s);

   MRETURN_ON_ERROR(SetSocketBlockingEnabled(s, false));

   const int fd = s.GetFileDescriptor();
   if (fd < 0) return ConstSocketRef();  // just to keep Coverity happy

   int result = -1;
   switch(s() ? s()->GetSocketFamily() : NUM_SOCKET_FAMILIES)  // s() will never return NULL, but Coverity needs reassurance
   {
      case SOCKET_FAMILY_IPV4:
      {
         DECLARE_SOCKADDR_IPV4(saAddr, &hostIAP.GetIPAddress(), hostIAP.GetPort());
         result = connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr));
      }
      break;

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         DECLARE_SOCKADDR_IPV6(saAddr, &hostIAP.GetIPAddress(), hostIAP.GetPort());
         result = connect(fd, (struct sockaddr *) &saAddr, sizeof(saAddr));
      }
      break;
#endif

      default:
         // empty
      break;
   }

#ifdef WIN32
   const bool inProgress = ((result < 0)&&(WSAGetLastError() == WSAEWOULDBLOCK));
#else
   const bool inProgress = ((result < 0)&&(errno == EINPROGRESS));
#endif
   if ((result == 0)||(inProgress))
   {
      retIsReady = (inProgress == false);
      return s;
   }
   else return B_ERRNO;
}

IPAddressAndPort GetSocketBindAddress(const ConstSocketRef & sock)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return IPAddressAndPort();

   switch(sock.GetSocketFamily())
   {
      case SOCKET_FAMILY_IPV4:
      {
         struct sockaddr_in saSocket;
         muscle_socklen_t len = sizeof(saSocket);
         if (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0) return IPAddressAndPort(saSocket);
      }
      break;

#ifndef MUSCLE_AVOID_IPV6
      case SOCKET_FAMILY_IPV6:
      {
         struct sockaddr_in6 saSocket;
         muscle_socklen_t len = sizeof(saSocket);
         if (getsockname(fd, (struct sockaddr *)&saSocket, &len) == 0) return IPAddressAndPort(saSocket);
      }
      break;
#endif

      default:
         // empty
      break;
   }

   return IPAddressAndPort(); // failure
}

IPAddressAndPort GetPeerAddress(const ConstSocketRef & sock, bool expandLocalhost)
{
   IPAddress ipAddress;
   uint16 port = 0;

   const int fd = sock.GetFileDescriptor();
   if (fd >= 0)
   {
      switch(sock.GetSocketFamily())
      {
         case SOCKET_FAMILY_IPV4:
         {
            DECLARE_SOCKADDR_IPV4(saTempAdd, NULL, 0);
            muscle_socklen_t length = sizeof(saTempAdd);
            if ((getpeername(fd, (struct sockaddr *)&saTempAdd, &length) == 0)&&(GET_SOCKADDR_FAMILY_IPV4(saTempAdd) == AF_INET))
            {
               GET_SOCKADDR_IP_IPV4(saTempAdd, ipAddress);
               port = GET_SOCKADDR_PORT_IPV4(saTempAdd);
               if (expandLocalhost) ExpandLocalhostAddress(ipAddress);
            }
         }
         break;

#ifndef MUSCLE_AVOID_IPV6
         case SOCKET_FAMILY_IPV6:
         {
            DECLARE_SOCKADDR_IPV6(saTempAdd, NULL, 0);
            muscle_socklen_t length = sizeof(saTempAdd);
            if ((getpeername(fd, (struct sockaddr *)&saTempAdd, &length) == 0)&&(GET_SOCKADDR_FAMILY_IPV6(saTempAdd) == AF_INET6))
            {
               GET_SOCKADDR_IP_IPV6(saTempAdd, ipAddress);
               port = GET_SOCKADDR_PORT_IPV6(saTempAdd);
               if (expandLocalhost) ExpandLocalhostAddress(ipAddress);
            }
         }
         break;
#endif

         default:
            // empty
         break;
      }
   }

   return IPAddressAndPort(ipAddress, port);
}

/* See the header file for description of what this does */
status_t CreateConnectedSocketPair(ConstSocketRef & socket1, ConstSocketRef & socket2, bool blocking)
{
   TCHECKPOINT;

   status_t ret;

#if defined(USE_SOCKETPAIR)
   int temp[2];
   if (socketpair(AF_UNIX, SOCK_STREAM, 0, temp) == 0)
   {
      socket1 = GetConstSocketRefFromPool(temp[0]);
      socket2 = GetConstSocketRefFromPool(temp[1]);
      if ((SetSocketBlockingEnabled(socket1, blocking).IsOK(ret))&&(SetSocketBlockingEnabled(socket2, blocking).IsOK(ret))) return B_NO_ERROR;
   }
   else return B_ERRNO;
#else
   uint16 port;
   socket1 = CreateAcceptingSocket(0, 1, &port, localhostIP);
   if (socket1())
   {
      socket2 = Connect(IPAddressAndPort(localhostIP, port));
      if (socket2())
      {
         ConstSocketRef newfd = Accept(socket1);
         if (newfd())
         {
            socket1 = newfd;
            if ((SetSocketBlockingEnabled(socket1, blocking).IsOK(ret))&&(SetSocketBlockingEnabled(socket2, blocking).IsOK(ret)))
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
   return ret | B_IO_ERROR;
}

status_t SetSocketBlockingEnabled(const ConstSocketRef & sock, bool blocking)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

#ifdef WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? B_NO_ERROR : B_ERRNO;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return B_ERRNO;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? B_NO_ERROR : B_ERRNO;
#endif
}

bool GetSocketBlockingEnabled(const ConstSocketRef & sock)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

#ifdef WIN32
   // Amazingly enough, Windows has no way to query if a socket is blocking or not.  So this function doesn't work under Windows.
   LogTime(MUSCLE_LOG_ERROR, "GetSocketBlockingEnabled() not implemented under Win32, returning false for socket %i.\n", fd);
   return false;
#else
   const int flags = fcntl(fd, F_GETFL, 0);
   return ((flags >= 0)&&((flags & O_NONBLOCK) == 0));
#endif
}

status_t SetUDPSocketBroadcastEnabled(const ConstSocketRef & sock, bool broadcast)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int val = (broadcast ? 1 : 0);
   return (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (const sockopt_arg *) &val, sizeof(val)) == 0) ? B_NO_ERROR : B_ERRNO;
}

bool GetUDPSocketBroadcastEnabled(const ConstSocketRef & sock)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

   int val;
   muscle_socklen_t len = sizeof(val);
   return (getsockopt(fd, SOL_SOCKET, SO_BROADCAST, (sockopt_arg *) &val, &len) == 0) ? (val != 0) : false;
}

status_t SetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock, bool enabled)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int enableNoDelay = enabled ? 0 : 1;
   return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const sockopt_arg *) &enableNoDelay, sizeof(enableNoDelay)) == 0) ? B_NO_ERROR : B_ERRNO;
}

bool GetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

   int enableNoDelay;
   muscle_socklen_t len = sizeof(enableNoDelay);
   return (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (sockopt_arg *) &enableNoDelay, &len) == 0) ? (enableNoDelay == 0) : false;
}

status_t SetSocketCorkAlgorithmEnabled(const ConstSocketRef & sock, bool enabled)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__)
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int iEnabled = enabled;  // it's gotta be an int!
# if defined(__linux__)
   const int corkOpt = TCP_CORK;
# else
   const int corkOpt = TCP_NOPUSH;
# endif
   return (setsockopt(fd, IPPROTO_TCP, corkOpt, (const sockopt_arg *) &iEnabled, sizeof(iEnabled)) == 0) ? B_NO_ERROR : B_ERRNO;
#else
   (void) sock;
   (void) enabled;
   return B_UNIMPLEMENTED;
#endif
}

bool GetSocketCorkAlgorithmEnabled(const ConstSocketRef & sock)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__)
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return false;

   int enabled;
   muscle_socklen_t len = sizeof(enabled);
# if defined(__linux__)
   const int corkOpt = TCP_CORK;
# else
   const int corkOpt = TCP_NOPUSH;
# endif
   return (getsockopt(fd, IPPROTO_TCP, corkOpt, (sockopt_arg *) &enabled, &len) == 0) ? (bool)enabled : false;
#else
   (void) sock;
   return false;
#endif
}

status_t FinalizeAsyncConnect(const ConstSocketRef & sock)
{
   TCHECKPOINT;

   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

#if defined(__FreeBSD__) || defined(BSD)
   // Nathan Whitehorn reports that send() doesn't do this trick under FreeBSD 7,
   // so for BSD we'll call getpeername() instead.  -- jaf
   struct sockaddr_in6 junk;  // gotta be in6 to avoid buffer-too-short problems
   muscle_socklen_t length = sizeof(junk);
   memset(&junk, 0, sizeof(junk));
   return (getpeername(fd, (struct sockaddr *)&junk, &length) == 0) ? B_NO_ERROR : B_ERRNO;
#else
   // For most platforms, the code below is all we need
   char junk;
   return (send_ignore_eintr(fd, &junk, 0, 0L) == 0) ? B_NO_ERROR : B_ERRNO;
#endif
}

static status_t SetSocketBufferSizeAux(const ConstSocketRef & sock, uint32 numBytes, int optionName)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int iSize = (int) numBytes;
   return (setsockopt(fd, SOL_SOCKET, optionName, (const sockopt_arg *) &iSize, sizeof(iSize)) == 0) ? B_NO_ERROR : B_ERRNO;
}
status_t SetSocketSendBufferSize(   const ConstSocketRef & sock, uint32 sendBufferSizeBytes) {return SetSocketBufferSizeAux(sock, sendBufferSizeBytes, SO_SNDBUF);}
status_t SetSocketReceiveBufferSize(const ConstSocketRef & sock, uint32 recvBufferSizeBytes) {return SetSocketBufferSizeAux(sock, recvBufferSizeBytes, SO_RCVBUF);}

static int32 GetSocketBufferSizeAux(const ConstSocketRef & sock, int optionName)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return -1;

   int iSize;
   muscle_socklen_t len = sizeof(iSize);
   return (getsockopt(fd, SOL_SOCKET, optionName, (sockopt_arg *) &iSize, &len) == 0) ? (int32)iSize : -1;
}
int32 GetSocketSendBufferSize(   const ConstSocketRef & sock) {return GetSocketBufferSizeAux(sock, SO_SNDBUF);}
int32 GetSocketReceiveBufferSize(const ConstSocketRef & sock) {return GetSocketBufferSizeAux(sock, SO_RCVBUF);}

NetworkInterfaceInfo :: NetworkInterfaceInfo()
   : _ip(invalidIP)
   , _netmask(invalidIP)
   , _broadcastIP(invalidIP)
   , _enabled(false)
   , _copper(false)
   , _macAddress(0)
   , _hardwareType(NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)
   , _mtu(0)
{
   // empty
}

NetworkInterfaceInfo :: NetworkInterfaceInfo(const String &name, const String & desc, const IPAddress & ip, const IPAddress & netmask, const IPAddress & broadIP, bool enabled, bool copper, uint64 macAddress, uint32 hardwareType, uint32 mtu)
   : _name(name)
   , _desc(desc)
   , _ip(ip)
   , _netmask(netmask)
   , _broadcastIP(broadIP)
   , _enabled(enabled)
   , _copper(copper)
   , _macAddress(macAddress)
   , _hardwareType(hardwareType)
   , _mtu(mtu)
{
   // empty
}

static String MACAddressToString(uint64 mac)
{
   if (mac == 0) return "None";

   char buf[128];
   muscleSprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
       (unsigned) ((mac>>(5*8))&0xFF),
       (unsigned) ((mac>>(4*8))&0xFF),
       (unsigned) ((mac>>(3*8))&0xFF),
       (unsigned) ((mac>>(2*8))&0xFF),
       (unsigned) ((mac>>(1*8))&0xFF),
       (unsigned) ((mac>>(0*8))&0xFF));
   return buf;
}

const char * NetworkInterfaceInfo :: GetNetworkHardwareTypeString(uint32 hardwareType)
{
   static const char * _hardwareTypeStrs[NUM_NETWORK_INTERFACE_HARDWARE_TYPES] = {
      "Unknown",
      "Loopback",
      "Ethernet",
      "WiFi",
      "TokenRing",
      "PPP",
      "ATM",
      "Tunnel",
      "Bridge",
      "FireWire",
      "Bluetooth",
      "Bonded",
      "IrDA",
      "Dialup",
      "Serial",
      "VLAN",
      "Cellular",
   };
   if (hardwareType >= ARRAYITEMS(_hardwareTypeStrs)) hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;
   return _hardwareTypeStrs[hardwareType];
}

bool NetworkInterfaceInfo :: operator == (const NetworkInterfaceInfo & rhs) const
{
   return ((_name         == rhs._name)
         &&(_desc         == rhs._desc)
         &&(_ip           == rhs._ip)
         &&(_netmask      == rhs._netmask)
         &&(_broadcastIP  == rhs._broadcastIP)
         &&(_enabled      == rhs._enabled)
         &&(_copper       == rhs._copper)
         &&(_macAddress   == rhs._macAddress)
         &&(_hardwareType == rhs._hardwareType)
         &&(_mtu          == rhs._mtu));
}

String NetworkInterfaceInfo :: ToString() const
{
   String ret = String("Name=[%1] Description=[%2] Type=[%3] IP=[%4] Netmask=[%5]").Arg(_name).Arg(_desc).Arg(GetNetworkHardwareTypeString(_hardwareType)).Arg(Inet_NtoA(_ip)).Arg(Inet_NtoA(_netmask));
   ret += String(" Broadcast=[%1] MAC=[%2] Enabled=%3 Copper=%4 MTU=%5").Arg(Inet_NtoA(_broadcastIP)).Arg(MACAddressToString(_macAddress)).Arg(_enabled).Arg(_copper).Arg(_mtu);
   return ret;
}

uint32 NetworkInterfaceInfo :: HashCode() const
{
   return _name.HashCode() + _desc.HashCode() + _hardwareType + _ip.HashCode() + _netmask.HashCode() + _broadcastIP.HashCode() + CalculateHashCode(_macAddress) +_enabled + _copper + _mtu;
}

const char * _gniiFlagLabels[] = {
   "IncludeIPv4",
   "IncludeIPv6",
   "IncludeLoopback",
   "IncludeNonLoopback",
   "IncludeEnabled",
   "IncludeDisabled",
   "IncludeLoopbackOnlyAsLastResort",
   "IncludeUnaddressed",
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_gniiFlagLabels, NUM_GNII_FLAGS);

#if defined(USE_GETIFADDRS) || defined(WIN32)
static IPAddress SockAddrToIPAddr(const struct sockaddr * a)
{
   if (a)
   {
      switch(a->sa_family)
      {
         case AF_INET:
         {
            const struct sockaddr_in & a4 = *(reinterpret_cast<const struct sockaddr_in *>(a));
            return IPAddress(a4.sin_addr);
         }

#ifndef MUSCLE_AVOID_IPV6
         case AF_INET6:
         {
            const struct sockaddr_in6 & a6 = *(reinterpret_cast<const struct sockaddr_in6 *>(a));
            return IPAddress(a6.sin6_addr, a6.sin6_scope_id);
         }
#endif

         default:
            /* empty */
         break;
      }
   }
   return invalidIP;
}
#endif

bool IPAddress :: IsIPv4() const
{
#ifdef MUSCLE_AVOID_IPV6
   return true;
#else
   if ((EqualsIgnoreInterfaceIndex(invalidIP))||(EqualsIgnoreInterfaceIndex(localhostIP_IPv6))) return false;  // :: and ::1 are considered to be IPv6 addresses

   if (GetHighBits() != 0) return false;
   const uint64 lb = (_lowBits>>32);
   return ((lb == 0)||(lb == 0xFFFF));  // 32-bit and IPv4-mapped, respectively
#endif
}

bool IPAddress :: IsValid() const
{
#ifdef MUSCLE_AVOID_IPV6
   return ((_highBits == 0)&&((_lowBits & (((uint64)0xFFFFFFFF)<<32)) == 0)&&(_lowBits != 0));
#else
   return ((_highBits != 0)||(_lowBits != 0));
#endif
}

bool IPAddress :: IsMulticast() const
{
#ifndef MUSCLE_AVOID_IPV6
   // In IPv6, any address that starts with 0xFF is a multicast address
   if (((_highBits >> 56)&((uint64)0xFF)) == 0xFF) return true;

   const uint64 mapBits = (((uint64)0xFFFF)<<32);
   if ((_highBits == 0)&&((_lowBits & mapBits) == mapBits))
   {
      IPAddress temp = *this; temp.SetLowBits(temp.GetLowBits() & ~mapBits);
      return temp.IsMulticast();  // don't count the map-to-IPv6 bits when determining multicast-ness
   }
#endif

   // check for IPv4 address-ness
   const IPAddress minMulticastAddress = Inet_AtoN("224.0.0.0");
   const IPAddress maxMulticastAddress = Inet_AtoN("239.255.255.255");
   return muscleInRange(_lowBits, minMulticastAddress.GetLowBits(), maxMulticastAddress.GetLowBits());
}

bool IPAddress :: IsBroadcast() const
{
   return ((IsIPv4())&&((GetLowBits() & 0xFF) == 0xFF));
}

bool IPAddress :: IsIPv6LinkLocal() const
{
   if (IsIPv4()) return false;

   const uint16 highestShort = (uint16) ((GetHighBits()>>48) & 0xFFFF);
   if (highestShort == 0xFE80) return true;  // link-local unicast address!
   if (((highestShort&0xFF00) == 0xFF00)&&((highestShort & 0x0F) == 0x02)) return true;  // link-local multicast address!
   return false;  // all others don't need/want a sin6_scope_id
}

bool IPAddress :: IsIPv6LocalMulticast(uint8 scope) const
{
   if ((IsIPv4() == false)&&(IsMulticast()))
   {
      const uint64 highBits = GetHighBits();
      const uint64 topEight = (((uint64)0xFF)<<56);
      if ((highBits & topEight) == topEight)
      {
         const uint8 scopeBits = (highBits >> 48) & 0x0F;
         return (scopeBits==scope);
      }
   }
   return false;
}

bool IPAddress :: IsStandardLoopbackDeviceAddress() const
{
#ifdef MUSCLE_AVOID_IPV6
   return (*this == localhostIP_IPv4);
#else
   // fe80::1 is another name for localhostIP in IPv6 land
   static const IPAddress localhostIP_linkScope(localhostIP.GetLowBits(), ((uint64)0xFE80)<<48);
   return ((EqualsIgnoreInterfaceIndex(localhostIP_IPv6))||(EqualsIgnoreInterfaceIndex(localhostIP_IPv4))||(EqualsIgnoreInterfaceIndex(localhostIP_linkScope)));
#endif
}

bool IPAddress :: IsSelfAssigned() const
{
   if (IsIPv4())
   {
      // In IPv4-land, any IP address of the form 169.254.*.* is a self-assigned IP address
      return ((((_lowBits >> 24) & 0xFF) == 169) && (((_lowBits >> 16) & 0xFF) == 254));
   }

#ifndef MUSCLE_AVOID_IPV6
   // In IPv6-land, andy IP address of the form fe80::* is a self-assigned IP address
   return (((_highBits >> 48) & 0xFFFF) == 0xFE80);
#else
   return false;
#endif
}

static bool IsGNIIBitMatch(const IPAddress & ip, bool isInterfaceEnabled, GNIIFlags includeFlags)
{
   if ((includeFlags.IsBitSet(GNII_FLAG_INCLUDE_ENABLED_INTERFACES)  == false)&&( isInterfaceEnabled)) return false;
   if ((includeFlags.IsBitSet(GNII_FLAG_INCLUDE_DISABLED_INTERFACES) == false)&&(!isInterfaceEnabled)) return false;

   if (ip == invalidIP)
   {
      if (includeFlags.IsBitSet(GNII_FLAG_INCLUDE_UNADDRESSED_INTERFACES) == false) return false;  // FogBugz #10286
   }
   else
   {
      const bool isLoopback = ip.IsStandardLoopbackDeviceAddress();
      if ((includeFlags.IsBitSet(GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES)    == false)&&( isLoopback)) return false;
      if ((includeFlags.IsBitSet(GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES) == false)&&(!isLoopback)) return false;

      const bool isIPv4 = ip.IsIPv4();
      if (( isIPv4)&&(includeFlags.IsBitSet(GNII_FLAG_INCLUDE_IPV4_INTERFACES) == false)) return false;
      if ((!isIPv4)&&(includeFlags.IsBitSet(GNII_FLAG_INCLUDE_IPV6_INTERFACES) == false)) return false;
   }

   return true;
}

# if defined(__APPLE__) && !(TARGET_OS_IPHONE)
// Given an Apple-style interface-type string, returns the corresponding NETWORK_INTERFACE_HARDWARE_TYPE_* value, or NETWORK_INTERFACE_TYPE_UNKNOWN.
static uint32 ParseAppleInterfaceTypeString(CFStringRef appleTypeString)
{
   static const CFStringRef _appleTypeStrings[] = {
      kSCNetworkInterfaceType6to4,
      kSCNetworkInterfaceTypeBluetooth,
      kSCNetworkInterfaceTypeBond,
      kSCNetworkInterfaceTypeEthernet,
      kSCNetworkInterfaceTypeFireWire,
      kSCNetworkInterfaceTypeIEEE80211,
      kSCNetworkInterfaceTypeIPSec,
#if (__MAC_OS_X_VERSION_MIN_REQUIRED < 120000)
      kSCNetworkInterfaceTypeIrDA,
#endif
      kSCNetworkInterfaceTypeL2TP,
      kSCNetworkInterfaceTypeModem,
      kSCNetworkInterfaceTypePPP,
      String("PPTP").ToCFStringRef(),   // was kSCNetworkInterfaceTypePPTP but I grew tired of the MacOS header complaining that it was deprecated --jaf
      kSCNetworkInterfaceTypeSerial,
      kSCNetworkInterfaceTypeVLAN,
      kSCNetworkInterfaceTypeWWAN,
      kSCNetworkInterfaceTypeIPv4,
   };
   static const uint32 _muscleTypes[] = {
      NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL,
      NETWORK_INTERFACE_HARDWARE_TYPE_BLUETOOTH,
      NETWORK_INTERFACE_HARDWARE_TYPE_BONDED,
      NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET,
      NETWORK_INTERFACE_HARDWARE_TYPE_FIREWIRE,
      NETWORK_INTERFACE_HARDWARE_TYPE_WIFI,
      NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL,   // IPSec is a form of tunnel, no?
#if (__MAC_OS_X_VERSION_MIN_REQUIRED < 120000)
      NETWORK_INTERFACE_HARDWARE_TYPE_IRDA,
#endif
      NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL,   // L2TP is a form of tunnel, no?
      NETWORK_INTERFACE_HARDWARE_TYPE_DIALUP,
      NETWORK_INTERFACE_HARDWARE_TYPE_PPP,
      NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL,   // PPTP is an obsolete form of tunnel
      NETWORK_INTERFACE_HARDWARE_TYPE_SERIAL,
      NETWORK_INTERFACE_HARDWARE_TYPE_VLAN,
      NETWORK_INTERFACE_HARDWARE_TYPE_CELLULAR,
      NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN,  // IPv4 isn't a hardware-type AFAIK!?
   };

   for (uint32 i=0; i<ARRAYITEMS(_appleTypeStrings); i++) if (CFStringCompare(appleTypeString, _appleTypeStrings[i], 0) == kCFCompareEqualTo) return _muscleTypes[i];

   // There doesn't appear to be a constant declared for "Bridge", but Apple returns it sometimes, so we might as well recognize it
   const String s(appleTypeString);
   if (s.EqualsIgnoreCase("bridge")) return NETWORK_INTERFACE_HARDWARE_TYPE_BRIDGE;

   return NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;
}
#endif

#ifdef WIN32
static uint32 ConvertWindowsInterfaceType(DWORD ifType)
{
   switch(ifType)
   {
      case IF_TYPE_ETHERNET_CSMACD:    return NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET;
      case IF_TYPE_ISO88025_TOKENRING: return NETWORK_INTERFACE_HARDWARE_TYPE_TOKENRING;
      case IF_TYPE_PPP:                return NETWORK_INTERFACE_HARDWARE_TYPE_PPP;
      case IF_TYPE_SOFTWARE_LOOPBACK:  return NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK;
      case IF_TYPE_ATM:                return NETWORK_INTERFACE_HARDWARE_TYPE_ATM;
      case IF_TYPE_IEEE80211:          return NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;
      case IF_TYPE_TUNNEL:             return NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL;
      case IF_TYPE_IEEE1394:           return NETWORK_INTERFACE_HARDWARE_TYPE_FIREWIRE;
      case IF_TYPE_OTHER: default:     return NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;
   }
}
#endif

#if defined(__linux__) && !defined(MUSCLE_AVOID_LINUX_DETECT_NETWORK_HARDWARE_TYPES)
static uint32 ConvertLinuxInterfaceType(int saFamily)
{
   switch(saFamily)
   {
      case ARPHRD_ETHER: case ARPHRD_EETHER:         return NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET;
      case ARPHRD_PRONET:                            return NETWORK_INTERFACE_HARDWARE_TYPE_TOKENRING;
      case ARPHRD_ATM:                               return NETWORK_INTERFACE_HARDWARE_TYPE_ATM;
      case ARPHRD_IEEE802:                           return NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;
      case ARPHRD_IEEE1394:                          return NETWORK_INTERFACE_HARDWARE_TYPE_FIREWIRE;
      case ARPHRD_PPP:                               return NETWORK_INTERFACE_HARDWARE_TYPE_PPP;
      case ARPHRD_TUNNEL: case ARPHRD_TUNNEL6:       return NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL;
      case ARPHRD_LOOPBACK:                          return NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK;
      case ARPHRD_IRDA:                              return NETWORK_INTERFACE_HARDWARE_TYPE_IRDA;
      case ARPHRD_IEEE802_TR: case ARPHRD_IEEE80211: return NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;
      default:                                       return NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;
   }
}
#endif

#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__) || defined(__linux__)
static uint32 GetNetworkInterfaceMTU(const ConstSocketRef & dummySocket, const String & iname)
{
   struct ifreq ifr;
   memset(&ifr, 0, sizeof(ifr));
   memcpy(ifr.ifr_name, iname(), iname.FlattenedSize());

   uint32 mtu = 0;
   if ((dummySocket())&&(ioctl(dummySocket()->GetFileDescriptor(), SIOCGIFMTU, &ifr) == 0)) mtu = ifr.ifr_mtu;
   return mtu;
}
#endif

status_t GetNetworkInterfaceInfos(Queue<NetworkInterfaceInfo> & results, GNIIFlags includeFlags)
{
   const uint32 origResultsSize = results.GetNumItems();
   status_t ret;

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
#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__)
      Hashtable<String, uint32> inameToType;         // e.g. "en0" -> NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET
      Hashtable<String, String> inameToDescription;  // e.g. "en0" -> "Ethernet"
      {
# if defined(__APPLE__) && !(TARGET_OS_IPHONE)
         CFArrayRef interfaces = SCNetworkInterfaceCopyAll();  // we can use this to get network interface hardware types later
         if (interfaces)
         {
            const CFIndex numInterfaces = CFArrayGetCount(interfaces);
            for (CFIndex i=0; i<numInterfaces; i++)
            {
               SCNetworkInterfaceRef ifRef = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(interfaces, i);
               if (ifRef)
               {
                  const String iName(SCNetworkInterfaceGetBSDName(ifRef));
                  if (iName.HasChars()) // paranoia
                  {
                     const uint32 typeVal = ParseAppleInterfaceTypeString(SCNetworkInterfaceGetInterfaceType(ifRef));
                     if (typeVal != NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN) (void) inameToType.Put(iName, typeVal);

                     const String desc(SCNetworkInterfaceGetLocalizedDisplayName(ifRef));
                     if (desc.HasChars()) (void) inameToDescription.Put(iName, desc);
                  }
               }
            }
            CFRelease(interfaces);
         }
# endif  // defined(__APPLE__) && !(TARGET_OS_IPHONE)
      }
#endif

      Hashtable<String, uint64> inameToMAC;
      {
         ConstSocketRef dummySocket = GetConstSocketRefFromPool(socket(AF_UNIX, SOCK_DGRAM, 0));  // just for doing ioctl()s on

         struct ifaddrs * p = ifap;
         while(p)
         {
            const String iname = p->ifa_name;
            if (p->ifa_addr)
            {
#if defined(__FreeBSD__) || defined(BSD) || defined(__APPLE__)
               if (p->ifa_addr->sa_family == AF_LINK)
               {
                  const unsigned char * ptr = (const unsigned char *)LLADDR((struct sockaddr_dl *)p->ifa_addr);
                  uint64 mac = 0; for (uint32 i=0; i<6; i++) mac |= (((uint64)ptr[i])<<(8*(5-i)));
                  (void) inameToMAC.Put(iname, mac);

                  if (inameToType.ContainsKey(iname) == false)
                  {
                     // fall back to trying to get network-interface-type info from the BSD interface
                     const struct if_data * ifd = (const struct if_data *) p->ifa_data;
                     if (ifd)
                     {
                        uint32 devType = NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;
                        switch(ifd->ifi_type)
                        {
                           case IFT_ETHER:      devType = NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET;  break; /* Ethernet or Wi-Fi (!?) */
                           case IFT_ISO88023:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET;  break; /* CMSA CD */
                           case IFT_ISO88025:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_TOKENRING; break; /* Token Ring */
                           case IFT_PPP:        devType = NETWORK_INTERFACE_HARDWARE_TYPE_PPP;       break; /* RFC 1331 */
                           case IFT_LOOP:       devType = NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK;  break; /* loopback */
                           case IFT_SLIP:       devType = NETWORK_INTERFACE_HARDWARE_TYPE_SERIAL;    break; /* IP over generic TTY */
                           case IFT_RS232:      devType = NETWORK_INTERFACE_HARDWARE_TYPE_SERIAL;    break;
                           case IFT_ATM:        devType = NETWORK_INTERFACE_HARDWARE_TYPE_ATM;       break; /* ATM cells */
                           case IFT_MODEM:      devType = NETWORK_INTERFACE_HARDWARE_TYPE_DIALUP;    break; /* Generic Modem */
                           case IFT_L2VLAN:     devType = NETWORK_INTERFACE_HARDWARE_TYPE_VLAN;      break; /* Layer 2 Virtual LAN using 802.1Q */
                           case IFT_IEEE1394:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_FIREWIRE;  break; /* IEEE1394 High Performance SerialBus*/
                           case IFT_BRIDGE:     devType = NETWORK_INTERFACE_HARDWARE_TYPE_BRIDGE;    break; /* Transparent bridge interface */
                           case IFT_ENC:        devType = NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL;    break; /* Encapsulation */
                           case IFT_CELLULAR:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_CELLULAR;  break; /* Packet Data over Cellular */
                           default:             /* empty */                                          break;
                        }

#if defined(__APPLE__)
                        // Oops, IFT_ETHER is still ambiguous, since it could actually be Wired Ethernet or Wi-Fi or etc.
                        // let's investigate a bit further and try to figure out what this network interface *really* is!
                        if ((dummySocket())&&((devType == NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)||(devType == NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET)))
                        {
                           struct ifreq ifr; memset(&ifr, 0, sizeof(ifr)); memcpy(ifr.ifr_name, iname(), iname.FlattenedSize());
                           if (ioctl(dummySocket()->GetFileDescriptor(), SIOCGIFFUNCTIONALTYPE, &ifr) == 0)
                           {
                              switch(ifr.ifr_ifru.ifru_functional_type)
                              {
                                 case IFRTYPE_FUNCTIONAL_LOOPBACK:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK; break;
                                 case IFRTYPE_FUNCTIONAL_WIRED:      devType = NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET; break;
                                 case IFRTYPE_FUNCTIONAL_WIFI_INFRA: devType = NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;     break;
                                 case IFRTYPE_FUNCTIONAL_WIFI_AWDL:  devType = NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;     break;
                                 case IFRTYPE_FUNCTIONAL_CELLULAR:   devType = NETWORK_INTERFACE_HARDWARE_TYPE_CELLULAR; break;
                                 default:                            /* empty */                                         break;
                              }
                           }
                        }
#endif

                        if (devType != NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN) (void) inameToType.Put(iname, devType);
                     }
                  }
               }
#elif defined(__linux__)
               if (p->ifa_addr->sa_family == AF_PACKET)
               {
                  const struct sockaddr_ll * s = (const struct sockaddr_ll *) p->ifa_addr;
                  uint64 mac = 0; for (uint32 i=0; i<6; i++) mac |= (((uint64)s->sll_addr[i])<<(8*(5-i)));
                  (void) inameToMAC.Put(iname, mac);
               }
#endif
            }

            IPAddress unicastIP     = SockAddrToIPAddr(p->ifa_addr);
            const IPAddress netmask = SockAddrToIPAddr(p->ifa_netmask);
            const IPAddress broadIP = SockAddrToIPAddr(p->ifa_broadaddr);
            const bool isEnabled    = ((p->ifa_flags & IFF_UP)      != 0);
            const bool hasCopper    = ((p->ifa_flags & IFF_RUNNING) != 0);
            uint32 hardwareType     = NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN;  // default

            const String * ifDesc = NULL;  // may be set below
#if defined(__APPLE__)
            hardwareType = inameToType.GetWithDefault(iname, NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN);
            ifDesc = inameToDescription.Get(iname);
#elif defined(__linux__) && !defined(MUSCLE_AVOID_LINUX_DETECT_NETWORK_HARDWARE_TYPES)
            if (dummySocket() == NULL) dummySocket = GetConstSocketRefFromPool(socket(AF_UNIX, SOCK_DGRAM, 0));
            if (dummySocket())
            {
               struct ifreq ifr;
               if (iname.Length() < sizeof(ifr.ifr_name))  // strictly less-than because there's also the NUL byte
               {
                  memcpy(ifr.ifr_name, iname(), iname.Length()+1);
                  if (ioctl(dummySocket()->GetFileDescriptor(), SIOCGIFHWADDR, &ifr) == 0) hardwareType = ConvertLinuxInterfaceType(ifr.ifr_hwaddr.sa_family);
               }
            }
#endif

            if ((hardwareType == NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)&&(unicastIP.IsStandardLoopbackDeviceAddress())) hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK;

            if (IsGNIIBitMatch(unicastIP, isEnabled, includeFlags))
            {
#ifndef MUSCLE_AVOID_IPV6
               if (unicastIP.IsIPv4() == false) unicastIP.SetInterfaceIndex(if_nametoindex(iname()));  // so the user can find out; it will be ignored by the TCP stack
#endif

               // coverity[dead_error_line] - it's okay, ifDesc does get modified in the MacOS case
               if (results.AddTail(NetworkInterfaceInfo(iname, ifDesc?*ifDesc:GetEmptyString(), unicastIP, netmask, broadIP, isEnabled, hasCopper, 0, hardwareType, GetNetworkInterfaceMTU(dummySocket, iname))).IsOK(ret))  // MAC address will be set later
               {
                  DECLARE_MUTEXGUARD(_cachedLocalhostAddressLock);
                  if (_cachedLocalhostAddress == invalidIP) _cachedLocalhostAddress = unicastIP;
               }
               else break;
            }
            p = p->ifa_next;
         }
      }
      freeifaddrs(ifap);

      if (inameToMAC.HasItems()) for (uint32 i=0; i<results.GetNumItems(); i++) results[i]._macAddress = inameToMAC.GetWithDefault(results[i].GetName());

      // If we have any interfaces that still have a unknown-hardware-type, see if we can figure out what they are by
      // looking at other interfaces with the same name.  This helps e.g. with lo0 on Mac.
      for (uint32 i=0; i<results.GetNumItems(); i++)
      {
         NetworkInterfaceInfo & nii = results[i];
         if (nii.GetHardwareType() == NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)
         {
            for (uint32 j=0; j<results.GetNumItems(); j++)
            {
               const NetworkInterfaceInfo & anotherNII = results[j];
               if ((i != j)&&(anotherNII.GetHardwareType() != NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)&&(anotherNII.GetName() == nii.GetName()))
               {
                  nii._hardwareType = anotherNII.GetHardwareType();
                  break;
               }
            }
         }

         // and finally some heuristics as a last resort
         if (nii.GetHardwareType() == NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN)
         {
            const String & iname = nii.GetName();
                 if (iname.ContainsIgnoreCase("tun"))    nii._hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL;
            else if (iname.StartsWithIgnoreCase("ppp"))  nii._hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_PPP;
            else if (iname.StartsWithIgnoreCase("bond")) nii._hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_BONDED;
            else if (iname.StartsWithIgnoreCase("awdl")) nii._hardwareType = NETWORK_INTERFACE_HARDWARE_TYPE_WIFI;
         }
      }
   }
   else ret = B_ERRNO;
#elif defined(WIN32)
   // IPv6 implementation, adapted from
   // http://msdn.microsoft.com/en-us/library/aa365915(VS.85).aspx
   //
#ifdef _MSC_VER
# pragma warning( push )
# pragma warning( disable: 4996 )
#endif
   SOCKET s = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
#ifdef _MSC_VER
# pragma warning( pop )
#endif
   if (s == INVALID_SOCKET) return B_ERROR("WSASocket() Failed");

   INTERFACE_INFO localAddrs[64];  // Assume there will be no more than 64 IP interfaces
   DWORD bytesReturned;
   if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, NULL, 0, &localAddrs, sizeof(localAddrs), &bytesReturned, NULL, NULL) == SOCKET_ERROR)
   {
      ret = B_ERRNO;
      closesocket(s);
      return ret;
   }
   else closesocket(s);

   PIP_ADAPTER_ADDRESSES pAddresses = NULL;
   ULONG outBufLen = 0;
   ret = B_ERROR;  // so we can enter the while-loop
   while(ret.IsError())  // keep going until we succeeded (on failure we'll return directly)
   {
      const DWORD flags = GAA_FLAG_INCLUDE_PREFIX|GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER;
      switch(GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &outBufLen))
      {
         case ERROR_BUFFER_OVERFLOW:
            if (pAddresses) muscleFree(pAddresses);

            // Add some breathing room so that if the required size
            // grows during the time period between the two calls to
            // GetAdaptersAddresses(), we won't have to reallocate again.
            outBufLen *= 2;

            pAddresses = (IP_ADAPTER_ADDRESSES *) muscleAlloc(outBufLen);
            MRETURN_OOM_ON_NULL(pAddresses);
         break;

         case ERROR_SUCCESS:
            for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses; ((pCurrAddresses = pCurrAddresses->Next) != NULL))
            {
               PIP_ADAPTER_UNICAST_ADDRESS ua = pCurrAddresses->FirstUnicastAddress;
               while(ua)
               {
                  IPAddress unicastIP = SockAddrToIPAddr(ua->Address.lpSockaddr);
                  const IPAddress ipv4_limited_broadcast_address(0xFFFFFFFF);

                  const bool isEnabled = true;  // It appears that GetAdaptersAddresses() only returns enabled adapters
                  if (IsGNIIBitMatch(unicastIP, isEnabled, includeFlags))
                  {
                     IPAddress broadIP, netmask;
                     const uint32 numLocalAddrs = (bytesReturned/sizeof(INTERFACE_INFO));
                     for (uint32 i=0; i<numLocalAddrs; i++)
                     {
                        const IPAddress nextIP = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiAddress);
                        if (nextIP == unicastIP)
                        {
                           broadIP = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiBroadcastAddress);
                           netmask = SockAddrToIPAddr((const sockaddr *) &localAddrs[i].iiNetmask);

                           // Berkeley FogBugz #9902:  If GetAdaptersAddresses() wants to be dumb
                           // and just return 255.255.255.255 as the broadcast address, then we'll
                           // just have to compute the direct broadcast address ourselves!
#ifdef MUSCLE_AVOID_IPV6
                           if (broadIP == ipv4_limited_broadcast_address) broadIP = (unicastIP & netmask) | ~netmask;
#else
                           if ((unicastIP.IsIPv4())&&(broadIP.EqualsIgnoreInterfaceIndex(ipv4_limited_broadcast_address))) broadIP.SetLowBits((unicastIP.GetLowBits() & netmask.GetLowBits()) | (0xFFFFFFFF & ~(netmask.GetLowBits())));
#endif
                           break;
                        }
                     }

#ifndef MUSCLE_AVOID_IPV6
                     unicastIP.SetInterfaceIndex(pCurrAddresses->Ipv6IfIndex);  // so the user can find out; it will be ignore by the TCP stack
#endif

                     char outBuf[512];
                     if (WideCharToMultiByte(CP_UTF8, 0, pCurrAddresses->Description, -1, outBuf, sizeof(outBuf), NULL, NULL) <= 0) outBuf[0] = '\0';

                     uint64 mac = 0;
                     if (pCurrAddresses->PhysicalAddressLength == 6) for (uint32 i=0; i<6; i++) mac |= (((uint64)(pCurrAddresses->PhysicalAddress[i]))<<(8*(5-i)));

                     const bool hasCopper = (pCurrAddresses->OperStatus==IfOperStatusUp);
                     const uint32 hardwareType = ConvertWindowsInterfaceType(pCurrAddresses->IfType);
                     if (results.AddTail(NetworkInterfaceInfo(pCurrAddresses->AdapterName, outBuf, unicastIP, netmask, broadIP, isEnabled, hasCopper, mac, hardwareType, pCurrAddresses->Mtu)).IsOK(ret))
                     {
                        if (_cachedLocalhostAddress == invalidIP) _cachedLocalhostAddress = unicastIP;
                     }
                     else
                     {
                        if (pAddresses) muscleFree(pAddresses);
                        return ret;
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
           return B_ERRNO;
      }
   }
#else
   return B_UNIMPLEMENTED; // for other OS's (including Android before API level 24), this function isn't implemented (patches are welcome!)
#endif

   return ((ret.IsOK())&&(results.GetNumItems() == origResultsSize)&&(includeFlags.IsBitSet(GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT))) ? GetNetworkInterfaceInfos(results, includeFlags.WithBit(GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES).WithoutBit(GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT)) : ret;
}

status_t GetNetworkInterfaceAddresses(Queue<IPAddress> & results, GNIIFlags includeFlags)
{
   Queue<NetworkInterfaceInfo> infos;
   MRETURN_ON_ERROR(GetNetworkInterfaceInfos(infos, includeFlags));
   MRETURN_ON_ERROR(results.EnsureSize(infos.GetNumItems()));
   for (uint32 i=0; i<infos.GetNumItems(); i++) (void) results.AddTail(infos[i].GetLocalAddress());  // guaranteed not to fail
   return B_NO_ERROR;
}

static void Inet4_NtoA(uint32 addr, char * buf)
{
   if (addr) muscleSnprintf(buf, 16, INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC "." INT32_FORMAT_SPEC, (addr>>24)&0xFF, (addr>>16)&0xFF, (addr>>8)&0xFF, (addr>>0)&0xFF);
        else *buf = '\0';  // return an invalid IPv4 address as ""
}

void Inet_NtoA(const IPAddress & addr, char * ipbuf, bool preferIPv4)
{
#ifdef MUSCLE_AVOID_IPV6
   (void) preferIPv4;
   Inet4_NtoA(addr.GetIPv4AddressAsUint32(), ipbuf);
#else
   if ((preferIPv4)&&((addr.IsValid() == false)||(addr.IsIPv4()))) Inet4_NtoA((uint32)(addr.GetLowBits()&0xFFFFFFFF), ipbuf);
   else
   {
      const int MIN_IPBUF_LENGTH = 64;
      uint8 ip6[16]; addr.WriteToNetworkArray(ip6, NULL);
      if (Inet_NtoP(AF_INET6, (const in6_addr *) ip6, ipbuf, MIN_IPBUF_LENGTH) != NULL)
      {
         if (addr.IsInterfaceIndexValid())
         {
            // Add the index suffix
            const size_t ipbuflen = strlen(ipbuf);
            muscleSnprintf(ipbuf+ipbuflen, MIN_IPBUF_LENGTH-ipbuflen, "@" UINT32_FORMAT_SPEC, addr.GetInterfaceIndex());
         }
      }
      else ipbuf[0] = '\0';
   }
#endif
}

String Inet_NtoA(const IPAddress & ipAddress, bool preferIPv4)
{
   char buf[64];
   Inet_NtoA(ipAddress, buf, preferIPv4);
   return buf;
}

static status_t Inet4_AtoN(const char * buf, IPAddress & retIP)
{
   // net_server inexplicably doesn't have this function; so I'll just fake it
   uint32 bits = 0;
   int shift = 24;  // fill out the MSB first
   bool startQuad = true;
   while((shift >= 0)&&(*buf))
   {
      if (startQuad)
      {
         uint8 quad = (uint8) atoi(buf);
         bits |= (((uint32)quad) << shift);
         shift -= 8;
      }
      startQuad = (*buf == '.');
      buf++;
   }
   if (shift >= 0) return B_BAD_ARGUMENT;  // gotta have four dotted quads

   retIP.SetIPv4AddressFromUint32(bits);
   return B_NO_ERROR;
}

#ifndef MUSCLE_AVOID_IPV6
static status_t Inet6_AtoN(const char * buf, uint32 iIdx, IPAddress & retIP)
{
   struct in6_addr dst;
   if (Inet_PtoN(AF_INET6, buf, &dst) > 0)
   {
      retIP.ReadFromNetworkArray(dst.s6_addr, iIdx ? &iIdx : NULL);
      return B_NO_ERROR;
   }
   else return IsIP4Address(buf) ? Inet4_AtoN(buf, retIP) : B_BAD_ARGUMENT;
}
#endif

IPAddress Inet_AtoN(const char * buf)
{
   IPAddress ret;
   return (ret.SetFromString(buf).IsOK()) ? ret : IPAddress();
}

String IPAddress :: ToString(bool preferIPv4Style) const
{
   return Inet_NtoA(*this, preferIPv4Style);
}

status_t IPAddress :: SetFromString(const String & ipAddressString)
{
#ifdef MUSCLE_AVOID_IPV6
   return Inet4_AtoN(ipAddressString(), *this);
#else
   const int32 atIdx = ipAddressString.IndexOf('@');
   if (atIdx >= 0)
   {
      // Gah... Inet_PtoN() won't accept the @idx suffix, so
      // I have to chop that out and parse it separately.
      const String withoutSuffix = ipAddressString.Substring(0, atIdx);
      const String suffix        = ipAddressString.Substring(atIdx+1);
      return Inet6_AtoN(withoutSuffix(), (uint32) Atoll(suffix()), *this);
   }
   else return Inet6_AtoN(ipAddressString(), MUSCLE_NO_LIMIT, *this);
#endif
}

static IPAddress ResolveIP(const String & s, bool allowDNSLookups)
{
   return allowDNSLookups ? GetHostByName(s()) : Inet_AtoN(s());
}

void IPAddressAndPort :: SetFromString(const String & s, uint16 defaultPort, bool allowDNSLookups)
{
#ifndef MUSCLE_AVOID_IPV6
   const int32 rBracket = s.StartsWith('[') ? s.IndexOf(']') : -1;
   if (rBracket >= 0)
   {
      // If there are brackets, they are assumed to surround the address part, e.g. "[::1]:9999"
      _ip = ResolveIP(s.Substring(1,rBracket), allowDNSLookups);

      const int32 colIdx = s.IndexOf(':', rBracket+1);
      _port = ((colIdx >= 0)&&(muscleInRange(s()[colIdx+1], '0', '9'))) ? (uint16)atoi(s()+colIdx+1) : defaultPort;
      return;
   }
   else
   {
      const int32 magicSuffixIdx = s.LastIndexOf("_port_");  // special stupid syntax for when passing bracket-chars on the command-line isn't possible (hi osascript)
      if (magicSuffixIdx >= 0)
      {
         _ip = ResolveIP(s.Substring(0, magicSuffixIdx), allowDNSLookups);

         const int32 portIdx = magicSuffixIdx+6;
         _port = (muscleInRange(s()[portIdx], '0', '9')) ? (uint16)atoi(s()+portIdx) : defaultPort;
         return;
      }
      else if (s.GetNumInstancesOf(':') != 1)  // I assume IPv6-style address strings never have exactly one colon in them
      {
         _ip   = ResolveIP(s, allowDNSLookups);
         _port = defaultPort;
         return;
      }
   }
#endif

   // Old style IPv4 parsing (e.g. "192.168.0.1" or "192.168.0.1:2960")
   const int colIdx = s.IndexOf(':');
   if ((colIdx >= 0)&&(muscleInRange(s()[colIdx+1], '0', '9')))
   {
      _ip   = ResolveIP(s.Substring(0, colIdx), allowDNSLookups);
      _port = (uint16) atoi(s()+colIdx+1);
   }
   else
   {
      _ip   = ResolveIP(s, allowDNSLookups);
      _port = defaultPort;
   }
}

String IPAddressAndPort :: ToString(bool includePort, bool preferIPv4Style) const
{
   const String s = Inet_NtoA(_ip, preferIPv4Style);

   if ((includePort)&&(_port > 0))
   {
      char buf[128];
#ifdef MUSCLE_AVOID_IPV6
      const bool useIPv4Style = true;
#else
      const bool useIPv4Style = ((preferIPv4Style)&&(_ip.IsIPv4()));  // FogBugz #8985
#endif
      if (useIPv4Style) muscleSprintf(buf, "%s:%u",   s(), _port);
                   else muscleSprintf(buf, "[%s]:%u", s(), _port);
      return buf;
   }
   else return s;
}

uint32 IPAddress :: CalculateChecksum() const
{
   return CalculateChecksumForUint64(_lowBits) + CalculateChecksumForUint64(_highBits) + _interfaceIndex;
}

void IPAddress :: Flatten(DataFlattener flat) const
{
   flat.WriteInt64(_lowBits);
   flat.WriteInt64(_highBits);
   flat.WriteInt32(_interfaceIndex);
}

status_t IPAddress :: Unflatten(DataUnflattener & unflat)
{
   _lowBits        = unflat.ReadInt64();
   _highBits       = unflat.ReadInt64();
   _interfaceIndex = unflat.ReadInt32();
   return unflat.GetStatus();
}

void IPAddressAndPort :: Flatten(DataFlattener flat) const
{
   flat.WriteFlat(_ip);
   flat.WriteInt16(_port);
}

status_t IPAddressAndPort :: Unflatten(DataUnflattener & unflat)
{
   _ip   = unflat.ReadFlat<IPAddress>();
   _port = unflat.ReadInt16();
   return unflat.GetStatus();
}

// defined here to avoid having to pull in MiscUtilityFunctions.cpp for everything
String GetConnectString(const String & host, uint16 port)
{
#ifdef MUSCLE_AVOID_IPV6
   char buf[32]; muscleSprintf(buf, ":%u", port);
   return host.WithAppend(buf);
#else
   char buf[32]; muscleSprintf(buf, "]:%u", port);
   return host.WithPrepend("[").WithAppend(buf);
#endif
}

ICallbackMechanism :: ~ICallbackMechanism()
{
   // Prevent dangling pointers in any still-registered ICallbackSubscriber objects
   DECLARE_MUTEXGUARD(_registeredSubscribersMutex);  // probably pointless since any internal thread should be dead already, but just for consistency's sake
   for (HashtableIterator<ICallbackSubscriber *, Void> iter(_registeredSubscribers); iter.HasData(); iter++) iter.GetKey()->SetCallbackMechanism(NULL);
}

void ICallbackMechanism :: DispatchCallbacks()
{
   {
      // Critical section:  grab the set of dirty-subscribers into _scratchSubscribers
      DECLARE_MUTEXGUARD(_dirtySubscribersMutex);
      _scratchSubscribers.SwapContents(_dirtySubscribers);
   }

   // Perform requested callbacks
   {
      DECLARE_MUTEXGUARD(_registeredSubscribersMutex);
      for (HashtableIterator<ICallbackSubscriber *, uint32> iter(_scratchSubscribers); iter.HasData(); iter++)
      {
         ICallbackSubscriber * sub = iter.GetKey();
         if (_registeredSubscribers.ContainsKey(sub)) sub->DispatchCallbacks(iter.GetValue());  // yes, the if-test is necessary!
      }
   }
   _scratchSubscribers.Clear();
}

void ICallbackMechanism :: RequestCallbackInDispatchThread(ICallbackSubscriber * sub, uint32 eventTypeBits, uint32 clearBits)
{
   if (eventTypeBits != 0)
   {
      bool sendSignal = false;
      {
         // Critical section
         DECLARE_MUTEXGUARD(_dirtySubscribersMutex);
         const bool wasEmpty = _dirtySubscribers.IsEmpty();
         uint32 * subBits = _dirtySubscribers.GetOrPut(sub);
         if (subBits)
         {
            *subBits &= ~clearBits;
            *subBits |= eventTypeBits;
            if (wasEmpty) sendSignal = true;
         }
      }

      // Note that we send the signal AFTER releasing the Mutex
      if (sendSignal) SignalDispatchThread();
   }
}

SocketCallbackMechanism :: SocketCallbackMechanism()
{
   status_t ret;
   if (CreateConnectedSocketPair(_dispatchThreadSock, _otherThreadsSock, false).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "SocketCallbackMechanism:  Unable to create notify-socket-pair! [%s]\n", ret());
   }
}

SocketCallbackMechanism :: ~SocketCallbackMechanism()
{
   // empty
}

void SocketCallbackMechanism :: DispatchCallbacks()
{
   // read and discard any signalling-bytes sent by the other threads
   char junkBuf[128];
   (void) ReceiveData(_dispatchThreadSock, junkBuf, sizeof(junkBuf), false);

   // Call superclass to perform the actual callbacks
   ICallbackMechanism::DispatchCallbacks();
}

void SocketCallbackMechanism :: SignalDispatchThread()
{
   const char junk = 'j';
   if (SendData(_otherThreadsSock, &junk, sizeof(junk), false) != sizeof(junk))
   {
      LogTime(MUSCLE_LOG_ERROR, "SocketCallbackMechanism::SignalDispatchThread():  Unable to send notification byte! [%s]\n", B_ERRNO());
   }
}

static IPAddress _customLocalhostIP = invalidIP;  // disabled by default
void SetLocalHostIPOverride(const IPAddress & ip) {_customLocalhostIP = ip;}
IPAddress GetLocalHostIPOverride() {return _customLocalhostIP;}

#ifndef MUSCLE_DISABLE_KEEPALIVE_API

status_t SetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 maxProbeCount, uint64 idleTime, uint64 retransmitTime)
{
#ifdef __linux__
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int arg = MicrosToSecondsRoundUp(idleTime);
   if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &arg, sizeof(arg)) != 0) return B_ERRNO;

   arg = (int) maxProbeCount;
   if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &arg, sizeof(arg)) != 0) return B_ERRNO;

   arg = MicrosToSecondsRoundUp(retransmitTime);
   if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &arg, sizeof(arg)) != 0) return B_ERRNO;

   arg = (maxProbeCount>0);  // true iff we want keepalive enabled
   if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const sockopt_arg *) &arg, sizeof(arg)) != 0) return B_ERRNO;

   return B_NO_ERROR;
#else
   // Other OS's don't support these calls, AFAIK
   (void) sock;
   (void) maxProbeCount;
   (void) idleTime;
   (void) retransmitTime;
   return B_UNIMPLEMENTED;
#endif
}

status_t GetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 * retMaxProbeCount, uint64 * retIdleTime, uint64 * retRetransmitTime)
{
#ifdef __linux__
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   int val;
   muscle_socklen_t valLen;
   if (retMaxProbeCount)
   {
      *retMaxProbeCount = 0;
      valLen = sizeof(val); if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_arg *) &val, &valLen) != 0) return B_ERRNO;
      if (val != 0)  // we only set *retMaxProbeCount if SO_KEEPALIVE is enabled, otherwise we return 0 to indicate no-keepalive
      {
         valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPCNT, (sockopt_arg *) &val, &valLen) != 0) return B_ERRNO;
         *retMaxProbeCount = val;
      }
   }

   if (retIdleTime)
   {
      valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (sockopt_arg *) &val, &valLen) != 0) return B_ERRNO;
      *retIdleTime = SecondsToMicros(val);
   }

   if (retRetransmitTime)
   {
      valLen = sizeof(val); if (getsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (sockopt_arg *) &val, &valLen) != 0) return B_ERRNO;
      *retRetransmitTime = SecondsToMicros(val);
   }

   return B_NO_ERROR;
#else
   // Other OS's don't support these calls, AFAIK
   (void) sock;
   (void) retMaxProbeCount;
   (void) retIdleTime;
   (void) retRetransmitTime;
   return B_UNIMPLEMENTED;
#endif
}

#endif

#ifndef MUSCLE_AVOID_MULTICAST_API

status_t SetSocketMulticastToSelf(const ConstSocketRef & sock, bool multicastToSelf)
{
   const int toSelf = multicastToSelf ? 1 : 0;
   const int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IP,   IP_MULTICAST_LOOP,   (const sockopt_arg *) &toSelf, sizeof(toSelf)) == 0)) ? B_NO_ERROR : B_ERRNO;
#else
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (const sockopt_arg *) &toSelf, sizeof(toSelf)) == 0)) ? B_NO_ERROR : B_ERRNO;
#endif
}

bool GetSocketMulticastToSelf(const ConstSocketRef & sock)
{
   uint8 toSelf;
   muscle_socklen_t size = sizeof(toSelf);
   const int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IP,   IP_MULTICAST_LOOP,   (sockopt_arg *) &toSelf, &size) == 0)&&(size == sizeof(toSelf))&&(toSelf));
#else
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, (sockopt_arg *) &toSelf, &size) == 0)&&(size == sizeof(toSelf))&&(toSelf));
#endif
}

status_t SetSocketMulticastTimeToLive(const ConstSocketRef & sock, uint8 ttl)
{
   const int fd = sock.GetFileDescriptor();
   const int ttl_arg = (int) ttl;
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IP,   IP_MULTICAST_TTL,    (const sockopt_arg *) &ttl_arg, sizeof(ttl_arg)) == 0)) ? B_NO_ERROR : B_ERRNO;
#else
   return ((fd>=0)&&(setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (const sockopt_arg *) &ttl_arg, sizeof(ttl_arg)) == 0)) ? B_NO_ERROR : B_ERRNO;
#endif
}

uint8 GetSocketMulticastTimeToLive(const ConstSocketRef & sock)
{
   int ttl = 0;
   muscle_socklen_t size = sizeof(ttl);
   const int fd = sock.GetFileDescriptor();
#ifdef MUSCLE_AVOID_IPV6
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IP,   IP_MULTICAST_TTL,    (sockopt_arg *) &ttl, &size) == 0)&&(size == sizeof(ttl))) ? (uint8)ttl : 0;
#else
   return ((fd>=0)&&(getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (sockopt_arg *) &ttl, &size) == 0)&&(size == sizeof(ttl))) ? (uint8)ttl : 0;
#endif
}

static status_t AddSocketToMulticastGroupIPv4(int fd, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   struct ip_mreq req; memset(&req, 0, sizeof(req));
   req.imr_multiaddr.s_addr = htonl(groupAddress.GetIPv4AddressAsUint32());
   req.imr_interface.s_addr = htonl(localInterfaceAddress.GetIPv4AddressAsUint32());
   return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERRNO;
}

static status_t RemoveSocketFromMulticastGroupIPv4(int fd, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   struct ip_mreq req; memset(&req, 0, sizeof(req));
   req.imr_multiaddr.s_addr = htonl(groupAddress.GetIPv4AddressAsUint32());
   req.imr_interface.s_addr = htonl(localInterfaceAddress.GetIPv4AddressAsUint32());
   return (setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERRNO;
}

status_t SetIPv4SocketMulticastSendInterfaceAddress(const ConstSocketRef & sock, const IPAddress & address)
{
   const int fd = (sock.GetSocketFamily() == SOCKET_FAMILY_IPV4) ? sock.GetFileDescriptor() : -1;
   if (fd < 0) return B_BAD_ARGUMENT;

   struct in_addr localInterface; memset(&localInterface, 0, sizeof(localInterface));
   localInterface.s_addr = htonl(address.GetIPv4AddressAsUint32());
   return (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (const sockopt_arg *) &localInterface, sizeof(localInterface)) == 0) ? B_NO_ERROR : B_ERRNO;
}

IPAddress GetIPv4SocketMulticastSendInterfaceAddress(const ConstSocketRef & sock)
{
   const int fd = (sock.GetSocketFamily() == SOCKET_FAMILY_IPV4) ? sock.GetFileDescriptor() : -1;
   if (fd < 0) return invalidIP;

   struct in_addr localInterface; memset(&localInterface, 0, sizeof(localInterface));
   muscle_socklen_t len = sizeof(localInterface);
   return ((getsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (sockopt_arg *) &localInterface, &len) == 0)&&(len == sizeof(localInterface))) ? IPAddress(ntohl(localInterface.s_addr)) : invalidIP;
}

#ifdef MUSCLE_AVOID_IPV6

// IPv4 multicast implementation

status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   return (sock.GetFileDescriptor() >= 0) ? AddSocketToMulticastGroupIPv4(sock.GetFileDescriptor(), groupAddress, localInterfaceAddress) : B_BAD_ARGUMENT;
}

status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   return (sock.GetFileDescriptor() >= 0) ? RemoveSocketFromMulticastGroupIPv4(sock.GetFileDescriptor(), groupAddress, localInterfaceAddress) : B_BAD_ARGUMENT;
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
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   const int idx = (interfaceIndex == MUSCLE_NO_LIMIT) ? 0 : (int) interfaceIndex;
   return (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (const sockopt_arg *) &idx, sizeof(idx)) == 0) ? B_NO_ERROR : B_ERRNO;
}

int32 GetSocketMulticastSendInterfaceIndex(const ConstSocketRef & sock)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return -1;

   int idx = 0;
   muscle_socklen_t len = sizeof(idx);
   return ((getsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (sockopt_arg *) &idx, &len) == 0)&&(len == sizeof(idx))) ? idx : -1;
}

status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   if (sock.GetSocketFamily() == SOCKET_FAMILY_IPV4) return AddSocketToMulticastGroupIPv4(fd, groupAddress, localInterfaceAddress);
   else
   {
      struct ipv6_mreq req; memset(&req, 0, sizeof(req));
      uint32 interfaceIdx;
      groupAddress.WriteToNetworkArray((uint8*)(&req.ipv6mr_multiaddr), &interfaceIdx);
      req.ipv6mr_interface = interfaceIdx;
      return (setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERRNO;
   }
}

status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress)
{
   const int fd = sock.GetFileDescriptor();
   if (fd < 0) return B_BAD_ARGUMENT;

   if (sock.GetSocketFamily() == SOCKET_FAMILY_IPV4) return RemoveSocketFromMulticastGroupIPv4(fd, groupAddress, localInterfaceAddress);
   else
   {
      struct ipv6_mreq req; memset(&req, 0, sizeof(req));
      uint32 interfaceIdx;
      groupAddress.WriteToNetworkArray((uint8*)(&req.ipv6mr_multiaddr), &interfaceIdx);
      req.ipv6mr_interface = interfaceIdx;
      return (setsockopt(fd, IPPROTO_IPV6, IPV6_DROP_MEMBERSHIP, (const sockopt_arg *) &req, sizeof(req)) == 0) ? B_NO_ERROR : B_ERRNO;
   }
}

#endif  // IPv6 multicast

#endif  // !MUSCLE_AVOID_MULTICAST_API

} // end namespace muscle
