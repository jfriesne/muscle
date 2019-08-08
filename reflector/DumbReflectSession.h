/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#ifndef MuscleDumbReflectSession_h
#define MuscleDumbReflectSession_h

#include "reflector/AbstractReflectSession.h"

namespace muscle {

/**
 *  This is a factory class that returns new DumbReflectSession objects.
 */
class DumbReflectSessionFactory : public ReflectSessionFactory
{
public:
   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo);
};
DECLARE_REFTYPES(DumbReflectSessionFactory);

/** These flags govern the default routing behavior of unrecognized Message types */
enum {
   MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF      = 0x01,  /**< If this bit is set, Messages broadcast by this session will be received by this session */
   MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS = 0x02,  /**< If this bit is set, unrecognized Messages received by this session's gateway will be broadcast to all neighbors. */
   MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY = 0x04   /**< If this bit is set, unrecognized Messages received by this session's neighbors will be forwarded to this session's gateway. */ 
};

#ifndef DEFAULT_MUSCLE_ROUTING_FLAGS_BIT_CHORD
/** Default message-forwarding instructions for DumbReflectSession::MessageReceivedFromGateway() and DumbReflectSession::MessageReceivedFromSession() to follow. */
# define DEFAULT_MUSCLE_ROUTING_FLAGS_BIT_CHORD (MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS | MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY)
#endif

/** This class represents a single TCP connection between a muscled server and a client program.  
 *  This class implements a simple "reflect-all-messages-to-all-clients"
 *  message forwarding strategy, but may be subclassed to perform more complex message routing logic.
 */
class DumbReflectSession : public AbstractReflectSession
{
public:
   /** Default constructor. */
   DumbReflectSession();

   /** Destructor. */
   virtual ~DumbReflectSession();

   virtual void MessageReceivedFromGateway(const MessageRef & msg, void * userData);

   /** Called when a message is sent to us by another session (possibly this one).  
    *  @param from The session that is sending us the message.
    *  @param msg Reference to the message that we are receiving.
    *  @param userData This is a user value whose semantics are defined by the subclass.
    */
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * userData);

   /** Sets or unsets the specified default routing flag.
     * @param flag a MUSCLE_ROUTING_FLAG_* value
     * @param enable If true, the flag is set; if false, the flag is un-set.
     * Note that the default states of the flags are specified by the DEFAULT_MUSCLE_ROUTING_FLAGS_BIT_CHORD macro, defined in DumbReflectSession.h
     * Currently the default state of the flags is for all defined routing bits to be set.
     */
   void SetRoutingFlag(uint32 flag, bool enable)
   {
      if (enable) _defaultRoutingFlags |= flag;
             else _defaultRoutingFlags &= ~flag;
   }

   /** Returns true iff the specified default routing flag is set.
     * @param flag a MUSCLE_ROUTING_FLAG_* value.
     */
   bool IsRoutingFlagSet(uint32 flag) const {return ((_defaultRoutingFlags & flag) != 0);}

private:
   uint32 _defaultRoutingFlags;

   DECLARE_COUNTED_OBJECT(DumbReflectSession);
};
DECLARE_REFTYPES(DumbReflectSession);

} // end namespace muscle

#endif
