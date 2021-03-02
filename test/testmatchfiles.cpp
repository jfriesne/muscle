/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "regex/FilePathExpander.h"
#include "util/String.h"

using namespace muscle;

int main(void) 
{
   char buf[1024];
   while(fgets(buf, sizeof(buf), stdin))
   {
      String s(buf); s = s.Trim();

      Queue<String> q;
      if (ExpandFilePathWildCards(s, q).IsOK())
      {
         printf("File path [%s] expanded to " UINT32_FORMAT_SPEC " paths:\n", s(), q.GetNumItems());
         for (uint32 i=0; i<q.GetNumItems(); i++) printf("   - [%s]\n", q[i]());
      }
      else printf("Error, couldn't expand file path [%s]\n", s());
      printf("\n\n");
   }
   return 0;
}
