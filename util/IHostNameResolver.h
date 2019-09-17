/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef IHostNameResolver_h
#define IHostNameResolver_h

#include "util/IPAddress.h"
#include "util/RefCount.h"

namespace muscle {

/** Interface for a function that knows how to resolve a host name to an IP address */
class IHostNameResolver : public RefCountable
{
public:
   /** Default constructor.  */
   IHostNameResolver() {/* empty */}

   /** Destructor.  */
   virtual ~IHostNameResolver() {/* empty */}
  
   /** Called by GetHostByName() when it wants to resolve a host name (e.g. "www.google.com" or "blah.local.") into an IPAddress.
     * @param name the hostname string to resolve
     * @param expandLocalhost true iff the caller would prefer that "localhost" should be expanded to a globally meaningful IP address; false if e.g. 127.0.0.1 or ::1 are preferable.
     * @param preferIPv6 true iff the caller would prefer an IPv6 address; false if an IPv4 address is preferable.
     * @param retIPAddress on success, the IP address to use should be written here (or an invalid/default-constructed IP address if you want the lookup to fail).
     * @return B_NO_ERROR if the IPAddress written to the (retIPAddress) argument should be used, or an error code (e.g. B_ERROR) if the hostname lookup should be continued by other means if possible.
     * @note that this callback may be called from multiple threads concurrently (e.g. if multiple threads are calling GetHostByName() concurrently)
     */
   virtual status_t GetIPAddressForHostName(const char * name, bool expandLocalhost, bool preferIPv6, IPAddress & retIPAddress) = 0;
};
DECLARE_REFTYPES(IHostNameResolver);

} // end namespace muscle

#endif

