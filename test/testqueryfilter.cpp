/* This file is Copyright 2000-2013 Meyer Sound Laboratories Inc.  See the included LICENSE.txt file for details. */

#include <stdio.h>

#include "regex/QueryFilter.h"

using namespace muscle;

#define TEST(x) if ((x) != B_NO_ERROR) printf("Operation failed, line %i\n", __LINE__);
#define NEGATIVETEST(x) if ((x) == B_NO_ERROR) printf("Operation succeeded when it should not have, line %i\n", __LINE__);

void printSep(const char * title);
void printSep(const char * title)
{
   printf("\n----------------- %s -------------------\n", title);
}

#define COMMAND_HELLO   0x1234
#define COMMAND_GOODBYE 0x4321

// This program exercises the Message class.
int main(void)
{
   Message m1;
   m1.AddFloat("va", 1.0f);
   printf("m1=" UINT32_FORMAT_SPEC"\n", m1.FlattenedSize());
   m1.AddInt32("co", 32);
   printf("m2=" UINT32_FORMAT_SPEC"\n", m1.FlattenedSize());

   printSep("Testing Replace*() with okayToAdd...");
   Message butter;
   butter.ReplaceInt8(true, "int8", 8);
   butter.ReplaceInt16(true, "int16", 16);
   butter.ReplaceInt32(true, "int32", 32);
   butter.ReplaceInt64(true, "int64", 64);
   butter.ReplaceFloat(true, "float", 3.14f);
   butter.ReplaceDouble(true, "double", 6.28);
   butter.ReplacePoint(true, "point", Point(5,4));
   butter.ReplaceRect(true, "rect", Rect(5,6,7,8));
   butter.ReplacePointer(true, "pointer", &butter);
   butter.PrintToStream();

   butter.ReplaceInt16(true, "int16", 0, 17);
   butter.ReplaceInt16(true, "int16", 1, 18);
   butter.ReplaceInt8(true, "int8", 25, 25);  // should work the same as AddInt8("int8", 25);

   butter.AddTag("Tag", RefCountableRef(GetMessageFromPool(6666)()));
   butter.AddTag("Tag", RefCountableRef(GetMessageFromPool(7777)()));
   butter.PrintToStream();

   printf("(butter==m1) == %i\n", butter == m1);
   printf("(butter==butter) == %i\n", butter == butter);

   printSep("Testing Add*()...");

   Message msg(COMMAND_HELLO);
   TEST(msg.AddString("Friesner", "Jeremy"));
   TEST(msg.AddString("Friesner", "Joanna"));
   TEST(msg.AddString("Friesner", "Joellen"));
   TEST(msg.AddString("Chicken", "Soup"));
   TEST(msg.AddString("Chicken", "Vegetable"));
   TEST(msg.AddString("Chicken", "Lips"));
   TEST(msg.AddString("Fred", "Flintstone"));
   TEST(msg.AddString("Buddha", "Bark"));
   TEST(msg.AddPoint("point12", Point(1,2)));
   TEST(msg.AddPoint("point12", Point(2,1)));
   TEST(msg.AddRect("rect1234", Rect(1,2,3,4)));
   TEST(msg.AddRect("rect2345", Rect(2,3,4,5)));
   TEST(msg.AddData("Data", B_RAW_TYPE, "ABCDEFGHIJKLMNOPQRS", 12));
   TEST(msg.AddData("Data", B_RAW_TYPE, "Mouse", 3));

   Message subMessage(1);
   TEST(subMessage.AddString("I am a", "sub message!"));
   TEST(subMessage.AddInt32("My age is", 32));

   Message subsubMessage(2);
   TEST(subsubMessage.AddBool("Wow, that's deep!", true));
   TEST(subsubMessage.AddMessage("This is actually okay to do!", subsubMessage));
   TEST(subMessage.AddMessage("subsubMessage", subsubMessage));

   TEST(msg.AddMessage("subMessage", subMessage));

   {for (int i=0; i<10; i++) TEST(msg.AddInt8("TestInt8", i));    }
   {for (int i=0; i<10; i++) TEST(msg.AddInt16("TestInt16", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddInt32("TestInt32", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddInt64("TestInt64", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddDouble("TestDouble", i));}
   {for (int i=0; i<10; i++) TEST(msg.AddFloat("TestFloat", i));  }
   {for (int i=0; i<10; i++) TEST(msg.AddBool("TestBool", i));    }

   printf("Finished message:\n");
   msg.PrintToStream();

   printSep("Testing RemoveName, RemoveData, Replace*()...");
   TEST(msg.RemoveData("TestInt8", 5));
   TEST(msg.RemoveName("Buddha"));
   TEST(msg.RemoveData("Fred", 0));
   TEST(msg.RemoveData("Friesner", 1));
   NEGATIVETEST(msg.RemoveData("Glorp", 0));
   NEGATIVETEST(msg.RemoveData("Chicken", 5));
   TEST(msg.ReplaceString(false, "Friesner", 0, "Jorge"));
   TEST(msg.ReplaceString(false, "Chicken", 1, "Feet"));
   TEST(msg.ReplaceString(false, "Chicken", 2, "Breast"));
   NEGATIVETEST(msg.ReplaceString(false, "Chicken", 3, "Soul"));
   TEST(msg.ReplaceDouble(true, "TestDouble", 2, 222.222));
   TEST(msg.ReplaceFloat(true, "TestFloat", 3, 333.333));
   NEGATIVETEST(msg.ReplaceFloat(false, "RootBeerFloat", 0, 444.444f));
   TEST(msg.ReplaceBool(false, "TestBool", 5));
   TEST(msg.ReplaceRect(false, "rect2345", Rect(2,3,4,5)));

   Message eqMsg = msg;
   printf("EQMSG=msg == %i\n", eqMsg==msg);

   printf("Replaced message:\n");
   msg.PrintToStream();

   printSep("Testing the Find() commands...");
   String strResult;
   TEST(msg.FindString("Friesner", strResult));
   printf("Friesner(0) = %s\n", strResult.Cstr());
   const char * res;
   TEST(msg.FindString("Friesner", 1, &res));
   printf("Friesner(1) = %s\n", res);
   NEGATIVETEST(msg.FindString("Friesner", 2, strResult));
   NEGATIVETEST(msg.FindString("Friesner", 3, strResult));

   int8 int8Result;
   TEST(msg.FindInt8("TestInt8", 5, int8Result));
   printf("TestInt8(5) = %i\n",int8Result);

   int16 int16Result;
   TEST(msg.FindInt16("TestInt16", 4, int16Result));
   printf("TestInt16(4) = %i\n",int16Result);

   int32 int32Result;
   TEST(msg.FindInt32("TestInt32", 4, int32Result));
   printf("TestInt32(4) = " INT32_FORMAT_SPEC"\n",int32Result);

   int64 int64Result;
   TEST(msg.FindInt64("TestInt64", 4, int64Result));
   printf("TestInt64(4) = " INT64_FORMAT_SPEC "\n",int64Result);

   float floatResult;
   TEST(msg.FindFloat("TestFloat", 4, floatResult));
   printf("TestFloat(4) = %f\n",floatResult);

   double doubleResult;
   TEST(msg.FindDouble("TestDouble", 4, doubleResult));
   printf("TestDouble(4) = %f\n",doubleResult);

   Rect rectResult;
   TEST(msg.FindRect("rect2345", rectResult));
   printf("TestRect: "); rectResult.PrintToStream();

   Point pointResult;
   TEST(msg.FindPoint("point12", 1, pointResult));
   printf("TestPoint: "); pointResult.PrintToStream();

   msg.AddTag("ThisShouldn'tBeBackAfterUnflatten", RefCountableRef(NULL));

   const void * gd;
   uint32 getDataSize;
   TEST(msg.FindData("Data", B_RAW_TYPE, &gd, &getDataSize));
   String dataStr((const char *) gd, getDataSize);
   printf("data=[%s], size=" UINT32_FORMAT_SPEC"\n", dataStr(), getDataSize);
   TEST(msg.FindData("Data", B_RAW_TYPE, 1, (const void **) &gd, &getDataSize));
   dataStr.SetCstr((const char *) gd, getDataSize);
   printf("data(1)=[%s], size=" UINT32_FORMAT_SPEC"\n", dataStr(), getDataSize);

   printSep("Testing misc");

   printf("There are " UINT32_FORMAT_SPEC" string entries\n", msg.GetNumNames(B_STRING_TYPE));
   msg.PrintToStream();
   Message tryMe = msg;
   printf("Msg is " UINT32_FORMAT_SPEC" bytes.\n",msg.FlattenedSize());
   msg.AddTag("anothertag", RefCountableRef(GetMessageFromPool()()));
   printf("After adding tag, msg is (hopefully still) " UINT32_FORMAT_SPEC" bytes.\n",msg.FlattenedSize());
   tryMe.PrintToStream();

   printf("Extracting...\n");
   Message extract;
   TEST(tryMe.FindMessage("subMessage", extract));
   printSep("Extracted subMessage!\n");
   extract.PrintToStream();

   Message subExtract;
   TEST(extract.FindMessage("subsubMessage", subExtract));
   printSep("Extracted subsubMessage!\n");
   subExtract.PrintToStream();

   uint32 flatSize = msg.FlattenedSize();
   printf("FlatSize=" UINT32_FORMAT_SPEC"\n",flatSize);
   uint8 * buf = new uint8[flatSize*10];
   {for (uint32 i=flatSize; i<flatSize*10; i++) buf[i] = 'J';}

   msg.Flatten(buf);

   {for (uint32 i=flatSize; i<flatSize*10; i++) if (buf[i] != 'J') printf("OVERWRITE ON BYTE " UINT32_FORMAT_SPEC"\n",i);}
   printf("\n====\n");

   Message copy;
   if (copy.Unflatten(buf, flatSize) == B_NO_ERROR)
   {
      printf("****************************\n");
      copy.PrintToStream();
      printf("***************************2\n");
      Message dup(copy);
      dup.PrintToStream();
   }
   else printf("Rats, Unflatten did not work.  :^(\n");

   {
      printf("Testing field name iterator... B_ANY_TYPE\n");
      for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_ANY_TYPE); it.HasData(); it++) printf("--> [%s] (%i)\n", it.GetFieldName()(), it.HasData());
   }

   {
      printf("Testing field name iterator... B_STRING_TYPE\n");
      for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_STRING_TYPE); it.HasData(); it++) printf("--> [%s] (%i)\n", it.GetFieldName()(), it.HasData());
   }

   {
      printf("Testing field name iterator... B_INT8_TYPE\n");
      for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_INT8_TYPE); it.HasData(); it++) printf("--> [%s] (%i)\n", it.GetFieldName()(), it.HasData());
   }

   {
      printf("Testing field name iterator... B_OBJECT_TYPE (should have no results)\n");
      for (MessageFieldNameIterator it = copy.GetFieldNameIterator(B_OBJECT_TYPE); it.HasData(); it++) printf("--> [%s] (%i)\n", it.GetFieldName()(), it.HasData());
   }
   return 0;
}
