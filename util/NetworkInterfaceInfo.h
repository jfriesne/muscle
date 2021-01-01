/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef NetworkInterfaceInfo_h
#define NetworkInterfaceInfo_h

#include "support/BitChord.h"
#include "util/IPAddress.h"
#include "util/Queue.h"
#include "util/String.h"

namespace muscle {

/** A list of possible network-interface-hardware-type values that may be returned by NetworkInterfaceInfo::GetHardwareType() */
enum {
   NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN = 0, /**< We weren't able to identify this interface as any of our recognized types */
   NETWORK_INTERFACE_HARDWARE_TYPE_LOOPBACK,    /**< Loopback interface (e.g. lo0), used to communicate within localhost only */
   NETWORK_INTERFACE_HARDWARE_TYPE_ETHERNET,    /**< Your standard wired Ethernet interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_WIFI,        /**< IEEE802.11/Wi-Fi wireless medium-range interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_TOKENRING,   /**< Token Ring interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_PPP,         /**< Point to Point Protocol interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_ATM,         /**< Asynchronous Transfer Mode interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_TUNNEL,      /**< Tunnel/encapsulation interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_BRIDGE,      /**< Bridge interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_FIREWIRE,    /**< IEEE1394/FireWire interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_BLUETOOTH,   /**< Bluetooth short-range wireless interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_BONDED,      /**< Virtual interface representing several other interfaces bonded together */
   NETWORK_INTERFACE_HARDWARE_TYPE_IRDA,        /**< IrDA line-of-sight infrared short-range wireless interface */ 
   NETWORK_INTERFACE_HARDWARE_TYPE_DIALUP,      /**< Phone-line dialup modem interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_SERIAL,      /**< Networking via serial line */
   NETWORK_INTERFACE_HARDWARE_TYPE_VLAN,        /**< VLAN interface */
   NETWORK_INTERFACE_HARDWARE_TYPE_CELLULAR,    /**< Cellular network long-range wireless interface */

   // more types may be inserted here in the future!
   NUM_NETWORK_INTERFACE_HARDWARE_TYPES         /**< Guard value (useful when iterating over all known types) */
};

/** Bits that can be passed to GetNetworkInterfaceInfos() or GetNetworkInterfaceAddresses(). */
enum {
   GNII_FLAG_INCLUDE_IPV4_INTERFACES = 0,                     /**< If set, IPv4-specific interfaces will be returned */
   GNII_FLAG_INCLUDE_IPV6_INTERFACES,                         /**< If set, IPv6-specific interfaces will be returned */
   GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES,                     /**< If set, loopback interfaces (e.g. lo0/127.0.0.1) will be returned */
   GNII_FLAG_INCLUDE_NONLOOPBACK_INTERFACES,                  /**< If set, non-loopback interfaces (e.g. en0) will be returned */
   GNII_FLAG_INCLUDE_ENABLED_INTERFACES,                      /**< If set, enabled (aka "up") interfaces will be returned */
   GNII_FLAG_INCLUDE_DISABLED_INTERFACES,                     /**< If set, disabled (aka "down") interfaces will be returned */
   GNII_FLAG_INCLUDE_LOOPBACK_INTERFACES_ONLY_AS_LAST_RESORT, /**< If set, loopback interfaces will be returned only if no other interfaces are found */
   GNII_FLAG_INCLUDE_UNADDRESSED_INTERFACES,                  /**< If set, we'll include even interfaces that don't have a valid IP address */
   // [future flags could go here...]
   NUM_GNII_FLAGS,                                            /**< Guard value */

   // For convenience, GNII_FLAG_INCLUDE_MUSCLE_PREFERRED_INTERFACES will specify interfaces of the family specified by MUSCLE_AVOID_IPV6's presence/abscence.
#ifdef MUSCLE_AVOID_IPV6
   GNII_FLAG_INCLUDE_MUSCLE_PREFERRED_INTERFACES = GNII_FLAG_INCLUDE_IPV4_INTERFACES, /**< If set, IPv4-specific or IPv6-specific interfaces will be returned (depending on whether MUSCLE_AVOID_IPV6 was specified during compilation) */
#else
   GNII_FLAG_INCLUDE_MUSCLE_PREFERRED_INTERFACES = GNII_FLAG_INCLUDE_IPV6_INTERFACES, /**< If set, IPv4-specific or IPv6-specific interfaces will be returned (depending on whether MUSCLE_AVOID_IPV6 was specified during compilation) */
#endif
};
DECLARE_BITCHORD_FLAGS_TYPE(GNIIFlags, NUM_GNII_FLAGS);

#define GNII_FLAGS_INCLUDE_ALL_INTERFACES (GNIIFlags::WithAllBitsSet())  /**< If set, all interfaces will be returned */
#define GNII_FLAGS_INCLUDE_ALL_ADDRESSED_INTERFACES (GNIIFlags::WithAllBitsSetExceptThese(GNII_FLAG_INCLUDE_UNADDRESSED_INTERFACES))  /**< default setting -- all interfaces that currently have an IP address will be returned */

/** This little container class is used to return data from the GetNetworkInterfaceInfos() function, below */
class NetworkInterfaceInfo MUSCLE_FINAL_CLASS
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
     * @param copper True iff the interface is currently operations (e.g. has a connected ethernet cable plugged into it)
     * @param macAddress 48-bit MAC address value, or 0 if MAC address is unknown.
     * @param hardwareType a NETWORK_INTERFACE_HARDWARE_TYPE_* value (NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN if the hardware type isn't known)
     */
   NetworkInterfaceInfo(const String & name, const String & desc, const IPAddress & ip, const IPAddress & netmask, const IPAddress & broadcastIP, bool enabled, bool copper, uint64 macAddress, uint32 hardwareType);

   /** Returns the name of this interface, or "" if the name is not known. */
   const String & GetName() const {return _name;}

   /** Returns a (human-readable) description of this interface, or "" if a description is unavailable. */
   const String & GetDescription() const {return _desc;}

   /** Returns the IP address of this interface */
   const IPAddress & GetLocalAddress() const {return _ip;}

   /** Returns the netmask of this interface */
   const IPAddress & GetNetmask() const {return _netmask;}

   /** If this interface is a point-to-point interface, this method returns the IP
     * address of the machine at the remote end of the interface.  Otherwise, this
     * method returns the broadcast address for this interface.
     */
   const IPAddress & GetBroadcastAddress() const {return _broadcastIP;}

   /** Returns the MAC address of this network interface, or 0 if the MAC address isn't known.
     * Note that only the lower 48 bits of the returned 64-bit word are valid; the upper 16 bits will always be zero.
     * @note This functionality is currently implemented under BSD/MacOSX, Linux, and Windows.  Under other OS's where
     *       this information isn't implemented, this method will return 0.
     */
   uint64 GetMACAddress() const {return _macAddress;}

   /** Returns a NETWORK_INTERFACE_HARDWARE_TYPE_* values describing the type of networking hardware this interface corresponds to.
     * @note that this functionality is currently implemented for MacOS/X, Linux, and Windows only.
     * Under other OS's this method currently only returns NETWORK_INTERFACE_HARDWARE_TYPE_UNKNOWN
     * or NETWORK_INTERFACE_HARDWARE_LOOPBACK.
     */
   uint32 GetHardwareType() const {return _hardwareType;}

   /** Returns true iff this interface is currently enabled ("up"). */
   bool IsEnabled() const {return _enabled;}

   /** Returns true iff this network interface is currently plugged in to anything 
     * (i.e. iff a connected Ethernet cable is attached to the Ethernet jack).
     */
   bool IsCopperDetected() const {return _copper;}

   /** For debugging.  Returns a human-readable string describing this interface. */
   String ToString() const;

   /** @copydoc DoxyTemplate::HashCode() const */
   uint32 HashCode() const;

   /** Given a NETWORK_INTERFACE_HARDWARE_TYPE_* value, returns a human-readable string describing the type (e.g. "Ethernet" or "WiFi")
     * @param hardwareType a NETWORK_INTERFACE_HARDWARE_TYPE_* value
     */
   static const char * GetNetworkHardwareTypeString(uint32 hardwareType);

   /** Comparison Operator.  Returns true iff the two NetworkInterfaceInfos contain the same data as each other in all fields.
     * @param rhs A NetworkInterfaceInfo to compare ourself with
     */
   bool operator == (const NetworkInterfaceInfo & rhs) const;

   /** Comparison Operator.  Returns true iff the two NetworkInterfaceInfos do not contain the same data as each other in all fields.
     * @param rhs A NetworkInterfaceInfo to compare ourself with
     */
   bool operator != (const NetworkInterfaceInfo & rhs) const {return !(*this==rhs);}

private:
   friend status_t GetNetworkInterfaceInfos(Queue<NetworkInterfaceInfo> & results, GNIIFlags includeFlags);  // so it can set the _macAddress field

   String _name;
   String _desc;
   IPAddress _ip;
   IPAddress _netmask;
   IPAddress _broadcastIP;
   bool _enabled;
   bool _copper;
   uint64 _macAddress;
   uint32 _hardwareType;
};

/** This function queries the local OS for information about all available network
  * interfaces.  Note that this method is only implemented for some OS's (Linux,
  * MacOS/X, Windows), and that on other OS's it may just always return B_UNIMPLEMENTED.
  * @param results On success, zero or more NetworkInterfaceInfo objects will
  *                be added to this Queue for you to look at.
  * @param includeFlags A chord of GNII_FLAG_INCLUDE_* bits indicating which types of network interface you want to be
  *                    included in the returned list.  Defaults to GNII_FLAG_INCLUDE_ALL_ADDRESSED_INTERFACES, which
  *                    indicates that any interface with an IPv4 or IPv6 interface should be included.
  *                    (Note:  if you need to specify this argument explicitly, the syntax for the BitChord constructor
  *                    is e.g. like this:  GNIIFlags(GNII_FLAG_INCLUDE_IPV4_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,...))
  * @returns B_NO_ERROR on success, or an errorcode on failure (out of memory, call not implemented for the current OS, etc)
  */
status_t GetNetworkInterfaceInfos(Queue<NetworkInterfaceInfo> & results, GNIIFlags includeFlags = GNII_FLAGS_INCLUDE_ALL_ADDRESSED_INTERFACES);

/** This is a more limited version of GetNetworkInterfaceInfos(), included for convenience.
  * Instead of returning all information about the local host's network interfaces, this
  * one returns only their IP addresses.  It is the same as calling GetNetworkInterfaceInfos()
  * and then iterating the returned list to assemble a list only of the IP addresses returned
  * by GetBroadcastAddress().
  * @param retAddresses On success, zero or more IPAddresses will be added to this Queue for you to look at.
  * @param includeFlags A chord of GNII_FLAG_INCLUDE_* bits indicating which types of network interface you want to be
  *                    included in the returned list.  Defaults to GNII_FLAG_INCLUDE_ALL_ADDRESSED_INTERFACES, which 
  *                    indicates that any interface with an IPv4 or IPv6 interface should be included.
  *                    (Note:  if you need to specify this argument explicitly, the syntax for the BitChord constructor
  *                    is e.g. like this:  GNIIFlags(GNII_FLAG_INCLUDE_IPV4_INTERFACES,GNII_FLAG_INCLUDE_IPV6_INTERFACES,...))
  * @returns B_NO_ERROR on success, or an errorcode on failure (out of memory,
  *          call not implemented for the current OS, etc)
  */
status_t GetNetworkInterfaceAddresses(Queue<IPAddress> & retAddresses, GNIIFlags includeFlags = GNII_FLAGS_INCLUDE_ALL_ADDRESSED_INTERFACES);

} // end namespace muscle

#endif

