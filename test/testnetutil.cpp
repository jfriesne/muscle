/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/Socket.h"

using namespace muscle;

class TestHostNameResolver : public IHostNameResolver
{
public:
   explicit TestHostNameResolver(int pri) : _pri(pri) {/* empty */}

   virtual status_t GetIPAddressForHostName(const char * name, bool expandLocalhost, bool preferIPv6, IPAddress & retIPAddress)
   {
      (void) retIPAddress;  // avoid compiler warning
      printf("TestHostNameResolver (priority %i):  name=[%s] expandLocalhost=%i preferIPv6=%i\n", _pri, name, expandLocalhost, preferIPv6);
      return B_ERROR("Artificially induced error");
   }

private:
   const int _pri;
};

static void TestGetHostByName(const char * hostname)
{
   IPAddress addr = GetHostByName(hostname);
   printf("GetHostByName(%s) returned %s\n", hostname, addr.ToString()());
}

int main(int, char **)
{
   CompleteSetupSystem css;

   printf("Querying local host's IP addresses:\n");

   printf("\n");

   Queue<NetworkInterfaceInfo> ifs;
   if (GetNetworkInterfaceInfos(ifs).IsOK())
   {
      printf("Found " UINT32_FORMAT_SPEC " local network interfaces:\n", ifs.GetNumItems());
      for (uint32 i=0; i<ifs.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = ifs[i];
         printf("  #" UINT32_FORMAT_SPEC ":  %s\n", i+1, nii.ToString()());
      }
   }
   else printf("GetNetworkInterfaceInfos() returned an error!\n");

   // This is mainly to make sure the callbacks get executed in descending-priority-order
   (void) PutHostNameResolver(IHostNameResolverRef(new TestHostNameResolver( 0)),  0);
   (void) PutHostNameResolver(IHostNameResolverRef(new TestHostNameResolver( 1)),  1);
   (void) PutHostNameResolver(IHostNameResolverRef(new TestHostNameResolver(-2)), -2);
   (void) PutHostNameResolver(IHostNameResolverRef(new TestHostNameResolver(-1)), -1);
   (void) PutHostNameResolver(IHostNameResolverRef(new TestHostNameResolver( 2)),  2);

   printf("\n\nTesting resolver callbacks...\n");
   TestGetHostByName("www.google.com");
   TestGetHostByName("127.0.0.1");
   TestGetHostByName("localhost");
   TestGetHostByName("foobar.local.");
   TestGetHostByName("obviously_broken.wtf.blah");

   printf("\n\nTesting IPAddressAndPort parsing... (defaulting to port 666 when port is unspecified)\n");
   char buf[256];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s(buf); s = s.Trim();
      printf("You typed %s ... I interpreted that as %s\n", s(), IPAddressAndPort(s,666,true).ToString(true,true)()); fflush(stdout);
   }

   return 0;
}
