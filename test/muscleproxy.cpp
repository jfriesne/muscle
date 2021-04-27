/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "iogateway/PlainTextMessageIOGateway.h"
#include "reflector/AbstractReflectSession.h"
#include "reflector/ReflectServer.h"
#include "util/IPAddress.h"
#include "util/MiscUtilityFunctions.h"  // for ParseArgs() and HandleStandardDaemonArgs()

using namespace muscle;

// Un-comment this line to enable plain-text support for our proxy-clients.  (If left commented out, we will expect our proxy-clients to speak MUSCLE Messages to us instead)
//#define PLAIN_TEXT_CLIENT_DEMO_MODE

// This class handles TCP traffic to and from the upstream server that we are acting as a proxy for
class UpstreamSession : public AbstractReflectSession
{
public:
   explicit UpstreamSession(AbstractReflectSession * downstreamSession) : _downstreamSession(downstreamSession) {/* empty */}

   virtual bool ClientConnectionClosed()
   {
      const bool ret = AbstractReflectSession::ClientConnectionClosed();
      if ((ret)&&(_downstreamSession)) _downstreamSession->EndSession();  // if we lose our TCP connection to the upstream server, then the downstream client should go away too
      return ret;
   }

   // When we receive a Message from our upstream-server via TCP, pass it back to our DownstreamSession for him to send to the downstream client
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *) {if (_downstreamSession) _downstreamSession->MessageReceivedFromSession(*this, msg, NULL);}

   // When we get handed an incoming-from-the-client Message from our DownstreamSession, pass it on to the upstream server
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void *) 
   {
      if (&from == _downstreamSession)
      {
         (void) AddOutgoingMessage(msg);
#ifdef PLAIN_TEXT_CLIENT_DEMO_MODE
         printf("Forwarding client's text to the upstream-server as this Message:\n");
         msg()->PrintToStream();
#endif
      }
   }

   virtual void EndSession()
   {
      _downstreamSession = NULL;  // avoid potential dangling-pointer problem if our UpstreamSession is on his way out
      AbstractReflectSession::EndSession();
   }

private:
   AbstractReflectSession * _downstreamSession;  // the DownstreamSession object that created us (stored as a raw pointer to avoid a cyclic reference problem)
};
DECLARE_REFTYPES(UpstreamSession);  // defines UpstreamSessionRef type

// This class handles TCP traffic to and from a client that has connected to us
class DownstreamSession : public AbstractReflectSession
{
public:
   explicit DownstreamSession(const IPAddressAndPort & upstreamLocation) : _upstreamLocation(upstreamLocation) {/* empty */}

   virtual status_t AttachedToServer()
   {
      MRETURN_ON_ERROR(AbstractReflectSession::AttachedToServer());

      // Launch our connection to the upstream server that we will forward our client's data to
      _upstreamSession.SetRef(newnothrow UpstreamSession(this));
      MRETURN_OOM_ON_NULL(_upstreamSession());

      return AddNewConnectSession(_upstreamSession, _upstreamLocation.GetIPAddress(), _upstreamLocation.GetPort());
   }

   virtual void AboutToDetachFromServer()
   {
      if (_upstreamSession()) _upstreamSession()->EndSession();  // make sure that we we go away, the UpstreamSession we created goes away also
      AbstractReflectSession::AboutToDetachFromServer();
   }

#ifdef PLAIN_TEXT_CLIENT_DEMO_MODE
   virtual AbstractMessageIOGatewayRef CreateGateway()
   {
      AbstractMessageIOGatewayRef ret(newnothrow PlainTextMessageIOGateway);
      if (ret() == NULL) MWARN_OUT_OF_MEMORY;
      return ret;
   }
#endif

   // When we receive a Message from our downstream client via TCP, pass it on to our UpstreamSession to send to the upstream server
   virtual void MessageReceivedFromGateway(const MessageRef & msg, void *) {if (_upstreamSession()) _upstreamSession()->MessageReceivedFromSession(*this, msg, NULL);}

   // When we get handed an incoming-from-the-upstream-server Message by our UpstreamSession, pass it back to our downstream client via TCP
   virtual void MessageReceivedFromSession(AbstractReflectSession & from, const MessageRef & msg, void *) {if (&from == _upstreamSession()) (void) AddOutgoingMessage(msg);}

private:
   const IPAddressAndPort _upstreamLocation;
   UpstreamSessionRef _upstreamSession;
};

// Factory to create a new DownstreamSession whenever an incoming TCP connection is accepted
class DownstreamSessionFactory : public ReflectSessionFactory
{
public:
   explicit DownstreamSessionFactory(const IPAddressAndPort & upstreamLocation) : _upstreamLocation(upstreamLocation)
   {
      // empty
   }

   virtual AbstractReflectSessionRef CreateSession(const String & clientAddress, const IPAddressAndPort & factoryInfo)
   {
      LogTime(MUSCLE_LOG_INFO, "DownstreamSessionFactory received incoming TCP connection from [%s] on [%s]\n", clientAddress(), factoryInfo.ToString()());

      AbstractReflectSessionRef ret(newnothrow DownstreamSession(_upstreamLocation));
      if (ret() == NULL) MWARN_OUT_OF_MEMORY;
      return ret; 
   }

private:
   const IPAddressAndPort _upstreamLocation;
};

// This program accepts incoming TCP connections on port 2961 and for each incoming
// TCP connection, it makes a corresponding outgoing TCP connection to port 2960.
// Its purpose is to demonstrate how one can write a proxy using MUSCLE's ReflectServer
// class.  Run ./muscled with default arguments first, then run this program, and
// then any program that you could previously connect via MUSCLE to port 2960
// can now be connected in the same way to port 2961, through this proxy.
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   Message args; (void) ParseArgs(argc, argv, args);
   HandleStandardDaemonArgs(args);

   // Where we should connect our outgoing-proxy-connections to
   IPAddressAndPort upstreamLocation(localhostIP, 2960);
   if (args.HasName("upstream")) upstreamLocation = IPAddressAndPort(args.GetString("upstream"), 2960, true);
   if (upstreamLocation.IsValid() == false)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to parse upstream location [%s]\n", args.GetString("upstream")());
      return 10;
   }

   uint16 acceptPort = 2961;
   if (args.HasName("acceptport")) acceptPort = atoi(args.GetString("acceptport")());
   if (acceptPort == 0)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to parse acceptport value [%s]\n", args.GetString("acceptport")());
      return 10;
   }

   DownstreamSessionFactory downstreamSessionFactory(upstreamLocation);

   status_t ret;

   ReflectServer server;
   if (server.PutAcceptFactory(acceptPort, ReflectSessionFactoryRef(&downstreamSessionFactory, false)).IsOK(ret))
   {
      LogTime(MUSCLE_LOG_INFO, "reflectclientproxy:  upstream server is at [%s], accepting incoming TCP connections on port %u.\n", upstreamLocation.ToString()(), acceptPort);
      ret = server.ServerProcessLoop();
      LogTime(MUSCLE_LOG_INFO, "reflectclientproxy:  ServerProcessLoop() returned [%s], exiting\n", ret());
   }
   else LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to set up DownstreamSessionFactory on port %u!  [%s]\n", acceptPort, ret());

   server.Cleanup();

   return 0;
}
