#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/IPAddress.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program allows you to interactively invoke the IPAddressAndPort class's string-parser.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   while(1)
   {
      printf("Please enter a string representing a hostname-colon-port or host-address-colon-port: ");
      fflush(stdout);

      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin) == NULL) break;

      String s = buf;
      s = s.Trim();  // get rid of newline ugliness

      IPAddressAndPort iap(s, 6666, true);
      printf("I parsed the string [%s] as IPAddressAndPort %s\n", s(), iap.ToString()());
     
      const IPAddress & ip = iap.GetIPAddress();
      printf("    ip.IsValid() returned %i\n", ip.IsValid());
      printf("    ip.IsIPv4() returned %i\n", ip.IsIPv4());
      printf("    ip.IsMulticast() returned %i\n", ip.IsMulticast());
      printf("    ip.IsStandardLoopbackAddress() returned %i\n", ip.IsStandardLoopbackDeviceAddress());
      printf("\n");
   }

   return 0;
}
