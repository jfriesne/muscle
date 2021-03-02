/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "message/Message.h"
#include "util/String.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "support/Tuple.h"  // make sure the template operators don't mess us up

using namespace muscle;

#define TEST(x) if (!(x)) printf("Test failed, line %i\n",__LINE__)

// This program exercises the String class.
int main(void) 
{
#ifdef TEST_DISTANCE
   while(1)
   {
      char base[512];  
      printf("Enter string A: "); fflush(stdout); if (fgets(base, sizeof(base), stdin) == NULL) base[0] = '\0';
      String a = base; a = a.Trim();
      printf("Enter string B: "); fflush(stdout); if (fgets(base, sizeof(base), stdin) == NULL) base[0] = '\0';
      String b = base; b = b.Trim();

      printf("Distance from [%s] to [%s] is %u\n", a(), b(), a.GetDistanceTo(b));
      printf("Distance from [%s] to [%s] is %u\n", b(), a(), b.GetDistanceTo(a));
   }
#endif

#ifdef TEST_MEMMEM
   char lookIn[512]; printf("Enter LookIn  string: "); fflush(stdout); if (fgets(lookIn, sizeof(lookIn), stdin) == NULL) lookIn[0] = '\0';
   lookIn[strlen(lookIn)-1] = '\0';
   while(1)
   {
      char lookFor[512]; printf("Enter LookFor string: "); fflush(stdout); if (fgets(lookFor, sizeof(lookFor), stdin) == NULL) lookFor[0] = '\0';
      lookFor[strlen(lookFor)-1] = '\0';

      printf("lookIn=[%s] lookFor=[%s]\n", lookIn, lookFor);
      const uint8 * r = MemMem((const uint8 *)lookIn, (uint32)strlen(lookIn), (const uint8 *)lookFor, (uint32)strlen(lookFor));
      printf("r=[%s]\n", r);
   }
#endif

#ifdef TEST_PARSE_ARGS
   while(1)
   {
      char base[512];  printf("Enter string: "); fflush(stdout); if (fgets(base, sizeof(base), stdin) == NULL) base[0] = '\0';

      Message args;
      if (ParseArgs(base, args).IsOK())
      {
         printf("Parsed: "); args.PrintToStream();
      }
      else printf("Parse failed!\n");
   }
#endif

   {
      // Test to make sure that when a string is set equal to an empty string, it deletes its buffer.
      // (That way long strings can't build up in an ObjectPool somewhere)
      String longString = "this is a very long string.  Well okay it's not THAT long, but long enough.";
      const String & emptyString = GetDefaultObjectForType<String>();
      printf("Before copy-from-empty:   longString [%s] bufSize=" UINT32_FORMAT_SPEC ", emptyString [%s] bufSize=" UINT32_FORMAT_SPEC "\n", longString(), longString.GetNumAllocatedBytes(), emptyString(), emptyString.GetNumAllocatedBytes());
      longString = emptyString;
      printf(" After copy-from-empty:   longString [%s] bufSize=" UINT32_FORMAT_SPEC ", emptyString [%s] bufSize=" UINT32_FORMAT_SPEC "\n", longString(), longString.GetNumAllocatedBytes(), emptyString(), emptyString.GetNumAllocatedBytes());
   }

   {
      printf("Testing string-buffer-expansion behavior...\n");

      const String shortString = "1234567890";
      printf("shortString=[%s] length=" UINT32_FORMAT_SPEC " numAllocedBytes=" UINT32_FORMAT_SPEC "\n", shortString(), shortString.Length(), shortString.GetNumAllocatedBytes());

      // Watch the behavior of the buffer size
      uint32 numAllocedBytes = 0;
      String s;
      for (int i=0; i<50000; i++)
      {
         s += 'x';
         const uint32 newNumAlloced = s.GetNumAllocatedBytes();
         if (newNumAlloced != numAllocedBytes)
         {
            printf("i=%i s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", i, s.Length(), newNumAlloced);
            numAllocedBytes = newNumAlloced;
         }
      }
      if (s.ShrinkToFit().IsOK()) printf("After ShrinkToFit():  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());
                              else printf("ShrinkToFit() failed!\n");

      s = "Now I'm small";
      printf("After setting small:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());

      if (s.ShrinkToFit().IsOK()) printf("After ShrinkToFit to small():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());
                              else printf("ShrinkToFit() to small failed!\n");

      s = "tiny";
      printf("After setting tiny:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());
      if (s.ShrinkToFit().IsOK()) printf("After ShrinkToFit to tiny():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());
                              else printf("ShrinkToFit() to tiny failed!\n");

      s = "tin";
      printf("After setting tin:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());
      if (s.ShrinkToFit().IsOK()) printf("After ShrinkToFit to tin():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());
                              else printf("ShrinkToFit() to tin failed!\n");
   }

#ifdef TEST_REPLACE_METHOD
   while(1)
   {
      char base[512];      printf("Enter string:    "); fflush(stdout); if (fgets(base,      sizeof(base),      stdin) == NULL) base[0]      = '\0';
      char replaceMe[512]; printf("Enter replaceMe: "); fflush(stdout); if (fgets(replaceMe, sizeof(replaceMe), stdin) == NULL) replaceMe[0] = '\0';
      char withMe[512];    printf("Enter withMe:    "); fflush(stdout); if (fgets(withMe,    sizeof(withMe),    stdin) == NULL) withMe[0]    = '\0';

      String b(base);
      const int32 ret = b.Replace(replaceMe, withMe);
      printf(INT32_FORMAT_SPEC ": Afterwards, [%s] (" UINT32_FORMAT_SPEC ")\n", ret, b(), b.Length());
   }
#endif

   // Test the multi-search-and-replace version of WithReplacements()
   {
      const String before = "One potato, Two potato, Three potato, Four.  Five potato, Six potato, Seven potato, more!  One Two Three Four Five";

      Hashtable<String,String> replaceMap;
      replaceMap.Put("One", "Two");
      replaceMap.Put("Two", "3");
      replaceMap.Put("Three", "4");
      replaceMap.Put("potato", "sweet potato");
      replaceMap.Put("sweet", "sour");   // shouldn't have any effect, since the original string doesn't contain the substring 'sour'
      const String after = before.WithReplacements(replaceMap);

      const String expected = "Two sweet potato, 3 sweet potato, 4 sweet potato, Four.  Five sweet potato, Six sweet potato, Seven sweet potato, more!  Two 3 4 Four Five";
      if (after == expected) printf("Multi-replace:  got expected result [%s]\n", after());
      else
      {
         printf("ERROR GOT WRONG MULTI-REPLACE RESULT [%s], expected [%s]\n", after(), expected());
         return 10;
      }
   }

   int five=5, six=6;
   muscleSwap(five, six);
   if ((five != 6)||(six != 5)) {printf("Oh no, trivial muscleSwap() is broken!  five=%i six=%i\n", five, six); exit(10);}

   String ss1 = "This is string 1", ss2 = "This is string 2";
   PrintAndClearStringCopyCounts("Before Swap");
   muscleSwap(ss1, ss2);
   PrintAndClearStringCopyCounts("After Swap");
   printf("ss1=[%s] ss2=[%s]\n", ss1(), ss2());

   const Point p(1.5,2.5);
   const Rect r(3.5,4.5,5.5,6.5);
   const int16 dozen = 13;
   String aString = String("%1 is a %2 %3 booltrue=%4 boolfalse=%5 point=%6 rect=%7").Arg(dozen).Arg("baker's dozen").Arg(3.14159).Arg(true).Arg(false).Arg(p).Arg(r);
   printf("arg string = [%s]\n", aString());

   String temp;
   temp.SetCstr("1234567890", 3);
   printf("123=[%s]\n", temp());
   temp.SetCstr("1234567890");
   printf("%s\n", temp());

   String scale("do"); scale = scale.AppendWord("re", ", ").AppendWord("mi").AppendWord(String("fa")).AppendWord("so").AppendWord("la").AppendWord("ti").AppendWord("do");
   printf("scale = [%s]\n", scale());
   
   String rem("Hello sailor");
   printf("[%s]\n", (rem+"maggot"-"sailor")());
   rem -= "llo";
   printf("[%s]\n", rem());
   rem -= "xxx";
   printf("[%s]\n", rem());
   rem -= 'H';
   printf("[%s]\n", rem());
   rem -= 'r';
   printf("[%s]\n", rem());
   rem -= rem;
   printf("[%s]\n", rem());

   String test = "hello";
   test = test + " and " + " goodbye " + '!';
   printf("test=[%s]\n", test());

   test.Replace(test, "foo");
   printf("foo=[%s]\n", test());
   test.Replace("o", test);
   printf("ffoofoo=[%s]\n", test());

   String s1("one");
   String s2("two");
   String s3;
   printf("[%s]\n", s1.AppendWord(s2, ", ").AppendWord(s3, ", ")());

   return 0;
}
