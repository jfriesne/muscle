/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "util/SharedFilterSessionFactory.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/MiscUtilityFunctions.h"

namespace muscle {

SharedFilterSessionFactory :: SharedFilterSessionFactory(const ReflectSessionFactoryRef & slaveRef, const String & sharedMemName, bool isGrantList, bool defaultPass)
   : ProxySessionFactory(slaveRef)
   , _sharedMemName(sharedMemName)
   , _isGrantList(isGrantList)
   , _defaultPass(defaultPass)
{
   // empty
}

SharedFilterSessionFactory :: ~SharedFilterSessionFactory()
{
   // empty
}

AbstractReflectSessionRef SharedFilterSessionFactory :: CreateSession(const String & clientIP, const IPAddressAndPort & iap)
{
   TCHECKPOINT;
   return ((GetSlave()())&&(IsAccessAllowedForIP(iap.GetIPAddress().IsStandardLoopbackDeviceAddress()?localhostIP:Inet_AtoN(clientIP())))) ? GetSlave()()->CreateSession(clientIP, iap) : AbstractReflectSessionRef();
}

static bool IsMemoryAllZeros(const uint8 * mem, uint32 numBytes)
{
   for (uint32 i=0; i<numBytes; i++) if (mem[i] != 0) return false;
   return true;
}

bool SharedFilterSessionFactory :: IsAccessAllowedForIP(const IPAddress & ip) const
{
   bool allowAccess = _defaultPass;
   if (ip != invalidIP)
   {
      // demand-setup the shared-memory object, and then lock it for read-only access
      if (((_sharedMemory.GetAreaSize() > 0)||(_sharedMemory.SetArea(_sharedMemName(), 0, false).IsOK()))&&(_sharedMemory.LockAreaReadOnly().IsOK()))
      {
         Queue<NetworkInterfaceInfo> ifs;
         bool gotIFs = false;  // we'll demand-allocate them

         // FogBugz #17090 special case:  If the memory-area is all-zeroes, then that means we will stick with our _defaultPass logic
         if (!IsMemoryAllZeros(_sharedMemory(), _sharedMemory.GetAreaSize()))
         {
            allowAccess = !_isGrantList;  // if there is a list, you're off it unless you're on it!

            const IPAddress * ips = reinterpret_cast<const IPAddress *>(_sharedMemory());
            const uint32 numIPs = _sharedMemory.GetAreaSize()/sizeof(IPAddress);
            for (uint32 i=0; i<numIPs; i++)
            {
               IPAddress nextIP = ips[i];
#ifdef MUSCLE_AVOID_IPV6
               if (nextIP == ip)
#else
               if (nextIP.EqualsIgnoreInterfaceIndex(ip))  // FogBugz #7490
#endif
               {
                  allowAccess = _isGrantList;
                  break;
               }
               else if (nextIP.IsStandardLoopbackDeviceAddress())
               {
                  if (gotIFs == false)
                  {
                     (void) GetNetworkInterfaceInfos(ifs);
                     gotIFs = true;
                  }

                  // Special case for the localhost IP... see if it matches any of our localhost's known IP addresses
                  bool matchedLocal = false;
                  for (uint32 j=0; j<ifs.GetNumItems(); j++)
                  {
#ifdef MUSCLE_AVOID_IPV6
                     if (ifs[j].GetLocalAddress() == ip)
#else
                     if (ifs[j].GetLocalAddress().EqualsIgnoreInterfaceIndex(ip))  // FogBugz #7490
#endif
                     {
                        allowAccess = _isGrantList;
                        matchedLocal = true;
                        break;
                     }
                  }
                  if (matchedLocal) break;
               }
            }
         }

         _sharedMemory.UnlockArea(); 
      }
   }
   return allowAccess;
}

} // end namespace muscle
