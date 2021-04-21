#include "system/SetupSystem.h"  // for CompleteSetupSystem
#include "util/String.h"

using namespace muscle;

static void PrintExampleDescription()
{
   printf("\n");
   printf("This example demonstrates the behavior of various String accessor-methods.\n");
   printf("\n");
}

void PrintSingleStringInfo(const char * desc, const String & s);   // forward declaration -- see bottom of this file

int main(int argc, char ** argv)
{
   CompleteSetupSystem css;

   PrintExampleDescription();

   printf("Please enter any string: ");
   fflush(stdout);

   char buf[1024];
   if (fgets(buf, sizeof(buf), stdin) == NULL) return 10;

   String s1 = buf;
   s1 = s1.Trim();   // Get rid of any newlines/whitespace at front/end of s1 

   printf("Please enter a second string: ");
   if (fgets(buf, sizeof(buf), stdin) == NULL) return 10;

   String s2 = buf;
   s2 = s2.Trim();   // Get rid of any newlines/whitespace at front/end of s2

   printf("\n");
   printf("You entered [%s] as your 1st string.\n", s1());
   printf("You entered [%s] as your 2nd string.\n", s2());

   printf("\n");
   PrintSingleStringInfo("first",  s1);  // see function below
   PrintSingleStringInfo("second", s2);  // see function below

   printf("\n");
   printf("Demonstration of various const-methods called on String s1 (\"%s\") using s2 (\"%s\") as an argument\n", s1(), s2());
   printf("   s1.Equals(\"%s\") returned %i (s1.EqualsIgnoreCase() returned %i)\n", s2(), s1.Equals(s2), s1.EqualsIgnoreCase(2));
   printf("   s1.CompareTo(\"%s\") returned %i (s1.CompareToIgnoreCase() returned %i)\n", s2(), s1.CompareTo(s2), s1.CompareToIgnoreCase(s2));
   printf("   s1.NumericAwareCompareTo(\"%s\") returned %i (s1.NumericAwareCompareToIgnoreCase() returned %i)\n", s2(), s1.NumericAwareCompareTo(s2), s1.NumericAwareCompareToIgnoreCase(s2));
   printf("   s1.StartsWith(\"%s\") returned %i (s1.StartsWithIgnoreCase() returned %i)\n", s2(), s1.StartsWith(s2), s1.StartsWithIgnoreCase(s2));
   printf("   s1.Contains(\"%s\") returned %i (s1.ContainsIgnoreCase() returned %i)\n", s2(), s1.Contains(s2), s1.ContainsIgnoreCase(s2));
   printf("   s1.EndsWith(\"%s\") returned %i (s1.EndsWithIgnoreCase() returned %i)\n", s2(), s1.EndsWith(s2), s1.EndsWithIgnoreCase(s2));
   printf("   s1.IndexOf(\"%s\") returned %i (s1.IndexOfIgnoreCase() returned %i)\n", s2(), s1.IndexOf(s2), s1.IndexOfIgnoreCase(s2));
   printf("   s1.LastIndexOf(\"%s\") returned %i (s1.LastIndexOfIgnoreCase() returned %i)\n", s2(), s1.LastIndexOf(s2), s1.LastIndexOfIgnoreCase(s2));
   printf("   s1.GetNumInstancesOf(\"%s\") returned " UINT32_FORMAT_SPEC "\n", s2(), s1.GetNumInstancesOf(s2));
   printf("   s1.Prepend(\"%s\") returned \"%s\"\n", s2(), s1.Prepend(s2)());
   printf("   s1.Append(\"%s\") returned \"%s\"\n", s2(), s1.Append(s2)());
   printf("   s1.PrependWord(\"%s\") returned \"%s\"\n", s2(), s1.PrependWord(s2)());
   printf("   s1.AppendWord(\"%s\") returned \"%s\"\n", s2(), s1.AppendWord(s2)());
   printf("   s1.Substring(\"%s\") returned \"%s\"\n", s2(), s1.Substring(s2)());
   printf("   s1.Substring(0, \"%s\") returned \"%s\"\n", s2(), s1.Substring(0, s2)());
   printf("   s1.WithPrefix(\"%s\") returned \"%s\"\n", s2(), s1.WithPrefix(s2)());
   printf("   s1.WithSuffix(\"%s\") returned \"%s\"\n", s2(), s1.WithSuffix(s2)());
   printf("   s1.WithoutPrefix(\"%s\") returned \"%s\" (s1.WithoutPrefixIgnoreCase() returned \"%s\")\n", s2(), s1.WithoutPrefix(s2)(), s1.WithoutPrefixIgnoreCase(s2)());
   printf("   s1.WithoutSuffix(\"%s\") returned \"%s\" (s1.WithoutSuffixIgnoreCase() returned \"%s\")\n", s2(), s1.WithoutSuffix(s2)(), s1.WithoutSuffixIgnoreCase(s2)());
   printf("\n");
   return 0;
}

// Utility function for printing interesting info about a given
// String, to demonstrate various methods of the String class
void PrintSingleStringInfo(const char * desc, const String & s)
{
   printf("\n");
   printf("Information for %s string:  [%s]\n", desc, s.Cstr());
   printf("   s.Length()            returned " UINT32_FORMAT_SPEC " (chars/bytes) (doesn't include NUL byte)\n", s.Length());
   printf("   s.FlattenedSize()     returned " UINT32_FORMAT_SPEC " (bytes)       (does include NUL byte)\n", s.FlattenedSize());
   printf("   s.HasChars()          returned %i\n", s.HasChars());
   printf("   s.IsEmpty()           returned %i\n", s.IsEmpty());
   printf("   s.StartsWithNumber()  returned %i\n", s.StartsWithNumber());
   printf("   s.ToLowerCase()       returned \"%s\"\n", s.ToLowerCase()());
   printf("   s.ToUpperCase()       returned \"%s\"\n", s.ToUpperCase()());
   printf("   s.ToMixedCase()       returned \"%s\"\n", s.ToMixedCase()());
   printf("   s.HashCode()          returned " UINT32_FORMAT_SPEC "\n", s.HashCode());
   printf("   s.HashCode64()        returned " UINT64_FORMAT_SPEC "\n", s.HashCode64());
   printf("   s.CalculateChecksum() returned " UINT32_FORMAT_SPEC "\n", s.CalculateChecksum());
}
