/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>
#include "system/SetupSystem.h"
#include "util/NetworkUtilityFunctions.h"
#include "util/Socket.h"

using namespace muscle;

int main(int, char **)
{
   CompleteSetupSystem css;

   printf("Querying local host's IP addresses:\n");

   printf("\n");

   Queue<NetworkInterfaceInfo> ifs;
   if (GetNetworkInterfaceInfos(ifs) == B_NO_ERROR)
   {
      printf("Found " UINT32_FORMAT_SPEC " local network interfaces:\n", ifs.GetNumItems());
      for (uint32 i=0; i<ifs.GetNumItems(); i++)
      {
         const NetworkInterfaceInfo & nii = ifs[i];
         char addrStr[64]; Inet_NtoA(nii.GetLocalAddress(), addrStr);
         char maskStr[64]; Inet_NtoA(nii.GetNetmask(), maskStr);
         char remtStr[64]; Inet_NtoA(nii.GetBroadcastAddress(), remtStr);
         printf("  #" UINT32_FORMAT_SPEC ":  name=[%s] address=[%s] netmask=[%s] broadcastAddress=[%s]\n", i+1, nii.GetName()(), addrStr, maskStr, remtStr);
      }
   }
   else printf("GetNetworkInterfaceInfos() returned an error!\n");

   printf("\n\nTesting IPAddressAndPort parsing... (defaulting to port 666 when port is unspecified)\n");
   char buf[256];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s(buf); s = s.Trim();
      printf("You typed %s ... I interpreted that as %s\n", s(), IPAddressAndPort(s,666,true).ToString(true,true)()); fflush(stdout);
   }

   return 0;
}
