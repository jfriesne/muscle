/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "message/Message.h"
#include "system/SetupSystem.h"
#include "util/String.h"
#include "util/MiscUtilityFunctions.h"
#include "util/NetworkUtilityFunctions.h"
#include "support/Tuple.h"  // make sure the template operators don't mess us up

using namespace muscle;

#define TEST(x) if (!(x)) printf("Test failed, line %i\n",__LINE__)

/** This class is here only to test the templated String::Arg() method */
class SomeClass
{
public:
   SomeClass() {/* empty */}

   String ToString() const {return "SomeClass::ToString() returned this";}
};

static int ThisFunctionsArgumentMustBeZero(int f)
{
   if (f == 0) return 0;  // the only safe argument

   MCRASH("ThisFunctionsArgumentMustBeZero() should not be called with a non-zero argument!  LogTime() is buggy, perhaps?");
   return f;
}

static status_t UnitTestString()
{
   CompleteSetupSystem css;

   // First, make sure that our logging doesn't evaluate arguments unless it needs to
   LogPlain(MUSCLE_LOG_INFO,  "Testing LogPlain() argument evaluation:  %i\n", ThisFunctionsArgumentMustBeZero(0));  // ThisFunctionsArgumentMustBeZero() SHOULD be called here!
   LogPlain(MUSCLE_LOG_DEBUG, "Testing LogPlain() argument evaluation:  %i\n", ThisFunctionsArgumentMustBeZero(1));  // ThisFunctionsArgumentMustBeZero() should NOT be called here!
   LogTime(MUSCLE_LOG_INFO,   "Testing LogTime()  argument evaluation:  %i\n", ThisFunctionsArgumentMustBeZero(0));  // ThisFunctionsArgumentMustBeZero() SHOULD be called here!
   LogTime(MUSCLE_LOG_DEBUG,  "Testing LogTime()  argument evaluation:  %i\n", ThisFunctionsArgumentMustBeZero(1));  // ThisFunctionsArgumentMustBeZero() should NOT be called here!

   {
      // Test to make sure that when a string is set equal to an empty string, it deletes its buffer.
      // (That way long strings can't build up in an ObjectPool somewhere)
      String longString = "this is a very long string.  Well okay it's not THAT long, but long enough.";
      const String & emptyString = GetDefaultObjectForType<String>();
      printf("Before copy-from-empty:   longString [%s] bufSize=" UINT32_FORMAT_SPEC ", emptyString [%s] bufSize=" UINT32_FORMAT_SPEC "\n", longString(), longString.GetNumAllocatedBytes(), emptyString(), emptyString.GetNumAllocatedBytes());
      longString = emptyString;
      printf(" After copy-from-empty:   longString [%s] bufSize=" UINT32_FORMAT_SPEC ", emptyString [%s] bufSize=" UINT32_FORMAT_SPEC "\n", longString(), longString.GetNumAllocatedBytes(), emptyString(), emptyString.GetNumAllocatedBytes());
      if (longString.GetNumAllocatedBytes() > (SMALL_MUSCLE_STRING_LENGTH+1)) return B_ERROR("String set from empty sting still has a non-default buffer!");
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
      MRETURN_ON_ERROR(s.ShrinkToFit());
      printf("After ShrinkToFit():  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());

      s = "Now I'm small";
      printf("After setting small:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());

      MRETURN_ON_ERROR(s.ShrinkToFit());
      printf("After ShrinkToFit to small():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());

      s = "tiny";
      printf("After setting tiny:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());
      MRETURN_ON_ERROR(s.ShrinkToFit());
      printf("After ShrinkToFit to tiny():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());

      s = "tin";
      printf("After setting tin:  s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s.Length(), s.GetNumAllocatedBytes());
      MRETURN_ON_ERROR(s.ShrinkToFit());
      printf("After ShrinkToFit to tin():  s=[%s] s.Length()=" UINT32_FORMAT_SPEC " s.GetNumAllocatedBytes()=" UINT32_FORMAT_SPEC "\n", s(), s.Length(), s.GetNumAllocatedBytes());
   }

   // Test the multi-search-and-replace version of WithReplacements()
   {
      const String before = "One potato, Two potato, Three potato, Four.  Five potato, Six potato, Seven potato, more!  One Two Three Four Five";

      Hashtable<String,String> replaceMap;
      replaceMap.Put("One",    "Two");
      replaceMap.Put("Two",    "3");
      replaceMap.Put("Three",  "4");
      replaceMap.Put("potato", "sweet potato");
      replaceMap.Put("sweet",  "sour");       // shouldn't have any effect, since the original string doesn't contain the substring 'sour'

      const String after = before.WithReplacements(replaceMap);
      const String expected = "Two sweet potato, 3 sweet potato, 4 sweet potato, Four.  Five sweet potato, Six sweet potato, Seven sweet potato, more!  Two 3 4 Four Five";
      if (after == expected) printf("Multi-replace:  got expected result [%s]\n", after());
      else
      {
         printf("ERROR GOT WRONG MULTI-REPLACE RESULT [%s], expected [%s]\n", after(), expected());
         return B_LOGIC_ERROR;
      }
   }

   int five=5, six=6;
   muscleSwap(five, six);
   if ((five != 6)||(six != 5)) {printf("Oh no, trivial muscleSwap() is broken!  five=%i six=%i\n", five, six); return B_LOGIC_ERROR;}

   const String oss1 = "This is string 1", oss2 = "This is string 2";
   String ss1 = oss1, ss2 = oss2;

   PrintAndClearStringCopyCounts("Before Swap");
   muscleSwap(ss1, ss2);

   PrintAndClearStringCopyCounts("After Swap");
   printf("ss1=[%s] ss2=[%s]\n", ss1(), ss2());

   if (ss1 != oss2) return B_LOGIC_ERROR;
   if (ss2 != oss1) return B_LOGIC_ERROR;

   const Point p(1.5,2.5);
   const Rect r(3.5,4.5,5.5,6.5);
   const int16 dozen = 13;
   String aString = String("%1 is a %2 %3 booltrue=%4 boolfalse=%5 point=%6 rect=%7 SomeClass=%8").Arg(dozen).Arg("baker's dozen").Arg(3.14159).Arg(true).Arg(false).Arg(p).Arg(r).Arg(SomeClass());
   aString += SomeClass();
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
#ifdef COMMENTED_OUT_TO_AVOID_COMPILER_WARNING_EVEN_THOUGH_ITS_A_VALID_TEST
   rem -= rem;
   printf("[%s]\n", rem());
#endif

   String test = "hello";
   test = test + " and " + " goodbye " + '!' + SomeClass();
   printf("test=[%s]\n", test());

   test.Replace(test, "foo");
   printf("foo=[%s]\n", test());
   test.Replace("o", test);
   printf("ffoofoo=[%s]\n", test());

   String s1("one");
   String s2("two");
   String s3;
   printf("[%s]\n", s1.AppendWord(s2, ", ").AppendWord(s3, ", ")());

   return B_NO_ERROR;
}

int main(int, char **)
{
   const status_t ret = UnitTestString();
   if (ret.IsOK())
   {
      LogTime(MUSCLE_LOG_INFO, "teststring unit test passed.\n");
      return 0;
   }
   else
   {
      LogTime(MUSCLE_LOG_CRITICALERROR, "teststring unit test failed! [%s]\n", ret());
      return 10;
   }
}
