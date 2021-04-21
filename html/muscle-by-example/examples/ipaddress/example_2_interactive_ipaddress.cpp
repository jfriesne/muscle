#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/IPAddress.h"
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This program allows you to interactively invoke the IPAddress class's string-parser.\n");
   printf("\n");
}

/* This little program demonstrates the parsing of IPAddress strings */
int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   while(1)
   {
      printf("Please enter a string representing an IPv4 or IPv6 numeric host-address: ");
      fflush(stdout);

      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin) == NULL) break;

      String s = buf;
      s = s.Trim();  // get rid of newline ugliness

      IPAddress ip; 
      if (ip.SetFromString(s).IsOK())
      {
         printf("I parsed the string [%s] as IPAddress %s\n", s(), ip.ToString()());
         printf("    ip.IsValid() returned %i\n", ip.IsValid());
         printf("    ip.IsIPv4() returned %i\n", ip.IsIPv4());
         printf("    ip.IsMulticast() returned %i\n", ip.IsMulticast());
         printf("    ip.IsStandardLoopbackAddress() returned %i\n", ip.IsStandardLoopbackDeviceAddress());
         printf("\n");
      }
      else printf("Error, couldn't parse [%s] as an IPAddress!\n", s());
   }
   return 0;
}
