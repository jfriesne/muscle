#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example program demonstrates generation of substrings from a muscle::String\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("String construction from only the first N chars of a C string:\n");
   {
      const char * p = "This is a long C string, we don't really want all of it";
      const String shortString(p, 4);  // Place only the first 4 chars into the shortString

      printf("  C string    = [%s]\n", p);
      printf("  shortString = [%s]\n", shortString());
   }

   printf("\n");
   printf("Computation of substrings via Substring-constructor:\n");
   {
      const String longString("0123456789");
      const String shortString(  longString, 5);    // substring starting at 5th char of longString
      const String shorterString(longString, 5, 7); // substring starting at 5th char, ending before 7th char

      printf("  longString    = [%s]\n", longString());
      printf("  shortString   = [%s]\n", shortString());
      printf("  shorterString = [%s]\n", shorterString());
   }

   printf("\n");
   printf("Use of the Substring() method to compute substrings:\n");
   {
      const String longString("0123456789");

      const String shortString   = longString.Substring(5);    // substring starting at 5th char of longString
      const String shorterString = longString.Substring(5, 7); // substring starting at 5th char, ending before 7th char

      printf("  longString    = [%s]\n", longString());
      printf("  shortString   = [%s]\n", shortString());
      printf("  shorterString = [%s]\n", shorterString());
   }
   
   printf("\n");
   printf("Use of the Substring() method to compute substrings based on a substring-search:\n");
   {
      const String longString("This is only a test");

      const String firstShortString = longString.Substring(0, "only");  // get only the text before the first occurrence of "only"
      const String lastShortString  = longString.Substring("only");     // get only the text after the last occurrence of "only"
  
      printf("  longString       = [%s]\n", longString());
      printf("  firstShortString = [%s]\n", firstShortString());
      printf("  lastShortString  = [%s]\n", lastShortString());
   }

   printf("\n");
   return 0;
}
