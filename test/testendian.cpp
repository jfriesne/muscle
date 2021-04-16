/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "support/MuscleSupport.h"
#include "util/MiscUtilityFunctions.h"

using namespace muscle;

#define TEST(x) if (!(x)) printf("Test failed, line %i\n",__LINE__)

static const uint32 ARRAYLEN=640000;

static int16  * origArray16;
static int32  * origArray32;
static int64  * origArray64;
static float  * origArrayFloat;
static double * origArrayDouble;

static int16  * bigArray16;
static int32  * bigArray32;
static int64  * bigArray64;
static float  * bigArrayFloat;
static double * bigArrayDouble;

static void PrintBytes(const void * b, int num)
{
   const uint8 * b8 = (const uint8 *) b;
   for (int i=0; i<num; i++) printf("%02x ", b8[i]);
   printf("\n");
}

static void Fail(const char * name, const void * orig, const void * xChange, const void * backAgain, int numBytes, int index)
{
   printf("Test [%s] failed at item %i/" UINT32_FORMAT_SPEC ", code is buggy!!!\n", name, index, ARRAYLEN);
   printf("   Orig: "); PrintBytes(orig, numBytes);
   printf("   Xchg: "); PrintBytes(xChange, numBytes);
   if (backAgain) {printf("   Back: "); PrintBytes(backAgain, numBytes);}
   ExitWithoutCleanup(10);
}

static void CheckSwap(const char * title, const void * oldVal, const void * newVal, int numBytes, int index)
{
   const uint8 * old8 = (const uint8 *) oldVal;
   const uint8 * new8 = (const uint8 *) newVal;
   for (int i=0; i<numBytes; i++) if (old8[i] != new8[numBytes-(i+1)]) Fail(title, oldVal, newVal, NULL, numBytes, index);
}

static void PrintSpeedResult(const char * desc, uint64 beginTime, uint64 endTime, uint64 numOps)
{
   printf("%s exercise took " UINT64_FORMAT_SPEC " ms to complete (" INT64_FORMAT_SPEC " swaps/millisecond).\n", desc, (endTime-beginTime)/1000, (numOps*1000)/(endTime-beginTime));
}

// This program checks the accuracy and measures the speed of muscle's byte-swapping macros.
int main(void) 
{
   // Allocate the test arrays...
   origArray16     = new int16[ARRAYLEN];
   origArray32     = new int32[ARRAYLEN];
   origArray64     = new int64[ARRAYLEN];
   origArrayFloat  = new float[ARRAYLEN];
   origArrayDouble = new double[ARRAYLEN];

   bigArray16     = new int16[ARRAYLEN];
   bigArray32     = new int32[ARRAYLEN];
   bigArray64     = new int64[ARRAYLEN];
   bigArrayFloat  = new float[ARRAYLEN];
   bigArrayDouble = new double[ARRAYLEN];

   // Initialize the arrays...
   {
      for (uint32 i=0; i<ARRAYLEN; i++) 
      {
         int si = (i-(ARRAYLEN/2));
         origArray16[i]     = si;
         origArray32[i]     = si*((int32)1024);
         origArray64[i]     = si*((int64)1024*1024);
         origArrayFloat[i]  = si*100.0f;
         origArrayDouble[i] = si*((double)100000000.0);
      }
   }

#if defined(__BEOS__)
   printf("NOTE:  USING BeOS-provided swap functions!\n");
#elif defined(__HAIKU__)
   printf("NOTE:  USING Haiku-provided swap functions!\n");
#elif defined(MUSCLE_USE_POWERPC_INLINE_ASSEMBLY)
   printf("NOTE:  USING PowerPC inline assembly swap functions!\n");
#elif defined(MUSCLE_USE_X86_INLINE_ASSEMBLY)
   printf("NOTE:  USING x86 inline assembly swap functions!\n");
#elif defined(MUSCLE_USE_MSVC_SWAP_FUNCTIONS)
   printf("NOTE:  USING Visual C++ runtime swap functions!\n");
#else
   printf("NOTE:  Using unoptimized C++ swap functions.\n");
#endif

   printf("testing B_SWAP_* ...\n");
   {
      for (uint32 i=0; i<ARRAYLEN; i++)
      {
         {
            const int16 o16 = origArray16[i];
            const int16 x16 = B_SWAP_INT16(o16); CheckSwap("A. B_SWAP i16", &o16, &x16, sizeof(o16), i);
            const int16 n16 = B_SWAP_INT16(x16); CheckSwap("B. B_SWAP i16", &x16, &n16, sizeof(x16), i);
            if ((muscleSwapBytes(o16) != x16) || (n16 != o16)) {Fail("C. B_SWAP i16", &o16, &x16, &n16, sizeof(int16), i);}
         }

         {
            const int32 o32 = origArray32[i];
            const int32 x32 = B_SWAP_INT32(o32); CheckSwap("A. B_SWAP i32", &o32, &x32, sizeof(o32), i);
            const int32 n32 = B_SWAP_INT32(x32); CheckSwap("B. B_SWAP i32", &x32, &n32, sizeof(x32), i);
            if ((muscleSwapBytes(o32) != x32) || (n32 != o32)) {Fail("C. B_SWAP i32", &o32, &x32, &n32, sizeof(int32), i);}
         }

         {
            const int64 o64 = origArray64[i];
            const int64 x64 = B_SWAP_INT64(o64); CheckSwap("A. B_SWAP i64", &o64, &x64, sizeof(o64), i);
            const int64 n64 = B_SWAP_INT64(x64); CheckSwap("B. B_SWAP i64", &x64, &n64, sizeof(x64), i);
            if ((muscleSwapBytes(o64) != x64) || (n64 != o64)) {Fail("C. B_SWAP i64", &o64, &x64, &n64, sizeof(int64), i);}
         }

         {
            const float ofl  = origArrayFloat[i];
            const uint32 xfl = B_SWAP_INT32(B_REINTERPRET_FLOAT_AS_INT32(ofl)); CheckSwap("A. B_SWAP float", &ofl, &xfl, sizeof(ofl), i);
            const float nfl  = B_REINTERPRET_INT32_AS_FLOAT(B_SWAP_INT32(xfl)); CheckSwap("B. B_SWAP float", &xfl, &nfl, sizeof(xfl), i);
            if ((muscleSwapBytes(B_REINTERPRET_FLOAT_AS_INT32(ofl)) != xfl) || (nfl != ofl)) {Fail("C. B_SWAP float", &ofl, &xfl, &nfl, sizeof(float), i);}
         }

         {
            const double odb = origArrayDouble[i];
            const uint64 xdb = B_SWAP_INT64(B_REINTERPRET_DOUBLE_AS_INT64(odb)); CheckSwap("A. B_SWAP double", &odb, &xdb, sizeof(odb), i);
            const double ndb = B_REINTERPRET_INT64_AS_DOUBLE(B_SWAP_INT64(xdb)); CheckSwap("B. B_SWAP double", &xdb, &ndb, sizeof(xdb), i);
            if ((muscleSwapBytes(B_REINTERPRET_DOUBLE_AS_INT64(odb)) != xdb) || (ndb != odb)) {Fail("C. B_SWAP double", &odb, &xdb, &ndb, sizeof(double), i);}
         }
      }
   }

   printf("testing B_HOST_TO_LENDIAN_* ...\n");
   {
      for (uint32 i=0; i<ARRAYLEN; i++)
      {
         {
            const int16 o16 = origArray16[i];
            const int16 x16 = B_HOST_TO_LENDIAN_INT16(o16);
            const int16 n16 = B_LENDIAN_TO_HOST_INT16(x16);
            if (n16 != o16) {Fail("D. HOST_TO_LENDIAN i16", &o16, &x16, &n16, sizeof(int16), i);}
         }

         {
            const int32 o32 = origArray32[i];
            const int32 x32 = B_HOST_TO_LENDIAN_INT32(o32);
            const int32 n32 = B_LENDIAN_TO_HOST_INT32(x32);
            if (n32 != o32) {Fail("D. HOST_TO_LENDIAN i32", &o32, &x32, &n32, sizeof(int32), i);}
         }

         {
            const int64 o64 = origArray64[i];
            const int64 x64 = B_HOST_TO_LENDIAN_INT64(o64);
            const int64 n64 = B_LENDIAN_TO_HOST_INT64(x64);
            if (n64 != o64) {Fail("D. HOST_TO_LENDIAN i64", &o64, &x64, &n64, sizeof(int64), i);}
         }

         {
            const float ofl  = origArrayFloat[i];
            const uint32 xfl = B_HOST_TO_LENDIAN_IFLOAT(ofl);
            const float nfl  = B_LENDIAN_TO_HOST_IFLOAT(xfl);
            if (nfl != ofl) {Fail("D. HOST_TO_LENDIAN float", &ofl, &xfl, &nfl, sizeof(float), i);}
         }

         {
            const double odb = origArrayDouble[i];
            const uint64 xdb = B_HOST_TO_LENDIAN_IDOUBLE(odb);
            const double ndb = B_LENDIAN_TO_HOST_IDOUBLE(xdb);
            if (ndb != odb) {Fail("D. HOST_TO_LENDIAN double", &odb, &xdb, &ndb, sizeof(double), i);}
         }
      }
   }

   printf("testing B_LENDIAN_TO_HOST_* ...\n");
   {
      for (uint32 i=0; i<ARRAYLEN; i++)
      {
         {
            const int16 o16 = origArray16[i];
            const int16 x16 = B_LENDIAN_TO_HOST_INT16(o16);
            const int16 n16 = B_HOST_TO_LENDIAN_INT16(x16);
            if (n16 != o16) {Fail("E. LENDIAN_TO_HOST i16", &o16, &x16, &x16, sizeof(int16), i);}
         }

         {
            const int32 o32 = origArray32[i];
            const int32 x32 = B_LENDIAN_TO_HOST_INT32(o32);
            const int32 n32 = B_HOST_TO_LENDIAN_INT32(x32);
            if (n32 != o32) {Fail("E. LENDIAN_TO_HOST i32", &o32, &x32, &n32, sizeof(int32), i);}
         }

         {
            const int64 o64 = origArray64[i];
            const int64 x64 = B_LENDIAN_TO_HOST_INT64(o64);
            const int64 n64 = B_HOST_TO_LENDIAN_INT64(x64);
            if (n64 != o64) {Fail("E. LENDIAN_TO_HOST i64", &o64, &x64, &n64, sizeof(int64), i);}
         }

         {
            const uint32 ofl = B_REINTERPRET_FLOAT_AS_INT32(origArrayFloat[i]);
            const float  xfl = B_LENDIAN_TO_HOST_IFLOAT(ofl);
            const uint32 nfl = B_HOST_TO_LENDIAN_IFLOAT(xfl);
            if (nfl != ofl) {Fail("E. LENDIAN_TO_HOST float", &ofl, &xfl, &nfl, sizeof(float), i);}
         }

         {
            const uint64 odb = B_REINTERPRET_DOUBLE_AS_INT64(origArrayDouble[i]);
            const double xdb = B_LENDIAN_TO_HOST_IDOUBLE(odb);
            const uint64 ndb = B_HOST_TO_LENDIAN_IDOUBLE(xdb);
            if (ndb != odb) {Fail("E. LENDIAN_TO_HOST double", &odb, &xdb, &ndb, sizeof(double), i);}
         }
      }
   }

   printf("testing B_HOST_TO_BENDIAN_* ...\n");
   {
      for (uint32 i=0; i<ARRAYLEN; i++)
      {
         {
            const int16 o16 = origArray16[i];
            const int16 x16 = B_HOST_TO_BENDIAN_INT16(o16);
            const int16 n16 = B_BENDIAN_TO_HOST_INT16(x16);
            if (n16 != o16) {Fail("F. HOST_TO_BENDIAN i16", &o16, &x16, &n16, sizeof(int16), i);}
         }

         {
            const int32 o32 = origArray32[i];
            const int32 x32 = B_HOST_TO_BENDIAN_INT32(o32);
            const int32 n32 = B_BENDIAN_TO_HOST_INT32(x32);
            if (n32 != o32) {Fail("F. HOST_TO_BENDIAN i32", &o32, &x32, &n32, sizeof(int32), i);}
         }

         {
            const int64 o64 = origArray64[i];
            const int64 x64 = B_HOST_TO_BENDIAN_INT64(o64);
            const int64 n64 = B_BENDIAN_TO_HOST_INT64(x64);
            if (n64 != o64) {Fail("F. HOST_TO_BENDIAN i64", &o64, &x64, &n64, sizeof(int64), i);}
         }

         {
            const float ofl  = origArrayFloat[i];
            const uint32 xfl = B_HOST_TO_BENDIAN_IFLOAT(ofl);
            const float nfl  = B_BENDIAN_TO_HOST_IFLOAT(xfl);
            if (nfl != ofl) {Fail("F. HOST_TO_BENDIAN float", &ofl, &xfl, &nfl, sizeof(float), i);}
         }

         {
            const double odb = origArrayDouble[i];
            const uint64 xdb = B_HOST_TO_BENDIAN_IDOUBLE(odb);
            const double ndb = B_BENDIAN_TO_HOST_IDOUBLE(xdb);
            if (ndb != odb) {Fail("F. HOST_TO_BENDIAN double", &odb, &xdb, &ndb, sizeof(double), i);}
         }
      }
   }

   printf("testing B_BENDIAN_TO_HOST_* ...\n");
   {
      for (uint32 i=0; i<ARRAYLEN; i++)
      {
         {
            const int16 o16 = origArray16[i];
            const int16 x16 = B_BENDIAN_TO_HOST_INT16(o16);
            const int16 n16 = B_HOST_TO_BENDIAN_INT16(x16);
            if (n16 != o16) {Fail("G. BENDIAN_TO_HOST i16", &o16, &x16, &n16, sizeof(int16), i);}
         }

         {
            const int32 o32 = origArray32[i];
            const int32 x32 = B_BENDIAN_TO_HOST_INT32(o32);
            const int32 n32 = B_HOST_TO_BENDIAN_INT32(x32);
            if (n32 != o32) {Fail("G. BENDIAN_TO_HOST i32", &o32, &x32, &n32, sizeof(int32), i);}
         }

         {
            const int64 o64 = origArray64[i];
            const int64 x64 = B_BENDIAN_TO_HOST_INT64(o64);
            const int64 n64 = B_HOST_TO_BENDIAN_INT64(x64);
            if (n64 != o64) {Fail("G. BENDIAN_TO_HOST i64", &o64, &x64, &n64, sizeof(int64), i);}
         }

         {
            const uint32 ofl = B_REINTERPRET_FLOAT_AS_INT32(origArrayFloat[i]);
            const float xfl  = B_BENDIAN_TO_HOST_IFLOAT(ofl);
            const uint32 nfl = B_HOST_TO_BENDIAN_IFLOAT(xfl);
            if (nfl != ofl) {Fail("G. BENDIAN_TO_HOST float", &ofl, &xfl, &nfl, sizeof(float), i);}
         }

         {
            const uint64 odb = B_REINTERPRET_DOUBLE_AS_INT64(origArrayDouble[i]);
            const double xdb = B_BENDIAN_TO_HOST_IDOUBLE(odb);
            const uint64 ndb = B_HOST_TO_BENDIAN_IDOUBLE(xdb);
            if (ndb != odb) {Fail("G. BENDIAN_TO_HOST double", &odb, &xdb, &ndb, sizeof(double), i);}
         }
      }
   }

   printf("Correctness test complete.\n");

   printf("Now doing speed testing....\n");

   static const uint32 NUM_ITERATIONS = 500;
   static const uint64 TOTAL_SPEED_OPS = ((uint64)ARRAYLEN) * ((uint64)NUM_ITERATIONS);

   // int16 speed test
   {
      {for (uint32 i=0; i<ARRAYLEN; i++) bigArray16[i] = (int16)i;}

      const uint64 beginTime = GetRunTime64();
      {
         for (uint32 i=0; i<NUM_ITERATIONS; i++)
         {
            for (uint32 j=0; j<ARRAYLEN; j++) bigArray16[j] = B_SWAP_INT16(bigArray16[j]);
         }
      }
      PrintSpeedResult("B_SWAP_INT16", beginTime, GetRunTime64(), TOTAL_SPEED_OPS);
   }

   // int32 speed test
   {
      {for (uint32 i=0; i<ARRAYLEN; i++) bigArray32[i] = i;}

      const uint64 beginTime = GetRunTime64();
      {
         for (uint32 i=0; i<NUM_ITERATIONS; i++)
         {
            for (uint32 j=0; j<ARRAYLEN; j++) bigArray32[j] = B_SWAP_INT32(bigArray32[j]);
         }
      }
      PrintSpeedResult("B_SWAP_INT32", beginTime, GetRunTime64(), TOTAL_SPEED_OPS);
   }

   // int64 speed test
   {
      {for (uint32 i=0; i<ARRAYLEN; i++) bigArray64[i] = i;}

      const uint64 beginTime = GetRunTime64();
      {
         for (uint32 i=0; i<NUM_ITERATIONS; i++)
         {
            for (uint32 j=0; j<ARRAYLEN; j++) bigArray64[j] = B_SWAP_INT64(bigArray64[j]);
         }
      }
      PrintSpeedResult("B_SWAP_INT64", beginTime, GetRunTime64(), TOTAL_SPEED_OPS);
   }

   // floating point speed test
   {
      {for (uint32 i=0; i<ARRAYLEN; i++) bigArrayFloat[i] = (float)i;}

      const uint64 beginTime = GetRunTime64();
      {
         for (uint32 i=0; i<NUM_ITERATIONS; i++)
         {
            for (uint32 j=0; j<ARRAYLEN; j++) bigArray32[j] = B_SWAP_INT32(B_REINTERPRET_FLOAT_AS_INT32(bigArrayFloat[j]));
         }
      }
      PrintSpeedResult("B_SWAP_FLOAT", beginTime, GetRunTime64(), TOTAL_SPEED_OPS);
   }

   // double precision floating point speed test
   {
      {for (uint32 i=0; i<ARRAYLEN; i++) bigArrayDouble[i] = (double)i;}

      const uint64 beginTime = GetRunTime64();
      {
         for (uint32 i=0; i<NUM_ITERATIONS; i++)
         {
            for (uint32 j=0; j<ARRAYLEN; j++) bigArray64[j] = B_SWAP_INT64(B_REINTERPRET_DOUBLE_AS_INT64(bigArrayDouble[j]));
         }
      }
      PrintSpeedResult("B_SWAP_DOUBLE", beginTime, GetRunTime64(), TOTAL_SPEED_OPS);
   }

   // Free the test arrays...
   delete [] origArray16;
   delete [] origArray32;
   delete [] origArray64;
   delete [] origArrayFloat;
   delete [] origArrayDouble;

   delete [] bigArray16;
   delete [] bigArray32;
   delete [] bigArray64;
   delete [] bigArrayFloat;
   delete [] bigArrayDouble;

   return 0;
}
