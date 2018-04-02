#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/IPAddress.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program demonstrates basic usage of the muscle::IPAddress and muscle::IPAddressAndPort classes.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   // Some well-known IP addresses, exported as globals for convenience
   printf("invalidIP=[%s] (all zeroes)\n", invalidIP.ToString()());
   printf("localhostIP=[%s]\n",            localhostIP.ToString()());
   printf("localhostIP_IPv4=[%s]\n",       localhostIP_IPv4.ToString()());
   printf("localhostIP_IPv6=[%s]\n",       localhostIP_IPv6.ToString()());
   printf("broadcastIP_IPv4=[%s]\n",       broadcastIP_IPv4.ToString()());
   printf("broadcastIP_IPv6=[%s]\n",       broadcastIP_IPv6.ToString()());

   // Example of parsing a string into an IPv4 address
   IPAddress example_localhostAddress4("192.168.0.1");
   printf("example_localhostAddress4=[%s]\n", example_localhostAddress4.ToString()());

   // Example of parsing a string into an IPv6 address
   IPAddress example_localhostAddress6("::1");
   printf("example_localhostAddress6=[%s]\n", example_localhostAddress6.ToString()());

   // You can also put together an IP address numerically, if you're the kind 
   // of person who likes to do things that way
   IPAddress myMulticastIP((uint64)0x12, ((uint64)0xFF)<<56, 3);  // low-64-bits, high-64-bits, scope-index
   printf("myMulticastIP=[%s]\n", myMulticastIP.ToString()()); 

   // Similar stuff for an IPAddressAndPort
   IPAddressAndPort myIAP_v4("127.0.0.1:9999", 6666, false);  // false means "avoid using DNS lookups"
   printf("myIAP_v4=[%s]\n", myIAP_v4.ToString()());

   IPAddressAndPort myIAP_v6("[::1]:9999", 6666, false);  // false means "avoid using DNS lookups"
   printf("myIAP_v6=[%s]\n", myIAP_v6.ToString()());

   IPAddressAndPort googleIAP("www.google.com:80", 6666, true);  // true means "allow DNS lookups"
   printf("googleIAP=[%s]\n", googleIAP.ToString()());
   printf("\n");

   return 0;
}
