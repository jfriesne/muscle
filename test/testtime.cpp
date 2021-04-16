/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/MiscUtilityFunctions.h"
#include "util/StringTokenizer.h"

using namespace muscle;

static float GetDiffHours(uint64 t1, uint64 t2) {return (float) ((double)((int64)t1 - (int64) t2))/(60.0 * 60.0 * (double)MICROS_PER_SECOND);}

// This program is used to test out muscle's time/date interpretation functions
int main(int argc, char ** argv) 
{
   CompleteSetupSystem css;

   Message argsMsg;
   ParseArgs(argc, argv, argsMsg);
   HandleStandardDaemonArgs(argsMsg);

   const uint64 epoch = 0;
   printf("epoch time (UTC) = %s\n", GetHumanReadableTimeString(epoch, MUSCLE_TIMEZONE_UTC)());
   printf("epoch time (loc) = %s\n", GetHumanReadableTimeString(epoch, MUSCLE_TIMEZONE_LOCAL)());

   const uint64 nowLocal = GetCurrentTime64(MUSCLE_TIMEZONE_LOCAL);
   const String nowLocalStr = GetHumanReadableTimeString(nowLocal, MUSCLE_TIMEZONE_LOCAL);
   printf("NOW (Local) = " UINT64_FORMAT_SPEC " = %s\n", nowLocal, nowLocalStr());
   const uint64 reparsedLocal = ParseHumanReadableTimeString(nowLocalStr(), MUSCLE_TIMEZONE_LOCAL);
   printf("   reparsed = " UINT64_FORMAT_SPEC " (diff=%.1f hours)\n", reparsedLocal, GetDiffHours(reparsedLocal, nowLocal));

   const uint64 nowUTC = GetCurrentTime64(MUSCLE_TIMEZONE_UTC);
   const String nowUTCStr = GetHumanReadableTimeString(nowUTC, MUSCLE_TIMEZONE_LOCAL);
   printf("NOW (UTC)   = " UINT64_FORMAT_SPEC " = %s\n            (or, in local terms, %s)\n", nowUTC, nowUTCStr(), GetHumanReadableTimeString(nowUTC, MUSCLE_TIMEZONE_UTC)());

   const uint64 reparsedUTC = ParseHumanReadableTimeString(nowUTCStr(), MUSCLE_TIMEZONE_LOCAL);
   printf("   reparsed = " UINT64_FORMAT_SPEC " (diff=%.1f hours)\n", reparsedUTC, GetDiffHours(reparsedUTC, nowUTC));

   printf("The offset between local time and UTC is %.1f hours.\n", GetDiffHours(nowLocal, nowUTC));

   HumanReadableTimeValues v;
   if (GetHumanReadableTimeValues(nowLocal, v, MUSCLE_TIMEZONE_LOCAL).IsOK()) printf("HRTV(local) = [%s]\n", v.ExpandTokens("%T DoW=%w (%t) (%f) (%q) (micro=%x) (rand=%r)")());
                                                                         else printf("Error getting human readable time values for local!\n");
   if (GetHumanReadableTimeValues(nowUTC, v, MUSCLE_TIMEZONE_LOCAL).IsOK())   printf("HRTV(UTC)   = [%s]\n", v.ExpandTokens("%T DoW=%w (%t) (%f) (%q) (micro=%x) (rand=%r)")());
                                                                         else printf("Error getting human readable time values for UTC!\n");

   // Interactive time interval testing
   if ((argc > 1)&&(strcmp(argv[1], "testintervals") == 0))
   {
      while(1)
      {
         printf("Enter micros, minPrecision(micros): ");  fflush(stdout);
         char buf[512]; if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = '\0';
         StringTokenizer tok(buf);
         const char * m = tok();
         const char * p = tok();
         if (m)
         {
            const uint64 micros    = Atoull(m);
            const uint64 precision = p ? Atoull(p) : 0;
            printf("  You entered " UINT64_FORMAT_SPEC " microseconds, minimum precision " UINT64_FORMAT_SPEC " microseconds.\n", micros, precision);
            bool isAccurate;
            const String s = GetHumanReadableTimeIntervalString(micros, MUSCLE_NO_LIMIT, precision, &isAccurate);
            printf("Result (%s) : %s\n", isAccurate?"Exact":"Approximate", s());
         }
      }
   }

   // Test intervals
   printf("Testing time interval parsing and generation.  This may take a little while...\n");
   const uint64 TEN_YEARS_IN_MICROSECONDS = ((uint64)315360)*NANOS_PER_SECOND;  // yes, nanos per second is correct here!
   uint64 delta = 1;
   for (uint64 i=0; i<=TEN_YEARS_IN_MICROSECONDS; i+=delta)
   {
      bool isAccurate;
      const String s = GetHumanReadableTimeIntervalString(i, MUSCLE_NO_LIMIT, 0, &isAccurate);
      if (isAccurate == false) printf("Error, string [%s] is not accurate for i=" UINT64_FORMAT_SPEC ".\n", s(), i);
      const uint64 t = ParseHumanReadableTimeIntervalString(s);
      //printf(" " UINT64_FORMAT_SPEC " -> %s -> " UINT64_FORMAT_SPEC "\n", i, s(), t);
      if (t != i) printf("Error, Recovered time " UINT64_FORMAT_SPEC " does not match original time " UINT64_FORMAT_SPEC " (string=[%s])\n", t, i, s());
      delta++;
   }

   return 0;
}
