/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "util/String.h"
#include "util/StringTokenizer.h"
#include "util/TimeUtilityFunctions.h"

using namespace muscle;

int main(int, char **)
{
   // First, a performance test
   {
      const uint32 BIGBUFSIZE = 10*1024*1024;  // a 10MB string, to give us some room to exercise
      char * tempBuf = new char[BIGBUFSIZE];
      for (uint32 i=0; i<BIGBUFSIZE; i++) tempBuf[i] = (rand()%80)+' ';
      tempBuf[BIGBUFSIZE-1] = '\0';

      StringTokenizer tok(false, tempBuf);
      uint32 count = 0;
      const uint64 startTime = GetRunTime64();
      {
         const char * t;
         while((t=tok()) != NULL) count++;
      }
      const uint64 endTime = GetRunTime64();
      LogTime(MUSCLE_LOG_INFO, "Tokenized " UINT32_FORMAT_SPEC " chars into " UINT32_FORMAT_SPEC " strings over [%s], speed is %.0f chars/sec\n", BIGBUFSIZE, count, GetHumanReadableTimeIntervalString(endTime-startTime)(), ((double)(1000000LL*count))/((endTime>startTime)?(endTime-startTime):1LL));
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
         StringTokenizer tok(s(), ",", " \t\r\n", '\\');

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
