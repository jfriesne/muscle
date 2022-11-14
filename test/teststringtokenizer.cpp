/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "system/SetupSystem.h"
#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

int main(int, char **)
{
   CompleteSetupSystem css;

   // First, a performance test
   {
      uint64 totalElapsedTime = 0;
      uint64 totalChars       = 0;
      const uint32 numRuns    = 10;

      for (uint32 i=0; i<numRuns; i++)
      {
         const uint32 BIGBUFSIZE = 50*1024*1024;  // a really big string, to give us some room to exercise
         char * tempBuf = new char[BIGBUFSIZE];
         for (uint32 i=0; i<BIGBUFSIZE; i++) tempBuf[i] = (rand()%80)+' ';
         tempBuf[BIGBUFSIZE-1] = '\0';

         StringTokenizer tok(false, tempBuf);
         uint32 count = 0;
         const uint64 startTime = GetRunTime64();
         {
            while(tok() != NULL) count++;
         }
         const uint64 runTime = GetRunTime64()-startTime;
         LogTime(MUSCLE_LOG_INFO, "Run #" UINT32_FORMAT_SPEC "/" UINT32_FORMAT_SPEC ": Tokenized " UINT32_FORMAT_SPEC " chars into " UINT32_FORMAT_SPEC " strings over [%s], speed was %.0f chars/usec\n", i+1, numRuns, BIGBUFSIZE, count, GetHumanReadableTimeIntervalString(runTime, 1)(), ((double)(BIGBUFSIZE))/((runTime>0)?runTime:1LL));
         totalChars       += BIGBUFSIZE;
         totalElapsedTime += runTime;
         delete [] tempBuf;
      }

      const uint64 averageRunTime = totalElapsedTime/numRuns;
      LogTime(MUSCLE_LOG_INFO, "Average run time over " UINT32_FORMAT_SPEC " runs was [%s], average speed was %.0f chars/usec\n", numRuns, GetHumanReadableTimeIntervalString(averageRunTime, 1)(), ((double)(totalChars))/((totalElapsedTime>0)?totalElapsedTime:1LL));
   }

   while(1)
   {
      printf("Enter a string to tokenize: ");
      fflush(stdout);

      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin))
      {
         String s = buf;
         s = s.WithoutSuffix("\n").WithoutSuffix("\r");

         printf("\nYou typed: [%s]\n", s());
         const char * t;
         StringTokenizer tok(s(), NULL, '\\');

         StringTokenizer tokCopy(tok);

         int i=0;
         while((t = tok()) != NULL) printf(" %i. tok=[%s] remainder=[%s]\n", i++, t, tok.GetRemainderOfString());
         printf("\n");

         printf("Checking copy of StringTokenizer:\n");
         while((t = tokCopy()) != NULL) printf(" %i. tok=[%s] remainder=[%s]\n", i++, t, tokCopy.GetRemainderOfString());
         printf("\n");

         // Call a few more times just to see that it returns NULL as expected
         const char * extra = tok();
         if (extra) printf("WTF A?  [%s]\n", extra);
         extra = tok();
         if (extra) printf("WTF B?  [%s]\n", extra);
         extra = tok();
         if (extra) printf("WTF C?  [%s]\n", extra);
      }
   }

   return 0;
}
