/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "system/SetupSystem.h"
#include "util/NetworkInterfaceInfo.h"
#include "util/NetworkUtilityFunctions.h"  // for GetNetworkInterfaceInfos()

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This programs demonstrates the use of GetNetworkInterfaceInfos() to gather information about available network interfaces.\n");
   printf("\n");
}

void PrintNetworkInterfaceInfos(const Queue<NetworkInterfaceInfo> & ifs, const char * desc);  // forward declaration (see bottom of file)

int main(int, char **)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Querying local host's network interfaces:\n");
   printf("\n");

   // First let's print the of NICs on this machine that are actually configured for use
   // This is usually the list you want.
   {
      Queue<NetworkInterfaceInfo> ifs;
      if (GetNetworkInterfaceInfos(ifs).IsOK())
      {
         PrintNetworkInterfaceInfos(ifs, "ACTIVE network interfaces");
      }
      else printf("GetNetworkInterfaceInfos() returned an error!\n");
   }

   printf("\n");

   // Now let's print the exhaustive list of ALL the NICs on this machine (set up or not!)
   {
      Queue<NetworkInterfaceInfo> ifs;
      if (GetNetworkInterfaceInfos(ifs, GNIIFlags(GNII_FLAGS_INCLUDE_ALL_INTERFACES)).IsOK())
      {
         PrintNetworkInterfaceInfos(ifs, "total network interfaces");
      }
      else printf("GetNetworkInterfaceInfos() returned an error!\n");
   }

   return 0;
}

void PrintNetworkInterfaceInfos(const Queue<NetworkInterfaceInfo> & ifs, const char * desc)
{
   printf("List of " UINT32_FORMAT_SPEC " %s:\n", ifs.GetNumItems(), desc);
   for (uint32 i=0; i<ifs.GetNumItems(); i++)
   {
      const NetworkInterfaceInfo & nii = ifs[i];
      printf("  #" UINT32_FORMAT_SPEC ":  %s\n", i+1, nii.ToString()());
   }
}
