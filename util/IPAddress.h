/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef IPAddress_h
#define IPAddress_h

#include "support/Flattenable.h"
#include "util/String.h"

namespace muscle {

enum {
   IP_ADDRESS_TYPE          = 1230004063, // 'IP__' 
   IP_ADDRESS_AND_PORT_TYPE = 1230004560  // 'IPaP' 
};

/** This class represents an IPv6 network address, including the 128-bit IP
  * address and the interface index field (necessary for connecting to link-local addresses)
  */
class IPAddress MUSCLE_FINAL_CLASS : public PseudoFlattenable
{
public:
   IPAddress(uint64 lowBits = 0, uint64 highBits = 0, uint32 interfaceIndex = 0) : _lowBits(lowBits), _highBits(highBits), _interfaceIndex(interfaceIndex) {/* empty */}
   IPAddress(const IPAddress & rhs) : _lowBits(rhs._lowBits), _highBits(rhs._highBits), _interfaceIndex(rhs._interfaceIndex) {/* empty */}

   IPAddress & operator = (const IPAddress & rhs) {_lowBits = rhs._lowBits; _highBits = rhs._highBits; _interfaceIndex = rhs._interfaceIndex; return *this;}

   bool EqualsIgnoreInterfaceIndex(const IPAddress & rhs) const {return ((_lowBits == rhs._lowBits)&&(_highBits == rhs._highBits));}
   bool operator ==               (const IPAddress & rhs) const {return ((EqualsIgnoreInterfaceIndex(rhs))&&(_interfaceIndex == rhs._interfaceIndex));}
   bool operator !=               (const IPAddress & rhs) const {return !(*this == rhs);}

   bool operator < (const IPAddress & rhs) const 
   {
      if (_highBits < rhs._highBits) return true;
      if (_highBits > rhs._highBits) return false;
      if (_lowBits  < rhs._lowBits)  return true;
      if (_lowBits  > rhs._lowBits)  return false;
      return (_interfaceIndex < rhs._interfaceIndex);
   }

   bool operator > (const IPAddress & rhs) const 
   {
      if (_highBits < rhs._highBits) return false;
      if (_highBits > rhs._highBits) return true;
      if (_lowBits  < rhs._lowBits)  return false;
      if (_lowBits  > rhs._lowBits)  return true;
      return (_interfaceIndex > rhs._interfaceIndex);
   }

   bool operator <= (const IPAddress & rhs) const {return (*this == rhs)||(*this < rhs);}
   bool operator >= (const IPAddress & rhs) const {return (*this == rhs)||(*this > rhs);}

   IPAddress operator & (const IPAddress & rhs) {return IPAddress(_lowBits & rhs._lowBits, _highBits & rhs._highBits, _interfaceIndex);}
   IPAddress operator | (const IPAddress & rhs) {return IPAddress(_lowBits | rhs._lowBits, _highBits | rhs._highBits, _interfaceIndex);}
   IPAddress operator ~ () {return IPAddress(~_lowBits, ~_highBits, _interfaceIndex);}

   IPAddress operator &= (const IPAddress & rhs) {*this = *this & rhs; return *this;}
   IPAddress operator |= (const IPAddress & rhs) {*this = *this | rhs; return *this;}

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

   /** Part of the Flattenable pseudo-interface:  Returns true */
   static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}

   /** Part of the Flattenable pseudo-interface:  Returns IP_ADDRESS_TYPE */
   static MUSCLE_CONSTEXPR uint32 TypeCode() {return IP_ADDRESS_TYPE;}

   /** Returns true iff (tc) equals IP_ADDRESS_TYPE. */
   static MUSCLE_CONSTEXPR bool AllowsTypeCode(uint32 tc) {return (TypeCode()==tc);}

   /** Part of the Flattenable pseudo-interface */
   static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return sizeof(uint64) + sizeof(uint64) + sizeof(uint32);}

   /** Returns a 32-bit checksum for this object. */
   uint32 CalculateChecksum() const;

   /** Copies this point into an endian-neutral flattened buffer.
    *  @param buffer Points to an array of at least FlattenedSize() bytes.
    */
   void Flatten(uint8 * buffer) const;

   /** Restores this point from an endian-neutral flattened buffer.
    *  @param buffer Points to an array of (size) bytes
    *  @param size The number of bytes (buffer) points to (should be at least FlattenedSize())
    *  @return B_NO_ERROR on success, B_ERROR on failure (size was too small)
    */
   status_t Unflatten(const uint8 * buffer, uint32 size);

   /** Convenience method:  Returns true iff this is a valid IP address
     * (where "valid" in this case means non-zero, or if MUSCLE_AVOID_IPV6 is defined,
     * it means that the high-bits are all zero, the upper 32-bits of the low-bits is zero,
     * and the lower 32-bits of the low-bits are non-zero.
     */
   bool IsValid() const;

   /** Returns true iff this address qualifies as an IPv4 address.
     * (If MUSCLE_AVOID_IPV6 is defined, this method always returns true)
     */
   bool IsIPv4() const;

   /** Returns true iff this address qualifies as a multicast address.  */
   bool IsMulticast() const;

   /** Returns true iff this address qualifies as a standard loopback-device address (e.g. 127.0.0.1 or ::1 or fe80::1) */
   bool IsStandardLoopbackDeviceAddress() const;

   /** Returns a human-readable string equivalent to this IPAddress object.  Behaves the same as Inet_NtoA(*this, preferIPv4Style).
     *  @param preferIPv4Style If set true, then IPv4 addresses will be returned as e.g. "192.168.1.1", not "::192.168.1.1" or "::ffff:192.168.1.1".
     *                         Defaults to false.  If MUSCLE_AVOID_IPV6 is defined, then this argument isn't used.
     */
   String ToString(bool preferIPv4Style = false) const;

   /** Given a human-readable IP-address string (as returned by ToString()), sets this IPAddress object to the
     * value represented by the string.  Behaves similarly to Inet_AtoN()
     * @param ipAddressString The string to parse
     * @returns B_NO_ERROR on success, or B_ERROR if the String could not be parsed.
     */
   status_t SetFromString(const String & ipAddressString);

   /** Sets us to the IPv4 address specified by (bits)
     * @param bits a 32-bit representation of an IPv4 address.
     */
   void SetIPv4AddressFromUint32(uint32 bits) {_lowBits = bits; _highBits = 0; _interfaceIndex = 0;}

   /** Returns our IPv4 address as a uint32.  
     * If we represent something other than an IPv4 address, the return value is undefined.
     */
   uint32 GetIPv4AddressAsUint32() const {return ((uint32)(_lowBits & 0xFFFFFFFF));}

private:
   void WriteToNetworkArrayAux( uint8 * out, const uint64 & in ) const {uint64 tmp = B_HOST_TO_BENDIAN_INT64(in); muscleCopyOut(out, tmp);}
   void ReadFromNetworkArrayAux(const uint8 * in, uint64 & out) const {uint64 tmp; muscleCopyIn(tmp, in); out = B_BENDIAN_TO_HOST_INT64(tmp);}

   uint64 _lowBits;
   uint64 _highBits;
   uint32 _interfaceIndex;
};
typedef IPAddress ip_address;  // for backwards compatibility with old code

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
class IPAddressAndPort MUSCLE_FINAL_CLASS : public PseudoFlattenable
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
   IPAddressAndPort(const IPAddress & ip, uint16 port) : _ip(ip), _port(port) {/* empty */}

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
   uint32 HashCode() const {return _ip.HashCode()+_port;}

   /** Returns this object's current IP address */
   const IPAddress & GetIPAddress() const {return _ip;}

   /** Returns this object's current port number */
   uint16 GetPort() const {return _port;}

   /** Sets this object's IP address to (ip) */
   void SetIPAddress(const IPAddress & ip) {_ip = ip;}

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

   /** Part of the Flattenable pseudo-interface:  Returns true */
   static MUSCLE_CONSTEXPR bool IsFixedSize() {return true;}

   /** Part of the Flattenable pseudo-interface:  Returns IP_ADDRESS_AND_PORT_TYPE */
   static MUSCLE_CONSTEXPR uint32 TypeCode() {return IP_ADDRESS_AND_PORT_TYPE;}

   /** Returns true iff (tc) equals IP_ADDRESS_AND_PORT_TYPE. */
   static MUSCLE_CONSTEXPR bool AllowsTypeCode(uint32 tc) {return (TypeCode()==tc);}

   /** Part of the Flattenable pseudo-interface */
   static MUSCLE_CONSTEXPR uint32 FlattenedSize() {return IPAddress::FlattenedSize() + sizeof(uint16);}

   /** Returns a 32-bit checksum for this object. */
   uint32 CalculateChecksum() const {return _ip.CalculateChecksum() + _port;}

   /** Copies this point into an endian-neutral flattened buffer.
    *  @param buffer Points to an array of at least FlattenedSize() bytes.
    */
   void Flatten(uint8 * buffer) const;

   /** Restores this point from an endian-neutral flattened buffer.
    *  @param buffer Points to an array of (size) bytes
    *  @param size The number of bytes (buffer) points to (should be at least FlattenedSize())
    *  @return B_NO_ERROR on success, B_ERROR on failure (size was too small)
    */
   status_t Unflatten(const uint8 * buffer, uint32 size);

#ifndef MUSCLE_AVOID_IPV6
   /** Convenience method:  Returns an IPAddressAndPort object identical to this one,
     * except that the include IPAddress has its interface index field set to the specified value.
     * @param interfaceIndex The new interface index value to use in the returned object.
     */
   IPAddressAndPort WithInterfaceIndex(uint32 interfaceIndex) const
   {
      IPAddress addr = _ip;
      addr.SetInterfaceIndex(interfaceIndex);
      return IPAddressAndPort(addr, _port); 
   }
#endif

private:
   IPAddress _ip;
   uint16 _port;
};

}; // end namespace muscle

#endif

