/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <typeinfo>   // For typeid().name()

#if defined(__GNUC__) 
# include <cxxabi.h>  // for abi::__cxa_demangle()
#endif

#include "reflector/ServerComponent.h"
#include "reflector/ReflectServer.h"

namespace muscle {

ServerComponent ::
ServerComponent()
   : _owner(NULL)
   , _fullyAttached(false)
{
   // empty
}

ServerComponent ::
~ServerComponent() 
{
   MASSERT(_owner == NULL, "ServerComponent deleted while still attached to its ReflectServer!  Maybe you did not call Cleanup() on the ReflectServer object, or did not forward an AboutToDetachFromServer() call to your superclass's implementation?");
}

static String DemangleTypeName(const char * mangled_name)
{
   String ret = mangled_name;

#if defined(__GNUC__) 
   // Stolen from Wikipedia:  https://en.wikipedia.org/wiki/Name_mangling#Standardised_name_mangling_in_C++
   int status = -1;
   char *demangled_name = abi::__cxa_demangle(ret(), NULL, NULL, &status);
   ret = demangled_name;
   free(demangled_name);
#endif

   const int32 doubleColonIdx = ret.IndexOf("::");
   return (doubleColonIdx >= 0) ? ret.Substring(doubleColonIdx+2) : ret;   // remove namespace prefix
}

status_t 
ServerComponent ::
AttachedToServer()
{
   return B_NO_ERROR;
}

const char *
ServerComponent ::
GetTypeName() const
{
   if ((_fullyAttached == false)||(_rttiTypeName.IsEmpty())) _rttiTypeName = DemangleTypeName(typeid(*this).name());
   return _rttiTypeName();
}

void
ServerComponent ::
AboutToDetachFromServer()
{
   // empty
}

Message & 
ServerComponent ::
GetCentralState() const 
{
   MASSERT(_owner, "Can not call GetCentralState() while not attached to the server");
   return _owner->GetCentralState();
}

const Hashtable<const String *, AbstractReflectSessionRef> &
ServerComponent ::
GetSessions() const
{
   MASSERT(_owner, "Can not call GetSessions() while not attached to the server");
   return _owner->GetSessions(); 
}

AbstractReflectSessionRef 
ServerComponent ::
GetSession(uint32 id) const
{
   MASSERT(_owner, "Can not call GetSession() while not attached to the server");
   return _owner->GetSession(id);
}

AbstractReflectSessionRef 
ServerComponent ::
GetSession(const String & id) const
{
   MASSERT(_owner, "Can not call GetSession() while not attached to the server");
   return _owner->GetSession(id);
}

const Hashtable<IPAddressAndPort, ReflectSessionFactoryRef> &
ServerComponent ::
GetFactories() const 
{
   MASSERT(_owner, "Can not call GetFactories() while not attached to the server");
   return _owner->GetFactories(); 
}

ReflectSessionFactoryRef 
ServerComponent ::
GetFactory(uint16 port) const
{
   MASSERT(_owner, "Can not call GetFactory() while not attached to the server");
   return _owner->GetFactory(port);
}

status_t
ServerComponent ::
AddNewSession(const AbstractReflectSessionRef & ref, const ConstSocketRef & socket)
{
   MASSERT(_owner, "Can not call AddNewSession() while not attached to the server");
   return _owner->AddNewSession(ref, socket);
}

status_t
ServerComponent ::
AddNewConnectSession(const AbstractReflectSessionRef & ref, const IPAddress & ip, uint16 port, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   MASSERT(_owner, "Can not call AddNewConnectSession() while not attached to the server");
   return _owner->AddNewConnectSession(ref, ip, port, autoReconnectDelay, maxAsyncConnectPeriod);
}

status_t
ServerComponent ::
AddNewDormantConnectSession(const AbstractReflectSessionRef & ref, const IPAddress & ip, uint16 port, uint64 autoReconnectDelay, uint64 maxAsyncConnectPeriod)
{
   MASSERT(_owner, "Can not call AddNewDormantConnectSession() while not attached to the server");
   return _owner->AddNewDormantConnectSession(ref, ip, port, autoReconnectDelay, maxAsyncConnectPeriod);
}

void
ServerComponent ::
EndServer()
{
   MASSERT(_owner, "Can not call EndServer() while not attached to the server");
   _owner->EndServer();
}

uint64
ServerComponent ::
GetServerStartTime() const
{
   MASSERT(_owner, "Can not call GetServerStartTime() while not attached to the server");
   return _owner->GetServerStartTime();
}

uint64
ServerComponent ::
GetServerSessionID() const
{
   MASSERT(_owner, "Can not call GetServerSessionID() while not attached to the server");
   return _owner->GetServerSessionID();
}

uint64 
ServerComponent ::
GetNumAvailableBytes() const
{
   MASSERT(_owner, "Can not call GetNumAvailableBytes() while not attached to the server");
   return _owner->GetNumAvailableBytes();
}
 
uint64 
ServerComponent ::
GetMaxNumBytes() const
{
   MASSERT(_owner, "Can not call GetMaxNumBytes() while not attached to the server");
   return _owner->GetMaxNumBytes();
}
 
uint64 
ServerComponent ::
GetNumUsedBytes() const
{
   MASSERT(_owner, "Can not call GetNumUsedBytes() while not attached to the server");
   return _owner->GetNumUsedBytes();
}

status_t
ServerComponent ::
PutAcceptFactory(uint16 port, const ReflectSessionFactoryRef & factoryRef, const IPAddress & optInterfaceIP, uint16 * optRetPort)
{
   MASSERT(_owner, "Can not call PutAcceptFactory() while not attached to the server");
   return _owner->PutAcceptFactory(port, factoryRef, optInterfaceIP, optRetPort);
}

status_t
ServerComponent ::
RemoveAcceptFactory(uint16 port, const IPAddress & optInterfaceIP)
{
   MASSERT(_owner, "Can not call RemoveAcceptFactory() while not attached to the server");
   return _owner->RemoveAcceptFactory(port, optInterfaceIP);
}

void 
ServerComponent ::
MessageReceivedFromSession(AbstractReflectSession &, const MessageRef &, void *)
{
   // empty
}

void 
ServerComponent ::
MessageReceivedFromFactory(ReflectSessionFactory &, const MessageRef &, void * )
{
   // empty
}

} // end namespace muscle
