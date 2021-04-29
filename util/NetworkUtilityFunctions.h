/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleNetworkUtilityFunctions_h
#define MuscleNetworkUtilityFunctions_h

#include "support/MuscleSupport.h"
#include "util/CountedObject.h"
#include "util/IHostNameResolver.h"
#include "util/IPAddress.h"
#include "util/NetworkInterfaceInfo.h"  // just for backwards compatibility with old code
#include "util/Queue.h"
#include "util/Socket.h"
#include "util/String.h"
#include "util/TimeUtilityFunctions.h"

// These includes are here so that people can use select() without having to #include the proper
// things themselves all the time.
#ifndef WIN32
# include <sys/types.h>
# include <sys/socket.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
# include <winsock2.h>
# include <ws2tcpip.h>
#endif

#ifdef BONE
# include <sys/select.h>  // sikosis at bebits.com says this is necessary... hmm.
#endif

namespace muscle {

/** @defgroup networkutilityfunctions The NetworkUtilityFunctions function API
 *  These functions are all defined in NetworkUtilityFunctions(.cpp,.h), and are stand-alone
 *  functions that do various network-related tasks
 *  @{
 */

#ifndef MUSCLE_EXPECTED_MTU_SIZE_BYTES
/** The total maximum size of a packet (including all headers and user data) that can be sent
 *  over a network without causing packet fragmentation.  Default value is 1500 (appropriate for
 *  standard Ethernet) but it can be overridden to another value at compile time via e.g. -DMUSCLE_EXPECTED_MTU_SIZE_BYTES=1200
 */
# define MUSCLE_EXPECTED_MTU_SIZE_BYTES 1500
#endif

#ifndef MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES
# define MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES 64 /**< Some extra room, just in case some router or VLAN headers need to be added also */
#endif

#ifdef MUSCLE_AVOID_IPV6
# define MUSCLE_IP_HEADER_SIZE_BYTES 24  /**< Number of bytes in an IPv4 packet header: assumes worst-case scenario (i.e. that the options field is included) */
#else
# define MUSCLE_IP_HEADER_SIZE_BYTES 40  /**< Number of bytes in an IPv6 packet header: assuming no additional header chunks, of course */
#endif

#define MUSCLE_UDP_HEADER_SIZE_BYTES (4*sizeof(uint16))  /**< Number of additional bytes in a UDP sub-header (sourceport, destport, length, checksum) */

#ifndef MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET
/** Total number of application-payload bytes that we can fit into a UDP packet without causing packet fragmentation.  Calculated based on the value of MUSCLE_EXPECTED_MTU_SIZE_BYTES
  * combined with the sizes of the various packet-headers.  Try not to put more than this-many bytes of application data into your UDP packets!
  */
# define MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (MUSCLE_EXPECTED_MTU_SIZE_BYTES-(MUSCLE_IP_HEADER_SIZE_BYTES+MUSCLE_UDP_HEADER_SIZE_BYTES+MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES))
#endif

#ifndef MUSCLE_AVOID_IPV6

/* IPv6 addressing support */

/** This function sets a global flag that indicates whether or not automatic translation of
  * IPv4-compatible IPv6 addresses (e.g. "::192.168.0.1") to IPv4-mapped IPv6 addresses (e.g. "::ffff:192.168.0.1")
  * should be enabled.  This flag is set to true by default; if you want to set it to false you
  * would typically do so only at the top of main() and then not set it again.
  * This automatic remapping is useful if you want your software to handle both IPv4 and IPv6
  * traffic without having to open separate IPv4 and IPv6 sockets and without having to do any 
  * special modification of IPv4 addresses.   The only time you'd need to set this flag to false
  * is if you need to run IPv6 traffic over non-IPv6-aware routers, in which case you'd need
  * to use the "::x.y.z.w" address space for actual IPv6 traffic instead.
  * @param enabled True if the transparent remapping should be enabled (the default state), or false to disable it.
  */
void SetAutomaticIPv4AddressMappingEnabled(bool enabled);

/** Returns true iff automatic translation of IPv4-compatible IPv6 addresses to IPv4-mapped IPv6 address is enabled.
  * @see SetAutomaticIPv4AddressMappingEnabled() for details.
  */
bool GetAutomaticIPv4AddressMappingEnabled();

#endif

/** Given a hostname or IP address string (e.g. "www.google.com" or "192.168.0.1" or "fe80::1"),
  * returns the numeric IPAddress value that corresponds to that name.  This version of the 
  * function will call through to IHostNameResolver objects that were previously registered
  * (via PutHostNameResolver()), and call GetHostByNameNative() if they don't succeed.
  *
  * @param name ASCII IP address or hostname to look up.
  * @param expandLocalhost If true, then if (name) corresponds to 127.0.0.1, this function
  *                        will attempt to determine the host machine's actual primary IP
  *                        address and return that instead.  Otherwise, 127.0.0.1 will be
  *                        returned in this case.  Defaults to false.
  * @param preferIPv6 If set to true, and both IPv4 and IPv6 addresses are returned for the specified
  *                   hostname, then the IPv6 address will be returned.  If false, the IPv4 address
  *                   will be returned.  Defaults to true.
  * @return The associated IP address (local endianness), or invalidIP on failure.
  * @note This function may invoke a synchronous DNS lookup, which means that it may take
  *       a long time to return (e.g. if the DNS server is not responding)
  */
IPAddress GetHostByName(const char * name, bool expandLocalhost = false, bool preferIPv6 = true);

/** This function is the same as GetHostByName(), except that only the built-in name-resolution
  * functionality will be used.  In particular, none of the registered IHostNameResolver callbacks
  * will be called by this version of the function.
  *
  * @param name ASCII IP address or hostname to look up.
  * @param expandLocalhost If true, then if (name) corresponds to 127.0.0.1, this function
  *                        will attempt to determine the host machine's actual primary IP
  *                        address and return that instead.  Otherwise, 127.0.0.1 will be
  *                        returned in this case.  Defaults to false.
  * @param preferIPv6 If set to true, and both IPv4 and IPv6 addresses are returned for the specified
  *                   hostname, then the IPv6 address will be returned.  If false, the IPv4 address
  *                   will be returned.  Defaults to true.
  * @return The associated IP address (local endianness), or invalidIP on failure.
  * @note This function may invoke a synchronous DNS lookup, which means that it may take
  *       a long time to return (e.g. if the DNS server is not responding)
  */
IPAddress GetHostByNameNative(const char * name, bool expandLocalhost = false, bool preferIPv6 = true);

/** Sets the parameters for GetHostByName()'s internal DNS-results LRU cache.
  * Note that this cache is disabled by default, so by default every call to GetHostByName()
  * incurs a call to gethostbyname() or getaddrinfo().  Calling this function with non-zero
  * arguments enables the DNS-results cache, which can be useful if DNS lookups are slow.
  * @param maxCacheSize The maximum number of lookup-result entries that may be stored in
  *                     GetHostByName()'s internal LRU cache at one time.
  * @param expirationTimeMicros How many microseconds a cached lookup-result is valid for.
  *                     Set this to MUSCLE_TIME_NEVER if you want lookup-results to remain
  *                     valid indefinitely.
  */
void SetHostNameCacheSettings(uint32 maxCacheSize, uint64 expirationTimeMicros);

/** If you'd like to have GetHostByName() call your own hostname-resolution algorithm
  * before (or after) the built-in gethostbyname()/getaddrinfo()-based functionality provided by
  * GetHostByNameNative(), you can install a callback routine using this function.
  * @param resolver reference to a IHostNameResolver object whose GetIPAddressForHostName()
  *                 method will be called to try to obtain an IP address for the given hostname.
  * @param priority Priority number, determines the order in which resolvers will be
  *                 called.  Resolvers with priority of 0 or greater will be called
  *                 before calling GetHostByNameNative(); resolvers with priority
  *                 less than zero will be called only if the GetHostByNameNative() doesn't
  *                 return a valid IP address.  Defaults to zero.
  * @returns B_NO_ERROR on success, or B_OUT_OF_MEMORY on failure. 
  */
status_t PutHostNameResolver(const IHostNameResolverRef & resolver, int priority = 0);

/** Removes a IHostNameResolver that had previously been installed via PutHostNameResolver().
  * @param resolver Reference the IHostNameResolver to remove.
  * @returns B_NO_ERROR on success, or B_DATA_NOT_FOUND if the given resolver wasn't currently installed.
  */
status_t RemoveHostNameResolver(const IHostNameResolverRef & resolver);

/** Removes all IHostNameResolvers that were ever installed via PutHostNameResolver() */
void ClearHostNameResolvers();

/** Convenience function for connecting with TCP to a given hostName/port.
 * @param hostName The ASCII host name or ASCII IP address of the computer to connect to.
 * @param port The port number to connect to.
 * @param debugTitle If non-NULL, debug output to stdout will be enabled and debug output will be prefaced by this string.
 * @param debugOutputOnErrorsOnly if true, debug output will be printed only if an error condition occurs.
 * @param maxConnectPeriod The maximum number of microseconds to spend attempting to make this connection.  If left as MUSCLE_TIME_NEVER
 *                         (the default) then no particular time limit will be enforced, and it will be left up to the operating system
 *                         to decide when the attempt should time out.
 * @param expandLocalhost If true, then if (name) corresponds to 127.0.0.1, this function
 *                        will attempt to determine the host machine's actual primary IP
 *                        address and return that instead.  Otherwise, 127.0.0.1 will be
 *                        returned in this case.  Defaults to false.
 * @return A non-NULL ConstSocketRef if the connection is successful, or a NULL ConstSocketRef if the connection failed.
 */
ConstSocketRef Connect(const char * hostName, uint16 port, const char * debugTitle = NULL, bool debugOutputOnErrorsOnly = true, uint64 maxConnectPeriod = MUSCLE_TIME_NEVER, bool expandLocalhost = false);

/** Mostly as above, only with the target IP address specified numerically, rather than as an ASCII string. 
 *  This version of connect will never do a DNS lookup.
 * @param hostIP The numeric host IP address to connect to.
 * @param port The port number to connect to.
 * @param debugHostName If non-NULL, we'll print this host name out when reporting errors.  It isn't used for networking purposes, though.
 * @param debugTitle If non-NULL, debug output to stdout will be enabled and debug output will be prefaced by this string.
 * @param debugOutputOnErrorsOnly if true, debug output will be printed only if an error condition occurs.
 * @param maxConnectPeriod The maximum number of microseconds to spend attempting to make this connection.  If left as MUSCLE_TIME_NEVER
 *                         (the default) then no particular time limit will be enforced, and it will be left up to the operating system
 *                         to decide when the attempt should time out.
 * @return A non-NULL ConstSocketRef if the connection is successful, or a NULL ConstSocketRef if the connection failed.
 */
ConstSocketRef Connect(const IPAddress & hostIP, uint16 port, const char * debugHostName = NULL, const char * debugTitle = NULL, bool debugOutputOnErrorsOnly = true, uint64 maxConnectPeriod = MUSCLE_TIME_NEVER);

/** As above, only this version of Connect() takes an IPAddressAndPort object instead of separate IPAddress and port arguments.
 * @param iap IP address and port to connect to.
 * @param debugHostName If non-NULL, we'll print this host name out when reporting errors.  It isn't used for networking purposes, though.
 * @param debugTitle If non-NULL, debug output to stdout will be enabled and debug output will be prefaced by this string.
 * @param debugOutputOnErrorsOnly if true, debug output will be printed only if an error condition occurs.
 * @param maxConnectPeriod The maximum number of microseconds to spend attempting to make this connection.  If left as MUSCLE_TIME_NEVER
 *                         (the default) then no particular time limit will be enforced, and it will be left up to the operating system
 *                         to decide when the attempt should time out.
 * @return A non-NULL ConstSocketRef if the connection is successful, or a NULL ConstSocketRef if the connection failed.
 */
inline ConstSocketRef Connect(const IPAddressAndPort & iap, const char * debugHostName = NULL, const char * debugTitle = NULL, bool debugOutputOnErrorsOnly = true, uint64 maxConnectPeriod = MUSCLE_TIME_NEVER) {return Connect(iap.GetIPAddress(), iap.GetPort(), debugHostName, debugTitle, debugOutputOnErrorsOnly, maxConnectPeriod);}

/** Accepts an incoming TCP connection on the given listening-socket.
 *  @param sock the listening-socket to accept the incoming TCP from.  This will typically be a ConstSocketRef that was previously returned by CreateAcceptingSocket().
 *  @param optRetLocalInfo if non-NULL, then on success the connecting peer's IP address will be written into
 *                         the IPAddress object that this pointer points to.  Defaults to NULL.
 *  @return A non-NULL ConstSocketRef if the accept was successful, or a NULL ConstSocketRef if the accept failed.
 */
ConstSocketRef Accept(const ConstSocketRef & sock, IPAddress * optRetLocalInfo = NULL);

/** Reads as many bytes as possible from the given socket and places them into (buffer).
 *  @param sock The socket to read from.
 *  @param buffer Location to write the received bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param socketIsInBlockingIOMode Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReceiveData(const ConstSocketRef & sock, void * buffer, uint32 bufferSizeBytes, bool socketIsInBlockingIOMode);

/** Identical to ReceiveData(), except that this function's logic is adjusted to handle UDP semantics properly. 
 *  @param sock The socket to read from.
 *  @param buffer Location to place the received bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param socketIsInBlockingIOMode Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @param optRetFromIP If set to non-NULL, then on success the IPAddress this parameter points to will be filled in
 *                      with the IP address that the received data came from.  Defaults to NULL.
 *  @param optRetFromPort If set to non-NULL, then on success the uint16 this parameter points to will be filled in
 *                      with the source port that the received data came from.  Defaults to NULL.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReceiveDataUDP(const ConstSocketRef & sock, void * buffer, uint32 bufferSizeBytes, bool socketIsInBlockingIOMode, IPAddress * optRetFromIP = NULL, uint16 * optRetFromPort = NULL);

/** Similar to ReceiveData(), except that it will call read() instead of recv().
 *  This is the function to use if (fd) is referencing a file descriptor instead of a socket.
 *  @param fd The file descriptor to read from.
 *  @param buffer Location to place the read bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param fdIsInBlockingIOMode Pass in true if the given fd is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReadData(const ConstSocketRef & fd, void * buffer, uint32 bufferSizeBytes, bool fdIsInBlockingIOMode);

/** Transmits as many bytes as possible from the given buffer over the given socket.
 *  @param sock The socket to transmit over.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to send.
 *  @param socketIsInBlockingIOMode Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 SendData(const ConstSocketRef & sock, const void * buffer, uint32 bufferSizeBytes, bool socketIsInBlockingIOMode);

/** Similar to SendData(), except that this function's logic is adjusted to handle UDP semantics properly.
 *  @param sock The socket to transmit over.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to send.
 *  @param socketIsInBlockingIOMode Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @param optDestIP If set to a valid IP address, the data will be sent to that IP address.  Otherwise it will
 *                   be sent to the socket's current IP address (see SetUDPSocketTarget()).  Defaults to invalidIP.
 *  @param destPort If set to non-zero, the data will be sent to the specified port.  Otherwise it will
 *                  be sent to the socket's current destination port (see SetUDPSocketTarget()).  Defaults to zero.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 SendDataUDP(const ConstSocketRef & sock, const void * buffer, uint32 bufferSizeBytes, bool socketIsInBlockingIOMode, const IPAddress & optDestIP = invalidIP, uint16 destPort = 0);

/** Similar to SendData(), except that the implementation calls write() instead of send().  This
 *  is the function to use when (fd) refers to a file descriptor instead of a socket.
 *  @param fd The file descriptor to write the data to.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to write.
 *  @param fdIsInBlockingIOMode Pass in true if the given file descriptor is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 WriteData(const ConstSocketRef & fd, const void * buffer, uint32 bufferSizeBytes, bool fdIsInBlockingIOMode);

/** This function initiates a non-blocking connection to the given host IP address and port.
  * It will return the created socket, which may or may not be fully connected yet.
  * If it is connected, (retIsReady) will be to true, otherwise it will be set to false.
  * If (retIsReady) is false, then you can use select() to find out when the state of the
  * socket has changed:  select() will return ready-to-write on the socket when it is 
  * fully connected (or when the connection fails), at which point you can call 
  * FinalizeAsyncConnect() on the socket:  if FinalizeAsyncConnect() succeeds, the connection 
  * succeeded; if not, the connection failed.
  * @param hostIP 32-bit IP address to connect to (hostname isn't used as hostname lookups can't be made asynchronous AFAIK)
  * @param port Port to connect to.
  * @param retIsReady On success, this bool is set to true iff the socket is ready to use, or 
  *                   false to indicate that an asynchronous connection is now in progress.
  * @return A non-NULL ConstSocketRef (which is likely still in the process of connecting) on success, or a NULL ConstSocketRef if the accept failed.
  */
ConstSocketRef ConnectAsync(const IPAddress & hostIP, uint16 port, bool & retIsReady);

/** As above, only this version of ConnectAsync() takes an IPAddressAndPort object instead of separate IP address and port arguments.
  * @param iap IP address and port to connect to.
  * @param retIsReady On success, this bool is set to true iff the socket is ready to use, or 
  *                   false to indicate that an asynchronous connection is now in progress.
  * @return A non-NULL ConstSocketRef (which is likely still in the process of connecting) on success, or a NULL ConstSocketRef if the accept failed.
  */
inline ConstSocketRef ConnectAsync(const IPAddressAndPort & iap, bool & retIsReady) {return ConnectAsync(iap.GetIPAddress(), iap.GetPort(), retIsReady);}

/** When a TCP socket that was connecting asynchronously finally selects as ready-for-write 
  * to indicate that the asynchronous connect attempt has reached a conclusion, call this function.
  * This function will finalize the asynchronous-TCP-connection-process, and make the TCP socket 
  * usable for data-transfer.
  * @param sock The socket that was connecting asynchronously
  * @returns B_NO_ERROR if the connection is ready to use, or an error code if the connect failed.
  * @note Under Windows, select() won't return ready-for-write if the asynchronous TCP connection fails... 
  *       instead it will select-as-ready for your socket on the exceptions-fd_set (if you provided one).
  *       Once this happens, there is no need to call FinalizeAsyncConnect() -- the fact that the
  *       socket notified on the exceptions fd_set is enough for you to know the asynchronous connection
  *       failed.  Successful asynchronous-TCP-connects do exhibit the standard/select-as-ready-for-write
  *       behaviour under Windows, though.
  */
status_t FinalizeAsyncConnect(const ConstSocketRef & sock);

/** Shuts the given socket down.  (Note that you don't generally need to call this function; it's generally
 *  only useful if you need to half-shutdown the socket, e.g. stop the output direction but not the input
 *  direction)
 *  @param sock The socket to permanently shut down communication on. 
 *  @param disableReception If true, further reception of data will be disabled on this socket.  Defaults to true.
 *  @param disableTransmission If true, further transmission of data will be disabled on this socket.  Defaults to true.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t ShutdownSocket(const ConstSocketRef & sock, bool disableReception = true, bool disableTransmission = true);

/** Creates a TCP listening-socket, that listens for incoming TCP connections on a specified TCP port.
 *  @param port Which TCP port to listen on, or 0 to have the system should choose an available TCP port for you.
 *  @param maxbacklog Maximum TCP-connection-backlog to allow (defaults to 20)
 *  @param optRetPort If non-NULL, the uint16 this value points to will be set to the actual port bound to (useful when you allowed the system to choose a port for you)
 *  @param optInterfaceIP Optional IP address of the local network interface to listen on.  If left unspecified, or
 *                        if passed in as (invalidIP), then this socket will listen on all available network interfaces.
 *  @return A non-NULL ConstSocketRef if the port was bound successfully, or a NULL ConstSocketRef if the accept failed.
 */
ConstSocketRef CreateAcceptingSocket(uint16 port, int maxbacklog = 20, uint16 * optRetPort = NULL, const IPAddress & optInterfaceIP = invalidIP);

/** Translates the given 4-byte IP address into a string representation.
 *  @param address The 4-byte IP address to translate into text.
 *  @param outBuf Buffer where the NUL-terminated ASCII representation of the string will be placed.
 *                This buffer must be at least 64 bytes long (or at least 16 bytes long if MUSCLE_AVOID_IPV6 is defined)
 *  @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
 *                         Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
 */
void Inet_NtoA(const IPAddress & address, char * outBuf, bool preferIPv4Style = false);

/** A more convenient version of INet_NtoA().  Given an IP address, returns a human-readable String representation of that address.
  *  @param ipAddress the IP address to return in String form.
  *  @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
  *                         Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
  */
String Inet_NtoA(const IPAddress & ipAddress, bool preferIPv4Style = false);

/** Returns true iff (s) is a well-formed IP address (e.g. "192.168.0.1" or "ff12::888" or etc)
  * @param s An ASCII string to check the formatting of
  * @returns true iff the given string represents an IP address; false if it doesn't.
  */
bool IsIPAddress(const char * s);

/** Given an IP address in ASCII format (e.g. "192.168.0.1" or "ff12::888" or etc), returns
  * the equivalent IP address in IPAddress (packed binary) form. 
  * @param buf numeric IP address in ASCII.
  * @returns IP address as a IPAddress, or invalidIP on failure.
  */
IPAddress Inet_AtoN(const char * buf);

/** Returns a string that is the local host's primary host name. */
String GetLocalHostName();

/** Reurns the IP address that the given socket is connected to.
 *  @param sock The socket to find out info about.
 *  @param expandLocalhost If true, then if the peer's ip address is reported as 127.0.0.1, this 
 *                         function will attempt to determine the host machine's actual primary IP
 *                         address and return that instead.  Otherwise, 127.0.0.1 will be
 *                         returned in this case.
 *  @param optRetPort if non-NULL, the port we are connected to on the remote peer will be written here.  Defaults to NULL.
 *  @return The IP address on success, or invalidIP on failure (such as if the socket isn't valid and connected).
 */
IPAddress GetPeerIPAddress(const ConstSocketRef & sock, bool expandLocalhost, uint16 * optRetPort = NULL);

/** Creates a pair of stream-oriented sockets that are connected to each other, so that any bytes 
 *  you write into one socket come out as bytes to read from the other socket.
 *  This is particularly useful for inter-thread communication and/or signalling.
 *  @param retSocket1 On success, this value will be set to the socket ID of the first socket.
 *  @param retSocket2 On success, this value will be set to the socket ID of the second socket.
 *  @param blocking Whether the two sockets should use blocking I/O or not.  Defaults to false.
 *  @return B_NO_ERROR on success, or an error code on failure.
 *  @note the Windows implementation of this function will return a pair of connected TCP sockets.
 *        On other OS's, an AF_UNIX socketpair() is returned.  The resulting behavior is the same in either case.
 */
status_t CreateConnectedSocketPair(ConstSocketRef & retSocket1, ConstSocketRef & retSocket2, bool blocking = false);

/** Enables or disables blocking I/O on the given socket.
 *  @note The default state for a newly created socket is: true/blocking-mode-enabled.
 *  @param sock the socket to act on.
 *  @param enabled True to set this socket to blocking I/O mode, or false to set it to non-blocking I/O mode.
 *  @return B_NO_ERROR on success, or an error code on failure.
 */
status_t SetSocketBlockingEnabled(const ConstSocketRef & sock, bool enabled);

/** Returns true iff the given socket has blocking I/O enabled.
 *  @param sock the socket to query.
 *  @returns true iff the socket is in blocking I/O mode, or false if it is not (or if it is an invalid socket)
 *  @note this function is not implemented under Windows (as Windows doesn't appear to provide any method to
 *        obtain this information from a socket).  Under Windows, this method will simply log an error message
 *        and return false.
 */ 
bool GetSocketBlockingEnabled(const ConstSocketRef & sock);

/**
  * Turns Nagle's algorithm (i.e. output-packet-buffering/coalescing, with a 200mS timeout) on or off.
  * @param sock the socket to act on.
  * @param enabled If true, outgoing TCP data will be held briefly (per Nagle's algorithm) before sending, 
  *                to allow for fewer, bigger packets.  If false, then each SendData() call will cause a 
  *                new TCP packet to be sent immediately.
  * @return B_NO_ERROR on success, an error code on error.
  */
status_t SetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock, bool enabled);

/** Returns true iff the given socket has Nagle's algorithm enabled.
 *  @param sock the socket to query.
 *  @returns true iff the socket is has Nagle's algorithm enabled, or false if it does not (or if it is an invalid socket)
 */ 
bool GetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock);

/**
  * Enables or disables the TCP_CORK algorithm (TCP_NOPUSH under BSD-based OS's).
  * With the algorithm enabled, only full TCP packets will be sent.
  * @param sock the socket to act on.
  * @param enabled If true, partial outgoing TCP packets will be held internally until the 
  *                TCP_CORK/TCP_NOPUSH algorithm has been disabled on this socket.  Full 
  *                packets will still be sent ASAP.
  * @note that this function is currently implemented only under MacOS/X, BSD, and Linux; on 
  *       other OS's it will return B_UNIMPLEMENTED.
  * @note that MacOS/X's implementation of TCP_NOPUSH is a bit buggy -- as of 10.15.7,
  *       disabling TCP_NOPUSH doesn't cause any of the socket's pending TCP data to be
  *       sent ASAP, and (worse) using TCP_NOPUSH seems to sometimes cause a 5 second
  *       delay in data being sent, even after TCP_NOPUSH has been disabled.  Until those
  *       problems are fixed, you might want to avoid calling this function under MacOS/X.
  * @return B_NO_ERROR on success, an error code on error.
  */
status_t SetSocketCorkAlgorithmEnabled(const ConstSocketRef & sock, bool enabled);

/** Returns true iff the given socket has the TCP_CORK/TCP_NOPUSH algorithm enabled.
 *  @param sock the socket to query.
 *  @returns true iff the socket is corked, or false if it is not corked.
 */ 
bool GetSocketCorkAlgorithmEnabled(const ConstSocketRef & sock);

/**
  * Sets the size of the given socket's outgoing-data-buffer to the specified
  * size (or as close to that size as is possible).
  * @param sock the socket to act on.
  * @param sendBufferSizeBytes New requested size for the outgoing-data-buffer, in bytes.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t SetSocketSendBufferSize(const ConstSocketRef & sock, uint32 sendBufferSizeBytes);

/** 
  * Returns the current size of the socket's outgoing-data-buffer.
  * @param sock The socket to query.
  * @return The current size of the socket's outgoing-data-buffer, in bytes, or a negative value on error (e.g. invalid socket)
  */
int32 GetSocketSendBufferSize(const ConstSocketRef & sock);

/**
  * Sets the size of the given socket's incoming-data-buffer to the specified
  * size (or as close to that size as is possible).
  * @param sock the socket to act on.
  * @param receiveBufferSizeBytes New size of the incoming-data-buffer, in bytes.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t SetSocketReceiveBufferSize(const ConstSocketRef & sock, uint32 receiveBufferSizeBytes);

/** 
  * Returns the current size of the socket's incoming-data-buffer.
  * @param sock The socket to query.
  * @return The size of the socket's incoming-data-buffer, in bytes, or a negative value on error (e.g. invalid socket)
  */
int32 GetSocketReceiveBufferSize(const ConstSocketRef & sock);

/** This class is an interface to an object that can have its SocketCallback() method called
  * at the appropriate times when certain actions are performed on a socket.  By installing
  * a GlobalSocketCallback object via SetGlobalSocketCallback(), behaviors can be set for all 
  * sockets in the process, which can be simpler than changing every individual code path
  * to install those behaviors on a per-socket basis.
  */ 
class GlobalSocketCallback
{
public:
   /** Default constructor */
   GlobalSocketCallback() {/* empty */}

   /** Destructor */
   virtual ~GlobalSocketCallback() {/* empty */}

   /** Called by certain functions in NetworkUtilityFunctions.cpp
     * @note this method may be called by different threads, so it needs to be thread-safe.
     * @param eventType a SOCKET_CALLBACK_* value indicating what caused this callback call.
     * @param sock The socket in question
     * @return B_NO_ERROR on success, or an error code if the calling operation should be aborted.
     */
   virtual status_t SocketCallback(uint32 eventType, const ConstSocketRef & sock) = 0;

   /** Enumeration of the types of SocketCallback event we support */
   enum {
      SOCKET_CALLBACK_CREATE_UDP = 0,   /**< socket was just created by CreateUDPSocket() */
      SOCKET_CALLBACK_CREATE_ACCEPTING, /**< socket was just created by CreateAcceptingSocket() */
      SOCKET_CALLBACK_ACCEPT,           /**< socket was just created by Accept() */
      SOCKET_CALLBACK_CONNECT,          /**< socket was just created by Connect() or ConnectAsync() */
      NUM_SOCKET_CALLBACKS              /**< guard value */
   };

private:
   DECLARE_COUNTED_OBJECT(GlobalSocketCallback);
};

/** Set the global socket-callback object for this process.
  * @param cb The object whose SocketCallback() method should be called by Connect(),
  *           Accept(), FinalizeAsyncConnect(), etc, or NULL to have no more callbacks
  */ 
void SetGlobalSocketCallback(GlobalSocketCallback * cb);

/** Returns the currently installed GlobalSocketCallback object, or NULL if there is none installed. */
GlobalSocketCallback * GetGlobalSocketCallback();

#ifndef MUSCLE_DISABLE_KEEPALIVE_API

/**
  * This function modifies the TCP keep-alive behavior for the given TCP socket -- that is, you can use this
  * function to control how often the TCP socket checks to see if the remote peer is still accessible when the
  * TCP connection is idle.  If this function is never called, the default TCP behavior is to never check the
  * remote peer when the connection is idle -- it will only check when there is data buffered up to send to the
  * remote peer.
  * @param sock The TCP socket to adjust the keepalive behavior of.
  * @param maxProbeCount The number of keepalive-ping probes that must go unanswered before the TCP connection is closed.
  *                      Passing zero to this argument will disable keepalive-ping behavior.
  * @param idleTime The amount of time (in microseconds) of inactivity on the TCP socket that must pass before the
  *                 first keepalive-ping probe is sent.  Note that the granularity of the timeout is determined by  
  *                 the operating system, so the actual timeout period may be somewhat more or less than the specified number
  *                 of microseconds.  (Currently it gets rounded up to the nearest second)
  * @param retransmitTime The amount of time (in microseconds) of further inactivity on the TCP socket that must pass before the
  *                       next keepalive-ping probe is sent.  Note that the granularity of the timeout is determined by  
  *                       the operating system, so the actual timeout period may be somewhat more or less than the specified number
  *                       of microseconds.  (Currently it gets rounded up to the nearest second)
  * @returns B_NO_ERROR on success, or an error code on failure.
  * @note This function is currently implemented only on Linux; on other OS's it will always just return B_UNIMPLEMENTED.
  */
status_t SetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 maxProbeCount, uint64 idleTime, uint64 retransmitTime);

/**
  * Queries the values previously set on the socket by SetSocketKeepAliveBehavior().
  * @param sock The TCP socket to return the keepalive behavior of.
  * @param retMaxProbeCount if non-NULL, the max-probe-count value of the socket will be written into this argument.
  * @param retIdleTime if non-NULL, the idle-time of the socket will be written into this argument.
  * @param retRetransmitTime if non-NULL, the transmit-time of the socket will be written into this argument.
  * @returns B_NO_ERROR on success, or an error code on failure.
  * @note This function is currently implemented only on Linux; on other OS's it will always just return B_UNIMPLEMENTED.
  * @see SetSocketKeepAliveBehavior()
  */ 
status_t GetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 * retMaxProbeCount, uint64 * retIdleTime, uint64 * retRetransmitTime);

#endif

/** Set a user-specified IP address to return from GetHostByName() and GetPeerIPAddress() instead of 127.0.0.1.
  * Note that this function <b>does not</b> change the computer's IP address -- it merely changes what
  * the aforementioned functions will report.
  * @param ip New IP address to return instead of 127.0.0.1, or invalidIP to disable this override.
  */
void SetLocalHostIPOverride(const IPAddress & ip);

/** Returns the user-specified IP address that was previously set by SetLocalHostIPOverride(), or 0
  * if none was set.  Note that this function <b>does not</b> report the local computer's IP address,
  * unless you previously called SetLocalHostIPOverride() with that address.
  */
IPAddress GetLocalHostIPOverride();

/** Creates and returns a socket that can be used for UDP communications.
 *  Returns a negative value on error, or a non-negative socket handle on
 *  success.  You'll probably want to call BindUDPSocket() or SetUDPSocketTarget()
 *  after calling this method.
 */
ConstSocketRef CreateUDPSocket();

/** Attempts to given UDP socket to the given port.  
 *  @param sock The UDP socket (as previously returned by CreateUDPSocket())
 *  @param port UDP port to bind the socket to.  If zero, the system will choose an available UDP port for you.
 *  @param optRetPort if non-NULL, then on successful return the uint16 that this pointer points to will contain
 *                    the port ID that the socket was bound to.  Defaults to NULL.
 *  @param optFrom If a valid IPAddress, then the socket will be bound in such a way that only data
 *                 packets addressed to the specified IP address will be accepted.  Defaults to 
 *                 invalidIP, meaning that packets addressed to any of this machine's IP addresses 
 *                 will be accepted.
 *  @param allowShared If set to true, the port will be set up so that multiple processes
 *                     can bind to it simultaneously.  This is useful for sockets that are 
 *                     to be receiving multicast or broadcast UDP packets, since then you can 
 *                     run multiple UDP multicast or broadcast-receiving processes on a single computer. 
 *  @returns B_NO_ERROR on success, or an error code on failure.
 */
status_t BindUDPSocket(const ConstSocketRef & sock, uint16 port, uint16 * optRetPort = NULL, const IPAddress & optFrom = invalidIP, bool allowShared = false);

/** Set the target/destination address for a UDP socket.  After successful return
 *  of this function, any data that is written to the UDP socket (with no explicit
 *  destination address specified) will be sent to this IP address and port.  
 *  Also, the UDP socket will only receive UDP packets from the specified address/port.
 *  @param sock The UDP socket to send to (previously created by CreateUDPSocket()).
 *  @param remoteIP Remote IP address that data should be sent to.
 *  @param remotePort Remote UDP port ID that data should be sent to.
 *  @returns B_NO_ERROR on success, or an error code on failure.
 */
status_t SetUDPSocketTarget(const ConstSocketRef & sock, const IPAddress & remoteIP, uint16 remotePort);

/** As above, except that the remote host is specified by hostname instead of IP address.
 *  Note that this function may take involve a DNS lookup, and so may take a significant
 *  amount of time to complete.
 *  @param sock The UDP socket to send to (previously created by CreateUDPSocket()).
 *  @param remoteHostName Name of remote host (e.g. "www.mycomputer.com" or "132.239.50.8")
 *  @param remotePort Remote UDP port ID that data should be sent to.
  * @param expandLocalhost If true, then if (name) corresponds to 127.0.0.1, this function
  *                        will attempt to determine the host machine's actual primary IP
  *                        address and return that instead.  Otherwise, 127.0.0.1 will be
  *                        returned in this case.  Defaults to false.
 *  @returns B_NO_ERROR on success, or an error code on failure.
 */
status_t SetUDPSocketTarget(const ConstSocketRef & sock, const char * remoteHostName, uint16 remotePort, bool expandLocalhost = false);

/** Enable/disable sending of broadcast packets on the given UDP socket.
 *  @param sock UDP socket to enable or disable the sending of broadcast UDP packets with.
 *              (Note that the default state of newly created UDP sockets is broadcast-disabled)
 *  @param broadcast True if broadcasting should be enabled, false if broadcasting should be disabled.
 *  @returns B_NO_ERROR on success, or an error code on failure.
 *  @note this function is only useful in conjunction with IPv4 sockets.  IPv6 doesn't use broadcast anyway.
 */
status_t SetUDPSocketBroadcastEnabled(const ConstSocketRef & sock, bool broadcast);

/** Returns true iff the specified UDP socket has broadcast enabled; or false if it does not.
  * @param sock The UDP socket to query.
  * @returns true iff the socket is enabled for UDP broadcast; false otherwise.
  * @note this function is only useful in conjunction with IPv4 sockets.  IPv6 doesn't use broadcast anyway.
  */
bool GetUDPSocketBroadcastEnabled(const ConstSocketRef & sock);

#ifndef MUSCLE_AVOID_MULTICAST_API

/** Sets whether multicast data sent on this socket should be received by
  * sockets on the local host machine, or not (IP_MULTICAST_LOOP).  
  * Default state is enabled/true.
  * @param sock The socket to set the state of the IP_MULTICAST_LOOP flag on.
  * @param multicastToSelf If true, enable loopback.  Otherwise, disable it.
  * @returns B_NO_ERROR on success, or an error code on failure.
  * @note If this option is set to false, the filtering is done at the IP-address
  *       level rather than at the per-socket level, which means that other sockets
  *       subscribed to the same multicast-group on the same host will not receive
  *       the filtered multicast packets either.  That limits the usefulness of
  *       this function for applications that need to support multiple multicast-using
  *       processes running together on on the same host.
  */
status_t SetSocketMulticastToSelf(const ConstSocketRef & sock, bool multicastToSelf);

/** Returns true iff multicast packets sent by this socket should be received by sockets on
  * the local host machine.
  * Default state is enabled.
  * @param sock The socket to query
  * @returns true iff this socket has multicast-to-self (IP_MULTICAST_LOOP) enabled.
  */
bool GetSocketMulticastToSelf(const ConstSocketRef & sock);

/** Set the "time to live" flag for packets sent by this socket.
  * Default state is 1, i.e. "local LAN segment only".
  * Other possible values include 0 ("localhost only"), 2-31 ("local site only"),
  * 32-63 ("local region only"), 64-127 ("local continent only"), or 128-255 ("global").
  * @param sock The socket to set the TTL value for
  * @param ttl The ttl value to set (see above).
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t SetSocketMulticastTimeToLive(const ConstSocketRef & sock, uint8 ttl);

/** Returns the "time to live" associated with multicast packets sent on this socket, or 0 on failure.
  * @param sock The socket to query the TTL value of.
  */
uint8 GetSocketMulticastTimeToLive(const ConstSocketRef & sock);

#ifdef MUSCLE_AVOID_IPV6

// IPv4 multicast

/** Specify the address of the local interface that the given socket should
  * send multicast packets on.  If this isn't called, the kernel will try to choose
  * an appropriate default interface to send on.
  * @param sock The socket to set the sending interface for.
  * @param address The address of the local interface to send multicast packets on.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t SetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock, const IPAddress & address);

/** Returns the address of the local interface that the given socket will
  * try to send multicast packets on, or invalidIP on failure.
  * @param sock The socket to query the sending interface of.
  * @returns the interface's IP address, or invalidIP on error.
  */
IPAddress GetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock);

/** Attempts to add the specified socket to the specified multicast group.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @param localInterfaceAddress Optional IP address of the local interface use for receiving
  *                              data from this group.  If left as (invalidIP), an appropriate
  *                              interface will be chosen automatically.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress = invalidIP);

/** Attempts to remove the specified socket from the specified multicast group
  * that it was previously added to.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @param localInterfaceAddress Optional IP address of the local interface used for receiving
  *                              data from this group.  If left as (invalidIP), the first matching
  *                              group will be removed.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress, const IPAddress & localInterfaceAddress = invalidIP);

#else  // end IPv4 multicast, begin IPv6 multicast

/** Specify the interface index of the local network interface that the given socket should
  * send multicast packets on.  If this isn't called, the kernel will choose an appropriate
  * default interface to send multicast packets over.
  * @param sock The socket to set the sending interface for.
  * @param interfaceIndex Optional index of the interface to send the multicast packets on.
  *                       Zero means the default interface, larger values mean another interface.
  *                       You can call GetNetworkInterfaceInfos() to get the list of available interfaces.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t SetSocketMulticastSendInterfaceIndex(const ConstSocketRef & sock, uint32 interfaceIndex);

/** Returns the index of the local interface that the given socket will
  * try to send multicast packets on, or -1 on failure.
  * @param sock The socket to query the sending interface index of.
  * @returns the socket's interface index, or -1 on error.
  */
int32 GetSocketMulticastSendInterfaceIndex(const ConstSocketRef & sock);

/** Attempts to add the specified socket to the specified multicast group.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @note Under Windows this call will fail unless the socket has already
  *       been bound to a port (e.g. with BindUDPSocket()).  Other OS's don't
  *       seem to have that requirement.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress);

/** Attempts to remove the specified socket from the specified multicast group that it was previously added to.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @returns B_NO_ERROR on success, or an error code on failure.
  */
status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const IPAddress & groupAddress);

#endif  // end IPv6 multicast

#endif  // !MUSCLE_AVOID_MULTICAST_API

/// @cond HIDDEN_SYMBOLS

// Different OS's use different types for pass-by-reference in accept(), etc.
// So I define my own muscle_socklen_t to avoid having to #ifdef all my code
#if defined(__amd64__) || defined(__aarch64__) || defined(__FreeBSD__) || defined(BSD) || defined(__PPC64__) || defined(__HAIKU__) || defined(ANDROID) || defined(_SOCKLEN_T) || defined(__EMSCRIPTEN__)
typedef socklen_t muscle_socklen_t;
#elif defined(__BEOS__) || defined(__HAIKU__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(WIN32) || defined(__QNX__) || defined(__osf__)
typedef int muscle_socklen_t;
#else
typedef size_t muscle_socklen_t;
#endif

static inline long recv_ignore_eintr(    int s, void *b, unsigned long numBytes, int flags) {long ret; do {ret = recv(s, (char *)b, numBytes, flags);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long recvfrom_ignore_eintr(int s, void *b, unsigned long numBytes, int flags, struct sockaddr *a, muscle_socklen_t * alen) {long ret; do {ret = recvfrom(s, (char *)b, numBytes, flags, a,  alen);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long send_ignore_eintr(    int s, const void *b, unsigned long numBytes, int flags) {long ret; do {ret = send(s, (char *)b, numBytes, flags);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long sendto_ignore_eintr(  int s, const void *b, unsigned long numBytes, int flags, const struct sockaddr * d, int dlen) {long ret; do {ret = sendto(s, (char *)b, numBytes, flags,  d, dlen);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
#ifndef WIN32
static inline long read_ignore_eintr(    int f, void *b, unsigned long nbyte) {long ret; do {ret = read(f, (char *)b, nbyte);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long write_ignore_eintr(   int f, const void *b, unsigned long nbyte) {long ret; do {ret = write(f, (char *)b, nbyte);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
#endif

/// @endcond

/** @} */ // end of networkutilityfunctions doxygen group

} // end namespace muscle

#endif

