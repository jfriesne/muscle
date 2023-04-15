/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "regex/FilePathExpander.h"
#include "util/String.h"
#include "system/SetupSystem.h"

using namespace muscle;

static status_t TextExpandFilePath(const String & s)
{
   Queue<String> q;
   status_t ret;
   if (ExpandFilePathWildCards(s, q).IsOK(ret))
   {
      printf("File path [%s] expanded to " UINT32_FORMAT_SPEC " paths:\n", s(), q.GetNumItems());
      for (uint32 i=0; i<q.GetNumItems(); i++) printf("   - [%s]\n", q[i]());
   }
   else printf("Error, couldn't expand file path [%s] [%s]\n", s(), ret());

   return ret;
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   if ((argc >= 2)&&(strcmp(argv[1], "fromscript") == 0))
   {
      return TextExpandFilePath("*.cpp").IsOK() ? 0 : 10;
   }
   else
   {
      int ret = 0;

      char buf[1024];
      while(fgets(buf, sizeof(buf), stdin))
      {
         String s(buf); s = s.Trimmed();
         if (TextExpandFilePath(s).IsError()) ret = 10;
         printf("\n\n");
      }
      return ret;
   }
}
