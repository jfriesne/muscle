/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include "util/NetworkUtilityFunctions.h"
#include "system/SetupSystem.h"

using namespace muscle;

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if (argc <= 1) 
   {
      LogTime(MUSCLE_LOG_INFO, "Usage:  portscan <ipaddress> [baseport] [numports]\n");
      return 5;
   }

   const char * hostName = argv[1];

   int base = 0;
   if (argc > 2) base = atoi(argv[2]);

   int count = 65536;
   if (argc > 3) count = atoi(argv[3]);

   Queue<int> foundPorts;

   const IPAddress ip = GetHostByName(hostName);
   if (ip == invalidIP)
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "Unable to resolve hostname [%s]\n", hostName);
      return 10;
   }

   char buf[64]; Inet_NtoA(ip, buf);
   const int final = muscleMin(base+count, 65536);
   LogTime(MUSCLE_LOG_INFO, "Beginning scan of ports %i-%i at %s...\n", base, final-1, buf);
   uint64 lastTime = 0;
   for (int i=base; i<final; i++)
   {
      ConstSocketRef s = Connect(ip, (uint16)i, NULL, NULL, true, 10000);
      if (s()) {foundPorts.AddTail(i); LogTime(MUSCLE_LOG_INFO, "Found open port %i!\n", i);}
      if (OnceEvery(MICROS_PER_SECOND, lastTime)) LogTime(MUSCLE_LOG_INFO, "Scanning %s (now at port %i...)\n", buf, i);
   }

   LogTime(MUSCLE_LOG_INFO, "\n\nFinal report\n\n");
   if (foundPorts.HasItems())
   {
      LogTime(MUSCLE_LOG_INFO, "The following " UINT32_FORMAT_SPEC " TCP ports were found open:\n", foundPorts.GetNumItems());
      for (uint32 i=0; i<foundPorts.GetNumItems(); i++) LogTime(MUSCLE_LOG_INFO, "    %i\n", foundPorts[i]);
   }  
   else LogTime(MUSCLE_LOG_INFO, "No TCP ports were found open.\n");

   return 0;
}
