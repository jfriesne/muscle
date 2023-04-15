/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleSharedFilterSessionFactory_h
#define MuscleSharedFilterSessionFactory_h

#include "reflector/AbstractReflectSession.h"
#include "reflector/AbstractSessionIOPolicy.h"
#include "regex/PathMatcher.h"
#include "system/SharedMemory.h"

namespace muscle {

/** This is a convenience decorator factory; whenever it is asked to create a session,
  * it will open the shared-memory area with the specified name (using the MUSCLE
  * SharedMemory class) and look in that area for a list of IP addresses.  The decision about
  * whether to pass the CreateSession() call on to the slave ReflectSessionFactory
  * or just fail (ie return NULL) will be made based on whether the requesting client's IP
  * address is present in that shared memory area (as an IPAddress).
  */
class SharedFilterSessionFactory : public ProxySessionFactory
{
public:
   /** Constructor.
     * @param slaveRef Reference to the slave factory that will do the actual creation for us.
     * @param sharedMemName Name of the SharedMemory area to consult
     * @param isGrantList If true, then the requesting IP address will be accepted only if
     *                    it is present in the SharedMemory area.  If false, then requesting IP
     *                    addresses in the SharedMemory area will be denied.
     * @param defaultPass Specifies what to do if the SharedMemory area doesn't exist.  If true,
     *                    then when the SharedMemory area doesn't exist, the call will be passed
     *                    on through to the slave factory.  If false, it won't be.
     */
   SharedFilterSessionFactory(const ReflectSessionFactoryRef & slaveRef, const String & sharedMemName, bool isGrantList, bool defaultPass);

   /** Destructor */
   virtual ~SharedFilterSessionFactory();

   /** Checks the SharedMemory area to see if our client's IP address is acceptable.
     * If so, the call is passed through to our held factory;  if not, it's "access denied" time, and we return NULL.
     * @param clientAddress A string representing the remote peer's host, in ASCII format (eg "192.168.1.102")
     * @param factoryInfo The IP address and port number of the local network interface on which this connection was received.
     * @returns A reference to a new session object on approval, or a NULL reference on denial or error.
     */
   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   /** Returns the name of the shared memory area to consult for a list of IP addresses. */
   MUSCLE_NODISCARD const String & GetSharedMemoryAreaName() const {return _sharedMemName;}

   /** Sets the name of the shared memory area to consult for a list of IP addresses.
     * @param name the new Shared Memory area name to use
     */
   void SetSharedMemoryAreaName(const String & name) {_sharedMemName = name;}

   /** Returns true iff IP addresses in the shared memory area are to be granted access. */
   MUSCLE_NODISCARD bool IsGrantList() const {return _isGrantList;}

   /** Sets whether IP addresses in the shared memory area are to be granted access.
     * @param igl true iff the list of IP addresses is a whitelist; false if it's a blacklist
     */
   void SetIsGrantList(bool igl) {_isGrantList = igl;}

   /** Returns true iff a missing shared memory area means all IP addresses should be granted access. */
   MUSCLE_NODISCARD bool IsDefaultPass() const {return _defaultPass;}

   /** Sets whether missing shared memory area means all IP addresses should be granted access.
     * @param dp true iff we default-grant access when the memory area is missing, false if we default-deny access in that case
     */
   void SetDefaultPass(bool dp) {_defaultPass = dp;}

   /** Convenience method:  Returns true iff access should be allowed for the given settings and IP address.
     * @param ip the IP address to inquire about
     */
   MUSCLE_NODISCARD bool IsAccessAllowedForIP(const IPAddress & ip) const;

private:
   String _sharedMemName;
   bool _isGrantList;
   bool _defaultPass;

   mutable SharedMemory _sharedMemory;
};
DECLARE_REFTYPES(SharedFilterSessionFactory);

} // end namespace muscle

#endif
