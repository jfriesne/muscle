/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */  

#include <stdio.h>

#include "message/Message.h"
#include "atheossupport/ConvertMessages.h"
#include "system/SetupSystem.h"

using namespace muscle;

#define TEST(x) if ((x).IsError()) printf("Operation failed, line " INT32_FORMAT_SPEC "\n", __LINE__);
#define NEGATIVETEST(x) if ((x).IsOK()) printf("Operation succeeded when it should not have, line " INT32_FORMAT_SPEC "\n", __LINE__);

// This program tests the Message <-> Message conversion functions in the atheossupport directory.
int main(void) 
{
   CompleteSetupSystem css;

   const int HELO = 1234;
   Message msg(HELO);
   TEST(msg.AddString("Hi", "there"));
   TEST(msg.AddString("Friesner", "Jeremy"));
   TEST(msg.AddString("Friesner", "Joanna"));
   TEST(msg.AddString("Friesner", "Joellen"));
   TEST(msg.AddString("Chicken", "Soup"));
   TEST(msg.AddString("Chicken", "Vegetable"));
   TEST(msg.AddString("Chicken", "Lips"));
   TEST(msg.AddString("Fred", "Flintstone"));
   TEST(msg.AddPoint("pointMe", Point(1,2)));
   TEST(msg.AddRect("rectMe", Rect(1,2,3,4)));
   TEST(msg.AddRect("rectMe", Rect(2,3,4,5)));
   TEST(msg.AddData("Data", B_RAW_TYPE, "Keyboard", 9));
   TEST(msg.AddData("Data", B_RAW_TYPE, "BLACKJACK", 2));

   Message subMsg(2345);
   Message deeper(4567);
   subMsg.AddMessage("Russian Dolls", deeper);
   TEST(msg.AddMessage("TestMessage", subMsg));

   for (int i=0; i<10; i++) TEST(msg.AddInt8("TestInt8", i));
   for (int i=0; i<12; i++) TEST(msg.AddInt16("TestInt16", i));
   for (int i=0; i<13; i++) TEST(msg.AddInt32("TestInt32", i));
   for (int i=0; i<11; i++) TEST(msg.AddInt64("TestInt64", i));
   for (int i=0; i<5; i++) TEST(msg.AddDouble("TestDouble", i));
   for (int i=0; i<6; i++) TEST(msg.AddFloat("TestFloat", i));
   for (int i=0; i<25; i++) TEST(msg.AddBool("TestBool", i));

   printf("ORIGINAL MESSAGE:\n");
   msg.PrintToStream();

   os::Message aMsg;
   printf("CONVERTING TO OS::MESSAGE...\n");
   TEST(ConvertToAMessage(msg, aMsg));
// aMsg.PrintToStream();

   printf("CONVERTING BACK TO MUSCLEMESSAGE...\n");
   Message mmsg;
   TEST(ConvertFromAMessage(aMsg, mmsg));
   mmsg.PrintToStream();

   Message rSub, rDeep;
   if ((mmsg.FindMessage("TestMessage", rSub).IsOK())&&
       (rSub.FindMessage("Russian Dolls", rDeep).IsOK())) 
   { 
      printf("Nested messages are:\n");
      rSub.PrintToStream();
      rDeep.PrintToStream();
   }
   else printf("ERROR RE-READING NESTED MESSAGES!\n");

   printf("TORTURE TEST!\n");
   int i=1000;
   uint32 origSize = mmsg.FlattenedSize();
   while(i--)
   {
      TEST(ConvertFromAMessage(aMsg, mmsg));
      TEST(ConvertToAMessage(mmsg, aMsg));
      uint32 flatSize = mmsg.FlattenedSize();
      if (flatSize != origSize) printf("ERROR, FLATTENED SIZE CHANGED " UINT32_FORMAT_SPEC " -> " UINT32_FORMAT_SPEC "\n", origSize, flatSize);
      uint8 * buf = new uint8[flatSize]; 
      mmsg.Flatten(buf);
      TEST(mmsg.Unflatten(buf, flatSize));
      delete [] buf;
   } 
   return 0;
}
