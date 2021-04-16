/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "util/String.h"
#include "util/StringTokenizer.h"

using namespace muscle;

int main(void) 
{
   while(1)
   {
      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin))
      {
         String s = buf;
         s = s.WithoutSuffix("\n").WithoutSuffix("\r");

         printf("\nYou typed: [%s]\n", s());
         const char * t;
         StringTokenizer tok(s());

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
