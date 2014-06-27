/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleNetworkUtilityFunctions_h
#define MuscleNetworkUtilityFunctions_h

#include "support/MuscleSupport.h"
#include "util/CountedObject.h"
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

#ifdef BONE
# include <sys/select.h>  // sikosis at bebits.com says this is necessary... hmm.
#endif

namespace muscle {

/** @defgroup networkutilityfunctions The NetworkUtilityFunctions function API
 *  These functions are all defined in NetworkUtilityFunctions(.cpp,.h), and are stand-alone
 *  functions that do various network-related tasks
 *  @{
 */

// The total maximum size of a packet (including all headers and user data) that can be sent
// over a network without causing packet fragmentation
#ifndef MUSCLE_EXPECTED_MTU_SIZE_BYTES
# define MUSCLE_EXPECTED_MTU_SIZE_BYTES 1500  // Default to the MTU that good old Ethernet uses
#endif

// Some extra room, just in case some router or VLAN headers need to be added also
#ifndef MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES
# define MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES 64
#endif

#ifdef MUSCLE_AVOID_IPV6
# define MUSCLE_IP_HEADER_SIZE_BYTES 24  // IPv4: assumes worst-case scenario (i.e. that the options field is included)
#else
# define MUSCLE_IP_HEADER_SIZE_BYTES 40  // IPv6: assuming no additional header chunks, of course
#endif

#define MUSCLE_UDP_HEADER_SIZE_BYTES (4*sizeof(uint16))  // (sourceport, destport, length, checksum)

#ifndef MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET
# define MUSCLE_MAX_PAYLOAD_BYTES_PER_UDP_ETHERNET_PACKET (MUSCLE_EXPECTED_MTU_SIZE_BYTES-(MUSCLE_IP_HEADER_SIZE_BYTES+MUSCLE_UDP_HEADER_SIZE_BYTES+MUSCLE_POTENTIAL_EXTRA_HEADERS_SIZE_BYTES))
#endif

#ifdef MUSCLE_AVOID_IPV6

/** IPv4 addressing support */
typedef uint32 ip_address;

/** Given an IP address, returns a 32-bit hash code for it.  IPv4 implementation. */
inline uint32 GetHashCodeForIPAddress(const ip_address & ip) {return CalculateHashCode(ip);}

/** IPv4 Numeric representation of a all-zeroes invalid/guard address */
const ip_address invalidIP = 0;

/** IPv4 Numeric representation of localhost (127.0.0.1), for convenience */
const ip_address localhostIP = ((((uint32)127)<<24)|((uint32)1));

/** IPv4 Numeric representation of broadcast (255.255.255.255), for convenience */
const ip_address broadcastIP = ((uint32)-1);

#else

/* IPv6 addressing support */

/** This function sets a global flag that indicates whether or not automatic translation of
  * IPv4-compatible IPv6 addresses (e.g. ::192.168.0.1) to IPv4-mapped IPv6 addresses (e.g. ::ffff:192.168.0.1)
  * should be enabled.  This flag is set to true by default; if you want to set it to false you
  * would typically do so only at the top of main() and then not set it again.
  * This automatic remapping is useful if you want your software to handle both IPv4 and IPv6
  * traffic without having to open separate IPv4 and IPv6 sockets and without having to do any 
  * special modification of IPv4 addresses.   The only time you'd need to set this flag to false
  * is if you need to run IPv6 traffic over non-IPv6-aware routers, in which case you'd need
  * to use the ::x.y.z.w address space for actual IPv6 traffic instead.
  * @param enabled True if the transparent remapping should be enabled (the default state), or false to disable it.
  */
void SetAutomaticIPv4AddressMappingEnabled(bool enabled);

/** Returns true iff automatic translation of IPv4-compatible IPv6 addresses to IPv4-mapped IPv6 address is enabled.
  * @see SetAutomaticIPv4AddressMappingEnabled() for details.
  */
bool GetAutomaticIPv4AddressMappingEnabled();

/** This class represents an IPv6 network address, including the 128-bit IP
  * address and the interface index field (necessary for connecting to link-local addresses)
  */
class ip_address
{
public:
   ip_address(uint64 lowBits = 0, uint64 highBits = 0, uint32 interfaceIndex = 0) : _lowBits(lowBits), _highBits(highBits), _interfaceIndex(interfaceIndex) {/* empty */}
   ip_address(const ip_address & rhs) : _lowBits(rhs._lowBits), _highBits(rhs._highBits), _interfaceIndex(rhs._interfaceIndex) {/* empty */}

   ip_address & operator = (const ip_address & rhs) {_lowBits = rhs._lowBits; _highBits = rhs._highBits; _interfaceIndex = rhs._interfaceIndex; return *this;}

   bool EqualsIgnoreInterfaceIndex(const ip_address & rhs) const {return ((_lowBits == rhs._lowBits)&&(_highBits == rhs._highBits));}
   bool operator ==               (const ip_address & rhs) const {return ((EqualsIgnoreInterfaceIndex(rhs))&&(_interfaceIndex == rhs._interfaceIndex));}
   bool operator !=               (const ip_address & rhs) const {return !(*this == rhs);}

   bool operator < (const ip_address & rhs) const 
   {
      if (_highBits < rhs._highBits) return true;
      if (_highBits > rhs._highBits) return false;
      if (_lowBits  < rhs._lowBits)  return true;
      if (_lowBits  > rhs._lowBits)  return false;
      return (_interfaceIndex < rhs._interfaceIndex);
   }

   bool operator > (const ip_address & rhs) const 
   {
      if (_highBits < rhs._highBits) return false;
      if (_highBits > rhs._highBits) return true;
      if (_lowBits  < rhs._lowBits)  return false;
      if (_lowBits  > rhs._lowBits)  return true;
      return (_interfaceIndex > rhs._interfaceIndex);
   }

   bool operator <= (const ip_address & rhs) const {return (*this == rhs)||(*this < rhs);}
   bool operator >= (const ip_address & rhs) const {return (*this == rhs)||(*this > rhs);}

   ip_address operator & (const ip_address & rhs) {return ip_address(_lowBits & rhs._lowBits, _highBits & rhs._highBits, _interfaceIndex);}
   ip_address operator | (const ip_address & rhs) {return ip_address(_lowBits | rhs._lowBits, _highBits | rhs._highBits, _interfaceIndex);}
   ip_address operator ~ () {return ip_address(~_lowBits, ~_highBits, _interfaceIndex);}

   ip_address operator &= (const ip_address & rhs) {*this = *this & rhs; return *this;}
   ip_address operator |= (const ip_address & rhs) {*this = *this | rhs; return *this;}

   void SetBits(uint64 lowBits, uint64 highBits) {_lowBits = lowBits; _highBits = highBits;}

   uint64 GetLowBits()  const {return _lowBits;}
   uint64 GetHighBits() const {return _highBits;}

   void SetLowBits( uint64 lb) {_lowBits  = lb;}
   void SetHighBits(uint64 hb) {_highBits = hb;}

   void SetInterfaceIndex(uint32 iidx) {_interfaceIndex = iidx;}
   uint32 GetInterfaceIndex() const    {return _interfaceIndex;}

   uint32 HashCode() const {return CalculateHashCode(_interfaceIndex)+CalculateHashCode(_lowBits)+CalculateHashCode(_highBits);}

   /** Writes our address into the specified uint8 array, in the required network-friendly order.
     * @param networkBuf If non-NULL, the 16-byte network-array to write to.  Typically you would pass in 
     *                   mySockAddr_in6.sin6_addr.s6_addr as the argument to this function.
     * @param optInterfaceIndex If non-NULL, this value will receive a copy of our interface index.  Typically
     *                          you would pass a pointer to mySockAddr_in6.sin6_addr.sin6_scope_id here.
     */
   void WriteToNetworkArray(uint8 * networkBuf, uint32 * optInterfaceIndex) const
   {
      if (networkBuf)
      {
         WriteToNetworkArrayAux(&networkBuf[0], _highBits);
         WriteToNetworkArrayAux(&networkBuf[8], _lowBits);
      }
      if (optInterfaceIndex) *optInterfaceIndex = _interfaceIndex;
   }

   /** Reads our address in from the specified uint8 array, in the required network-friendly order.
     * @param networkBuf If non-NULL, a 16-byte network-endian-array to read from.  Typically you would pass in 
     *                    mySockAddr_in6.sin6_addr.s6_addr as the argument to this function.
     * @param optInterfaceIndex If non-NULL, this value will be used to set this object's _interfaceIndex value.
     */
   void ReadFromNetworkArray(const uint8 * networkBuf, const uint32 * optInterfaceIndex)
   {
      if (networkBuf)
      {
         ReadFromNetworkArrayAux(&networkBuf[0], _highBits);
         ReadFromNetworkArrayAux(&networkBuf[8], _lowBits);
      }
      if (optInterfaceIndex) _interfaceIndex = *optInterfaceIndex;
   }

private:
   void WriteToNetworkArrayAux( uint8 * out, const uint64 & in ) const {uint64 tmp = B_HOST_TO_BENDIAN_INT64(in); muscleCopyOut(out, tmp);}
   void ReadFromNetworkArrayAux(const uint8 * in, uint64 & out) const {uint64 tmp; muscleCopyIn(tmp, in); out = B_BENDIAN_TO_HOST_INT64(tmp);}

   uint64 _lowBits;
   uint64 _highBits;
   uint32 _interfaceIndex;
};

/** Given an IP address, returns a 32-bit hash code for it.  For IPv6, this is done by calling the HashCode() method on the ip_address object. */
inline uint32 GetHashCodeForIPAddress(const ip_address & ip) {return ip.HashCode();}

/** IPv6 Numeric representation of a all-zeroes invalid/guard address */
const ip_address invalidIP(0x00);

/** IPv6 Numeric representation of localhost (::1) for convenience */
const ip_address localhostIP(0x01);

/** IPv6 Numeric representation of link-local broadcast (ff02::1), for convenience */
const ip_address broadcastIP(0x01, ((uint64)0xFF02)<<48);

#endif

/** IPv4 Numeric representation of broadcast (255.255.255.255), for convenience 
  * This constant is defined in both IPv6 and IPv4 modes, since sometimes you
  * need to do an IPv4 broadcast even in an IPv6 program
  */
const ip_address broadcastIP_IPv4 = ip_address((uint32)-1);

/** IPv4 Numeric representation of broadcast (255.255.255.255), for convenience 
  * This constant is defined in both IPv6 and IPv4 modes, since sometimes you
  * need to do an IPv4 broadcast even in an IPv6 program
  */
const ip_address localhostIP_IPv4 = ip_address((((uint32)127)<<24)|((uint32)1));

/** This simple class holds an IP address and a port number, and lets you do
  * useful things on the two such as using them as key values in a hash table,
  * converting them to/from user-readable strings, etc.
  */
class IPAddressAndPort
{
public:
   /** Default constructor.   Creates an IPAddressAndPort object with the address field
     * set to (invalidIP) and the port field set to zero.
     */
   IPAddressAndPort() : _ip(invalidIP), _port(0) {/* empty */}

   /** Explicit constructor
     * @param ip The IP address to represent
     * @param port The port number to represent
     */
   IPAddressAndPort(const ip_address & ip, uint16 port) : _ip(ip), _port(port) {/* empty */}

   /** Convenience constructor.  Calling this is equivalent to creating an IPAddressAndPort
     * object and then calling SetFromString() on it with the given arguments.
     */
   IPAddressAndPort(const String & s, uint16 defaultPort, bool allowDNSLookups) {SetFromString(s(), defaultPort, allowDNSLookups);}

   /** Copy constructor */
   IPAddressAndPort(const IPAddressAndPort & rhs) : _ip(rhs._ip), _port(rhs._port) {/* empty */}

   /** Comparison operator.  Returns true iff (rhs) is equal to this object. 
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator == (const IPAddressAndPort & rhs) const {return (_ip == rhs._ip)&&(_port == rhs._port);}

   /** Comparison operator.  Returns true iff (rhs) is not equal to this object. 
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator != (const IPAddressAndPort & rhs) const {return !(*this==rhs);}

   /** Comparison operator.  Returns true iff this object is "less than" (rhs).
     * The comparison is done first on the IP address, and if that matches, a sub-comparison is done on the port field.
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator < (const IPAddressAndPort & rhs) const {return ((_ip < rhs._ip)||((_ip == rhs._ip)&&(_port < rhs._port)));}

   /** Comparison operator.  Returns true iff this object is "greater than" (rhs).
     * The comparison is done first on the IP address, and if that matches, a sub-comparison is done on the port field.
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator > (const IPAddressAndPort & rhs) const {return ((_ip > rhs._ip)||((_ip == rhs._ip)&&(_port > rhs._port)));}

   /** Comparison operator.  Returns true iff this object is "less than or equal to" (rhs).
     * The comparison is done first on the IP address, and if that matches, a sub-comparison is done on the port field.
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator <= (const IPAddressAndPort & rhs) const {return !(*this>rhs);}

   /** Comparison operator.  Returns true iff this object is "greater than or equal to" (rhs).
     * The comparison is done first on the IP address, and if that matches, a sub-comparison is done on the port field.
     * @param rhs The IPAddressAndPort object to compare this object to. 
     */
   bool operator >= (const IPAddressAndPort & rhs) const {return !(*this<rhs);}

   /** HashCode returns a usable 32-bit hash code value for this object, based on its contents. */
   uint32 HashCode() const {return GetHashCodeForIPAddress(_ip)+_port;}

   /** Returns this object's current IP address */
   const ip_address & GetIPAddress() const {return _ip;}

   /** Returns this object's current port number */
   uint16 GetPort() const {return _port;}

   /** Sets this object's IP address to (ip) */
   void SetIPAddress(const ip_address & ip) {_ip = ip;}

   /** Sets this object's port number to (port) */
   void SetPort(uint16 port) {_port = port;}

   /** Resets this object to its default state; as if it had just been created by the default constructor. */
   void Reset() {_ip = invalidIP; _port = 0;}

   /** Returns true iff both our IP address and port number are valid (i.e. non-zero) */
   bool IsValid() const {return ((_ip != invalidIP)&&(_port != 0));}

   /** Sets this object's state from the passed-in character string.
     * IPv4 address may be of the form "192.168.1.102", or of the form "192.168.1.102:2960".
     * As long as -DMUSCLE_AVOID_IPV6 is not defined, IPv6 address (e.g. "::1") are also supported.
     * Note that if you want to specify an IPv6 address and a port number at the same
     * time, you will need to enclose the IPv6 address in brackets, like this:  "[::1]:2960"
     * @param s The user-readable IP-address string, with optional port specification
     * @param defaultPort What port number to assign if the string does not specify a port number.
     * @param allowDNSLookups If true, this function will try to interpret non-numeric host names
     *                        e.g. "www.google.com" by doing a DNS lookup.  If false, then no
     *                        name resolution will be attempted, and only numeric IP addesses will be parsed.
     */
   void SetFromString(const String & s, uint16 defaultPort, bool allowDNSLookups);

   /** Returns a string representation of this object, similar to the forms
     * described in the SetFromString() documentation, above.
     * @param includePort If true, the port will be included in the string.  Defaults to true.
     * @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
     *                        Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
     */
   String ToString(bool includePort = true, bool preferIPv4Style = false) const;

private:
   ip_address _ip;
   uint16 _port;
};

/** Given a hostname or IP address string (e.g. "www.google.com" or "192.168.0.1" or "fe80::1"),
  * returns the numeric ip_address value that corresponds to that name.
  * @param name ASCII IP address or hostname to look up.
  * @param expandLocalhost If true, then if (name) corresponds to 127.0.0.1, this function
  *                        will attempt to determine the host machine's actual primary IP
  *                        address and return that instead.  Otherwise, 127.0.0.1 will be
  *                        returned in this case.  Defaults to false.
  * @return The associated IP address (local endianness), or 0 on failure.
  * @note This function may invoke a synchronous DNS lookup, which means that it may take
  *       a long time to return (e.g. if the DNS server is not responding)
  */
ip_address GetHostByName(const char * name, bool expandLocalhost = false);

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
ConstSocketRef Connect(const ip_address & hostIP, uint16 port, const char * debugHostName = NULL, const char * debugTitle = NULL, bool debugOutputOnErrorsOnly = true, uint64 maxConnectPeriod = MUSCLE_TIME_NEVER);

/** As above, only this version of Connect() takes an IPAddressAndPort object instead of separate ip_address and port arguments.
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

/** Convenience function for accepting a TCP connection on a given socket.  Has the same semantics as accept().
 *  (This is somewhat better than calling accept() directly, as certain cross-platform issues get transparently taken care of)
 * @param sock socket FD to accept from (e.g. a socket that was previously returned by CreateAcceptingSocket())
 * @param optRetLocalInfo if non-NULL, then on success, some information about the local side of the new
 *                        connection will be written here.  In particular, optRetLocalInfo()->GetIPAddress()
 *                        will return the IP address of the local network interface that the connection was
 *                        received on, and optRetLocalInfo()->GetPort() will return the port number that
 *                        the connection was received on.  Defaults to NULL.
 * @return A non-NULL ConstSocketRef if the accept was successful, or a NULL ConstSocketRef if the accept failed.
 */
ConstSocketRef Accept(const ConstSocketRef & sock, ip_address * optRetLocalInfo = NULL);

/** Reads as many bytes as possible from the given socket and places them into (buffer).
 *  @param sock The socket to read from.
 *  @param buffer Location to write the received bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param socketIsBlockingIO Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReceiveData(const ConstSocketRef & sock, void * buffer, uint32 bufferSizeBytes, bool socketIsBlockingIO);

/** Identical to ReceiveData(), except that this function's logic is adjusted to handle UDP semantics properly. 
 *  @param sock The socket to read from.
 *  @param buffer Location to place the received bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param socketIsBlockingIO Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @param optRetFromIP If set to non-NULL, then on success the ip_address this parameter points to will be filled in
 *                      with the IP address that the received data came from.  Defaults to NULL.
 *  @param optRetFromPort If set to non-NULL, then on success the uint16 this parameter points to will be filled in
 *                      with the source port that the received data came from.  Defaults to NULL.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReceiveDataUDP(const ConstSocketRef & sock, void * buffer, uint32 bufferSizeBytes, bool socketIsBlockingIO, ip_address * optRetFromIP = NULL, uint16 * optRetFromPort = NULL);

/** Similar to ReceiveData(), except that it will call read() instead of recv().
 *  This is the function to use if (fd) is referencing a file descriptor instead of a socket.
 *  @param fd The file descriptor to read from.
 *  @param buffer Location to place the read bytes into.
 *  @param bufferSizeBytes Number of bytes available at the location indicated by (buffer).
 *  @param fdIsBlockingIO Pass in true if the given fd is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes read into (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 ReadData(const ConstSocketRef & fd, void * buffer, uint32 bufferSizeBytes, bool fdIsBlockingIO);

/** Transmits as many bytes as possible from the given buffer over the given socket.
 *  @param sock The socket to transmit over.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to send.
 *  @param socketIsBlockingIO Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 SendData(const ConstSocketRef & sock, const void * buffer, uint32 bufferSizeBytes, bool socketIsBlockingIO);

/** Similar to SendData(), except that this function's logic is adjusted to handle UDP semantics properly.
 *  @param sock The socket to transmit over.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to send.
 *  @param socketIsBlockingIO Pass in true if the given socket is set to use blocking I/O, or false otherwise.
 *  @param optDestIP If set to non-zero, the data will be sent to the given IP address.  Otherwise it will
 *                   be sent to the socket's current IP address (see SetUDPSocketTarget()).  Defaults to zero.
 *  @param destPort If set to non-zero, the data will be sent to the specified port.  Otherwise it will
 *                  be sent to the socket's current destination port (see SetUDPSocketTarget()).  Defaults to zero.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 SendDataUDP(const ConstSocketRef & sock, const void * buffer, uint32 bufferSizeBytes, bool socketIsBlockingIO, const ip_address & optDestIP = invalidIP, uint16 destPort = 0);

/** Similar to SendData(), except that the implementation calls write() instead of send().  This
 *  is the function to use when (fd) refers to a file descriptor instead of a socket.
 *  @param fd The file descriptor to write the data to.
 *  @param buffer Buffer to read the outgoing bytes from.
 *  @param bufferSizeBytes Number of bytes to write.
 *  @param fdIsBlockingIO Pass in true if the given file descriptor is set to use blocking I/O, or false otherwise.
 *  @return The number of bytes sent from (buffer), or a negative value if there was an error.
 *          Note that this value may be smaller than (bufferSizeBytes).
 */
int32 WriteData(const ConstSocketRef & fd, const void * buffer, uint32 bufferSizeBytes, bool fdIsBlockingIO);

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
ConstSocketRef ConnectAsync(const ip_address & hostIP, uint16 port, bool & retIsReady);

/** As above, only this version of ConnectAsync() takes an IPAddressAndPort object instead of separate IP address and port arguments.
  * @param iap IP address and port to connect to.
  * @param retIsReady On success, this bool is set to true iff the socket is ready to use, or 
  *                   false to indicate that an asynchronous connection is now in progress.
  * @return A non-NULL ConstSocketRef (which is likely still in the process of connecting) on success, or a NULL ConstSocketRef if the accept failed.
  */
inline ConstSocketRef ConnectAsync(const IPAddressAndPort & iap, bool & retIsReady) {return ConnectAsync(iap.GetIPAddress(), iap.GetPort(), retIsReady);}

/** When a socket that was connecting asynchronously finally
  * selects ready-for-write to indicate that the asynchronous connect 
  * attempt has reached a conclusion, call this method.  It will finalize
  * the connection and make it ready for use.
  * @param sock The socket that was connecting asynchronously
  * @returns B_NO_ERROR if the connection is ready to use, or B_ERROR if the connect failed.
  * @note Under Windows, select() won't return ready-for-write if the connection fails... instead
  *       it will select-notify for your socket on the exceptions fd_set (if you provided one).
  *       Once this happens, there is no need to call FinalizeAsyncConnect() -- the fact that the
  *       socket notified on the exceptions fd_set is enough for you to know the asynchronous connection
  *       failed.  Successful asynchronous connect()s do exhibit the standard (select ready-for-write)
  *       behaviour, though.
  */
status_t FinalizeAsyncConnect(const ConstSocketRef & sock);

/** Shuts the given socket down.  (Note that you don't generally need to call this function; it's generally
 *  only useful if you need to half-shutdown the socket, e.g. stop the output direction but not the input
 *  direction)
 *  @param sock The socket to shutdown communication on. 
 *  @param disableReception If true, further reception of data will be disabled on this socket.
 *  @param disableTransmission If true, further transmission of data will be disabled on this socket.
 *  @return B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t ShutdownSocket(const ConstSocketRef & sock, bool disableReception = true, bool disableTransmission = true);

/** Convenience function for creating a TCP socket that is listening on a given local port for incoming TCP connections.
 *  @param port Which port to listen on, or 0 to have the system should choose a port for you
 *  @param maxbacklog Maximum connection backlog to allow for (defaults to 20)
 *  @param optRetPort If non-NULL, the uint16 this value points to will be set to the actual port bound to (useful when you want the system to choose a port for you)
 *  @param optInterfaceIP Optional IP address of the local network interface to listen on.  If left unspecified, or
 *                        if passed in as (invalidIP), then this socket will listen on all available network interfaces.
 * @return A non-NULL ConstSocketRef if the port was bound successfully, or a NULL ConstSocketRef if the accept failed.
 */
ConstSocketRef CreateAcceptingSocket(uint16 port, int maxbacklog = 20, uint16 * optRetPort = NULL, const ip_address & optInterfaceIP = invalidIP);

/** Translates the given 4-byte IP address into a string representation.
 *  @param address The 4-byte IP address to translate into text.
 *  @param outBuf Buffer where the NUL-terminated ASCII representation of the string will be placed.
 *                This buffer must be at least 64 bytes long (or at least 16 bytes long if MUSCLE_AVOID_IPV6 is defined)
 *  @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
 *                         Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
 */
void Inet_NtoA(const ip_address & address, char * outBuf, bool preferIPv4Style = false);

/** A more convenient version of INet_NtoA().  Given an IP address, returns a human-readable String representation of that address.
  *  @param ipAddress the IP address to return in String form.
  *  @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
  *                         Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
  */
String Inet_NtoA(const ip_address & ipAddress, bool preferIPv4Style = false);

/** Returns true iff (s) is a well-formed IP address (e.g. "192.168.0.1")
  * @param (s) An ASCII string to check the formatting of
  * @returns true iff there are exactly four dot-separated integers between 0 and 255
  *               and no extraneous characters in the string.
  */
bool IsIPAddress(const char * s);

/** Given a dotted-quad IP address in ASCII format (e.g. "192.168.0.1"), returns
  * the equivalent IP address in ip_address (packed binary) form. 
  * @param buf numeric IP address in ASCII.
  * @returns IP address as a ip_address, or invalidIP on failure.
  */
ip_address Inet_AtoN(const char * buf);

/** Returns a string that is the local host's primary host name. */
String GetLocalHostName();

/** Reurns the IP address that the given socket is connected to.
 *  @param sock The socket descriptor to find out info about.
 *  @param expandLocalhost If true, then if the peer's ip address is reported as 127.0.0.1, this 
 *                         function will attempt to determine the host machine's actual primary IP
 *                         address and return that instead.  Otherwise, 127.0.0.1 will be
 *                         returned in this case.
 *  @param optRetPort if non-NULL, the port we are connected to on the remote peer will be written here.  Defaults to NULL.
 *  @return The IP address on success, or invalidIP on failure (such as if the socket isn't valid and connected).
 */
ip_address GetPeerIPAddress(const ConstSocketRef & sock, bool expandLocalhost, uint16 * optRetPort = NULL);

/** Creates a pair of sockets that are connected to each other,
 *  so that any bytes you pass into one socket come out the other socket.
 *  This is useful when you want to wake up a thread that is blocked in select()...
 *  you have it select() on one socket, and you send a byte on the other.
 *  @param retSocket1 On success, this value will be set to the socket ID of the first socket.
 *  @param retSocket2 On success, this value will be set to the socket ID of the second socket.
 *  @param blocking Whether the two sockets should use blocking I/O or not.  Defaults to false.
 *  @return B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t CreateConnectedSocketPair(ConstSocketRef & retSocket1, ConstSocketRef & retSocket2, bool blocking = false);

/** Enables or disables blocking I/O on the given socket. 
 *  (Default for a socket is blocking mode enabled)
 *  @param sock the socket descriptor to act on.
 *  @param enabled Whether I/O on this socket should be enabled or not.
 *  @return B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t SetSocketBlockingEnabled(const ConstSocketRef & sock, bool enabled);

/** Returns true iff the given socket has blocking I/O enabled.
 *  @param sock the socket descriptor to query.
 *  @returns true iff the socket is in blocking I/O mode, or false if it is not (or if it is an invalid socket)
 *  @note this function is not implemented under Windows (as Windows doesn't appear to provide any method to
 *        obtain this information from a socket).  Under Windows, this method will simply log an erorr message
 *        and return false.
 */ 
bool GetSocketBlockingEnabled(const ConstSocketRef & sock);

/**
  * Turns Nagle's algorithm (output packet buffering/coalescing) on or off.
  * @param sock the socket descriptor to act on.
  * @param enabled If true, data will be held momentarily before sending, 
  *                to allow for bigger packets.  If false, each Write() 
  *                call will cause a new packet to be sent immediately.
  * @return B_NO_ERROR on success, B_ERROR on error.
  */
status_t SetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock, bool enabled);

/** Returns true iff the given socket has Nagle's algorithm enabled.
 *  @param sock the socket descriptor to query.
 *  @returns true iff the socket is has Nagle's algorithm enabled, or false if it does not (or if it is an invalid socket)
 */ 
bool GetSocketNaglesAlgorithmEnabled(const ConstSocketRef & sock);

/**
  * Sets the size of the given socket's TCP send buffer to the specified
  * size (or as close to that size as is possible).
  * @param sock the socket descriptor to act on.
  * @param sendBufferSizeBytes New size of the TCP send buffer, in bytes.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketSendBufferSize(const ConstSocketRef & sock, uint32 sendBufferSizeBytes);

/** 
  * Returns the current size of the socket's TCP send buffer.
  * @param sock The socket to query.
  * @return The size of the socket's TCP send buffer, in bytes, or -1 on error (e.g. invalid socket)
  */
int32 GetSocketSendBufferSize(const ConstSocketRef & sock);

/**
  * Sets the size of the given socket's TCP receive buffer to the specified
  * size (or as close to that size as is possible).
  * @param sock the socket descriptor to act on.
  * @param receiveBufferSizeBytes New size of the TCP receive buffer, in bytes.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketReceiveBufferSize(const ConstSocketRef & sock, uint32 receiveBufferSizeBytes);

/** 
  * Returns the current size of the socket's TCP receive buffer.
  * @param sock The socket to query.
  * @return The size of the socket's TCP receive buffer, in bytes, or -1 on error (e.g. invalid socket)
  */
int32 GetSocketReceiveBufferSize(const ConstSocketRef & sock);

/** This class is an interface to an object that can have its SocketCallback() method called
  * at the appropriate times when certain actions are performed on a socket.  By installing
  * a GlobalSocketCallback object via SetGlobalSocketCallback(), behaviors can be set for all 
  * sockets in the process, which can be simpler than changing every individual code path
  * to install those behaviors on a per-socket basis.
  */ 
class GlobalSocketCallback : private CountedObject<GlobalSocketCallback>
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
     * @returns B_NO_ERROR on success, or B_ERROR if the calling operation should be aborted.
     */
   virtual status_t SocketCallback(uint32 eventType, const ConstSocketRef & sock) = 0;

   enum {
      SOCKET_CALLBACK_CREATE_UDP = 0,   // socket was just created by CreateUDPSocket()
      SOCKET_CALLBACK_CREATE_ACCEPTING, // socket was just created by CreateAcceptingSocket()
      SOCKET_CALLBACK_ACCEPT,           // socket was just created by Accept()
      SOCKET_CALLBACK_CONNECT,          // socket was just created by Connect() or ConnectAsync()
      NUM_SOCKET_CALLBACKS
   };
};

/** Set the global socket-callback object for this process.
  * @param cb The object whose SocketCallback() method should be called by Connect(),
  *           Accept(), FinalizeAsyncConnect(), etc, or NULL to have no more callbacks
  */ 
void SetGlobalSocketCallback(GlobalSocketCallback * cb);

/** Returns the currently installed GlobalSocketCallback object, or NULL if there is none installed. */
GlobalSocketCallback * GetGlobalSocketCallback();

#ifdef MUSCLE_ENABLE_KEEPALIVE_API

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
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 maxProbeCount, uint64 idleTime, uint64 retransmitTime);

/**
  * Queries the values previously set on the socket by SetSocketKeepAliveBehavior().
  * @param sock The TCP socket to return the keepalive behavior of.
  * @param retMaxProbeCount if non-NULL, the max-probe-count value of the socket will be written into this argument.
  * @param retIdleTime if non-NULL, the idle-time of the socket will be written into this argument.
  * @param retRetransmitTime if non-NULL, the transmit-time of the socket will be written into this argument.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  * @see SetSocketKeepAliveBehavior()
  */ 
status_t GetSocketKeepAliveBehavior(const ConstSocketRef & sock, uint32 * retMaxProbeCount, uint64 * retIdleTime, uint64 * retRetransmitTime);

#endif

/** Set a user-specified IP address to return from GetHostByName() and GetPeerIPAddress() instead of 127.0.0.1.
  * Note that this function <b>does not</b> change the computer's IP address -- it merely changes what
  * the aforementioned functions will report.
  * @param ip New IP address to return instead of 127.0.0.1, or 0 to disable this override.
  */
void SetLocalHostIPOverride(const ip_address & ip);

/** Returns the user-specified IP address that was previously set by SetLocalHostIPOverride(), or 0
  * if none was set.  Note that this function <b>does not</b> report the local computer's IP address,
  * unless you previously called SetLocalHostIPOverride() with that address.
  */
ip_address GetLocalHostIPOverride();

/** Creates and returns a socket that can be used for UDP communications.
 *  Returns a negative value on error, or a non-negative socket handle on
 *  success.  You'll probably want to call BindUDPSocket() or SetUDPSocketTarget()
 *  after calling this method.
 */
ConstSocketRef CreateUDPSocket();

/** Attempts to given UDP socket to the given port.  
 *  @param sock The UDP socket (previously created by CreateUDPSocket())
 *  @param port UDP port ID to bind the socket to.  If zero, the system will choose a port ID.
 *  @param optRetPort if non-NULL, then on successful return the value this pointer points to will contain
 *                    the port ID that the socket was bound to.  Defaults to NULL.
 *  @param optFrom If non-zero, then the socket will be bound in such a way that only data
 *                 packets addressed to this IP address will be accepted.  Defaults to zero,
 *                 meaning that packets addressed to any of this machine's IP addresses will
 *                 be accepted.  (This parameter is typically only useful on machines with
 *                 multiple IP addresses) 
 *  @param allowShared If set to true, the port will be set up so that multiple processes
 *                     can bind to it simultaneously.  This is useful for sockets that are 
 *                     to be receiving broadcast UDP packets, since then you can run multiple
 *                     UDP broadcast receivers on a single computer. 
 *  @returns B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t BindUDPSocket(const ConstSocketRef & sock, uint16 port, uint16 * optRetPort = NULL, const ip_address & optFrom = invalidIP, bool allowShared = false);

/** Set the target/destination address for a UDP socket.  After successful return
 *  of this function, any data that is written to the UDP socket will be sent to this
 *  IP address and port.  This function is guaranteed to return quickly.
 *  @param sock The UDP socket to send to (previously created by CreateUDPSocket()).
 *  @param remoteIP Remote IP address that data should be sent to.
 *  @param remotePort Remote UDP port ID that data should be sent to.
 *  @returns B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t SetUDPSocketTarget(const ConstSocketRef & sock, const ip_address & remoteIP, uint16 remotePort);

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
 *  @returns B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t SetUDPSocketTarget(const ConstSocketRef & sock, const char * remoteHostName, uint16 remotePort, bool expandLocalhost = false);

/** Enable/disable sending of broadcast packets on the given UDP socket.
 *  @param sock UDP socket to enable or disable the sending of broadcast UDP packets with.
 *              (Note that the default state of newly created UDP sockets is broadcast-disabled)
 *  @param broadcast True if broadcasting should be enabled, false if broadcasting should be disabled.
 *  @returns B_NO_ERROR on success, or B_ERROR on failure.
 */
status_t SetUDPSocketBroadcastEnabled(const ConstSocketRef & sock, bool broadcast);

/** Returns true iff the specified UDP socket has broadcast enabled; or false if it does not.
  * @param sock The UDP socket to query.
  * @returns true iff the socket is enabled for UDP broadcast; false otherwise.
  */
bool GetUDPSocketBroadcastEnabled(const ConstSocketRef & sock);

/** This function returns true iff (ip) is one of the standard, well-known addresses for the
  * localhost loopback device.  (e.g. 127.0.0.1 or ::1 or fe80::1)
  * @param ip an IP address.
  * @returns true iff (ip) is a well-known loopback device address.  Note that this function
  *          does NOT check to see if an arbitrary IP address actually maps to the local host;  
  *          it merely sees if (ip) is one of a few hard-coded values as described above.
  */
bool IsStandardLoopbackDeviceAddress(const ip_address & ip);

/** This function returns true iff (ip) is a multicast IP address.
  * @param ip an IP address.
  * @returns true iff (ip) is a multicast address.
  */
bool IsMulticastIPAddress(const ip_address & ip);

/** Returns true iff (ip) is a non-zero IP address.
  * @param ip an IP adress.
  * @returns true iff (ip) is not all zeroes.  (Note: In IPv6 mode, the interface index is not considered here)
  */
bool IsValidAddress(const ip_address & ip);

/** Returns true iff (ip) is an IPv4-style (32-bits-only) or IPv4-mapped
  * (32-bits-only plus ffff prefix) address.
  * @param ip The IP address to examine.
  * @returns treu if (ip) is an IPv4 or IPv4-mapped address, else false.
  */
bool IsIPv4Address(const ip_address & ip);

/** This little container class is used to return data from the GetNetworkInterfaceInfos() function, below */
class NetworkInterfaceInfo
{
public:
   /** Default constructor.  Sets all member variables to default values. */
   NetworkInterfaceInfo();

   /** Constructor.  Sets all member variables to the values specified in the argument list.
     * @param name The name of the interface, as it is known to the computer (e.g. "/dev/eth0").
     * @param desc A human-readable description string describing the interface (e.g. "Ethernet Jack 0", or somesuch).
     * @param ip The local IP address associated with the interface.
     * @param netmask The netmask being used by this interface.
     * @param broadcastIP The broadcast IP address associated with this interface.
     * @param enabled True iff the interface is currently enabled; false if it is not.
     * @param copper True iff the interface currently has an ethernet cable plugged into it.
     */
   NetworkInterfaceInfo(const String & name, const String & desc, const ip_address & ip, const ip_address & netmask, const ip_address & broadcastIP, bool enabled, bool copper);

   /** Returns the name of this interface, or "" if the name is not known. */
   const String & GetName() const {return _name;}

   /** Returns a (human-readable) description of this interface, or "" if a description is unavailable. */
   const String & GetDescription() const {return _desc;}

   /** Returns the IP address of this interface */
   const ip_address & GetLocalAddress() const {return _ip;}

   /** Returns the netmask of this interface */
   const ip_address & GetNetmask() const {return _netmask;}

   /** If this interface is a point-to-point interface, this method returns the IP
     * address of the machine at the remote end of the interface.  Otherwise, this
     * method returns the broadcast address for this interface.
     */
   const ip_address & GetBroadcastAddress() const {return _broadcastIP;}

   /** Returns true iff this interface is currently enabled ("up"). */
   bool IsEnabled() const {return _enabled;}

   /** Returns true iff this network interface is currently plugged in to anything 
     * (i.e. iff the Ethernet cable is connected to the jack).
     * Note that copper detection is not currently supported under Windows, so
     * under Windows this will always return false.
     */
   bool IsCopperDetected() const {return _copper;}

   /** For debugging.  Returns a human-readable string describing this interface. */
   String ToString() const;

   /** Returns a hash code for this NetworkInterfaceInfo object. */
   uint32 HashCode() const;

private:
   String _name;
   String _desc;
   ip_address _ip;
   ip_address _netmask;
   ip_address _broadcastIP;
   bool _enabled;
   bool _copper;
};

/** Bits that can be passed to GetNetworkInterfaceInfos() or GetNetworkInterfaceAddresses(). */
enum {
   GNII_INCLUDE_IPV4_INTERFACES        = 0x01, // If set, IPv4-specific interfaces will be returned
   GNII_INCLUDE_IPV6_INTERFACES        = 0x02, // If set, IPv6-specific interfaces will be returned
   GNII_INCLUDE_LOOPBACK_INTERFACES    = 0x04, // If set, loopback interfaces (e.g. lo0/127.0.0.1) will be returned
   GNII_INCLUDE_NONLOOPBACK_INTERFACES = 0x08, // If set, non-loopback interfaces (e.g. en0) will be returned
   GNII_INCLUDE_ENABLED_INTERFACES     = 0x10, // If set, enabled (aka "up") interfaces will be returned
   GNII_INCLUDE_DISABLED_INTERFACES    = 0x20, // If set, disabled (aka "down") interfaces will be returned
   GNII_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT = 0x40, // If set, loopback interfaces will be returned only if no other interfaces are found

   // For convenience, GNII_INCLUDE_MUSCLE_PREFERRED_INTERFACES will specify interfaces of the family specified by MUSCLE_AVOID_IPV6's presence/abscence.
#ifdef MUSCLE_AVOID_IPV6
   GNII_INCLUDE_MUSCLE_PREFERRED_INTERFACES = GNII_INCLUDE_IPV4_INTERFACES,
#else
   GNII_INCLUDE_MUSCLE_PREFERRED_INTERFACES = GNII_INCLUDE_IPV6_INTERFACES,
#endif

   GNII_INCLUDE_ALL_INTERFACES  = 0xFFFFFFFF,  // If set, all interfaces will be returned
};

/** This function queries the local OS for information about all available network
  * interfaces.  Note that this method is only implemented for some OS's (Linux,
  * MacOS/X, Windows), and that on other OS's it may just always return B_ERROR.
  * @param results On success, zero or more NetworkInterfaceInfo objects will
  *                be added to this Queue for you to look at.
  * @param includeBits A chord of GNII_INCLUDE_* bits indicating which types of network interface you want to be
  *                    included in the returned list.  Defaults to GNII_INCLUDE_ALL_INTERFACES, which indicates that
  *                    no filtering of the returned list should be done.
  * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory, call not implemented for the current OS, etc)
  */
status_t GetNetworkInterfaceInfos(Queue<NetworkInterfaceInfo> & results, uint32 includeBits = GNII_INCLUDE_ALL_INTERFACES);

/** This is a more limited version of GetNetworkInterfaceInfos(), included for convenience.
  * Instead of returning all information about the local host's network interfaces, this
  * one returns only their IP addresses.  It is the same as calling GetNetworkInterfaceInfos()
  * and then iterating the returned list to assemble a list only of the IP addresses returned
  * by GetBroadcastAddress().
  * @param retAddresses On success, zero or more ip_addresses will be added to this Queue for you to look at.
  * @param includeBits A chord of GNII_INCLUDE_* bits indicating which types of network interface you want to be
  *                    included in the returned list.  Defaults to GNII_INCLUDE_ALL_INTERFACES, which indicates that
  *                    no filtering of the returned list should be done.
  * @returns B_NO_ERROR on success, or B_ERROR on failure (out of memory,
  *          call not implemented for the current OS, etc)
  */
status_t GetNetworkInterfaceAddresses(Queue<ip_address> & retAddresses, uint32 includeBits = GNII_INCLUDE_ALL_INTERFACES);

#ifndef MUSCLE_AVOID_MULTICAST_API

/** Sets whether multicast data sent on this socket should be received by
  * this socket or not (IP_MULTICAST_LOOP).  Default state is enabled/true.
  * @param sock The socket to set the state of the IP_MULTICAST_LOOP flag on.
  * @param multicastToSelf If true, enable loopback.  Otherwise, disable it.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketMulticastToSelf(const ConstSocketRef & sock, bool multicastToSelf);

/** Returns true iff multicast packets sent by this socket should be received by this socket.
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
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketMulticastTimeToLive(const ConstSocketRef & sock, uint8 ttl);

/** Returns the "time to live" associated with multicast packets sent on
  * this socket, or 0 on failure.
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
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t SetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock, const ip_address & address);

/** Returns the address of the local interface that the given socket will
  * try to send multicast packets on, or invalidIP on failure.
  * @param sock The socket to query the sending interface of.
  * @returns the interface's IP address, or invalidIP on error.
  */
ip_address GetSocketMulticastSendInterfaceAddress(const ConstSocketRef & sock);

/** Attempts to add the specified socket to the specified multicast group.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @param localInterfaceAddress Optional IP address of the local interface use for receiving
  *                              data from this group.  If left as (invalidIP), an appropriate
  *                              interface will be chosen automatically.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress, const ip_address & localInterfaceAddress = invalidIP);

/** Attempts to remove the specified socket from the specified multicast group
  * that it was previously added to.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @param localInterfaceAddress Optional IP address of the local interface used for receiving
  *                              data from this group.  If left as (invalidIP), the first matching
  *                              group will be removed.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress, const ip_address & localInterfaceAddress = invalidIP);

#else  // end IPv4 multicast, begin IPv6 multicast

/** Specify the interface index of the local network interface that the given socket should
  * send multicast packets on.  If this isn't called, the kernel will choose an appropriate
  * default interface to send multicast packets over.
  * @param sock The socket to set the sending interface for.
  * @param interfaceIndex Optional index of the interface to send the multicast packets on.
  *                       Zero means the default interface, larger values mean another interface.
  *                       You can call GetNetworkInterfaceInfos() to get the list of available interfaces.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
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
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t AddSocketToMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress);

/** Attempts to remove the specified socket from the specified multicast group that it was previously added to.
  * @param sock The socket to add to the multicast group
  * @param groupAddress The IP address of the multicast group.
  * @returns B_NO_ERROR on success, or B_ERROR on failure.
  */
status_t RemoveSocketFromMulticastGroup(const ConstSocketRef & sock, const ip_address & groupAddress);

#endif  // end IPv6 multicast

#endif  // !MUSCLE_AVOID_MULTICAST_API

/// @cond HIDDEN_SYMBOLS

// Different OS's use different types for pass-by-reference in accept(), etc.
// So I define my own muscle_socklen_t to avoid having to #ifdef all my code
#if defined(__amd64__) || defined(__FreeBSD__) || defined(BSD) || defined(__PPC64__) || defined(__HAIKU__) || defined(ANDROID) || defined(_SOCKLEN_T)
typedef socklen_t muscle_socklen_t;
#elif defined(__BEOS__) || defined(__HAIKU__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(WIN32) || defined(__QNX__) || defined(__osf__)
typedef int muscle_socklen_t;
#else
typedef size_t muscle_socklen_t;
#endif

static inline long recv_ignore_eintr(    int s, void *b, unsigned long numBytes, int flags) {int ret; do {ret = recv(s, (char *)b, numBytes, flags);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long recvfrom_ignore_eintr(int s, void *b, unsigned long numBytes, int flags, struct sockaddr *a, muscle_socklen_t * alen) {int ret; do {ret = recvfrom(s, (char *)b, numBytes, flags, a,  alen);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long send_ignore_eintr(    int s, const void *b, unsigned long numBytes, int flags) {int ret; do {ret = send(s, (char *)b, numBytes, flags);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long sendto_ignore_eintr(  int s, const void *b, unsigned long numBytes, int flags, const struct sockaddr * d, int dlen) {int ret; do {ret = sendto(s, (char *)b, numBytes, flags,  d, dlen);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
#ifndef WIN32
static inline long read_ignore_eintr(    int f, void *b, unsigned long nbyte) {int ret; do {ret = read(f, (char *)b, nbyte);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
static inline long write_ignore_eintr(   int f, const void *b, unsigned long nbyte) {int ret; do {ret = write(f, (char *)b, nbyte);} while((ret<0)&&(PreviousOperationWasInterrupted())); return ret;}
#endif

/// @endcond

/** @} */ // end of networkutilityfunctions doxygen group

}; // end namespace muscle

#endif

