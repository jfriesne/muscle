/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "util/TimeUnitConversionFunctions.h"

using namespace muscle;

#define TEST_CONVERSION(fromType, toType)    \
{                                            \
    int64 before = 1;                        \
    int64 after = 0;                         \
    while(1)                                 \
    {                                        \
       after = fromType##To##toType(before); \
       if (after == 0) before *= 10;         \
                 else break;                 \
    }                                        \
    printf(INT64_FORMAT_SPEC " %s is " INT64_FORMAT_SPEC " %s.\n", before, #fromType, after, #toType); \
}

int main(int /*argc*/, char ** /*argv*/)
{
   TEST_CONVERSION(Nanos, Micros);
   TEST_CONVERSION(Nanos, Millis);
   TEST_CONVERSION(Nanos, Seconds);
   TEST_CONVERSION(Nanos, Minutes);
   TEST_CONVERSION(Nanos, Hours);
   TEST_CONVERSION(Nanos, Days);
   TEST_CONVERSION(Nanos, Weeks);

   TEST_CONVERSION(Micros, Nanos);
   TEST_CONVERSION(Micros, Millis);
   TEST_CONVERSION(Micros, Seconds);
   TEST_CONVERSION(Micros, Minutes);
   TEST_CONVERSION(Micros, Hours);
   TEST_CONVERSION(Micros, Days);
   TEST_CONVERSION(Micros, Weeks);

   TEST_CONVERSION(Millis, Nanos);
   TEST_CONVERSION(Millis, Micros);
   TEST_CONVERSION(Millis, Seconds);
   TEST_CONVERSION(Millis, Minutes);
   TEST_CONVERSION(Millis, Hours);
   TEST_CONVERSION(Millis, Days);
   TEST_CONVERSION(Millis, Weeks);

   TEST_CONVERSION(Seconds, Nanos);
   TEST_CONVERSION(Seconds, Micros);
   TEST_CONVERSION(Seconds, Millis);
   TEST_CONVERSION(Seconds, Minutes);
   TEST_CONVERSION(Seconds, Hours);
   TEST_CONVERSION(Seconds, Days);
   TEST_CONVERSION(Seconds, Weeks);

   TEST_CONVERSION(Minutes, Nanos);
   TEST_CONVERSION(Minutes, Micros);
   TEST_CONVERSION(Minutes, Millis);
   TEST_CONVERSION(Minutes, Seconds);
   TEST_CONVERSION(Minutes, Hours);
   TEST_CONVERSION(Minutes, Days);
   TEST_CONVERSION(Minutes, Weeks);

   TEST_CONVERSION(Hours, Nanos);
   TEST_CONVERSION(Hours, Micros);
   TEST_CONVERSION(Hours, Millis);
   TEST_CONVERSION(Hours, Seconds);
   TEST_CONVERSION(Hours, Minutes);
   TEST_CONVERSION(Hours, Days);
   TEST_CONVERSION(Hours, Weeks);

   TEST_CONVERSION(Days, Nanos);
   TEST_CONVERSION(Days, Micros);
   TEST_CONVERSION(Days, Millis);
   TEST_CONVERSION(Days, Seconds);
   TEST_CONVERSION(Days, Minutes);
   TEST_CONVERSION(Days, Hours);
   TEST_CONVERSION(Days, Weeks);

   TEST_CONVERSION(Weeks, Nanos);
   TEST_CONVERSION(Weeks, Micros);
   TEST_CONVERSION(Weeks, Millis);
   TEST_CONVERSION(Weeks, Seconds);
   TEST_CONVERSION(Weeks, Minutes);
   TEST_CONVERSION(Weeks, Hours);
   TEST_CONVERSION(Weeks, Days);

   return 0;
}
