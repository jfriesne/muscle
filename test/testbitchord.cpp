/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

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

// This program exercises the String class.
int main(void) 
{
   TestOptionBits val(OPTION_J, OPTION_E, OPTION_R, OPTION_E, OPTION_M, OPTION_Y);
   printf("X01 [%s]\n", val.ToHexString()());

   val.SetBit(OPTION_X);
   printf("X02 [%s]\n", val.ToHexString()());

   val.SetBits(OPTION_Y, OPTION_Z, OPTION_9);
   printf("X03 [%s]\n", val.ToHexString()());

   val.ToggleBit(OPTION_Z);
   printf("X04 [%s]\n", val.ToHexString()());

   val.ToggleBits(OPTION_F, OPTION_R, OPTION_I, OPTION_E, OPTION_S, OPTION_N, OPTION_E, OPTION_R);
   printf("X05 [%s]\n", val.ToHexString()());

   val.ClearBit(OPTION_Y);
   printf("X06 [%s]\n", val.ToHexString()());

   val.ClearBits(OPTION_J, OPTION_E, OPTION_R, OPTION_E, OPTION_M, OPTION_Y);
   printf("X07 [%s] ***\n", val.ToHexString()());

   const TestOptionBits v2(val);
   printf("X08 [%s]\n", v2.ToHexString()());

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
   printf("X21 [%s]\n", fromBytes.ToHexString()());

   return 0;
}
