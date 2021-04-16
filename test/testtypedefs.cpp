/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "support/MuscleSupport.h"

using namespace muscle;

template<typename T> inline void testWidth(const T & val, int expectedSize, const char * name)
{
   printf("%s: size=%i expected %i (%s)\n", name, (int) sizeof(val), expectedSize, (sizeof(val) == expectedSize) ? "pass" : "ERROR, WRONG SIZE!");
}

static void testStr(const char * title, const char * gen, const char * expected)
{
   if (strcmp(gen, expected) == 0) printf("%s:  pass (%s)\n", title, gen);
                              else printf("%s:  ERROR, got [%s], expected [%s]\n", title, gen, expected);
}

// This program makes sure that the MUSCLE typedefs have the proper bit-widths.
int main(int, char **) 
{
   printf("Testing MUSCLE typedefs to make sure they are defined to the correct bit-widths...\n");
   if (sizeof(void *) == sizeof(uintptr)) printf("uintptr:  pass, sizeof(uintptr)=%i, sizeof(void *)=%i\n", (int)sizeof(uintptr), (int)sizeof(void *));
                                     else printf("uintptr:  ERROR, sizeof(uintptr)=%i, sizeof(void *)=%i\n", (int)sizeof(uintptr), (int)sizeof(void *));

   {int8   i = 0; testWidth(i, 1, "  int8");}
   {uint8  i = 0; testWidth(i, 1, " uint8");}
   {int16  i = 0; testWidth(i, 2, " int16");}
   {uint16 i = 0; testWidth(i, 2, "uint16");}
   {int32  i = 0; testWidth(i, 4, " int32");}
   {uint32 i = 0; testWidth(i, 4, "uint32");}
   {int64  i = 0; testWidth(i, 8, " int64");}
   {uint64 i = 0; testWidth(i, 8, "uint64");}
   {float  i = 0; testWidth(i, 4, " float");}
   {double i = 0; testWidth(i, 8, "double");}
   {uintptr i= 0; testWidth(i, sizeof(void *), "uintptr");}
   printf("Typedef bit-width testing complete.\n");

   printf("\nTesting MUSCLE muscleSprintf() macros to make sure they are output the correct strings...\n");
   char buf[128];
   {muscleSprintf(buf, "%i %i %i %i",   (int8)1,   (int8)2,   (int8)3,   (int8)4); testStr("  int8", buf, "1 2 3 4");}
   {muscleSprintf(buf, "%i %i %i %i",  (uint8)1,  (uint8)2,  (uint8)3,  (uint8)4); testStr(" uint8", buf, "1 2 3 4");}
   {muscleSprintf(buf, "%i %i %i %i",  (int16)1,  (int16)2,  (int16)3,  (int16)4); testStr(" int16", buf, "1 2 3 4");}
   {muscleSprintf(buf, "%i %i %i %i", (uint16)1, (uint16)2, (uint16)3, (uint16)4); testStr("uint16", buf, "1 2 3 4");}
   {muscleSprintf(buf, INT32_FORMAT_SPEC  " " INT32_FORMAT_SPEC  " " INT32_FORMAT_SPEC  " " INT32_FORMAT_SPEC,   (int32)1,  (int32)2,  (int32)3,  (int32)4); testStr(" int32", buf, "1 2 3 4");}
   {muscleSprintf(buf, UINT32_FORMAT_SPEC " " UINT32_FORMAT_SPEC " " UINT32_FORMAT_SPEC " " UINT32_FORMAT_SPEC, (uint32)1, (uint32)2, (uint32)3, (uint32)4); testStr("uint32", buf, "1 2 3 4");}
   {muscleSprintf(buf, XINT32_FORMAT_SPEC " " XINT32_FORMAT_SPEC " " XINT32_FORMAT_SPEC " " XINT32_FORMAT_SPEC, (int32)26, (int32)27, (int32)28, (int32)29); testStr("xint32", buf, "1a 1b 1c 1d");}
   {muscleSprintf(buf, INT64_FORMAT_SPEC  " " INT64_FORMAT_SPEC  " " INT64_FORMAT_SPEC  " " INT64_FORMAT_SPEC,   (int64)1,  (int64)2,  (int64)3,  (int64)4); testStr(" int64", buf, "1 2 3 4");}
   {muscleSprintf(buf, UINT64_FORMAT_SPEC " " UINT64_FORMAT_SPEC " " UINT64_FORMAT_SPEC " " UINT64_FORMAT_SPEC, (uint64)1, (uint64)2, (uint64)3, (uint64)4); testStr("uint64", buf, "1 2 3 4");}
   {muscleSprintf(buf, "%.1f %.1f %.1f %.1f", 1.5f, 2.5f, 3.5f, 4.5f); testStr(" float", buf, "1.5 2.5 3.5 4.5");}
   {muscleSprintf(buf, "%.1f %.1f %.1f %.1f", 1.5,  2.5,  3.5,  4.5);  testStr("double", buf, "1.5 2.5 3.5 4.5");}
   printf("String format testing complete.\n");
   return 0;
}
