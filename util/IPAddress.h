/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef IPAddress_h
#define IPAddress_h

extern "C" {
struct sockaddr_in;   // forward declaration
struct in_addr;       // forward declaration
#ifndef MUSCLE_AVOID_IPV6
struct sockaddr_in6;  // forward declaration
struct in6_addr;      // forward declaration
#endif
};

#include "support/PseudoFlattenable.h"
#include "util/String.h"

namespace muscle {

enum {
   IP_ADDRESS_TYPE          = 1230004063, /**< Typecode for the IPAddress class: 'IP__' */
   IP_ADDRESS_AND_PORT_TYPE = 1230004560  /**< Typecode for the IPAddressAndPort class: 'IPaP' */
};

/** This class represents an IPv6 network address, including the 128-bit IP
  * address and the interface index field (necessary for connecting to link-local addresses).
  * It can also be used to represent an IPv4 network address.
  */
class MUSCLE_NODISCARD IPAddress MUSCLE_FINAL_CLASS : public PseudoFlattenable<IPAddress>
{
public:
   /** Constructor
     * @param lowBits the lower 64 bits of the IP address (defaults to 0).  For an IPv4 address, only the lower 32 bits of this value are used.
     * @param highBits the upper 64 bits of the IP address (defaults to 0).  Not used for IPv4 addresses.
     * @param interfaceIndex the interface index (defaults to MUSCLE_NO_LIMIT, a.k.a invalid).  Useful primarily for link-local IPv6 addresses.  Not used for IPv4.
     */
   MUSCLE_CONSTEXPR IPAddress(uint64 lowBits = 0, uint64 highBits = 0, uint32 interfaceIndex = MUSCLE_NO_LIMIT) : _lowBits(lowBits), _highBits(highBits), _interfaceIndex(interfaceIndex) {/* empty */}

   /** Convenience constructor.  Calling this is equivalent to creating an IPAddress
     * object and then calling SetFromString() on it with the given arguments.
     * @param s an IPAddress to parse (eg "127.0.0.1" or "ff12::02@3")
     */
   explicit IPAddress(const String & s) : _lowBits(0), _highBits(0), _interfaceIndex(MUSCLE_NO_LIMIT) {(void) SetFromString(s);}

   /** Convenience constructor.  Create this IPAddress based on the contents of the given in_addr struct.
     * @param ip4Address an IPv4 address expressed as a BSD Sockets API's in_addr
     */
   IPAddress(const struct in_addr & ip4Address);

#ifndef MUSCLE_AVOID_IPV6
   /** Convenience constructor.  Initializes this IPAddress based on the contents of the given sockaddr_in6 struct.
     * @param ipv6Address an IPv6 address expressed as a BSD Sockets API's in6_addr
     * @param optInterfaceIndex if set to a value other than MUSCLE_NO_LIMIT, AND (ipv6Address) is a link-local
     *                          IPv6 address, then this will be used as the local IPv6 scope (aka network interface index)
     *                          for this IP address.
     *                          For IPv4 addresses and non-link-local addresses, this argument will be ignored.
     */
   IPAddress(const struct in6_addr & ipv6Address, uint32 optInterfaceIndex = MUSCLE_NO_LIMIT);
#endif

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   MUSCLE_CONSTEXPR IPAddress(const IPAddress & rhs) : _lowBits(rhs._lowBits), _highBits(rhs._highBits), _interfaceIndex(rhs._interfaceIndex) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   IPAddress & operator = (const IPAddress & rhs) {_lowBits = rhs._lowBits; _highBits = rhs._highBits; _interfaceIndex = rhs._interfaceIndex; return *this;}

   /** Returns true if this IPAddress is equal to (rhs), disregarding the interfaceIndex field.
     * @param rhs the IPAddress to compare this IPAddress against
     */
   MUSCLE_NODISCARD bool EqualsIgnoreInterfaceIndex(const IPAddress & rhs) const {return ((_lowBits == rhs._lowBits)&&(_highBits == rhs._highBits));}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator == (const IPAddress & rhs) const {return ((EqualsIgnoreInterfaceIndex(rhs))&&(_interfaceIndex == rhs._interfaceIndex));}

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
   bool operator != (const IPAddress & rhs) const {return !(*this == rhs);}

   /** @copydoc DoxyTemplate::operator<(const DoxyTemplate &) const */
   bool operator < (const IPAddress & rhs) const
   {
      if (_highBits < rhs._highBits) return true;
      if (_highBits > rhs._highBits) return false;
      if (_lowBits  < rhs._lowBits)  return true;
      if (_lowBits  > rhs._lowBits)  return false;
      return (_interfaceIndex < rhs._interfaceIndex);
   }

   /** @copydoc DoxyTemplate::operator>(const DoxyTemplate &) const */
   bool operator > (const IPAddress & rhs) const
   {
      if (_highBits < rhs._highBits) return false;
      if (_highBits > rhs._highBits) return true;
      if (_lowBits  < rhs._lowBits)  return false;
      if (_lowBits  > rhs._lowBits)  return true;
      return (_interfaceIndex > rhs._interfaceIndex);
   }

   /** @copydoc DoxyTemplate::operator<=(const DoxyTemplate &) const */
   bool operator <= (const IPAddress & rhs) const {return (*this == rhs)||(*this < rhs);}

   /** @copydoc DoxyTemplate::operator>=(const DoxyTemplate &) const */
   bool operator >= (const IPAddress & rhs) const {return (*this == rhs)||(*this > rhs);}

   /** @copydoc DoxyTemplate::operator&(const DoxyTemplate &) const */
   IPAddress operator & (const IPAddress & rhs) const {return IPAddress(_lowBits & rhs._lowBits, _highBits & rhs._highBits, _interfaceIndex);}

   /** @copydoc DoxyTemplate::operator|(const DoxyTemplate &) const */
   IPAddress operator | (const IPAddress & rhs) const {return IPAddress(_lowBits | rhs._lowBits, _highBits | rhs._highBits, _interfaceIndex);}

   /** @copydoc DoxyTemplate::operator~() const */
   IPAddress operator ~ () const {return IPAddress(~_lowBits, ~_highBits, _interfaceIndex);}

   /** @copydoc DoxyTemplate::operator&=(const DoxyTemplate &) */
   IPAddress operator &= (const IPAddress & rhs) {*this = *this & rhs; return *this;}

   /** @copydoc DoxyTemplate::operator|=(const DoxyTemplate &) */
   IPAddress operator |= (const IPAddress & rhs) {*this = *this | rhs; return *this;}

   /** Sets all 128 bits of this IP address
     * @param lowBits the lower 64 bits of the IP address
     * @param highBits the upper 64 bits of the IP address
     */
   void SetBits(uint64 lowBits, uint64 highBits) {_lowBits = lowBits; _highBits = highBits;}

   /** Returns the lower 64 bits of the IP address (for IPv4 the lower 32 bits of this value represent the IPv4 address) */
   MUSCLE_NODISCARD uint64 GetLowBits()  const {return _lowBits;}

   /** Returns the upper 64 bits of the IP address (for IPv4 this value should always be zero) */
   MUSCLE_NODISCARD uint64 GetHighBits() const {return _highBits;}

   /** Sets the lower 64 bits of the IP address
     * @param lb the new lower-64-bits value
     */
   void SetLowBits( uint64 lb) {_lowBits  = lb;}

   /** Sets the upper 64 bits of the IP address
     * @param hb the new upper-64-bits value
     */
   void SetHighBits(uint64 hb) {_highBits = hb;}

   /** Set a valid interface-index/Zone-ID value for this IP address.
     * @param iidx the new interface index to use.  To specify an invalid Interface Index, pass
     *             MUSCLE_NO_LIMIT here, or call UnsetInterfaceIndex() instead.
     * @note this value is meaningful only for link-local IPv6 addresses.
     */
   void SetInterfaceIndex(uint32 iidx) {_interfaceIndex = iidx;}

   /** Convenience method:  Sets our own interface-index to match that of (ip)
     * @param ip the IPAddress who interface-index we should match
     */
   void SetInterfaceIndexFrom(const IPAddress & ip) {_interfaceIndex = ip._interfaceIndex;}

   /** Resets our interface-index field back to an invalid value. */
   void UnsetInterfaceIndex() {_interfaceIndex = MUSCLE_NO_LIMIT;}

   /** Returns true iff our interface-index field is currently set to a valid value */
   MUSCLE_NODISCARD bool IsInterfaceIndexValid() const {return (_interfaceIndex != MUSCLE_NO_LIMIT);}

   /** Returns the interface-index/Zone-ID value for this IP address.  Meaningful only in the context of IPv6.
     * @param optDefaultValue what value to return if our interface-index field isn't currently valid.  Defaults to 0.
     */
   MUSCLE_NODISCARD uint32 GetInterfaceIndex(uint32 optDefaultValue = 0) const {return IsInterfaceIndexValid() ? _interfaceIndex : optDefaultValue;}

   /** @copydoc DoxyTemplate::HashCode() const */
   MUSCLE_NODISCARD uint32 HashCode() const {return CalculateHashCode(_interfaceIndex)+CalculateHashCode(_lowBits)+CalculateHashCode(_highBits);}

   /** Convenience method:  Writes our current state out to (writeToInAddr), if possible.
     * @param writeToInAddr on return, this object's fields will be filled out based on our state.
     * @note a sockaddr_in cannot hold an IPv6 address, only an IPv4 address, so calling this method on an
     *                     IPAddress object that contains an IPv6 address will not produce a usable result.
     */
   void WriteToInAddr(struct in_addr & writeToInAddr) const;

#ifndef MUSCLE_AVOID_IPV6
   /** Convenience method:  Writes our current state out to (writeToInAddr6), if possible.
     * @param writeToInAddr6 on return, this object's fields will be filled out based on our state.
     * @param optWriteInterfaceIndex if non-NULL, we'll copy our network-interface-index into the value this argument points to.
     */
   void WriteToIn6Addr(struct in6_addr & writeToInAddr6, uint32 * optWriteInterfaceIndex = NULL) const;
#endif

   /** Writes our address into the specified uint8 array, in the required network-friendly order.
     * @param networkBuf If non-NULL, the 16-byte network-array to write to.  Typically you would pass in
     *                   mySockAddr_in6.sin6_addr.s6_addr as the argument to this function.
     * @param optWriteInterfaceIndex If non-NULL, this value will receive a copy of our interface index.  Typically
     *                          you would pass a pointer to mySockAddr_in6.sin6_addr.sin6_scope_id here.
     */
   void WriteToNetworkArray(uint8 * networkBuf, uint32 * optWriteInterfaceIndex) const
   {
      if (networkBuf)
      {
         WriteToNetworkArrayAux(&networkBuf[0*sizeof(uint64)], _highBits);
         WriteToNetworkArrayAux(&networkBuf[1*sizeof(uint64)], _lowBits);
      }
      if (optWriteInterfaceIndex) *optWriteInterfaceIndex = GetInterfaceIndex();
   }

   /** Reads our address in from the specified uint8 array, in the required network-friendly order.
     * @param networkBuf If non-NULL, a 16-byte network-endian-array to read from.  Typically you would pass in
     *                    mySockAddr_in6.sin6_addr.s6_addr as the argument to this function.
     * @param optReadInterfaceIndex If non-NULL, this value will be passed to SetInterfaceIndex() to set this object's interface-index value.
     */
   void ReadFromNetworkArray(const uint8 * networkBuf, const uint32 * optReadInterfaceIndex)
   {
      if (networkBuf)
      {
         ReadFromNetworkArrayAux(&networkBuf[0*sizeof(uint64)], _highBits);
         ReadFromNetworkArrayAux(&networkBuf[1*sizeof(uint64)], _lowBits);
      }
      if (optReadInterfaceIndex) SetInterfaceIndex(*optReadInterfaceIndex);
   }

   /** Part of the PseudoFlattenable pseudo-interface:  Returns true */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}

   /** Part of the PseudoFlattenable pseudo-interface:  Returns IP_ADDRESS_TYPE */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 TypeCode() {return IP_ADDRESS_TYPE;}

   /** Part of the PseudoFlattenable pseudo-interface */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return sizeof(uint64) + sizeof(uint64) + sizeof(uint32);}

   /** @copydoc DoxyTemplate::CalculateChecksum() const */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const;

   /** @copydoc DoxyTemplate::Flatten(DataFlattener flat) const */
   void Flatten(DataFlattener flat) const;

   /** @copydoc DoxyTemplate::Unflatten(DataUnflattener &) */
   status_t Unflatten(DataUnflattener & unflat);

   /** Convenience method:  Returns a SOCKET_FAMILY_* value describing this IP address's associated socket family.
     * @returns SOCKET_FAMILY_IPV4 is we are an IPv4 address, or SOCKET_FAMILY_IPV6 is we are an IPv6 address ,or SOCKET_FAMILY_INVALID if we are an invalid address.
     */
   MUSCLE_NODISCARD int GetSocketFamily() const
   {
#ifdef MUSCLE_AVOID_IPV6
      return IsValid() ? SOCKET_FAMILY_IPV4 : SOCKET_FAMILY_INVALID;
#else
      return IsValid() ? (IsIPv4() ? SOCKET_FAMILY_IPV4 : SOCKET_FAMILY_IPV6) : SOCKET_FAMILY_INVALID;
#endif
   }

   /** Convenience method:  Returns true iff this is a valid IP address
     * (where "valid" in this case means non-zero: or if MUSCLE_AVOID_IPV6 is defined,
     * it means that the 64 high-bits are all zero, and the upper 32-bits of the
     * 64 low-bits are zero, and the lower 32-bits of the 64 low-bits are non-zero.
     */
   MUSCLE_NODISCARD bool IsValid() const;

   /** Returns true iff this address qualifies as an IPv4 address.
     * (If MUSCLE_AVOID_IPV6 is defined, this method always returns true)
     */
   MUSCLE_NODISCARD bool IsIPv4() const;

   /** Returns true iff this address qualifies as an IPv4 broadcast address.
     * @note This method is IPv4-specific (there is no such thing as an IPv6 broadcast address, only multicast!)
     */
   MUSCLE_NODISCARD bool IsBroadcast() const;

   /** Returns true iff this address qualifies as a multicast address.  */
   MUSCLE_NODISCARD bool IsMulticast() const;

   /** Returns true iff this address qualifies as an IPv6 node-local multicast address.  */
   MUSCLE_NODISCARD bool IsIPv6NodeLocalMulticast() const {return IsIPv6LocalMulticast(0x01);}

   /** Returns true iff this address qualifies as an IPv6 link-local multicast address.  */
   MUSCLE_NODISCARD bool IsIPv6LinkLocalMulticast() const {return IsIPv6LocalMulticast(0x02);}

   /** Returns true iff this address is IPv6-link-local (ie it can use the network-interface-index field) */
   MUSCLE_NODISCARD bool IsIPv6LinkLocal() const;

   /** Returns true iff this address qualifies as a standard loopback-device address (eg 127.0.0.1 or ::1 or fe80::1) */
   MUSCLE_NODISCARD bool IsStandardLoopbackDeviceAddress() const;

   /** Returns true iff this address is a stateless/self-assigned IP address (ie 169.254.*.* for IPv4, or fe80::* for IPv6) */
   MUSCLE_NODISCARD bool IsSelfAssigned() const;

   /** Returns a human-readable string equivalent to this IPAddress object.  Behaves the same as Inet_NtoA(*this, printIPv4AddressesInIPv4Style, expandScopeIndicesToNetworkInterfaceNames).
     * @param printIPv4AddressesInIPv4Style If set true, then IPv4 addresses will be returned as eg "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
     *                                      Defaults to true.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
     * @param expandScopeIndicesToNetworkInterfaceNames If set to true, IPv6 addresses with multicast-scope annotations (e.g. fe80::123@5)
     *                                      will result in a string with the multicast-scope annotation returned as its interface-name
     *                                      (e.g. "fe80::123%en0") instead of just the scope ID integer.  Enabling this option does make this
     *                                      call significantly more expensive (since it may need to iterate through the local NICs listing)
     *                                      so this argument defaults to false.
     */
   String ToString(bool printIPv4AddressesInIPv4Style = true, bool expandScopeIndicesToNetworkInterfaceNames = false) const;

   /** Given a human-readable IP-address string (as returned by ToString()), sets this IPAddress object to the
     * value represented by the string.  Behaves similarly to Inet_AtoN()
     * @param ipAddressString The string to parse
     * @returns B_NO_ERROR on success, or B_BAD_ARGUMENT if the String could not be parsed.
     */
   status_t SetFromString(const String & ipAddressString);

   /** Convenience method for producing a URL (e.g. "http://127.0.0.1") string from this IP address
     * @param handler the handler-type to include (e.g. "http" or "https")
     * @returns the computed URL string, or an empty String if this IPAddress isn't valid
     */
   String ToURL(const String & handler) const;

   /** Sets us to the IPv4 address specified by (bits)
     * @param bits a 32-bit representation of an IPv4 address.
     */
   void SetIPv4AddressFromUint32(uint32 bits) {_lowBits = bits; _highBits = 0; UnsetInterfaceIndex();}

   /** Returns our IPv4 address as a uint32.
     * If we represent something other than an IPv4 address, the return value is undefined.
     */
   MUSCLE_NODISCARD uint32 GetIPv4AddressAsUint32() const {return ((uint32)(_lowBits & 0xFFFFFFFF));}

   /** Convenience method:  Returns an IPAddress object identical to this one,
     * except that the returned IPAddress has its interface index field set to the specified value.
     * @param interfaceIndex The new interface index value to use in the returned object (or MUSCLE_NO_LIMIT to specify an invalid interface index)
     */
   IPAddress WithInterfaceIndex(uint32 interfaceIndex) const
   {
      IPAddress addr = *this;
      addr.SetInterfaceIndex(interfaceIndex);
      return addr;
   }

   /** Convenience method:  Returns an IPAddress object identical to this one, but with the interface-index field unset. */
   IPAddress WithoutInterfaceIndex() const {return WithInterfaceIndex(MUSCLE_NO_LIMIT);}

private:
   MUSCLE_NODISCARD bool IsIPv6LocalMulticast(uint8 scope) const;
   void WriteToNetworkArrayAux(uint8 * out, uint64 in)          const {BigEndianConverter::Export(in, out);}
   void ReadFromNetworkArrayAux(const uint8 * in, uint64 & out) const {BigEndianConverter::Import(in, out);}

   uint64 _lowBits;
   uint64 _highBits;
   uint32 _interfaceIndex;  // will be set to MUSCLE_NO_LIMIT if the interface-index value is invalid
};

/** Numeric representation of a all-zeroes invalid/guard address (same for both IPv4 and IPv6) */
const IPAddress invalidIP(0x00);

/** IPv4 Numeric representation of broadcast (255.255.255.255) */
const IPAddress localhostIP_IPv4 = IPAddress((((uint32)127)<<24)|((uint32)1));

/** IPv6 Numeric representation of localhost (::1) for convenience */
const IPAddress localhostIP_IPv6(0x01);

/** IPv4 Numeric representation of broadcast (255.255.255.255), for convenience
  * This constant is defined in both IPv6 and IPv4 modes, since sometimes you
  * need to do an IPv4 broadcast even in an IPv6 program
  */
const IPAddress broadcastIP_IPv4 = IPAddress((uint32)-1);

/** IPv6 Numeric representation of link-local broadcast (ff02::1), for convenience */
const IPAddress broadcastIP_IPv6(0x01, ((uint64)0xFF02)<<48);

#ifdef MUSCLE_AVOID_IPV6

/** Representation of the canonical localhost IP address (127.0.0.1 if MUSCLE_AVOID_IPV6 is defined, otherwise ::1) */
const IPAddress localhostIP = localhostIP_IPv4;

/** Representation of the canonical broadcast IP address (255.255.255.255 if MUSCLE_AVOID_IPV6 is defined, otherwise ff02::1) */
const IPAddress broadcastIP = broadcastIP_IPv4;
#else

/** Representation of the canonical localhost IP address (127.0.0.1 if MUSCLE_AVOID_IPV6 is defined, otherwise ::11) */
const IPAddress localhostIP = localhostIP_IPv6;

/** Representation of the canonical broadcast IP address (255.255.255.255 if MUSCLE_AVOID_IPV6 is defined, otherwise ff02::1) */
const IPAddress broadcastIP = broadcastIP_IPv6;

#endif

/** This simple class holds both an IP address and a port number, and lets you do
  * useful things on the two such as using them as key values in a hash table,
  * converting them to/from user-readable strings, etc.
  */
class MUSCLE_NODISCARD IPAddressAndPort MUSCLE_FINAL_CLASS : public PseudoFlattenable<IPAddressAndPort>
{
public:
   /** Default constructor.   Creates an IPAddressAndPort object with the address field
     * set to (invalidIP) and the port field set to zero.
     */
   MUSCLE_CONSTEXPR IPAddressAndPort() : _ip(invalidIP), _port(0) {/* empty */}

   /** Explicit constructor
     * @param ip The IP address to represent
     * @param port The port number to represent
     */
   MUSCLE_CONSTEXPR IPAddressAndPort(const IPAddress & ip, uint16 port) : _ip(ip), _port(port) {/* empty */}

   /** Convenience constructor.  Initializes this IPAddressAndPort based on the contents of the given sockaddr_in struct.
     * @param sockAddr an IPv4 address expressed as a BSD Sockets API's sockaddr_in
     */
   IPAddressAndPort(const struct sockaddr_in & sockAddr);

#ifndef MUSCLE_AVOID_IPV6
   /** Convenience constructor.  Initializes this IPAddressAndPort based on the contents of the given sockaddr_in6 struct.
     * @param sockAddr6 an IPv6 address expressed as a BSD Sockets API's sockaddr_in
     */
   IPAddressAndPort(const struct sockaddr_in6 & sockAddr6);
#endif

   /** Convenience constructor.  Calling this is equivalent to creating an IPAddressAndPort
     * object and then calling SetFromString() on it with the given arguments.
     * @param s an IPAddressAndPortString to parse (eg "127.0.0.1:9999" or "www.google.com:80")
     * @param defaultPort what value we should use as a default port, if (s) doesn't specify one
     * @param allowDNSLookups true iff we want to parse hostnames (could be slow!); false if we only care about parsing explicit IP addresses
     */
   IPAddressAndPort(const String & s, uint16 defaultPort, bool allowDNSLookups) {SetFromString(s, defaultPort, allowDNSLookups);}

   /** @copydoc DoxyTemplate::DoxyTemplate(const DoxyTemplate &) */
   IPAddressAndPort(const IPAddressAndPort & rhs) : _ip(rhs._ip), _port(rhs._port) {/* empty */}

   /** @copydoc DoxyTemplate::operator=(const DoxyTemplate &) */
   IPAddressAndPort & operator = (const IPAddressAndPort & rhs) {_ip = rhs._ip; _port = rhs._port; return *this;}

   /** @copydoc DoxyTemplate::operator==(const DoxyTemplate &) const */
   bool operator == (const IPAddressAndPort & rhs) const {return (_ip == rhs._ip)&&(_port == rhs._port);}

   /** @copydoc DoxyTemplate::operator!=(const DoxyTemplate &) const */
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

   /** @copydoc DoxyTemplate::HashCode() const */
   MUSCLE_NODISCARD uint32 HashCode() const {return _ip.HashCode()+_port;}

   /** Returns this object's current IP address */
   MUSCLE_NODISCARD const IPAddress & GetIPAddress() const {return _ip;}

   /** Returns this object's current port number */
   MUSCLE_NODISCARD uint16 GetPort() const {return _port;}

   /** Sets both the IP address and port fields of this object.
     * @param ip the new IP address to use
     * @param port the new port to use
     */
   void Set(const IPAddress & ip, uint16 port) {_ip = ip; _port = port;}

   /** Sets this object's IP address to (ip)
     * @param ip the new IP address to use
     */
   void SetIPAddress(const IPAddress & ip) {_ip = ip;}

   /** Sets this object's port number to (port)
     * @param port the new port to use
     */
   void SetPort(uint16 port) {_port = port;}

   /** Resets this object to its default state; as if it had just been created by the default constructor. */
   void Reset() {_ip = invalidIP; _port = 0;}

   /** Returns true iff both our IP address and port number are valid (ie non-zero) */
   MUSCLE_NODISCARD bool IsValid() const {return ((_ip != invalidIP)&&(_port != 0));}

   /** Sets this object's state from the passed-in character string.
     * IPv4 address may be of the form "192.168.1.102", or of the form "192.168.1.102:2960".
     * As long as -DMUSCLE_AVOID_IPV6 is not defined, IPv6 address (eg "::1") are also supported.
     * Note that if you want to specify an IPv6 address and a port number at the same
     * time, you will need to enclose the IPv6 address in brackets, like this:  "[::1]:2960"
     * @param s The user-readable IP-address string, with optional port specification
     * @param defaultPort What port number to assign if the string does not specify a port number.
     * @param allowDNSLookups If true, this function will try to interpret non-numeric host names
     *                        eg "www.google.com" by doing a DNS lookup.  If false, then no
     *                        name resolution will be attempted, and only numeric IP addesses will be parsed.
     */
   void SetFromString(const String & s, uint16 defaultPort, bool allowDNSLookups);

   /** Returns a string representation of this object, similar to the forms
     * described in the SetFromString() documentation, above.
     * @param includePort If true, the port will be included in the string.  Defaults to true.
     * @param printIPv4AddressesInIPv4Style If set true, then IPv4 addresses will be returned as eg "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
     *                                      Defaults to true.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
     * @param expandScopeIndicesToNetworkInterfaceNames If set to true, IPv6 addresses with multicast-scope annotations (e.g. fe80::123@5)
     *                                      will result in a string with the multicast-scope annotation returned as its interface-name
     *                                      (e.g. "fe80::123%en0") instead of just the scope ID integer.  Enabling this option does make this
     *                                      call significantly more expensive (since it may need to iterate through the local NICs listing)
     *                                      so this argument defaults to false.
     */
   String ToString(bool includePort = true, bool printIPv4AddressesInIPv4Style = true, bool expandScopeIndicesToNetworkInterfaceNames = false) const;

   /** Convenience method for producing a URL (e.g. "http://127.0.0.1:8080") string from this IPAddressAndPort
     * @param handler the handler-type to include (e.g. "http" or "https")
     * @returns the computed URL string, or an empty String if this IPAddressAndPort's IPAddress isn't valid
     */
   String ToURL(const String & handler) const;

   /** Convenience method:  Writes our current state out to (writeToSockAddrIn), if possible.
     * @param writeToSockAddrIn on return, this object's fields will be filled out based on our state.
     * @note a sockaddr_in cannot hold an IPv6 address, only an IPv4 address, so calling this method on an
     *                     IPAddressAndPort object that contains an IPv6 address will not produce a usable result.
     */
   void WriteToSockAddrIn(struct sockaddr_in & writeToSockAddrIn) const;

#ifndef MUSCLE_AVOID_IPV6
   /** Convenience method:  Writes our current state out to (writeToSockAddrIn6), if possible.
     * @param writeToSockAddrIn6 on return, this object's fields will be filled out based on our state.
     */
   void WriteToSockAddrIn6(struct sockaddr_in6 & writeToSockAddrIn6) const;
#endif

   /** Part of the Flattenable pseudo-interface:  Returns true */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}

   /** Part of the Flattenable pseudo-interface:  Returns IP_ADDRESS_AND_PORT_TYPE */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 TypeCode() {return IP_ADDRESS_AND_PORT_TYPE;}

   /** Part of the Flattenable pseudo-interface */
   MUSCLE_NODISCARD static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return IPAddress::FlattenedSize() + sizeof(uint16);}

   /** @copydoc DoxyTemplate::CalculateChecksum() const */
   MUSCLE_NODISCARD uint32 CalculateChecksum() const {return _ip.CalculateChecksum() + _port;}

   /** Copies this point into an endian-neutral flattened buffer.
    *  @param flat the DataFlattener to use to output serialized bytes
    */
   void Flatten(DataFlattener flat) const;

   /** Restores this point from an endian-neutral flattened buffer.
    *  @param unflat the DataUnflattener to use to read the flattened bytes.
    *  @return B_NO_ERROR on success, B_BAD_DATA on failure (size was too small)
    */
   status_t Unflatten(DataUnflattener & unflat);

   /** Convenience method:  Returns an IPAddressAndPort object identical to this one,
     * except that the included IPAddress has its interface index field set to the specified value.
     * @param interfaceIndex The new interface index value to use in the returned object (or MUSCLE_NO_LIMIT to specify an invalid index)
     */
   IPAddressAndPort WithInterfaceIndex(uint32 interfaceIndex) const {return IPAddressAndPort(_ip.WithInterfaceIndex(interfaceIndex), _port);}

   /** Convenience method:  Returns an IPAddressAndPort object identical to this one, but with the interface-index field unset. */
   IPAddressAndPort WithoutInterfaceIndex() const {return WithInterfaceIndex(MUSCLE_NO_LIMIT);}

private:
   IPAddress _ip;
   uint16 _port;
};

} // end namespace muscle

#endif

