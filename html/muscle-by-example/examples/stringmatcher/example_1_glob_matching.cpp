#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "regex/StringMatcher.h"
#include "util/String.h"

#include "common_words.inc"  // an array of the 1000 most common words in English

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates bash-style pattern-glob matching using the StringMatcher class.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Enter a bash-style wildcard pattern and I'll print out\n");
   printf("the word that match that pattern from the list of some\n");
   printf("common English words.  Enter * to see the entire list.\n");
   printf("\n");
   printf("Extensions note:  You can prepend a ~ to your pattern to\n");
   printf("match only the strings that DON'T match the pattern, and\n");
   printf("you can use the special form e.g. <3-12,20-25> to match\n");
   printf("strings-that-represent-numbers in the specified range.\n");
   printf("\n");

   while(true)
   {
      printf("Enter a wildcard pattern: ");
      fflush(stdout);

      char buf[1024];
      if (fgets(buf, sizeof(buf), stdin) == NULL) break;

      String s = buf;
      s = s.Trim();  // get rid of newlines, etc

      printf("You entered:  [%s]\n", s());
      printf("\n");
      printf("Matching words from our dictionary are:\n");

      int matchCount = 0;
      StringMatcher sm(s);
      for (uint32 i=0; i<ARRAYITEMS(_commonWords); i++)
      {
         const char * nextWord = _commonWords[i];
         if (sm.Match(nextWord))
         {
            printf("   %s\n", nextWord);
            matchCount++;
         }
      }
      printf("%i/%u dictionary words matched glob pattern [%s]\n", matchCount, (unsigned int) ARRAYITEMS(_commonWords), s());
      printf("\n");
   }

   return 0;
}
