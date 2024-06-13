/* This file is Copyright 2000-2022 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "util/String.h"
#include "support/BitChord.h"

using namespace muscle;

// Test a BitChord with 36 options
enum {
   OPTION_A = 0,
   OPTION_B,
   OPTION_C,
   OPTION_D,
   OPTION_E,
   OPTION_F,
   OPTION_G,
   OPTION_H,
   OPTION_I,
   OPTION_J,
   OPTION_K,
   OPTION_L,
   OPTION_M,
   OPTION_N,
   OPTION_O,
   OPTION_P,
   OPTION_Q,
   OPTION_R,
   OPTION_S,
   OPTION_T,
   OPTION_U,
   OPTION_V,
   OPTION_W,
   OPTION_X,
   OPTION_Y,
   OPTION_Z,
   OPTION_0,
   OPTION_1,
   OPTION_2,
   OPTION_3,
   OPTION_4,
   OPTION_5,
   OPTION_6,
   OPTION_7,
   OPTION_8,
   OPTION_9,
   NUM_OPTIONS
};
DECLARE_BITCHORD_FLAGS_TYPE(TestOptionBits, NUM_OPTIONS);

enum {
   FRUIT_APPLE=0,
   FRUIT_BANANA,
   FRUIT_CHERRY,
   FRUIT_GRAPE,
   NUM_FRUITS
};
const char * _fruitBitsLabels[] = {
   "Apple",
   "Banana",
   NULL,  // just to make sure we can handle a NULL entry gracefully
   "Grape",
};
MUSCLE_STATIC_ASSERT_ARRAY_LENGTH(_fruitBitsLabels, NUM_FRUITS);
DECLARE_LABELLED_BITCHORD_FLAGS_TYPE(FruitBits, NUM_FRUITS, _fruitBitsLabels);

// This program exercises the String class.
int main(void)
{
   FruitBits fruits = FruitBits::WithAllBitsSet().WithoutBit(FRUIT_GRAPE);
   printf("fruits=[%s]\n", fruits.ToString()());

   TestOptionBits val(OPTION_J, OPTION_E, OPTION_R, OPTION_E, OPTION_M, OPTION_Y, OPTION_Z);
   const String t = val.ToString();
   const TestOptionBits t2 = TestOptionBits::FromString(t());
   printf("X01 [%s] [%s] -> [%s]\n", val.ToHexString()(), t(), t2.ToString()());

   val.SetBit(OPTION_X);
   printf("X02 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   val.SetBits(OPTION_Y, OPTION_Z, OPTION_9);
   printf("X03 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   val.ToggleBit(OPTION_Z);
   printf("X04 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   val.ToggleBits(OPTION_F, OPTION_R, OPTION_I, OPTION_E, OPTION_S, OPTION_N, OPTION_E, OPTION_R);
   printf("X05 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   val.ClearBit(OPTION_Y);
   printf("X06 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   val.ClearBits(OPTION_J, OPTION_E, OPTION_R, OPTION_E, OPTION_M, OPTION_Y);
   printf("X07 [%s] [%s]\n", val.ToHexString()(), val.ToString()());

   const TestOptionBits v2(val);
   printf("X08 [%s] [%s]\n", v2.ToHexString()(), val.ToString()());

   printf("X09 [%s]\n", val.WithBit(OPTION_Q).ToHexString()());
   printf("X10 [%s]\n", val.WithoutBit(OPTION_S).ToHexString()());
   printf("X11 [%s]\n", val.WithToggledBit(OPTION_T).ToHexString()());

   printf("\n");
   printf("XXX [%s]\n", TestOptionBits(OPTION_X, OPTION_Y, OPTION_Z).ToHexString()());
   printf("X12 [%s]\n", val.WithBits(OPTION_X, OPTION_Y, OPTION_Z).ToHexString()());

   printf("\n");
   printf("XXX [%s]\n", TestOptionBits(OPTION_S, OPTION_T, OPTION_E, OPTION_V).ToHexString()());
   printf("X13 [%s]\n", val.WithoutBits(OPTION_S, OPTION_T, OPTION_E, OPTION_V).ToHexString()());

   printf("\n");
   printf("XXX [%s]\n", TestOptionBits(OPTION_X, OPTION_Z).ToHexString()());
   printf("X14 [%s]\n", val.WithToggledBits(OPTION_X, OPTION_Z).ToHexString()());

   printf("X15 %i\n", val.AreAllOfTheseBitsSet(OPTION_A, OPTION_B, OPTION_C, OPTION_D));
   printf("X16 %i\n", val.AreAnyOfTheseBitsSet(OPTION_E, OPTION_F, OPTION_G, OPTION_H));
   printf("X17 %i\n", val.AreAllOfTheseBitsUnset(OPTION_A, OPTION_B, OPTION_C, OPTION_D));
   printf("X18 %i\n", val.AreAnyOfTheseBitsUnset(OPTION_E, OPTION_F, OPTION_G, OPTION_H));
   printf("X19 [%s]\n", TestOptionBits::WithAllBitsSetExceptThese(OPTION_A, OPTION_C, OPTION_D).ToHexString()());

   TestOptionBits fromWords = TestOptionBits::FromWords((uint32)3,(uint32)0xFFFFFFF5);
   printf("X20 [%s]\n", fromWords.ToHexString()());

   TestOptionBits fromBytes = TestOptionBits::FromBytes((uint8)1,(uint8)2,(uint8)3,(uint8)4,(uint8)(0x65));
   const String hexString = fromBytes.ToHexString();
   printf("X21 [%s]\n", hexString());

   const TestOptionBits restoredFromHex = TestOptionBits::FromHexString(hexString);
   if (restoredFromHex != fromBytes)
   {
      printf("ERROR:  FromHexString() didn't return the original value again!  [%s] -> [%s] -> [%s]\n", fromBytes.ToString()(), hexString(), restoredFromHex.ToString()());
      return 10;
   }

   const String binString = fromBytes.ToBinaryString();
   printf("X22 [%s]\n", binString());

   const TestOptionBits restoredFromBin = TestOptionBits::FromBinaryString(binString);
   if (restoredFromBin != fromBytes)
   {
      printf("ERROR:  FromBinaryString() didn't return the original value again!  [%s] -> [%s] -> [%s]\n", fromBytes.ToString()(), binString(), restoredFromBin.ToString()());
      return 10;
   }

   return 0;
}
