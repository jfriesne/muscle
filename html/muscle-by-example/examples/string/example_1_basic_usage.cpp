#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates basic usage of the muscle::String class\n");
   printf("\n");
}

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Basic String declaration and concatenation:\n");
   {
      const String s1("a string");
      const String s2("another string");
      const String s3 = s1 + s2;

      printf("  s1 = [%s]\n", s1());  // Note s1() is a synonym for s1.Cstr()
      printf("  s2 = [%s]\n", s2());  // which returns a (const char *) pointer
      printf("  s3 = [%s]\n", s3());  // to the string's contents.
   }

   printf("\n");
   printf("In-Place string concatenation:\n");
   {
      String s = "Do";
      s += " Re";
      s += " Mi";
      s += " Fa";
      printf("   s = [%s]\n", s());
   }
   
   printf("\n");
   return 0;
}
