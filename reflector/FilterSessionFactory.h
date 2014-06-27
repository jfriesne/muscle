/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleFilterSessionFactory_h
#define MuscleFilterSessionFactory_h

#include "reflector/AbstractReflectSession.h"
#include "reflector/AbstractSessionIOPolicy.h"
#include "regex/PathMatcher.h"

namespace muscle {

/** This is a convenience decorator factory; it holds a set of "ban patterns", and a
  * set of "require patterns".  It will refuse to grant access to any clients whose host 
  * IP match at least one ban pattern, or who don't match at least one require pattern
  * (if there are any require patterns)  For IP addresses that don't match a pattern, 
  * the request is passed through to the held factory.
  */
class FilterSessionFactory : public ProxySessionFactory
{
public:
   /** Constructor.
     * @param slaveRef Reference to the slave factory that will do the actual creation for us.
     * @param maxSessionsPerHost If set, this is the maximum number of simultaneous connections
     *                           we will allow to be in existence from any one host at a time.
     * @param totalMaxSessions If set, this is the maximum number of simultaneous connections
     *                         we will allow to be in existence on the server at one time.
     */
   FilterSessionFactory(const ReflectSessionFactoryRef & slaveRef, uint32 maxSessionsPerHost = MUSCLE_NO_LIMIT, uint32 totalMaxSessions = MUSCLE_NO_LIMIT);

   /** Destructor */
   virtual ~FilterSessionFactory();

   /** Checks to see if the new session meets all our acceptance criteria.
     * If so, the call is passed through to our held factory;  if not, it's "access denied" time, and we return NULL.
     * @param clientAddress A string representing the connecting client's host (typically an IP address, e.g. "192.168.1.102")
     * @param factoryInfo the IP address and port of the network interface that accepted the connection
     * @returns A reference to a new session object on approval, or a NULL reference on denial or error.
     */
   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);

   /** Implemented to handle PR_COMMAND_(ADD/REMOVE)(BANS/REQUIRES) messages from our sessions */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msgRef, void * userData);       
   /** Add a new ban pattern to our set of ban patterns 
     * @param banPattern Pattern to match against (e.g. "192.168.0.*")
     * @return B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t PutBanPattern(const String & banPattern);

   /** Add a new require pattern to our set of require patterns
     * @param requirePattern Pattern to match against (e.g. "192.168.0.*")
     * @return B_NO_ERROR on success, or B_ERROR on failure (out of memory?)
     */
   status_t PutRequirePattern(const String & requirePattern);

   /** Remove the first matching instance of (banPattern) from our set of ban patterns.
     * Note that we don't do any pattern matching here, we will remove exactly one ban
     * pattern that is exactly equal to the supplied argument.
     * @param requirePattern Pattern to remove from the set of ban patterns
     * @return B_NO_ERROR on success, or B_ERROR if the given pattern wasn't found in the set.
     */
   status_t RemoveBanPattern(const String & requirePattern);

   /** Remove the first matching instance of (requirePattern) from our set of require patterns.
     * Note that we don't do any pattern matching here, we will remove exactly one require
     * pattern that is exactly equal to the supplied argument.
     * @param requirePattern Pattern to remove from the set of require patterns
     * @return B_NO_ERROR on success, or B_ERROR if the given pattern wasn't found in the set.
     */
   status_t RemoveRequirePattern(const String & requirePattern);

   /** Removes all ban patterns who match the given regular expression.
     * @param exp Expression to match on.
     */
   void RemoveMatchingBanPatterns(const String & exp);

   /** Removes all require patterns who match the given regular expression.
     * @param exp Expression to match on.
     */
   void RemoveMatchingRequirePatterns(const String & exp);

   /** Sets the input-bandwidth-allocation policy to apply to sessions that we create */
   void SetInputPolicy(const AbstractSessionIOPolicyRef & ref) {_inputPolicyRef = ref;}

   /** Sets the output-bandwidth-allocation policy to apply to sessions that we create */
   void SetOutputPolicy(const AbstractSessionIOPolicyRef & ref) {_outputPolicyRef = ref;}

   /** Sets the new max-sessions-per-host limit -- i.e. how many sessions from any given IP address
     * may be connected to our server concurrently.
     */
   void SetMaxSessionsPerHost(uint32 maxSessionsPerHost) {_maxSessionsPerHost = maxSessionsPerHost;}

   /** Sets the new total-max-sessions limit -- i.e. how many sessions may be connected to our server concurrently.  */
   void SetTotalMaxSessions(uint32 maxSessions) {_totalMaxSessions = maxSessions;}

   /** Returns the current max-sessions-per-host limit  */
   uint32 GetMaxSessionsPerHost() const {return _maxSessionsPerHost;}

   /** Sets the current total-max-sessions limit -- i.e. how many sessions may be connected to our server concurrently.  */
   uint32 GetTotalMaxSessions() const {return _totalMaxSessions;}

private:
   Hashtable<String, StringMatcherRef> _bans;
   Hashtable<String, StringMatcherRef> _requires;
   AbstractReflectSession * _tempLogFor;

   AbstractSessionIOPolicyRef _inputPolicyRef;
   AbstractSessionIOPolicyRef _outputPolicyRef;

   uint32 _maxSessionsPerHost;
   uint32 _totalMaxSessions;
};

}; // end namespace muscle

#endif
