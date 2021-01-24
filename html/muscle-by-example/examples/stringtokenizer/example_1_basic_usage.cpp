#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/StringTokenizer.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::StringTokenizer class.\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   const char * someCString = "   One, Two, Three,,, Four, Five   ";
   printf("Here is the string we want to parse tokens out of:\n");
   printf("   [%s]\n", someCString);

   printf("\n");
   printf("Basic StringTokenizer usage (using default separator-chars arguments:  hard=\",\" soft=\" \\t\\r\\n\"):\n");
   {
      StringTokenizer tok(someCString);

      const char * nextTok;
      while((nextTok = tok()) != NULL)
      {
         printf("   nextTok = [%s]\n", nextTok);
      }
   }

   printf("\n");
   printf("Basic StringTokenizer usage (with explicit separator-chars arguments:  hard=\",\" soft=NULL)\n");
   {
      StringTokenizer tok(someCString, ",", NULL);

      const char * nextTok;
      while((nextTok = tok()) != NULL)
      {
         printf("   nextTok = [%s]\n", nextTok);
      }
   }

   printf("\n");
   printf("Let's get only the first three tokens, then print the rest of the string:\n");
   {
      StringTokenizer tok(someCString);

      const char * t1 = tok();
      const char * t2 = tok();
      const char * t3 = tok();
      const char * rest = tok.GetRemainderOfString();

      printf("   t1 = [%s]\n", t1);
      printf("   t2 = [%s]\n", t2);
      printf("   t3 = [%s]\n", t3);
      printf(" rest = [%s]\n", rest);
   }

   printf("\n");
   return 0;
}
