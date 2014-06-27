/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "reflector/DumbReflectSession.h"

namespace muscle {

// This is a callback function that may be passed to the ServerProcessLoop() function.
// It creates and returns a new DumbReflectSession object.
AbstractReflectSessionRef DumbReflectSessionFactory :: CreateSession(const String &, const IPAddressAndPort &)
{
   AbstractReflectSession * ret = newnothrow DumbReflectSession;
   if (ret == NULL) WARN_OUT_OF_MEMORY;
   return AbstractReflectSessionRef(ret);
}

DumbReflectSession :: 
DumbReflectSession() : _defaultRoutingFlags(DEFAULT_MUSCLE_ROUTING_FLAGS_BIT_CHORD)
{
   // empty
}

DumbReflectSession :: 
~DumbReflectSession()
{
   // empty
}

// Called when a new message is received from our IO gateway.  We forward it on to all our server-side neighbors.
void 
DumbReflectSession :: 
MessageReceivedFromGateway(const MessageRef & msgRef, void *) 
{
   if (IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_GATEWAY_TO_NEIGHBORS)) BroadcastToAllSessions(msgRef, NULL, IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_REFLECT_TO_SELF));
}

// Called when a new message is sent to us by one of our server-side neighbors.  We forward it on to our client.
void 
DumbReflectSession :: 
MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void * /*userData*/) 
{
   if ((&from == this)||(IsRoutingFlagSet(MUSCLE_ROUTING_FLAG_NEIGHBORS_TO_GATEWAY))) (void) AddOutgoingMessage(msg);
}

}; // end namespace muscle
